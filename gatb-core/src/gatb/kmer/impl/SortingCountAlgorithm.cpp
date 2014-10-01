/*****************************************************************************
 *   GATB : Genome Assembly Tool Box
 *   Copyright (C) 2014  INRIA
 *   Authors: R.Chikhi, G.Rizk, E.Drezen
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

/********************************************************************************/
// We include required definitions
/********************************************************************************/

//#define PROTO_COMP
#include <gatb/kmer/impl/SortingCountAlgorithm.hpp>
#include <gatb/tools/misc/impl/Stringify.hpp>
#include <cmath>

#define DEBUG(a)  //printf a

/********************************************************************************/
// We use the required packages
/********************************************************************************/
using namespace std;

using namespace gatb::core::system;
using namespace gatb::core::system::impl;

using namespace gatb::core::bank;
using namespace gatb::core::bank::impl;

using namespace gatb::core::tools::collections;
using namespace gatb::core::tools::collections::impl;

using namespace gatb::core::tools::storage::impl;

using namespace gatb::core::tools::misc;
using namespace gatb::core::tools::misc::impl;

using namespace gatb::core::tools::dp;
using namespace gatb::core::tools::dp::impl;

using namespace gatb::core::tools::math;

using namespace gatb::core::kmer::impl;


/********************************************************************************/
namespace gatb  {  namespace core  {   namespace kmer  {   namespace impl {
/********************************************************************************/

/********************************************************************************/
static const char* progressFormat1 = "DSK: Pass %d/%d, Step 1: partitioning    ";
static const char* progressFormat2 = "DSK: Pass %d/%d, Step 2: counting kmers  ";

	
	
	
	
	
	
	
	
	
/*********************************************************************
** METHOD  :
** PURPOSE :
** INPUT   :
** OUTPUT  :
** RETURN  :
** REMARKS :  
R: why are we repeating those long initializations for all constructors?
R: I think it has to do with: http://stackoverflow.com/questions/761917/handling-a-class-with-a-long-initialization-list-and-multiple-constructors
*********************************************************************/
template<size_t span>
SortingCountAlgorithm<span>::SortingCountAlgorithm ()
    : Algorithm("dsk", 0, 0),
      _storage(0),
      _bank(0),
      _kmerSize(0), _abundance(0),
      _partitionType(0), _nbCores(0), _prefix(""),
      _progress (0),
      _estimateSeqNb(0), _estimateSeqTotalSize(0), _estimateSeqMaxSize(0),
      _max_disk_space(0), _max_memory(0), _volume(0), _nb_passes(0), _nb_partitions(0), _current_pass(0),
      _histogram (0), _histogramUri(""),
      _partitionsStorage(0), _partitions(0), _totalKmerNb(0), _solidCounts(0), _solidKmers(0) ,_nbCores_per_partition(1) ,_nb_partitions_in_parallel(0)
{
}

/*********************************************************************
** METHOD  :
** PURPOSE :
** INPUT   :
** OUTPUT  :
** RETURN  :
** REMARKS :
*********************************************************************/
template<size_t span>
SortingCountAlgorithm<span>::SortingCountAlgorithm (
    Storage* storage,
    gatb::core::bank::IBank* bank,
    size_t      kmerSize,
    size_t      abundance,
    u_int32_t   max_memory,
    u_int64_t   max_disk_space,
    size_t      nbCores,
    size_t      partitionType,
    const std::string& prefix,
    const std::string& histogramUri,
    gatb::core::tools::misc::IProperties* options
)
  : Algorithm("dsk", nbCores, options),
    _storage(storage),
    _bank(0),
    _kmerSize(kmerSize), _abundance(abundance),
    _partitionType(partitionType), _nbCores(nbCores), _prefix(prefix),
    _progress (0),
    _estimateSeqNb(0), _estimateSeqTotalSize(0), _estimateSeqMaxSize(0),
    _max_disk_space(max_disk_space), _max_memory(max_memory), _volume(0), _nb_passes(0), _nb_partitions(0), _current_pass(0),
    _histogram (0), _histogramUri(histogramUri),
    _partitionsStorage(0), _partitions(0), _totalKmerNb(0), _solidCounts(0), _solidKmers(0) ,_nbCores_per_partition (1) ,_nb_partitions_in_parallel (nbCores)
{
    setBank (bank);

    /** We create the collection corresponding to the solid kmers output. */
    setSolidCounts (& (*_storage)("dsk").getCollection<Count> ("solid"));

    /** We set the histogram instance. */
    setHistogram (new Histogram  (10000, & (*_storage)("dsk").getCollection<Histogram::Entry>("histogram") ));
}

/*********************************************************************
** METHOD  :
** PURPOSE :
** INPUT   :
** OUTPUT  :
** RETURN  :
** REMARKS :
*********************************************************************/
template<size_t span>
SortingCountAlgorithm<span>::SortingCountAlgorithm (tools::storage::impl::Storage& storage)
  : Algorithm("dsk", 0, 0),
    _storage(&storage),
    _bank(0),
    _kmerSize(0), _abundance(0),
    _partitionType(0), _nbCores(0), _prefix(""),
    _progress (0),
    _estimateSeqNb(0), _estimateSeqTotalSize(0), _estimateSeqMaxSize(0),
    _max_disk_space(0), _max_memory(0), _volume(0), _nb_passes(0), _nb_partitions(0), _current_pass(0),
    _histogram (0), _histogramUri(""),
    _partitionsStorage(0), _partitions(0), _totalKmerNb(0), _solidCounts(0), _solidKmers(0) ,_nbCores_per_partition(1),_nb_partitions_in_parallel(0)
{
    Group& group = (*_storage)(this->getName());

    /** We create the collection corresponding to the solid kmers output. */
    setSolidCounts (& group.getCollection<Count> ("solid"));

    string xmlString = group.getProperty ("xml");
    stringstream ss; ss << xmlString;   getInfo()->readXML (ss);
}

/*********************************************************************
** METHOD  :
** PURPOSE :
** INPUT   :
** OUTPUT  :
** RETURN  :
** REMARKS :
*********************************************************************/
template<size_t span>
SortingCountAlgorithm<span>::~SortingCountAlgorithm ()
{
    setProgress          (0);
    setBank              (0);
    setPartitionsStorage (0);
    setPartitions        (0);
    setSolidCounts       (0);
    setHistogram         (0);
}

/*********************************************************************
** METHOD  :
** PURPOSE :
** INPUT   :
** OUTPUT  :
** RETURN  :
** REMARKS :
*********************************************************************/
template<size_t span>
SortingCountAlgorithm<span>& SortingCountAlgorithm<span>::operator= (const SortingCountAlgorithm& s)
{
    if (this != &s)
    {
        _storage                = s._storage;
        _kmerSize               = s._kmerSize;
        _abundance              = s._abundance;
        _partitionType          = s._partitionType;
        _nbCores                = s._nbCores;
		_nbCores_per_partition  = s._nbCores_per_partition;
		_nb_partitions_in_parallel = s._nb_partitions_in_parallel;
        _prefix                 = s._prefix;
        _estimateSeqNb          = s._estimateSeqNb;
        _estimateSeqTotalSize   = s._estimateSeqTotalSize;
        _estimateSeqMaxSize     = s._estimateSeqMaxSize;
        _max_disk_space         = s._max_disk_space;
        _max_memory             = s._max_memory;
        _volume                 = s._volume;
        _nb_passes              = s._nb_passes;
        _nb_partitions          = s._nb_partitions;
        _current_pass           = s._current_pass;
        _histogramUri           = s._histogramUri;
        _totalKmerNb            = s._totalKmerNb;

        setBank                 (s._bank);
        setProgress             (s._progress);
        setHistogram            (s._histogram);
        setPartitionsStorage    (s._partitionsStorage);
        setPartitions           (s._partitions);
        setSolidCounts          (s._solidCounts);
    }
    return *this;
}

/*********************************************************************
** METHOD  :
** PURPOSE :
** INPUT   :
** OUTPUT  :
** RETURN  :
** REMARKS :
*********************************************************************/
template<size_t span>
void SortingCountAlgorithm<span>::execute ()
{
	

	
    /** We retrieve the actual number of cores. */
    _nbCores = getDispatcher()->getExecutionUnitsNumber();
	_nb_partitions_in_parallel = _nbCores;
    assert (_nbCores > 0);

    /** We configure dsk by computing the number of passes and partitions we will have
     * according to the allowed disk and memory space. */
    configure (_bank);

    /** We create the sequences iterator. */
    Iterator<Sequence>* itSeq = _bank->iterator();
    LOCAL (itSeq);

    /** We configure the progress bar. */
	printf("progress inited with %lli (vol %lli) ", 2 * _volume * MBYTE / sizeof(Type),_volume);
    setProgress ( createIteratorListener (2 * _volume * MBYTE / sizeof(Type), "counting kmers"));
    _progress->init ();

	PartiInfo<5> * pInfo = new PartiInfo<5>(_nb_partitions, 8); //minimsize here et ailleurs
	
    /*************************************************************/
    /*                         MAIN LOOP                         */
    /*************************************************************/
    /** We loop N times the bank. For each pass, we will consider a subset of the whole kmers set of the bank. */
    for (_current_pass=0; _current_pass<_nb_passes; _current_pass++)
    {
        DEBUG (("SortingCountAlgorithm<span>::execute  pass [%ld,%d] \n", _current_pass+1, _nb_passes));

		pInfo->clear();
		
        /** 1) We fill the partition files. */
        fillPartitions (_current_pass, itSeq,pInfo);

		
		//return ;
        /** 2) We fill the kmers solid file from the partition files. */
        fillSolidKmers (_solidCounts->bag(),pInfo);
		
		//pInfo->printInfo();

    }

	//debug
	
	delete pInfo;
	
	

	
    _progress->finish ();

    /** We flush the solid kmers file. */
    _solidCounts->bag()->flush();

    /** We save the histogram if any. */
    _histogram->save ();
	

    /** compute auto cutoff **/
    _histogram->compute_threshold ();

	/** store auto cutoff and corresponding number of solid kmers **/
	Collection<NativeInt64>& storecutoff =   (*_storage)("dsk").getCollection<NativeInt64>("cutoff") ;
	storecutoff.insert(_histogram->get_solid_cutoff());
	storecutoff.flush();
	
	Collection<NativeInt64>& storesolids =   (*_storage)("dsk").getCollection<NativeInt64>("nbsolidsforcutoff") ;
	storesolids.insert(_histogram->get_nbsolids_auto());
	storesolids.flush();
	


    /** We want to remove physically the partitions. */
    _partitions->remove ();

    u_int64_t nbSolids = _solidCounts->iterable()->getNbItems();

    /** We gather some statistics. */
    getInfo()->add (1, "stats");
    getInfo()->add (2, "kmers_nb_valid",     "%ld", _totalKmerNb);
    getInfo()->add (2, "kmers_nb_solid",     "%ld", nbSolids);
    getInfo()->add (2, "kmers_nb_weak",      "%ld", _totalKmerNb - nbSolids);
    if (_totalKmerNb > 0)  {  getInfo()->add (2, "kmers_percent_weak", "%.1f", 100.0 - 100.0 * (double)nbSolids / (double)_totalKmerNb  );  }

    getInfo()->add (1, getTimeInfo().getProperties("time"));

    /** We save (as metadata) some information. */
    (*_storage)("dsk").addProperty ("kmer_size", Stringify::format("%d", _kmerSize));
}

/*********************************************************************
** METHOD  :
** PURPOSE :
** INPUT   :
** OUTPUT  :
** RETURN  :
** REMARKS :
*********************************************************************/
template<size_t span>
void SortingCountAlgorithm<span>::configure (IBank* bank)
{
    float load_factor = 0.7;

	
    // optimism == 1 mean that we guarantee worst case the memory usage,
    // any value above assumes that, on average, a k-mer will be seen 'optimism' times
    int optimism = 0; // guarantees always work, above 0 : assumes kmer seen (optimism+1) times (with optim 1  'OAHash: max rehashes..'  happened sometimes)

    /** We get some information about the bank. */
    bank->estimate (_estimateSeqNb, _estimateSeqTotalSize, _estimateSeqMaxSize);

    // We get the available space (in MBytes) of the current directory.
    u_int64_t available_space = System::file().getAvailableSpace (System::file().getCurrentDirectory()) / 1024;

    u_int64_t kmersNb  = (_estimateSeqTotalSize - _estimateSeqNb * (_kmerSize-1));
    u_int64_t bankSize = _estimateSeqTotalSize / MBYTE;

    _volume =  kmersNb * sizeof(Type) / MBYTE;  // in MBytes
	
	if (_volume == 0)   { _volume = 1; }    // tiny files fix

	 u_int64_t volume_minim = _volume * 0.5 *1.2 ; //0.5 for using kxmers   1.2 if bad repartition of minimizers ( todo sampling to assert ram usage)
	
	
    if (volume_minim == 0)   { volume_minim = 1; }    // tiny files fix

    if (_max_disk_space == 0)  { _max_disk_space = std::min (available_space/2, bankSize);  }
    if (_max_disk_space == 0)  { _max_disk_space = 10000; }

    if (_max_memory == 0)  {  _max_memory = System::info().getMemoryProject(); }
    if (_max_memory == 0)  {  _max_memory = 1000; }

    assert (_max_disk_space > 0);
    
	//_nb_passes = ( (_volume/3) / _max_disk_space ) + 1; //minim, approx volume /3
	_nb_passes = 1; //do not constrain nb passes on disk space anymore (anyway with minim, not very big)
	//increase it only if ram issue
	
	printf("_volume  %lli volume_minim %lli _max_disk_space %lli  _nb_passes init %i  \n", _volume,volume_minim,_max_disk_space,_nb_passes);
    size_t max_open_files = System::file().getMaxFilesNumber() / 2;
    u_int64_t volume_per_pass;

    do  {

        assert (_nb_passes > 0);
        volume_per_pass = volume_minim / _nb_passes;

        assert (_max_memory > 0);
		printf("volume_per_pass %lli  _nbCores %zu _max_memory %i \n",volume_per_pass, _nbCores,_max_memory);

       // _nb_partitions  = ( (volume_per_pass*_nbCores) / _max_memory ) + 1;

		_nb_partitions  = ( ( volume_per_pass* _nb_partitions_in_parallel) / _max_memory ) + 1;

		
		printf("nb passes  %i  (nb part %i / %zu)\n",_nb_passes,_nb_partitions,max_open_files);
		//_nb_partitions = max_open_files; break;
		
		if (_nb_partitions >= max_open_files &&  _nb_partitions_in_parallel >1)   { _nb_partitions_in_parallel  = _nb_partitions_in_parallel /2;  }
        else if (_nb_partitions >= max_open_files && _nb_partitions_in_parallel == 1)   { _nb_passes++;  }
        else                                    { break;         }

		printf("update nb passes  %i  (nb part %i / %zu)\n",_nb_passes,_nb_partitions,max_open_files);
    } while (1);

	if (_nb_partitions < 50 &&  (max_open_files - _nb_partitions  > 30) ) _nb_partitions += 30; //some more does not hurt
	
	//then put _nbCores_per_partition
	
	_nbCores_per_partition =  _nbCores / _nb_partitions_in_parallel ; //how to best distrib available cores ?
	// with this formula we'll sometimes use less than _nbCores (maybe enforce _nb_partitions_in_parallel is power of two ?)
	printf("_nbCores %zu  _nb_partitions_in_parallel %zu   _nbCores_per_partition %zu \n",_nbCores,_nb_partitions_in_parallel,_nbCores_per_partition);

	assert(_nbCores_per_partition > 0);
	
    /** We gather some statistics. */
    getInfo()->add (1, "config");
    getInfo()->add (2, "kmer_size",         "%ld", _kmerSize);
    getInfo()->add (2, "abundance",         "%ld", _abundance);
    getInfo()->add (2, "available_space",   "%ld", available_space);
    getInfo()->add (2, "bank_size",         "%ld", bankSize);
    getInfo()->add (2, "sequence_number",   "%ld", _estimateSeqNb);
    getInfo()->add (2, "sequence_volume",   "%ld", _estimateSeqTotalSize / MBYTE);
    getInfo()->add (2, "kmers_number",      "%ld", kmersNb);
    getInfo()->add (2, "kmers_volume",      "%ld", _volume);
    getInfo()->add (2, "max_disk_space",    "%ld", _max_disk_space);
    getInfo()->add (2, "max_memory",        "%ld", _max_memory);
    getInfo()->add (2, "nb_passes",         "%d",  _nb_passes);
    getInfo()->add (2, "nb_partitions",     "%d",  _nb_partitions);
    getInfo()->add (2, "nb_bits_per_kmer",  "%d",  Type::getSize());
    getInfo()->add (2, "nb_cores",          "%d",  getDispatcher()->getExecutionUnitsNumber());
	getInfo()->add (2, "nb_cores_per_partition",     "%d",  _nbCores_per_partition);
    getInfo()->add (2, "nb_partitions_in_parallel",  "%d",  _nb_partitions_in_parallel);

   // getInfo()->add (2, "partition_type",    "%d",  _partitionType);
}

/********************************************************************************/

template<size_t span>
class FillPartitions
{
public:


