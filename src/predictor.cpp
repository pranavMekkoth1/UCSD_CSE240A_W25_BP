//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <math.h>
#include "predictor.h"
#include <string.h>

//
// TODO:Student Information
//
const char *studentName = "Pranav Mekkoth";
const char *studentID = "A16785376";
const char *email = "pmekkoth@ucsd.edu";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = {"Static", "Gshare",
                         "Tournament", "Custom"};

// define number of bits required for indexing the BHT here.
int ghistoryBits = 17; // Number of bits used for Global History
int bpType;            // Branch Prediction Type
int verbose;

//tournament predictor variables (will need to figure out the propper widths for these)
int LocalHist_Bits=13;
int LocalPred_Bits=13;
int GlobalPred_Bits=15;
int ChooserBits=15;

//below are the original values for tournament predictor:
//int LocalHist_Bits=13;
//int LocalPred_Bits=13;
//int GlobalPred_Bits=15;
//int ChooserBits=15;

//custom predictor variables
int BimodalBits=13; //need 13 bits for 8192 entries
int Tage1Bits=13; //need 13 bits for 8192 entries
int Tage2Bits=12; //need 12 bits for 4096 entries
int Tage3Bits=11; //need 11 bits for 2048 entries
int Tage4Bits=10; //need 10 bits for 1024 entries 
int Tage5Bits=9; //need 9 bits for 512 entries

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

//
// TODO: Add your own Branch Predictor data structures here
//

// gshare
uint8_t *bht_gshare; //this is the branch history table for gshare (pointer to it)
uint64_t ghistory; //this is the global history register for gshare

//tournament predictor: Alpha 21264
uint16_t *LocalHistTable; //pointer to the local predictor 1 (10 bit local history)
uint8_t *LocalPredictTable; //pointer to the local predictor 2 (2 bit saturation counter per entry)
uint8_t *GlobalPredict; //pointer to the global predictor (2 bit saturation counter per entry)
uint8_t *Chooser; //pointer to the chooser predictor
uint64_t ghistory_tournament; //global history register for tournament predictor


//custom predictor: Tage branch predictor
uint8_t *BimodalTable; //pointer to the bimodal predictor (2 bit saturation counter per entry)

typedef struct{//this is the tage table entry

  uint64_t tag; //tag for the entry (6-14 bit)
  uint8_t saturating_counter; //saturating counter for the entry (2-bit)
  uint8_t use_counter; //useful counter (4-bit)

} tageEntry;

tageEntry *Tage1Table; //pointer to the tage predictor 1 
tageEntry *Tage2Table; //pointer to the tage predictor 2 
tageEntry *Tage3Table; //pointer to the tage predictor 3 
tageEntry *Tage4Table; //pointer to the tage predictor 4 
tageEntry *Tage5Table; //pointer to the tage predictor 5 
//uint16_t *PartialTag; //pointer to entries in the partial tag table (18 to 16 bit tags per entry)

uint64_t ghistory_custom; //this is the global history register for the custom predictor
//uint16_t phr_custom; //this is the pattern history register for the custom predictor



//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//

// gshare functions
void init_gshare()
{
  int bht_entries = 1 << ghistoryBits; //this is the number of entries in the BHT, 2^ghistoryBits (ghistoryBits=17, so 2^17=131072 entries)
  bht_gshare = (uint8_t *)malloc(bht_entries * sizeof(uint8_t)); //allocate memory for the BHT
  int i = 0;
  for (i = 0; i < bht_entries; i++) //iterate through the BHT
  {
    bht_gshare[i] = WN; //initialize all entries to weakly not taken
  }
  ghistory = 0; //initialize the ghistory register to 0
}

uint8_t gshare_predict(uint32_t pc)
{
  // get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits; //get again the number of entries in the BHT
  uint32_t pc_lower_bits = pc & (bht_entries - 1); //get the lower bits of the pc
  uint32_t ghistory_lower_bits = ghistory & (bht_entries - 1); //get the lower bits of the glhistory register
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits; //get the index for the BHT by XORing the lower bits of the pc and ghistory
  switch (bht_gshare[index]) //return the prediction based on the state of the entry in the BHT
  {
  case WN:
    return NOTTAKEN;
  case SN:
    return NOTTAKEN;
  case WT:
    return TAKEN;
  case ST:
    return TAKEN;
  default:
    printf("Warning: Undefined state of entry in GSHARE BHT!\n");
    return NOTTAKEN;
  }
}

void train_gshare(uint32_t pc, uint8_t outcome)
{
  // get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits; //get again the number of entries in the BHT
  uint32_t pc_lower_bits = pc & (bht_entries - 1); //get the lower bits of the pc
  uint32_t ghistory_lower_bits = ghistory & (bht_entries - 1);//get the lower bits of the glhistory register
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;//XOR the lower bits of the pc and ghistory

  // Update state of entry in bht based on outcome
  switch (bht_gshare[index])
  {
  case WN:
    bht_gshare[index] = (outcome == TAKEN) ? WT : SN; //if the outcome is taken, set to weakly taken, else set to strong not taken
    break;
  case SN:
    bht_gshare[index] = (outcome == TAKEN) ? WN : SN; //if the outcome is taken, set to weakly not taken, else set to strong not taken
    break;
  case WT:
    bht_gshare[index] = (outcome == TAKEN) ? ST : WN; //if the outcome is taken, set to strong taken, else set to weakly not taken
    break;
  case ST:
    bht_gshare[index] = (outcome == TAKEN) ? ST : WT; //if the outcome is taken, set to strong taken, else set to weakly taken
    break;
  default:
    printf("Warning: Undefined state of entry in GSHARE BHT!\n");
    break;
  }

  // Update history register
  ghistory = ((ghistory << 1) | outcome);
}

void cleanup_gshare()
{
  free(bht_gshare);
}


//Tournament predictor functions:
void init_tournament(){
  uint32_t localHist_entries= 1 << LocalHist_Bits; //get the number of entries in the local history table
  uint32_t localPred_entries= 1 << LocalPred_Bits; //get the number of entries in the local predictor
  uint32_t globalPred_entries= 1 << GlobalPred_Bits; //get the number of entries in the global predictor
  uint32_t chooser_entries= 1 << ChooserBits; //get the number of entries in the chooser predictor

  LocalHistTable= (uint16_t*)malloc(localHist_entries*sizeof(uint16_t)); //allocate memory for the local history table
  LocalPredictTable=(uint8_t*)malloc(localPred_entries*sizeof(uint8_t)); //allocate memory for the local predictor
  GlobalPredict=(uint8_t*)malloc(globalPred_entries*sizeof(uint8_t)); //allocate memory for the global predictor
  Chooser=(uint8_t*)malloc(chooser_entries*sizeof(uint8_t)); //allocate memory for the chooser predictor

  int i=0; 
  for(i=0;i<localHist_entries;i++){ //iterate through the local history table
    LocalHistTable[i]= 0; //initialize all entries to weakly not taken
  }
  for(i=0;i<localPred_entries;i++){//iterate through the 2nd local predictor
    LocalPredictTable[i]=WN; //initialize all entries to weakly not taken
  }
  int j=0;
  for(j=0;j<globalPred_entries;j++){//iterate through the global predictor
    GlobalPredict[j]=WN; //initialize all entries to weakly not taken
  }
  for(j=0;j<chooser_entries;j++){//iterate through the chooser predictor
    Chooser[i]=choose_global; //initialize all entries to choosing the global predictor initially
  }
  ghistory_tournament=0; //initialize the global history register to 0
}

