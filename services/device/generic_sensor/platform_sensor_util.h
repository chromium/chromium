// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_UTIL_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_UTIL_H_

#include "services/device/public/cpp/generic_sensor/sensor_reading.h"

namespace device {

// Sensors are accurate enough that it is possible to sample sensor data to
// implement a "fingerprint attack" to reveal the sensor calibration data.
// This data is effectively a unique number, or fingerprint, for a device. To
// combat this, sensor values are rounded to the nearest multiple of some small
// number. This is still accurate enough to satisfy the needs of the sensor
// user, but guards against this attack.
//
// Additional info can be found at https://crbug.com/1018180.
//
// Rounding also protects against using the gyroscope as a primitive microphone
// to record audio. Additional info at https://crbug.com/1031190.

// Units are SI meters per second squared (m/s^2).
constexpr double kAccelerometerRoundingMultiple = 0.1;

// Units are radians/second. This value corresponds to 0.1 deg./sec.
constexpr double kGyroscopeRoundingMultiple = 0.00174532925199432963;

// Units are degrees.
constexpr double kOrientationEulerRoundingMultiple = 0.1;

// Units are radians. This value corresponds to 0.1 degrees.
constexpr double kOrientationQuaternionRoundingMultiple = 0.0017453292519943296;

// Round |value| to be a multiple of |multiple|.
//
// NOTE: Exposed for testing. Please use other Rounding functions below.
//
// Some examples:
//
// ( 1.24, 0.1) => 1.2
// ( 1.25, 0.1) => 1.3
// (-1.24, 0.1) => -1.2
// (-1.25, 0.1) => -1.3
double RoundToMultiple(double value, double multiple);

// Round accelerometer sensor reading to guard user privacy.
void RoundAccelerometerReading(SensorReadingXYZ* reading);

// Round gyroscope sensor reading to guard user privacy.
void RoundGyroscopeReading(SensorReadingXYZ* reading);

// Round orientation Euler angle sensor reading to guard user privacy.
void RoundOrientationEulerReading(SensorReadingXYZ* reading);

// Round orientation quaternion sensor reading to guard user privacy.
// |reading| is assumed to be unscaled (unit length).
void RoundOrientationQuaternionReading(SensorReadingQuat* reading);

// Round the sensor reading to guard user privacy.
void RoundSensorReading(SensorReading* reading, mojom::SensorType sensor_type);

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_UTIL_H_
