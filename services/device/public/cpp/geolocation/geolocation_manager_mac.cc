// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/geolocation/geolocation_manager.h"

namespace device {

void GeolocationManager::NotifyPositionObservers(
    const device::mojom::Geoposition& position) {
  last_position_ = position;
  position_observers_->Notify(FROM_HERE, &PositionObserver::OnPositionUpdated,
                              position);
}

void GeolocationManager::StartWatchingPosition(bool high_accuracy) {
  system_geolocation_source_->StartWatchingPosition(high_accuracy);
}

void GeolocationManager::StopWatchingPosition() {
  system_geolocation_source_->StopWatchingPosition();
}

device::mojom::Geoposition GeolocationManager::GetLastPosition() const {
  return last_position_;
}

scoped_refptr<GeolocationManager::PositionObserverList>
GeolocationManager::GetPositionObserverList() const {
  return position_observers_;
}

}  // namespace device
