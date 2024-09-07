// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_UDEV_LINUX_UDEV1_LOADER_H_
#define DEVICE_UDEV_LINUX_UDEV1_LOADER_H_

#include "device/udev_linux/udev_loader.h"

namespace device {

class COMPONENT_EXPORT(DEVICE_UDEV_LINUX) Udev1Loader : public UdevLoader {
 public:
  Udev1Loader();

  Udev1Loader(const Udev1Loader&) = delete;
  Udev1Loader& operator=(const Udev1Loader&) = delete;

  ~Udev1Loader() override;

 private:
  const char* udev_device_get_action(udev_device* udev_device) override;
  const char* udev_device_get_devnode(udev_device* udev_device) override;
  const char* udev_device_get_devtype(udev_device* udev_device) override;
  udev_device* udev_device_get_parent(udev_device* udev_device) override;
  udev_device* udev_device_get_parent_with_subsystem_devtype(
      udev_device* udev_device,
      const char* subsystem,
      const char* devtype) override;
  udev_list_entry* udev_device_get_properties_list_entry(
      struct udev_device* udev_device) override;
  const char* udev_device_get_property_value(udev_device* udev_device,
                                             const char* key) override;
  const char* udev_device_get_subsystem(udev_device* udev_device) override;
  const char* udev_device_get_sysattr_value(udev_device* udev_device,
                                            const char* sysattr) override;
  const char* udev_device_get_sysname(udev_device* udev_device) override;
  const char* udev_device_get_syspath(udev_device* udev_device) override;
  udev_device* udev_device_new_from_devnum(udev* udev,
                                           char type,
                                           dev_t devnum) override;
  udev_device* udev_device_new_from_subsystem_sysname(
      udev* udev,
      const char* subsystem,
      const char* sysname) override;
  udev_device* udev_device_new_from_syspath(udev* udev,
                                            const char* syspath) override;
  void udev_device_unref(udev_device* udev_device) override;
  int udev_enumerate_add_match_subsystem(udev_enumerate* udev_enumerate,
                                         const char* subsystem) override;
  udev_list_entry* udev_enumerate_get_list_entry(
      udev_enumerate* udev_enumerate) override;
  udev_enumerate* udev_enumerate_new(udev* udev) override;
  int udev_enumerate_scan_devices(udev_enumerate* udev_enumerate) override;
  void udev_enumerate_unref(udev_enumerate* udev_enumerate) override;
  udev_list_entry* udev_list_entry_get_next(
      udev_list_entry* list_entry) override;
  const char* udev_list_entry_get_name(udev_list_entry* list_entry) override;
  int udev_monitor_enable_receiving(udev_monitor* udev_monitor) override;
  int udev_monitor_filter_add_match_subsystem_devtype(
      udev_monitor* udev_monitor,
      const char* subsystem,
      const char* devtype) override;
  int udev_monitor_get_fd(udev_monitor* udev_monitor) override;
  udev_monitor* udev_monitor_new_from_netlink(udev* udev,
                                              const char* name) override;
  udev_device* udev_monitor_receive_device(udev_monitor* udev_monitor) override;
  void udev_monitor_unref(udev_monitor* udev_monitor) override;
  udev* udev_new() override;
  void udev_unref(udev* udev) override;
};

}  // namespace device

#endif  // DEVICE_UDEV_LINUX_UDEV1_LOADER_H_
