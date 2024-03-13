// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/fake_geolocation_system_permission_manager.h"

#include <memory>

#include "build/build_config.h"
#include "services/device/public/cpp/geolocation/system_geolocation_source.h"
#include "services/device/public/cpp/test/fake_system_geolocation_source.h"

namespace device {

FakeGeolocationSystemPermissionManager::FakeGeolocationSystemPermissionManager()
    : GeolocationSystemPermissionManager(
          std::make_unique<FakeSystemGeolocationSource>()) {}

void FakeGeolocationSystemPermissionManager::SetSystemPermission(
    LocationSystemPermissionStatus status) {
  return static_cast<FakeSystemGeolocationSource&>(
             SystemGeolocationSourceForTest())
      .SetSystemPermission(status);
}

}  // namespace device
