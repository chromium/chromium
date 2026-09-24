// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/app_startup_utils.h"

#import "base/apple/bundle_locations.h"
#import "ios/chrome/browser/default_browser/model/features.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

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
  kExperienceKitCatalog = 9,
  kOtherApp = 10,
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
  if ([caller_app_id isEqualToString:@"com.google.ChronosCatalog"] ||
      [caller_app_id hasPrefix:@"com.google.ChronosCatalog."]) {
    return kExperienceKitCatalog;
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

// LINT.IfChange(IsCallerAppFirstParty)
bool IsCallerAppFirstParty(MobileSessionCallerApp caller_app) {
  switch (caller_app) {
    case CALLER_APP_GOOGLE_SEARCH:
    case CALLER_APP_GOOGLE_GMAIL:
    case CALLER_APP_GOOGLE_PLUS:
    case CALLER_APP_GOOGLE_DRIVE:
    case CALLER_APP_GOOGLE_EARTH:
    case CALLER_APP_GOOGLE_OTHER:
    case CALLER_APP_GOOGLE_YOUTUBE:
    case CALLER_APP_GOOGLE_MAPS:
    case CALLER_APP_GOOGLE_CHROME_SHARE_EXTENSION:
    case CALLER_APP_GOOGLE_CHROME_OPEN_EXTENSION:
    case CALLER_APP_GOOGLE_CHROME:
      return true;
    case CALLER_APP_OTHER:
    case CALLER_APP_APPLE_MOBILESAFARI:
    case CALLER_APP_APPLE_OTHER:
    case CALLER_APP_THIRD_PARTY:
    case CALLER_APP_NOT_AVAILABLE:
    case MOBILE_SESSION_CALLER_APP_COUNT:
      return false;
  }
}
// LINT.ThenChange(//ios/chrome/app/startup/app_launch_metrics.h:MobileSessionCallerApp)

bool IsCallerAppAllowListedForAISummarization(NSString* caller_app_id) {
  if (!IsAppSwitcherAISummarizationEnabled()) {
    return false;
  }
  CallerApp caller_app = CallerAppFromAppID(caller_app_id);
  if (caller_app == CallerApp::kGmail ||
      caller_app == CallerApp::kExperienceKitCatalog) {
    return true;
  }
  return false;
}

bool IsCallerAppAllowListedForApplicationMode(NSString* caller_app_id) {
  CallerApp caller_app = CallerAppFromAppID(caller_app_id);
  if (caller_app == CallerApp::kYoutube ||
      caller_app == CallerApp::kExperienceKitCatalog) {
    return true;
  }
  return false;
}

void SaveFieldTrialValuesForGroupApp() {
  NSUserDefaults* shared_defaults = app_group::GetCommonGroupUserDefaults();
  NSNumber* supports_show_default_browser_promo = @YES;

  NSMutableDictionary* capabilities = [[shared_defaults
      dictionaryForKey:app_group::kChromeCapabilitiesPreference] mutableCopy];
  if (!capabilities) {
    capabilities = [[NSMutableDictionary alloc] init];
  }

  [capabilities setObject:supports_show_default_browser_promo
                   forKey:app_group::kChromeShowDefaultBrowserPromoCapability];

  [capabilities
      setObject:@(IsShareDefaultBrowserStatusEnabled())
         forKey:app_group::kChromeSupportShareDefaultBrowserStatusCapability];

  [capabilities
      setObject:@[ app_group::kYoutubeBundleID ]
         forKey:app_group::kChromeSupportOpenLinksParametersFromCapability];

  if (!IsAppSwitcherAISummarizationEnabled()) {
    [capabilities
        removeObjectForKey:app_group::kChromeSupportsAISummarizationCapability];
    [capabilities
        removeObjectForKey:app_group::kChromeUserIsEligibleForGeminiCapability];
  }

  [shared_defaults setObject:capabilities
                      forKey:app_group::kChromeCapabilitiesPreference];
}
