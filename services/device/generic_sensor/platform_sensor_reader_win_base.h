// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_WIN_BASE_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_WIN_BASE_H_

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
  virtual bool StartSensor(
      const PlatformSensorConfiguration& configuration) = 0;
  virtual void StopSensor() = 0;

  virtual ~PlatformSensorReaderWinBase() = default;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_WIN_BASE_H_