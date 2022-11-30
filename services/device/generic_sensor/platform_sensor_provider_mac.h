// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_MAC_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_MAC_H_

#include "services/device/generic_sensor/platform_sensor_provider.h"

namespace device {

class PlatformSensorProviderMac : public PlatformSensorProvider {
 public:
  PlatformSensorProviderMac();

  PlatformSensorProviderMac(const PlatformSensorProviderMac&) = delete;
  PlatformSensorProviderMac& operator=(const PlatformSensorProviderMac&) =
      delete;

  ~PlatformSensorProviderMac() override;

 protected:
  void CreateSensorInternal(mojom::SensorType type,
                            SensorReadingSharedBuffer* reading_buffer,
                            CreateSensorCallback callback) override;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_MAC_H_
