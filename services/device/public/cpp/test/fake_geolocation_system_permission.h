// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_GEOLOCATION_SYSTEM_PERMISSION_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_GEOLOCATION_SYSTEM_PERMISSION_H_

#include "services/device/public/cpp/geolocation/geolocation_system_permission_mac.h"

class FakeSystemGeolocationPermissionsManager
    : public device::GeolocationSystemPermissionManager {
 public:
  FakeSystemGeolocationPermissionsManager() = default;

  ~FakeSystemGeolocationPermissionsManager() override = default;

  device::LocationSystemPermissionStatus GetSystemPermission() override;

  void set_status(device::LocationSystemPermissionStatus status);

 private:
  device::LocationSystemPermissionStatus status_ =
      device::LocationSystemPermissionStatus::kDenied;
};

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_GEOLOCATION_SYSTEM_PERMISSION_H_