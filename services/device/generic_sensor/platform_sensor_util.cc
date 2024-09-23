// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_util.h"

#include <cmath>

#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"

namespace device {

namespace {

// Check that each rounding multiple is positive number.
static_assert(kAccelerometerRoundingMultiple > 0.0,
              "Rounding multiple must be positive.");

static_assert(kAlsRoundingMultiple > 0,
              "Rounding multiple must be positive.");

static_assert(kGyroscopeRoundingMultiple > 0.0,
              "Rounding multiple must be positive.");

static_assert(kOrientationEulerRoundingMultiple > 0.0,
              "Rounding multiple must be positive.");

static_assert(kOrientationQuaternionRoundingMultiple > 0.0,
              "Rounding multiple must be positive.");

static_assert(kMagnetometerRoundingMultiple > 0.0,
              "Rounding multiple must be positive.");

// Check that threshold value is at least half of rounding multiple.
static_assert(kAlsSignificanceThreshold >= (kAlsRoundingMultiple / 2),
              "Threshold must be at least half of rounding multiple.");

template <typename T>
T square(T x) {
  return x * x;
}

}  // namespace

double RoundToMultiple(double value, double multiple) {
  double scaledValue = value / multiple;

  if (value < 0.0) {
    return -multiple * floor(-scaledValue + 0.5);
  } else {
    return multiple * floor(scaledValue + 0.5);
  }
}

void RoundAccelerometerReading(SensorReadingXYZ* reading) {
  reading->x = RoundToMultiple(reading->x, kAccelerometerRoundingMultiple);
  reading->y = RoundToMultiple(reading->y, kAccelerometerRoundingMultiple);
  reading->z = RoundToMultiple(reading->z, kAccelerometerRoundingMultiple);
}

void RoundGyroscopeReading(SensorReadingXYZ* reading) {
  reading->x = RoundToMultiple(reading->x, kGyroscopeRoundingMultiple);
  reading->y = RoundToMultiple(reading->y, kGyroscopeRoundingMultiple);
  reading->z = RoundToMultiple(reading->z, kGyroscopeRoundingMultiple);
}

void RoundIlluminanceReading(SensorReadingSingle* reading) {
  reading->value = RoundToMultiple(reading->value, kAlsRoundingMultiple);
}

void RoundOrientationQuaternionReading(SensorReadingQuat* reading) {
  double original_angle_div_2 = std::acos(reading->w);
  double rounded_angle_div_2 =
      RoundToMultiple(original_angle_div_2 * 2.0,
                      kOrientationQuaternionRoundingMultiple) /
      2.0;
  if (rounded_angle_div_2 == 0.0) {
    // If there's no rotation after rounding, return the identity quaternion.
    reading->w = 1;
    reading->x = reading->y = reading->z = 0;
    return;
  }
  // After this, original_angle_div_2 will definitely not be too close to 0.
  double sin_angle_div_2 = std::sin(original_angle_div_2);
  double axis_x = reading->x / sin_angle_div_2;
  double axis_y = reading->y / sin_angle_div_2;
  double axis_z = reading->z / sin_angle_div_2;

  // Convert from (x,y,z) vector to azimuth/elevation.
  double xy_dist = std::sqrt(square(axis_x) + square(axis_y));

  double azim = std::atan2(axis_x, axis_y);
  double elev = std::atan2(axis_z, xy_dist);
  azim = RoundToMultiple(azim, kOrientationQuaternionRoundingMultiple);
  elev = RoundToMultiple(elev, kOrientationQuaternionRoundingMultiple);

  // Convert back from azimuth/elevation to the (x,y,z) unit vector.
  axis_x = std::sin(azim) * std::cos(elev);
  axis_y = std::cos(azim) * std::cos(elev);
  axis_z = std::sin(elev);

  // Reconstruct Quaternion from (x,y,z,rotation).
  sin_angle_div_2 = std::sin(rounded_angle_div_2);
  reading->x = axis_x * sin_angle_div_2;
  reading->y = axis_y * sin_angle_div_2;
  reading->z = axis_z * sin_angle_div_2;
  reading->w = std::cos(rounded_angle_div_2);
}

void RoundOrientationEulerReading(SensorReadingXYZ* reading) {
  reading->x = RoundToMultiple(reading->x, kOrientationEulerRoundingMultiple);
  reading->y = RoundToMultiple(reading->y, kOrientationEulerRoundingMultiple);
  reading->z = RoundToMultiple(reading->z, kOrientationEulerRoundingMultiple);
}

void RoundMagnetometerReading(SensorReadingXYZ* reading) {
  reading->x = RoundToMultiple(reading->x, kMagnetometerRoundingMultiple);
  reading->y = RoundToMultiple(reading->y, kMagnetometerRoundingMultiple);
  reading->z = RoundToMultiple(reading->z, kMagnetometerRoundingMultiple);
}

void RoundSensorReading(SensorReading* reading, mojom::SensorType sensor_type) {
  switch (sensor_type) {
    case mojom::SensorType::ACCELEROMETER:
    case mojom::SensorType::GRAVITY:
    case mojom::SensorType::LINEAR_ACCELERATION:
      RoundAccelerometerReading(&reading->accel);
      break;

    case mojom::SensorType::GYROSCOPE:
      RoundGyroscopeReading(&reading->gyro);
      break;

    case mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
    case mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES:
      RoundOrientationEulerReading(&reading->orientation_euler);
      break;

    case mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION:
    case mojom::SensorType::RELATIVE_ORIENTATION_QUATERNION:
      RoundOrientationQuaternionReading(&reading->orientation_quat);
      break;

    case mojom::SensorType::AMBIENT_LIGHT:
      RoundIlluminanceReading(&reading->als);
      break;

    case mojom::SensorType::MAGNETOMETER:
      RoundMagnetometerReading(&reading->magn);
      break;
  }
}

}  // namespace device
