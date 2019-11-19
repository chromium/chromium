// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_WIN_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_WIN_H_

#include <SensorsApi.h>
#include <wrl/client.h>

#include "services/device/generic_sensor/platform_sensor_provider.h"

namespace device {

class PlatformSensorReaderWinBase;

// Implementation of PlatformSensorProvider for Windows platform.
// PlatformSensorProviderWin is responsible for following tasks:
// - Starts sensor thread and stops it when there are no active sensors.
// - Initialises ISensorManager and creates sensor reader on sensor thread.
// - Constructs PlatformSensorWin on IPC thread and returns it to requester.
class PlatformSensorProviderWin final : public PlatformSensorProvider {
 public:
  PlatformSensorProviderWin();
  ~PlatformSensorProviderWin() override;

  // Overrides ISensorManager COM interface provided by the system, used
  // only for testing purposes.
  void SetSensorManagerForTesting(
      Microsoft::WRL::ComPtr<ISensorManager> sensor_manager);

  scoped_refptr<base::SingleThreadTaskRunner> GetComStaTaskRunnerForTesting();

 protected:
  // PlatformSensorProvider interface implementation.
  void CreateSensorInternal(mojom::SensorType type,
                            SensorReadingSharedBuffer* reading_buffer,
                            const CreateSensorCallback& callback) override;

 private:
  void InitSensorManager();
  void OnInitSensorManager(mojom::SensorType type,
                           SensorReadingSharedBuffer* reading_buffer,
                           const CreateSensorCallback& callback);
  std::unique_ptr<PlatformSensorReaderWinBase> CreateSensorReader(
      mojom::SensorType type);
  void SensorReaderCreated(
      mojom::SensorType type,
      SensorReadingSharedBuffer* reading_buffer,
      const CreateSensorCallback& callback,
      std::unique_ptr<PlatformSensorReaderWinBase> sensor_reader);

  scoped_refptr<base::SingleThreadTaskRunner> com_sta_task_runner_;
  Microsoft::WRL::ComPtr<ISensorManager> sensor_manager_;

  DISALLOW_COPY_AND_ASSIGN(PlatformSensorProviderWin);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_WIN_H_