    /** Shortcut. */
    typedef typename SortingCountAlgorithm<span>::Type                  Type;
	//typedef typename SortingCountAlgorithm<span>::ModelDirect           ModelDirect;
	typedef typename SortingCountAlgorithm<span>::ModelCanonical             ModelCanonical;
    typedef typename SortingCountAlgorithm<span>::Model 	Model;

	
	 typedef typename Model::Kmer  Kmer;
	
	
	
	
	void compact_and_insert_superkmer ( u_int64_t minimk, std::vector<Kmer *>  &superKp)
    {
	
		
		
		if ((minimk % nbPass) == pass && minimk!=1000000000 ) //check if falls into pass
		{
		//	u_int64_t reduced_kmer = minimk / nbPass; //opt 1 pass
			u_int64_t reduced_kmer = minimk ;
			
			
			size_t p = _repart_table [reduced_kmer];
			

			int64_t zero = 0;
			Type masknt ((int64_t) 3);
			

			Type radix, radix_kxmer_forward ,radix_kxmer ;
			
			Type nbK ((int64_t) superKp.size());
			Type compactedK(zero);
			
			int maxs = (compactedK.getSize() - 8 ) ;
			
			Type mm (minimk);
			//printf("new minim %s      \n",mm.toString(system::g_msize).c_str());

			//printf("K0 %s      \n",superK[0].toString(31).c_str());

			
			bool prev_which = superKp[0]->which();
			size_t kx_size =0;
			
			radix_kxmer_forward =  (superKp[0]->value() & _mask_radix) >> ((_kmersize - 4)*2);

			for (int ii = 1 ; ii <superKp.size(); ii++) {
				//printf("K%i %s      \n",ii,superK[ii].toString(31).c_str());

				//compute here stats on  kx mer
				//tant que tai <= xmer et which kmer[ii] == which kmer [ii-1] --> cest un kxmer
				//do the same in sampling : gives ram estimation
				if(superKp[ii]->which() != prev_which || kx_size >= _kx) // kxmer_size = 1 //cost should diminish with larger kxmer
				{
					//output kxmer size kx_size,radix_kxmer
					//kx mer is composed of superKp[ii-1] superKp[ii-2] .. superKp[ii-n] with nb elems  n  == kxmer_size +1  (un seul kmer ==k+0)
					if(prev_which)
					{
						radix_kxmer = radix_kxmer_forward;
					}
					else // si revcomp, le radix du kxmer est le debut du dernier kmer
					{
						radix_kxmer =  (superKp[ii-1]->value() & _mask_radix) >> ((_kmersize - 4)*2);
					}

					_local_pInfo.incKmer_and_rad(p, radix_kxmer.getVal(),kx_size ); //nb of superkmer per x per parti per radix

					radix_kxmer_forward =  (superKp[ii]->value() & _mask_radix) >> ((_kmersize - 4)*2);
					kx_size =0;
				}
				else
				{
					kx_size++;
				}
				
				prev_which = superKp[ii]->which() ;

				compactedK = compactedK << 2  ;
				compactedK = compactedK | ( (superKp[ii]->forward()) & masknt) ;

			}
			
			//record last kx mer
			if(prev_which)
			{
				radix_kxmer = radix_kxmer_forward;
			}
			else // si revcomp, le radix du kxmer est le debut du dernier kmer
			{
				radix_kxmer =  (superKp.back()->value() & _mask_radix) >> ((_kmersize - 4)*2);
			}
			
			_local_pInfo.incKmer_and_rad(p, radix_kxmer.getVal(),kx_size );

			
			compactedK = compactedK | (  nbK << maxs ) ; // 56 = nb bits in type - 8
			
			//then output first and amorce
			//		printf("WRITE A %llx\n",compactedK.getVal());
			//		printf("WRITE B %llx\n",debk.getVal());
			
			_partition[p].insert (compactedK);

			_partition[p].insert (superKp[0]->forward());
			//_partition[p].insert (superK[0]);

			
			///// end compact
	
			nbWrittenKmers+= superKp.size();
		}
		
	}
	
