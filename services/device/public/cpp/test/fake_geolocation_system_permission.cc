// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/fake_geolocation_system_permission.h"
#include "build/build_config.h"

device::LocationSystemPermissionStatus
FakeSystemGeolocationPermissionsManager::GetSystemPermission() {
  return status_;
}

void FakeSystemGeolocationPermissionsManager::set_status(
    device::LocationSystemPermissionStatus status) {
  status_ = status;
  NotifyObservers(status_);
}