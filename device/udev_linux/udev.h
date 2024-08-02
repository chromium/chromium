// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_UDEV_LINUX_UDEV_H_
#define DEVICE_UDEV_LINUX_UDEV_H_

#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string>
#include "base/component_export.h"

#if !defined(USE_UDEV)
#error "USE_UDEV not defined"
#endif

// Adapted from libudev.h.
#define udev_list_entry_foreach(list_entry, first_entry) \
  for (list_entry = first_entry; list_entry != NULL;     \
       list_entry = ::device::udev_list_entry_get_next(list_entry))

// Forward declarations of opaque structs.
struct udev;
struct udev_device;
struct udev_enumerate;
struct udev_list_entry;
struct udev_monitor;

namespace device {

COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
const char* udev_device_get_action(udev_device* udev_device);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
const char* udev_device_get_devnode(udev_device* udev_device);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
const char* udev_device_get_devtype(udev_device* udev_device);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
udev_device* udev_device_get_parent(udev_device* udev_device);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
udev_device* udev_device_get_parent_with_subsystem_devtype(
    udev_device* udev_device,
    const char* subsystem,
    const char* devtype);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
udev_list_entry* udev_device_get_properties_list_entry(
    struct udev_device* udev_device);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
const char* udev_device_get_property_value(udev_device* udev_device,
                                           const char* key);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
const char* udev_device_get_subsystem(udev_device* udev_device);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
const char* udev_device_get_sysattr_value(udev_device* udev_device,
                                          const char* sysattr);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
const char* udev_device_get_sysname(udev_device* udev_device);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
const char* udev_device_get_syspath(udev_device* udev_device);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
udev_device* udev_device_new_from_devnum(udev* udev, char type, dev_t devnum);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
udev_device* udev_device_new_from_subsystem_sysname(udev* udev,
                                                    const char* subsystem,
                                                    const char* sysname);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
udev_device* udev_device_new_from_syspath(udev* udev, const char* syspath);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
void udev_device_unref(udev_device* udev_device);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
int udev_enumerate_add_match_subsystem(udev_enumerate* udev_enumerate,
                                       const char* subsystem);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
udev_list_entry* udev_enumerate_get_list_entry(udev_enumerate* udev_enumerate);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
udev_enumerate* udev_enumerate_new(udev* udev);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
int udev_enumerate_scan_devices(udev_enumerate* udev_enumerate);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
void udev_enumerate_unref(udev_enumerate* udev_enumerate);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
udev_list_entry* udev_list_entry_get_next(udev_list_entry* list_entry);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
const char* udev_list_entry_get_name(udev_list_entry* list_entry);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
int udev_monitor_enable_receiving(udev_monitor* udev_monitor);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
int udev_monitor_filter_add_match_subsystem_devtype(udev_monitor* udev_monitor,
                                                    const char* subsystem,
                                                    const char* devtype);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
int udev_monitor_get_fd(udev_monitor* udev_monitor);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
udev_monitor* udev_monitor_new_from_netlink(udev* udev, const char* name);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
udev_device* udev_monitor_receive_device(udev_monitor* udev_monitor);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
void udev_monitor_unref(udev_monitor* udev_monitor);
COMPONENT_EXPORT(DEVICE_UDEV_LINUX) udev* udev_new();
COMPONENT_EXPORT(DEVICE_UDEV_LINUX) void udev_unref(udev* udev);

// Calls udev_device_get_property_value() and replaces missing values with
// the empty string.
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
std::string UdevDeviceGetPropertyValue(udev_device* udev_device,
                                       const char* key);

// Calls udev_device_get_sysattr_value() and replaces missing values with
// the empty string.
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
std::string UdevDeviceGetSysattrValue(udev_device* udev_device,
                                      const char* key);

// Walks up the chain of parent devices calling udev_device_get_sysattr_value()
// until a value is found. If no value is found, an empty string is returned.
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
std::string UdevDeviceRecursiveGetSysattrValue(udev_device* udev_device,
                                               const char* key);

// Decodes udev-encoded string. Useful for decoding "*_ENC" udev properties.
COMPONENT_EXPORT(DEVICE_UDEV_LINUX)
std::string UdevDecodeString(const std::string& encoded);

}  // namespace device

#endif  // DEVICE_UDEV_LINUX_UDEV_H_
