// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_LINUX_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_LINUX_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"

namespace device {

class PlatformSensorConfiguration;
class PlatformSensorLinux;
struct SensorInfoLinux;

// A generic reader class that can be implemented with two different strategies:
// polling and on trigger. All methods are not thread-safe and must be called
// on a polling thread that allows I/O.
class SensorReader {
 public:
  // Creates a new instance of SensorReader. At the moment, only polling
  // reader is supported.
  static std::unique_ptr<SensorReader> Create(
      const SensorInfoLinux& sensor_info,
      base::WeakPtr<PlatformSensorLinux> sensor);

  SensorReader(const SensorReader&) = delete;
  SensorReader& operator=(const SensorReader&) = delete;

  virtual ~SensorReader();

  // Starts fetching data based on strategy this reader has chosen.
  // Only polling strategy is supported at the moment.
  virtual void StartFetchingData(
      const PlatformSensorConfiguration& configuration) = 0;

  // Stops fetching data.
  virtual void StopFetchingData() = 0;

 protected:
  explicit SensorReader(base::WeakPtr<PlatformSensorLinux> sensor);

  // Notifies |sensor_| about an error.
  void NotifyReadError();

  // A sensor that this reader is owned by and notifies about errors and
  // readings to.
  base::WeakPtr<PlatformSensorLinux> sensor_;

  // Indicates if reading is active.
  bool is_reading_active_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_LINUX_H_
