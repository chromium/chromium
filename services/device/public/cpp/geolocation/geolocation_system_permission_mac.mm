// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <CoreLocation/CoreLocation.h>
#import <memory>

#include "base/mac/scoped_nsobject.h"
#include "base/sequence_checker.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_mac.h"

class SystemGeolocationPermissionsManagerImpl;

@interface SystemGeolocationPermissionsDelegate
    : NSObject <CLLocationManagerDelegate> {
  bool _permissionReceived;
  bool _hasPermission;
  base::WeakPtr<SystemGeolocationPermissionsManagerImpl> _manager;
}

- (id)initWithManager:
    (base::WeakPtr<SystemGeolocationPermissionsManagerImpl>)manager;

// CLLocationManagerDelegate
- (void)locationManager:(CLLocationManager*)manager
    didChangeAuthorizationStatus:(CLAuthorizationStatus)status;
- (bool)hasPermission;
- (bool)permissionReceived;
@end

class SystemGeolocationPermissionsManagerImpl
    : public device::GeolocationSystemPermissionManager {
 public:
  SystemGeolocationPermissionsManagerImpl() {
    location_manager_.reset([[CLLocationManager alloc] init]);
    delegate_.reset([[SystemGeolocationPermissionsDelegate alloc]
        initWithManager:weak_ptr_factory_.GetWeakPtr()]);
    location_manager_.get().delegate = delegate_;
  }

  ~SystemGeolocationPermissionsManagerImpl() override = default;

  void PermissionUpdated() { NotifyObservers(GetSystemPermission()); }

  device::LocationSystemPermissionStatus GetSystemPermission() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (![delegate_ permissionReceived])
      return device::LocationSystemPermissionStatus::kNotDetermined;

    if ([delegate_ hasPermission])
      return device::LocationSystemPermissionStatus::kAllowed;

    return device::LocationSystemPermissionStatus::kDenied;
  }

 private:
  base::scoped_nsobject<SystemGeolocationPermissionsDelegate> delegate_;
  base::scoped_nsobject<CLLocationManager> location_manager_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<SystemGeolocationPermissionsManagerImpl>
      weak_ptr_factory_{this};
};

namespace device {

// static
std::unique_ptr<GeolocationSystemPermissionManager>
GeolocationSystemPermissionManager::Create() {
  return std::make_unique<SystemGeolocationPermissionsManagerImpl>();
}

GeolocationSystemPermissionManager::GeolocationSystemPermissionManager()
    : observers_(new ObserverList()) {}

GeolocationSystemPermissionManager::~GeolocationSystemPermissionManager() =
    default;

void GeolocationSystemPermissionManager::AddObserver(
    GeolocationPermissionObserver* observer) {
  observers_->AddObserver(observer);
}

void GeolocationSystemPermissionManager::RemoveObserver(
    GeolocationPermissionObserver* observer) {
  observers_->RemoveObserver(observer);
}

void GeolocationSystemPermissionManager::NotifyObservers(
    LocationSystemPermissionStatus status) {
  observers_->Notify(FROM_HERE,
                     &GeolocationPermissionObserver::OnSystemPermissionUpdate,
                     status);
}

scoped_refptr<GeolocationSystemPermissionManager::ObserverList>
GeolocationSystemPermissionManager::GetObserverList() {
  return observers_;
}

}  // device namespace

@implementation SystemGeolocationPermissionsDelegate

- (id)initWithManager:
    (base::WeakPtr<SystemGeolocationPermissionsManagerImpl>)Manager {
  if (self = [super init]) {
    _permissionReceived = false;
    _hasPermission = false;
    _manager = Manager;
  }
  return self;
}

- (void)locationManager:(CLLocationManager*)manager
    didChangeAuthorizationStatus:(CLAuthorizationStatus)status {
  _permissionReceived = true;
  if (@available(macOS 10.12.0, *)) {
    if (status == kCLAuthorizationStatusAuthorizedAlways)
      _hasPermission = true;
    else
      _hasPermission = false;
  } else {
    if (status == kCLAuthorizationStatusAuthorized)
      _hasPermission = true;
    else
      _hasPermission = false;
  }
  _manager->PermissionUpdated();
}

- (bool)hasPermission {
  return _hasPermission;
}

- (bool)permissionReceived {
  return _permissionReceived;
}

@end