uint8_t tournament_predict(uint32_t pc){
  uint32_t localHist_entries= 1 << LocalHist_Bits; //get the number of entries in the local history table (10 bits wide or 2^13=8192 entries)
  uint32_t localPred_entries= 1 << LocalPred_Bits; //get the number of entries in the local predictor table (2 bits wide or 2^12=8192 entries)
  uint32_t globalPred_entries= 1 << GlobalPred_Bits; //get the number of entries in the global predictor(15 bits wide or 2^15=32768 entries)
  uint32_t chooser_entries= 1 << ChooserBits; //get the number of entries in the chooser predictor(15 bits wide or 2^15=32768 entries)
 
  uint32_t pc_lower_bits=pc&(localHist_entries-1); //get the lower bits of the pc (pc_lower_bits is : 0-4095)
  uint32_t globalhistReg_bits=ghistory_tournament&(globalPred_entries-1); //get the lower bits of the global history register (globalhistReg_bits is : 0-32767)
  
  int indexLP1=pc_lower_bits & (localHist_entries-1); //get the index for the local history table
  int indexLP2=LocalHistTable[indexLP1] & (localPred_entries-1); //get the index for the local predictor 2
  int indexGP= globalhistReg_bits & (GlobalPred_Bits-1); //get the index for the global predictor
  int indexChooser=globalhistReg_bits & (chooser_entries-1); //get the index for the chooser predictor

  if(Chooser[indexChooser]==choose_global){ //if the chooser index we are pointing to says to choose global, look at global predictor entry
    switch(GlobalPredict[indexGP]){ //look at the global predictor entry 
      case SN://if strongly not taken
        return NOTTAKEN;
      case WN://if weakly not taken
        return NOTTAKEN;
      case WT://if weakly taken
        return TAKEN;
      case ST://if strongly taken
        return TAKEN;
    }
  }
  else if(Chooser[indexChooser]==choose_local){ //if the chooser index we are pointing to says to choose local, look at local predictor entry
    switch(LocalPredictTable[indexLP2]){
      case SN://if strongly not taken
        return NOTTAKEN;
      case WN://if weakly not taken
        return NOTTAKEN;
      case WT://if weakly taken
        return TAKEN;
      case ST://if strongly taken
        return TAKEN;
    }
  }
  return NOTTAKEN;
}


