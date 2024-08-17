// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/udev_linux/fake_udev_loader.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"

struct udev {
  // empty
};

struct udev_list_entry {
  explicit udev_list_entry(std::string name) : name(std::move(name)) {}
  udev_list_entry(const udev_list_entry& other) = delete;
  udev_list_entry& operator=(const udev_list_entry& other) = delete;

  const std::string name;
  raw_ptr<udev_list_entry, DanglingUntriaged> next = nullptr;
};

struct udev_device {
  udev_device(std::string name,
              std::string syspath,
              std::string subsystem,
              std::optional<std::string> devnode,
              std::optional<std::string> devtype,
              std::map<std::string, std::string> sysattrs,
              std::map<std::string, std::string> prop_map)
      : name(std::move(name)),
        syspath(std::move(syspath)),
        subsystem(std::move(subsystem)),
        devnode(std::move(devnode)),
        devtype(std::move(devtype)),
        sysattrs(std::move(sysattrs)) {
    properties = std::move(prop_map);
    for (auto const& pair : properties) {
      auto prop = std::make_unique<udev_list_entry>(pair.first);
      if (!udev_prop_list.empty())
        udev_prop_list.back()->next = prop.get();
      udev_prop_list.push_back(std::move(prop));
    }
  }
  udev_device(const udev_device& other) = delete;
  udev_device& operator=(const udev_device& other) = delete;

  const std::string name;
  const std::string syspath;
  const std::string subsystem;
  const std::optional<std::string> devnode;
  const std::optional<std::string> devtype;
  std::map<std::string, std::string> sysattrs;
  std::map<std::string, std::string> properties;
  std::vector<std::unique_ptr<udev_list_entry>> udev_prop_list;
};

struct udev_enumerate {
  explicit udev_enumerate(
      const std::vector<std::unique_ptr<udev_device>>& devices) {
    for (const auto& device : devices) {
      auto entry = std::make_unique<udev_list_entry>(device->syspath);
      if (!entries.empty()) {
        entries.back()->next = entry.get();
      }
      entries.push_back(std::move(entry));
    }
  }
  udev_enumerate(const udev_enumerate& other) = delete;
  udev_enumerate& operator=(const udev_enumerate& other) = delete;

  std::vector<std::unique_ptr<udev_list_entry>> entries;
};

struct udev_monitor {
  udev_monitor() {
    bool res = base::CreatePipe(&read_fd, &write_fd, true);
    DCHECK(res);
  }
  udev_monitor(const udev_monitor& other) = delete;
  udev_monitor& operator=(const udev_monitor& other) = delete;

  // |read_fd| will be returned by udev_monitor_get_fd() and will be signaled
  // by writing to |write_fd| to indicate that an event is available.
  base::ScopedFD read_fd;
  base::ScopedFD write_fd;
};

