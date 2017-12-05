#include "config.h"
// threading library
#include "pt_cornell_1_2.h"

////////////////////////////////////
// graphics libraries
#include "tft_master.h"
#include "tft_gfx.h"
// need for rand function
#include <math.h>
#include <stdlib.h>
#include "bubble.h"
#include "wavLocNew2.h"
#include "sounds8again2.h"


//ADC STUFF
// define setup parameters for OpenADC10
// ADC_CLK_AUTO -- Internal counter ends sampling and
//starts conversion (Auto convert)
// ADC_AUTO_SAMPLING_ON -- Sampling begins immediately after last
//conversion completes; SAMP bit is automatically set
// ADC_AUTO_SAMPLING_OFF -- Sampling begins with AcquireADC10();
// Turn module on|ouput in integer|trigger mode auto|enable autosample
#define PARAM1 ADC_FORMAT_INTG16 | ADC_CLK_AUTO | ADC_AUTO_SAMPLING_OFF
// ADC ref external | disable offset test | disable scan mode |
// do 1 sample | use single buf | alternate mode off
#define PARAM2 ADC_VREF_AVDD_AVSS | ADC_OFFSET_CAL_DISABLE | ADC_SCAN_OFF | ADC_SAMPLES_PER_INT_1 | ADC_ALT_BUF_OFF | ADC_ALT_INPUT_OFF
// use peripherial bus clock | set sample time | set ADC clock divider
// ADC_CONV_CLK_Tcy2 means divide CLK_PB by 2 (max speed)
// ADC_SAMPLE_TIME_5 seems to work with a source resistance < 1kohm
#define PARAM3 ADC_CONV_CLK_PB | ADC_SAMPLE_TIME_5 | ADC_CONV_CLK_Tcy2
// set AN11 and as analog inputs
#define PARAM4 ENABLE_AN11_ANA // pin 24
// do not assign channels to scan
#define PARAM5 SKIP_SCAN_ALL
// use ground as neg ref for A | use AN11 for input A


#define EnablePullDownA(bits) CNPUACLR=bits; CNPDASET=bits;
#define DisablePullDownA(bits) CNPDACLR=bits;
#define EnablePullUpA(bits) CNPDACLR=bits; CNPUASET=bits;
#define DisablePullUpA(bits) CNPUACLR=bits;
#define EnablePullDownB(bits) CNPUBCLR=bits; CNPDBSET=bits;
#define DisablePullDownB(bits) CNPDBCLR=bits;
#define EnablePullUpB(bits) CNPDBCLR=bits; CNPUBSET=bits;
#define DisablePullUpB(bits) CNPUBCLR=bits;
#define DAC_config_chan_A 0b0011000000000000
#define DAC_config_chan_B 0b1011000000000000
//originals ones on left, new on right
#define PIN_0  BIT_14 //BIT_11//RB11(makes sound but fucks up board)          //RB3 //incorrect    //3rd from the left       
#define PIN_1  BIT_10 //RB10          //incorrect 
#define PIN_2  BIT_0  //RA0          //incorrect
#define PIN_3   BIT_1 //RA1        //incorrect
#define PIN_4   BIT_3 //RB3          //incorrect
#define PIN_5  BIT_2 //RA2           //incorrect
#define PIN_6   BIT_3 //RA3         //incorrect
#define PIN_7   BIT_9  //RB9        // rb9 // this is the highest note 
#define PIN_8  BIT_8              // rb8
#define PIN_9  BIT_7              // rb7

//A B Clow D E F G Chi
//tried RB2, RB1, RA4, RB4, RB 15, RB5
//RB14 doesnt crash but still stops TFT

#define rWavSize 15500
//#define wavLength //this is defined in the wavLoc header now

volatile SpiChannel spiChn = SPI_CHANNEL2 ; // the SPI channel to use
volatile int spiClkDiv = 2 ; 

// string buffer
char buffer[60];
char arrow[2]; // arrow for menu selection

char getReady[10];
int readyVar=0;

