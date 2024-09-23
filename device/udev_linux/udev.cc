// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/udev_linux/udev.h"

#include <stddef.h>

#include "base/strings/string_util.h"
#include "device/udev_linux/udev_loader.h"

namespace device {

namespace {

std::string StringOrEmptyIfNull(const char* value) {
  return value ? value : std::string();
}

}  // namespace

const char* udev_device_get_action(udev_device* udev_device) {
  return UdevLoader::Get()->udev_device_get_action(udev_device);
}

const char* udev_device_get_devnode(udev_device* udev_device) {
  return UdevLoader::Get()->udev_device_get_devnode(udev_device);
}

const char* udev_device_get_devtype(udev_device* udev_device) {
  return UdevLoader::Get()->udev_device_get_devtype(udev_device);
}

udev_device* udev_device_get_parent(udev_device* udev_device) {
  return UdevLoader::Get()->udev_device_get_parent(udev_device);
}

udev_device* udev_device_get_parent_with_subsystem_devtype(
    udev_device* udev_device,
    const char* subsystem,
    const char* devtype) {
  return UdevLoader::Get()->udev_device_get_parent_with_subsystem_devtype(
      udev_device, subsystem, devtype);
}

udev_list_entry* udev_device_get_properties_list_entry(
    struct udev_device* udev_device) {
  return UdevLoader::Get()->udev_device_get_properties_list_entry(udev_device);
}

const char* udev_device_get_property_value(udev_device* udev_device,
                                           const char* key) {
  return UdevLoader::Get()->udev_device_get_property_value(udev_device, key);
}

const char* udev_device_get_subsystem(udev_device* udev_device) {
  return UdevLoader::Get()->udev_device_get_subsystem(udev_device);
}

const char* udev_device_get_sysattr_value(udev_device* udev_device,
                                          const char* sysattr) {
  return UdevLoader::Get()->udev_device_get_sysattr_value(udev_device, sysattr);
}

const char* udev_device_get_sysname(udev_device* udev_device) {
  return UdevLoader::Get()->udev_device_get_sysname(udev_device);
}

const char* udev_device_get_syspath(udev_device* udev_device) {
  return UdevLoader::Get()->udev_device_get_syspath(udev_device);
}

udev_device* udev_device_new_from_devnum(udev* udev, char type, dev_t devnum) {
  return UdevLoader::Get()->udev_device_new_from_devnum(udev, type, devnum);
}

udev_device* udev_device_new_from_subsystem_sysname(
    udev* udev,
    const char* subsystem,
    const char* sysname) {
  return UdevLoader::Get()->udev_device_new_from_subsystem_sysname(
      udev, subsystem, sysname);
}

udev_device* udev_device_new_from_syspath(udev* udev, const char* syspath) {
  return UdevLoader::Get()->udev_device_new_from_syspath(udev, syspath);
}

void udev_device_unref(udev_device* udev_device) {
  UdevLoader::Get()->udev_device_unref(udev_device);
}

int udev_enumerate_add_match_subsystem(udev_enumerate* udev_enumerate,
                                       const char* subsystem) {
  return UdevLoader::Get()->udev_enumerate_add_match_subsystem(udev_enumerate,
                                                               subsystem);
}

udev_list_entry* udev_enumerate_get_list_entry(udev_enumerate* udev_enumerate) {
  return UdevLoader::Get()->udev_enumerate_get_list_entry(udev_enumerate);
}

udev_enumerate* udev_enumerate_new(udev* udev) {
  return UdevLoader::Get()->udev_enumerate_new(udev);
}

int udev_enumerate_scan_devices(udev_enumerate* udev_enumerate) {
  return UdevLoader::Get()->udev_enumerate_scan_devices(udev_enumerate);
}

void udev_enumerate_unref(udev_enumerate* udev_enumerate) {
  UdevLoader::Get()->udev_enumerate_unref(udev_enumerate);
}

udev_list_entry* udev_list_entry_get_next(udev_list_entry* list_entry) {
  return UdevLoader::Get()->udev_list_entry_get_next(list_entry);
}

const char* udev_list_entry_get_name(udev_list_entry* list_entry) {
  return UdevLoader::Get()->udev_list_entry_get_name(list_entry);
}

int udev_monitor_enable_receiving(udev_monitor* udev_monitor) {
  return UdevLoader::Get()->udev_monitor_enable_receiving(udev_monitor);
}

int udev_monitor_filter_add_match_subsystem_devtype(udev_monitor* udev_monitor,
                                                    const char* subsystem,
                                                    const char* devtype) {
  return UdevLoader::Get()->udev_monitor_filter_add_match_subsystem_devtype(
      udev_monitor, subsystem, devtype);
}

int udev_monitor_get_fd(udev_monitor* udev_monitor) {
  return UdevLoader::Get()->udev_monitor_get_fd(udev_monitor);
}

udev_monitor* udev_monitor_new_from_netlink(udev* udev, const char* name) {
  return UdevLoader::Get()->udev_monitor_new_from_netlink(udev, name);
}

udev_device* udev_monitor_receive_device(udev_monitor* udev_monitor) {
  return UdevLoader::Get()->udev_monitor_receive_device(udev_monitor);
}

void udev_monitor_unref(udev_monitor* udev_monitor) {
  UdevLoader::Get()->udev_monitor_unref(udev_monitor);
}

udev* udev_new() {
  return UdevLoader::Get()->udev_new();
}

void udev_unref(udev* udev) {
  UdevLoader::Get()->udev_unref(udev);
}

std::string UdevDeviceGetPropertyValue(udev_device* udev_device,
                                       const char* key) {
  return StringOrEmptyIfNull(udev_device_get_property_value(udev_device, key));
}

std::string UdevDeviceGetSysattrValue(udev_device* udev_device,
                                      const char* key) {
  return StringOrEmptyIfNull(udev_device_get_sysattr_value(udev_device, key));
}

std::string UdevDeviceRecursiveGetSysattrValue(udev_device* udev_device,
                                               const char* key) {
  while (udev_device) {
    const char* result = udev_device_get_sysattr_value(udev_device, key);
    if (result) {
      return result;
    }

    udev_device = udev_device_get_parent(udev_device);
  }

  return "";
}

std::string UdevDecodeString(const std::string& encoded) {
  std::string decoded;
  const size_t size = encoded.size();
  for (size_t i = 0; i < size; ++i) {
    char c = encoded[i];
    if ((i + 3 < size) && c == '\\' && encoded[i + 1] == 'x') {
      c = (base::HexDigitToInt(encoded[i + 2]) << 4) +
          base::HexDigitToInt(encoded[i + 3]);
      i += 3;
    }
    decoded.push_back(c);
  }
  return decoded;
}

}  // namespace device