    void operator() (Sequence& sequence)
    {
		

		
		std::vector<Kmer *>  superKp;
		

		u_int64_t prevmin = 1000000000 ;
		int nb_superk = 1;
		superKp.clear();

		Type temp;
		

		
		int maxs = (temp.getSize() - _mm )/2 ;

		
		


		if (model.build (sequence.getData(), kmers) == false)  { return; }

//		if(kmers.size()> 0)
//		{
//			 prevmin =  kmers[0].minimizer().value().getVal() ;
//		}
		

        for (size_t i=0; i<kmers.size(); i++)
        {
			
			if (kmers[i].isValid() == false)  {
				
			//on onvalid kmer : output previous superk utput prev
				compact_and_insert_superkmer( prevmin,superKp); superKp.clear();
				prevmin = 1000000000; //marking will have to restart 'from new'
				continue;
			
			}

			u_int64_t h =  kmers[i].minimizer().value().getVal();
			if(prevmin == 1000000000) prevmin= h;

			
			if(   h!= prevmin || superKp.size() >= maxs) //could use (kmers[i].hasChanged() && i > 0
			{
				nb_superk ++;
				
				compact_and_insert_superkmer( prevmin,superKp);
				
				superKp.clear(); superKp.push_back( & (kmers[i]));
			}
			else
			{
				superKp.push_back( & (kmers[i]));
			}

			prevmin =  h;
			
        }
		
		//output last superK
		compact_and_insert_superkmer( prevmin,superKp);

        if (nbWrittenKmers > 500000)   {  _progress.inc (nbWrittenKmers);  nbWrittenKmers = 0;  }
    }


