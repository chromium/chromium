// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/location_provider_android.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "services/device/geolocation/location_api_adapter_android.h"

namespace device {

// LocationProviderAndroid
LocationProviderAndroid::LocationProviderAndroid() {}

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

// static
std::unique_ptr<LocationProvider> NewSystemLocationProvider() {
  return base::WrapUnique(new LocationProviderAndroid);
}

}  // namespace device
