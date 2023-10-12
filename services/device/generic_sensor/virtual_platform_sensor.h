// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_VIRTUAL_PLATFORM_SENSOR_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_VIRTUAL_PLATFORM_SENSOR_H_

#include "services/device/generic_sensor/platform_sensor.h"
#include "services/device/public/cpp/generic_sensor/platform_sensor_configuration.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/mojom/sensor.mojom-shared.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

// OS-agnostic PlatformSensor implementation used in (web) tests. New instances
// are created by VirtualPlatformSensorProvider via GetSensor() calls.
class VirtualPlatformSensor : public PlatformSensor {
 public:
  VirtualPlatformSensor(mojom::SensorType type,
                        SensorReadingSharedBuffer* reading_buffer,
                        PlatformSensorProvider* provider);

  // Simulates the reporting of a new reading by a platform sensor. The new
  // reading still goes through the UpdateSharedBufferAndNotifyClients()
  // machinery, which means it may not end up being stored (for example, if the
  // sensor is not active or a threshold check fails).
  void AddReading(const SensorReading&);

  // Simulates that a platform sensor has been removed and therefore stopped
  // providing readings.
  void SimulateSensorRemoval();

  // Returns the current PlatformSensorConfiguration being used by the sensor
  // if it is currently active, or a null absl::optional otherwise.
  const absl::optional<PlatformSensorConfiguration>& optimal_configuration()
      const {
    return optimal_configuration_;
  }

  void set_minimum_supported_frequency(double frequency) {
    minimum_supported_frequency_ = frequency;
  }
  void set_maximum_supported_frequency(double frequency) {
    maximum_supported_frequency_ = frequency;
  }

  void set_reporting_mode(mojom::ReportingMode reporting_mode) {
    reporting_mode_ = reporting_mode;
  }

 protected:
  ~VirtualPlatformSensor() override;

 private:
  // PlatformSensor overrides.
  bool StartSensor(const PlatformSensorConfiguration& configuration) override;
  void StopSensor() override;
  mojom::ReportingMode GetReportingMode() override;
  PlatformSensorConfiguration GetDefaultConfiguration() override;
  bool CheckSensorConfiguration(
      const PlatformSensorConfiguration& configuration) override;
  double GetMinimumSupportedFrequency() override;
  double GetMaximumSupportedFrequency() override;

  absl::optional<double> minimum_supported_frequency_;
  absl::optional<double> maximum_supported_frequency_;
  absl::optional<PlatformSensorConfiguration> optimal_configuration_;
  absl::optional<mojom::ReportingMode> reporting_mode_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_VIRTUAL_PLATFORM_SENSOR_H_