    FillPartitions (Model& model, size_t nbPasses, size_t currentPass, Partition<Type>* partition, u_int32_t max_memory, IteratorListener* progress, unsigned int * repart_table, int minim_size, PartiInfo<5> & pInfo)
	: model(model), pass(currentPass), nbPass(nbPasses), nbPartitions(partition->size()), nbWrittenKmers(0), _repart_table (repart_table),_mm(minim_size),

      _partition (*partition,1<<12,0), _extern_pInfo(pInfo) , _local_pInfo(nbPartitions,_mm), _kmersize(model.getKmerSize()),
	_progress  (progress,System::thread().newSynchronizer()) ,_kx(4)  {
		
		_mask_radix =  (int64_t) 255 ;
		_mask_radix =  _mask_radix << ((_kmersize - 4)*2); //get first 4 nt  of the kmers (heavy weight)

	}

	 ~FillPartitions ()
	{
		//add to global parti_info
		_extern_pInfo += _local_pInfo;
		
	}
private:

	Type _mask_radix;
	
    /** Local resources. */
    Model&    model;
    size_t    pass;
    size_t    nbPass;
    size_t    nbPartitions;
    size_t    nbWrittenKmers;
	int _kx;
    Data      binaryData;
    vector<Kmer> kmers;
	unsigned int * _repart_table;
	int _mm;
	int _kmersize;
	PartiInfo<5> & _extern_pInfo;
	PartiInfo<5>  _local_pInfo;
	
