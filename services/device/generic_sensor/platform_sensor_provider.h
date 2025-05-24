// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "services/device/generic_sensor/platform_sensor.h"

namespace device {

// Base class that defines factory methods for PlatformSensor creation.
class PlatformSensorProvider {
 public:
  using CreateSensorCallback =
      base::OnceCallback<void(scoped_refptr<PlatformSensor>)>;

  PlatformSensorProvider(const PlatformSensorProvider&) = delete;
  PlatformSensorProvider& operator=(const PlatformSensorProvider&) = delete;

  virtual ~PlatformSensorProvider();

  virtual base::WeakPtr<PlatformSensorProvider> AsWeakPtr() = 0;

  // Returns a PlatformSensorProvider for the current platform.
  // Note: returns 'nullptr' if there is no available implementation for
  // the current platform.
  static std::unique_ptr<PlatformSensorProvider> Create();

  // Creates new instance of PlatformSensor.
  void CreateSensor(mojom::SensorType type, CreateSensorCallback callback);

  // Removes `sensor` from `sensor_map_`, then frees resources if `sensor_map_`
  // is now empty. Does nothing if `sensor` is not in `sensor_map_`.
  void RemoveSensor(mojom::SensorType type, PlatformSensor* sensor);

  // Gets a previously created instance of PlatformSensor by sensor type
  // |type|.
  scoped_refptr<PlatformSensor> GetSensor(mojom::SensorType type);

  // Shared memory region getter.
  base::ReadOnlySharedMemoryRegion CloneSharedMemoryRegion();

  // If `mapped_region_` has been created, returns a pointer to the sensor
  // reading buffer for sensor readings of type `type`. Otherwise, returns
  // nullptr. The buffer is contained within `mapped_region_` and is owned by
  // this provider.
  SensorReadingSharedBuffer* GetSensorReadingSharedBufferForType(
      mojom::SensorType type);

  bool has_sensors() const { return !sensor_map_.empty(); }
  bool has_pending_requests() const { return !requests_map_.empty(); }

 protected:
  PlatformSensorProvider();

  // Method that must be implemented by platform specific classes.
  virtual void CreateSensorInternal(mojom::SensorType type,
                                    CreateSensorCallback callback) = 0;

  // Implementations might override this method to free resources when there
  // are no sensors left.
  virtual void FreeResources() {}

  void NotifySensorCreated(mojom::SensorType type,
                           scoped_refptr<PlatformSensor> sensor);

  std::vector<mojom::SensorType> GetPendingRequestTypes();

  bool CreateSharedBufferIfNeeded();

  // Determines if the ISensor or Windows.Devices.Sensors implementation
  // should be used on Windows.
  static bool UseWindowsWinrt();

  THREAD_CHECKER(thread_checker_);

 private:
  using CallbackQueue = std::vector<CreateSensorCallback>;

  void FreeResourcesIfNeeded();

  std::map<mojom::SensorType, raw_ptr<PlatformSensor, CtnExperimental>>
      sensor_map_;
  std::map<mojom::SensorType, CallbackQueue> requests_map_;
  base::MappedReadOnlyRegion mapped_region_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_H_
