// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_GEOLOCATION_CONFIG_H_
#define SERVICES_DEVICE_GEOLOCATION_GEOLOCATION_CONFIG_H_

#include "base/compiler_specific.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/geolocation/geolocation_provider_impl.h"
#include "services/device/public/mojom/geolocation_config.mojom.h"

namespace device {

// Implements the GeolocationConfig Mojo interface.
class GeolocationConfig : public mojom::GeolocationConfig {
 public:
  GeolocationConfig();
  ~GeolocationConfig() override;

  static void Create(mojo::PendingReceiver<mojom::GeolocationConfig> receiver);

  void IsHighAccuracyLocationBeingCaptured(
      IsHighAccuracyLocationBeingCapturedCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GeolocationConfig);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_GEOLOCATION_CONFIG_H_
