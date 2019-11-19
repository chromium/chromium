// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/geolocation_config.h"

#include "base/bind.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace device {

GeolocationConfig::GeolocationConfig() = default;

GeolocationConfig::~GeolocationConfig() = default;

// static
void GeolocationConfig::Create(
    mojo::PendingReceiver<mojom::GeolocationConfig> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<GeolocationConfig>(),
                              std::move(receiver));
}

void GeolocationConfig::IsHighAccuracyLocationBeingCaptured(
    IsHighAccuracyLocationBeingCapturedCallback callback) {
  std::move(callback).Run(
      GeolocationProvider::GetInstance()->HighAccuracyLocationInUse());
}

}  // namespace device
