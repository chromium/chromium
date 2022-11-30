// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_SENSOR_TRAITS_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_SENSOR_TRAITS_H_

#include "services/device/public/mojom/sensor.mojom.h"

namespace device {

template <mojom::SensorType>
struct SensorTraits {
  static constexpr double kMaxAllowedFrequency = 60.0;
  // Used if the actual value cannot be obtained from the platform.
  static constexpr double kDefaultFrequency = 10.0;
};

template <>
struct SensorTraits<mojom::SensorType::AMBIENT_LIGHT> {
  static constexpr double kMaxAllowedFrequency = 10.0;
  static constexpr double kDefaultFrequency = 5.0;
};

template <>
struct SensorTraits<mojom::SensorType::MAGNETOMETER> {
  static constexpr double kMaxAllowedFrequency = 10.0;
  static constexpr double kDefaultFrequency = 10.0;
};

double GetSensorMaxAllowedFrequency(mojom::SensorType type);

double GetSensorDefaultFrequency(mojom::SensorType type);

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_SENSOR_TRAITS_H_
