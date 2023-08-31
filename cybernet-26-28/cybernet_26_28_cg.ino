
#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include "Adafruit_GC9106_kbv.h"
#include <Fonts/Free12.h>
#include <Fonts/Alleencijfers.h>    // these take a lot of memory

#define pulseLow(pin) {digitalWrite(pin, LOW); digitalWrite(pin, HIGH); }
#define TFT_CS 8
#define TFT_DC 9
#define TFT_RST 10 
//Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
Adafruit_GC9106_kbv tft = Adafruit_GC9106_kbv (TFT_CS, TFT_DC, TFT_RST);

#include <Wire.h>

#include "si5351.h"

Si5351 si5351;

int encoderPin2 = 2;             // encoder
int encoderPin1 = 3;             // encoder
int txpin= 6;                    // assign tx to D9
int txout= 5;                    // voor rogerbeep
int rogerbeep= 4;                // sound out
int rogeraan= 7;
int roger;
int switchcontact = A1;           // This pin is connected to the switch of the encoder
volatile int lastEncoded = 0;    // encoder
long lastencoderValue = 0;       // encoder
int lastMSB = 0;                 // encoder
int lastLSB = 0;                 // encoder
int encoderValue = 0;            // encoder
long rx=1125000;                 // Starting frequency of VFO in 1Hz steps , this means 26000000+1000000=27000000Hz=27MHz
const char* call = "BONKJE";
int  hertzPosition = 5;
long rx2=1;                    // variable to hold the updated frequency
long increment = 10000;     // starting VFO update increment in HZ.
int buttonstate = 0;                          // if the button is pressed or not
int mode, oldmode =0;
byte save_showFreq[8]={10,10,10,10,10,10,10,0};
int band;
int channel;
int dispchannel;
int dispmode;

void setup() {
  setup_display();
  pinMode(encoderPin1, INPUT);
  pinMode(encoderPin2, INPUT);
  pinMode(txpin,INPUT);
  pinMode(txout, OUTPUT);
  pinMode(rogeraan, INPUT);
  pinMode(switchcontact, INPUT);
  digitalWrite(encoderPin1, HIGH);           //turn pullup resistor on
  digitalWrite(encoderPin2, HIGH);           //turn pullup resistor on
  attachInterrupt(0, updateEncoder, CHANGE); //interrupt connected to the encoder
  attachInterrupt(1, updateEncoder, CHANGE);
  pinMode(A0,INPUT);                         // Connect to a button that goes to GND on push
  digitalWrite(A0,HIGH);                     //turn pullup resistor on
  roger = digitalRead(rogeraan);
  pinMode(A1,INPUT);                          
  dispmode = digitalRead(A1);
  Serial.begin(57600);
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  si5351.set_correction(135796, SI5351_PLL_INPUT_XO); // corr 141796 nr1  135100 nr2  mijn 61100
  si5351.output_enable(SI5351_CLK0, 1);
}

void loop() {
    byte tx=digitalRead(txpin);
    mode=tx+2;
    int frequencyerror = 0;                   // add here the frequency-error in Hz  
    int offset=0;                                            
     if ((rx != rx2) || (mode!=oldmode)){     //determine if something has changed 
           if (rx >=1405000){rx=rx2;};        // UPPER VFO LIMIT 27.5 MHz
           if (rx <=965000){rx=rx2;};         // LOWER VFO LIMIT 26.5 MHz
        sendFrequency((rx+offset),tx);        // frequency is send to the DDS
        showFreq();                           //frequency is send to the display
        rx2 = rx;                             //memory
           if (rx2 >=1406000){rx2=1405000;};  // Correction in case there have been 2 interrupts
           if (rx2 <=964000){rx2=965000;};     
      }
  buttonstate = digitalRead(A0);              // button is connected to A0
 oldmode=mode;
    if(buttonstate == LOW) {
        setincrement();        
    };
}

