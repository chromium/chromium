// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_platform_data_fetcher_linux.h"

#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "device/udev_linux/scoped_udev.h"
#include "device/udev_linux/udev_linux.h"

namespace device {

GamepadPlatformDataFetcherLinux::GamepadPlatformDataFetcherLinux() = default;

GamepadPlatformDataFetcherLinux::~GamepadPlatformDataFetcherLinux() {
  for (auto it = devices_.begin(); it != devices_.end(); ++it) {
    GamepadDeviceLinux* device = it->get();
    device->Shutdown();
  }
}

GamepadSource GamepadPlatformDataFetcherLinux::source() {
  return Factory::static_source();
}

void GamepadPlatformDataFetcherLinux::OnAddedToProvider() {
  std::vector<UdevLinux::UdevMonitorFilter> filters;
  filters.push_back(
      UdevLinux::UdevMonitorFilter(UdevGamepadLinux::kInputSubsystem, nullptr));
  filters.push_back(UdevLinux::UdevMonitorFilter(
      UdevGamepadLinux::kHidrawSubsystem, nullptr));
  udev_.reset(new UdevLinux(
      filters, base::Bind(&GamepadPlatformDataFetcherLinux::RefreshDevice,
                          base::Unretained(this))));

  for (auto it = devices_.begin(); it != devices_.end(); ++it)
    it->get()->Shutdown();
  devices_.clear();

  EnumerateSubsystemDevices(UdevGamepadLinux::kInputSubsystem);
  EnumerateSubsystemDevices(UdevGamepadLinux::kHidrawSubsystem);
}

void GamepadPlatformDataFetcherLinux::GetGamepadData(bool) {
  TRACE_EVENT0("GAMEPAD", "GetGamepadData");

  // Update our internal state.
  for (size_t i = 0; i < Gamepads::kItemsLengthCap; ++i)
    ReadDeviceData(i);
}

// static
void GamepadPlatformDataFetcherLinux::UpdateGamepadStrings(
    const std::string& name,
    uint16_t vendor_id,
    uint16_t product_id,
    bool has_standard_mapping,
    Gamepad* pad) {
  // Set the ID string. The ID contains the device name, vendor and product IDs,
  // and an indication of whether the standard mapping is in use.
  std::string id = base::StringPrintf(
      "%s (%sVendor: %04x Product: %04x)", name.c_str(),
      has_standard_mapping ? "STANDARD GAMEPAD " : "", vendor_id, product_id);
  base::TruncateUTF8ToByteSize(id, Gamepad::kIdLengthCap - 1, &id);
  base::string16 tmp16 = base::UTF8ToUTF16(id);
  memset(pad->id, 0, sizeof(pad->id));
  tmp16.copy(pad->id, arraysize(pad->id) - 1);

  // Set the mapper string to "standard" if the gamepad has a standard mapping,
  // or the empty string otherwise.
  if (has_standard_mapping) {
    std::string mapping = "standard";
    base::TruncateUTF8ToByteSize(mapping, Gamepad::kMappingLengthCap - 1,
                                 &mapping);
    tmp16 = base::UTF8ToUTF16(mapping);
    memset(pad->mapping, 0, sizeof(pad->mapping));
    tmp16.copy(pad->mapping, arraysize(pad->mapping) - 1);
  } else {
    pad->mapping[0] = 0;
  }
}

// Used during enumeration, and monitor notifications.
void GamepadPlatformDataFetcherLinux::RefreshDevice(udev_device* dev) {
  std::unique_ptr<UdevGamepadLinux> udev_gamepad =
      UdevGamepadLinux::Create(dev);
  if (udev_gamepad) {
    const UdevGamepadLinux& pad_info = *udev_gamepad.get();
    if (pad_info.type == UdevGamepadLinux::Type::JOYDEV)
      RefreshJoydevDevice(dev, pad_info);
    else if (pad_info.type == UdevGamepadLinux::Type::EVDEV)
      RefreshEvdevDevice(dev, pad_info);
    else if (pad_info.type == UdevGamepadLinux::Type::HIDRAW)
      RefreshHidrawDevice(dev, pad_info);
  }
}

GamepadDeviceLinux* GamepadPlatformDataFetcherLinux::GetDeviceWithJoydevIndex(
    int joydev_index) {
  for (auto it = devices_.begin(); it != devices_.end(); ++it) {
    GamepadDeviceLinux* device = it->get();
    if (device->GetJoydevIndex() == joydev_index)
      return device;
  }

  return nullptr;
}

void GamepadPlatformDataFetcherLinux::RemoveDevice(GamepadDeviceLinux* device) {
  for (auto it = devices_.begin(); it != devices_.end(); ++it) {
    if (it->get() == device) {
      device->Shutdown();
      devices_.erase(it);
      break;
    }
  }
}

GamepadDeviceLinux* GamepadPlatformDataFetcherLinux::GetOrCreateMatchingDevice(
    const UdevGamepadLinux& pad_info) {
  for (auto it = devices_.begin(); it != devices_.end(); ++it) {
    GamepadDeviceLinux* device = it->get();
    if (device->IsSameDevice(pad_info))
      return device;
  }

  auto emplace_result = devices_.emplace(
      std::make_unique<GamepadDeviceLinux>(pad_info.syspath_prefix));
  return emplace_result.first->get();
}

void GamepadPlatformDataFetcherLinux::RefreshJoydevDevice(
    udev_device* dev,
    const UdevGamepadLinux& pad_info) {
  const int joydev_index = pad_info.index;
  if (joydev_index < 0 || joydev_index >= (int)Gamepads::kItemsLengthCap)
    return;

  GamepadDeviceLinux* device = GetOrCreateMatchingDevice(pad_info);
  if (device == nullptr)
    return;

  PadState* state = GetPadState(joydev_index);
  if (!state) {
    // No slot available for device, don't use.
    RemoveDevice(device);
    return;
  }

  // The device pointed to by dev contains information about the logical
  // joystick device. In order to get the information about the physical
  // hardware, get the parent device that is also in the "input" subsystem.
  // This function should just walk up the tree one level.
  udev_device* parent_dev =
      device::udev_device_get_parent_with_subsystem_devtype(
          dev, UdevGamepadLinux::kInputSubsystem, nullptr);
  if (!parent_dev) {
    device->CloseJoydevNode();
    if (device->IsEmpty())
      RemoveDevice(device);
    return;
  }

  // If the device cannot be opened, the joystick has been disconnected.
  if (!device->OpenJoydevNode(pad_info, dev)) {
    if (device->IsEmpty())
      RemoveDevice(device);
    return;
  }

  state->mapper = device->GetMappingFunction();

  Gamepad& pad = state->data;
  UpdateGamepadStrings(device->GetName(), device->GetVendorId(),
                       device->GetProductId(), state->mapper != nullptr, &pad);

  pad.vibration_actuator.type = GamepadHapticActuatorType::kDualRumble;
  pad.vibration_actuator.not_null = device->SupportsVibration();

  pad.connected = true;
}

void GamepadPlatformDataFetcherLinux::RefreshEvdevDevice(
    udev_device* dev,
    const UdevGamepadLinux& pad_info) {
  GamepadDeviceLinux* device = GetOrCreateMatchingDevice(pad_info);
  if (device == nullptr)
    return;

  if (!device->OpenEvdevNode(pad_info)) {
    if (device->IsEmpty())
      RemoveDevice(device);
    return;
  }

  int joydev_index = device->GetJoydevIndex();
  if (joydev_index >= 0) {
    PadState* state = GetPadState(joydev_index);
    DCHECK(state);
    if (state) {
      Gamepad& pad = state->data;

      // To select the correct mapper for an arbitrary gamepad we may need info
      // from both the joydev and evdev nodes. For instance, a gamepad that
      // connects over USB and Bluetooth may need to select a mapper based on
      // the connection type, but this information is only available through
      // evdev. To ensure that gamepads are usable when evdev is unavailable, a
      // preliminary mapping is assigned when the joydev node is enumerated.
      //
      // Here we check if associating the evdev node changed the mapping
      // function that should be used for this gamepad. If so, assign the new
      // mapper and rebuild the gamepad strings.
      GamepadStandardMappingFunction mapper = device->GetMappingFunction();
      if (mapper != state->mapper) {
        state->mapper = mapper;
        UpdateGamepadStrings(device->GetName(), device->GetVendorId(),
                             device->GetProductId(), mapper != nullptr, &pad);
      }

      pad.vibration_actuator.not_null = device->SupportsVibration();
    }
  }
}

void GamepadPlatformDataFetcherLinux::RefreshHidrawDevice(
    udev_device* dev,
    const UdevGamepadLinux& pad_info) {
  GamepadDeviceLinux* device = GetOrCreateMatchingDevice(pad_info);
  if (device == nullptr)
    return;

  if (!device->OpenHidrawNode(pad_info)) {
    if (device->IsEmpty())
      RemoveDevice(device);
    return;
  }

  int joydev_index = device->GetJoydevIndex();
  if (joydev_index >= 0) {
    PadState* state = GetPadState(joydev_index);
    DCHECK(state);
    if (state) {
      Gamepad& pad = state->data;
      pad.vibration_actuator.not_null = device->SupportsVibration();
    }
  }
}

void GamepadPlatformDataFetcherLinux::EnumerateSubsystemDevices(
    const std::string& subsystem) {
  if (!udev_->udev_handle())
    return;
  ScopedUdevEnumeratePtr enumerate(udev_enumerate_new(udev_->udev_handle()));
  if (!enumerate)
    return;
  int ret =
      udev_enumerate_add_match_subsystem(enumerate.get(), subsystem.c_str());
  if (ret != 0)
    return;
  ret = udev_enumerate_scan_devices(enumerate.get());
  if (ret != 0)
    return;

  udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate.get());
  for (udev_list_entry* dev_list_entry = devices; dev_list_entry != nullptr;
       dev_list_entry = udev_list_entry_get_next(dev_list_entry)) {
    // Get the filename of the /sys entry for the device and create a
    // udev_device object (dev) representing it
    const char* path = udev_list_entry_get_name(dev_list_entry);
    ScopedUdevDevicePtr dev(
        udev_device_new_from_syspath(udev_->udev_handle(), path));
    if (!dev)
      continue;
    RefreshDevice(dev.get());
  }
}

