// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_UDEV_LINUX_UDEV_LOADER_H_
#define DEVICE_UDEV_LINUX_UDEV_LOADER_H_

#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "base/component_export.h"

#if !defined(USE_UDEV)
#error "USE_UDEV not defined"
#endif

struct udev;
struct udev_device;
struct udev_enumerate;
struct udev_list_entry;
struct udev_monitor;

namespace device {

// Interface to libudev. Accessed through the static Get() function, which
// will try to load libudev1. If the libraries does not load successfully, the
// program will fail with a crash.
//
// All the methods have the same signatures as libudev's functions. e.g.
// udev_monitor_get_fd(mon) simply becomes device::udev_monitor_get_fd(mon).
class COMPONENT_EXPORT(DEVICE_UDEV_LINUX) UdevLoader {
 public:
  static UdevLoader* Get();

  // Allows to set a particular implementation of the loader.
  // Given the shape of the existing API, bad things will happen if one gets
  // a udev_device instance, switches to a new loader, and tries to use that
  // udev_instance again. The expectation is that when running unit tests,
  // before we switch to fake udev, all attempts to interact with real udev
  // will fail.
  static void SetForTesting(UdevLoader* loader, bool delete_previous = true);

  virtual ~UdevLoader();

  virtual const char* udev_device_get_action(udev_device* udev_device) = 0;
  virtual const char* udev_device_get_devnode(udev_device* udev_device) = 0;
  virtual const char* udev_device_get_devtype(udev_device* udev_device) = 0;
  virtual udev_device* udev_device_get_parent(udev_device* udev_device) = 0;
  virtual udev_device* udev_device_get_parent_with_subsystem_devtype(
      udev_device* udev_device,
      const char* subsystem,
      const char* devtype) = 0;
  virtual udev_list_entry* udev_device_get_properties_list_entry(
      struct udev_device* udev_device) = 0;
  virtual const char* udev_device_get_property_value(udev_device* udev_device,
                                                     const char* key) = 0;
  virtual const char* udev_device_get_subsystem(udev_device* udev_device) = 0;
  virtual const char* udev_device_get_sysattr_value(udev_device* udev_device,
                                                    const char* sysattr) = 0;
  virtual const char* udev_device_get_sysname(udev_device* udev_device) = 0;
  virtual const char* udev_device_get_syspath(udev_device* udev_device) = 0;
  virtual udev_device* udev_device_new_from_devnum(udev* udev,
                                                   char type,
                                                   dev_t devnum) = 0;
  virtual udev_device* udev_device_new_from_subsystem_sysname(
      udev* udev,
      const char* subsystem,
      const char* sysname) = 0;
  virtual udev_device* udev_device_new_from_syspath(udev* udev,
                                                    const char* syspath) = 0;
  virtual void udev_device_unref(udev_device* udev_device) = 0;
  virtual int udev_enumerate_add_match_subsystem(udev_enumerate* udev_enumerate,
                                                 const char* subsystem) = 0;
  virtual udev_list_entry* udev_enumerate_get_list_entry(
      udev_enumerate* udev_enumerate) = 0;
  virtual udev_enumerate* udev_enumerate_new(udev* udev) = 0;
  virtual int udev_enumerate_scan_devices(udev_enumerate* udev_enumerate) = 0;
  virtual void udev_enumerate_unref(udev_enumerate* udev_enumerate) = 0;
  virtual udev_list_entry* udev_list_entry_get_next(
      udev_list_entry* list_entry) = 0;
  virtual const char* udev_list_entry_get_name(udev_list_entry* list_entry) = 0;
  virtual int udev_monitor_enable_receiving(udev_monitor* udev_monitor) = 0;
  virtual int udev_monitor_filter_add_match_subsystem_devtype(
      udev_monitor* udev_monitor,
      const char* subsystem,
      const char* devtype) = 0;
  virtual int udev_monitor_get_fd(udev_monitor* udev_monitor) = 0;
  virtual udev_monitor* udev_monitor_new_from_netlink(udev* udev,
                                                      const char* name) = 0;
  virtual udev_device* udev_monitor_receive_device(
      udev_monitor* udev_monitor) = 0;
  virtual void udev_monitor_unref(udev_monitor* udev_monitor) = 0;
  virtual udev* udev_new() = 0;
  virtual void udev_unref(udev* udev) = 0;
};

}  // namespace device

#endif  // DEVICE_UDEV_LINUX_UDEV_LOADER_H_
