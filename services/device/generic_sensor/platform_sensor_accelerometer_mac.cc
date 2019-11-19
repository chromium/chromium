// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_accelerometer_mac.h"

#include <stdint.h>

#include <cmath>

#include "base/bind.h"
#include "base/numerics/math_constants.h"
#include "device/base/synchronization/shared_memory_seqlock_buffer.h"
#include "services/device/generic_sensor/platform_sensor_provider_mac.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"
#include "third_party/sudden_motion_sensor/sudden_motion_sensor_mac.h"

namespace {

constexpr double kGravityThreshold = base::kMeanGravityDouble * 0.01;

bool IsSignificantlyDifferent(const device::SensorReading& reading1,
                              const device::SensorReading& reading2) {
  return (std::fabs(reading1.accel.x - reading2.accel.x) >=
          kGravityThreshold) ||
         (std::fabs(reading1.accel.y - reading2.accel.y) >=
          kGravityThreshold) ||
         (std::fabs(reading1.accel.z - reading2.accel.z) >= kGravityThreshold);
}

}  // namespace

namespace device {

using mojom::SensorType;

PlatformSensorAccelerometerMac::PlatformSensorAccelerometerMac(
    SensorReadingSharedBuffer* reading_buffer,
    PlatformSensorProvider* provider)
    : PlatformSensor(SensorType::ACCELEROMETER, reading_buffer, provider),
      sudden_motion_sensor_(SuddenMotionSensor::Create()) {}

PlatformSensorAccelerometerMac::~PlatformSensorAccelerometerMac() = default;

mojom::ReportingMode PlatformSensorAccelerometerMac::GetReportingMode() {
  return mojom::ReportingMode::ON_CHANGE;
}

bool PlatformSensorAccelerometerMac::CheckSensorConfiguration(
    const PlatformSensorConfiguration& configuration) {
  return configuration.frequency() > 0 &&
         configuration.frequency() <=
             SensorTraits<SensorType::ACCELEROMETER>::kMaxAllowedFrequency;
}

PlatformSensorConfiguration
PlatformSensorAccelerometerMac::GetDefaultConfiguration() {
  return PlatformSensorConfiguration(
      SensorTraits<SensorType::ACCELEROMETER>::kDefaultFrequency);
}

bool PlatformSensorAccelerometerMac::StartSensor(
    const PlatformSensorConfiguration& configuration) {
  if (!sudden_motion_sensor_)
    return false;

  float axis_value[3];
  if (!sudden_motion_sensor_->ReadSensorValues(axis_value))
    return false;

  timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMicroseconds(base::Time::kMicrosecondsPerSecond /
                                        configuration.frequency()),
      this, &PlatformSensorAccelerometerMac::PollForData);

  return true;
}

void PlatformSensorAccelerometerMac::StopSensor() {
  timer_.Stop();
}

void PlatformSensorAccelerometerMac::PollForData() {
  // Retrieve per-axis calibrated values.
  float axis_value[3];
  if (!sudden_motion_sensor_->ReadSensorValues(axis_value))
    return;

  SensorReading reading;
  reading.accel.timestamp =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  reading.accel.x = axis_value[0] * base::kMeanGravityDouble;
  reading.accel.y = axis_value[1] * base::kMeanGravityDouble;
  reading.accel.z = axis_value[2] * base::kMeanGravityDouble;

  if (IsSignificantlyDifferent(reading_, reading)) {
    reading_ = reading;
    UpdateSharedBufferAndNotifyClients(reading);
  }
}

}  // namespace device