int drumPlay[8]={0}; //this tracks if a drum note has been played yet
int arrPos=20, oldArrPos=20;
int recording,playback=0,oldPlayback=0, oldOldPlayback=0; // 1 if recording, 0 if not
int soundOut=0; //final sound output
int pressed[10]={0}; //if or if not each button is pressed
int oldPressed[10]={0}; //if or if not each button is pressed
int userWav =0, userWav2=0;
int i =0, j=0;

int bitAddresses[8]={PIN_0, PIN_1, PIN_2, PIN_3, PIN_4, PIN_5, PIN_6, PIN_7};

int mode =0; //mode 0=piano, 1=guitar, 2=bass 3=drums 

//sounds is the 8xwavLength table of sound files

// sound recording wave
int rInd =0;
 
short recordWav[rWavSize] = {0};

static struct pt pt_draw, pt_button ;

void __ISR(_TIMER_2_VECTOR, ipl2) Timer2Handler2(void)
{
    // 74 cycles to get to this point from timer event
    mT2ClearIntFlag();
    soundOut=0;
    if(mode <4){
      for (i = 0; i < 8; ++i){
      	if (pressed[i]){
          if (mode==3){
            if (drumPlay[i]==0){
              soundOut+=sounds[wavLoc[mode][i]]<<2;
              if (i==7){
                drumPlay[i]=wavLoc[mode][i]+1>=wavLength ? 1 : 0;
                wavLoc[mode][i]= wavLoc[mode][i]+1>=wavLength ? wavStart[mode][i] : wavLoc[mode][i]+1;
              } else {
                drumPlay[i]= wavLoc[mode][i]+1>=wavStart[mode][i+1] ? 1 : 0;   
                wavLoc[mode][i]= wavLoc[mode][i]+1>=wavStart[mode][(i+1)] ? wavStart[mode][i] : wavLoc[mode][i]+1;           
              }
            }
          } else {
      		  soundOut+=sounds[wavLoc[mode][i]]<<1;
           if(i ==7){
              wavLoc[mode][i]= wavLoc[mode][i]+1>=wavStart[mode+1][0] ? wavStart[mode][i] : wavLoc[mode][i]+1;
            }
            else wavLoc[mode][i]= wavLoc[mode][i]+1>=wavStart[mode][(i+1)] ? wavStart[mode][i] : wavLoc[mode][i]+1;
          }
      	}
      }
}
    if (recording){
       recordWav[rInd] += soundOut;
       if(mode == 4){

         userWav= ReadADC10(0)*512/1024; 
         recordWav[rInd] += userWav;  // read the result of channel 9 conversion from the idle buffer
         AcquireADC10();
       }
    }
    if (playback){
      soundOut+=recordWav[rInd];
    }
    
    if (playback || recording){ //increment the playback/record index
      rInd++;
      if(rInd >= rWavSize){
          recording = 0;
          rInd=0;
          if(readyVar && playback){
            recording =1;
            readyVar =0;
          }
      }
    }
	mPORTBClearBits(BIT_4); // start transaction
	// write to spi2 
	WriteSPI2( DAC_config_chan_A | (soundOut));
	while (SPI2STATbits.SPIBUSY); // wait for end of transaction
	// CS high
	mPORTBSetBits(BIT_4); // end transaction
} // end ISR TIMER2

