// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_UDEV_LINUX_SCOPED_UDEV_H_
#define DEVICE_UDEV_LINUX_SCOPED_UDEV_H_

#include <memory>

#include "device/udev_linux/udev.h"

#if !defined(USE_UDEV)
#error "USE_UDEV not defined"
#endif

namespace device {

struct UdevDeleter {
  void operator()(udev* dev) const {
    udev_unref(dev);
  }
};
struct UdevEnumerateDeleter {
  void operator()(udev_enumerate* enumerate) const {
    udev_enumerate_unref(enumerate);
  }
};
struct UdevDeviceDeleter {
  void operator()(udev_device* device) const {
    udev_device_unref(device);
  }
};
struct UdevMonitorDeleter {
  void operator()(udev_monitor* monitor) const {
    udev_monitor_unref(monitor);
  }
};

typedef std::unique_ptr<udev, UdevDeleter> ScopedUdevPtr;
typedef std::unique_ptr<udev_enumerate, UdevEnumerateDeleter>
    ScopedUdevEnumeratePtr;
typedef std::unique_ptr<udev_device, UdevDeviceDeleter> ScopedUdevDevicePtr;
typedef std::unique_ptr<udev_monitor, UdevMonitorDeleter> ScopedUdevMonitorPtr;

}  // namespace device

#endif  // DEVICE_UDEV_LINUX_SCOPED_UDEV_H_
