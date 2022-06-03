// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/generic_sensor/platform_sensor_configuration.h"

#include "base/check_op.h"

namespace device {

PlatformSensorConfiguration::PlatformSensorConfiguration(double frequency)
    : frequency_(frequency) {
  DCHECK_GT(frequency_, 0.0);
}

PlatformSensorConfiguration::PlatformSensorConfiguration() = default;
PlatformSensorConfiguration::~PlatformSensorConfiguration() = default;

void PlatformSensorConfiguration::set_frequency(double frequency) {
  DCHECK_GT(frequency_, 0.0);
  frequency_ = frequency;
}

bool PlatformSensorConfiguration::operator==(
    const PlatformSensorConfiguration& other) const {
  return frequency_ == other.frequency();
}

bool PlatformSensorConfiguration::operator>(
    const PlatformSensorConfiguration& other) const {
  return frequency() > other.frequency();
}

}  // namespace device
