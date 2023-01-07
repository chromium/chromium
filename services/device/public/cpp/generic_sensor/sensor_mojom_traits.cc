// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/generic_sensor/sensor_mojom_traits.h"

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

}  // namespace mojo
