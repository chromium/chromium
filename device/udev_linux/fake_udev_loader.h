// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_UDEV_LINUX_FAKE_UDEV_LOADER_H_
#define DEVICE_UDEV_LINUX_FAKE_UDEV_LOADER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "device/udev_linux/udev_loader.h"

namespace testing {

class FakeUdevLoader : public device::UdevLoader {
 public:
  FakeUdevLoader();
  FakeUdevLoader(const FakeUdevLoader& other) = delete;
  FakeUdevLoader& operator=(const FakeUdevLoader& other) = delete;
  ~FakeUdevLoader() override;

  udev_device* AddFakeDevice(std::string name,
                             std::string syspath,
                             std::string subsystem,
                             std::optional<std::string> devnode,
                             std::optional<std::string> devtype,
                             std::map<std::string, std::string> sysattrs,
                             std::map<std::string, std::string> properties);

  void Reset();

 private:
  const char* udev_device_get_action(udev_device* device) override;
  const char* udev_device_get_devnode(udev_device* device) override;
  const char* udev_device_get_devtype(udev_device* device) override;
  udev_device* udev_device_get_parent(udev_device* device) override;
  udev_device* udev_device_get_parent_with_subsystem_devtype(
      udev_device* device,
      const char* subsystem,
      const char* devtype) override;
  udev_list_entry* udev_device_get_properties_list_entry(
      struct udev_device* device) override;
  const char* udev_device_get_property_value(udev_device* device,
                                             const char* key) override;
  const char* udev_device_get_subsystem(udev_device* device) override;
  const char* udev_device_get_sysattr_value(udev_device* device,
                                            const char* sysattr) override;
  const char* udev_device_get_sysname(udev_device* device) override;
  const char* udev_device_get_syspath(udev_device* device) override;
  udev_device* udev_device_new_from_devnum(udev* udev_context,
                                           char type,
                                           dev_t devnum) override;
  udev_device* udev_device_new_from_subsystem_sysname(
      udev* udev_context,
      const char* subsystem,
      const char* sysname) override;
  udev_device* udev_device_new_from_syspath(udev* udev_context,
                                            const char* syspath) override;
  void udev_device_unref(udev_device* device) override;
  int udev_enumerate_add_match_subsystem(udev_enumerate* enumeration_context,
                                         const char* subsystem) override;
  udev_list_entry* udev_enumerate_get_list_entry(
      udev_enumerate* enumeration_context) override;
  udev_enumerate* udev_enumerate_new(udev* udev_context) override;
  int udev_enumerate_scan_devices(udev_enumerate* enumeration_context) override;
  void udev_enumerate_unref(udev_enumerate* enumeration_context) override;
  udev_list_entry* udev_list_entry_get_next(
      udev_list_entry* list_entry) override;
  const char* udev_list_entry_get_name(udev_list_entry* list_entry) override;
  int udev_monitor_enable_receiving(udev_monitor* monitor) override;
  int udev_monitor_filter_add_match_subsystem_devtype(
      udev_monitor* monitor,
      const char* subsystem,
      const char* devtype) override;
  int udev_monitor_get_fd(udev_monitor* monitor) override;
  udev_monitor* udev_monitor_new_from_netlink(udev* udev_context,
                                              const char* name) override;
  udev_device* udev_monitor_receive_device(udev_monitor* monitor) override;
  void udev_monitor_unref(udev_monitor* monitor) override;
  udev* udev_new() override;
  void udev_unref(udev* udev_context) override;

  std::vector<std::unique_ptr<udev_device>> devices_;
};

}  // namespace testing

#endif  // DEVICE_UDEV_LINUX_FAKE_UDEV_LOADER_H_
