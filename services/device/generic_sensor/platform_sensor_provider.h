// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_H_

#include <memory>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "services/device/generic_sensor/platform_sensor.h"

namespace device {

// Base class that defines factory methods for PlatformSensor creation.
// Its implementations must be accessed via GetInstance() method.
class PlatformSensorProvider {
 public:
  using CreateSensorCallback =
      base::OnceCallback<void(scoped_refptr<PlatformSensor>)>;

  PlatformSensorProvider(const PlatformSensorProvider&) = delete;
  PlatformSensorProvider& operator=(const PlatformSensorProvider&) = delete;

  virtual ~PlatformSensorProvider();

  // Returns a PlatformSensorProvider for the current platform.
  // Note: returns 'nullptr' if there is no available implementation for
  // the current platform.
  static std::unique_ptr<PlatformSensorProvider> Create();

  // Creates new instance of PlatformSensor.
  void CreateSensor(mojom::SensorType type, CreateSensorCallback callback);

  // Gets a previously created instance of PlatformSensor by sensor type
  // |type|.
  scoped_refptr<PlatformSensor> GetSensor(mojom::SensorType type);

  // Shared memory region getter.
  base::ReadOnlySharedMemoryRegion CloneSharedMemoryRegion();

  bool has_sensors() const { return !sensor_map_.empty(); }
  bool has_pending_requests() const { return !requests_map_.empty(); }

 protected:
  PlatformSensorProvider();

  // Method that must be implemented by platform specific classes.
  virtual void CreateSensorInternal(mojom::SensorType type,
                                    SensorReadingSharedBuffer* reading_buffer,
                                    CreateSensorCallback callback) = 0;

  // Implementations might override this method to free resources when there
  // are no sensors left.
  virtual void FreeResources() {}

  void NotifySensorCreated(mojom::SensorType type,
                           scoped_refptr<PlatformSensor> sensor);

  std::vector<mojom::SensorType> GetPendingRequestTypes();

  bool CreateSharedBufferIfNeeded();

  SensorReadingSharedBuffer* GetSensorReadingSharedBufferForType(
      mojom::SensorType type);
  void RemoveSensor(mojom::SensorType type, PlatformSensor* sensor);

  // Determines if the ISensor or Windows.Devices.Sensors implementation
  // should be used on Windows.
  static bool UseWindowsWinrt();

  THREAD_CHECKER(thread_checker_);

 private:
  friend class PlatformSensor;  // To call RemoveSensor();

  using CallbackQueue = std::vector<CreateSensorCallback>;

  void FreeResourcesIfNeeded();

  std::map<mojom::SensorType, PlatformSensor*> sensor_map_;
  std::map<mojom::SensorType, CallbackQueue> requests_map_;
  base::MappedReadOnlyRegion mapped_region_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_H_
