// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/geolocation/location_manager_delegate.h"

#include "base/metrics/histogram_functions.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/public/cpp/geolocation/system_geolocation_source_apple.h"

@implementation LocationManagerDelegate

- (instancetype)initWithManager:
    (base::WeakPtr<device::SystemGeolocationSourceApple>)manager {
  if ((self = [super init])) {
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

  // Records the accuracy value (in meters) of a valid Geoposition.
  // Values above 10000 meters are considered very inaccurate and are
  // categorized into the overflow bucket. This cap prioritizes accuracy
  // resolution in the lower range.
  base::UmaHistogramCounts10000("Geolocation.CoreLocationProvider.Accuracy",
                                static_cast<int>(position.accuracy));
  _manager->PositionUpdated(position);
}

- (void)locationManager:(CLLocationManager*)manager
       didFailWithError:(NSError*)error {
  base::UmaHistogramSparse("Geolocation.CoreLocationProvider.ErrorCode",
                           static_cast<int>(error.code));
  GEOLOCATION_LOG(ERROR)
      << "CLLocationManager::didFailWithError invoked with error code: "
      << static_cast<int>(error.code);

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
    case kCLErrorLocationUnknown: {
      if (!_manager->WasWifiEnabled()) {
        // If Wi-Fi was already disabled when `StartWatchingPosition` was
        // called, we can immediately trigger the fallback mechanism by
        // reporting a `kWifiDisabled` error.
        position_error.error_code =
            device::mojom::GeopositionErrorCode::kWifiDisabled;
      } else if (!_manager->IsWifiEnabled()) {
        // If Wi-Fi was enabled but is now disabled when
        // `kCLErrorLocationUnknown` is reported, initiate the fallback
        // mechanism after the network changed event fires. This ensures the
        // initial network request doesn't fail due to an unsettled network
        // state.
        _manager->StartNetworkChangedTimer();
        return;
      } else {
        // In all other cases, let the `kPositionUnavailable` error propagate.
        position_error.error_code =
            device::mojom::GeopositionErrorCode::kPositionUnavailable;
      }
      position_error.error_message =
          device::mojom::kGeoPositionUnavailableErrorMessage;
      position_error.error_technical =
          "CoreLocationProvider: CoreLocation framework reported a "
          "kCLErrorLocationUnknown failure.";
      break;
    }
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
