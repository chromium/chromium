// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/geolocation/geolocation_manager.h"

#include "base/check_op.h"
#include "base/sequence_checker.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"

namespace device {

namespace {
// This checks that all accesses to the global manager are done in sequence
class CheckedAccessWrapper {
 public:
  static CheckedAccessWrapper& GetInstance() {
    static CheckedAccessWrapper wrapper;
    return wrapper;
  }

  void SetManager(std::unique_ptr<GeolocationManager> manager) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
    manager_ = std::move(manager);
  }

  GeolocationManager* GetManager() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return manager_.get();
  }

 private:
  std::unique_ptr<GeolocationManager> manager_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

GeolocationManager* GeolocationManager::GetInstance() {
  return CheckedAccessWrapper::GetInstance().GetManager();
}

void GeolocationManager::SetInstance(
    std::unique_ptr<GeolocationManager> manager) {
  CheckedAccessWrapper::GetInstance().SetManager(std::move(manager));
}

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
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

SystemGeolocationSource& GeolocationManager::SystemGeolocationSourceForTest() {
  return *system_geolocation_source_;
}

#endif

void GeolocationManager::TrackGeolocationAttempted() {
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  system_geolocation_source_->TrackGeolocationAttempted();
#endif
}

void GeolocationManager::TrackGeolocationRelinquished() {
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  system_geolocation_source_->TrackGeolocationRelinquished();
#endif
}

void GeolocationManager::RequestSystemPermission() {
#if BUILDFLAG(IS_APPLE)
  system_geolocation_source_->RequestPermission();
#endif
}

}  // namespace device
