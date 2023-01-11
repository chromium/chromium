// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_platform_data_fetcher_linux.h"

#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "device/gamepad/gamepad_blocklist.h"
#include "device/gamepad/gamepad_id_list.h"
#include "device/gamepad/gamepad_uma.h"
#include "device/gamepad/nintendo_controller.h"
#include "device/udev_linux/scoped_udev.h"

namespace device {

GamepadPlatformDataFetcherLinux::Factory::Factory(
    scoped_refptr<base::SequencedTaskRunner> dbus_runner)
    : dbus_runner_(std::move(dbus_runner)) {}

GamepadPlatformDataFetcherLinux::Factory::~Factory() = default;

std::unique_ptr<GamepadDataFetcher>
GamepadPlatformDataFetcherLinux::Factory::CreateDataFetcher() {
  return std::make_unique<GamepadPlatformDataFetcherLinux>(dbus_runner_);
}

GamepadSource GamepadPlatformDataFetcherLinux::Factory::source() {
  return GamepadSource::kLinuxUdev;
}

GamepadSource GamepadPlatformDataFetcherLinux::Factory::static_source() {
  return GamepadSource::kLinuxUdev;
}

GamepadPlatformDataFetcherLinux::GamepadPlatformDataFetcherLinux(
    scoped_refptr<base::SequencedTaskRunner> dbus_runner)
    : dbus_runner_(dbus_runner) {}

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
  std::vector<UdevWatcher::Filter> filters;
  filters.emplace_back(UdevGamepadLinux::kInputSubsystem, "");
  filters.emplace_back(UdevGamepadLinux::kHidrawSubsystem, "");
  udev_watcher_ = UdevWatcher::StartWatching(this, filters);

  for (auto it = devices_.begin(); it != devices_.end(); ++it)
    it->get()->Shutdown();
  devices_.clear();

  udev_watcher_->EnumerateExistingDevices();
}

void GamepadPlatformDataFetcherLinux::OnDeviceAdded(
    ScopedUdevDevicePtr device) {
  RefreshDevice(device.get());
}

void GamepadPlatformDataFetcherLinux::OnDeviceRemoved(
    ScopedUdevDevicePtr device) {
  RefreshDevice(device.get());
}

void GamepadPlatformDataFetcherLinux::OnDeviceChanged(
    ScopedUdevDevicePtr device) {
  RefreshDevice(device.get());
}

void GamepadPlatformDataFetcherLinux::GetGamepadData(bool) {
  TRACE_EVENT0("GAMEPAD", "GetGamepadData");

  // Update our internal state.
  for (const auto& device : devices_) {
    if (device->GetJoydevIndex() >= 0)
      ReadDeviceData(device->GetJoydevIndex());
  }
}

// Used during enumeration, and monitor notifications.
void GamepadPlatformDataFetcherLinux::RefreshDevice(udev_device* dev) {
  std::unique_ptr<UdevGamepadLinux> udev_gamepad =
      UdevGamepadLinux::Create(dev);
  if (udev_gamepad) {
    const UdevGamepadLinux& pad_info = *udev_gamepad.get();
    if (pad_info.type == UdevGamepadLinux::Type::JOYDEV) {
      // If |syspath_prefix| is empty, the device was already disconnected.
      if (pad_info.syspath_prefix.empty())
        DisconnectUnrecognizedGamepad(pad_info.index);
      else
        RefreshJoydevDevice(dev, pad_info);
    } else if (pad_info.type == UdevGamepadLinux::Type::EVDEV) {
      RefreshEvdevDevice(dev, pad_info);
    } else if (pad_info.type == UdevGamepadLinux::Type::HIDRAW) {
      RefreshHidrawDevice(dev, pad_info);
    }
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
      return;
    }
  }
}

bool GamepadPlatformDataFetcherLinux::DisconnectUnrecognizedGamepad(int index) {
  for (auto it = devices_.begin(); it != devices_.end(); ++it) {
    const auto& device = *it;
    if (device->GetJoydevIndex() == index) {
      device->Shutdown();
      devices_.erase(it);
      return true;
    }
  }
  return false;
}

