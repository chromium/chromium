// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"

#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"

BASE_FEATURE(kIOSParcelTracking,
             "IOSParcelTracking",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsIOSParcelTrackingEnabled() {
  return base::FeatureList::IsEnabled(kIOSParcelTracking);
}

bool IsUserEligibleParcelTrackingOptInPrompt(
    ChromeBrowserState* browser_state) {
  PrefService* pref_service = browser_state->GetPrefs();
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(browser_state);
  if (!authentication_service || !authentication_service->initialized()) {
    return false;
  }
  bool signed_in =
      authentication_service->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
  return IsIOSParcelTrackingEnabled() &&
         !pref_service->GetBoolean(
             prefs::kIosParcelTrackingOptInPromptDisplayed) &&
         signed_in;
}
