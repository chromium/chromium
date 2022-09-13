// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/base/device_monitor_linux.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/threading/scoped_blocking_call.h"
#include "device/udev_linux/udev.h"

namespace device {

namespace {

const char kUdevName[] = "udev";
const char kUdevActionAdd[] = "add";
const char kUdevActionRemove[] = "remove";

// The instance will be reset when message loop destroys.
base::LazyInstance<DeviceMonitorLinux>::Leaky g_device_monitor_linux =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

DeviceMonitorLinux::DeviceMonitorLinux() : monitor_fd_(-1) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  udev_.reset(udev_new());
  if (!udev_) {
    LOG(ERROR) << "Failed to create udev.";
    return;
  }
  monitor_.reset(udev_monitor_new_from_netlink(udev_.get(), kUdevName));
  if (!monitor_) {
    LOG(ERROR) << "Failed to create udev monitor.";
    return;
  }

  int ret = udev_monitor_enable_receiving(monitor_.get());
  if (ret != 0) {
    LOG(ERROR) << "Failed to start udev monitoring.";
    return;
  }

  monitor_fd_ = udev_monitor_get_fd(monitor_.get());
  if (monitor_fd_ <= 0) {
    LOG(ERROR) << "Failed to get udev monitor FD.";
    return;
  }
}

// static
DeviceMonitorLinux* DeviceMonitorLinux::GetInstance() {
  return g_device_monitor_linux.Pointer();
}

void DeviceMonitorLinux::AddObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.AddObserver(observer);

  if (monitor_watch_controller_)
    return;

  monitor_watch_controller_ = base::FileDescriptorWatcher::WatchReadable(
      monitor_fd_,
      base::BindRepeating(&DeviceMonitorLinux::OnMonitorCanReadWithoutBlocking,
                          base::Unretained(this)));
}

void DeviceMonitorLinux::RemoveObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.RemoveObserver(observer);

  if (!observers_.empty())
    return;

  monitor_watch_controller_.reset();
}

void DeviceMonitorLinux::Enumerate(const EnumerateCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  ScopedUdevEnumeratePtr enumerate(udev_enumerate_new(udev_.get()));

  if (!enumerate) {
    LOG(ERROR) << "Failed to enumerate devices.";
    return;
  }

  if (udev_enumerate_scan_devices(enumerate.get()) != 0) {
    LOG(ERROR) << "Failed to enumerate devices.";
    return;
  }

  // This list is managed by |enumerate|.
  udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate.get());
  for (udev_list_entry* i = devices; i != nullptr;
       i = udev_list_entry_get_next(i)) {
    ScopedUdevDevicePtr device(
        udev_device_new_from_syspath(udev_.get(), udev_list_entry_get_name(i)));
    if (device)
      callback.Run(device.get());
  }
}

DeviceMonitorLinux::~DeviceMonitorLinux() {
  // A leaky LazyInstance is never destroyed.
  NOTREACHED();
}

void DeviceMonitorLinux::OnMonitorCanReadWithoutBlocking() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  ScopedUdevDevicePtr device(udev_monitor_receive_device(monitor_.get()));
  if (!device)
    return;

  std::string action(udev_device_get_action(device.get()));
  if (action == kUdevActionAdd) {
    for (auto& observer : observers_)
      observer.OnDeviceAdded(device.get());
  } else if (action == kUdevActionRemove) {
    for (auto& observer : observers_)
      observer.OnDeviceRemoved(device.get());
  }
}

}  // namespace device