GamepadDeviceLinux* GamepadPlatformDataFetcherLinux::GetOrCreateMatchingDevice(
    const UdevGamepadLinux& pad_info) {
  for (auto it = devices_.begin(); it != devices_.end(); ++it) {
    GamepadDeviceLinux* device = it->get();
    if (device->IsSameDevice(pad_info))
      return device;
  }

  auto emplace_result = devices_.emplace(std::make_unique<GamepadDeviceLinux>(
      pad_info.syspath_prefix, dbus_runner_));
  return emplace_result.first->get();
}

void GamepadPlatformDataFetcherLinux::RefreshJoydevDevice(
    udev_device* dev,
    const UdevGamepadLinux& pad_info) {
  const int joydev_index = pad_info.index;

  GamepadDeviceLinux* device = GetOrCreateMatchingDevice(pad_info);
  if (device == nullptr)
    return;

  // If the device cannot be opened, the joystick has been disconnected.
  if (!device->OpenJoydevNode(pad_info, dev)) {
    if (device->IsEmpty())
      RemoveDevice(device);
    return;
  }

  std::string name = device->GetName();
  uint16_t vendor_id = device->GetVendorId();
  uint16_t product_id = device->GetProductId();

  // Filter out devices that have gamepad-like HID usages but aren't gamepads.
  if (GamepadIsExcluded(vendor_id, product_id)) {
    device->CloseJoydevNode();
    RemoveDevice(device);
    return;
  }

  const GamepadId gamepad_id =
      GamepadIdList::Get().GetGamepadId(name, vendor_id, product_id);

  if (NintendoController::IsNintendoController(gamepad_id)) {
    // Nintendo devices are handled by the Nintendo data fetcher.
    device->CloseJoydevNode();
    RemoveDevice(device);
    return;
  }

  bool is_recognized = gamepad_id != GamepadId::kUnknownGamepad;

  PadState* state = GetPadState(joydev_index, is_recognized);
  if (!state) {
    // No slot available for device, don't use.
    device->CloseJoydevNode();
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

  // If the device is on our list, record it by ID. If the device is unknown,
  // record that an unknown gamepad was enumerated.

  if (is_recognized)
    RecordUnknownGamepad(source());
  else
    RecordConnectedGamepad(gamepad_id);

  state->mapper = device->GetMappingFunction();

  Gamepad& pad = state->data;
  UpdateGamepadStrings(name, vendor_id, product_id, state->mapper != nullptr,
                       pad);

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
                             device->GetProductId(), mapper != nullptr, pad);
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

  device->OpenHidrawNode(
      pad_info,
      base::BindOnce(&GamepadPlatformDataFetcherLinux::OnHidrawDeviceOpened,
                     weak_factory_.GetWeakPtr()));
}

void GamepadPlatformDataFetcherLinux::OnHidrawDeviceOpened(
    GamepadDeviceLinux* device) {
  DCHECK(device);
  if (device->IsEmpty()) {
    RemoveDevice(device);
    return;
  }

  int joydev_index = device->GetJoydevIndex();
  if (joydev_index < 0)
    return;

  PadState* state = GetPadState(joydev_index);
  if (!state)
    return;

  Gamepad& pad = state->data;
  pad.vibration_actuator.type = GamepadHapticActuatorType::kDualRumble;
  pad.vibration_actuator.not_null = device->SupportsVibration();
}

void GamepadPlatformDataFetcherLinux::ReadDeviceData(size_t index) {
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
    mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  if (pad_id < 0 || pad_id >= static_cast<int>(Gamepads::kItemsLengthCap)) {
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }

  GamepadDeviceLinux* device = GetDeviceWithJoydevIndex(pad_id);
  if (!device) {
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }

  device->PlayEffect(type, std::move(params), std::move(callback),
                     std::move(callback_runner));
}

void GamepadPlatformDataFetcherLinux::ResetVibration(
    int pad_id,
    mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  if (pad_id < 0 || pad_id >= static_cast<int>(Gamepads::kItemsLengthCap)) {
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }

  GamepadDeviceLinux* device = GetDeviceWithJoydevIndex(pad_id);
  if (!device) {
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }

  device->ResetVibration(std::move(callback), std::move(callback_runner));
}

}  // namespace device
