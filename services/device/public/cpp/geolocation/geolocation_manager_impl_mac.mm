// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <CoreLocation/CoreLocation.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "base/sequence_checker.h"
#include "services/device/public/cpp/geolocation/geolocation_manager_impl_mac.h"

@interface GeolocationManagerDelegate : NSObject <CLLocationManagerDelegate> {
  BOOL _permissionInitialized;
  BOOL _hasPermission;
  base::WeakPtr<device::GeolocationManagerImpl> _manager;
}

- (instancetype)initWithManager:
    (base::WeakPtr<device::GeolocationManagerImpl>)manager;

// CLLocationManagerDelegate
- (void)locationManager:(CLLocationManager*)manager
     didUpdateLocations:(NSArray*)locations;
- (void)locationManager:(CLLocationManager*)manager
    didChangeAuthorizationStatus:(CLAuthorizationStatus)status;

- (BOOL)hasPermission;
- (BOOL)permissionInitialized;
@end

namespace device {

GeolocationManagerImpl::GeolocationManagerImpl()
    : location_manager_([[CLLocationManager alloc] init]) {
  delegate_.reset([[GeolocationManagerDelegate alloc]
      initWithManager:weak_ptr_factory_.GetWeakPtr()]);
  location_manager_.get().delegate = delegate_;
}

GeolocationManagerImpl::~GeolocationManagerImpl() = default;

// static
std::unique_ptr<GeolocationManager> GeolocationManagerImpl::Create() {
  return std::make_unique<GeolocationManagerImpl>();
}

void GeolocationManagerImpl::PermissionUpdated() {
  NotifyPermissionObservers(GetSystemPermission());
}

void GeolocationManagerImpl::PositionUpdated(
    const mojom::Geoposition& position) {
  NotifyPositionObservers(position);
}

void GeolocationManagerImpl::StartWatchingPosition(bool high_accuracy) {
  if (high_accuracy) {
    location_manager_.get().desiredAccuracy = kCLLocationAccuracyBest;
  } else {
    // Using kCLLocationAccuracyHundredMeters for consistency with Android.
    location_manager_.get().desiredAccuracy = kCLLocationAccuracyHundredMeters;
  }
  [location_manager_ startUpdatingLocation];
}

void GeolocationManagerImpl::StopWatchingPosition() {
  [location_manager_ stopUpdatingLocation];
}

LocationSystemPermissionStatus GeolocationManagerImpl::GetSystemPermission()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (![delegate_ permissionInitialized])
    return LocationSystemPermissionStatus::kNotDetermined;

  if ([delegate_ hasPermission])
    return LocationSystemPermissionStatus::kAllowed;

  return LocationSystemPermissionStatus::kDenied;
}

}  // namespace device

@implementation GeolocationManagerDelegate

- (instancetype)initWithManager:
    (base::WeakPtr<device::GeolocationManagerImpl>)manager {
  if (self = [super init]) {
    _permissionInitialized = NO;
    _hasPermission = NO;
    _manager = manager;
  }
  return self;
}

- (void)locationManager:(CLLocationManager*)manager
    didChangeAuthorizationStatus:(CLAuthorizationStatus)status {
  _permissionInitialized = YES;
  if (status == kCLAuthorizationStatusAuthorizedAlways)
    _hasPermission = YES;
  else
    _hasPermission = NO;
  _manager->PermissionUpdated();
}

- (BOOL)hasPermission {
  return _hasPermission;
}

- (BOOL)permissionInitialized {
  return _permissionInitialized;
}

- (void)locationManager:(CLLocationManager*)manager
     didUpdateLocations:(NSArray*)locations {
  CLLocation* location = [locations lastObject];
  device::mojom::Geoposition position;
  position.latitude = location.coordinate.latitude;
  position.longitude = location.coordinate.longitude;
  position.timestamp =
      base::Time::FromDoubleT(location.timestamp.timeIntervalSince1970);
  position.altitude = location.altitude;
  position.accuracy = location.horizontalAccuracy;
  position.altitude_accuracy = location.verticalAccuracy;
  position.speed = location.speed;
  position.heading = location.course;

  _manager->PositionUpdated(position);
}
@end
