// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/geolocation/system_geolocation_source_apple.h"

#import <CoreLocation/CoreLocation.h>

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/mac/mac_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"

@interface GeolocationSystemPermissionManagerDelegate
    : NSObject <CLLocationManagerDelegate> {
  BOOL _permissionInitialized;
  BOOL _hasPermission;
  base::WeakPtr<device::SystemGeolocationSourceApple> _manager;
}

- (instancetype)initWithManager:
    (base::WeakPtr<device::SystemGeolocationSourceApple>)manager;

// CLLocationManagerDelegate
- (void)locationManager:(CLLocationManager*)manager
     didUpdateLocations:(NSArray*)locations;
- (void)locationManager:(CLLocationManager*)manager
    didChangeAuthorizationStatus:(CLAuthorizationStatus)status;
- (void)locationManager:(CLLocationManager*)manager
       didFailWithError:(NSError*)error;

- (BOOL)hasPermission;
- (BOOL)permissionInitialized;
@end

namespace device {

SystemGeolocationSourceApple::SystemGeolocationSourceApple()
    : location_manager_([[CLLocationManager alloc] init]),
      permission_update_callback_(base::DoNothing()),
      position_observers_(base::MakeRefCounted<PositionObserverList>()) {
  delegate_ = [[GeolocationSystemPermissionManagerDelegate alloc]
      initWithManager:weak_ptr_factory_.GetWeakPtr()];
  location_manager_.delegate = delegate_;
}

SystemGeolocationSourceApple::~SystemGeolocationSourceApple() = default;

// static
std::unique_ptr<GeolocationSystemPermissionManager>
SystemGeolocationSourceApple::CreateGeolocationSystemPermissionManager() {
  return std::make_unique<GeolocationSystemPermissionManager>(
      std::make_unique<SystemGeolocationSourceApple>());
}

void SystemGeolocationSourceApple::RegisterPermissionUpdateCallback(
    PermissionUpdateCallback callback) {
  permission_update_callback_ = callback;
  permission_update_callback_.Run(GetSystemPermission());
}

void SystemGeolocationSourceApple::PermissionUpdated() {
  permission_update_callback_.Run(GetSystemPermission());
}

void SystemGeolocationSourceApple::PositionUpdated(
    const mojom::Geoposition& position) {
  position_observers_->Notify(FROM_HERE, &PositionObserver::OnPositionUpdated,
                              position);
}

void SystemGeolocationSourceApple::PositionError(
    const mojom::GeopositionError& error) {
  position_observers_->Notify(FROM_HERE, &PositionObserver::OnPositionError,
                              error);
}

void SystemGeolocationSourceApple::StartWatchingPosition(bool high_accuracy) {
  if (high_accuracy) {
    location_manager_.desiredAccuracy = kCLLocationAccuracyBest;
  } else {
    // Using kCLLocationAccuracyHundredMeters for consistency with Android.
    location_manager_.desiredAccuracy = kCLLocationAccuracyHundredMeters;
  }
  [location_manager_ startUpdatingLocation];
}

void SystemGeolocationSourceApple::StopWatchingPosition() {
  [location_manager_ stopUpdatingLocation];
}

LocationSystemPermissionStatus SystemGeolocationSourceApple::GetSystemPermission()
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

void SystemGeolocationSourceApple::OpenSystemPermissionSetting() {
#if BUILDFLAG(IS_MAC)
  base::mac::OpenSystemSettingsPane(
      base::mac::SystemSettingsPane::kPrivacySecurity_LocationServices);
#endif
}

void SystemGeolocationSourceApple::RequestPermission() {
  [location_manager_ requestWhenInUseAuthorization];
}

void SystemGeolocationSourceApple::AddPositionUpdateObserver(
    PositionObserver* observer) {
  position_observers_->AddObserver(observer);
}

void SystemGeolocationSourceApple::RemovePositionUpdateObserver(
    PositionObserver* observer) {
  position_observers_->RemoveObserver(observer);
}

}  // namespace device

@implementation GeolocationSystemPermissionManagerDelegate

- (instancetype)initWithManager:
    (base::WeakPtr<device::SystemGeolocationSourceApple>)manager {
  if (self = [super init]) {
    _permissionInitialized = NO;
    _hasPermission = NO;
    _manager = manager;
  }
  return self;
}

- (void)locationManager:(CLLocationManager*)manager
    didChangeAuthorizationStatus:(CLAuthorizationStatus)status {
  if (status == kCLAuthorizationStatusNotDetermined) {
    _permissionInitialized = NO;
    return;
  }
  _permissionInitialized = YES;
  if (status == kCLAuthorizationStatusAuthorizedAlways) {
    _hasPermission = YES;
  } else {
    _hasPermission = NO;
  }

#if BUILDFLAG(IS_IOS)
  if (status == kCLAuthorizationStatusAuthorizedWhenInUse) {
    _hasPermission = YES;
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
  position.timestamp = base::Time::FromSecondsSinceUnixEpoch(
      location.timestamp.timeIntervalSince1970);
  position.altitude = location.altitude;
  position.accuracy = location.horizontalAccuracy;
  position.altitude_accuracy = location.verticalAccuracy;
  position.speed = location.speed;
  position.heading = location.course;

  _manager->PositionUpdated(position);
}

- (void)locationManager:(CLLocationManager*)manager
       didFailWithError:(NSError*)error {
  base::UmaHistogramSparse("Geolocation.CoreLocationProvider.ErrorCode",
                           static_cast<int>(error.code));
  device::mojom::GeopositionError position_error;

  switch (error.code) {
    case kCLErrorDenied:
      position_error.error_code =
          device::mojom::GeopositionErrorCode::kPermissionDenied;
      position_error.error_message =
          device::mojom::kGeoPermissionDeniedErrorMessage;
      position_error.error_technical =
          "CoreLocationProvider: CoreLocation framework reported a "
          "kCLErrorDenied failure.";
      break;
    case kCLErrorPromptDeclined:
      position_error.error_code =
          device::mojom::GeopositionErrorCode::kPermissionDenied;
      position_error.error_message =
          device::mojom::kGeoPermissionDeniedErrorMessage;
      position_error.error_technical =
          "CoreLocationProvider: CoreLocation framework reported a "
          "kCLErrorPromptDeclined failure.";
      break;
    case kCLErrorLocationUnknown:
      position_error.error_code =
          device::mojom::GeopositionErrorCode::kPositionUnavailable;
      position_error.error_message =
          device::mojom::kGeoPositionUnavailableErrorMessage;
      position_error.error_technical =
          "CoreLocationProvider: CoreLocation framework reported a "
          "kCLErrorLocationUnknown failure.";
      break;
    case kCLErrorNetwork:
      position_error.error_code =
          device::mojom::GeopositionErrorCode::kPositionUnavailable;
      position_error.error_message =
          device::mojom::kGeoPositionUnavailableErrorMessage;
      position_error.error_technical =
          "CoreLocationProvider: CoreLocation framework reported a "
          "kCLErrorNetwork failure.";
      break;
    default:
      // For non-critical errors (heading, ranging, or region monitoring),
      // return immediately without setting the position_error, as they may not
      // be relevant to the caller.
      return;
  }

  _manager->PositionError(position_error);
}

@end
