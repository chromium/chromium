// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_WIN_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_WIN_H_

#include <SensorsApi.h>
#include <wrl/client.h>

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
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
// ISensor is an apartment-threaded COM interface, so instances of this class
// must be created, used and destructed on the COM STA sequence that was
// current when Create() was called. Create() returns ownership as a
// ScopedPlatformSensorReaderWinBase, which enforces the destruction part.
class PlatformSensorReaderWin32 final : public PlatformSensorReaderWinBase {
 public:
  static ScopedPlatformSensorReaderWinBase Create(
      mojom::SensorType type,
      Microsoft::WRL::ComPtr<ISensorManager> sensor_manager);

  // The following methods may be called from any sequence. Rather than locking,
  // the ones that touch COM state hop to the COM STA sequence, which means they
  // complete asynchronously when called from elsewhere; in particular
  // StartSensor() reports failures via Client::OnSensorError() rather than by
  // returning false.
  void SetClient(Client* client) override;
  base::TimeDelta GetMinimalReportingInterval() const override;
  [[nodiscard]] bool StartSensor(
      const PlatformSensorConfiguration& configuration) override;
  void StopSensor() override;

  PlatformSensorReaderWin32(const PlatformSensorReaderWin32&) = delete;
  PlatformSensorReaderWin32& operator=(const PlatformSensorReaderWin32&) =
      delete;

  // Must be destructed on the COM STA sequence used during construction.
  ~PlatformSensorReaderWin32() override;

 private:
  class EventListener;

  PlatformSensorReaderWin32(Microsoft::WRL::ComPtr<ISensor> sensor,
                            std::unique_ptr<ReaderInitParams> params);

  static Microsoft::WRL::ComPtr<ISensor> GetSensorForType(
      REFSENSOR_TYPE_ID sensor_type,
      Microsoft::WRL::ComPtr<ISensorManager> sensor_manager);

  // |stop_count_at_start| is the value of |stop_count_| observed when the start
  // was requested; it is used to detect that a stop has superseded this start.
  void StartSensorInternal(const PlatformSensorConfiguration& configuration,
                           uint64_t stop_count_at_start);
  void StopSensorInternal();
  [[nodiscard]] bool SetReportingInterval(
      const PlatformSensorConfiguration& configuration);
  [[nodiscard]] HRESULT SensorReadingChanged(ISensorDataReport* report,
                                             SensorReading* reading);
  void SensorError();

  // Returns true if a stop was requested after the start that observed
  // |stop_count_at_start| was requested. Such a start must not report errors:
  // the client is no longer listening and PlatformSensor::NotifySensorError()
  // would disable the sensor for every other client.
  bool WasStartSupersededByStop(uint64_t stop_count_at_start);

  SEQUENCE_CHECKER(com_sta_sequence_checker_);

  const std::unique_ptr<ReaderInitParams> init_params_;
  const scoped_refptr<base::SingleThreadTaskRunner> com_sta_task_runner_;

  // Protects |client_| and |stop_count_|, which are accessed both from the
  // main sequence (SetClient, StartSensor, StopSensor) and from
  // |com_sta_task_runner_| (SensorReadingChanged, SensorError).
  base::Lock lock_;
  raw_ptr<Client> client_ GUARDED_BY(lock_);
  // Number of stops requested so far. See WasStartSupersededByStop().
  uint64_t stop_count_ GUARDED_BY(lock_) = 0;

  // Following class members are accessed exclusively on |com_sta_task_runner_|,
  // which runs in a COM STA.
  bool sensor_active_ GUARDED_BY_CONTEXT(com_sta_sequence_checker_) = false;
  Microsoft::WRL::ComPtr<ISensor> sensor_
      GUARDED_BY_CONTEXT(com_sta_sequence_checker_);
  Microsoft::WRL::ComPtr<EventListener> event_listener_
      GUARDED_BY_CONTEXT(com_sta_sequence_checker_);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_WIN_H_
