// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Security/Security.h>

#include "services/device/geolocation/core_location_provider.h"

#include "base/apple/scoped_cftyperef.h"
#include "base/task/single_thread_task_runner.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"

namespace device {

CoreLocationProvider::CoreLocationProvider(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    GeolocationSystemPermissionManager* geolocation_system_permission_manager)
    : geolocation_system_permission_manager_(
          geolocation_system_permission_manager),
      permission_observers_(
          geolocation_system_permission_manager->GetObserverList()),
      system_geolocation_source_(
          geolocation_system_permission_manager->GetSystemGeolocationSource()) {
  permission_observers_->AddObserver(this);
  main_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GeolocationSystemPermissionManager::GetSystemPermission,
                     base::Unretained(geolocation_system_permission_manager_)),
      base::BindOnce(&CoreLocationProvider::OnSystemPermissionUpdated,
                     weak_ptr_factory_.GetWeakPtr()));
}

CoreLocationProvider::~CoreLocationProvider() {
  permission_observers_->RemoveObserver(this);
  StopProvider();
}

void CoreLocationProvider::FillDiagnostics(
    mojom::GeolocationDiagnostics& diagnostics) {
  if (!is_started_) {
    diagnostics.provider_state =
        mojom::GeolocationDiagnostics::ProviderState::kStopped;
  } else if (!has_permission_) {
    diagnostics.provider_state = mojom::GeolocationDiagnostics::ProviderState::
        kBlockedBySystemPermission;
  } else if (high_accuracy_) {
    diagnostics.provider_state =
        mojom::GeolocationDiagnostics::ProviderState::kHighAccuracy;
  } else {
    diagnostics.provider_state =
        mojom::GeolocationDiagnostics::ProviderState::kLowAccuracy;
  }
}

void CoreLocationProvider::SetUpdateCallback(
    const LocationProviderUpdateCallback& callback) {
  callback_ = callback;
}

void CoreLocationProvider::StartProvider(bool high_accuracy) {
  is_started_ = true;
  high_accuracy_ = high_accuracy;
  // macOS guarantees that didChangeAuthorization will be called at least once
  // with the initial authorization status. Therefore this variable will be
  // updated regardless of whether that authorization status has recently
  // changed.
  if (has_permission_) {
    StartWatching();
  } else {
    provider_start_attemped_ = true;
  }
}

void CoreLocationProvider::StartWatching() {
  system_geolocation_source_->AddPositionUpdateObserver(this);
  system_geolocation_source_->StartWatchingPosition(high_accuracy_);
}

void CoreLocationProvider::StopProvider() {
  is_started_ = false;
  system_geolocation_source_->RemovePositionUpdateObserver(this);
  system_geolocation_source_->StopWatchingPosition();
}

const mojom::GeopositionResult* CoreLocationProvider::GetPosition() {
  return last_result_.get();
}

void CoreLocationProvider::OnPermissionGranted() {
  // Nothing to do here.
}

void CoreLocationProvider::OnSystemPermissionUpdated(
    LocationSystemPermissionStatus new_status) {
  has_permission_ = new_status == LocationSystemPermissionStatus::kAllowed;
  if (provider_start_attemped_ && has_permission_) {
    StartWatching();
    provider_start_attemped_ = false;
  }
}

void CoreLocationProvider::OnPositionUpdated(
    const mojom::Geoposition& location) {
  last_result_ = mojom::GeopositionResult::NewPosition(location.Clone());
  callback_.Run(this, last_result_.Clone());
}

void CoreLocationProvider::OnPositionError(
    const mojom::GeopositionError& error) {
  last_result_ = mojom::GeopositionResult::NewError(error.Clone());
  callback_.Run(this, last_result_.Clone());
}

std::unique_ptr<LocationProvider> NewSystemLocationProvider(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    GeolocationSystemPermissionManager* geolocation_system_permission_manager) {
  if (!base::FeatureList::IsEnabled(features::kMacCoreLocationBackend)) {
    return nullptr;
  }

  return std::make_unique<CoreLocationProvider>(
      std::move(main_task_runner), geolocation_system_permission_manager);
}

}  // namespace device