static PT_THREAD (protothread_button(struct pt *pt)){
  PT_BEGIN(pt);
    while(1){
      oldPressed[0]=mPORTBReadBits(PIN_0);
      oldPressed[1]=mPORTBReadBits(PIN_1);
      oldPressed[2]=mPORTAReadBits(PIN_2);
      oldPressed[3]=mPORTAReadBits(PIN_3);
      oldPressed[4]=mPORTBReadBits(PIN_4);
      oldPressed[5]=mPORTAReadBits(PIN_5);
      oldPressed[6]=mPORTAReadBits(PIN_6);
      oldPressed[7]=mPORTBReadBits(PIN_7);
      oldPressed[8]=mPORTBReadBits(PIN_8);
      oldPressed[9]=mPORTBReadBits(PIN_9);
      PT_YIELD_TIME_msec(30);
      if (mode==3){
        drumPlay[0]=oldPressed[0]< mPORTBReadBits(PIN_0) ?          0 :drumPlay[0];
        drumPlay[1]=oldPressed[1]< mPORTBReadBits(PIN_1) ?          0 :drumPlay[1];
        drumPlay[2]=oldPressed[2]< mPORTAReadBits(PIN_2) ?          0 :drumPlay[2];
        drumPlay[3]=oldPressed[3]< mPORTAReadBits(PIN_3) ?          0 :drumPlay[3];
        drumPlay[4]=oldPressed[4]< mPORTBReadBits(PIN_4) ?          0 :drumPlay[4];
        drumPlay[5]=oldPressed[5]< mPORTAReadBits(PIN_5) ?          0 :drumPlay[5];
        drumPlay[6]=oldPressed[6]< mPORTAReadBits(PIN_6) ?          0 :drumPlay[6];
        drumPlay[7]=oldPressed[7]< mPORTBReadBits(PIN_7) ?          0 :drumPlay[7];
      }
      pressed[0]=oldPressed[0]==mPORTBReadBits(PIN_0) ? oldPressed[0] : pressed[0];
      pressed[1]=oldPressed[1]==mPORTBReadBits(PIN_1) ? oldPressed[1] : pressed[1];
      pressed[2]=oldPressed[2]==mPORTAReadBits(PIN_2) ? oldPressed[2] : pressed[2];
      pressed[3]=oldPressed[3]==mPORTAReadBits(PIN_3) ? oldPressed[3] : pressed[3];
      pressed[4]=oldPressed[4]==mPORTBReadBits(PIN_4) ? oldPressed[4] : pressed[4];
      pressed[5]=oldPressed[5]==mPORTAReadBits(PIN_5) ? oldPressed[5] : pressed[5];
      pressed[6]=oldPressed[6]==mPORTAReadBits(PIN_6) ? oldPressed[6] : pressed[6];
      pressed[7]=oldPressed[7]==mPORTBReadBits(PIN_7) ? oldPressed[7] : pressed[7];
      pressed[8]=oldPressed[8]< mPORTBReadBits(PIN_8) ?             1 :          0;
      pressed[9]=oldPressed[9]< mPORTBReadBits(PIN_9) ?             1 :          0;
      
      // pressed[0]=0; //works - no noise when not pressed
      // pressed[1]=0; //works
      // pressed[2]=0;//always pressed
      // pressed[3]=0;//always pressed
      // pressed[4]=0;//works 
      // pressed[5]=0;//works
      // pressed[6]=0;//works
      // pressed[7]=0;//works
      // pressed[8]=0;//works
      // pressed[9]=0;//works
      
      if (pressed[8]){
        arrPos+=20;
        //if(mode<4)
        mode++;
        if (arrPos>140){
          arrPos=20;
          mode = 0;
        }
      }      
      if (pressed[9]){
        if (arrPos==120){ //delete recording
          for (i = 0; i < rWavSize; ++i){
            recordWav[i]=0;
          }
        } else if(arrPos==140){ //set playback
            playback=!playback;
            // if (playback && recording){
            //   rInd=0; // reset reconding indexs
            // }
        } else {
          if(recording==0){
            if(playback){
              readyVar=1;
              
              tft_setTextColor(ILI9340_WHITE);
              tft_setCursor(100,300);
              tft_writeString(getReady);
            }
            else{
              tft_setCursor(210,300);
              sprintf(buffer, "3");
              tft_writeString(buffer);
              tft_setCursor(10,250);
              sprintf(buffer,"[                ]");
              tft_writeString(buffer);
              readyVar=1;
              oldPlayback=playback;
              playback=0;
              PT_YIELD_TIME_msec(400);
              tft_setCursor(210,300);
              tft_setTextColor(ILI9340_BLACK);
              sprintf(buffer, "3");
              tft_writeString(buffer);
              tft_setCursor(210,300);
              tft_setTextColor(ILI9340_WHITE);
              sprintf(buffer, "2");
              tft_writeString(buffer);
              PT_YIELD_TIME_msec(400);

              tft_setCursor(210,300);
              tft_setTextColor(ILI9340_BLACK);
              sprintf(buffer, "2");
              tft_writeString(buffer);
              tft_setCursor(210,300);
              tft_setTextColor(ILI9340_WHITE);
              sprintf(buffer, "1");
              tft_writeString(buffer);
              tft_setCursor(210,300);
              PT_YIELD_TIME_msec(400);
              tft_setCursor(210,300);
              tft_setTextColor(ILI9340_BLACK);
              sprintf(buffer, "1");
              tft_writeString(buffer);

              readyVar=0;
         
              tft_setTextColor(ILI9340_YELLOW); 
              playback=oldPlayback;
              recording= recording ==0 ? 1 :0;
            }
          }
          
          // if(playback && recording) rInd =0;
        }
      }
//      PT_YIELD_TIME_msec(50);
    }
  PT_END(pt);
}


