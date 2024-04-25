// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_LINUX_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_LINUX_H_

#include "services/device/generic_sensor/platform_sensor_provider_linux_base.h"

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "services/device/generic_sensor/linux/sensor_device_manager.h"

namespace device {

struct SensorInfoLinux;

class PlatformSensorProviderLinux : public PlatformSensorProviderLinuxBase,
                                    public SensorDeviceManager::Delegate {
 public:
  PlatformSensorProviderLinux();

  PlatformSensorProviderLinux(const PlatformSensorProviderLinux&) = delete;
  PlatformSensorProviderLinux& operator=(const PlatformSensorProviderLinux&) =
      delete;

  ~PlatformSensorProviderLinux() override;

  base::WeakPtr<PlatformSensorProvider> AsWeakPtr() override;

  // Sets another service provided by tests.
  void SetSensorDeviceManagerForTesting(
      std::unique_ptr<SensorDeviceManager> sensor_device_manager);

 protected:
  // PlatformSensorProviderLinuxBase overrides:
  void CreateSensorInternal(mojom::SensorType type,
                            CreateSensorCallback callback) override;
  void FreeResources() override;
  bool IsSensorTypeAvailable(mojom::SensorType type) const override;

 private:
  friend class PlatformSensorAndProviderLinuxTest;

  // This is also needed for testing, as we create one provider per test, and
  // std::unique_ptr needs access to the destructor here.
  friend std::unique_ptr<PlatformSensorProviderLinux>::deleter_type;

  using SensorDeviceMap =
      std::unordered_map<mojom::SensorType, std::unique_ptr<SensorInfoLinux>>;

  // Returns SensorInfoLinux structure of a requested type.
  // If a request cannot be processed immediately, returns nullptr and
  // all the requests stored in |requests_map_| are processed after
  // enumeration is ready.
  SensorInfoLinux* GetSensorDevice(mojom::SensorType type) const;

  void DidEnumerateSensors(mojom::SensorType type,
                           CreateSensorCallback callback);

  // SensorDeviceManager::Delegate overrides:
  void OnDeviceAdded(mojom::SensorType type,
                     std::unique_ptr<SensorInfoLinux> sensor_device) override;
  void OnDeviceRemoved(mojom::SensorType type,
                       const std::string& device_node) override;

  // Stores all available sensor devices by type.
  SensorDeviceMap sensor_devices_by_type_;

  // A task runner that can run blocking tasks. SensorDeviceManager's methods
  // run in this task runner, as they need to interact with udev.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  // This manager is being used to get |SensorInfoLinux|, which represents
  // all the information of a concrete sensor provided by OS.
  std::unique_ptr<SensorDeviceManager, base::OnTaskRunnerDeleter>
      sensor_device_manager_;

  base::WeakPtrFactory<PlatformSensorProviderLinux> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_LINUX_H_