void train_tournament(uint32_t pc, uint8_t outcome){
  int localHist_entries= 1 << LocalHist_Bits; //get the number of entries in the local history table (10 bits wide or 2^13=8192 entries)
  int localPred_entries= 1 << LocalPred_Bits; //get the number of entries in the local predictor table (2 bits wide or 2^12=8192 entries)
  int globalPred_entries= 1 << GlobalPred_Bits; //get the number of entries in the global predictor(15 bits wide or 2^15=32768 entries)
  int chooser_entries= 1 << ChooserBits; //get the number of entries in the chooser predictor(15 bits wide or 2^15=32768 entries)

  uint32_t pc_lower_bits=pc&(localHist_entries-1); //get the lower bits of the pc (pc_lower_bits is : 0-4095)
  uint32_t globalhistReg_bits=ghistory_tournament&(globalPred_entries-1); //get the lower bits of the global history register (globalhistReg_bits is : 0-32767)
  
  int indexLP1=pc_lower_bits & (localHist_entries-1); //get the index for the local predictor 1
  int indexLP2=LocalHistTable[indexLP1] & (localPred_entries-1); //get the index for the local predictor 2
  int indexGP= globalhistReg_bits & (GlobalPred_Bits-1); //get the index for the global predictor
  int indexChooser=globalhistReg_bits & (chooser_entries-1); //get the index for the chooser predictor

  if(Chooser[indexChooser]== choose_global){ //if the chooser chooses global predictor
    switch(GlobalPredict[indexGP]){
      case SN: //if strongly not taken

        GlobalPredict[indexGP]= (outcome==TAKEN)? WN: SN; //if the outcome is taken, set to weakly not taken, else set to strong not taken
        if(GlobalPredict[indexGP]== WN){ //if we made the wrong prediction
          //no need to change chooser choice since we are are still correctly predicting not taken after update
        
          ghistory_tournament=((ghistory_tournament<<1)|outcome); //update the global history register by shifting left and adding the outcome
          LocalHistTable[indexLP1]=((LocalHistTable[indexLP1]<<1)|outcome); //update the local history table entry by shifting left and adding the outcome
          switch(LocalPredictTable[indexLP2]){ //update the local predictor entry based on the outcome
            case SN:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? WN: SN; //if the outcome is taken, set to weakly not taken, else set to strong not taken
              break;
            case WN:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? WT: SN; //if the outcome is taken, set to weakly taken, else set to strong not taken
              break;
            case WT:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? ST: WN; //if the outcome is taken, set to strong taken, else set to weakly not taken
              break;
            case ST:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? ST: WT; //if the outcome is taken, set to strong taken, else set to weakly taken
              break;
          }
         
        }
        else{ //if GlobalPredict[indexP1]= Strongly not taken that means that the prediction was correct
          //no need to update chooser because chooser was right
          ghistory_tournament=((ghistory_tournament<<1)|outcome); //update the global history register by shifting left and adding the outcome
          LocalHistTable[indexLP1]=((LocalHistTable[indexLP1]<<1)|outcome); //update the local history table entry by shifting left and adding the outcome
          switch(LocalPredictTable[indexLP2]){ //update the local predictor entry as well
            case SN:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? WN: SN; //if the outcome is taken, set to weakly not taken, else set to strong not taken
              break;
            case WN:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? WT: SN; //if the outcome is taken, set to weakly taken, else set to strong not taken
              break;
            case WT:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? ST: WN; //if the outcome is taken, set to strong taken, else set to weakly not taken
              break;
            case ST:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? ST: WT; //if the outcome is taken, set to strong taken, else set to weakly taken
              break;
          }
        }
        break;

      case WN: //if weakly not taken
        GlobalPredict[indexGP]= (outcome==TAKEN)? WT: SN; 

        if(GlobalPredict[indexGP]== WT){//this means that the prediction was incorrect 

          Chooser[indexChooser]=choose_local; //update the chooser to choose local predictor
          ghistory_tournament=((ghistory_tournament<<1)|outcome); //update the global history register by shifting left and adding the outcome
          LocalHistTable[indexLP1]=((LocalHistTable[indexLP1]<<1)|outcome); //update the local history table entry by shifting left and adding the outcome
          switch(LocalPredictTable[indexLP2]){ //update the local predictor entry based on the outcome
            case SN:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? WN: SN; //if the outcome is taken, set to weakly not taken, else set to strong not taken
              break;
            case WN:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? WT: SN; //if the outcome is taken, set to weakly taken, else set to strong not taken
              break;
            case WT:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? ST: WN; //if the outcome is taken, set to strong taken, else set to weakly not taken
              break;
            case ST:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? ST: WT; //if the outcome is taken, set to strong taken, else set to weakly taken
              break;
          }

        }
        else if(GlobalPredict[indexGP]== SN){ //this means our prediction was correct 
          //no need to update chooser since we were correct
          ghistory_tournament=((ghistory_tournament<<1)|outcome); //update the global history register by shifting left and adding the outcome
          LocalHistTable[indexLP1]=((LocalHistTable[indexLP1]<<1)|outcome); //update the local history table entry by shifting left and adding the outcome
          switch(LocalPredictTable[indexLP2]){ //update the local predictor entry as well
            case SN:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? WN: SN; //if the outcome is taken, set to weakly not taken, else set to strong not taken
              break;
            case WN:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? WT: SN; //if the outcome is taken, set to weakly taken, else set to strong not taken
              break;
            case WT:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? ST: WN; //if the outcome is taken, set to strong taken, else set to weakly not taken
              break;
            case ST:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? ST: WT; //if the outcome is taken, set to strong taken, else set to weakly taken
              break;
          }
        }

        break;
      case WT: //if weakly taken
        GlobalPredict[indexGP]= (outcome==TAKEN)? ST: WN; 
        if(GlobalPredict[indexGP]== ST){ //we made the correct prediction
          //no need to update chooser
          ghistory_tournament=((ghistory_tournament<<1)|outcome); //update the global history register by shifting left and adding the outcome
          LocalHistTable[indexLP1]=((LocalHistTable[indexLP1]<<1)|outcome); //update the local history table entry by shifting left and adding the outcome
          switch(LocalPredictTable[indexLP2]){ //update the local predictor entry as well
            case SN:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? WN: SN; //if the outcome is taken, set to weakly not taken, else set to strong not taken
              break;
            case WN:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? WT: SN; //if the outcome is taken, set to weakly taken, else set to strong not taken
              break;
            case WT:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? ST: WN; //if the outcome is taken, set to strong taken, else set to weakly not taken
              break;
            case ST:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? ST: WT; //if the outcome is taken, set to strong taken, else set to weakly taken
              break;
          }
        }
        else{ //we made the wrong prediction
          Chooser[indexChooser]=choose_local; //update the chooser to choose local predictor
          ghistory_tournament=((ghistory_tournament<<1)|outcome); //update the global history register by shifting left and adding the outcome
          LocalHistTable[indexLP1]=((LocalHistTable[indexLP1]<<1)|outcome); //update the local history table entry by shifting left and adding the outcome
          switch(LocalPredictTable[indexLP2]){ //update the local predictor entry based on the outcome
            case SN:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? WN: SN; //if the outcome is taken, set to weakly not taken, else set to strong not taken
              break;
            case WN:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? WT: SN; //if the outcome is taken, set to weakly taken, else set to strong not taken
              break;
            case WT:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? ST: WN; //if the outcome is taken, set to strong taken, else set to weakly not taken
              break;
            case ST:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? ST: WT; //if the outcome is taken, set to strong taken, else set to weakly taken
              break;
          }
        }
        break;
      case ST: //if strongly taken
        GlobalPredict[indexGP]= (outcome==TAKEN)? ST: WT; 
        if(GlobalPredict[indexGP]== ST){ //correct prediction made
          //no need to update chooser
          ghistory_tournament=((ghistory_tournament<<1)|outcome); //update the global history register by shifting left and adding the outcome
          LocalHistTable[indexLP1]=((LocalHistTable[indexLP1]<<1)|outcome); //update the local history table entry by shifting left and adding the outcome
          switch(LocalPredictTable[indexLP2]){ //update the local predictor entry as well
            case SN:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? WN: SN; //if the outcome is taken, set to weakly not taken, else set to strong not taken
              break;
            case WN:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? WT: SN; //if the outcome is taken, set to weakly taken, else set to strong not taken
              break;
            case WT:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? ST: WN; //if the outcome is taken, set to strong taken, else set to weakly not taken
              break;
            case ST:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? ST: WT; //if the outcome is taken, set to strong taken, else set to weakly taken
              break;
          }
        }
        else if(GlobalPredict[indexGP== WT]){//wrong prediction 
          //no need to update chooser since we would still be predicting Taken
          ghistory_tournament=((ghistory_tournament<<1)|outcome); //update the global history register by shifting left and adding the outcome
          LocalHistTable[indexLP1]=((LocalHistTable[indexLP1]<<1)|outcome); //update the local history table entry by shifting left and adding the outcome
          switch(LocalPredictTable[indexLP2]){ //update the local predictor entry as well
            case SN:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? WN: SN; //if the outcome is taken, set to weakly not taken, else set to strong not taken
              break;
            case WN:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? WT: SN; //if the outcome is taken, set to weakly taken, else set to strong not taken
              break;
            case WT:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? ST: WN; //if the outcome is taken, set to strong taken, else set to weakly not taken
              break;
            case ST:
              LocalPredictTable[indexLP2]= (outcome==TAKEN)? ST: WT; //if the outcome is taken, set to strong taken, else set to weakly taken
              break;
          }

        }
        break;
    }
  }
  else if(Chooser[indexChooser]== choose_local){
    switch(LocalPredictTable[indexLP2]){
      case SN:
        LocalPredictTable[indexLP2]= (outcome==TAKEN)? WN: SN; 

        if(LocalPredictTable[indexLP2]== SN){ //correct predition
          //no need to change the chooser entry
          ghistory_tournament=((ghistory_tournament<<1)|outcome); //update the global history register by shifting left and adding the outcome
          LocalHistTable[indexLP1]=((LocalHistTable[indexLP1]<<1)|outcome); //update the local history table entry by shifting left and adding the outcome
          switch(GlobalPredict[indexGP]){ //update the global predictor entry based on the outcome
            case SN:
              GlobalPredict[indexGP]= (outcome==TAKEN)? WN: SN; //if the outcome is taken, set to weakly not taken, else set to strong not taken
              break;
            case WN:
              GlobalPredict[indexGP]= (outcome==TAKEN)? WT: SN; //if the outcome is taken, set to weakly taken, else set to strong not taken
              break;
            case WT:
              GlobalPredict[indexGP]= (outcome==TAKEN)? ST: WN; //if the outcome is taken, set to strong taken, else set to weakly not taken
              break;
            case ST:
              GlobalPredict[indexGP]= (outcome==TAKEN)? ST: WT; //if the outcome is taken, set to strong taken, else set to weakly taken
              break;
          }
        }
        else if(LocalPredictTable[indexLP2]== WN){ //incorrect prediction
          //no need to update chooser since new prediction still predicts not taken
          ghistory_tournament=((ghistory_tournament<<1)|outcome); //update the global history register by shifting left and adding the outcome
          LocalHistTable[indexLP1]=((LocalHistTable[indexLP1]<<1)|outcome); //update the local history table entry by shifting left and adding the outcome
          switch(GlobalPredict[indexGP]){ //update the global predictor entry based on the outcome
            case SN:
              GlobalPredict[indexGP]= (outcome==TAKEN)? WN: SN; //if the outcome is taken, set to weakly not taken, else set to strong not taken
              break;
            case WN:
              GlobalPredict[indexGP]= (outcome==TAKEN)? WT: SN; //if the outcome is taken, set to weakly taken, else set to strong not taken
              break;
            case WT:
              GlobalPredict[indexGP]= (outcome==TAKEN)? ST: WN; //if the outcome is taken, set to strong taken, else set to weakly not taken
              break;
            case ST:
              GlobalPredict[indexGP]= (outcome==TAKEN)? ST: WT; //if the outcome is taken, set to strong taken, else set to weakly taken
              break;
          }
        }
        break;
      case WN:
        LocalPredictTable[indexLP2]= (outcome==TAKEN)? WT: SN; 

        if(LocalPredictTable[indexLP2]== WT){//this is an incorrect prediction
          Chooser[indexChooser]=choose_global; //update the chooser to choose global predictor
          ghistory_tournament=((ghistory_tournament<<1)|outcome); //update the global history register by shifting left and adding the outcome
          LocalHistTable[indexLP1]=((LocalHistTable[indexLP1]<<1)|outcome); //update the local history table entry by shifting left and adding the outcome
          switch(GlobalPredict[indexGP]){ //update the global predictor entry based on the outcome
            case SN:
              GlobalPredict[indexGP]= (outcome==TAKEN)? WN: SN; //if the outcome is taken, set to weakly not taken, else set to strong not taken
              break;
            case WN:
              GlobalPredict[indexGP]= (outcome==TAKEN)? WT: SN; //if the outcome is taken, set to weakly taken, else set to strong not taken
              break;
            case WT:
              GlobalPredict[indexGP]= (outcome==TAKEN)? ST: WN; //if the outcome is taken, set to strong taken, else set to weakly not taken
              break;
            case ST:
              GlobalPredict[indexGP]= (outcome==TAKEN)? ST: WT; //if the outcome is taken, set to strong taken, else set to weakly taken
              break;
          }
        }
        else if(LocalPredictTable[indexLP2]== SN){ //correct prediction
            //no need to change the chooser entry
          ghistory_tournament=((ghistory_tournament<<1)|outcome); //update the global history register by shifting left and adding the outcome
          LocalHistTable[indexLP1]=((LocalHistTable[indexLP1]<<1)|outcome); //update the local history table entry by shifting left and adding the outcome
          switch(GlobalPredict[indexGP]){ //update the global predictor entry based on the outcome
            case SN:
              GlobalPredict[indexGP]= (outcome==TAKEN)? WN: SN; //if the outcome is taken, set to weakly not taken, else set to strong not taken
              break;
            case WN:
              GlobalPredict[indexGP]= (outcome==TAKEN)? WT: SN; //if the outcome is taken, set to weakly taken, else set to strong not taken
              break;
            case WT:
              GlobalPredict[indexGP]= (outcome==TAKEN)? ST: WN; //if the outcome is taken, set to strong taken, else set to weakly not taken
              break;
            case ST:
              GlobalPredict[indexGP]= (outcome==TAKEN)? ST: WT; //if the outcome is taken, set to strong taken, else set to weakly taken
              break;
          }
        }

        break;
      case WT:
        LocalPredictTable[indexLP2]= (outcome==TAKEN)? ST: WN; 

        if(LocalPredictTable[indexLP2]== ST){ //we made correct prediction
          //no need to update chooser
          ghistory_tournament=((ghistory_tournament<<1)|outcome); //update the global history register by shifting left and adding the outcome
          LocalHistTable[indexLP1]=((LocalHistTable[indexLP1]<<1)|outcome); //update the local history table entry by shifting left and adding the outcome
          switch(GlobalPredict[indexGP]){ //update the global predictor entry based on the outcome
            case SN:
              GlobalPredict[indexGP]= (outcome==TAKEN)? WN: SN; //if the outcome is taken, set to weakly not taken, else set to strong not taken
              break;
            case WN:
              GlobalPredict[indexGP]= (outcome==TAKEN)? WT: SN; //if the outcome is taken, set to weakly taken, else set to strong not taken
              break;
            case WT:
              GlobalPredict[indexGP]= (outcome==TAKEN)? ST: WN; //if the outcome is taken, set to strong taken, else set to weakly not taken
              break;
            case ST:
              GlobalPredict[indexGP]= (outcome==TAKEN)? ST: WT; //if the outcome is taken, set to strong taken, else set to weakly taken
              break;
          }
        }
        else{ //we made an incorrect prediction
          Chooser[indexChooser]=choose_global; //update the chooser to choose global predictor
          ghistory_tournament=((ghistory_tournament<<1)|outcome); //update the global history register by shifting left and adding the outcome
          LocalHistTable[indexLP1]=((LocalHistTable[indexLP1]<<1)|outcome); //update the local history table entry by shifting left and adding the outcome
          switch(GlobalPredict[indexGP]){ //update the global predictor entry based on the outcome
            case SN:
              GlobalPredict[indexGP]= (outcome==TAKEN)? WN: SN; //if the outcome is taken, set to weakly not taken, else set to strong not taken
              break;
            case WN:
              GlobalPredict[indexGP]= (outcome==TAKEN)? WT: SN; //if the outcome is taken, set to weakly taken, else set to strong not taken
              break;
            case WT:
              GlobalPredict[indexGP]= (outcome==TAKEN)? ST: WN; //if the outcome is taken, set to strong taken, else set to weakly not taken
              break;
            case ST:
              GlobalPredict[indexGP]= (outcome==TAKEN)? ST: WT; //if the outcome is taken, set to strong taken, else set to weakly taken
              break;
          }
        }
        break;

      case ST:
        LocalPredictTable[indexLP2]= (outcome==TAKEN)? ST: WT;

        if(LocalPredictTable[indexLP2]== ST){//correct prediction
          //no need to change chooser
          ghistory_tournament=((ghistory_tournament<<1)|outcome); //update the global history register by shifting left and adding the outcome
          LocalHistTable[indexLP1]=((LocalHistTable[indexLP1]<<1)|outcome); //update the local history table entry by shifting left and adding the outcome
          switch(GlobalPredict[indexGP]){ //update the global predictor entry based on the outcome
            case SN:
              GlobalPredict[indexGP]= (outcome==TAKEN)? WN: SN; //if the outcome is taken, set to weakly not taken, else set to strong not taken
              break;
            case WN:
              GlobalPredict[indexGP]= (outcome==TAKEN)? WT: SN; //if the outcome is taken, set to weakly taken, else set to strong not taken
              break;
            case WT:
              GlobalPredict[indexGP]= (outcome==TAKEN)? ST: WN; //if the outcome is taken, set to strong taken, else set to weakly not taken
              break;
            case ST:
              GlobalPredict[indexGP]= (outcome==TAKEN)? ST: WT; //if the outcome is taken, set to strong taken, else set to weakly taken
              break;
          }
        }
        else if(LocalPredictTable[indexLP2]== WT){//incorrect prediction
          //no need to change chooser since we will still be predicting taken
          ghistory_tournament=((ghistory_tournament<<1)|outcome); //update the global history register by shifting left and adding the outcome
          LocalHistTable[indexLP1]=((LocalHistTable[indexLP1]<<1)|outcome); //update the local history table entry by shifting left and adding the outcome
          switch(GlobalPredict[indexGP]){ //update the global predictor entry based on the outcome
            case SN:
              GlobalPredict[indexGP]= (outcome==TAKEN)? WN: SN; //if the outcome is taken, set to weakly not taken, else set to strong not taken
              break;
            case WN:
              GlobalPredict[indexGP]= (outcome==TAKEN)? WT: SN; //if the outcome is taken, set to weakly taken, else set to strong not taken
              break;
            case WT:
              GlobalPredict[indexGP]= (outcome==TAKEN)? ST: WN; //if the outcome is taken, set to strong taken, else set to weakly not taken
              break;
            case ST:
              GlobalPredict[indexGP]= (outcome==TAKEN)? ST: WT; //if the outcome is taken, set to strong taken, else set to weakly taken
              break;
          }
        }
        break;
    }
  }
  //updating the global history register is already done within the case statments
}