void setincrement(){
  if      (increment == 1000)     {increment = 100000;tft.fillRect(96,42, 22,4, ST77XX_BLACK) ;tft.fillRect(42,42, 22,4, ST77XX_ORANGE); }
  else if (increment == 100000)   {increment = 10000; tft.fillRect(42,42, 22,4, ST77XX_BLACK)  ;tft.fillRect(69,42, 22,4, ST77XX_ORANGE); }
  else                            {increment = 1000;  tft.fillRect(69,42, 22,4, ST77XX_BLACK)  ;tft.fillRect(96,42, 22,4, ST77XX_ORANGE); };
  showFreq();     //well put it on the screen
  delay(200);   // added delay to get a more smooth reaction of the push button
}

void updateEncoder(){                       //reads out the encouser-switch
int MSB = digitalRead(encoderPin1);         //MSB = most significant bit
int LSB = digitalRead(encoderPin2);         //LSB = least significant bit
int encoded = (MSB << 1) |LSB;              //converting the 2 pin value to single number 
int sum = (lastEncoded << 2) | encoded;     //adding it to the previous encoded value 
if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderValue ++; 
if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderValue --;  
if(encoderValue == 4) {rx=rx+increment ; encoderValue=0;} // there are 4 changes between one click of the encoder
if(encoderValue == -4) {rx=rx-increment ; encoderValue=0;}   // it can be neccessary to reduce these to 2 for other encoders
lastEncoded = encoded;                      //store this value for next time 

}
void setup_display(){
  tft.begin();
  //tft.initR(INITR_MINI160x80);//modified Adafruit_ST7735.cpp _colstart=26 and _rowstart=0 
  tft.setRotation(3);
  tft.invertDisplay(0);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setFont(&FreeSans12pt7b);
  tft.setCursor(5,80);  tft.print(call); 
  delay(500);
  tft.setCursor(5,80); tft.setTextColor(ST77XX_BLACK);  tft.print(call); 
  tft.setCursor(5,36);  tft.setTextColor(ST77XX_GREEN);tft.print("2");
  tft.fillRect(69,42, 22,4, ST77XX_ORANGE);                                   
}  

void sendFrequency(double frequency, byte tx) {                            // frequency in hz above 26MHz
  frequency +=26000000;                                        // actual frequency in Hz
    if (tx)   {
      frequency -=20480000;
      frequency *=4096;
      frequency/=2634;
      tft.setCursor(120,80);tft.setTextColor(ST77XX_BLUE);  tft.print("TX"); 
      };     //TX frequency   4096 = reference   2636 is CH9 divider
    if (!tx) {
      frequency -=20935000;
      frequency *=4096;
      frequency/=2452;
      if (roger==1 && oldmode==3) {
        digitalWrite(txout, HIGH);
        tft.setCursor(100,80);tft.setTextColor(ST77XX_BLUE);  tft.print("R"); 
        tone(rogerbeep, 2475,100);
        delay(100);
        tone(rogerbeep, 2175,100);
        delay(100);
        digitalWrite(txout, LOW);
        tft.setCursor(100,80);tft.setTextColor(ST77XX_BLACK);  tft.print("R"); 
        };
        tft.setCursor(120,80); tft.setTextColor(ST77XX_BLACK);  tft.print("TX"); 
      };     //RX frequency  4096 = reference   2452 is CH9 divider
    si5351.set_freq(frequency * 100ULL, SI5351_CLK0);  
}


int convertFrequencyToChannel() {      // rx = 500000 to 1500000 in Hz
     int rndchannel = (rx)/10000;      //  now in rounded steps of 10kHz
     rndchannel -=6;                   // for the lowest band  which starts at 26.065MHz
     band = rndchannel/45;             // all bands have 45 channel , alpha channels included
     int chan_array[45]={1,2,3,103,4,5,6,7,107,8,9,10,11,111,12,13,14,15,115,16,17,18,19,119,20,21,22,24,25,23,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40};
     channel = chan_array[(rndchannel%45)];      // returns the channel
     return;
} 

