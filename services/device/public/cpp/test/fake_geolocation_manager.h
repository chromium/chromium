// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_GEOLOCATION_MANAGER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_GEOLOCATION_MANAGER_H_

#include "services/device/public/cpp/geolocation/geolocation_manager.h"

namespace device {

class FakeGeolocationManager : public GeolocationManager {
 public:
  FakeGeolocationManager() = default;
  ~FakeGeolocationManager() override = default;

  // GeolocationManager implementation.
  LocationSystemPermissionStatus GetSystemPermission() const override;
  void StartWatchingPosition(bool high_accuracy) override;
  void StopWatchingPosition() override;

  void SetSystemPermission(LocationSystemPermissionStatus status);
  bool watching_position() { return watching_position_; }
  void FakePositionUpdated(const device::mojom::Geoposition& position);

 private:
  LocationSystemPermissionStatus status_ =
      LocationSystemPermissionStatus::kDenied;
  bool watching_position_ = false;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_GEOLOCATION_MANAGER_H_
