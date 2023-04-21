// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "build/build_config.h"
#include "fake_geolocation_manager.h"
#include "services/device/public/cpp/geolocation/system_geolocation_source.h"
#include "services/device/public/cpp/test/fake_geolocation_manager.h"

namespace device {

namespace {
class FakeGeolocationSource : public SystemGeolocationSource {
 public:
  void RegisterPermissionUpdateCallback(
      PermissionUpdateCallback callback) override {
    permission_callback_ = callback;
  }
  void SetSystemPermission(LocationSystemPermissionStatus status) {
    status_ = status;
    permission_callback_.Run(status_);
  }

#if BUILDFLAG(IS_MAC)
  void RegisterPositionUpdateCallback(
      PositionUpdateCallback callback) override {
    position_callback_ = callback;
  }
  void StartWatchingPosition(bool high_accuracy) override {
    watching_position_ = true;
  }
  void StopWatchingPosition() override { watching_position_ = false; }
  bool watching_position() { return watching_position_; }

  void FakePositionUpdated(mojom::GeopositionResultPtr result) {
    position_callback_.Run(std::move(result));
  }
#endif  // BUILDFLAG(IS_MAC)

 private:
  LocationSystemPermissionStatus status_ =
      LocationSystemPermissionStatus::kDenied;
  PermissionUpdateCallback permission_callback_;

#if BUILDFLAG(IS_MAC)
  bool watching_position_ = false;
  PositionUpdateCallback position_callback_;
#endif  // BUILDFLAG(IS_MAC)
};
}  // namespace

FakeGeolocationManager::FakeGeolocationManager()
    : GeolocationManager(std::make_unique<FakeGeolocationSource>()) {}

void FakeGeolocationManager::SetSystemPermission(
    LocationSystemPermissionStatus status) {
  return static_cast<FakeGeolocationSource&>(SystemGeolocationSourceForTest())
      .SetSystemPermission(status);
}

#if BUILDFLAG(IS_MAC)
bool FakeGeolocationManager::watching_position() {
  return static_cast<FakeGeolocationSource&>(SystemGeolocationSourceForTest())
      .watching_position();
}

void FakeGeolocationManager::FakePositionUpdated(
    mojom::GeopositionResultPtr position) {
  return static_cast<FakeGeolocationSource&>(SystemGeolocationSourceForTest())
      .FakePositionUpdated(std::move(position));
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace device
