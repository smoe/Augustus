#ifndef _JG_TRANSCRIPT_HPP
#define _JG_TRANSCRIPT_HPP

#include <stdlib.h>
#include <string.h>
#include <cstring>
#include <iostream>
#include <list>
#include <unordered_map>
#include <algorithm>
#include <fstream>

using namespace std;

class Gene;
class Transcript;

class Exon{
 public:
    int from, to;
    string chr;
    string feature;
    float score;
    int frame;
    int tomany;
    int toless; // MARIO: tooFew
    int penalty;
    int distance;
    bool isUtr () { return (frame == -1); }
    bool operator<(Exon const& rhs) const {
	if (from != rhs.from)
	    return (from < rhs.from);
	else
	    return (to < rhs.to);
    }
};

class Transcript{
 public:
    list<Transcript*> supporter;		// at the moment these list only consist the pointer to transcripts, who are totaly equal to this transcript (doesnt matter whether their creation is related)
    list<Exon> exon_list;				// saves every feature (cds, utr, exon [cds+utr]
    Gene* parent;
    string source;
    string t_id;
    char strand;
    int tis;							// position of first base in translation sequence (translation initation site)
    int tes;							// position of last base in translation sequence (translation end site)
    int priority;
    pair<int,int> pred_range;
    pair<bool,bool> tx_complete;		// transcript end complete at <lower,higher> base number
    pair<bool,bool> tl_complete;		// translation end complete at <start,stop> codon
    pair<bool,bool> separated_codon;	// separated <start,stop> codon
    pair<unsigned int,unsigned int> boha;		// pred range boundary status at <start/stop> side: 0-no problem, 1-too close, 2-too close but last exon erased (MARIO)
    pair<Transcript*,Transcript*> joinpartner;	// pointer to transcript that was used to fulfill the <start,stop> codon side
    pair<Transcript*,Transcript*> utr_joinpartner;	// pointer to transcript that was used to fulfill the <downstream-UTR,upstream-UTR> codon side
    pair<list<Transcript*>,list<Transcript*>> descendant;		// pointer to new transcripts arise from these with <start,stop>-join
    pair<Transcript*,Transcript*> correction_ancestor;
    bool must_be_deleted;

    double penalty;

    void initiate(){
	exon_list.sort();				// essential for the next steps
	exontoutr();
	tes_to_cds();					// needs utr structure!
    }
    int getcdsfront(){
	for (list<Exon>::iterator it = exon_list.begin(); it != exon_list.end(); it++){
	    if ((*it).feature == "CDS"){
		return (*it).from;
	    }
	}
	return -1;
    }
    int getcdsback(){
	for (list<Exon>::reverse_iterator it = exon_list.rbegin(); it != exon_list.rend(); it++){
	    if ((*it).feature == "CDS"){
		return (*it).to;
	    }
	}
	return -1;
    }
    void tes_to_cds(){
	int cdsfront = getcdsfront();
	int cdsback = getcdsback();	
	if (strand == '+'){
	    if (tl_complete.second){
		if (cdsback != tes){
		    for (list<Exon>::iterator it = exon_list.begin(); it != exon_list.end(); it++){
			if ((*it).feature == "UTR" && cdsback < (*it).from){
			    if ((*it).from == cdsback + 1){
				int temp = (*it).to - (*it).from + 1;
				if (temp > 3){
				    (*it).from += 3;
				    it--;
				    (*it).to += 3;
				}else if (temp == 3){
				    it = exon_list.erase(it);
				    it--;
				    (*it).to += 3;
				}else{
				    it = exon_list.erase(it);
				    it--;
				    (*it).to += temp;
				    it++;
				    if ((*it).to - (*it).from + 1 > 3 - temp){
					Exon new_cds = (*it);
					new_cds.from = (*it).from;
					new_cds.to = (*it).from + (3 - (temp + 1));
					new_cds.feature = "CDS";
					new_cds.frame = (new_cds.to - new_cds.from + 1) % 3;
					(*it).from += 3 - temp;
					exon_list.insert(it, new_cds);
				    }else if ((*it).to - (*it).from + 1 == 3 - temp){
					(*it).feature = "CDS";
					(*it).frame = 0;
				    }else{
					cerr << "Fatal error: An exon with start/stop codon part is not even 3 bases long." << endl;
					exit( EXIT_FAILURE );
				    }
				}
			    }else{
				Exon new_cds = (*it);
				new_cds.from = (*it).from;
				new_cds.to = (*it).from + 2;
				new_cds.feature = "CDS";
				new_cds.frame = 0;
				(*it).from += 3;
				exon_list.insert(it, new_cds);
			    }
			    break;
			}
		    }
		}
	    }else{
		tes = cdsback;
	    }
	    if (!tl_complete.first){
		tis = cdsfront;
	    }
	}else{
	    if (tl_complete.second){
		if (cdsfront != tes){

		    for (list<Exon>::iterator it = exon_list.begin(); it != exon_list.end(); it++){
			if ((*it).feature == "CDS" && cdsfront == (*it).from){
			    if(it == exon_list.end()){
				cerr << "Fatal error: No exon for an existing codon." << endl;
				exit( EXIT_FAILURE );
			    }
			    it--;
			    if ((*it).to == cdsfront - 1){
				int temp = (*it).to - (*it).from + 1;
				if (temp > 3){
				    (*it).to -= 3;
				    it++;
				    (*it).from -= 3;
				}else if (temp == 3){
				    it = exon_list.erase(it);
				    (*it).from -= 3;
				}else{
				    it = exon_list.erase(it);
				    (*it).from -= temp;
				    it--;
				    if ((*it).to - (*it).from + 1 > 3 - temp){
					Exon new_cds = (*it);
					new_cds.to = (*it).to;
					new_cds.from = (*it).to - (3 - (temp + 1));
					new_cds.feature = "CDS";
					new_cds.frame = (new_cds.to - new_cds.from + 1) % 3;
					(*it).to -= 3 - temp;
					it++;
					exon_list.insert(it, new_cds);
				    }else if ((*it).to - (*it).from + 1 == 3 - temp){
					(*it).feature = "CDS";
					(*it).frame = 0;
				    }else{
					cerr << "Fatal error: An exon with start/stop codon part is not even 3 bases long." << endl;
					exit( EXIT_FAILURE );
				    }
				}
			    }else{
				Exon new_cds = (*it);
				new_cds.to = (*it).to;
				new_cds.from = (*it).to - 2;
				new_cds.feature = "CDS";
				new_cds.frame = 0;
				(*it).from -= 3;
				it++;
				exon_list.insert(it, new_cds);
			    }
			    break;
			}
		    }
		}
	    }else{
		tes = cdsfront;
	    }
	    if (!tl_complete.first){
		tis = cdsback;
	    }
	}
    }

