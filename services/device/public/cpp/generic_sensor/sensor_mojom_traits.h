// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_SENSOR_MOJOM_TRAITS_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_SENSOR_MOJOM_TRAITS_H_

#include "services/device/public/cpp/generic_sensor/platform_sensor_configuration.h"
#include "services/device/public/mojom/sensor.mojom.h"

namespace mojo {

template <>
struct StructTraits<device::mojom::SensorConfigurationDataView,
                    device::PlatformSensorConfiguration> {
  static double frequency(const device::PlatformSensorConfiguration& input) {
    return input.frequency();
  }

  static bool Read(device::mojom::SensorConfigurationDataView data,
                   device::PlatformSensorConfiguration* out);
};

}  // namespace mojo

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_SENSOR_MOJOM_TRAITS_H_