void cleanup_tournament(){
  //free all the data structures for the tournament predictor
  free(LocalHistTable);
  free(LocalPredictTable);
  free(GlobalPredict);
  free(Chooser); 
}


//custom predictor functions:
void init_custom(){
  int bimodalBits= 1 << BimodalBits; //get the number of entries in the bimodal predictor(2 bits wide or 2^13=8192 entries)
  int Tage1_bits= 1<< Tage1Bits; //get the number of entries in the tage1 predictor( 2^13=8192 entries)
  int Tage2_bits= 1<< Tage2Bits; //get the number of entries in the tage2 predictor( 2^12=4096 entries)
  int Tage3_bits= 1<< Tage3Bits; //get the number of entries in the tage3 predictor( 2^11=2048 entries)
  int Tage4_bits= 1<< Tage4Bits; //get the number of entries in the tage4 predictor( 2^10=1024 entries)
  int Tage5_bits= 1<< Tage5Bits; //get the number of entries in the tage5 predictor( 2^9=512 entries)

  BimodalTable= (uint8_t*)malloc(bimodalBits*sizeof(uint8_t)); //allocate memory for the bimodal predictor
  for(int i=0; i<bimodalBits; i++){ //initialize all entries to weakly not taken
    BimodalTable[i]=WN;
  }

  Tage1Table= (tageEntry *)malloc(Tage1_bits*sizeof(tageEntry)); //allocate memory for the tage1 predictor (each entry the size of the struct)
  for(int j=0; j<Tage1_bits; j++){ //initialize all entries in tage table 1
    Tage1Table[j].saturating_counter=WN;
    Tage1Table[j].tag=0;
    Tage1Table[j].use_counter=0;
  }
  
  Tage2Table= (tageEntry *)malloc(Tage2_bits *sizeof(tageEntry)); //allocate memory for the tage2 predictor (each entry the size of the struct)
  for(int j=0; j<Tage2_bits; j++){ //initialize all entries in tage table 2
    Tage2Table[j].saturating_counter=WN;
    Tage2Table[j].tag=0;
    Tage2Table[j].use_counter=0;
  }
  
  Tage3Table= (tageEntry *)malloc(Tage3_bits*sizeof(tageEntry)); //allocate memory for the tage3 predictor (each entry the size of the struct)
  for(int j=0; j<Tage3_bits; j++){ //initialize all entries in tage table 3
    Tage3Table[j].saturating_counter=WN;
    Tage3Table[j].tag=0;
    Tage3Table[j].use_counter=0;
  }
  
  Tage4Table= (tageEntry *)malloc(Tage4_bits*sizeof(tageEntry)); //allocate memory for the tage4 predictor (each entry the size of the struct)
  for(int j=0; j<Tage4_bits; j++){ //initialize all entries in tage table 4
    Tage4Table[j].saturating_counter=WN;
    Tage4Table[j].tag=0;
    Tage4Table[j].use_counter=0;
  }
  
  Tage5Table= (tageEntry *)malloc(Tage5_bits*sizeof(tageEntry)); //allocate memory for the tage5 predictor (each entry the size of the struct)
  for(int j=0; j<Tage5_bits; j++){ //initialize all entries in tage table 5
    Tage5Table[j].saturating_counter=WN;
    Tage5Table[j].tag=0;
    Tage5Table[j].use_counter=0;
  }

  ghistory_custom=0;//initialize the global history register to 0
}

