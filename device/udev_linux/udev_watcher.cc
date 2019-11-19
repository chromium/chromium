// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/udev_linux/udev_watcher.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/threading/scoped_blocking_call.h"

namespace device {

UdevWatcher::Filter::Filter(base::StringPiece subsystem_in,
                            base::StringPiece devtype_in) {
  if (subsystem_in.data())
    subsystem_ = subsystem_in.as_string();
  if (devtype_in.data())
    devtype_ = devtype_in.as_string();
}

UdevWatcher::Filter::Filter(const Filter&) = default;
UdevWatcher::Filter::~Filter() = default;

const char* UdevWatcher::Filter::devtype() const {
  return devtype_ ? devtype_.value().c_str() : nullptr;
}

const char* UdevWatcher::Filter::subsystem() const {
  return subsystem_ ? subsystem_.value().c_str() : nullptr;
}

UdevWatcher::Observer::~Observer() = default;

std::unique_ptr<UdevWatcher> UdevWatcher::StartWatching(
    Observer* observer,
    const std::vector<Filter>& filters) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  ScopedUdevPtr udev(udev_new());
  if (!udev) {
    LOG(ERROR) << "Failed to initialize udev.";
    return nullptr;
  }

  ScopedUdevMonitorPtr udev_monitor(
      udev_monitor_new_from_netlink(udev.get(), "udev"));
  if (!udev_monitor) {
    LOG(ERROR) << "Failed to initialize a udev monitor.";
    return nullptr;
  }

  for (const Filter& filter : filters) {
    const int ret = udev_monitor_filter_add_match_subsystem_devtype(
        udev_monitor.get(), filter.subsystem(), filter.devtype());
    CHECK_EQ(0, ret);
  }

  if (udev_monitor_enable_receiving(udev_monitor.get()) != 0) {
    LOG(ERROR) << "Failed to enable receiving udev events.";
    return nullptr;
  }

  int monitor_fd = udev_monitor_get_fd(udev_monitor.get());
  if (monitor_fd < 0) {
    LOG(ERROR) << "Udev monitor file descriptor unavailable.";
    return nullptr;
  }

  return base::WrapUnique(new UdevWatcher(
      std::move(udev), std::move(udev_monitor), monitor_fd, observer, filters));
}

UdevWatcher::~UdevWatcher() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
}

void UdevWatcher::EnumerateExistingDevices() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  ScopedUdevEnumeratePtr enumerate(udev_enumerate_new(udev_.get()));
  if (!enumerate) {
    LOG(ERROR) << "Failed to initialize a udev enumerator.";
    return;
  }

  for (const Filter& filter : udev_filters_) {
    const int ret =
        udev_enumerate_add_match_subsystem(enumerate.get(), filter.subsystem());
    CHECK_EQ(0, ret);
  }

  if (udev_enumerate_scan_devices(enumerate.get()) != 0) {
    LOG(ERROR) << "Failed to begin udev enumeration.";
    return;
  }

  udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate.get());
  for (udev_list_entry* i = devices; i != nullptr;
       i = udev_list_entry_get_next(i)) {
    ScopedUdevDevicePtr device(
        udev_device_new_from_syspath(udev_.get(), udev_list_entry_get_name(i)));
    if (device)
      observer_->OnDeviceAdded(std::move(device));
  }
}

UdevWatcher::UdevWatcher(ScopedUdevPtr udev,
                         ScopedUdevMonitorPtr udev_monitor,
                         int monitor_fd,
                         Observer* observer,
                         const std::vector<Filter>& filters)
    : udev_(std::move(udev)),
      udev_monitor_(std::move(udev_monitor)),
      observer_(observer),
      udev_filters_(filters) {
  file_watcher_ = base::FileDescriptorWatcher::WatchReadable(
      monitor_fd, base::BindRepeating(&UdevWatcher::OnMonitorReadable,
                                      base::Unretained(this)));
}

void UdevWatcher::OnMonitorReadable() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  ScopedUdevDevicePtr device(udev_monitor_receive_device(udev_monitor_.get()));
  if (!device)
    return;

  std::string action(udev_device_get_action(device.get()));
  if (action == "add")
    observer_->OnDeviceAdded(std::move(device));
  else if (action == "remove")
    observer_->OnDeviceRemoved(std::move(device));
  else if (action == "change")
    observer_->OnDeviceChanged(std::move(device));
  else
    DVLOG(1) << "Unknown udev action: " << action;
}

}  // namespace device
