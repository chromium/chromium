// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <CoreLocation/CoreLocation.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/weak_ptr.h"
#include "services/device/geolocation/mac_location_permission_delegate.h"

@interface LocationPermissionDelegate : NSObject <CLLocationManagerDelegate> {
  base::WeakPtr<device::MacLocationPermissionDelegate> permission_delegate_;
}

- (id)initWithPermissionDelegate:
    (base::WeakPtr<device::MacLocationPermissionDelegate>)provider;

- (void)locationManager:(CLLocationManager*)manager
    didChangeAuthorizationStatus:(CLAuthorizationStatus)status;

@end

@implementation LocationPermissionDelegate

- (id)initWithPermissionDelegate:
    (base::WeakPtr<device::MacLocationPermissionDelegate>)permission_delegate {
  self = [super init];
  permission_delegate_ = permission_delegate;
  return self;
}

- (void)locationManager:(CLLocationManager*)manager
    didChangeAuthorizationStatus:(CLAuthorizationStatus)status {
  if (!permission_delegate_)
    return;
  if (@available(macOS 10.12.0, *)) {
    if (status == kCLAuthorizationStatusAuthorizedAlways) {
      permission_delegate_->SetPermission(true);
    } else {
      permission_delegate_->SetPermission(false);
    }
  } else {
    if (status == kCLAuthorizationStatusAuthorized) {
      permission_delegate_->SetPermission(true);
    } else {
      permission_delegate_->SetPermission(false);
    }
  }
}

@end

namespace device {

class LocationManagerHolder {
 public:
  LocationManagerHolder(base::WeakPtr<device::MacLocationPermissionDelegate>
                            location_permission) {
    location_manager_.reset([[CLLocationManager alloc] init]);
    delegate_.reset([[LocationPermissionDelegate alloc]
        initWithPermissionDelegate:location_permission]);
    location_manager_.get().delegate = delegate_;
  }
  ~LocationManagerHolder() = default;

 private:
  base::scoped_nsobject<CLLocationManager> location_manager_;
  base::scoped_nsobject<LocationPermissionDelegate> delegate_;
  base::WeakPtrFactory<LocationManagerHolder> weak_ptr_factory_{this};
};

MacLocationPermissionDelegate::MacLocationPermissionDelegate() {
  permission_delegate_ =
      std::make_unique<LocationManagerHolder>(weak_ptr_factory_.GetWeakPtr());
}

MacLocationPermissionDelegate::~MacLocationPermissionDelegate() {}

bool MacLocationPermissionDelegate::IsLocationPermissionGranted() {
  return has_permission_;
}

void MacLocationPermissionDelegate::SetPermissionUpdateCallback(
    PermissionUpdateCallback callback) {
  callback_ = callback;
}

void MacLocationPermissionDelegate::SetPermission(bool allowed) {
  has_permission_ = allowed;
  callback_.Run(allowed);
}

}  // namespace device
