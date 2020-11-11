// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_CHROMEOS_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_CHROMEOS_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "chromeos/components/sensors/mojom/sensor.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/generic_sensor/platform_sensor.h"

namespace device {

class PlatformSensorChromeOS
    : public PlatformSensor,
      public chromeos::sensors::mojom::SensorDeviceSamplesObserver {
 public:
  PlatformSensorChromeOS(int32_t iio_device_id,
                         mojom::SensorType type,
                         SensorReadingSharedBuffer* reading_buffer,
                         PlatformSensorProvider* provider,
                         double scale,
                         mojo::Remote<chromeos::sensors::mojom::SensorDevice>
                             sensor_device_remote);
  PlatformSensorChromeOS(const PlatformSensorChromeOS&) = delete;
  PlatformSensorChromeOS& operator=(const PlatformSensorChromeOS&) = delete;

  // PlatformSensor overrides:
  // Only ambient light sensors' ReportingMode is ON_CHANGE.
  mojom::ReportingMode GetReportingMode() override;
  bool CheckSensorConfiguration(
      const PlatformSensorConfiguration& configuration) override;
  PlatformSensorConfiguration GetDefaultConfiguration() override;

  // chromeos::sensors::mojom::SensorDeviceSamplesObserver overrides:
  void OnSampleUpdated(const base::flat_map<int32_t, int64_t>& sample) override;
  void OnErrorOccurred(
      chromeos::sensors::mojom::ObserverErrorType type) override;

 protected:
  ~PlatformSensorChromeOS() override;

  // PlatformSensor overrides:
  bool StartSensor(const PlatformSensorConfiguration& configuration) override;
  void StopSensor() override;

 private:
  void ResetOnError();

  void StartReadingIfReady();

  mojo::PendingRemote<chromeos::sensors::mojom::SensorDeviceSamplesObserver>
  BindNewPipeAndPassRemote();
  void OnObserverDisconnect();

  void SetRequiredChannels();
  void GetAllChannelIdsCallback(
      const std::vector<std::string>& iio_channel_ids);

  void UpdateSensorDeviceFrequency();
  void SetFrequencyCallback(double target_frequency, double result_frequency);

  void SetChannelsEnabled();
  void SetChannelsEnabledCallback(const std::vector<int32_t>& failed_indices);

  double GetScaledValue(int64_t value) const;

  int32_t iio_device_id_;
  const PlatformSensorConfiguration default_configuration_;
  PlatformSensorConfiguration current_configuration_;

  double scale_;

  mojo::Remote<chromeos::sensors::mojom::SensorDevice> sensor_device_remote_;

  // The required channel ids for the sensor.
  std::vector<std::string> required_channel_ids_;

  // The list of channel ids retrieved from iioservice. Use channels' indices
  // in this list to identify them.
  std::vector<std::string> iio_channel_ids_;
  // Channel indices of |required_channel_ids_| to enable.
  std::vector<int32_t> channel_indices_;

  // Stores previously read values that are used to
  // determine whether the recent values are changed
  // and IPC can be notified that updates are available.
  SensorReading old_values_;

  mojo::Receiver<chromeos::sensors::mojom::SensorDeviceSamplesObserver>
      receiver_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PlatformSensorChromeOS> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_CHROMEOS_H_