uint8_t custom_predict(uint32_t pc){
  int bimodal_index= pc & (1<< BimodalBits)-1; //get the index for the bimodal predictor
  uint8_t prediction= NOTTAKEN; //initialize prediction to not taken
  uint8_t bimodal= BimodalTable[bimodal_index]; //get the bimodal prediction
  switch(bimodal){ //check the bimodal prediction
    case SN:
      prediction= NOTTAKEN;
      break;
    case WN:
      prediction= NOTTAKEN;
      break;
    case WT:
      prediction= TAKEN;
      break;
    case ST:
      prediction= TAKEN;
      break;
  }
  
  //now itterate from table with longest history entries to smallest this way we are able to make sure the 
  //history is not parter of a larger pattern before looking at smaller patterns (tage5 to tage1)

  int longestMatchTable=0;//this is the table number that has the longest match, if none match, fall back to Bimodalpredictor
  
  //start at tage table 5 (history length 13)
  int tage5_index= (pc ^ (ghistory_custom >> (64- 9))) % Tage5Bits; //get the index for the tage5 predictor
  if(Tage5Table[tage5_index].tag== (pc & ((1<< Tage5Bits)-1))){ //if the tag matches t
    longestMatchTable=5;//we found a match in this label
    prediction= Tage5Table[tage5_index].saturating_counter; //the saturating counter is the prediction
    return prediction; //since we found a match in this tage table, no need to teck others
  }

  //check table 4 (history length 12)
  int tage4_index= (pc ^ (ghistory_custom >> (64-10))) % Tage4Bits; //get the index for the tage4 predictor
  if(Tage4Table[tage4_index].tag== (pc & ((1<< Tage4Bits)-1))){ //if the tag matches
    longestMatchTable=4;//we found a match in this label
    prediction= Tage4Table[tage4_index].saturating_counter; //the saturating counter is the prediction
    return prediction; //since we found a match in this tage table, no need to teck others
  }

  //now check tage 3 table (history length 11)
  int tage3_index= (pc ^ (ghistory_custom >> (64-11))) % Tage3Bits; //get the index for the tage3 predictor
  if(Tage3Table[tage3_index].tag== (pc & ((1<< Tage3Bits)-1))){ //if the tag matches
    longestMatchTable=3;//we found a match in this label
    prediction= Tage3Table[tage3_index].saturating_counter; //the saturating counter is the prediction
    return prediction; //since we found a match in this tage table, no need to teck others
  }

  //check table 2 (history length 10)
  int tage2_index= (pc ^ (ghistory_custom >> (64-12))) % Tage2Bits; //get the index for the tage2 predictor
  if(Tage2Table[tage2_index].tag== (pc & ((1<< Tage2Bits)-1))){ //if the tag matches
    longestMatchTable=2;//we found a match in this label
    prediction= Tage2Table[tage2_index].saturating_counter; //the saturating counter is the prediction
    return prediction; //since we found a match in this tage table, no need to teck others
  }

  //check table 1 (history length 9)
  int tage1_index= (pc ^ (ghistory_custom >> (64-13))) % Tage1Bits; //get the index for the tage1 predictor
  if(Tage1Table[tage1_index].tag== (pc & ((1<< Tage1Bits)-1))){ //if the tag matches
    longestMatchTable=1;//we found a match in this label
    prediction= Tage1Table[tage1_index].saturating_counter; //the saturating counter is the prediction
    return prediction; //since we found a match in this tage table, no need to teck others
  }


  //default to taking the prediction of bimodal predictor if there is no matches in tage tables
  return prediction; 
}

