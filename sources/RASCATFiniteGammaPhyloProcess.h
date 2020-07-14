
/********************

PhyloBayes MPI. Copyright 2010-2013 Nicolas Lartillot, Nicolas Rodrigue, Daniel Stubbs, Jacques Richer.

PhyloBayes is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
PhyloBayes is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details. You should have received a copy of the GNU General Public License
along with PhyloBayes. If not, see <http://www.gnu.org/licenses/>.

**********************/


#ifndef RASCATFINITE_H
#define RASCATFINITE_H

#include "PoissonSubstitutionProcess.h"
#include "PoissonPhyloProcess.h"
#include "DGamRateProcess.h"
#include "PoissonFiniteProfileProcess.h"
#include "GammaBranchProcess.h"

class RASCATFiniteSubstitutionProcess : public virtual PoissonSubstitutionProcess, public virtual DGamRateProcess, public virtual PoissonFiniteProfileProcess {

	public:

	RASCATFiniteSubstitutionProcess() {}
	virtual ~RASCATFiniteSubstitutionProcess() {}

	protected:

	virtual void Create(int, int)	{
		cerr << "error : in RASCATSubProcess::Create(int,int)\n";
		exit(1);
	}

	virtual void Create(int nsite, int nratecat, int ncat, int nstate, int infixncomp, int inempmix, string inmixtype, int insitemin,int insitemax)	{
		if (ncat == -1)	{
			ncat = nsite;
		}
	
		PoissonSubstitutionProcess::Create(nsite,nstate,insitemin,insitemax);
		DGamRateProcess::Create(nsite,nratecat);
		PoissonFiniteProfileProcess::Create(nsite,nstate,ncat,infixncomp,inempmix,inmixtype);
	}

	virtual void Delete()	{
		PoissonFiniteProfileProcess::Delete();
		DGamRateProcess::Delete();
		PoissonSubstitutionProcess::Delete();
	}

};

class RASCATFiniteGammaPhyloProcess : public virtual PoissonPhyloProcess, public virtual RASCATFiniteSubstitutionProcess, public virtual GammaBranchProcess	{

	public:

    virtual void SlaveExecute(MESSAGE);
	virtual void GlobalUpdateParameters();
	virtual void SlaveUpdateParameters();

	RASCATFiniteGammaPhyloProcess() {}

	RASCATFiniteGammaPhyloProcess(string indatafile, string treefile, int nratecat, int ncat, int infixncomp, int inempmix, string inmixtype, double indirweightprior, int infixtopo, int inNSPR, int inNNNI, int indc, int me, int np)	{
		myid = me;
		nprocs = np;

        withfulllogl = 0;

        dirweightprior = indirweightprior;

		fixtopo = infixtopo;
		NSPR = inNSPR;
		NNNI = inNNNI;
		dc = indc;

		datafile = indatafile;
		SequenceAlignment* plaindata = new FileSequenceAlignment(datafile,0,myid);
		if (dc)	{
			plaindata->DeleteConstantSites();
		}
		const TaxonSet* taxonset = plaindata->GetTaxonSet();
		if (treefile == "None")	{
			tree = new Tree(taxonset);
			if (myid == 0)	{
				tree->MakeRandomTree();
				GlobalBroadcastTree();
			}
			else	{
				SlaveBroadcastTree();
			}
		}
		else	{
			tree = new Tree(treefile);
		}
		tree->RegisterWith(taxonset,myid);
		
		int insitemin = -1,insitemax = -1;
		if (myid > 0) {
			int width = plaindata->GetNsite()/(nprocs-1);
			insitemin = (myid-1)*width;
			if (myid == (nprocs-1)) {
				insitemax = plaindata->GetNsite();
			}
			else {
				insitemax = myid*width;
			}
		}

		Create(tree,plaindata,nratecat,ncat,infixncomp,inempmix,inmixtype,insitemin,insitemax);

		if (myid == 0)	{
			Sample();
			GlobalUnfold();
		}
	}

	RASCATFiniteGammaPhyloProcess(istream& is, int me, int np)	{
		myid = me;
		nprocs = np;

        withfulllogl = 0;

		FromStreamHeader(is);
		is >> datafile;
		int nratecat;
		is >> nratecat;
		int infixncomp;
		int inempmix;
		string inmixtype;
		is >> infixncomp >> inempmix >> inmixtype;
		if (atof(version.substr(0,3).c_str()) > 1.7)	{
            is >> dirweightprior;
        }
		is >> fixtopo;
		if (atof(version.substr(0,3).c_str()) > 1.4)	{
			is >> NSPR;
			is >> NNNI;
		}
		else	{
			NSPR = 10;
			NNNI = 0;
		}
		is >> dc;
		SequenceAlignment* plaindata = new FileSequenceAlignment(datafile,0,myid);
		if (dc)	{
			plaindata->DeleteConstantSites();
		}
		const TaxonSet* taxonset = plaindata->GetTaxonSet();

		int insitemin = -1,insitemax = -1;
		if (myid > 0) {
			int width = plaindata->GetNsite()/(nprocs-1);
			insitemin = (myid-1)*width;
			if (myid == (nprocs-1)) {
				insitemax = plaindata->GetNsite();
			}
			else {
				insitemax = myid*width;
			}
		}

		tree = new Tree(taxonset);
		if (myid == 0)	{
			tree->ReadFromStream(is);
			GlobalBroadcastTree();
		}
		else	{
			SlaveBroadcastTree();
		}
		tree->RegisterWith(taxonset,0);

		Create(tree,plaindata,nratecat,1,infixncomp,inempmix,inmixtype,insitemin,insitemax);

		if (myid == 0)	{
			FromStream(is);
			GlobalUnfold();
		}
	}

