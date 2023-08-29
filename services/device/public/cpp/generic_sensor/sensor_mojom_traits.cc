// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/generic_sensor/sensor_mojom_traits.h"

#include "base/ranges/algorithm.h"

namespace mojo {

// Note: the constant below must be maximum defined sensor traits
// (see sensor_traits.h).
constexpr double kMaxAllowedFrequency = 60.0;

// static
bool StructTraits<device::mojom::SensorConfigurationDataView,
                  device::PlatformSensorConfiguration>::
    Read(device::mojom::SensorConfigurationDataView data,
         device::PlatformSensorConfiguration* out) {
  if (data.frequency() > kMaxAllowedFrequency || data.frequency() <= 0.0) {
    return false;
  }

  out->set_frequency(data.frequency());
  return true;
}

// static
bool StructTraits<
    device::mojom::SensorReadingRawDataView,
    device::SensorReading>::Read(device::mojom::SensorReadingRawDataView data,
                                 device::SensorReading* out) {
  if (data.timestamp() < 0) {
    return false;
  }

  std::array<double, device::SensorReadingRaw::kValuesCount> raw_values;
  // We cannot use std::size(out->raw.values) in the static_assert(), so resort
  // to checking the sizeof()s match, as they should both be arrays of the same
  // type and same size.
  static_assert(sizeof(raw_values) == sizeof(out->raw.values),
                "Array sizes must match");
  if (!data.ReadValues(&raw_values)) {
    return false;
  }

  out->raw.timestamp = data.timestamp();
  base::ranges::copy(raw_values, out->raw.values);

  return true;
}

}  // namespace mojo
