// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_UDEV_LINUX_UDEV_H_
#define DEVICE_UDEV_LINUX_UDEV_H_

#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string>

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

const char* udev_device_get_action(udev_device* udev_device);
const char* udev_device_get_devnode(udev_device* udev_device);
const char* udev_device_get_devtype(udev_device* udev_device);
udev_device* udev_device_get_parent(udev_device* udev_device);
udev_device* udev_device_get_parent_with_subsystem_devtype(
    udev_device* udev_device,
    const char* subsystem,
    const char* devtype);
const char* udev_device_get_property_value(udev_device* udev_device,
                                           const char* key);
const char* udev_device_get_subsystem(udev_device* udev_device);
const char* udev_device_get_sysattr_value(udev_device* udev_device,
                                          const char* sysattr);
const char* udev_device_get_sysname(udev_device* udev_device);
const char* udev_device_get_syspath(udev_device* udev_device);
udev_device* udev_device_new_from_devnum(udev* udev, char type, dev_t devnum);
udev_device* udev_device_new_from_subsystem_sysname(
    udev* udev,
    const char* subsystem,
    const char* sysname);
udev_device* udev_device_new_from_syspath(udev* udev, const char* syspath);
void udev_device_unref(udev_device* udev_device);
int udev_enumerate_add_match_subsystem(udev_enumerate* udev_enumerate,
                                       const char* subsystem);
udev_list_entry* udev_enumerate_get_list_entry(udev_enumerate* udev_enumerate);
udev_enumerate* udev_enumerate_new(udev* udev);
int udev_enumerate_scan_devices(udev_enumerate* udev_enumerate);
void udev_enumerate_unref(udev_enumerate* udev_enumerate);
udev_list_entry* udev_list_entry_get_next(udev_list_entry* list_entry);
const char* udev_list_entry_get_name(udev_list_entry* list_entry);
int udev_monitor_enable_receiving(udev_monitor* udev_monitor);
int udev_monitor_filter_add_match_subsystem_devtype(udev_monitor* udev_monitor,
                                                    const char* subsystem,
                                                    const char* devtype);
int udev_monitor_get_fd(udev_monitor* udev_monitor);
udev_monitor* udev_monitor_new_from_netlink(udev* udev, const char* name);
udev_device* udev_monitor_receive_device(udev_monitor* udev_monitor);
void udev_monitor_unref(udev_monitor* udev_monitor);
udev* udev_new();
void udev_set_log_fn(
    struct udev* udev,
    void (*log_fn)(struct udev* udev, int priority, const char* file, int line,
                   const char* fn, const char* format, va_list args));
void udev_set_log_priority(struct udev* udev, int priority);
void udev_unref(udev* udev);

// Calls udev_device_get_property_value() and replaces missing values with
// the empty string.
std::string UdevDeviceGetPropertyValue(udev_device* udev_device,
                                       const char* key);

// Calls udev_device_get_sysattr_value() and replaces missing values with
// the empty string.
std::string UdevDeviceGetSysattrValue(udev_device* udev_device,
                                      const char* key);

// Walks up the chain of parent devices calling udev_device_get_sysattr_value()
// until a value is found. If no value is found, an empty string is returned.
std::string UdevDeviceRecursiveGetSysattrValue(udev_device* udev_device,
                                               const char* key);

// Decodes udev-encoded string. Useful for decoding "*_ENC" udev properties.
std::string UdevDecodeString(const std::string& encoded);

}  // namespace device

#endif  // DEVICE_UDEV_LINUX_UDEV_H_
