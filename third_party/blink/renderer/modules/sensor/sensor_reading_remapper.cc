// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/sensor_reading_remapper.h"

#include "base/notreached.h"
#include "services/device/public/mojom/sensor.mojom-shared.h"

using device::SensorReading;
using device::SensorReadingXYZ;
using device::SensorReadingQuat;
using device::mojom::blink::SensorType;

namespace blink {

namespace {
constexpr int SinScreenAngle(uint16_t angle) {
  switch (angle) {
    case 0:
      return 0;
    case 90:
      return 1;
    case 180:
      return 0;
    case 270:
      return -1;
    default:
      NOTREACHED_IN_MIGRATION();
      return 0;
  }
}

constexpr int CosScreenAngle(uint16_t angle) {
  switch (angle) {
    case 0:
      return 1;
    case 90:
      return 0;
    case 180:
      return -1;
    case 270:
      return 0;
    default:
      NOTREACHED_IN_MIGRATION();
      return 1;
  }
}

void RemapSensorReadingXYZ(uint16_t angle, SensorReadingXYZ& reading) {
  int cos = CosScreenAngle(angle);
  int sin = SinScreenAngle(angle);
  double x = reading.x;
  double y = reading.y;

  reading.x = x * cos + y * sin;
  reading.y = y * cos - x * sin;
}

constexpr double kInverseSqrt2 = 0.70710678118;

// Returns sin(-angle/2) for the given orientation angle.
constexpr double SinNegativeHalfScreenAngle(uint16_t angle) {
  switch (angle) {
    case 0:
      return 0;  // sin 0
    case 90:
      return -kInverseSqrt2;  // sin -45
    case 180:
      return -1;  // sin -90
    case 270:
      return -kInverseSqrt2;  // sin -135
    default:
      NOTREACHED_IN_MIGRATION();
      return 0;
  }
}

// Returns cos(-angle/2) for the given orientation angle.
constexpr double CosNegativeHalfScreenAngle(uint16_t angle) {
  switch (angle) {
    case 0:
      return 1;  // cos 0
    case 90:
      return kInverseSqrt2;  // cos -45
    case 180:
      return 0;  // cos -90
    case 270:
      return -kInverseSqrt2;  // cos -135
    default:
      NOTREACHED_IN_MIGRATION();
      return 1;
  }
}

void RemapSensorReadingQuat(uint16_t angle, SensorReadingQuat& reading) {
  // Remapping quaternion = q = [qx, qy, qz, qw] =
  // [0, 0, sin(-angle / 2), cos(-angle / 2)] - unit quaternion.
  // reading = [x, y, z, w] - unit quaternion.
  // Resulting unit quaternion = reading * q.
  double qw = CosNegativeHalfScreenAngle(angle);
  double qz = SinNegativeHalfScreenAngle(angle);
  double x = reading.x;
  double y = reading.y;
  double z = reading.z;
  double w = reading.w;
  // Given that qx == 0 and qy == 0.
  reading.x = qw * x + qz * y;
  reading.y = qw * y - qz * x;
  reading.z = qw * z + qz * w;
  reading.w = qw * w - qz * z;
}

}  // namespace

// static
void SensorReadingRemapper::RemapToScreenCoords(
    SensorType type,
    uint16_t angle,
    device::SensorReading* reading) {
  DCHECK(reading);
  switch (type) {
    case SensorType::AMBIENT_LIGHT:
      NOTREACHED_IN_MIGRATION()
          << "Remap must not be performed for the sensor type " << type;
      break;
    case SensorType::ACCELEROMETER:
    case SensorType::LINEAR_ACCELERATION:
    case SensorType::GRAVITY:
      RemapSensorReadingXYZ(angle, reading->accel);
      break;
    case SensorType::GYROSCOPE:
      RemapSensorReadingXYZ(angle, reading->gyro);
      break;
    case SensorType::MAGNETOMETER:
      RemapSensorReadingXYZ(angle, reading->magn);
      break;
    case SensorType::ABSOLUTE_ORIENTATION_QUATERNION:
    case SensorType::RELATIVE_ORIENTATION_QUATERNION:
      RemapSensorReadingQuat(angle, reading->orientation_quat);
      break;
    case SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
    case SensorType::RELATIVE_ORIENTATION_EULER_ANGLES:
      NOTREACHED_IN_MIGRATION()
          << "Remap is not yet implemented for the sensor type " << type;
      break;
  }
}

}  // namespace blink
