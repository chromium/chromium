// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/geolocation/omnibox_geolocation_local_state.h"

#import <CoreLocation/CoreLocation.h>

#include "base/check.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/pref_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OmniboxGeolocationLocalState ()

- (int)intForPath:(const char*)path;
- (void)setInt:(int)value forPath:(const char*)path;
- (std::string)stringForPath:(const char*)path;
- (void)setString:(const std::string&)value forPath:(const char*)path;

@end

@implementation OmniboxGeolocationLocalState

+ (void)registerLocalState:(PrefRegistrySimple*)registry {
  registry->RegisterIntegerPref(
      prefs::kOmniboxGeolocationAuthorizationState,
      geolocation::kAuthorizationStateNotDeterminedWaiting);
  registry->RegisterStringPref(
      prefs::kOmniboxGeolocationLastAuthorizationAlertVersion, "");
}

- (geolocation::AuthorizationState)authorizationState {
  int authorizationState =
      [self intForPath:prefs::kOmniboxGeolocationAuthorizationState];
  // Sanitize the stored value: if the value is corrupt, then treat it as
  // kAuthorizationStateNotDeterminedWaiting.
  switch (authorizationState) {
    case geolocation::kAuthorizationStateNotDeterminedWaiting:
    case geolocation::kAuthorizationStateNotDeterminedSystemPrompt:
    case geolocation::kAuthorizationStateDenied:
    case geolocation::kAuthorizationStateAuthorized:
      break;
    default:
      authorizationState = geolocation::kAuthorizationStateNotDeterminedWaiting;
      break;
  }

  switch (CLLocationManager.authorizationStatus) {
    case kCLAuthorizationStatusNotDetermined:
      // If the user previously authorized or denied geolocation but reset the
      // system settings, then start over.
      if (authorizationState == geolocation::kAuthorizationStateAuthorized ||
          authorizationState == geolocation::kAuthorizationStateDenied) {
        authorizationState =
            geolocation::kAuthorizationStateNotDeterminedWaiting;
      }
      break;

    case kCLAuthorizationStatusRestricted:
    case kCLAuthorizationStatusDenied:
      authorizationState = geolocation::kAuthorizationStateDenied;
      break;

    case kCLAuthorizationStatusAuthorizedAlways:
    case kCLAuthorizationStatusAuthorizedWhenInUse:
      break;
  }

  return static_cast<geolocation::AuthorizationState>(authorizationState);
}

- (void)setAuthorizationState:
        (geolocation::AuthorizationState)authorizationState {
  [self setInt:authorizationState
       forPath:prefs::kOmniboxGeolocationAuthorizationState];
}

- (std::string)lastAuthorizationAlertVersion {
  return [self
      stringForPath:prefs::kOmniboxGeolocationLastAuthorizationAlertVersion];
}

- (void)setLastAuthorizationAlertVersion:(std::string)value {
  [self setString:value
          forPath:prefs::kOmniboxGeolocationLastAuthorizationAlertVersion];
}

#pragma mark - Private

- (int)intForPath:(const char*)path {
  return GetApplicationContext()->GetLocalState()->GetInteger(path);
}

- (void)setInt:(int)value forPath:(const char*)path {
  GetApplicationContext()->GetLocalState()->SetInteger(path, value);
}

- (std::string)stringForPath:(const char*)path {
  return GetApplicationContext()->GetLocalState()->GetString(path);
}

- (void)setString:(const std::string&)value forPath:(const char*)path {
  GetApplicationContext()->GetLocalState()->SetString(path, value);
}

@end
