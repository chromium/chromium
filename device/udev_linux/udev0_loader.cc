// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/udev_linux/udev0_loader.h"

#include <memory>

#include "base/debug/dump_without_crashing.h"
#include "library_loaders/libudev0.h"

namespace device {

Udev0Loader::Udev0Loader() = default;

Udev0Loader::~Udev0Loader() = default;

bool Udev0Loader::Init() {
  if (lib_loader_)
    return lib_loader_->loaded();
  lib_loader_ = std::make_unique<LibUdev0Loader>();
  // TODO(crbug.com/1237497): Remove the next line if it triggers. Remove this
  // entire file if it does not.
  base::debug::DumpWithoutCrashing();
  return lib_loader_->Load("libudev.so.0");
}

const char* Udev0Loader::udev_device_get_action(udev_device* udev_device) {
  return lib_loader_->udev_device_get_action(udev_device);
}

const char* Udev0Loader::udev_device_get_devnode(udev_device* udev_device) {
  return lib_loader_->udev_device_get_devnode(udev_device);
}

const char* Udev0Loader::udev_device_get_devtype(udev_device* udev_device) {
  return lib_loader_->udev_device_get_devtype(udev_device);
}

udev_device* Udev0Loader::udev_device_get_parent(udev_device* udev_device) {
  return lib_loader_->udev_device_get_parent(udev_device);
}

udev_device* Udev0Loader::udev_device_get_parent_with_subsystem_devtype(
    udev_device* udev_device,
    const char* subsystem,
    const char* devtype) {
  return lib_loader_->udev_device_get_parent_with_subsystem_devtype(
      udev_device, subsystem, devtype);
}

udev_list_entry* Udev0Loader::udev_device_get_properties_list_entry(
    struct udev_device* udev_device) {
  return lib_loader_->udev_device_get_properties_list_entry(udev_device);
}

const char* Udev0Loader::udev_device_get_property_value(
    udev_device* udev_device,
    const char* key) {
  return lib_loader_->udev_device_get_property_value(udev_device, key);
}

const char* Udev0Loader::udev_device_get_subsystem(udev_device* udev_device) {
  return lib_loader_->udev_device_get_subsystem(udev_device);
}

const char* Udev0Loader::udev_device_get_sysattr_value(udev_device* udev_device,
                                                       const char* sysattr) {
  return lib_loader_->udev_device_get_sysattr_value(udev_device, sysattr);
}

const char* Udev0Loader::udev_device_get_sysname(udev_device* udev_device) {
  return lib_loader_->udev_device_get_sysname(udev_device);
}

const char* Udev0Loader::udev_device_get_syspath(udev_device* udev_device) {
  return lib_loader_->udev_device_get_syspath(udev_device);
}

udev_device* Udev0Loader::udev_device_new_from_devnum(udev* udev,
                                                      char type,
                                                      dev_t devnum) {
  return lib_loader_->udev_device_new_from_devnum(udev, type, devnum);
}

udev_device* Udev0Loader::udev_device_new_from_subsystem_sysname(
    udev* udev,
    const char* subsystem,
    const char* sysname) {
  return lib_loader_->udev_device_new_from_subsystem_sysname(
      udev, subsystem, sysname);
}

udev_device* Udev0Loader::udev_device_new_from_syspath(udev* udev,
                                                       const char* syspath) {
  return lib_loader_->udev_device_new_from_syspath(udev, syspath);
}

void Udev0Loader::udev_device_unref(udev_device* udev_device) {
  lib_loader_->udev_device_unref(udev_device);
}

int Udev0Loader::udev_enumerate_add_match_subsystem(
    udev_enumerate* udev_enumerate,
    const char* subsystem) {
  return lib_loader_->udev_enumerate_add_match_subsystem(udev_enumerate,
                                                         subsystem);
}

udev_list_entry* Udev0Loader::udev_enumerate_get_list_entry(
    udev_enumerate* udev_enumerate) {
  return lib_loader_->udev_enumerate_get_list_entry(udev_enumerate);
}

udev_enumerate* Udev0Loader::udev_enumerate_new(udev* udev) {
  return lib_loader_->udev_enumerate_new(udev);
}

int Udev0Loader::udev_enumerate_scan_devices(udev_enumerate* udev_enumerate) {
  return lib_loader_->udev_enumerate_scan_devices(udev_enumerate);
}

void Udev0Loader::udev_enumerate_unref(udev_enumerate* udev_enumerate) {
  lib_loader_->udev_enumerate_unref(udev_enumerate);
}

udev_list_entry* Udev0Loader::udev_list_entry_get_next(
    udev_list_entry* list_entry) {
  return lib_loader_->udev_list_entry_get_next(list_entry);
}

const char* Udev0Loader::udev_list_entry_get_name(udev_list_entry* list_entry) {
  return lib_loader_->udev_list_entry_get_name(list_entry);
}

int Udev0Loader::udev_monitor_enable_receiving(udev_monitor* udev_monitor) {
  return lib_loader_->udev_monitor_enable_receiving(udev_monitor);
}

int Udev0Loader::udev_monitor_filter_add_match_subsystem_devtype(
    udev_monitor* udev_monitor,
    const char* subsystem,
    const char* devtype) {
  return lib_loader_->udev_monitor_filter_add_match_subsystem_devtype(
      udev_monitor, subsystem, devtype);
}

int Udev0Loader::udev_monitor_get_fd(udev_monitor* udev_monitor) {
  return lib_loader_->udev_monitor_get_fd(udev_monitor);
}

udev_monitor* Udev0Loader::udev_monitor_new_from_netlink(udev* udev,
                                                         const char* name) {
  return lib_loader_->udev_monitor_new_from_netlink(udev, name);
}

udev_device* Udev0Loader::udev_monitor_receive_device(
    udev_monitor* udev_monitor) {
  return lib_loader_->udev_monitor_receive_device(udev_monitor);
}

void Udev0Loader::udev_monitor_unref(udev_monitor* udev_monitor) {
  lib_loader_->udev_monitor_unref(udev_monitor);
}

udev* Udev0Loader::udev_new() {
  return lib_loader_->udev_new();
}

void Udev0Loader::udev_set_log_fn(
      struct udev* udev,
      void (*log_fn)(struct udev* udev, int priority,
                     const char* file, int line,
                     const char* fn, const char* format, va_list args)) {
  return lib_loader_->udev_set_log_fn(udev, log_fn);
}

void Udev0Loader::udev_set_log_priority(struct udev* udev, int priority) {
  return lib_loader_->udev_set_log_priority(udev, priority);
}

void Udev0Loader::udev_unref(udev* udev) {
  lib_loader_->udev_unref(udev);
}

}  // namespace device
