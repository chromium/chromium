// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_GEOLOCATION_SYSTEM_PERMISSION_MANAGER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_GEOLOCATION_SYSTEM_PERMISSION_MANAGER_H_

#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"

namespace device {

class FakeGeolocationSystemPermissionManager
    : public GeolocationSystemPermissionManager {
 public:
  FakeGeolocationSystemPermissionManager();
  ~FakeGeolocationSystemPermissionManager() override = default;

  void SetSystemPermission(LocationSystemPermissionStatus status);
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_GEOLOCATION_SYSTEM_PERMISSION_MANAGER_H_
