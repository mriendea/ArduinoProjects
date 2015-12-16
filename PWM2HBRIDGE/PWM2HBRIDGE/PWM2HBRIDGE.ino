/*
  PWM2HBRIDGE - M. Riendeau
  
  Takes a servo signal and converts it to two dual h-bridge drive signal sets ( 4 signals ) This is 
  sufficient to drive 2 sets of two wheels independently and cheaply. The intent is to make model available for 
  FIRST robotics teams, without too much information.
  
 */

// Channel status byte coordinates channel values
#define CS_SVFB 0x1
#define CS_SVLR 0x2
#define CS_SVX  0x3
#define CS_MLA  0x10
#define CS_MLB  0x20
#define CS_MRA  0x40
#define CS_MRB  0x80
#define CS_MLX  PS_MLA  | PS_MLB
#define CS_MRX  PS_MRA  | PS_MRB
#define CS_MX   PS_MLAX | PS_MLBX

byte CHstat = 0x0;

// dwell is the amount of time (us) around the neutral pos(+/-) where changes to the PWM signals not applied to motors
#define ZERO_DWELL 5
#define PANIC_SW   1000000

// Time in ms to wait for operator
#define OPERATOR_DLY 2000 
#define HB_PERIOD

// State variable used to initialize/cal and deploy
#define ST_INIT      0 // Reset/Initialize
#define ST_DETECT    1 // Detect existence of pulses, then wait for operator to center the controls
#define ST_ZEROCOUNT 2 // Capture the loop count of the 1.5ms null pos.
#define ST_DEPLOY    3 // Ready to go

int State = ST_INIT;

/* Servo inputs channels used FB is forward/backward LR is turn left/right */

#define SV_FBCHAN 11
#define SV_LRCHAN 10

typedef struct {
  byte ch;      // Arduino channel number
  byte val;     // Current value
  int zero;     // Neutral count at 1.5ms
  int pcount;   // Current loop count for the pulse
  int period;   // Servo period count
  int panic;    // Test for signal loss
  int fullrg;   // Number of counts for 100% duty
} servochan;

servochan svFB;
servochan svLR;

/* H-Bridge PWM outputs */

typedef struct {
  byte cha;
  byte chb;
  int pcount;
  int counthi;
} motordrv;

/*
   OUTPUT channels used 
   HBRIDGE IN1/IN2 Arduino CH4/CH5 left  motor gnd/vcc
   HBRIDGE IN3/IN4 Arduino CH6/CH7 right motor gnd/vcc
*/

#define MOT_PERIOD 200 // counts

motordrv motL {4,5,0,0};
motordrv motR {6,7,0,0};

// the setup routine runs once when you press reset:
void setup() {
  // init Servo inputs
  pinMode(svFB.ch, INPUT);
  pinMode(svLR.ch, INPUT);

  // init HB
  pinMode(motL.cha,OUTPUT);
  pinMode(motL.chb,OUTPUT);
  pinMode(motR.cha,OUTPUT);
  pinMode(motR.chb,OUTPUT);
}

// the loop routine runs over and over again forever:

void loop() {
  int i;
  int duty;
      
  if ( State == ST_INIT ) // initialize
  {  
    memset(&svFB,0,sizeof(servochan));
    memset(&svLR,0,sizeof(servochan));
    svFB.ch = SV_FBCHAN;
    svLR.ch = SV_LRCHAN;

    // motors off
    digitalWrite(motL.cha,LOW);
    digitalWrite(motL.chb,LOW);
    digitalWrite(motR.cha,LOW);
    digitalWrite(motR.chb,LOW);
    
    Pstat = 0;
    State = ST_DETECT;
  }
  
  svFB.val = digitalRead(svFB.ch);
  svLR.val = digitalRead(svLR.ch);
    
  // Look for falling edges to capture null pos. counts
  if ( State == ST_ZEROCOUNT && (Pstat & CS_SVX) == CS_SVX && ( svFB.zero == 0 || svLR.zero == 0 ) )
  {
    if (svFB.val == 0) svFB.zero = svFB.pcount;
    if (svLR.val == 0) svLR.zero = svLR.pcount;
    if ( svFB.zero == 0 || svLR.zero == 0 ) return;
    // calculate the duty factor in loops/ms
    svFB.fullrg = (svFB.zero * 0.5) / 1.5; // counts/ms * 0.5ms(delta +/-) / 1.5ms (zero pt) 
    svLR.fullrg = (svLR.zero * 0.5) / 1.5;
    State = ST_DEPLOY;
    return;
  }
       
  // Did we lose a signal?
  
  if ( svFB.val == 0 )
  { svFB.pcount = 0; if (svFB.panic++ > PANIC_SW) State = ST_INIT; return; }
  else
  { svFB.pcount++; CHstat |= CS_SVFB; svFB.panic = 0;  }
    
  if ( svLR.val == 0 )
  { svLR.pcount = 0; if (svLR.panic++ > PANIC_SW) State = ST_INIT; return; }
  else
  { svLR.pcount++; Pstat |= CS_SVLR; svLR.panic = 0; }
  
  if ( State == ST_DETECT && (Pstat & PS_SVX) == PS_SVX ) // test for pulse existence on all servos
  {
    Pstat &= ~PS_SVX;
    delay(OPERATOR_DLY);
    State = ST_ZEROCOUNT;
    return;
  }
    
  // Operate normally
  
  if ( State == ST_DEPLOY )
  {
    motL.counthi = 0;
    motR.counthi = 0;
    
    i = svFB.pcount - svFB.zero;
    if ( i < -1 * ZERO_DWELL || i > ZERO_DWELL )
    {
      // Set duty to pure FB motion
      duty = (i / svFB.fullrg) * MOT_PERIOD;
      motL.counthi = duty;
      motR.counthi = duty;
  
      // TBD adjust duty for turning

    }
    
    // Motor period has ended
    if ( motL.pcount > motL.counthi )
    {
        motL.pcount = 0;
        if (motL.counthi > 0) // we do have voltage
          CHstat |= CH_MLB
    }
    
    if ( motL.counthi > 0 and motL.pcount == 0 )
    { Pstat |= PS_MLB; }
      

    digitalWrite(motL.cha, HIGH);
    digitalWrite(motL.chb, LOW);
  }

  delayMicroseconds(1);
}


