// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/location_provider_android.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "services/device/geolocation/location_api_adapter_android.h"

namespace device {

class GeolocationManager;

LocationProviderAndroid::LocationProviderAndroid() = default;

LocationProviderAndroid::~LocationProviderAndroid() {
  DCHECK(thread_checker_.CalledOnValidThread());
  StopProvider();
}

void LocationProviderAndroid::NotifyNewGeoposition(
    const mojom::Geoposition& position) {
  DCHECK(thread_checker_.CalledOnValidThread());
  last_position_ = position;
  if (!callback_.is_null())
    callback_.Run(this, position);
}

void LocationProviderAndroid::SetUpdateCallback(
    const LocationProviderUpdateCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  callback_ = callback;
}

void LocationProviderAndroid::StartProvider(bool high_accuracy) {
  DCHECK(thread_checker_.CalledOnValidThread());
  LocationApiAdapterAndroid::GetInstance()->Start(
      base::BindRepeating(&LocationProviderAndroid::NotifyNewGeoposition,
                          weak_ptr_factory_.GetWeakPtr()),
      high_accuracy);
}

void LocationProviderAndroid::StopProvider() {
  DCHECK(thread_checker_.CalledOnValidThread());
  LocationApiAdapterAndroid::GetInstance()->Stop();
}

const mojom::Geoposition& LocationProviderAndroid::GetPosition() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return last_position_;
}

void LocationProviderAndroid::OnPermissionGranted() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Nothing to do here.
}

std::unique_ptr<LocationProvider> NewSystemLocationProvider(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    GeolocationManager* geolocation_manager) {
  return std::make_unique<LocationProviderAndroid>();
}

}  // namespace device
