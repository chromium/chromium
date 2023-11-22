// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/udev_gamepad_linux.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "device/udev_linux/udev.h"

namespace device {

namespace {

// Extract the device |index| from a device node |path|. The path must begin
// with |prefix| and the remainder of the string must be parsable as an integer.
// Returns true if parsing succeeded.
bool DeviceIndexFromDevicePath(std::string_view path,
                               std::string_view prefix,
                               int* index) {
  DCHECK(index);
  if (!base::StartsWith(path, prefix))
    return false;
  std::string_view index_str = path;
  index_str.remove_prefix(prefix.length());
  return base::StringToInt(index_str, index);
}

// Small helper to avoid constructing a `std::string_view` from nullptr.
std::string_view ToStringView(const char* str) {
  return str ? std::string_view(str) : std::string_view();
}

}  // namespace

const char UdevGamepadLinux::kInputSubsystem[] = "input";
const char UdevGamepadLinux::kHidrawSubsystem[] = "hidraw";

UdevGamepadLinux::UdevGamepadLinux(Type type,
                                   int index,
                                   std::string_view path,
                                   std::string_view syspath_prefix)
    : type(type), index(index), path(path), syspath_prefix(syspath_prefix) {}

// static
std::unique_ptr<UdevGamepadLinux> UdevGamepadLinux::Create(udev_device* dev) {
  using DeviceRootPair = std::pair<Type, const char*>;
  static const std::vector<DeviceRootPair> device_roots = {
      {Type::EVDEV, "/dev/input/event"},
      {Type::JOYDEV, "/dev/input/js"},
      {Type::HIDRAW, "/dev/hidraw"},
  };

  if (!dev)
    return nullptr;

  const auto node_path = ToStringView(device::udev_device_get_devnode(dev));
  if (node_path.empty())
    return nullptr;

  const auto node_syspath = ToStringView(device::udev_device_get_syspath(dev));
  if (node_syspath.empty())
    return nullptr;

  std::string_view parent_syspath;
  udev_device* parent_dev =
      device::udev_device_get_parent_with_subsystem_devtype(
          dev, kInputSubsystem, nullptr);
  if (parent_dev)
    parent_syspath = ToStringView(device::udev_device_get_syspath(parent_dev));

  for (const auto& entry : device_roots) {
    const Type node_type = entry.first;
    const std::string_view prefix = entry.second;
    int index_value;
    if (!DeviceIndexFromDevicePath(node_path, prefix, &index_value))
      continue;

    // The syspath can be used to associate device nodes that describe the same
    // gamepad through multiple interfaces. For input nodes (evdev and joydev),
    // we use the syspath of the parent node, which describes the underlying
    // physical device. For hidraw nodes, we use the syspath of the node itself.
    //
    // The parent syspaths for matching evdev and joydev nodes will be identical
    // because they share the same parent node. The syspath for hidraw nodes
    // will also be identical up to the subsystem. For instance, if the syspath
    // for the input subsystem is:
    //     /sys/devices/[...]/0003:054C:09CC.0026/input/input91
    // And the corresponding hidraw syspath is:
    //     /sys/devices/[...]/0003:054C:09CC.0026/hidraw/hidraw3
    // Then |syspath_prefix| is the common prefix before "input" or "hidraw":
    //     /sys/devices/[...]/0003:054C:09CC.0026/
    std::string_view syspath;
    std::string_view subsystem;
    if (node_type == Type::EVDEV || node_type == Type::JOYDEV) {
      // If the device is in the input subsystem but does not have the
      // ID_INPUT_JOYSTICK property, ignore it.
      if (!device::udev_device_get_property_value(dev, "ID_INPUT_JOYSTICK"))
        return nullptr;

      syspath = parent_syspath;
      subsystem = kInputSubsystem;
    } else if (node_type == Type::HIDRAW) {
      syspath = node_syspath;
      subsystem = kHidrawSubsystem;
    }

    std::string_view syspath_prefix;
    if (!syspath.empty()) {
      size_t subsystem_start = syspath.find(subsystem);
      if (subsystem_start == std::string::npos)
        return nullptr;
      syspath_prefix = syspath.substr(0, subsystem_start);
    }

    return std::make_unique<UdevGamepadLinux>(node_type, index_value, node_path,
                                              syspath_prefix);
  }
  return nullptr;
}

}  // namespace device
