// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_LINUX_BASE_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_LINUX_BASE_H_

#include "services/device/generic_sensor/platform_sensor_provider.h"

namespace device {

class PlatformSensorProviderLinuxBase : public PlatformSensorProvider {
 protected:
  virtual bool IsFusionSensorType(mojom::SensorType type) const;

  void CreateFusionSensor(mojom::SensorType type,
                          CreateSensorCallback callback);

  virtual bool IsSensorTypeAvailable(mojom::SensorType type) const = 0;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_LINUX_BASE_H_