namespace testing {

FakeUdevLoader::FakeUdevLoader() {
  // Nothing to construct, just register it as testing backend.
  UdevLoader::SetForTesting(this, true);
}

FakeUdevLoader::~FakeUdevLoader() {
  // Clean up after ourselves if this instance of fake udev loader was used
  // as test backend.
  if (UdevLoader::Get() == this)
    UdevLoader::SetForTesting(nullptr, false);
}

udev_device* FakeUdevLoader::AddFakeDevice(
    std::string name,
    std::string syspath,
    std::string subsystem,
    std::optional<std::string> devnode,
    std::optional<std::string> devtype,
    std::map<std::string, std::string> sysattrs,
    std::map<std::string, std::string> properties) {
  devices_.emplace_back(
      new udev_device(std::move(name), std::move(syspath), std::move(subsystem),
                      std::move(devnode), std::move(devtype),
                      std::move(sysattrs), std::move(properties)));
  return devices_.back().get();
}

void FakeUdevLoader::Reset() {
  devices_.clear();
}

const char* FakeUdevLoader::udev_device_get_action(udev_device* device) {
  DCHECK(device);
  return nullptr;
}

const char* FakeUdevLoader::udev_device_get_devnode(udev_device* device) {
  DCHECK(device);
  if (!device->devnode)
    return nullptr;

  return device->devnode->c_str();
}

const char* FakeUdevLoader::udev_device_get_devtype(udev_device* device) {
  DCHECK(device);
  if (!device->devtype)
    return nullptr;

  return device->devtype->c_str();
}

udev_device* FakeUdevLoader::udev_device_get_parent(udev_device* device) {
  DCHECK(device);
  udev_device* parent = nullptr;
  const base::FilePath syspath(device->syspath);
  for (const auto& d : devices_) {
    if (!base::FilePath(d->syspath).IsParent(syspath))
      continue;

    if (!parent || d->syspath.size() > parent->syspath.size())
      parent = d.get();
  }
  return parent;
}

udev_device* FakeUdevLoader::udev_device_get_parent_with_subsystem_devtype(
    udev_device* device,
    const char* subsystem,
    const char* devtype) {
  DCHECK(device && subsystem);
  return nullptr;
}

udev_list_entry* FakeUdevLoader::udev_device_get_properties_list_entry(
    struct udev_device* device) {
  DCHECK(device);
  return device->udev_prop_list.front().get();
}

const char* FakeUdevLoader::udev_device_get_property_value(udev_device* device,
                                                           const char* key) {
  DCHECK(device && key);
  const auto it = device->properties.find(key);
  return it == device->properties.end() ? nullptr : it->second.c_str();
}

const char* FakeUdevLoader::udev_device_get_subsystem(udev_device* device) {
  DCHECK(device);
  return device->subsystem.c_str();
}

const char* FakeUdevLoader::udev_device_get_sysattr_value(udev_device* device,
                                                          const char* sysattr) {
  DCHECK(device && sysattr);
  auto it = device->sysattrs.find(sysattr);
  return it == device->sysattrs.end() ? nullptr : it->second.c_str();
}

const char* FakeUdevLoader::udev_device_get_sysname(udev_device* device) {
  DCHECK(device);
  return device->name.c_str();
}

const char* FakeUdevLoader::udev_device_get_syspath(udev_device* device) {
  DCHECK(device);
  return device->syspath.c_str();
}

udev_device* FakeUdevLoader::udev_device_new_from_devnum(udev* udev_context,
                                                         char type,
                                                         dev_t devnum) {
  return nullptr;
}

udev_device* FakeUdevLoader::udev_device_new_from_subsystem_sysname(
    udev* udev_context,
    const char* subsystem,
    const char* sysname) {
  DCHECK(subsystem && sysname);
  return nullptr;
}

udev_device* FakeUdevLoader::udev_device_new_from_syspath(udev* udev_context,
                                                          const char* syspath) {
  DCHECK(syspath);
  auto it = base::ranges::find(devices_, syspath, &udev_device::syspath);
  return it == devices_.end() ? nullptr : it->get();
}

void FakeUdevLoader::udev_device_unref(udev_device* device) {
  // Nothing to do, the device will be destroyed when FakeUdevLoader instance
  // gets destroyed.
}

int FakeUdevLoader::udev_enumerate_add_match_subsystem(
    udev_enumerate* enumeration_context,
    const char* subsystem) {
  DCHECK(enumeration_context);
  return 0;
}

udev_list_entry* FakeUdevLoader::udev_enumerate_get_list_entry(
    udev_enumerate* enumeration_context) {
  DCHECK(enumeration_context);
  if (enumeration_context->entries.empty())
    return nullptr;

  return enumeration_context->entries.front().get();
}

udev_enumerate* FakeUdevLoader::udev_enumerate_new(udev* udev_context) {
  return new udev_enumerate(devices_);
}

int FakeUdevLoader::udev_enumerate_scan_devices(
    udev_enumerate* enumeration_context) {
  DCHECK(enumeration_context);
  return 0;
}

void FakeUdevLoader::udev_enumerate_unref(udev_enumerate* enumeration_context) {
  if (enumeration_context)
    delete enumeration_context;
}

udev_list_entry* FakeUdevLoader::udev_list_entry_get_next(
    udev_list_entry* list_entry) {
  if (!list_entry)
    return nullptr;

  return list_entry->next;
}

const char* FakeUdevLoader::udev_list_entry_get_name(
    udev_list_entry* list_entry) {
  if (!list_entry)
    return nullptr;

  return list_entry->name.c_str();
}

int FakeUdevLoader::udev_monitor_enable_receiving(udev_monitor* monitor) {
  DCHECK(monitor);
  return 0;
}

int FakeUdevLoader::udev_monitor_filter_add_match_subsystem_devtype(
    udev_monitor* monitor,
    const char* subsystem,
    const char* devtype) {
  DCHECK(monitor && subsystem);
  return 0;
}

int FakeUdevLoader::udev_monitor_get_fd(udev_monitor* monitor) {
  DCHECK(monitor);
  return monitor->read_fd.get();
}

udev_monitor* FakeUdevLoader::udev_monitor_new_from_netlink(udev* udev_context,
                                                            const char* name) {
  return new udev_monitor;
}

udev_device* FakeUdevLoader::udev_monitor_receive_device(
    udev_monitor* monitor) {
  DCHECK(monitor);
  return nullptr;
}

void FakeUdevLoader::udev_monitor_unref(udev_monitor* monitor) {
  if (monitor)
    delete monitor;
}

udev* FakeUdevLoader::udev_new() {
  return new udev;
}

void FakeUdevLoader::udev_unref(udev* udev_context) {
  if (udev_context)
    delete udev_context;
}

}  // namespace testing
