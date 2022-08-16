#include <Sim800L.h>
#include "HX711.h"
#include "Nextion.h"
#include <SoftwareSerial.h>


// HX711 wire connection

const int LOADCELL_DOUT_PIN = 2;   
const int LOADCELL_SCK_PIN = 3;

HX711 scale;

//********************************************************************************

String data ;
void init_gsm();
void gprs_connect();
boolean gprs_disconnect();
boolean is_gprs_connected();
void post_to_firebase(String data);
boolean waitResponse(String expected_answer="OK", unsigned int timeout=2000);

//********************************************************************************
#define rxPin 4
#define txPin 5
SoftwareSerial SIM800(rxPin,txPin);
//********************************************************************************
//nextion text declaration
NexText t0 = NexText (0, 1, "t0");

//********************************************************************************


const String APN  = "internet";
const String USER = "Vodafone";
const String PASS = "vodafone";

const String FIREBASE_HOST  = "https://loadcell-gsm-default-rtdb.europe-west1.firebasedatabase.app/";
const String FIREBASE_SECRET  = "FmpR1CzYjPqXQsXi81TFfRZGS64rONNFDmIy7sAc";


#define USE_SSL true
#define DELAY_MS 500


//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//Function: setup() start
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
void setup() {

//  serial communication with Serial Monitor
  Serial.begin(9600);


 
// scale connection
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN); // Starting the loadcell
  Serial.println("initializing scale..."); // hmi yazma işlemi entegre edilecek ( hoşgeldiniz yazısı)
  
// scale initialize

  scale.read(); //raw reading
  scale.read_average(20);
  scale.get_value(5);
 (scale.get_units(5),1);
  scale.set_scale(104);
  scale.tare();
// scale initialize

  //Begin serial communication with SIM800
  SIM800.begin(9600);
 
  Serial.println("Initializing SIM800...");
  init_gsm();

  //nextion 

  nexSerial.begin(9600);
  nexInit();
}



//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//Function: loop() start
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
void loop() {



  Serial.println("Reading....."); //hmi üzerine işle
  
   data = (scale.get_units(10),1);
   Serial.print("weight=");
   Serial.println(data); 
   
  scale.power_down(); // going dark
  delay(2000);
  scale.power_up();
 
  if(!is_gprs_connected()){
    gprs_connect();
  }
 
  post_to_firebase(data);
  delay(1000);

//  String sending "t0.txt";

  Serial.print("t0.txt=");
  Serial.print(data);
  Serial.write(0xff);
  Serial.write(0xff);
  Serial.write(0xff);

 
  delay(1000);
}


