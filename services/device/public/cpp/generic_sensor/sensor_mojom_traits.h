// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_SENSOR_MOJOM_TRAITS_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_SENSOR_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/device/public/cpp/generic_sensor/platform_sensor_configuration.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/mojom/sensor.mojom-shared.h"

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

template <>
struct StructTraits<device::mojom::SensorReadingRawDataView,
                    device::SensorReading> {
  static double timestamp(const device::SensorReading& reading) {
    return reading.timestamp();
  }

  static std::array<double, 4> values(const device::SensorReading& reading) {
    return {reading.raw.values[0], reading.raw.values[1], reading.raw.values[2],
            reading.raw.values[3]};
  }

  static bool Read(device::mojom::SensorReadingRawDataView data,
                   device::SensorReading* out_reading);
};

}  // namespace mojo

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_SENSOR_MOJOM_TRAITS_H_
