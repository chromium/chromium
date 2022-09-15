// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Key and value names of the location of the RLZ shared state.

#include "rlz/lib/lib_values.h"

#include "rlz/lib/assert.h"

namespace rlz_lib {

//
// Ping information.
//

// rep=2: includes the new stateful events.
const char kProtocolCgiArgument[] = "rep=2";

const char kEventsCgiVariable[]         = "events";
const char kStatefulEventsCgiVariable[] = "stateful-events";
const char kEventsCgiSeparator          = ',';

const char kRlzCgiVariable[]  = "rlz";
const char kRlzCgiSeparator[] = ",";
const char kRlzCgiIndicator[] = ":";

const char kProductSignatureCgiVariable[] = "as";
const char kProductBrandCgiVariable[]     = "brand";
const char kProductLanguageCgiVariable[]  = "hl";
const char kProductIdCgiVariable[] = "pid";

const char kDccCgiVariable[] = "dcc";
const char kRlsCgiVariable[] = "rls";
const char kMachineIdCgiVariable[] = "id";
const char kSetDccResponseVariable[] = "set_dcc";

//
// Financial server information.
//

const char kFinancialPingPath[] = "/tools/pso/ping";
const char kFinancialServer[]   = "clients1.google.com";
const int kFinancialPort = 443;

// Ping times in 100-nanosecond intervals.
const int64_t kEventsPingInterval = 24LL * 3600LL * 10000000LL;   // 1 day
const int64_t kNoEventsPingInterval = kEventsPingInterval * 7LL;  // 1 week

const char kFinancialPingUserAgent[] = "Mozilla/4.0 (compatible; Win32)";
const char* kFinancialPingResponseObjects[] = { "text/*", NULL };

//
// AccessPoint and Event names.
//
//

const char* GetAccessPointName(AccessPoint point) {
  switch (point) {
  case NO_ACCESS_POINT:               return "";
  case IE_DEFAULT_SEARCH:             return "I7";
  case IE_HOME_PAGE:                  return "W1";
  case IETB_SEARCH_BOX:               return "T4";
  case QUICK_SEARCH_BOX:              return "Q1";
  case GD_DESKBAND:                   return "D1";
  case GD_SEARCH_GADGET:              return "D2";
  case GD_WEB_SERVER:                 return "D3";
  case GD_OUTLOOK:                    return "D4";
  case CHROME_OMNIBOX:                return "C1";
  case CHROME_HOME_PAGE:              return "C2";
  case FFTB2_BOX:                     return "B2";
  case FFTB3_BOX:                     return "B3";
  case PINYIN_IME_BHO:                return "N1";
  case IGOOGLE_WEBPAGE:               return "G1";
  case MOBILE_IDLE_SCREEN_BLACKBERRY: return "H1";
  case MOBILE_IDLE_SCREEN_WINMOB:     return "H2";
  case MOBILE_IDLE_SCREEN_SYMBIAN:    return "H3";
  case FF_HOME_PAGE:                  return "R0";
  case FF_SEARCH_BOX:                 return "R1";
  case IE_BROWSED_PAGE:               return "R2";
  case QSB_WIN_BOX:                   return "R3";
  case WEBAPPS_CALENDAR:              return "R4";
  case WEBAPPS_DOCS:                  return "R5";
  case WEBAPPS_GMAIL:                 return "R6";
  case IETB_LINKDOCTOR:               return "R7";
  case FFTB_LINKDOCTOR:               return "R8";
  case IETB7_SEARCH_BOX:              return "T7";
  case TB8_SEARCH_BOX:                return "T8";
  case CHROME_FRAME:                  return "C3";
  case PARTNER_AP_1:                  return "V1";
  case PARTNER_AP_2:                  return "V2";
  case PARTNER_AP_3:                  return "V3";
  case PARTNER_AP_4:                  return "V4";
  case PARTNER_AP_5:                  return "V5";
  case CHROME_MAC_OMNIBOX:            return "C5";
  case CHROME_MAC_HOME_PAGE:          return "C6";
  case CHROMEOS_OMNIBOX:              return "CA";
  case CHROMEOS_HOME_PAGE:            return "CB";
  case CHROMEOS_APP_LIST:             return "CC";
  case CHROME_IOS_OMNIBOX_TABLET:     return "C9";
  case CHROME_IOS_OMNIBOX_MOBILE:     return "CD";
  case CHROME_APP_LIST:               return "C7";
  case CHROME_MAC_APP_LIST:           return "C8";
  case UNDEFINED_AP_Q:                return "RQ";
  case UNDEFINED_AP_R:                return "RR";
  case UNDEFINED_AP_S:                return "RS";
  case UNDEFINED_AP_T:                return "RT";
  case UNDEFINED_AP_U:                return "RU";
  case UNDEFINED_AP_V:                return "RV";
  case UNDEFINED_AP_W:                return "RW";
  case UNDEFINED_AP_X:                return "RX";
  case UNDEFINED_AP_Y:                return "RY";
  case UNDEFINED_AP_Z:                return "RZ";
  case PACK_AP0:                      return "U0";
  case PACK_AP1:                      return "U1";
  case PACK_AP2:                      return "U2";
  case PACK_AP3:                      return "U3";
  case PACK_AP4:                      return "U4";
  case PACK_AP5:                      return "U5";
  case PACK_AP6:                      return "U6";
  case PACK_AP7:                      return "U7";
  case PACK_AP8:                      return "U8";
  case PACK_AP9:                      return "U9";
  case PACK_AP10:                     return "UA";
  case PACK_AP11:                     return "UB";
  case PACK_AP12:                     return "UC";
  case PACK_AP13:                     return "UD";
  case LAST_ACCESS_POINT:             ;  // Fall through.
  }

  ASSERT_STRING("GetAccessPointName: Unknown Access Point");
  return NULL;
}


bool GetAccessPointFromName(const char* name, AccessPoint* point) {
  if (!point) {
    ASSERT_STRING("GetAccessPointFromName: point is NULL");
    return false;
  }
  *point = NO_ACCESS_POINT;
  if (!name)
    return false;

  for (int i = NO_ACCESS_POINT; i < LAST_ACCESS_POINT; i++)
    if (strcmp(name, GetAccessPointName(static_cast<AccessPoint>(i))) == 0) {
      *point = static_cast<AccessPoint>(i);
      return true;
    }

  return false;
}


const char* GetEventName(Event event) {
  switch (event) {
  case INVALID_EVENT: return "";
  case INSTALL:       return "I";
  case SET_TO_GOOGLE: return "S";
  case FIRST_SEARCH:  return "F";
  case REPORT_RLS:    return "R";
  case ACTIVATE:      return "A";
  case LAST_EVENT:    ;  // Fall through.
  }

  ASSERT_STRING("GetPointName: Unknown Event");
  return NULL;
}


bool GetEventFromName(const char* name, Event* event) {
  if (!event) {
    ASSERT_STRING("GetEventFromName: event is NULL");
    return false;
  }
  *event = INVALID_EVENT;
  if (!name)
    return false;

  for (int i = INVALID_EVENT; i < LAST_EVENT; i++)
    if (strcmp(name, GetEventName(static_cast<Event>(i))) == 0) {
      *event = static_cast<Event>(i);
      return true;
    }

  return false;
}

const char* GetProductName(Product product) {
  switch (product) {
  case IE_TOOLBAR:       return "T";
  case TOOLBAR_NOTIFIER: return "P";
  case PACK:             return "U";
  case DESKTOP:          return "D";
  case CHROME:           return "C";
  case FF_TOOLBAR:       return "B";
  case QSB_WIN:          return "K";
  case WEBAPPS:          return "W";
  case PINYIN_IME:       return "N";
  case PARTNER:          return "V";
  }

  ASSERT_STRING("GetProductName: Unknown Product");
  return "";
}

}  // namespace rlz_lib
