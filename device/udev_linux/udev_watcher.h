// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_UDEV_LINUX_UDEV_WATCHER_H_
#define DEVICE_UDEV_LINUX_UDEV_WATCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "device/udev_linux/scoped_udev.h"

namespace device {

// This class wraps an instance of udev_monitor, watching for devices that are
// added and removed from the system. This class has sequence affinity.
class UdevWatcher {
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
    Filter(base::StringPiece subsystem_in, base::StringPiece devtype_in);
    Filter(const Filter&);
    ~Filter();

    const char* devtype() const;
    const char* subsystem() const;

   private:
    base::Optional<std::string> subsystem_;
    base::Optional<std::string> devtype_;
  };

  static std::unique_ptr<UdevWatcher> StartWatching(
      Observer* observer,
      const std::vector<Filter>& filters = {});

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
  Observer* observer_;
  const std::vector<Filter> udev_filters_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> file_watcher_;
  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(UdevWatcher);
};

}  // namespace device

#endif  // DEVICE_UDEV_LINUX_UDEV_WATCHER_H_
