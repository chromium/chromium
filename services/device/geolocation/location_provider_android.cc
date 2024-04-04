// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/location_provider_android.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "services/device/geolocation/location_api_adapter_android.h"

namespace device {

LocationProviderAndroid::LocationProviderAndroid() = default;

LocationProviderAndroid::~LocationProviderAndroid() {
  DCHECK(thread_checker_.CalledOnValidThread());
  StopProvider();
}

void LocationProviderAndroid::NotifyNewGeoposition(
    mojom::GeopositionResultPtr result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  last_result_ = std::move(result);
  if (!callback_.is_null())
    callback_.Run(this, last_result_.Clone());
}

void LocationProviderAndroid::FillDiagnostics(
    mojom::GeolocationDiagnostics& diagnostics) {
  diagnostics.provider_state = state_;
}

void LocationProviderAndroid::SetUpdateCallback(
    const LocationProviderUpdateCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  callback_ = callback;
}

void LocationProviderAndroid::StartProvider(bool high_accuracy) {
  DCHECK(thread_checker_.CalledOnValidThread());
  state_ = high_accuracy
               ? mojom::GeolocationDiagnostics::ProviderState::kHighAccuracy
               : mojom::GeolocationDiagnostics::ProviderState::kLowAccuracy;
  LocationApiAdapterAndroid::GetInstance()->Start(
      base::BindRepeating(&LocationProviderAndroid::NotifyNewGeoposition,
                          weak_ptr_factory_.GetWeakPtr()),
      high_accuracy);
}

void LocationProviderAndroid::StopProvider() {
  DCHECK(thread_checker_.CalledOnValidThread());
  state_ = mojom::GeolocationDiagnostics::ProviderState::kStopped;
  LocationApiAdapterAndroid::GetInstance()->Stop();
}

const mojom::GeopositionResult* LocationProviderAndroid::GetPosition() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return last_result_.get();
}

void LocationProviderAndroid::OnPermissionGranted() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Nothing to do here.
}

std::unique_ptr<LocationProvider> NewSystemLocationProvider() {
  return std::make_unique<LocationProviderAndroid>();
}

}  // namespace device