void GamepadPlatformDataFetcherLinux::ReadDeviceData(size_t index) {
  CHECK_LT(index, Gamepads::kItemsLengthCap);

  GamepadDeviceLinux* device = GetDeviceWithJoydevIndex(index);
  if (!device)
    return;

  PadState* state = GetPadState(index);
  if (!state)
    return;

  device->ReadPadState(&state->data);
}

void GamepadPlatformDataFetcherLinux::PlayEffect(
    int pad_id,
    mojom::GamepadHapticEffectType type,
    mojom::GamepadEffectParametersPtr params,
    mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback) {
  if (pad_id < 0 || pad_id >= static_cast<int>(Gamepads::kItemsLengthCap)) {
    std::move(callback).Run(
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }

  GamepadDeviceLinux* device = GetDeviceWithJoydevIndex(pad_id);
  if (!device) {
    std::move(callback).Run(
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }

  device->PlayEffect(type, std::move(params), std::move(callback));
}

void GamepadPlatformDataFetcherLinux::ResetVibration(
    int pad_id,
    mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback) {
  if (pad_id < 0 || pad_id >= static_cast<int>(Gamepads::kItemsLengthCap)) {
    std::move(callback).Run(
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }

  GamepadDeviceLinux* device = GetDeviceWithJoydevIndex(pad_id);
  if (!device) {
    std::move(callback).Run(
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }

  device->ResetVibration(std::move(callback));
}

}  // namespace device
