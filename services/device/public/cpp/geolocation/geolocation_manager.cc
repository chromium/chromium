// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/geolocation/geolocation_manager.h"

#include "base/check_op.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"

namespace device {

GeolocationManager::GeolocationManager(
    std::unique_ptr<SystemGeolocationSource> system_geolocation_source)
    : system_geolocation_source_(std::move(system_geolocation_source)),
      observers_(base::MakeRefCounted<PermissionObserverList>())
#if BUILDFLAG(IS_APPLE)
      ,
      position_observers_(base::MakeRefCounted<PositionObserverList>())
#endif
{
  DCHECK(system_geolocation_source_);
  system_geolocation_source_->RegisterPermissionUpdateCallback(
      base::BindRepeating(&GeolocationManager::UpdateSystemPermission,
                          weak_factory_.GetWeakPtr()));
#if BUILDFLAG(IS_APPLE)
  system_geolocation_source_->RegisterPositionUpdateCallback(
      base::BindRepeating(&GeolocationManager::NotifyPositionObservers,
                          weak_factory_.GetWeakPtr()));
#endif
}

GeolocationManager::~GeolocationManager() = default;

void GeolocationManager::AddObserver(PermissionObserver* observer) {
  observers_->AddObserver(observer);
}

void GeolocationManager::RemoveObserver(PermissionObserver* observer) {
  observers_->RemoveObserver(observer);
}

LocationSystemPermissionStatus GeolocationManager::GetSystemPermission() const {
  return permission_cache_;
}

void GeolocationManager::UpdateSystemPermission(
    LocationSystemPermissionStatus status) {
  permission_cache_ = status;
  NotifyPermissionObservers();
}

void GeolocationManager::NotifyPermissionObservers() {
  observers_->Notify(FROM_HERE, &PermissionObserver::OnSystemPermissionUpdated,
                     GetSystemPermission());
}

scoped_refptr<GeolocationManager::PermissionObserverList>
GeolocationManager::GetObserverList() const {
  return observers_;
}

void GeolocationManager::AppAttemptsToUseGeolocation() {
  system_geolocation_source_->AppAttemptsToUseGeolocation();
}

void GeolocationManager::AppCeasesToUseGeolocation() {
  system_geolocation_source_->AppCeasesToUseGeolocation();
}

SystemGeolocationSource& GeolocationManager::SystemGeolocationSourceForTest() {
  return *system_geolocation_source_;
}

}  // namespace device