static PT_THREAD (protothread_draw(struct pt *pt)){
	PT_BEGIN(pt);
	while(1){
  		tft_setCursor(10,oldArrPos);
  		tft_setTextColor(ILI9340_BLACK);
  		tft_writeString(arrow);
    	tft_setTextColor(ILI9340_YELLOW); 
  		tft_setCursor(10, arrPos);
      oldArrPos = arrPos;
    	tft_writeString(arrow);
    	if (recording){
    		tft_fillCircle(230,310,5,ILI9340_RED);
        tft_setTextColor(ILI9340_BLACK);
        tft_setCursor(100,300);
        tft_writeString(getReady);
        tft_setTextColor(ILI9340_WHITE);
        tft_setCursor(10,250);
        sprintf(buffer,"[                ]");
        tft_writeString(buffer);
    	} else {
    		tft_fillCircle(230,310,5,ILI9340_BLACK);
        if (!readyVar){
          
          tft_setTextColor(ILI9340_BLACK);
          tft_setCursor(10,250);
          sprintf(buffer,"[                ]");
          tft_writeString(buffer);
         // tft_fillRoundRect(10,253,220,8,0,ILI9340_BLACK);
          tft_setTextColor(ILI9340_WHITE);
        }
    	}

      tft_setCursor(130,140);
      if (playback>oldOldPlayback){
        sprintf(buffer," off");
        tft_setTextColor(ILI9340_BLACK);
        tft_writeString(buffer);
        tft_setCursor(130,140);
        sprintf(buffer," on");
        tft_setTextColor(ILI9340_WHITE);
        tft_writeString(buffer);
        oldOldPlayback=playback;
      } else if (playback<oldOldPlayback){
        sprintf(buffer," on");
        tft_setTextColor(ILI9340_BLACK);
        tft_writeString(buffer);
        tft_setCursor(130,140);
        sprintf(buffer," off");
        tft_setTextColor(ILI9340_WHITE);
        tft_writeString(buffer);
        oldOldPlayback=playback;
      }
      
      
      if (playback || recording){
        tft_fillRoundRect(rInd*210/rWavSize+10,253,6,8,0,ILI9340_WHITE);
        if (rInd>(rWavSize-500)){
          tft_fillRoundRect(10,253,220,8,0,ILI9340_BLACK);
        }
      }
      else if(!readyVar) tft_fillRoundRect(10,253,220,8,0,ILI9340_BLACK);

    	PT_YIELD_TIME_msec(50);
	}
  	PT_END(pt);
}