//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//Function: post_to_firebase() start
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
void post_to_firebase(String data)
{
 
  //********************************************************************************
  //Start HTTP connection
  SIM800.println("AT+HTTPINIT");
  waitResponse();
  delay(DELAY_MS);
  //********************************************************************************
  //Enabling SSL 1.0
  if(USE_SSL == true){
    SIM800.println("AT+HTTPSSL=1");
    waitResponse();
    delay(DELAY_MS);
  }
  //********************************************************************************
  //Setting up parameters for HTTP session
  SIM800.println("AT+HTTPPARA=\"CID\",1");
  waitResponse();
  delay(DELAY_MS);
  //********************************************************************************
  //Set the HTTP URL - Firebase URL and FIREBASE SECRET
  SIM800.println("AT+HTTPPARA=\"URL\","+FIREBASE_HOST+".json?auth="+FIREBASE_SECRET);
  waitResponse();
  delay(DELAY_MS);
  //********************************************************************************
  //Setting up re direct
  SIM800.println("AT+HTTPPARA=\"REDIR\",1");
  waitResponse();
  delay(DELAY_MS);
  //********************************************************************************
  //Setting up content type
  SIM800.println("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
  waitResponse();
  delay(DELAY_MS);
  //********************************************************************************
  //Setting up Data Size
  //+HTTPACTION: 1,601,0 - error occurs if data length is not correct
  SIM800.println("AT+HTTPDATA=" + String(data.length()) + ",10000");
  waitResponse("DOWNLOAD");
  //delay(DELAY_MS);
  //********************************************************************************
  //Sending Data
  SIM800.println(data);
  waitResponse();
  delay(DELAY_MS);
  //********************************************************************************
  //Sending HTTP POST request
  SIM800.println("AT+HTTPACTION=1");
 
  for (uint32_t start = millis(); millis() - start < 20000;){
    while(!SIM800.available());
    String response = SIM800.readString();
    if(response.indexOf("+HTTPACTION:") > 0)
    {
      Serial.println(response);
      break;
    }
  }
   
  delay(DELAY_MS);
  //********************************************************************************
  //+HTTPACTION: 1,603,0 (POST to Firebase failed)
  //+HTTPACTION: 0,200,0 (POST to Firebase successfull)
  //Read the response
  SIM800.println("AT+HTTPREAD");
  waitResponse("OK");
  delay(DELAY_MS);
  //********************************************************************************
  //Stop HTTP connection
  SIM800.println("AT+HTTPTERM");
  waitResponse("OK",1000);
  delay(DELAY_MS);
  //********************************************************************************
}





//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// Initialize GSM Module
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
void init_gsm()
{
  //********************************************************************************
  //Testing AT Command
  SIM800.println("AT");
  waitResponse();
  delay(DELAY_MS);
  //********************************************************************************
  //Checks if the SIM is ready
  SIM800.println("AT+CPIN?");
  waitResponse("+CPIN: READY");
  delay(DELAY_MS);
  //********************************************************************************
  //Turning ON full functionality
  SIM800.println("AT+CFUN=1");
  waitResponse();
  delay(DELAY_MS);
  //********************************************************************************
  //Turn ON verbose error codes
  SIM800.println("AT+CMEE=2");
  waitResponse();
  delay(DELAY_MS);
  //********************************************************************************
  //Enable battery checks
  SIM800.println("AT+CBATCHK=1");
  waitResponse();
  delay(DELAY_MS);
  //********************************************************************************
  //Register Network (+CREG: 0,1 or +CREG: 0,5 for valid network)
  //+CREG: 0,1 or +CREG: 0,5 for valid network connection
  SIM800.println("AT+CREG?");
  waitResponse("+CREG: 0,");
  delay(DELAY_MS);
  //********************************************************************************
  //setting SMS text mode
  SIM800.print("AT+CMGF=1\r");
  waitResponse("OK");
  delay(DELAY_MS);
  //********************************************************************************
}





//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//Connect to the internet
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
void gprs_connect()
{
  //********************************************************************************
  //DISABLE GPRS
  SIM800.println("AT+SAPBR=0,1");
  waitResponse("OK",60000);
  delay(DELAY_MS);
  //********************************************************************************
  //Connecting to GPRS: GPRS - bearer profile 1
  SIM800.println("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
  waitResponse();
  delay(DELAY_MS);
  //********************************************************************************
  //sets the APN settings for your sim card network provider.
  SIM800.println("AT+SAPBR=3,1,\"APN\","+APN);
  waitResponse();
  delay(DELAY_MS);
  //********************************************************************************
  //sets the user name settings for your sim card network provider.
  if(USER != ""){
    SIM800.println("AT+SAPBR=3,1,\"USER\","+USER);
    waitResponse();
    delay(DELAY_MS);
  }
  //********************************************************************************
  //sets the password settings for your sim card network provider.
  if(PASS != ""){
    SIM800.println("AT+SAPBR=3,1,\"PASS\","+PASS);
    waitResponse();
    delay(DELAY_MS);
  }
  //********************************************************************************
  //after executing the following command. the LED light of
  //sim800l blinks very fast (twice a second)
  //enable the GPRS: enable bearer 1
  SIM800.println("AT+SAPBR=1,1");
  waitResponse("OK", 30000);
  delay(DELAY_MS);
  //********************************************************************************
  //Get IP Address - Query the GPRS bearer context status
  SIM800.println("AT+SAPBR=2,1");
  waitResponse("OK");
  delay(DELAY_MS);
  //********************************************************************************
}


/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
* Function: gprs_disconnect()
* AT+CGATT = 1 modem is attached to GPRS to a network.
* AT+CGATT = 0 modem is not attached to GPRS to a network
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
boolean gprs_disconnect()
{
  //********************************************************************************
  //Disconnect GPRS
  SIM800.println("AT+CGATT=0");
  waitResponse("OK",60000);
  //delay(DELAY_MS);
  //********************************************************************************
  //DISABLE GPRS
  //SIM800.println("AT+SAPBR=0,1");
  //waitResponse("OK",60000);
  //delay(DELAY_MS);
  //********************************************************************************

  return true;
}





/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
* Function: gprs_disconnect()
* checks if the gprs connected.
* AT+CGATT = 1 modem is attached to GPRS to a network.
* AT+CGATT = 0 modem is not attached to GPRS to a network
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
boolean is_gprs_connected()
{
  SIM800.println("AT+CGATT?");
  if(waitResponse("+CGATT: 1",6000) == 1) { return false; }

  return true;
}



//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//Handling AT COMMANDS
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//boolean waitResponse(String expected_answer="OK", unsigned int timeout=2000) //uncomment if syntax error (arduino)
boolean waitResponse(String expected_answer, unsigned int timeout) //uncomment if syntax error (esp8266)
{
  uint8_t x=0, answer=0;
  String response;
  unsigned long previous;
   
  //Clean the input buffer
  while( SIM800.available() > 0) SIM800.read();
 
  //********************************************************************************
  previous = millis();
  do{
    //if data in UART INPUT BUFFER, reads it
    if(SIM800.available() != 0){
        char c = SIM800.read();
        response.concat(c);
        x++;
        //checks if the (response == expected_answer)
        if(response.indexOf(expected_answer) > 0){
            answer = 1;
        }
    }
  }while((answer == 0) && ((millis() - previous) < timeout));
  //********************************************************************************
 
  Serial.println(response);
  return answer;
}
