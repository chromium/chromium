// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/generic_sensor/sensor_reading.h"

#include <new>

namespace device {

SensorReadingRaw::SensorReadingRaw() = default;

SensorReadingBase::SensorReadingBase() = default;

SensorReadingSingle::SensorReadingSingle() = default;

SensorReadingXYZ::SensorReadingXYZ() = default;

SensorReadingQuat::SensorReadingQuat() = default;

SensorReading::SensorReading() {
  // We have a static_assert in the class declaration that verifies that |raw|
  // is trivially destructible so we do not need a custom destructor here that
  // invokes |raw|'s and can keep SensorReading trivially copyable.
  new (&raw) SensorReadingRaw();
}

}  // namespace device
