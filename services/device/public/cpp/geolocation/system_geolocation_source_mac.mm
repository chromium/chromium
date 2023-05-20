// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/geolocation/system_geolocation_source_mac.h"

#import <CoreLocation/CoreLocation.h>

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface GeolocationManagerDelegate : NSObject <CLLocationManagerDelegate> {
  BOOL _permissionInitialized;
  BOOL _hasPermission;
  base::WeakPtr<device::SystemGeolocationSourceMac> _manager;
}

- (instancetype)initWithManager:
    (base::WeakPtr<device::SystemGeolocationSourceMac>)manager;

// CLLocationManagerDelegate
- (void)locationManager:(CLLocationManager*)manager
     didUpdateLocations:(NSArray*)locations;
- (void)locationManager:(CLLocationManager*)manager
    didChangeAuthorizationStatus:(CLAuthorizationStatus)status;

- (BOOL)hasPermission;
- (BOOL)permissionInitialized;
@end

namespace device {

SystemGeolocationSourceMac::SystemGeolocationSourceMac()
    : location_manager_([[CLLocationManager alloc] init]),
      permission_update_callback_(base::DoNothing()),
      position_update_callback_(base::DoNothing()) {
  delegate_.reset([[GeolocationManagerDelegate alloc]
      initWithManager:weak_ptr_factory_.GetWeakPtr()]);
  location_manager_.get().delegate = delegate_;
}

SystemGeolocationSourceMac::~SystemGeolocationSourceMac() = default;

// static
std::unique_ptr<GeolocationManager>
SystemGeolocationSourceMac::CreateGeolocationManagerOnMac() {
  return std::make_unique<GeolocationManager>(
      std::make_unique<SystemGeolocationSourceMac>());
}

void SystemGeolocationSourceMac::RegisterPermissionUpdateCallback(
    PermissionUpdateCallback callback) {
  permission_update_callback_ = callback;
  permission_update_callback_.Run(GetSystemPermission());
}

void SystemGeolocationSourceMac::RegisterPositionUpdateCallback(
    PositionUpdateCallback callback) {
  position_update_callback_ = callback;
}

void SystemGeolocationSourceMac::PermissionUpdated() {
  permission_update_callback_.Run(GetSystemPermission());
}

void SystemGeolocationSourceMac::PositionUpdated(
    const mojom::Geoposition& position) {
  position_update_callback_.Run(
      mojom::GeopositionResult::NewPosition(position.Clone()));
}

void SystemGeolocationSourceMac::PositionError(
    const mojom::GeopositionError& error) {
  position_update_callback_.Run(
      mojom::GeopositionResult::NewError(error.Clone()));
}

void SystemGeolocationSourceMac::StartWatchingPosition(bool high_accuracy) {
  if (high_accuracy) {
    location_manager_.get().desiredAccuracy = kCLLocationAccuracyBest;
  } else {
    // Using kCLLocationAccuracyHundredMeters for consistency with Android.
    location_manager_.get().desiredAccuracy = kCLLocationAccuracyHundredMeters;
  }
  [location_manager_ startUpdatingLocation];
}

void SystemGeolocationSourceMac::StopWatchingPosition() {
  [location_manager_ stopUpdatingLocation];
}

LocationSystemPermissionStatus SystemGeolocationSourceMac::GetSystemPermission()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (![delegate_ permissionInitialized]) {
    return LocationSystemPermissionStatus::kNotDetermined;
  }

  if ([delegate_ hasPermission]) {
    return LocationSystemPermissionStatus::kAllowed;
  }

  return LocationSystemPermissionStatus::kDenied;
}

void SystemGeolocationSourceMac::AppAttemptsToUseGeolocation() {
#if BUILDFLAG(IS_IOS)
  if (@available(ios 8.0, macOS 10.15, *)) {
    [location_manager_ requestWhenInUseAuthorization];
  }
#endif
}

}  // namespace device

@implementation GeolocationManagerDelegate

- (instancetype)initWithManager:
    (base::WeakPtr<device::SystemGeolocationSourceMac>)manager {
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
  if (status == kCLAuthorizationStatusAuthorizedAlways) {
    _hasPermission = YES;
  } else {
    _hasPermission = NO;
  }

#if BUILDFLAG(IS_IOS)
  if (@available(iOS 8.0, *)) {
    if (status == kCLAuthorizationStatusAuthorizedWhenInUse) {
      _hasPermission = YES;
    }
  }
#endif

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