    void exontoutr(){
	list<Exon>::iterator it_prev;
	list<Exon>::iterator it_next;
	for (list<Exon>::iterator it = exon_list.begin(); it != exon_list.end(); it++){
	    if ((*it).feature == "exon"){
		if (it != exon_list.begin()){
		    it_prev = it;
		    it_prev--;
		    if ((*it_prev).feature == "CDS"){
			if ((*it_prev).to >= (*it).from){
			    if ((*it_prev).from == (*it).from && (*it_prev).to == (*it).to){
				it = exon_list.erase(it);
				it--;
				continue;
			    }else if ((*it_prev).from == (*it).from){
				(*it).from = (*it_prev).to + 1;
				(*it).feature = "UTR";
				continue;
			    }else{cout << "unexpected case in exontoutr() nr 1" << endl;}
			}
		    }
		}
		it_next = it;
		it_next++;
		if (it_next != exon_list.end()){
		    if ((*it_next).feature == "CDS"){
			if ((*it).to >= (*it_next).from){
			    if ((*it_next).from == (*it).from && (*it_next).to == (*it).to){
				it = exon_list.erase(it);
				it--;
				continue;
			    }else if ((*it_next).to == (*it).to){
				(*it).to = (*it_next).from - 1;
				(*it).feature = "UTR";
				continue;
			    }else if ((*it).to > (*it_next).to){
				(*it).feature = "UTR";
				Exon new_utr = (*it);
				new_utr.from = (*it_next).to + 1;
				(*it).to = (*it_next).from - 1;
				it_next++;
				exon_list.insert(it_next, new_utr);
				continue;
			    }else{cout << "unexpected case in exontoutr() nr 2" << endl;}
			}
		    }
		}
		(*it).feature = "UTR";
	    }
	}
	//				cout << t_id << endl;
	//				for (list<Exon>::iterator it2 = exon_list.begin(); it2 != exon_list.end(); it2++){cout << (*it2).feature << " ";}
	//				cout << endl;
    }
    bool operator<(Transcript const& rhs) const {
	if (min(tis,tes) != min(rhs.tis,rhs.tes))
	    return (min(tis,tes) < min(rhs.tis,rhs.tes));
	else
	    return (max(tis,tes) < max(rhs.tis,rhs.tes));
    }
    Transcript(){
	tx_complete.first = false;
	tx_complete.second = false;
	tl_complete.first = false;
	tl_complete.second = false;
    }
};

class Gene{
 public:
    string g_id;
    list<Transcript*> children;
};

class Point{
 public:
    double i_x;
    double i_y;
    int x;
    int y;

    bool operator<(Point const& rhs) const {
	if (y != rhs.y)
	    return (y > rhs.y);
	else
	    return (x < rhs.x);
    }
};

void divide_in_overlaps_and_conquer(list<Transcript> &transcript_list, string &outfilename, int errordistance);
void work_at_overlap(list<Transcript*> &overlap, list<Transcript> &new_transcripts, int errordistance);
void toclosetoborder(list<Transcript*> &overlap, list<Transcript> &new_transcripts, char strand, int errordistance);
void search_n_destroy_doublings(list<Transcript*> &overlap, int errordistance, bool ab_initio);
bool compare_transcripts(Transcript const* t1, Transcript const* t2);
pair<bool,bool> is_part_of(Transcript const* t1, Transcript const* t2);
bool compare_priority(Transcript const* lhs, Transcript const* rhs);
void join(list<Transcript*> &overlap, list<Transcript> &new_transcripts, char side);
void joining(Transcript* t1, Transcript* t2, char strand, char side, Transcript &tx_new, int fitting_case);
int is_combinable(Transcript const* t1, Transcript const* t2, char strand, char side);
bool check_frame_annotation(Transcript const &transcript);
void eval_gtf(list<Transcript*> &overlap, int errordistance);
bool strandeq(Exon ex1, Exon ex2, char strand);
void output_exon_list(Transcript const* tx);							// only for semantic tests
void scatter_plot(list<Point> input);									// only for some tests
void weight_info(list<Transcript*> &overlap);

#endif