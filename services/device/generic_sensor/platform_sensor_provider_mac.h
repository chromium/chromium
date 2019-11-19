// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_MAC_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_MAC_H_

#include "services/device/generic_sensor/platform_sensor_provider.h"

namespace device {

class PlatformSensorProviderMac : public PlatformSensorProvider {
 public:
  PlatformSensorProviderMac();
  ~PlatformSensorProviderMac() override;

 protected:
  void CreateSensorInternal(mojom::SensorType type,
                            SensorReadingSharedBuffer* reading_buffer,
                            const CreateSensorCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PlatformSensorProviderMac);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_MAC_H_
