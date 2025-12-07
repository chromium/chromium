// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_LOCATION_MANAGER_DELEGATE_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_LOCATION_MANAGER_DELEGATE_H_

#import <CoreLocation/CoreLocation.h>
#import <Foundation/Foundation.h>

#include "base/memory/weak_ptr.h"

namespace device {

class SystemGeolocationSourceApple;

}  // namespace device

@interface LocationManagerDelegate : NSObject <CLLocationManagerDelegate> {
  BOOL _permissionInitialized;
  BOOL _hasPermission;
  base::WeakPtr<device::SystemGeolocationSourceApple> _manager;
}

- (instancetype)initWithManager:
    (base::WeakPtr<device::SystemGeolocationSourceApple>)manager;

// CLLocationManagerDelegate.
- (void)locationManager:(CLLocationManager*)manager
     didUpdateLocations:(NSArray*)locations;
- (void)locationManager:(CLLocationManager*)manager
    didChangeAuthorizationStatus:(CLAuthorizationStatus)status;
- (void)locationManager:(CLLocationManager*)manager
       didFailWithError:(NSError*)error;

- (BOOL)hasPermission;
- (BOOL)permissionInitialized;
@end

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_LOCATION_MANAGER_DELEGATE_H_
