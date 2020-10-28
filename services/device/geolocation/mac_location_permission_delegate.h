// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_MAC_LOCATION_PERMISSION_DELEGATE_H_
#define SERVICES_DEVICE_GEOLOCATION_MAC_LOCATION_PERMISSION_DELEGATE_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"

namespace device {

class LocationManagerHolder;

class MacLocationPermissionDelegate {
  using PermissionUpdateCallback = base::RepeatingCallback<void(bool)>;

 public:
  MacLocationPermissionDelegate();
  ~MacLocationPermissionDelegate();

  bool IsLocationPermissionGranted();
  void SetPermissionUpdateCallback(PermissionUpdateCallback callback);

  // Only to be called by the CLLocationDelegate.
  void SetPermission(bool allowed);

 private:
  std::unique_ptr<LocationManagerHolder> permission_delegate_;
  PermissionUpdateCallback callback_;
  bool has_permission_ = false;
  base::WeakPtrFactory<MacLocationPermissionDelegate> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_MAC_LOCATION_PERMISSION_DELEGATE_H_
