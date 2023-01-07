// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RLZ_LIB_RLZ_ENUMS_H_
#define RLZ_LIB_RLZ_ENUMS_H_

namespace rlz_lib {

// An Access Point offers a way to search using Google.
enum AccessPoint {
  NO_ACCESS_POINT = 0,

  // Access points on Windows PCs.
  IE_DEFAULT_SEARCH,  // The IE7+ chrome search box next to the address bar.
  IE_HOME_PAGE,       // Search box on IE 5+ primary home page when Google.
  IETB_SEARCH_BOX,    // IE Toolbar v4+ search box.
  QUICK_SEARCH_BOX,   // Search box brought up by ctrl-ctrl key sequence,
                      // distributed as a part of Google Desktop
  GD_DESKBAND,        // Search box in deskbar when GD in deskbar mode.
  GD_SEARCH_GADGET,   // Search gadget when GD in sidebar mode.
  GD_WEB_SERVER,      // Boxes in web pages shown by local GD web server.
  GD_OUTLOOK,         // Search box installed within outlook by GD.
  CHROME_OMNIBOX,     // Chrome searches through the address bar omnibox (Win).
  CHROME_HOME_PAGE,   // Chrome searches through Google as home page (Win).
  FFTB2_BOX,          // Firefox Toolbar v2 Search Box.
  FFTB3_BOX,          // Firefox Toolbar v3+ Search Box.
  PINYIN_IME_BHO,     // Goopy Input Method Editor BHO (Pinyin).
  IGOOGLE_WEBPAGE,    // Searches on iGoogle through partner deals.

  // Mobile idle screen search for different platforms.
  MOBILE_IDLE_SCREEN_BLACKBERRY,
  MOBILE_IDLE_SCREEN_WINMOB,
  MOBILE_IDLE_SCREEN_SYMBIAN,

  FF_HOME_PAGE,       // Firefox home page when set to Google.
  FF_SEARCH_BOX,      // Firefox search box when set to Google.
  IE_BROWSED_PAGE,    // Search made in IE through user action (no product).
  QSB_WIN_BOX,        // Search box brought up by ctrl+space by default,
                      // distributed by toolbar and separate from the GD QSB
  WEBAPPS_CALENDAR,   // Webapps use of calendar.
  WEBAPPS_DOCS,       // Webapps use of writely.
  WEBAPPS_GMAIL,      // Webapps use of Gmail.

  IETB_LINKDOCTOR,    // Linkdoctor of IE Toolbar
  FFTB_LINKDOCTOR,    // Linkdoctor of FF Toolbar
  IETB7_SEARCH_BOX,   // IE Toolbar search box.
  TB8_SEARCH_BOX,     // IE/FF Toolbar search box.
  CHROME_FRAME,       // Chrome Frame.

  // Partner access points.
  PARTNER_AP_1,
  PARTNER_AP_2,
  PARTNER_AP_3,
  PARTNER_AP_4,
  PARTNER_AP_5,

  CHROME_MAC_OMNIBOX,  // Chrome searches through the address bar omnibox (Mac).
  CHROME_MAC_HOME_PAGE,// Chrome searches through Google as home page (Mac).

  CHROMEOS_OMNIBOX,    // ChromeOS searches through the address bar omnibox.
  CHROMEOS_HOME_PAGE,  // ChromeOS searches through Google as home page.
  CHROMEOS_APP_LIST,   // ChromeOS searches through the app launcher search box.

  // Chrome searches through the address bar omnibox (iOS) on tablet or phone.
  CHROME_IOS_OMNIBOX_TABLET,
  CHROME_IOS_OMNIBOX_MOBILE,

  CHROME_APP_LIST,     // Chrome searches through the app launcher search box.
  CHROME_MAC_APP_LIST, // Chrome searches through the app launcher search box
                       // (Mac).

  // Unclaimed access points - should be used first before creating new APs.
  // Please also make sure you re-name the enum before using an unclaimed value;
  // this acts as a check to ensure we don't have collisions.
  UNDEFINED_AP_Q,
  UNDEFINED_AP_R,
  UNDEFINED_AP_S,
  UNDEFINED_AP_T,
  UNDEFINED_AP_U,
  UNDEFINED_AP_V,
  UNDEFINED_AP_W,
  UNDEFINED_AP_X,
  UNDEFINED_AP_Y,
  UNDEFINED_AP_Z,

  PACK_AP0,
  PACK_AP1,
  PACK_AP2,
  PACK_AP3,
  PACK_AP4,
  PACK_AP5,
  PACK_AP6,
  PACK_AP7,
  PACK_AP8,
  PACK_AP9,
  PACK_AP10,
  PACK_AP11,
  PACK_AP12,
  PACK_AP13,

  // New Access Points should be added here without changing existing enums,
  // (i.e. before LAST_ACCESS_POINT)
  LAST_ACCESS_POINT
};

// A product is an entity which wants to gets credit for setting
// an Access Point.
enum Product {
  IE_TOOLBAR = 1,
  TOOLBAR_NOTIFIER,
  PACK,
  DESKTOP,
  CHROME,
  FF_TOOLBAR,
  QSB_WIN,
  WEBAPPS,
  PINYIN_IME,
  PARTNER
  // New Products should be added here without changing existing enums.
};

// Events that note Product and Access Point modifications.
enum Event {
  INVALID_EVENT = 0,
  INSTALL = 1,    // Access Point added to the system.
  SET_TO_GOOGLE,  // Point set from non-Google provider to Google.
  FIRST_SEARCH,   // First search from point since INSTALL
  REPORT_RLS,     // Report old system "RLS" financial value for this point.
  // New Events should be added here without changing existing enums,
  // before LAST_EVENT.
  ACTIVATE,       // Product being used for a period of time.
  LAST_EVENT
};

}  // namespace rlz_lib

#endif  // RLZ_LIB_RLZ_ENUMS_H_
