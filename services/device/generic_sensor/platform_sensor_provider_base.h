// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_BASE_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_BASE_H_

#include "base/memory/read_only_shared_memory_region.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "services/device/generic_sensor/platform_sensor.h"

namespace device {

// Base class that defines factory methods for PlatformSensor creation.
// Its implementations must be accessed via GetInstance() method.
class PlatformSensorProviderBase {
 public:
  using CreateSensorCallback =
      base::OnceCallback<void(scoped_refptr<PlatformSensor>)>;

  PlatformSensorProviderBase(const PlatformSensorProviderBase&) = delete;
  PlatformSensorProviderBase& operator=(const PlatformSensorProviderBase&) =
      delete;

  // Creates new instance of PlatformSensor.
  void CreateSensor(mojom::SensorType type, CreateSensorCallback callback);

  // Gets previously created instance of PlatformSensor by sensor type |type|.
  scoped_refptr<PlatformSensor> GetSensor(mojom::SensorType type);

  // Shared memory region getters.
  base::ReadOnlySharedMemoryRegion CloneSharedMemoryRegion();

 protected:
  PlatformSensorProviderBase();
  virtual ~PlatformSensorProviderBase();

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

  THREAD_CHECKER(thread_checker_);

 private:
  friend class PlatformSensor;  // To call RemoveSensor();

  void FreeResourcesIfNeeded();

 private:
  using CallbackQueue = std::vector<CreateSensorCallback>;

  std::map<mojom::SensorType, PlatformSensor*> sensor_map_;
  std::map<mojom::SensorType, CallbackQueue> requests_map_;
  base::MappedReadOnlyRegion mapped_region_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_BASE_H_