    /** Shared resources (must support concurrent accesses). */ //PartitionCacheSorted
#ifdef PROTO_COMP
    PartitionCacheSorted<Type> _partition;
#else
    PartitionCache<Type> _partition;
#endif
    
    ProgressSynchro _progress;
};

	
	

/********************************************************************************/
	
	//ie ce sera posible d avoir plus dinfo , estim ram max par exemple ?
template<size_t span>
class SampleRepart
{
public:
	
	
	/** Shortcut. */
	typedef typename SortingCountAlgorithm<span>::Type                  Type;
	typedef typename SortingCountAlgorithm<span>::ModelCanonical             ModelCanonical;
	typedef typename SortingCountAlgorithm<span>::Model 	Model;
	
	
	typedef typename Model::Kmer  Kmer;
	
	
	
	void operator() (Sequence& sequence)
	{

		
		//printf("Sample Sequence %s _binsize %p \n",sequence.getComment().c_str(),_binsize);
		
		
		u_int64_t prevmin = 1000000000 ; //invalid mark
		
		int superk_size =0;
		Type temp;
		int maxs = (temp.getSize() - _mm )/2 ;
		
		if (model.build (sequence.getData(), kmers) == false)  { return; }
//		
//		if(kmers.size()> 0)
//		{
//			temp = kmers[0].minimizer().value(); prevmin = temp.getVal() ;
//			
//		}
//		

        for (size_t i=0; i<kmers.size(); i++)
        {
			
			if (kmers[i].isValid() == false)  {
				
				//count previous
				if ((prevmin % nbPass) == pass && prevmin!=1000000000)
					_local_pInfo.incSuperKmer_per_minimBin(prevmin);
				
	
				
				prevmin=1000000000; continue;
			
			}

			
			u_int64_t h = kmers[i].minimizer().value().getVal();
			if(prevmin == 1000000000) prevmin= h;

			if(  h!= prevmin  || superk_size >= maxs)
			{
				if ((prevmin % nbPass) == pass)
					_local_pInfo.incSuperKmer_per_minimBin(prevmin);
				
				superk_size =1;
			}
			else
			{
				superk_size ++;
			}
			
			prevmin =  h;
			
        }
		if ((prevmin % nbPass) == pass && prevmin!=1000000000)
			_local_pInfo.incSuperKmer_per_minimBin(prevmin);
		

		nbseqread++;
		
		if (nbseqread > 1000)   {  _progress.inc (nbseqread);  nbseqread = 0;  }

		
    }
	
	
	SampleRepart (Model& model, size_t nbPasses, size_t currentPass, int nbparti, u_int32_t max_memory, IteratorListener* progress, int minim_size, PartiInfo<5> & pInfo)
	: model(model), pass(currentPass), nbPass(nbPasses), nbPartitions(nbparti), nbseqread(0), _mm(minim_size),
	 _extern_pInfo(pInfo) , _local_pInfo(nbparti,minim_size),
	_progress  (progress,System::thread().newSynchronizer())
	{ }
	
	
	~SampleRepart ()
	{
		//add to global parti_info
		_extern_pInfo += _local_pInfo;
		
	}
private:
	
