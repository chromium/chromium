// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_WINRT_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_WINRT_H_

#include "services/device/generic_sensor/platform_sensor_provider.h"

namespace device {

class PlatformSensorReaderWinBase;

// Helper class used to instantiate new PlatformSensorReaderWinBase instances.
class SensorReaderFactory {
 public:
  virtual ~SensorReaderFactory() = default;
  virtual std::unique_ptr<PlatformSensorReaderWinBase> CreateSensorReader(
      mojom::SensorType type);
};

// Implementation of PlatformSensorProvider for Windows platform using the
// Windows.Devices.Sensors WinRT API. PlatformSensorProviderWinrt is
// responsible for the following tasks:
// - Starts sensor thread and stops it when there are no active sensors.
// - Creates sensor reader.
// - Constructs PlatformSensorWin on IPC thread and returns it to requester.
class PlatformSensorProviderWinrt final : public PlatformSensorProvider {
 public:
  PlatformSensorProviderWinrt();
  ~PlatformSensorProviderWinrt() override;

  void SetSensorReaderFactoryForTesting(
      std::unique_ptr<SensorReaderFactory> sensor_reader_factory);

 protected:
  // PlatformSensorProvider interface implementation.
  void CreateSensorInternal(mojom::SensorType type,
                            SensorReadingSharedBuffer* reading_buffer,
                            const CreateSensorCallback& callback) override;

 private:
  std::unique_ptr<PlatformSensorReaderWinBase> CreateSensorReader(
      mojom::SensorType type);

  void SensorReaderCreated(
      mojom::SensorType type,
      SensorReadingSharedBuffer* reading_buffer,
      const CreateSensorCallback& callback,
      std::unique_ptr<PlatformSensorReaderWinBase> sensor_reader);

  // The Windows.Devices.Sensors WinRT API supports both STA and MTA
  // threads. STA was chosen as PlatformSensorWin can only handle STA.
  scoped_refptr<base::SingleThreadTaskRunner> com_sta_task_runner_;

  std::unique_ptr<SensorReaderFactory> sensor_reader_factory_;

  PlatformSensorProviderWinrt(const PlatformSensorProviderWinrt&) = delete;
  PlatformSensorProviderWinrt& operator=(const PlatformSensorProviderWinrt&) =
      delete;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_WINRT_H_