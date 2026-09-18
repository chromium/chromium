// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_WIN_BASE_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_WIN_BASE_H_

#include <memory>

#include "base/task/sequenced_task_runner.h"

namespace base {
class TimeDelta;
}

namespace device {

class PlatformSensorConfiguration;
union SensorReading;

class PlatformSensorReaderWinBase {
 public:
  // Client interface that can be used to receive notifications about sensor
  // error or data change events.
  class Client {
   public:
    virtual void OnReadingUpdated(const SensorReading& reading) = 0;
    virtual void OnSensorError() = 0;

   protected:
    virtual ~Client() = default;
  };

  // Following methods must be thread safe.
  // Sets the client PlatformSensorReaderWinBase will use to notify
  // about errors or data change events. Only one client can be registered
  // at a time (last client to register wins) and can be removed by
  // setting the client to nullptr.
  virtual void SetClient(Client* client) = 0;
  virtual base::TimeDelta GetMinimalReportingInterval() const = 0;
  // Starts the sensor. Returning false means the sensor definitely could not
  // be started. Returning true only means that no failure was detected
  // synchronously: implementations are allowed to complete the start
  // asynchronously (PlatformSensorReaderWin32 marshals its COM calls to a COM
  // STA sequence) and report a later failure via Client::OnSensorError().
  virtual bool StartSensor(
      const PlatformSensorConfiguration& configuration) = 0;
  virtual void StopSensor() = 0;

  virtual ~PlatformSensorReaderWinBase() = default;
};

// Readers own COM objects that must be released on the COM STA sequence they
// were created on, so ownership of a reader is always expressed with a deleter
// that posts the destruction back to that sequence. Note that this also makes
// the reader outlive any task it posted to that sequence, because the deletion
// task is necessarily queued behind them.
using ScopedPlatformSensorReaderWinBase =
    std::unique_ptr<PlatformSensorReaderWinBase, base::OnTaskRunnerDeleter>;

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_WIN_BASE_H_