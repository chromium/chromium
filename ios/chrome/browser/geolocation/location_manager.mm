// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/geolocation/location_manager.h"

#include "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/geolocation/CLLocation+OmniboxGeolocation.h"
#import "ios/chrome/browser/geolocation/location_manager+Testing.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/geolocation_updater_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CLLocationDistance kLocationDesiredAccuracy =
    kCLLocationAccuracyHundredMeters;
// Number of seconds to wait before automatically stopping location updates.
const NSTimeInterval kLocationStopUpdateDelay = 5.0;
// A large value to disable automatic location updates in GeolocationUpdater.
const NSTimeInterval kLocationUpdateInterval = 365.0 * 24.0 * 60.0 * 60.0;

}  // namespace

@interface LocationManager () {
  id<GeolocationUpdater> _locationUpdater;
  NSDate* _startTime;
}

// Handles GeolocationUpdater notification for an updated device location.
- (void)handleLocationUpdateNotification:(NSNotification*)notification;
// Handles GeolocationUpdater notification for ending device location updates.
- (void)handleLocationStopNotification:(NSNotification*)notification;
// Handles GeolocationUpdater notification for changing authorization.
- (void)handleAuthorizationChangeNotification:(NSNotification*)notification;

@end

@implementation LocationManager
@synthesize delegate = _delegate;
@synthesize currentLocation = _currentLocation;

- (id)init {
  self = [super init];
  if (self) {
    ios::GeolocationUpdaterProvider* provider =
        ios::GetChromeBrowserProvider()->GetGeolocationUpdaterProvider();

    // |provider| may be null in tests.
    if (provider) {
      _locationUpdater = provider->CreateGeolocationUpdater(false);
      [_locationUpdater setDesiredAccuracy:kLocationDesiredAccuracy
                            distanceFilter:kLocationDesiredAccuracy / 2];
      [_locationUpdater setStopUpdateDelay:kLocationStopUpdateDelay];
      [_locationUpdater setUpdateInterval:kLocationUpdateInterval];

      NSNotificationCenter* defaultCenter =
          [NSNotificationCenter defaultCenter];
      [defaultCenter addObserver:self
                        selector:@selector(handleLocationUpdateNotification:)
                            name:provider->GetUpdateNotificationName()
                          object:_locationUpdater];
      [defaultCenter addObserver:self
                        selector:@selector(handleLocationStopNotification:)
                            name:provider->GetStopNotificationName()
                          object:_locationUpdater];
      [defaultCenter
          addObserver:self
             selector:@selector(handleAuthorizationChangeNotification:)
                 name:provider->GetAuthorizationChangeNotificationName()
               object:nil];
    }
  }
  return self;
}

- (CLAuthorizationStatus)authorizationStatus {
  return [CLLocationManager authorizationStatus];
}

- (CLLocation*)currentLocation {
  if (!_currentLocation)
    _currentLocation = [_locationUpdater currentLocation];
  return _currentLocation;
}

- (BOOL)locationServicesEnabled {
  return !tests_hook::DisableGeolocation() &&
         [CLLocationManager locationServicesEnabled];
}

- (void)startUpdatingLocation {
  CLLocation* currentLocation = self.currentLocation;
  if (!currentLocation || [currentLocation cr_shouldRefresh]) {
    if (![_locationUpdater isEnabled])
      _startTime = [[NSDate alloc] init];

    [_locationUpdater requestWhenInUseAuthorization];
    [_locationUpdater setEnabled:YES];
  }
}

- (void)stopUpdatingLocation {
  [_locationUpdater setEnabled:NO];
}

- (void)setDesiredAccuracy:(CLLocationAccuracy)desiredAccuracy
            distanceFilter:(CLLocationDistance)distanceFilter {
  [_locationUpdater setDesiredAccuracy:desiredAccuracy
                        distanceFilter:distanceFilter];
}

#pragma mark - Private

- (void)handleLocationUpdateNotification:(NSNotification*)notification {
  NSString* newLocationKey = ios::GetChromeBrowserProvider()
                                 ->GetGeolocationUpdaterProvider()
                                 ->GetUpdateNewLocationKey();
  CLLocation* location = [[notification userInfo] objectForKey:newLocationKey];
  if (location) {
    _currentLocation = location;

    if (_startTime) {
      NSTimeInterval interval = -[_startTime timeIntervalSinceNow];
      [_currentLocation cr_setAcquisitionInterval:interval];
    }
  }
}

- (void)handleLocationStopNotification:(NSNotification*)notification {
  [_locationUpdater setEnabled:NO];
}

- (void)handleAuthorizationChangeNotification:(NSNotification*)notification {
  [_delegate locationManagerDidChangeAuthorizationStatus:self];
}

#pragma mark - LocationManager+Testing

- (void)setGeolocationUpdater:(id<GeolocationUpdater>)geolocationUpdater {
  _locationUpdater = geolocationUpdater;
}

@end
