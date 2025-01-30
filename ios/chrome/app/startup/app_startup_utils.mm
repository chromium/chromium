// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/app_startup_utils.h"

#import "base/apple/bundle_locations.h"

namespace {

enum CallerApp {
  kGoogleSearch = 0,
  kGmail = 1,
  kGooglePlus = 2,
  kGoogleDrive = 3,
  kGoogleEarth = 4,
  kGoogleOther = 5,
  kYoutube = 6,
  kGoogleMaps = 7,
  kChrome = 8,
  kOtherApp = 9,
  kMaxValue = kOtherApp,
};

CallerApp CallerAppFromAppID(NSString* caller_app_id) {
  if ([caller_app_id isEqualToString:@"com.google.GoogleMobile"]) {
    return kGoogleSearch;
  }
  if ([caller_app_id isEqualToString:@"com.google.Gmail"]) {
    return kGmail;
  }
  if ([caller_app_id isEqualToString:@"com.google.GooglePlus"]) {
    return kGooglePlus;
  }
  if ([caller_app_id isEqualToString:@"com.google.Drive"]) {
    return kGoogleDrive;
  }
  if ([caller_app_id isEqualToString:@"com.google.b612"]) {
    return kGoogleEarth;
  }

  if ([caller_app_id isEqualToString:@"com.google.ios.youtube"] ||
      [caller_app_id hasPrefix:@"com.google.ios.youtube."]) {
    return kYoutube;
  }
  if ([caller_app_id isEqualToString:@"com.google.Maps"]) {
    return kGoogleMaps;
  }
  if ([caller_app_id
          isEqualToString:[base::apple::FrameworkBundle() bundleIdentifier]]) {
    return kChrome;
  }

  if ([caller_app_id hasPrefix:@"com.google."]) {
    return kGoogleOther;
  }
  return kOtherApp;
}

}  // namespace

bool IsCallerAppFirstParty(NSString* caller_app_id) {
  CallerApp caller_app = CallerAppFromAppID(caller_app_id);
  switch (caller_app) {
    case CallerApp::kGoogleSearch:
    case CallerApp::kGmail:
    case CallerApp::kGooglePlus:
    case CallerApp::kGoogleDrive:
    case CallerApp::kGoogleEarth:
    case CallerApp::kGoogleOther:
    case CallerApp::kYoutube:
    case CallerApp::kGoogleMaps:
    case CallerApp::kChrome:
      return true;
    case CallerApp::kOtherApp:
      return false;
  }
}

bool IsCallerAppAllowListed(NSString* caller_app_id) {
  CallerApp caller_app = CallerAppFromAppID(caller_app_id);
  if (caller_app == CallerApp::kYoutube) {
    return true;
  }
  return false;
}
