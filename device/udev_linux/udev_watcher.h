// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_UDEV_LINUX_UDEV_WATCHER_H_
#define DEVICE_UDEV_LINUX_UDEV_WATCHER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "device/udev_linux/scoped_udev.h"

namespace device {

// This class wraps an instance of udev_monitor, watching for devices that are
// added and removed from the system. This class has sequence affinity.
class COMPONENT_EXPORT(DEVICE_UDEV_LINUX) UdevWatcher {
 public:
  class Observer {
   public:
    virtual void OnDeviceAdded(ScopedUdevDevicePtr device) = 0;
    virtual void OnDeviceRemoved(ScopedUdevDevicePtr device) = 0;
    virtual void OnDeviceChanged(ScopedUdevDevicePtr device) = 0;

   protected:
    virtual ~Observer();
  };

  // subsystem and devtype parameter for
  // udev_monitor_filter_add_match_subsystem_devtype().
  class Filter {
   public:
    Filter(std::string_view subsystem_in, std::string_view devtype_in);
    Filter(const Filter&);
    ~Filter();

    const char* devtype() const;
    const char* subsystem() const;

   private:
    std::optional<std::string> subsystem_;
    std::optional<std::string> devtype_;
  };

  static std::unique_ptr<UdevWatcher> StartWatching(
      Observer* observer,
      const std::vector<Filter>& filters = {});

  UdevWatcher(const UdevWatcher&) = delete;
  UdevWatcher& operator=(const UdevWatcher&) = delete;

  ~UdevWatcher();

  // Synchronously enumerates the all devices known to udev, calling
  // OnDeviceAdded on the provided Observer for each.
  void EnumerateExistingDevices();

 private:
  UdevWatcher(ScopedUdevPtr udev,
              ScopedUdevMonitorPtr udev_monitor,
              int monitor_fd,
              Observer* observer,
              const std::vector<Filter>& filters);

  void OnMonitorReadable();

  ScopedUdevPtr udev_;
  ScopedUdevMonitorPtr udev_monitor_;
  raw_ptr<Observer> observer_;
  const std::vector<Filter> udev_filters_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> file_watcher_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace device

#endif  // DEVICE_UDEV_LINUX_UDEV_WATCHER_H_
