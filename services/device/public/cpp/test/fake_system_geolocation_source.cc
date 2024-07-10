// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/fake_system_geolocation_source.h"

namespace device {

FakeSystemGeolocationSource::FakeSystemGeolocationSource() = default;
FakeSystemGeolocationSource::~FakeSystemGeolocationSource() = default;

void FakeSystemGeolocationSource::RegisterPermissionUpdateCallback(
    PermissionUpdateCallback callback) {
  permission_callback_ = callback;
}
void FakeSystemGeolocationSource::SetSystemPermission(
    LocationSystemPermissionStatus status) {
  status_ = status;
  permission_callback_.Run(status_);
}

#if BUILDFLAG(IS_APPLE)
void FakeSystemGeolocationSource::StartWatchingPosition(bool high_accuracy) {
  watching_position_ = true;
}
void FakeSystemGeolocationSource::StopWatchingPosition() {
  watching_position_ = false;
}

void FakeSystemGeolocationSource::AddPositionUpdateObserver(
    PositionObserver* observer) {
  position_observers_->AddObserver(observer);
}
void FakeSystemGeolocationSource::RemovePositionUpdateObserver(
    PositionObserver* observer) {
  position_observers_->RemoveObserver(observer);
}

void FakeSystemGeolocationSource::FakePositionUpdatedForTesting(
    const mojom::Geoposition& position) {
  position_observers_->Notify(FROM_HERE, &PositionObserver::OnPositionUpdated,
                              position);
}

void FakeSystemGeolocationSource::FakePositionErrorForTesting(
    const mojom::GeopositionError& error) {
  position_observers_->Notify(FROM_HERE, &PositionObserver::OnPositionError,
                              error);
}
#endif  // BUILDFLAG(IS_APPLE)

}  // namespace device