	~RASCATFiniteGammaPhyloProcess() {
		Delete();
	}

	double GetLogProb()	{
		return GetLogPrior() + GetLogLikelihood();
	}

	double GetLogPrior()	{
		// yet to be implemented
		return 0;
	}

	double GetLogLikelihood()	{
		return logL;
	}

    void TraceMixtureProfileAlloc(ostream& os) {
        for (int i=0; i<GetNsite(); i++) {
            os << MixtureProfileProcess::alloc[i] << ',';
        }
    }
    

	void TraceHeader(ostream& os)	{
		os << "#time\ttime\ttopo\tloglik\tlength\talpha\tNmode\tstatent\tstatalpha\tallocvec";
		os << '\n'; 
	}

	void Trace(ostream& os)	{

		os << GetIndex();
		if (chronototal.GetTime())	{
			os << '\t' << chronototal.GetTime() / 1000;
			os << '\t' << ((int) (propchrono.GetTime() / chronototal.GetTime() * 100));
			chronototal.Reset();
			propchrono.Reset();
		}
		else	{
			os << '\t' << 0;
			os << '\t' << 0;
		}

        if (withfulllogl)   {
            os << '\t' << GlobalGetFullLogLikelihood();
        }
        else    {
            os << '\t' << GetLogLikelihood();
        }
        os << '\t' << GetRenormTotalLength() << '\t' << GetAlpha();
		os << '\t' << GetNOccupiedComponent() << '\t' << GetStatEnt();
		os << '\t' << GetMeanDirWeight() << '\t';
		// os << '\t' << kappa << '\t' << GetAllocEntropy();
        TraceMixtureProfileAlloc(os);
		os << '\n';
	}

	virtual double Move(double tuning = 1.0)	{

		chronototal.Start();
		propchrono.Start();
		if (! fixbl)	{
			BranchLengthMove(tuning);
			BranchLengthMove(0.1 * tuning);
		}
		if (! fixtopo)	{
			MoveTopo(NSPR,NNNI);
		}
		propchrono.Stop();

		GlobalCollapse();

		if (! fixbl)	{
			GammaBranchProcess::Move(tuning,50);
			GammaBranchProcess::Move(0.1*tuning,50);
		}

		// this one is important 
		GlobalUpdateParameters();
		DGamRateProcess::Move(tuning,50);
		DGamRateProcess::Move(0.3*tuning,50);
		DGamRateProcess::Move(0.03*tuning,50);
		// RASCATSubstitutionProcess::MoveRate(tuning);

		// this one is not useful
		// because uniformized process:
		// conditional on discrete substitution mapping
		// profiles do not depend on branch lengths and site rates
		// GlobalUpdateParameters();

		PoissonFiniteProfileProcess::Move(1,1,5);
		GlobalUpdateParameters();

		GlobalUnfold();
		chronototal.Stop();

		return 1;
	
	}

	virtual void ReadPB(int argc, char* argv[]);
	void SlaveComputeCVScore();
	void SlaveComputeSiteLogL();

	void ToStreamHeader(ostream& os)	{
		PhyloProcess::ToStreamHeader(os);
		os << datafile << '\n';
		os << GetNcat() << '\n';
		os << fixncomp << '\t' << empmix << '\t' << mixtype << '\n';
        os << dirweightprior << '\n';
		os << fixtopo << '\n';
		os << NSPR << '\t' << NNNI << '\n';
		os << dc << '\n';
		SetNamesFromLengths();
		GetTree()->ToStream(os);
	}

	void ToStream(ostream& os)	{
		GammaBranchProcess::ToStream(os);
		DGamRateProcess::ToStream(os);
		PoissonFiniteProfileProcess::ToStream(os);
	}

	void FromStream(istream& is)	{
		GammaBranchProcess::FromStream(is);
		DGamRateProcess::FromStream(is);
		PoissonFiniteProfileProcess::FromStream(is);
		GlobalUpdateParameters();
	}


	virtual void Create(Tree* intree, SequenceAlignment* indata, int nratecat,int ncat,int infixncomp, int inempmix, string inmixtype, int insitemin,int insitemax)	{
		PoissonPhyloProcess::Create(intree,indata);
		// PoissonPhyloProcess::Create(intree,indata,indata->GetNstate(),insitemin,insitemax);
		RASCATFiniteSubstitutionProcess::Create(indata->GetNsite(),nratecat,ncat,indata->GetNstate(),infixncomp, inempmix, inmixtype, insitemin,insitemax);
		GammaBranchProcess::Create(intree);
	}
		
	virtual void Delete()	{
		GammaBranchProcess::Delete();
		RASCATFiniteSubstitutionProcess::Delete();
		PoissonPhyloProcess::Delete();
	}

	int fixtopo;
	int NSPR;
	int NNNI;
	int dc;
    int withfulllogl;
};

#endif

