// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/fake_geolocation_manager.h"
#include "build/build_config.h"

namespace device {

LocationSystemPermissionStatus FakeGeolocationManager::GetSystemPermission()
    const {
  return status_;
}

void FakeGeolocationManager::SetSystemPermission(
    LocationSystemPermissionStatus status) {
  status_ = status;
  NotifyPermissionObservers(status_);
}

void FakeGeolocationManager::FakePositionUpdated(
    const mojom::Geoposition& position) {
  NotifyPositionObservers(position);
}

void FakeGeolocationManager::StartWatchingPosition(bool high_accuracy) {
  watching_position_ = true;
}

void FakeGeolocationManager::StopWatchingPosition() {
  watching_position_ = false;
}

}  // namespace device
