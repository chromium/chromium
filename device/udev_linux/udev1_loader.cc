// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/udev_linux/udev1_loader.h"

#include <libudev.h>

namespace device {

Udev1Loader::Udev1Loader() = default;

Udev1Loader::~Udev1Loader() = default;

const char* Udev1Loader::udev_device_get_action(udev_device* udev_device) {
  return ::udev_device_get_action(udev_device);
}

const char* Udev1Loader::udev_device_get_devnode(udev_device* udev_device) {
  return ::udev_device_get_devnode(udev_device);
}

const char* Udev1Loader::udev_device_get_devtype(udev_device* udev_device) {
  return ::udev_device_get_devtype(udev_device);
}

udev_device* Udev1Loader::udev_device_get_parent(udev_device* udev_device) {
  return ::udev_device_get_parent(udev_device);
}

udev_device* Udev1Loader::udev_device_get_parent_with_subsystem_devtype(
    udev_device* udev_device,
    const char* subsystem,
    const char* devtype) {
  return ::udev_device_get_parent_with_subsystem_devtype(udev_device, subsystem,
                                                         devtype);
}

udev_list_entry* Udev1Loader::udev_device_get_properties_list_entry(
    struct udev_device* udev_device) {
  return ::udev_device_get_properties_list_entry(udev_device);
}

const char* Udev1Loader::udev_device_get_property_value(
    udev_device* udev_device,
    const char* key) {
  return ::udev_device_get_property_value(udev_device, key);
}

const char* Udev1Loader::udev_device_get_subsystem(udev_device* udev_device) {
  return ::udev_device_get_subsystem(udev_device);
}

const char* Udev1Loader::udev_device_get_sysattr_value(udev_device* udev_device,
                                                       const char* sysattr) {
  return ::udev_device_get_sysattr_value(udev_device, sysattr);
}

const char* Udev1Loader::udev_device_get_sysname(udev_device* udev_device) {
  return ::udev_device_get_sysname(udev_device);
}

const char* Udev1Loader::udev_device_get_syspath(udev_device* udev_device) {
  return ::udev_device_get_syspath(udev_device);
}

udev_device* Udev1Loader::udev_device_new_from_devnum(udev* udev,
                                                      char type,
                                                      dev_t devnum) {
  return ::udev_device_new_from_devnum(udev, type, devnum);
}

udev_device* Udev1Loader::udev_device_new_from_subsystem_sysname(
    udev* udev,
    const char* subsystem,
    const char* sysname) {
  return ::udev_device_new_from_subsystem_sysname(udev, subsystem, sysname);
}

udev_device* Udev1Loader::udev_device_new_from_syspath(udev* udev,
                                                       const char* syspath) {
  return ::udev_device_new_from_syspath(udev, syspath);
}

void Udev1Loader::udev_device_unref(udev_device* udev_device) {
  ::udev_device_unref(udev_device);
}

int Udev1Loader::udev_enumerate_add_match_subsystem(
    udev_enumerate* udev_enumerate,
    const char* subsystem) {
  return ::udev_enumerate_add_match_subsystem(udev_enumerate, subsystem);
}

udev_list_entry* Udev1Loader::udev_enumerate_get_list_entry(
    udev_enumerate* udev_enumerate) {
  return ::udev_enumerate_get_list_entry(udev_enumerate);
}

udev_enumerate* Udev1Loader::udev_enumerate_new(udev* udev) {
  return ::udev_enumerate_new(udev);
}

int Udev1Loader::udev_enumerate_scan_devices(udev_enumerate* udev_enumerate) {
  return ::udev_enumerate_scan_devices(udev_enumerate);
}

void Udev1Loader::udev_enumerate_unref(udev_enumerate* udev_enumerate) {
  ::udev_enumerate_unref(udev_enumerate);
}

udev_list_entry* Udev1Loader::udev_list_entry_get_next(
    udev_list_entry* list_entry) {
  return ::udev_list_entry_get_next(list_entry);
}

const char* Udev1Loader::udev_list_entry_get_name(udev_list_entry* list_entry) {
  return ::udev_list_entry_get_name(list_entry);
}

int Udev1Loader::udev_monitor_enable_receiving(udev_monitor* udev_monitor) {
  return ::udev_monitor_enable_receiving(udev_monitor);
}

int Udev1Loader::udev_monitor_filter_add_match_subsystem_devtype(
    udev_monitor* udev_monitor,
    const char* subsystem,
    const char* devtype) {
  return ::udev_monitor_filter_add_match_subsystem_devtype(udev_monitor,
                                                           subsystem, devtype);
}

int Udev1Loader::udev_monitor_get_fd(udev_monitor* udev_monitor) {
  return ::udev_monitor_get_fd(udev_monitor);
}

udev_monitor* Udev1Loader::udev_monitor_new_from_netlink(udev* udev,
                                                         const char* name) {
  return ::udev_monitor_new_from_netlink(udev, name);
}

udev_device* Udev1Loader::udev_monitor_receive_device(
    udev_monitor* udev_monitor) {
  return ::udev_monitor_receive_device(udev_monitor);
}

void Udev1Loader::udev_monitor_unref(udev_monitor* udev_monitor) {
  ::udev_monitor_unref(udev_monitor);
}

udev* Udev1Loader::udev_new() {
  return ::udev_new();
}

void Udev1Loader::udev_unref(udev* udev) {
  ::udev_unref(udev);
}

}  // namespace device