	/** Local resources. */
	Model&    model;
	size_t    pass;
	size_t    nbPass;
	size_t    nbPartitions;
	size_t    nbseqread;
	size_t    _mm_size;
	u_int64_t _nb_minims;
	Data      binaryData;
    vector<Kmer> kmers;

	PartiInfo<5> & _extern_pInfo;
	PartiInfo<5>  _local_pInfo;
	
	int _mm;

	ProgressSynchro _progress;

};
	
	
	
	//giv it the binsize estimation
	//method distribute will compute even distrib of bins across n partitions, result in repart table
class Repartitor
{
public:
	
	typedef std::pair<int,int> ipair;
	
	struct mycomp {
		bool operator() (ipair l,ipair r) { return l.first > r.first; }
	} pair_comp;
	
	
	
	void computeDistrib ()
	{
		u_int64_t _nb_minims =   1 << (_mm*2) ;
		_repart_table = (unsigned int *)  calloc(_nb_minims , sizeof(unsigned int));
		
		_space_left = ( int *)  calloc(_nbpart, sizeof(unsigned int));
		
		std::vector<ipair> bin_size_vec;
		
		
		
		//sum total bins size
		u_int64_t sumsizes =0;
		for (int ii=0; ii< _nb_minims; ii++) {
			sumsizes +=   _extern_pInfo.getNbSuperKmer_per_minim(ii); // _binsize[ii];
			bin_size_vec.push_back(ipair( _extern_pInfo.getNbSuperKmer_per_minim(ii) ,ii));
		}
		u_int64_t mean_size =  sumsizes /  _nbpart;
		//printf("mean size per parti :  %lli  (total %lli )\n",mean_size,sumsizes);
		
		//init space left
		for (int jj = 0; jj < _nbpart; jj++) {
			_space_left[jj] = mean_size;
		}
		
		//sort minim bins per size
		std::sort (bin_size_vec.begin (), bin_size_vec.end (),pair_comp);
		
		//debug
//				printf("100 largest bin sizes \n");
//				for(int ii=0; ii<100; ii++ ) //_nb_minims 100  print largest bins
//				{
//					printf("binsize [%i] = %i \n",bin_size_vec[ii].second,bin_size_vec[ii].first);
//				}
		
		
		int cur_minim = 0;
		//loop 2 times over partitions
		for (int nbfill=0; nbfill<2 && (cur_minim < _nb_minims); nbfill++) {
			
			//loop over parts, fill them with bins until each part is full
			for (int jj = 0; jj < _nbpart; jj++) {
				while(
					  (( _space_left[jj] - bin_size_vec[cur_minim].first  )>0   ||  _space_left[jj] == mean_size) // if part empty, allow depassement
					  &&  (cur_minim < _nb_minims))
				{
					//affect bin cur_minim to partition jj
					_repart_table[bin_size_vec[cur_minim].second] = jj;
					_space_left[jj] -= bin_size_vec[cur_minim].first;
					cur_minim++;
					
					//printf("affected minim %i to part %i  space left %i  (msize %i) \n",bin_size_vec[cur_minim-1].second,jj,_space_left[jj],bin_size_vec[cur_minim-1].first);
					
				}
			}
			
		}
		
		

		
		//debug info
		//		for(int ii=0; ii<_nbpart; ii++ )
		//		{
		//			printf("space left [%i] = %i \n",ii,_space_left[ii]);
		//		}
		
		
		//affect remainder randomly
		//printf("will affect %i smallest  bins randomly \n",cur_minim);
		if(cur_minim < _nb_minims)
		{
			for(int ii=cur_minim; ii<_nb_minims; ii++ )
			{
				_repart_table[bin_size_vec[ii].second] = simplehash16 (NativeInt64(ii),0)  % _nbpart;
			}
		}
		
		
		
		//swap second with last
		//swap 3rd with n-5   : idea : when parall several parti, parti 0,1,2 are potentially the largest, scatter them
		//or todo  dynamic nb_partitions_in_parralel
		if(_nbpart> 30)
		{
			int second_goto =   _nbpart-1;
			int fourth_goto =   _nbpart-5;

			
			for(int ii=0; ii<_nb_minims; ii++ )
				{
					if(_repart_table[ii]== 1 ) _repart_table[ii]= second_goto;
					else if(_repart_table[ii]== 3 ) _repart_table[ii]= fourth_goto;
					else if(_repart_table[ii]== second_goto ) _repart_table[ii]= 1;
					else if(_repart_table[ii]== fourth_goto ) _repart_table[ii]= 3;
				}
			
			
			
			

		}
		
		
		
		//debug info
		//		printf("repart  table \n");
		//		for(int ii=0; ii<_nb_minims; ii++ )
		//		{
		//			printf("repart table [%i] = %i \n",ii,_repart_table[ii]);
		//		}
		
		
		free(_space_left);
		
	}
	
