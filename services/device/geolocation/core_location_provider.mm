// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/core_location_provider.h"
#include "services/device/public/cpp/device_features.h"

@interface LocationDelegate : NSObject <CLLocationManagerDelegate> {
  base::WeakPtr<device::CoreLocationProvider> provider_;
}

- (id)initWithProvider:(base::WeakPtr<device::CoreLocationProvider>)provider;

- (void)locationManager:(CLLocationManager*)manager
     didUpdateLocations:(NSArray*)locations;
- (void)locationManager:(CLLocationManager*)manager
    didChangeAuthorizationStatus:(CLAuthorizationStatus)status;

@end

@implementation LocationDelegate

- (id)initWithProvider:(base::WeakPtr<device::CoreLocationProvider>)provider {
  self = [super init];
  provider_ = provider;
  return self;
}

- (void)locationManager:(CLLocationManager*)manager
    didChangeAuthorizationStatus:(CLAuthorizationStatus)status {
  if (!provider_)
    return;
  if (@available(macOS 10.12.0, *)) {
    if (status == kCLAuthorizationStatusAuthorizedAlways) {
      provider_->SystemLocationPermissionGranted();
    } else {
      provider_->SystemLocationPermissionDenied();
    }
  } else {
    if (status == kCLAuthorizationStatusAuthorized) {
      provider_->SystemLocationPermissionGranted();
    } else {
      provider_->SystemLocationPermissionDenied();
    }
  }
}

- (void)locationManager:(CLLocationManager*)manager
     didUpdateLocations:(NSArray*)locations {
  if (provider_)
    provider_->DidUpdatePosition([locations lastObject]);
}

@end

namespace device {

CoreLocationProvider::CoreLocationProvider() {
  location_manager_.reset([[CLLocationManager alloc] init]);
  delegate_.reset([[LocationDelegate alloc]
      initWithProvider:weak_ptr_factory_.GetWeakPtr()]);
  location_manager_.get().delegate = delegate_;
}

CoreLocationProvider::~CoreLocationProvider() {
  StopProvider();
}

void CoreLocationProvider::SetUpdateCallback(
    const LocationProviderUpdateCallback& callback) {
  callback_ = callback;
}

void CoreLocationProvider::StartProvider(bool high_accuracy) {
  if (high_accuracy) {
    location_manager_.get().desiredAccuracy = kCLLocationAccuracyBest;
  } else {
    // Using kCLLocationAccuracyHundredMeters for consistency with Android.
    location_manager_.get().desiredAccuracy = kCLLocationAccuracyHundredMeters;
  }

  // macOS guarantees that didChangeAuthorization will be called at least once
  // with the initial authorization status. Therefore this variable will be
  // updated regardless of whether that authorization status has recently
  // changed.
  if (has_permission_) {
    [location_manager_ startUpdatingLocation];
  } else {
    provider_start_attemped_ = true;
  }
}

void CoreLocationProvider::StopProvider() {
  [location_manager_ stopUpdatingLocation];
}

const mojom::Geoposition& CoreLocationProvider::GetPosition() {
  return last_position_;
}

void CoreLocationProvider::OnPermissionGranted() {
  // Nothing to do here.
}

void CoreLocationProvider::SystemLocationPermissionGranted() {
  has_permission_ = true;
  if (provider_start_attemped_) {
    [location_manager_ startUpdatingLocation];
    provider_start_attemped_ = false;
  }
}

void CoreLocationProvider::SystemLocationPermissionDenied() {
  has_permission_ = false;
}

void CoreLocationProvider::DidUpdatePosition(CLLocation* location) {
  // The error values in CLLocation correlate exactly to our error values.
  last_position_.latitude = location.coordinate.latitude;
  last_position_.longitude = location.coordinate.longitude;
  last_position_.timestamp =
      base::Time::FromDoubleT(location.timestamp.timeIntervalSince1970);
  last_position_.altitude = location.altitude;
  last_position_.accuracy = location.horizontalAccuracy;
  last_position_.altitude_accuracy = location.verticalAccuracy;
  last_position_.speed = location.speed;
  last_position_.heading = location.course;

  callback_.Run(this, last_position_);
}

void CoreLocationProvider::SetManagerForTesting(
    CLLocationManager* location_manager) {
  location_manager_.reset(location_manager);
  location_manager_.get().delegate = delegate_;
}

// static
std::unique_ptr<LocationProvider> NewSystemLocationProvider() {
  if (!base::FeatureList::IsEnabled(features::kMacCoreLocationBackend))
    return nullptr;

  return std::make_unique<CoreLocationProvider>();
}

}  // namespace device
