// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/generic_sensor/sensor_traits.h"

namespace device {

using mojom::SensorType;

double GetSensorMaxAllowedFrequency(SensorType type) {
  switch (type) {
    case SensorType::AMBIENT_LIGHT:
      return SensorTraits<SensorType::AMBIENT_LIGHT>::kMaxAllowedFrequency;
    case SensorType::ACCELEROMETER:
      return SensorTraits<SensorType::ACCELEROMETER>::kMaxAllowedFrequency;
    case SensorType::LINEAR_ACCELERATION:
      return SensorTraits<
          SensorType::LINEAR_ACCELERATION>::kMaxAllowedFrequency;
    case SensorType::GRAVITY:
      return SensorTraits<SensorType::GRAVITY>::kMaxAllowedFrequency;
    case SensorType::GYROSCOPE:
      return SensorTraits<SensorType::GYROSCOPE>::kMaxAllowedFrequency;
    case SensorType::MAGNETOMETER:
      return SensorTraits<SensorType::MAGNETOMETER>::kMaxAllowedFrequency;
    case SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
      return SensorTraits<
          SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES>::kMaxAllowedFrequency;
    case SensorType::ABSOLUTE_ORIENTATION_QUATERNION:
      return SensorTraits<
          SensorType::ABSOLUTE_ORIENTATION_QUATERNION>::kMaxAllowedFrequency;
    case SensorType::RELATIVE_ORIENTATION_EULER_ANGLES:
      return SensorTraits<
          SensorType::RELATIVE_ORIENTATION_EULER_ANGLES>::kMaxAllowedFrequency;
    case SensorType::RELATIVE_ORIENTATION_QUATERNION:
      return SensorTraits<
          SensorType::RELATIVE_ORIENTATION_QUATERNION>::kMaxAllowedFrequency;
    // No default so the compiler will warn us if a new type is added.
  }
}

double GetSensorDefaultFrequency(mojom::SensorType type) {
  switch (type) {
    case SensorType::AMBIENT_LIGHT:
      return SensorTraits<SensorType::AMBIENT_LIGHT>::kDefaultFrequency;
    case SensorType::ACCELEROMETER:
      return SensorTraits<SensorType::ACCELEROMETER>::kDefaultFrequency;
    case SensorType::LINEAR_ACCELERATION:
      return SensorTraits<SensorType::LINEAR_ACCELERATION>::kDefaultFrequency;
    case SensorType::GRAVITY:
      return SensorTraits<SensorType::GRAVITY>::kDefaultFrequency;
    case SensorType::GYROSCOPE:
      return SensorTraits<SensorType::GYROSCOPE>::kDefaultFrequency;
    case SensorType::MAGNETOMETER:
      return SensorTraits<SensorType::MAGNETOMETER>::kDefaultFrequency;
    case SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
      return SensorTraits<
          SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES>::kDefaultFrequency;
    case SensorType::ABSOLUTE_ORIENTATION_QUATERNION:
      return SensorTraits<
          SensorType::ABSOLUTE_ORIENTATION_QUATERNION>::kDefaultFrequency;
    case SensorType::RELATIVE_ORIENTATION_EULER_ANGLES:
      return SensorTraits<
          SensorType::RELATIVE_ORIENTATION_EULER_ANGLES>::kDefaultFrequency;
    case SensorType::RELATIVE_ORIENTATION_QUATERNION:
      return SensorTraits<
          SensorType::RELATIVE_ORIENTATION_QUATERNION>::kDefaultFrequency;
    // No default so the compiler will warn us if a new type is added.
  }
}

}  // namespace device
