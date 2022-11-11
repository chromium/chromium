// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/geolocation/geolocation_manager.h"

namespace device {

#if BUILDFLAG(IS_MAC)
GeolocationManager::GeolocationManager()
    : observers_(base::MakeRefCounted<PermissionObserverList>()),
      position_observers_(base::MakeRefCounted<PositionObserverList>()) {}

GeolocationManager::~GeolocationManager() = default;

void GeolocationManager::AddObserver(PermissionObserver* observer) {
  observers_->AddObserver(observer);
}

void GeolocationManager::RemoveObserver(PermissionObserver* observer) {
  observers_->RemoveObserver(observer);
}

void GeolocationManager::NotifyPermissionObservers(
    LocationSystemPermissionStatus status) {
  observers_->Notify(FROM_HERE, &PermissionObserver::OnSystemPermissionUpdated,
                     status);
}

void GeolocationManager::NotifyPositionObservers(
    const device::mojom::Geoposition& position) {
  last_position_ = position;
  position_observers_->Notify(FROM_HERE, &PositionObserver::OnPositionUpdated,
                              position);
}

device::mojom::Geoposition GeolocationManager::GetLastPosition() const {
  return last_position_;
}

scoped_refptr<GeolocationManager::PositionObserverList>
GeolocationManager::GetPositionObserverList() const {
  return position_observers_;
}

scoped_refptr<GeolocationManager::PermissionObserverList>
GeolocationManager::GetObserverList() const {
  return observers_;
}
#endif

}  // namespace device