void train_custom(uint32_t pc, uint8_t outcome){
  int bimodal_index= pc & ((1<< BimodalBits)-1); //get the index for the bimodal predictor
  int longestMatchTable=0;//this is the table number that has the longest match, if none match, fall back to Bimodalpredictor

  //below is a repeat of the code from cutom_predict()
    //this is because we want to follow the same method to find the tage table with longest matching entry
    //if non, we fall back on bimodal
  //start at tage table 5 (history length 13)
  int tage5_index= (pc ^ (ghistory_custom >> (64- 9))) % Tage5Bits; //get the index for the tage5 predictor
  if(Tage5Table[tage5_index].tag== (pc & ((1<< Tage5Bits)-1))){ //if the tag matches t
    longestMatchTable=5;//we found a match in this label
  }

  //check table 4 (history length 12)
  int tage4_index= (pc ^ (ghistory_custom >> (64-10))) % Tage4Bits; //get the index for the tage4 predictor
  if((Tage4Table[tage4_index].tag== (pc & ((1<< Tage4Bits)-1))) && longestMatchTable==0){ //if the tag matches/ no previous match
    longestMatchTable=4;//we found a match in this label
  }

  //now check tage 3 table (history length 11)
  int tage3_index= (pc ^ (ghistory_custom >> (64-11))) % Tage3Bits; //get the index for the tage3 predictor
  if((Tage3Table[tage3_index].tag== (pc & ((1<< Tage3Bits)-1))) && longestMatchTable==0 ){ //if the tag matches
    longestMatchTable=3;//we found a match in this label
  }

  //check table 2 (history length 10)
  int tage2_index= (pc ^ (ghistory_custom >> (64-12))) % Tage2Bits; //get the index for the tage2 predictor
  if((Tage2Table[tage2_index].tag== (pc & ((1<< Tage2Bits)-1))) && longestMatchTable==0){ //if the tag matches
    longestMatchTable=2;//we found a match in this label
  }

  //check table 1 (history length 9)
  int tage1_index= (pc ^ (ghistory_custom >> (64-13))) % Tage1Bits; //get the index for the tage1 predictor
  if((Tage1Table[tage1_index].tag== (pc & ((1<< Tage1Bits)-1))) && longestMatchTable==0){ //if the tag matches
    longestMatchTable=1;//we found a match in this label
  }

  // Update the bimodal predictor
  switch (BimodalTable[bimodal_index]) {
    case WN:
        BimodalTable[bimodal_index] = (outcome == TAKEN) ? WT : SN;
        break;
    case SN:
        BimodalTable[bimodal_index] = (outcome == TAKEN) ? WN : SN;
        break;
    case WT:
        BimodalTable[bimodal_index] = (outcome == TAKEN) ? ST : WN;
        break;
    case ST:
        BimodalTable[bimodal_index] = (outcome == TAKEN) ? ST : WT;
        break;
  }

  //now preform updates depending on if match found in a tage table or not
  if(longestMatchTable ==0){ //we found no matches
    //now itterate through table from smallest history length to largest till we find one with an entry with useful counter=0
    int usefulFound=0;//goes to 1 if we found an entry to fill
    //start at tage table 1 (history length 9)
    uint32_t indexT1= (pc ^ (ghistory_custom >> (64-13))) % Tage1Bits; //get the index for the tage1 predictor
    if(Tage1Table[indexT1].use_counter==0){
      //we found an entry with usefullness=0, this means we can replace the entry with a new one based on outcome
      
      Tage1Table[indexT1].tag= (pc & ((1<< Tage1Bits)-1)); //set the tag
      Tage1Table[indexT1].use_counter= 1; //set the usefulness to 1

      if(outcome== TAKEN && Tage1Table[indexT1].saturating_counter==ST){
        Tage1Table[indexT1].saturating_counter= ST;
      }
      else if(outcome== NOTTAKEN && Tage1Table[indexT1].saturating_counter==ST){
        Tage1Table[indexT1].saturating_counter= WT;
      }
      else if(outcome== TAKEN && Tage1Table[indexT1].saturating_counter==WT){
        Tage1Table[indexT1].saturating_counter= ST;
      }
      else if(outcome== NOTTAKEN && Tage1Table[indexT1].saturating_counter==WT){
        Tage1Table[indexT1].saturating_counter= WN;
      }
      else if(outcome== TAKEN && Tage1Table[indexT1].saturating_counter==WN){
        Tage1Table[indexT1].saturating_counter= WT;
      }
      else if(outcome== NOTTAKEN && Tage1Table[indexT1].saturating_counter==WN){
        Tage1Table[indexT1].saturating_counter= SN;
      }
      else if(outcome== TAKEN && Tage1Table[indexT1].saturating_counter==SN){
        Tage1Table[indexT1].saturating_counter= WN;
      }
      else if(outcome== NOTTAKEN && Tage1Table[indexT1].saturating_counter==SN){
        Tage1Table[indexT1].saturating_counter= SN;
      }

      usefulFound=1;
    }
    
    //now look at tage table 2 (history length 10)
    uint32_t indexT2= (pc ^ (ghistory_custom >> (64-12))) % Tage2Bits; //get the index for the tage2 predictor
    if((Tage2Table[indexT2].use_counter==0) && usefulFound==0){
      //we found an entry with usefullness=0, this means we can replace the entry with a new one based on outcome
      usefulFound=1;
      Tage2Table[indexT2].tag= (pc & ((1<< Tage2Bits)-1)); //set the tag
      Tage2Table[indexT2].use_counter= 1; //set the usefulness to 1

      if(outcome== TAKEN && Tage2Table[indexT2].saturating_counter==ST){
        Tage2Table[indexT2].saturating_counter= ST;
      }
      else if(outcome== NOTTAKEN && Tage2Table[indexT2].saturating_counter==ST){
        Tage2Table[indexT2].saturating_counter= WT;
      }
      else if(outcome== TAKEN && Tage2Table[indexT2].saturating_counter==WT){
        Tage2Table[indexT2].saturating_counter= ST;
      }
      else if(outcome== NOTTAKEN && Tage2Table[indexT2].saturating_counter==WT){
        Tage2Table[indexT2].saturating_counter= WN;
      }
      else if(outcome== TAKEN && Tage2Table[indexT2].saturating_counter==WN){
        Tage2Table[indexT2].saturating_counter= WT;
      }
      else if(outcome== NOTTAKEN && Tage2Table[indexT2].saturating_counter==WN){
        Tage2Table[indexT2].saturating_counter= SN;
      }
      else if(outcome== TAKEN && Tage2Table[indexT2].saturating_counter==SN){
        Tage2Table[indexT2].saturating_counter= WN;
      }
      else if(outcome== NOTTAKEN && Tage2Table[indexT2].saturating_counter==SN){
        Tage2Table[indexT2].saturating_counter= SN;
      }
    }
    
    //now look at tage table 3 (history length 11)
    uint32_t indexT3= (pc ^ (ghistory_custom >> (64-11))) % Tage3Bits; //get the index for the tage3 predictor
    if((Tage3Table[indexT3].use_counter==0) && usefulFound==0){
      //we found an entry with usefullness=0, this means we can replace the entry with a new one based on outcome
      usefulFound=1;
      Tage3Table[indexT3].tag= (pc & ((1<< Tage3Bits)-1)); //set the tag
      Tage3Table[indexT3].use_counter= 1; //set the usefulness to 1

      if(outcome== TAKEN && Tage3Table[indexT3].saturating_counter==ST){
        Tage3Table[indexT3].saturating_counter= ST;
      }
      else if(outcome== NOTTAKEN && Tage3Table[indexT3].saturating_counter==ST){
        Tage3Table[indexT3].saturating_counter= WT;
      }
      else if(outcome== TAKEN && Tage3Table[indexT3].saturating_counter==WT){
        Tage3Table[indexT3].saturating_counter= ST;
      }
      else if(outcome== NOTTAKEN && Tage3Table[indexT3].saturating_counter==WT){
        Tage3Table[indexT3].saturating_counter= WN;
      }
      else if(outcome== TAKEN && Tage3Table[indexT3].saturating_counter==WN){
        Tage3Table[indexT3].saturating_counter= WT;
      }
      else if(outcome== NOTTAKEN && Tage3Table[indexT3].saturating_counter==WN){
        Tage3Table[indexT3].saturating_counter= SN;
      }
      else if(outcome== TAKEN && Tage3Table[indexT3].saturating_counter==SN){
        Tage3Table[indexT3].saturating_counter= WN;
      }
      else if(outcome== NOTTAKEN && Tage3Table[indexT3].saturating_counter==SN){
        Tage3Table[indexT3].saturating_counter= SN;
      }
    }

    //now look at tage table 4 (history length 12)
    uint32_t indexT4= (pc ^ (ghistory_custom >> (64-10))) % Tage4Bits; //get the index for the tage4 predictor
    if((Tage4Table[indexT4].use_counter==0) && usefulFound==0){
      //we found an entry with usefullness=0, this means we can replace the entry with a new one based on outcome
      usefulFound=1;
      Tage4Table[indexT4].tag= (pc & ((1<< Tage4Bits)-1)); //set the tag
      Tage4Table[indexT4].use_counter= 1; //set the usefulness to 1

      if(outcome== TAKEN && Tage4Table[indexT4].saturating_counter==ST){
        Tage4Table[indexT4].saturating_counter= ST;
      }
      else if(outcome== NOTTAKEN && Tage4Table[indexT4].saturating_counter==ST){
        Tage4Table[indexT4].saturating_counter= WT;
      }
      else if(outcome== TAKEN && Tage4Table[indexT4].saturating_counter==WT){
        Tage4Table[indexT4].saturating_counter= ST;
      }
      else if(outcome== NOTTAKEN && Tage4Table[indexT4].saturating_counter==WT){
        Tage4Table[indexT4].saturating_counter= WN;
      }
      else if(outcome== TAKEN && Tage4Table[indexT4].saturating_counter==WN){
        Tage4Table[indexT4].saturating_counter= WT;
      }
      else if(outcome== NOTTAKEN && Tage4Table[indexT4].saturating_counter==WN){
        Tage4Table[indexT4].saturating_counter= SN;
      }
      else if(outcome== TAKEN && Tage4Table[indexT4].saturating_counter==SN){
        Tage4Table[indexT4].saturating_counter= WN;
      }
      else if(outcome== NOTTAKEN && Tage4Table[indexT4].saturating_counter==SN){
        Tage4Table[indexT4].saturating_counter= SN;
      }
    }
  
    //now look at tage table 5 (history length 13)
    uint32_t indexT5= (pc ^ (ghistory_custom >> (64-9))) % Tage5Bits; //get the index for the tage5 predictor
    if((Tage5Table[indexT5].use_counter==0) && usefulFound==0){
      //we found an entry with usefullness=0, this means we can replace the entry with a new one based on outcome
      usefulFound=1;
      Tage5Table[indexT5].tag= (pc & ((1<< Tage5Bits)-1)); //set the tag
      Tage5Table[indexT5].use_counter= 1; //set the usefulness to 1

      if(outcome== TAKEN && Tage5Table[indexT5].saturating_counter==ST){
        Tage5Table[indexT5].saturating_counter= ST;
      }
      else if(outcome== NOTTAKEN && Tage5Table[indexT5].saturating_counter==ST){
        Tage5Table[indexT5].saturating_counter= WT;
      }
      else if(outcome== TAKEN && Tage5Table[indexT5].saturating_counter==WT){
        Tage5Table[indexT5].saturating_counter= ST;
      }
      else if(outcome== NOTTAKEN && Tage5Table[indexT5].saturating_counter==WT){
        Tage5Table[indexT5].saturating_counter= WN;
      }
      else if(outcome== TAKEN && Tage5Table[indexT5].saturating_counter==WN){
        Tage5Table[indexT5].saturating_counter= WT;
      }
      else if(outcome== NOTTAKEN && Tage5Table[indexT5].saturating_counter==WN){
        Tage5Table[indexT5].saturating_counter= SN;
      }
      else if(outcome== TAKEN && Tage5Table[indexT5].saturating_counter==SN){
        Tage5Table[indexT5].saturating_counter= WN;
      }
      else if(outcome== NOTTAKEN && Tage5Table[indexT5].saturating_counter==SN){
        Tage5Table[indexT5].saturating_counter= SN;
      }
    }
  }
  else{ //there is a match in one of the tage tables
    uint32_t indexMatch;
    switch(longestMatchTable){
      case 1:
        indexMatch=tage1_index;
        break;
      case 2:
        indexMatch=tage2_index;
        break;
      case 3:
        indexMatch=tage3_index;
        break;
      case 4:
        indexMatch=tage4_index;
        break;
      case 5:
        indexMatch=tage5_index;
        break;
    }
    if(outcome== TAKEN){
      if(longestMatchTable==1){
        switch(Tage1Table[indexMatch].saturating_counter){
          case SN:
            Tage1Table[indexMatch].saturating_counter=WN;
            break;
          case WN:
            Tage1Table[indexMatch].saturating_counter=WT;
            break;
          case WT:
            Tage1Table[indexMatch].saturating_counter=ST;
            break;
          case ST:
            Tage1Table[indexMatch].saturating_counter=ST;
            break;
        }
      }
      else if(longestMatchTable==2){
        switch(Tage2Table[indexMatch].saturating_counter){
          case SN:
            Tage2Table[indexMatch].saturating_counter=WN;
            break;
          case WN:
            Tage2Table[indexMatch].saturating_counter=WT;
            break;
          case WT:
            Tage2Table[indexMatch].saturating_counter=ST;
            break;
          case ST:
            Tage2Table[indexMatch].saturating_counter=ST;
            break;
        }
      }
      else if(longestMatchTable==3){
        switch(Tage3Table[indexMatch].saturating_counter){
          case SN:
            Tage3Table[indexMatch].saturating_counter=WN;
            break;
          case WN:
            Tage3Table[indexMatch].saturating_counter=WT;
            break;
          case WT:
            Tage3Table[indexMatch].saturating_counter=ST;
            break;
          case ST:
            Tage3Table[indexMatch].saturating_counter=ST;
            break;
        }
      }
      else if(longestMatchTable==4){
        switch(Tage4Table[indexMatch].saturating_counter){
          case SN:
            Tage4Table[indexMatch].saturating_counter=WN;
            break;
          case WN:
            Tage4Table[indexMatch].saturating_counter=WT;
            break;
          case WT:
            Tage4Table[indexMatch].saturating_counter=ST;
            break;
          case ST:
            Tage4Table[indexMatch].saturating_counter=ST;
            break;
        }
      }
      else if(longestMatchTable==5){
        switch(Tage5Table[indexMatch].saturating_counter){
          case SN:
            Tage5Table[indexMatch].saturating_counter=WN;
            break;
          case WN:
            Tage5Table[indexMatch].saturating_counter=WT;
            break;
          case WT:
            Tage5Table[indexMatch].saturating_counter=ST;
            break;
          case ST:
            Tage5Table[indexMatch].saturating_counter=ST;
            break;
        }
      }
    }
    else{ //the outcome is not taken
      if(longestMatchTable==1){
        switch(Tage1Table[indexMatch].saturating_counter){
          case SN:
            Tage1Table[indexMatch].saturating_counter=SN;
            break;
          case WN:
            Tage1Table[indexMatch].saturating_counter=SN;
            break;
          case WT:
            Tage1Table[indexMatch].saturating_counter=WN;
            break;
          case ST:
            Tage1Table[indexMatch].saturating_counter=WT;
            break;
        }
      }
      else if(longestMatchTable==2){
        switch(Tage2Table[indexMatch].saturating_counter){
          case SN:
            Tage2Table[indexMatch].saturating_counter=SN;
            break;
          case WN:
            Tage2Table[indexMatch].saturating_counter=SN;
            break;
          case WT:
            Tage2Table[indexMatch].saturating_counter=WN;
            break;
          case ST:
            Tage2Table[indexMatch].saturating_counter=WT;
            break;
        }
      }
      else if(longestMatchTable==3){
        switch(Tage3Table[indexMatch].saturating_counter){
          case SN:
            Tage3Table[indexMatch].saturating_counter=SN;
            break;
          case WN:
            Tage3Table[indexMatch].saturating_counter=SN;
            break;
          case WT:
            Tage3Table[indexMatch].saturating_counter=WN;
            break;
          case ST:
            Tage3Table[indexMatch].saturating_counter=WT;
            break;
        }
      }
      else if(longestMatchTable==4){
        switch(Tage4Table[indexMatch].saturating_counter){
          case SN:
            Tage4Table[indexMatch].saturating_counter=SN;
            break;
          case WN:
            Tage4Table[indexMatch].saturating_counter=SN;
            break;
          case WT:
            Tage4Table[indexMatch].saturating_counter=WN;
            break;
          case ST:
            Tage4Table[indexMatch].saturating_counter=WT;
            break;
        }
      }
      else if(longestMatchTable==5){
        switch(Tage5Table[indexMatch].saturating_counter){
          case SN:
            Tage5Table[indexMatch].saturating_counter=SN;
            break;
          case WN:
            Tage5Table[indexMatch].saturating_counter=SN;
            break;
          case WT:
            Tage5Table[indexMatch].saturating_counter=WN;
            break;
          case ST:
            Tage5Table[indexMatch].saturating_counter=WT;
            break;
        }
      }
    }

  //end by updating the ghr based on the outcome. (No changes ever made to PC)
    ghistory_custom= ((ghistory_custom <<1) | outcome);//update the global history register
  }
}

