// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_provider_mac.h"

namespace device {

PlatformSensorProviderMac::PlatformSensorProviderMac() = default;

PlatformSensorProviderMac::~PlatformSensorProviderMac() = default;

base::WeakPtr<PlatformSensorProvider> PlatformSensorProviderMac::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void PlatformSensorProviderMac::CreateSensorInternal(
    mojom::SensorType type,
    CreateSensorCallback callback) {
  // All the cool code to poke at undocumented hardware internals stopped
  // working long ago and no longer functions with modern hardware. Perhaps
  // some day we can find new ways to access hardware, but for now, there is
  // nothing available.
  std::move(callback).Run(nullptr);
}

}  // namespace device
