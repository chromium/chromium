// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/core_location_provider.h"

#include "base/apple/scoped_cftyperef.h"
#include "base/task/single_thread_task_runner.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"
#include "services/device/public/mojom/geolocation_internals.mojom-shared.h"

namespace device {

CoreLocationProvider::CoreLocationProvider(
    SystemGeolocationSource& system_geolocation_source)
    : system_geolocation_source_(system_geolocation_source) {}

CoreLocationProvider::~CoreLocationProvider() {
  StopProvider();
}

void CoreLocationProvider::FillDiagnostics(
    mojom::GeolocationDiagnostics& diagnostics) {
  if (!is_started_) {
    diagnostics.provider_state =
        mojom::GeolocationDiagnostics::ProviderState::kStopped;
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
  if (!is_started_) {
    // Register for position updates (done only once).
    system_geolocation_source_->AddPositionUpdateObserver(this);
    is_started_ = true;
  }
  high_accuracy_ = high_accuracy;

  // Allows re-entry to start or adjust position tracking based on accuracy.
  system_geolocation_source_->StartWatchingPosition(high_accuracy_);
}

void CoreLocationProvider::StopProvider() {
  if (!is_started_) {
    return;
  }
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
    SystemGeolocationSource& system_geolocation_source) {
  return std::make_unique<CoreLocationProvider>(system_geolocation_source);
}

}  // namespace device
