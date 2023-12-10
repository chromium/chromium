// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_UDEV_GAMEPAD_LINUX_H_
#define DEVICE_GAMEPAD_UDEV_GAMEPAD_LINUX_H_

#include <memory>
#include <string>
#include <string_view>

extern "C" {
struct udev_device;
}

namespace device {

class UdevGamepadLinux {
 public:
  enum class Type {
    JOYDEV,
    EVDEV,
    HIDRAW,
  };

  static const char kInputSubsystem[];
  static const char kHidrawSubsystem[];

  UdevGamepadLinux(Type type,
                   int index,
                   std::string_view path,
                   std::string_view syspath_prefix);
  ~UdevGamepadLinux() = default;

  // Factory method for creating UdevGamepadLinux instances. Extracts info
  // about the device and returns a UdevGamepadLinux describing it, or nullptr
  // if the device cannot be a gamepad.
  static std::unique_ptr<UdevGamepadLinux> Create(udev_device* dev);

  // The kernel interface used to communicate with this device.
  const Type type;

  // The index of this device node among nodes of this type. For instance, a
  // device with |path| /dev/input/js3 has |index| 3.
  const int index;

  // The filesystem path of the device node representing this device.
  const std::string path;

  // A string identifier used to identify device nodes that refer to the same
  // physical device. For nodes in the input subsystem (joydev and evdev), the
  // |syspath_prefix| is a prefix of the syspath of the parent node. For hidraw
  // nodes, it is a prefix of the node's syspath. |syspath_prefix| is empty if
  // the syspath could not be queried (usually this indicates the device has
  // been disconnected).
  const std::string syspath_prefix;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_UDEV_GAMEPAD_LINUX_H_
