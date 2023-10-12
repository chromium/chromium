// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/virtual_platform_sensor.h"

#include "services/device/public/cpp/generic_sensor/platform_sensor_configuration.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"

namespace device {

VirtualPlatformSensor::VirtualPlatformSensor(
    mojom::SensorType type,
    SensorReadingSharedBuffer* reading_buffer,
    PlatformSensorProvider* provider)
    : PlatformSensor(type, reading_buffer, provider) {}

VirtualPlatformSensor::~VirtualPlatformSensor() = default;

void VirtualPlatformSensor::AddReading(const SensorReading& reading) {
  UpdateSharedBufferAndNotifyClients(reading);
}

void VirtualPlatformSensor::SimulateSensorRemoval() {
  // We could also try to set is_active_ to false and/or call
  // ResetReadingBuffer(). The current behavior matches the Linux and Win32
  // backends and just leaves it up to clients like Blink to ultimately call
  // PlatformSensor::StopListening().
  NotifySensorError();
}

bool VirtualPlatformSensor::StartSensor(
    const PlatformSensorConfiguration& optimal_configuration) {
  optimal_configuration_ = optimal_configuration;
  return true;
}

void VirtualPlatformSensor::StopSensor() {
  optimal_configuration_.reset();
}

mojom::ReportingMode VirtualPlatformSensor::GetReportingMode() {
  // Ambient Light Sensors' ReportingMode is ON_CHANGE on all platforms we
  // support; make it so here as well.
  const mojom::ReportingMode default_mode =
      GetType() == mojom::SensorType::AMBIENT_LIGHT
          ? mojom::ReportingMode::ON_CHANGE
          : mojom::ReportingMode::CONTINUOUS;

  return reporting_mode_.value_or(default_mode);
}

PlatformSensorConfiguration VirtualPlatformSensor::GetDefaultConfiguration() {
  return PlatformSensorConfiguration(GetSensorDefaultFrequency(GetType()));
}

bool VirtualPlatformSensor::CheckSensorConfiguration(
    const PlatformSensorConfiguration& configuration) {
  return configuration.frequency() <= GetMaximumSupportedFrequency() &&
         configuration.frequency() >= GetMinimumSupportedFrequency();
}

double VirtualPlatformSensor::GetMinimumSupportedFrequency() {
  return minimum_supported_frequency_.value_or(
      PlatformSensor::GetMinimumSupportedFrequency());
}

double VirtualPlatformSensor::GetMaximumSupportedFrequency() {
  return maximum_supported_frequency_.value_or(
      GetSensorMaxAllowedFrequency(GetType()));
}

}  // namespace device
