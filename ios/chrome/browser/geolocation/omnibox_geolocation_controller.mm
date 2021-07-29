// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"

#import <CoreLocation/CoreLocation.h>

#include "base/metrics/histogram_macros.h"
#include "components/google/core/common/google_util.h"
#import "ios/chrome/app/tests_hook.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Values for the histograms that record the user's action when prompted to
// authorize the use of location by Chrome. These match the definition of
// GeolocationAuthorizationAction in Chromium
// src-internal/tools/histograms/histograms.xml.
typedef enum {
  // The user authorized use of location.
  kAuthorizationActionAuthorized = 0,
  // The user permanently denied use of location (Don't Allow).
  kAuthorizationActionPermanentlyDenied,
  // The user denied use of location at this prompt (Not Now).
  kAuthorizationActionDenied,
  // The number of possible AuthorizationAction values to report.
  kAuthorizationActionCount,
} AuthorizationAction;

// Name of the histogram recording AuthorizationAction for an existing user.
const char* const kGeolocationAuthorizationActionExistingUser =
    "Geolocation.AuthorizationActionExistingUser";

// Name of the histogram recording AuthorizationAction for a new user.
const char* const kGeolocationAuthorizationActionNewUser =
    "Geolocation.AuthorizationActionNewUser";

// Returns the current authorization status for the given CLLocationManager.
// TODO(crbug.com/1173902): Remove this helper once the min deployment target is
// updated to iOS 14.
CLAuthorizationStatus GetAuthorizationStatus(CLLocationManager* manager) {
#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_14_0
  return manager.authorizationStatus;
#else
  return CLLocationManager.authorizationStatus;
#endif
}

}  // anonymous namespace

@interface OmniboxGeolocationController () <CLLocationManagerDelegate>

@property(nonatomic, strong) CLLocationManager* locationManager;

// Records whether we are prompting for a new user, so that we can record the
// user's action to the right histogram (either
// kGeolocationAuthorizationActionExistingUser or
// kGeolocationAuthorizationActionNewUser).
@property(nonatomic, assign) BOOL newUser;

// Whether the permission was undefined or not. Used to choose whether to log
// the permission or not.
@property(nonatomic, assign) BOOL permissionWasUndefined;

@end

@implementation OmniboxGeolocationController

+ (OmniboxGeolocationController*)sharedInstance {
  static OmniboxGeolocationController* instance =
      [[OmniboxGeolocationController alloc] init];
  return instance;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _locationManager = [[CLLocationManager alloc] init];
    [_locationManager setDelegate:self];
    _permissionWasUndefined = GetAuthorizationStatus(_locationManager) ==
                              kCLAuthorizationStatusNotDetermined;
  }
  return self;
}

- (void)triggerSystemPrompt {
  if (self.locationServicesEnabled &&
      GetAuthorizationStatus(self.locationManager) ==
          kCLAuthorizationStatusNotDetermined) {
    self.newUser = YES;

    // Turn on location updates, so that iOS will prompt the user.
    [self.locationManager requestWhenInUseAuthorization];
  }
}

- (void)systemPromptSkippedForNewUser {
  self.newUser = YES;
}

#pragma mark - Private

- (void)recordAuthorizationAction:(AuthorizationAction)authorizationAction {
  self.permissionWasUndefined = NO;
  if (self.newUser) {
    self.newUser = NO;

    UMA_HISTOGRAM_ENUMERATION(kGeolocationAuthorizationActionNewUser,
                              authorizationAction, kAuthorizationActionCount);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kGeolocationAuthorizationActionExistingUser,
                              authorizationAction, kAuthorizationActionCount);
  }
}

// Boolean value indicating whether location services are enabled on the
// device.
- (BOOL)locationServicesEnabled {
  return !tests_hook::DisableGeolocation() &&
         [CLLocationManager locationServicesEnabled];
}

#pragma mark - CLLocationManagerDelegate

- (void)locationManagerDidChangeAuthorization:
    (CLLocationManager*)locationManager {
  if (self.permissionWasUndefined) {
    switch (GetAuthorizationStatus(locationManager)) {
      case kCLAuthorizationStatusNotDetermined:
        // We may get a spurious notification about a transition to
        // |kCLAuthorizationStatusNotDetermined| when we first start location
        // services. Ignore it and don't reset |systemPrompt_| until we get a
        // real change.
        break;

      case kCLAuthorizationStatusRestricted:
      case kCLAuthorizationStatusDenied:
        [self recordAuthorizationAction:kAuthorizationActionPermanentlyDenied];
        break;

      case kCLAuthorizationStatusAuthorizedAlways:
      case kCLAuthorizationStatusAuthorizedWhenInUse:
        [self recordAuthorizationAction:kAuthorizationActionAuthorized];
        break;
    }
  }
}

@end
