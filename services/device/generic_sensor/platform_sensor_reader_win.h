// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_WIN_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_WIN_H_

#include <SensorsApi.h>
#include <wrl/client.h>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "services/device/generic_sensor/platform_sensor_reader_win_base.h"
#include "services/device/public/mojom/sensor.mojom.h"

namespace base {
class TimeDelta;
}

namespace device {

class PlatformSensorConfiguration;
struct ReaderInitParams;
union SensorReading;

// Generic class that uses ISensor interface to fetch sensor data. Used
// by PlatformSensorWin and delivers notifications via Client interface.
// Instances of this class must be created and destructed on the same thread.
class PlatformSensorReaderWin32 final : public PlatformSensorReaderWinBase {
 public:
  static std::unique_ptr<PlatformSensorReaderWinBase> Create(
      mojom::SensorType type,
      Microsoft::WRL::ComPtr<ISensorManager> sensor_manager);

  // Following methods are thread safe.
  void SetClient(Client* client) override;
  base::TimeDelta GetMinimalReportingInterval() const override;
  [[nodiscard]] bool StartSensor(
      const PlatformSensorConfiguration& configuration) override;
  void StopSensor() override;

  PlatformSensorReaderWin32(const PlatformSensorReaderWin32&) = delete;
  PlatformSensorReaderWin32& operator=(const PlatformSensorReaderWin32&) =
      delete;

  // Must be destructed on the same thread that was used during construction.
  ~PlatformSensorReaderWin32() override;

 private:
  PlatformSensorReaderWin32(Microsoft::WRL::ComPtr<ISensor> sensor,
                            std::unique_ptr<ReaderInitParams> params);

  static Microsoft::WRL::ComPtr<ISensor> GetSensorForType(
      REFSENSOR_TYPE_ID sensor_type,
      Microsoft::WRL::ComPtr<ISensorManager> sensor_manager);

  [[nodiscard]] bool SetReportingInterval(
      const PlatformSensorConfiguration& configuration);
  void ListenSensorEvent();
  [[nodiscard]] HRESULT SensorReadingChanged(ISensorDataReport* report,
                                             SensorReading* reading);
  void SensorError();

 private:
  friend class EventListener;

  const std::unique_ptr<ReaderInitParams> init_params_;
  scoped_refptr<base::SingleThreadTaskRunner> com_sta_task_runner_;
  // Following class members are protected by lock, because SetClient,
  // StartSensor and StopSensor are called from another thread by
  // PlatformSensorWin that can modify internal state of the object.
  base::Lock lock_;
  bool sensor_active_ GUARDED_BY(lock_);
  raw_ptr<Client> client_ GUARDED_BY(lock_);
  Microsoft::WRL::ComPtr<ISensor> sensor_;
  Microsoft::WRL::ComPtr<ISensorEvents> event_listener_;
  base::WeakPtrFactory<PlatformSensorReaderWin32> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_WIN_H_