void showFreq(){     //   0 frequency     1 frequencystep
    convertFrequencyToChannel();
    byte millions=6+(rx/1000000);         //MHz
    byte hundredk = ((rx/100000)%10);     //100kHz
    byte tenk =     ((rx/10000)%10);      //10kHz
    byte onek =     ((rx/1000)%10);       //1kHz
    byte hundreds = ((rx/100)%10);        //100Hz
    byte tens =     ((rx/10)%10);         //10Hz
    byte negative=0; if (channel > 1000){channel-=1000;negative=1;}    // 
    tft.setFont(&FreeSans24pt7b);         // tft.setTextSize(2); tft.setTextColor(ST77XX_GREEN);
    //tft.fillRect(1,1,1,1,ST77XX_BLACK);   //looks like a bug.....
    if (hundredk!=save_showFreq[2]){tft.setTextColor(ST77XX_BLACK);tft.setCursor(41, 36);tft.print(save_showFreq[2]);tft.setCursor(41, 36); tft.setTextColor(ST77XX_GREEN);  tft.print(hundredk);} 
    if (tenk!=save_showFreq[3])    {tft.setTextColor(ST77XX_BLACK);tft.setCursor(68, 36);tft.print(save_showFreq[3]);tft.setCursor(68, 36); tft.setTextColor(ST77XX_GREEN);  tft.print(tenk);}   
    if (onek!=save_showFreq[4])    {tft.setTextColor(ST77XX_BLACK);tft.setCursor(95, 36);tft.print(save_showFreq[4]);tft.setCursor(95, 36); tft.setTextColor(ST77XX_GREEN);  tft.print(onek);}

    tft.setFont(&FreeSans12pt7b);         // tft.setTextSize(1);   
    if (millions!=save_showFreq[1]){tft.setTextColor(ST77XX_BLACK);tft.setCursor(20, 36);tft.print(save_showFreq[1]);tft.setCursor(20, 36); tft.setTextColor(ST77XX_GREEN);  tft.print(millions);}  
    if (hundreds!=save_showFreq[5]){tft.setTextColor(ST77XX_BLACK);tft.setCursor(125, 36);tft.print(save_showFreq[5]);tft.setCursor(125, 36); tft.setTextColor(ST77XX_GREEN);tft.print(hundreds);}
    if (tens!=save_showFreq[6])    {tft.setTextColor(ST77XX_BLACK);tft.setCursor(138, 36);tft.print(save_showFreq[6]);tft.setCursor(138, 36); tft.setTextColor(ST77XX_GREEN);tft.print(tens);}
    tft.setFont(&FreeSans24pt7b);         // tft.setTextSize(2); tft.setTextColor(ST77XX_GREEN);
    tft.setCursor(32, 37); tft.setTextColor(ST77XX_GREEN);tft.print(".");
    tft.setFont(&FreeSans12pt7b);         // tft.setTextSize(1);   

    save_showFreq[1]=millions;save_showFreq[2]=hundredk;save_showFreq[3]=tenk;save_showFreq[4]=onek;save_showFreq[5]=hundreds;save_showFreq[6]=tens; //save values for the next time to write in black

    tft.fillRect(5,62, 66,18, ST77XX_BLACK);
    if (band==1) { tft.setCursor(5,78);tft.setTextColor(ST77XX_MAGENTA) ;tft.print("B");} 
    if (band==2) { tft.setCursor(5,78);tft.setTextColor(ST77XX_MAGENTA) ;tft.print("C");} 
    if (band==3) { tft.setCursor(5,78);tft.setTextColor(ST77XX_MAGENTA) ;tft.print("D");} 
    if (band==4) { tft.setCursor(5,78);tft.setTextColor(ST77XX_MAGENTA) ;tft.print("E");} 
    dispchannel=channel;
    if (channel > 100) {dispchannel = channel - 100;}
    tft.setCursor(29,78);tft.setTextColor(ST77XX_MAGENTA);tft.print(dispchannel);
    if (channel > 100){tft.print("A");}
}
