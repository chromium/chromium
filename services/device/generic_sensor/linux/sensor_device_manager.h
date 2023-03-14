// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_LINUX_SENSOR_DEVICE_MANAGER_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_LINUX_SENSOR_DEVICE_MANAGER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "device/udev_linux/udev_watcher.h"
#include "services/device/public/mojom/sensor.mojom.h"

namespace device {

struct SensorInfoLinux;

// Monitors udev added/removed events and enumerates existing sensor devices;
// after processing, notifies its |Delegate|. It has own cache to speed up an
// identification process of removed devices.
// Start() must run in a task runner that can block.
class SensorDeviceManager : public UdevWatcher::Observer {
 public:
  class Delegate {
   public:
    // Called after SensorDeviceManager has identified a udev device, which
    // belongs to "iio" subsystem.
    virtual void OnDeviceAdded(mojom::SensorType type,
                               std::unique_ptr<SensorInfoLinux> sensor) = 0;

    // Called after "removed" event is received from LinuxDeviceMonitor and
    // sensor is identified as known.
    virtual void OnDeviceRemoved(mojom::SensorType type,
                                 const std::string& device_node) = 0;

   protected:
    virtual ~Delegate() {}
  };

  explicit SensorDeviceManager(base::WeakPtr<Delegate> delegate);

  SensorDeviceManager(const SensorDeviceManager&) = delete;
  SensorDeviceManager& operator=(const SensorDeviceManager&) = delete;

  ~SensorDeviceManager() override;

  // Starts monitoring sensor-related udev events, and enumerates existing
  // sensors. If enumeration has already completed, does nothing.
  // This method must be run from a task runner that can block.
  virtual void MaybeStartEnumeration();

 protected:
  using SensorDeviceMap = std::unordered_map<std::string, mojom::SensorType>;

  // Wrappers around udev system methods that can be implemented differently
  // by tests.
  virtual std::string GetUdevDeviceGetSubsystem(udev_device* dev);
  virtual std::string GetUdevDeviceGetSyspath(udev_device* dev);
  virtual std::string GetUdevDeviceGetSysattrValue(
      udev_device* dev,
      const std::string& attribute);
  virtual std::string GetUdevDeviceGetDevnode(udev_device* dev);

  // UdevWatcher::Observer overrides
  void OnDeviceAdded(ScopedUdevDevicePtr udev_device) override;
  void OnDeviceRemoved(ScopedUdevDevicePtr device) override;
  void OnDeviceChanged(ScopedUdevDevicePtr device) override;

  // Represents a map of sensors that are already known to this manager after
  // initial enumeration.
  SensorDeviceMap sensors_by_node_;

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<UdevWatcher> udev_watcher_;

  base::WeakPtr<Delegate> delegate_;

  scoped_refptr<base::SequencedTaskRunner> delegate_task_runner_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_LINUX_SENSOR_DEVICE_MANAGER_H_