	unsigned int * getRepartTable()
	{
		if(_repart_table==NULL)
			this->computeDistrib();
		
		return _repart_table;
	}
	
	Repartitor(int nbpart, int minimsize,PartiInfo<5> & pInfo) : _nbpart(nbpart), _mm(minimsize), _extern_pInfo(pInfo)
	{
		_repart_table = NULL;
	}
	
private:
	
	
	
	unsigned int * _repart_table ;
	int * _space_left ;
	PartiInfo<5> & _extern_pInfo;

	int _nbpart;
	int _mm;
};
	
	

	
/*********************************************************************
** METHOD  :
** PURPOSE :
** INPUT   :
** OUTPUT  :
** RETURN  :
** REMARKS :
*********************************************************************/
template<size_t span>
void SortingCountAlgorithm<span>::fillPartitions (size_t pass, Iterator<Sequence>* itSeq, PartiInfo<5> * pInfo)
{
    TIME_INFO (getTimeInfo(), "fill_partitions");

    DEBUG (("SortingCountAlgorithm<span>::fillPartitions  pass \n", pass));

    /** We create a kmer model. */
	Model model (_kmerSize,8 , CustomMinimizer(8)); //minimsize here et ailleurs
	
	int mmsize = model.getMmersModel().getKmerSize();
	
	//printf(" k / m   %zu  mmsize %i\n",_kmerSize,mmsize);
	
    /** We delete the previous partitions storage. */
    if (_partitionsStorage)  { _partitionsStorage->remove (); }

    /** We create the partition files for the current pass. */
#ifdef PROTO_COMP
    setPartitionsStorage (StorageFactory(STORAGE_COMPRESSED_FILE).create ("partitions", true, false));
    //setPartitionsStorage (StorageFactory(STORAGE_GZFILE).create ("partitions", true, false));
#else
    setPartitionsStorage (StorageFactory(STORAGE_FILE).create ("partitions", true, false));
#endif
    
	setPartitions        (0); // close the partitions first, otherwise new files are opened before  closing parti from previous pass

    setPartitions        ( & (*_partitionsStorage)().getPartition<Type> ("parts", _nb_partitions));

    /** We update the message of the progress bar. */
    _progress->setMessage (progressFormat1, _current_pass+1, _nb_passes);

    /** We launch the iteration of the sequences iterator with the created functors. */
    // R to E: typical question from a puzzled developer: what's this last argument of iterate() (15*1000)? so to answer this, I need to find the definition of iterate(), where is it? maybe to answer this, where does getDispatcher come from? any way we could search http://gatb-core.gforge.inria.fr/ to quickly find out?
	


	TruncateIterator<Sequence> it_sample (*itSeq, 10000000); //sample with first 5  million reads ? //mettre max de nb reads, 5M reads,  10 % des reads ?

	//fill bin sizes here
	
	PartiInfo<5> * sample_info = new PartiInfo<5>(_nb_partitions,mmsize);
	
	gatb::core::tools::dp::IteratorListener* _progress_sample = createIteratorListener (10000000, "Collecting stats on read sample");
	_progress_sample->init();
    getDispatcher()->iterate (it_sample,  SampleRepart<span> (model, _nb_passes, pass,_nb_partitions, _max_memory, _progress_sample,mmsize,*pInfo), 15*1000);
	_progress_sample->finish();

	
	Repartitor repartitor (_partitions->size(),mmsize,*pInfo);

	repartitor.computeDistrib();


	//repartitor.getRepartTable()
    getDispatcher()->iterate (itSeq, FillPartitions<span> (model, _nb_passes, pass, _partitions, _max_memory, _progress,repartitor.getRepartTable(),mmsize,*pInfo), 15*1000);
	

	

}

/*********************************************************************
** METHOD  :
** PURPOSE :
** INPUT   :
** OUTPUT  :
** RETURN  :
** REMARKS :
*********************************************************************/
template<size_t span>
std::vector<size_t> SortingCountAlgorithm<span>::getNbCoresList ()
{
    std::vector<size_t> result;

    for (size_t p=0; p<_nb_partitions; )
    {
        size_t i=0;  for (i=0; i< _nb_partitions_in_parallel && p<_nb_partitions; i++, p++)  {}
		//ici get ram chaque parti pour parall dynamique ? une ou plusieurs parti a la fois selon  taille parti ?
        result.push_back (i);
    }

    return result;
}

/*********************************************************************
** METHOD  :
** PURPOSE :
** INPUT   :
** OUTPUT  :
** RETURN  :
** REMARKS :
*********************************************************************/
template<size_t span>
void SortingCountAlgorithm<span>::fillSolidKmers (Bag<Count>*  solidKmers, PartiInfo<5> * pInfo)
{
    TIME_INFO (getTimeInfo(), "fill_solid_kmers");

    DEBUG (("SortingCountAlgorithm<span>::fillSolidKmers\n"));

    /** We update the message of the progress bar. */
    _progress->setMessage (progressFormat2, _current_pass+1, _nb_passes);

    /** We retrieve the list of cores number for dispatching N partitions in N threads.
     *  We need to know these numbers for allocating the N maps according to the maximum allowed memory.
     */
    vector<size_t> coreList = getNbCoresList(); //uses _nb_partitions_in_parallel

	//for serial mode
	//SerialDispatcher * sd  = new SerialDispatcher();
	
    size_t p = 0;
    for (size_t i=0; i<coreList.size(); i++)
    {

        vector<ICommand*> cmds;

        size_t currentNbCores = coreList[i];
        assert (currentNbCores > 0);

        /** We correct the number of memory per map according to the max allowed memory.
         * Note that _max_memory has initially been divided by the user provided cores number. */
        u_int64_t mem = (_max_memory*MBYTE)/currentNbCores;

        ISynchronizer* synchro = System::thread().newSynchronizer();
        LOCAL (synchro);

        for (size_t j=0; j<currentNbCores; j++, p++)
        {
            ICommand* cmd = 0;

            /** We get the length of the current partition file. */
            size_t partitionLen = (*_partitions)[p].getNbItems(); // hmm this will be unknown for a gz file, maybe estimation possible du coup utilise le hash

            /* Get the memory taken by this partition if loaded for sorting */
            uint64_t memoryPartition = partitionLen * sizeof(Type);

			//only use by vector now
//            if (memoryPartition >= mem)
//            {
//                cmd = new PartitionsByHashCommand<span>   (solidKmers, (*_partitions)[p], _histogram, synchro, _totalKmerNb, _abundance, _progress, mem);
//            }
//            else
            {
                cmd = new PartitionsByVectorCommand<span> (solidKmers, (*_partitions)[p], _histogram, synchro, _totalKmerNb, _abundance, _progress,pInfo,p,_nbCores_per_partition, _kmerSize);
            }

            cmds.push_back (cmd);
        }

        getDispatcher()->dispatchCommands (cmds, 0);

    }

}

/********************************************************************************/

/********************************************************************************/
} } } } /* end of namespaces. */
/********************************************************************************/
