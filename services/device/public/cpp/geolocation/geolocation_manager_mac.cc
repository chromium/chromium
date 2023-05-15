// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/geolocation/geolocation_manager.h"

namespace device {

void GeolocationManager::NotifyPositionObservers(
    device::mojom::GeopositionResultPtr result) {
  last_result_ = std::move(result);
  if (last_result_->is_error()) {
    position_observers_->Notify(FROM_HERE, &PositionObserver::OnPositionError,
                                *last_result_->get_error());
    return;
  }
  position_observers_->Notify(FROM_HERE, &PositionObserver::OnPositionUpdated,
                              *last_result_->get_position());
}

void GeolocationManager::StartWatchingPosition(bool high_accuracy) {
  system_geolocation_source_->StartWatchingPosition(high_accuracy);
}

void GeolocationManager::StopWatchingPosition() {
  system_geolocation_source_->StopWatchingPosition();
}

const device::mojom::GeopositionResult* GeolocationManager::GetLastPosition()
    const {
  return last_result_.get();
}

scoped_refptr<GeolocationManager::PositionObserverList>
GeolocationManager::GetPositionObserverList() const {
  return position_observers_;
}

}  // namespace device