void main(void) {

 SYSTEMConfigPerformance(PBCLK);

 ANSELA = 0; ANSELB = 0; CM1CON = 0; CM2CON = 0;
 // OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_1, 907); //40Mhz/907=44.1khz ;;;;;
  OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_1, 5000); // this is for 8 khz:40Mhz/5000 aka 8 khz
  // set up the timer interrupt with a priority of 2
  ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_2);
  mT2ClearIntFlag(); // and clear the interrupt flag

  /// SPI setup //////////////////////////////////////////
  // SCK2 is pin 26 
  // SDO2 is in PPS output group 2, could be connected to RB5 which is pin 14
  PPSOutput(2, RPB5, SDO2);
  // control CS for DAC
  mPORTBSetPinsDigitalOut(BIT_4);
  mPORTBSetBits(BIT_4);
  // divide Fpb by 2, configure the I/O ports. Not using SS in this example
  // 16 bit transfer CKP=1 CKE=1
  // possibles SPI_OPEN_CKP_HIGH;   SPI_OPEN_SMP_END;  SPI_OPEN_CKE_REV
  // For any given peripherial, you will need to match these
  SpiChnOpen(spiChn, SPI_OPEN_ON | SPI_OPEN_MODE16 | SPI_OPEN_MSTEN | SPI_OPEN_CKE_REV , spiClkDiv);
  // === config threads ==========
  // turns OFF UART support and debugger pin
 PT_setup();

  // === setup system wide interrupts  ========
 INTEnableSystemMultiVectoredInt();

 //Enable Pulldown for Keyboard
 mPORTASetPinsDigitalIn(PIN_3 | PIN_2 | PIN_5 | PIN_6);    //Set port as input
 EnablePullDownA(PIN_3 | PIN_2 | PIN_5 | PIN_6);
 mPORTBSetPinsDigitalIn(PIN_0 | PIN_1 | PIN_4 | PIN_7 | PIN_8 | PIN_9);    //Set port as input
 EnablePullDownB(PIN_0 | PIN_1 | PIN_4 | PIN_7 | PIN_8 | PIN_9);


//ADC
 CloseADC10(); // ensure the ADC is off before setting the configuration
  
  // configure to sample AN11
  SetChanADC10( ADC_CH0_NEG_SAMPLEA_NVREF | ADC_CH0_POS_SAMPLEA_AN11);
  // configure ADC using the parameters defined above
  OpenADC10( PARAM1 , PARAM2 , PARAM3 , PARAM4 , PARAM5 );
  EnableADC10(); // Enable the ADC

 sprintf(arrow, " >");
 sprintf(getReady, "Get ready!");

  // init the threads
 // PT_INIT(&pt_timer);
 // PT_INIT(&pt_color);
 PT_INIT(&pt_draw);
 PT_INIT(&pt_button);

  // init the display
 tft_init_hw();
 tft_begin();
 tft_setTextColor(ILI9340_WHITE);  tft_setTextSize(2);
 tft_fillScreen(ILI9340_BLACK);
  //240x320 vertical display
  tft_setRotation(0); // Use tft_setRotation(1) for 320x240

  mPORTBSetPinsDigitalIn(BIT_10);    //Set port as input
  mPORTBSetPinsDigitalIn(BIT_3);    //Set port as input
  // round-robin scheduler for threads

  tft_setCursor(40,20);
  sprintf(buffer,"piano"); // when in this mode, buttons 1-8 play the instrument
  tft_writeString(buffer);
  tft_setCursor(40,40);
  sprintf(buffer,"guitar");// when in this mode, buttons 1-8 play the instrument
  tft_writeString(buffer);
  tft_setCursor(40,60);
  sprintf(buffer,"bass");// when in this mode, buttons 1-8 play the instrument
  tft_writeString(buffer);
  tft_setCursor(40,80);
  sprintf(buffer,"drums");// when in this mode, buttons 1-8 play the instrument
  tft_writeString(buffer);
  tft_setCursor(40,100);
  sprintf(buffer,"user"); // when in this mode, record the user
  tft_writeString(buffer);
  //for all options above, hitting record starts recording the sounds being played, the effect of pressing
  // record changes for the menu options below
  tft_setCursor(40,120);
  sprintf(buffer,"delete");// when in this mode, delete the recorded item if record is pressed
  tft_writeString(buffer);

  tft_setCursor(40,140);
  sprintf(buffer,"playback");// toggles playback if record is pressed
  tft_writeString(buffer);

  tft_setCursor(130,140);
  sprintf(buffer," off");
  tft_writeString(buffer);

  while (1){
    PT_SCHEDULE(protothread_draw(&pt_draw));
    PT_SCHEDULE(protothread_button(&pt_button));
  }
  
} // main