// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "services/device/public/cpp/device_features.h"

namespace device {

namespace {
// This checks that all accesses to the global manager are done in sequence
class CheckedAccessWrapper {
 public:
  static CheckedAccessWrapper& GetInstance() {
    static base::NoDestructor<CheckedAccessWrapper> wrapper;
    return *wrapper;
  }

  void SetManager(std::unique_ptr<GeolocationSystemPermissionManager> manager) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
    manager_ = std::move(manager);
  }

  GeolocationSystemPermissionManager* GetManager() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return manager_.get();
  }

 private:
  std::unique_ptr<GeolocationSystemPermissionManager> manager_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

GeolocationSystemPermissionManager*
GeolocationSystemPermissionManager::GetInstance() {
  return CheckedAccessWrapper::GetInstance().GetManager();
}

void GeolocationSystemPermissionManager::SetInstance(
    std::unique_ptr<GeolocationSystemPermissionManager> manager) {
  CheckedAccessWrapper::GetInstance().SetManager(std::move(manager));
}

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
GeolocationSystemPermissionManager::GeolocationSystemPermissionManager(
    std::unique_ptr<SystemGeolocationSource> system_geolocation_source)
    : system_geolocation_source_(std::move(system_geolocation_source)) {
  DCHECK(system_geolocation_source_);
  system_geolocation_source_->RegisterPermissionUpdateCallback(
      base::BindRepeating(
          &GeolocationSystemPermissionManager::UpdateSystemPermission,
          weak_factory_.GetWeakPtr()));
}

GeolocationSystemPermissionManager::~GeolocationSystemPermissionManager() =
    default;

void GeolocationSystemPermissionManager::AddObserver(
    PermissionObserver* observer) {
  observers_.AddObserver(observer);
}

void GeolocationSystemPermissionManager::RemoveObserver(
    PermissionObserver* observer) {
  observers_.RemoveObserver(observer);
}

void GeolocationSystemPermissionManager::UpdateSystemPermission(
    LocationSystemPermissionStatus status) {
  permission_cache_ = status;
  NotifyPermissionObservers();
}

void GeolocationSystemPermissionManager::NotifyPermissionObservers() {
  for (auto& observer : observers_) {
    observer.OnSystemPermissionUpdated(GetSystemPermission());
  }
}

SystemGeolocationSource&
GeolocationSystemPermissionManager::SystemGeolocationSourceForTest() {
  return *system_geolocation_source_;
}

#endif

LocationSystemPermissionStatus
GeolocationSystemPermissionManager::GetSystemPermission() const {
  CHECK(features::IsOsLevelGeolocationPermissionSupportEnabled());
  return permission_cache_;
}

void GeolocationSystemPermissionManager::RequestSystemPermission() {
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
  system_geolocation_source_->RequestPermission();
#endif
}

void GeolocationSystemPermissionManager::OpenSystemPermissionSetting() {
#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  system_geolocation_source_->OpenSystemPermissionSetting();
#endif
}

void GeolocationSystemPermissionManager::Shutdown() {
#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  for (auto& observer : observers_) {
    observer.OnPermissionManagerShuttingDown();
  }
#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
}

}  // namespace device
