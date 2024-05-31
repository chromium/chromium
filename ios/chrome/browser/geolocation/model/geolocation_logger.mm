// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/geolocation/model/geolocation_logger.h"

#import <CoreLocation/CoreLocation.h>

#import "base/metrics/histogram_macros.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(AuthorizationStatus)
enum class AuthorizationStatus {
  // The user has not chosen whether to allow location access.
  kNotDetermined = 0,
  // The user cannot allow location access.
  kRestricted = 1,
  // The user denied location access either for the app or globally.
  kDenied = 2,
  // The user granted location access at all times.
  kAuthorizedAlways = 3,
  // The user granted location access only while the app is in use.
  kAuthorizedWhenInUse = 4,
  kMaxValue = kAuthorizedWhenInUse
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/geolocation/enums.xml)

// Name of the histogram recording initial geolocation authorization state.
constexpr char kGeolocationInitialAuthorizationStateHistogram[] =
    "Geolocation.IOS.InitialAuthorizationState";

// Name of the histogram recording a change in geolocation authorization state.
constexpr char kGeolocationAuthorizationStateChangedHistogram[] =
    "Geolocation.IOS.ChangedAuthorizationState";

AuthorizationStatus ToAuthorizationStatus(
    CLAuthorizationStatus authorization_status) {
  switch (authorization_status) {
    case kCLAuthorizationStatusNotDetermined:
      return AuthorizationStatus::kNotDetermined;
    case kCLAuthorizationStatusRestricted:
      return AuthorizationStatus::kRestricted;
    case kCLAuthorizationStatusDenied:
      return AuthorizationStatus::kDenied;
    case kCLAuthorizationStatusAuthorizedAlways:
      return AuthorizationStatus::kAuthorizedAlways;
    case kCLAuthorizationStatusAuthorizedWhenInUse:
      return AuthorizationStatus::kAuthorizedWhenInUse;
    default:
      // Since CLAuthorizationStatus is an iOS-provided enum, safely handle new
      // values by falling back to `kNotDetermined`.
      return AuthorizationStatus::kNotDetermined;
  }
}

}  // anonymous namespace

@interface GeolocationLogger () <CLLocationManagerDelegate>

@property(nonatomic, strong) CLLocationManager* locationManager;

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
  }
  return self;
}

#pragma mark - CLLocationManagerDelegate

- (void)locationManagerDidChangeAuthorization:
    (CLLocationManager*)locationManager {
  // The initial call to this method represents the initial value of geolocation
  // authorization status rather than a change.
  static BOOL initialCall = YES;
  if (initialCall) {
    initialCall = NO;
    UMA_HISTOGRAM_ENUMERATION(
        kGeolocationInitialAuthorizationStateHistogram,
        ToAuthorizationStatus(self.locationManager.authorizationStatus));
    return;
  }

  UMA_HISTOGRAM_ENUMERATION(
      kGeolocationAuthorizationStateChangedHistogram,
      ToAuthorizationStatus(self.locationManager.authorizationStatus));
}

@end