void cleanup_custom(){
  free(BimodalTable);
  free(Tage1Table);
  free(Tage2Table);
  free(Tage3Table);
  free(Tage4Table);
  free(Tage5Table);
  //free(PartialTag);
}


void init_predictor()
{
  switch (bpType)
  {
  case STATIC:
    break;
  case GSHARE:
    init_gshare();
    break;
  case TOURNAMENT:
    init_tournament();
    break;
  case CUSTOM:
    init_custom();
    break;
  default:
    break;
  }
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint32_t make_prediction(uint32_t pc, uint32_t target, uint32_t direct)
{

  // Make a prediction based on the bpType
  switch (bpType)
  {
  case STATIC:
    return TAKEN;
  case GSHARE:
    return gshare_predict(pc);
  case TOURNAMENT:
    return tournament_predict(pc);
  case CUSTOM:
    return custom_predict(pc);
  default:
    break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//

void train_predictor(uint32_t pc, uint32_t target, uint32_t outcome, uint32_t condition, uint32_t call, uint32_t ret, uint32_t direct)
{
  if (condition)
  {
    switch (bpType)
    {
    case STATIC:
      return;
    case GSHARE:
      return train_gshare(pc, outcome);
    case TOURNAMENT:
      return train_tournament(pc, outcome);
    case CUSTOM:
      return train_custom(pc, outcome);
    default:
      break;
    }
  }
}
