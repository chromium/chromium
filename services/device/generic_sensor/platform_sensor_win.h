// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_WIN_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_WIN_H_

#include "base/memory/weak_ptr.h"
#include "services/device/generic_sensor/platform_sensor.h"
#include "services/device/generic_sensor/platform_sensor_reader_win.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace device {

// Implementation of PlatformSensor interface for Windows platform. Instance
// of PlatformSensorWin is bound to IPC thread where PlatformSensorProvider is
// running and communication with Windows platform sensor is done through
// PlatformSensorReaderWinBase |sensor_reader_| interface which is bound to
// sensor thread and communicates with PlatformSensorWin using
// PlatformSensorReaderWinBase::Client interface. The error and data change
// events are forwarded to IPC task runner.
class PlatformSensorWin final : public PlatformSensor,
                                public PlatformSensorReaderWinBase::Client {
 public:
  PlatformSensorWin(
      mojom::SensorType type,
      SensorReadingSharedBuffer* reading_buffer,
      PlatformSensorProvider* provider,
      scoped_refptr<base::SingleThreadTaskRunner> sensor_thread_runner,
      std::unique_ptr<PlatformSensorReaderWinBase> sensor_reader);

  PlatformSensorConfiguration GetDefaultConfiguration() override;
  mojom::ReportingMode GetReportingMode() override;
  double GetMaximumSupportedFrequency() override;

  // PlatformSensorReaderWinBase::Client interface implementation.
  void OnReadingUpdated(const SensorReading& reading) override;
  void OnSensorError() override;

 protected:
  ~PlatformSensorWin() override;

  // PlatformSensor interface implementation.
  bool StartSensor(const PlatformSensorConfiguration& configuration) override;
  void StopSensor() override;
  bool CheckSensorConfiguration(
      const PlatformSensorConfiguration& configuration) override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> sensor_thread_runner_;
  PlatformSensorReaderWinBase* const sensor_reader_;
  base::WeakPtrFactory<PlatformSensorWin> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PlatformSensorWin);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_WIN_H_
