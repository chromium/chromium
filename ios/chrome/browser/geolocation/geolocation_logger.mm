// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/geolocation/geolocation_logger.h"

#import <CoreLocation/CoreLocation.h>

#import "base/metrics/histogram_macros.h"
#import "components/google/core/common/google_util.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/web/public/navigation/navigation_item.h"
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

enum class PermissionStatus {
  // Status unknown, usually because the system is slow to respond.
  kPermissionUnknown = 0,
  // User has not made a permission choice.
  kPermissionNotDetermined = 1,
  // User has made a permission choice, be it allowed, denied, etc.
  kPermissionDetermined = 2,
  kMaxValue = kPermissionDetermined,
};

}  // anonymous namespace

@interface GeolocationLogger () <CLLocationManagerDelegate>

@property(nonatomic, strong) CLLocationManager* locationManager;

// Whether the permission was undefined or not. Used to choose whether to log
// the permission or not.
@property(atomic, assign) PermissionStatus permissionStatus;

@end

@implementation GeolocationLogger

+ (GeolocationLogger*)sharedInstance {
  static GeolocationLogger* instance = [[GeolocationLogger alloc] init];
  return instance;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _locationManager = [[CLLocationManager alloc] init];
    [_locationManager setDelegate:self];
    dispatch_queue_t priorityQueue =
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0ul);
    _permissionStatus = PermissionStatus::kPermissionUnknown;
    dispatch_async(priorityQueue, ^{
      self.permissionStatus = self.locationManager.authorizationStatus ==
                                      kCLAuthorizationStatusNotDetermined
                                  ? PermissionStatus::kPermissionNotDetermined
                                  : PermissionStatus::kPermissionDetermined;
    });
  }
  return self;
}

#pragma mark - Private

- (void)recordAuthorizationAction:(AuthorizationAction)authorizationAction {
  self.permissionStatus = PermissionStatus::kPermissionDetermined;
  UMA_HISTOGRAM_ENUMERATION(kGeolocationAuthorizationActionExistingUser,
                            authorizationAction, kAuthorizationActionCount);
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
  if (self.permissionStatus == PermissionStatus::kPermissionUnknown)
    return;

  if (self.permissionStatus == PermissionStatus::kPermissionNotDetermined) {
    switch (locationManager.authorizationStatus) {
      case kCLAuthorizationStatusNotDetermined:
        // We may get a spurious notification about a transition to
        // `kCLAuthorizationStatusNotDetermined` when we first start location
        // services. Ignore it and don't reset `systemPrompt_` until we get a
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
