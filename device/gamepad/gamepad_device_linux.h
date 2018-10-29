// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEPAD_DEVICE_LINUX_
#define DEVICE_GAMEPAD_GAMEPAD_DEVICE_LINUX_

#include <memory>
#include <string>
#include <vector>

#include "device/gamepad/abstract_haptic_gamepad.h"
#include "device/gamepad/dualshock4_controller_linux.h"
#include "device/gamepad/gamepad_standard_mappings.h"
#include "device/gamepad/switch_pro_controller_linux.h"
#include "device/gamepad/udev_gamepad_linux.h"

extern "C" {
struct udev_device;
}

namespace device {

// GamepadDeviceLinux represents a single gamepad device which may be accessed
// through multiple host interfaces. Gamepad button and axis state are queried
// through the joydev interface, while haptics commands are routed through the
// evdev interface. A gamepad must be enumerated through joydev to be usable,
// but the evdev interface is only required for haptic effects.
//
// Dualshock4 haptics are not supported through evdev and are instead sent
// through the raw HID (hidraw) interface.
class GamepadDeviceLinux : public AbstractHapticGamepad {
 public:
  GamepadDeviceLinux(const std::string& syspath_prefix);
  ~GamepadDeviceLinux() override;

  // Delete any stored effect and close file descriptors.
  void DoShutdown() override;

  // Returns true if no device nodes are associated with this device.
  bool IsEmpty() const;

  int GetJoydevIndex() const { return joydev_index_; }
  uint16_t GetVendorId() const { return vendor_id_; }
  uint16_t GetProductId() const { return product_id_; }
  uint16_t GetVersionNumber() const { return version_number_; }
  std::string GetName() const { return name_; }
  std::string GetSyspathPrefix() const { return syspath_prefix_; }
  GamepadBusType GetBusType() const { return bus_type_; }
  GamepadStandardMappingFunction GetMappingFunction() const;

  bool SupportsVibration() const;

  // Reads the current gamepad state into |pad|.
  void ReadPadState(Gamepad* pad);

  // Reads the state of gamepad buttons and axes using joydev. Returns true if
  // |pad| was updated.
  bool ReadJoydevState(Gamepad* pad);

  // Discovers and assigns button indices for key codes that are outside the
  // normal gamepad button range.
  void InitializeEvdevSpecialKeys();

  // Reads the state of keys outside the normal button range using evdev.
  // Returns true if |pad| was updated.
  bool ReadEvdevSpecialKeys(Gamepad* pad);

  // Returns true if |pad_info| describes this device.
  bool IsSameDevice(const UdevGamepadLinux& pad_info);

  // Opens the joydev device node and queries device info.
  bool OpenJoydevNode(const UdevGamepadLinux& pad_info, udev_device* device);

  // Closes the joydev device node and clears device info.
  void CloseJoydevNode();

  // Opens the evdev device node and initializes haptics.
  bool OpenEvdevNode(const UdevGamepadLinux& pad_info);

  // Closes the evdev device node and shuts down haptics.
  void CloseEvdevNode();

  // Opens the hidraw device node and initializes haptics.
  bool OpenHidrawNode(const UdevGamepadLinux& pad_info);

  // Closes the hidraw device node and shuts down haptics.
  void CloseHidrawNode();

  // AbstractHapticGamepad
  void SetVibration(double strong_magnitude, double weak_magnitude) override;
  void SetZeroVibration() override;

 private:
  // The syspath prefix is used to identify device nodes that refer to the same
  // underlying gamepad through different interfaces.
  //
  // Joydev and evdev nodes that refer to the same device will share a parent
  // node that represents the physical device. We can compare the syspaths of
  // the parent nodes to determine when two nodes refer to the same device.
  //
  // The syspath for a hidraw node will match the parent syspath of a joydev or
  // evdev node up to the subsystem. To simplify this comparison, we only store
  // the syspath prefix up to the subsystem.
  std::string syspath_prefix_;

  // The file descriptor for the device's joydev node, or -1 if no joydev node
  // is associated with this device.
  int joydev_fd_ = -1;

  // The index of the device's joydev node, or -1 if unknown.
  // The joydev index is the integer at the end of the joydev node path and is
  // used to assign the gamepad to a slot. For example, a device with path
  // /dev/input/js2 has index 2 and will be assigned to the 3rd gamepad slot.
  int joydev_index_ = -1;

  // Maps from indices in the Gamepad buttons array to a boolean value
  // indicating whether the button index is already mapped.
  std::vector<bool> button_indices_used_;

  // The vendor ID of the device.
  uint16_t vendor_id_;

  // The product ID of the device.
  uint16_t product_id_;

  // The version number of the device.
  uint16_t version_number_;

  // A string identifying the manufacturer and model of the device.
  std::string name_;

  // The file descriptor for the device's evdev node, or -1 if no evdev node is
  // associated with this device.
  int evdev_fd_ = -1;

  // The ID of the haptic effect stored on the device, or -1 if none is stored.
  int effect_id_ = -1;

  // True if the device supports rumble effects through the evdev device node.
  bool supports_force_feedback_ = false;

  // Set to true once the evdev button capabilities have been checked.
  bool evdev_special_keys_initialized_ = false;

  // Mapping from "special" index (an index within the kSpecialKeys table) to
  // button index (an index within the Gamepad buttons array), or -1 if the
  // button is not mapped. Empty if no special buttons are mapped.
  std::vector<int> special_button_map_;

  // The file descriptor for the device's hidraw node, or -1 if no hidraw node
  // is associated with this device.
  int hidraw_fd_ = -1;

  // The type of the bus through which the device is connected, or
  // GAMEPAD_BUS_UNKNOWN if the bus type could not be determined.
  GamepadBusType bus_type_ = GAMEPAD_BUS_UNKNOWN;

  // Dualshock4 functionality, if available.
  std::unique_ptr<Dualshock4ControllerLinux> dualshock4_;

  // Nintendo Switch Pro controller functionality, if available.
  std::unique_ptr<SwitchProControllerLinux> switch_pro_;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_DEVICE_LINUX_
