// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BASE_DEVICE_MONITOR_LINUX_H_
#define DEVICE_BASE_DEVICE_MONITOR_LINUX_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "device/base/device_base_export.h"
#include "device/udev_linux/scoped_udev.h"

struct udev_device;

namespace device {

// This class listends for notifications from libudev about
// connected/disconnected devices. This class is *NOT* thread-safe.
class DEVICE_BASE_EXPORT DeviceMonitorLinux {
 public:
  typedef base::RepeatingCallback<void(udev_device* device)> EnumerateCallback;

  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnDeviceAdded(udev_device* device) = 0;
    virtual void OnDeviceRemoved(udev_device* device) = 0;
  };

  DeviceMonitorLinux();

  DeviceMonitorLinux(const DeviceMonitorLinux&) = delete;
  DeviceMonitorLinux& operator=(const DeviceMonitorLinux&) = delete;

  static DeviceMonitorLinux* GetInstance();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void Enumerate(const EnumerateCallback& callback);

 private:
  friend std::default_delete<DeviceMonitorLinux>;

  ~DeviceMonitorLinux();

  void OnMonitorCanReadWithoutBlocking();

  ScopedUdevPtr udev_;
  ScopedUdevMonitorPtr monitor_;
  int monitor_fd_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller>
      monitor_watch_controller_;

  base::ObserverList<Observer, true>::Unchecked observers_;

  base::ThreadChecker thread_checker_;
};

}  // namespace device

#endif  // DEVICE_BASE_DEVICE_MONITOR_LINUX_H_
