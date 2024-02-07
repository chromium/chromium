// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_platform_data_fetcher_mac.h"

#include <stdint.h>
#include <string.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/containers/contains.h"
#include "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "device/gamepad/gamepad_blocklist.h"
#include "device/gamepad/gamepad_device_mac.h"
#include "device/gamepad/gamepad_id_list.h"
#include "device/gamepad/gamepad_uma.h"
#include "device/gamepad/nintendo_controller.h"

#import <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDKeys.h>

namespace device {

namespace {

// http://www.usb.org/developers/hidpage
const uint16_t kGenericDesktopUsagePage = 0x01;
const uint16_t kJoystickUsageNumber = 0x04;
const uint16_t kGameUsageNumber = 0x05;
const uint16_t kMultiAxisUsageNumber = 0x08;

NSDictionary* DeviceMatching(uint32_t usage_page, uint32_t usage) {
  return @{
    @kIOHIDDeviceUsagePageKey : @(usage_page),
    @kIOHIDDeviceUsageKey : @(usage)
  };
}

}  // namespace

GamepadPlatformDataFetcherMac::GamepadPlatformDataFetcherMac() = default;

GamepadSource GamepadPlatformDataFetcherMac::source() {
  return Factory::static_source();
}

void GamepadPlatformDataFetcherMac::OnAddedToProvider() {
  hid_manager_ref_.reset(
      IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone));
  if (CFGetTypeID(hid_manager_ref_.get()) != IOHIDManagerGetTypeID()) {
    enabled_ = false;
    return;
  }

  NSArray* criteria = @[
    DeviceMatching(kGenericDesktopUsagePage, kJoystickUsageNumber),
    DeviceMatching(kGenericDesktopUsagePage, kGameUsageNumber),
    DeviceMatching(kGenericDesktopUsagePage, kMultiAxisUsageNumber),
  ];
  IOHIDManagerSetDeviceMatchingMultiple(hid_manager_ref_.get(),
                                        base::apple::NSToCFPtrCast(criteria));

  RegisterForNotifications();
}

void GamepadPlatformDataFetcherMac::RegisterForNotifications() {
  // Register for plug/unplug notifications.
  IOHIDManagerRegisterDeviceMatchingCallback(hid_manager_ref_.get(),
                                             DeviceAddCallback, this);
  IOHIDManagerRegisterDeviceRemovalCallback(hid_manager_ref_.get(),
                                            DeviceRemoveCallback, this);

  // Register for value change notifications.
  IOHIDManagerRegisterInputValueCallback(hid_manager_ref_.get(),
                                         ValueChangedCallback, this);

  IOHIDManagerScheduleWithRunLoop(hid_manager_ref_.get(), CFRunLoopGetCurrent(),
                                  kCFRunLoopDefaultMode);

  const auto result =
      IOHIDManagerOpen(hid_manager_ref_.get(), kIOHIDOptionsTypeNone);
  enabled_ = (result == kIOReturnSuccess || result == kIOReturnExclusiveAccess);
}

void GamepadPlatformDataFetcherMac::UnregisterFromNotifications() {
  IOHIDManagerUnscheduleFromRunLoop(
      hid_manager_ref_.get(), CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
  IOHIDManagerClose(hid_manager_ref_.get(), kIOHIDOptionsTypeNone);
}

void GamepadPlatformDataFetcherMac::PauseHint(bool pause) {
  paused_ = pause;
}

GamepadPlatformDataFetcherMac::~GamepadPlatformDataFetcherMac() {
  UnregisterFromNotifications();
  for (auto& iter : devices_) {
    iter.second->Shutdown();
  }
}

GamepadPlatformDataFetcherMac*
GamepadPlatformDataFetcherMac::InstanceFromContext(void* context) {
  return reinterpret_cast<GamepadPlatformDataFetcherMac*>(context);
}

void GamepadPlatformDataFetcherMac::DeviceAddCallback(void* context,
                                                      IOReturn result,
                                                      void* sender,
                                                      IOHIDDeviceRef ref) {
  InstanceFromContext(context)->DeviceAdd(ref);
}

void GamepadPlatformDataFetcherMac::DeviceRemoveCallback(void* context,
                                                         IOReturn result,
                                                         void* sender,
                                                         IOHIDDeviceRef ref) {
  InstanceFromContext(context)->DeviceRemove(ref);
}

void GamepadPlatformDataFetcherMac::ValueChangedCallback(void* context,
                                                         IOReturn result,
                                                         void* sender,
                                                         IOHIDValueRef ref) {
  InstanceFromContext(context)->ValueChanged(ref);
}

GamepadDeviceMac* GamepadPlatformDataFetcherMac::GetGamepadFromHidDevice(
    IOHIDDeviceRef device) {
  for (auto& iter : devices_) {
    if (iter.second->IsSameDevice(device)) {
      return iter.second.get();
    }
  }

  return nullptr;
}

void GamepadPlatformDataFetcherMac::DeviceAdd(IOHIDDeviceRef device) {
  using base::apple::CFCastStrict;
  using base::apple::CFToNSPtrCast;

  if (!enabled_) {
    return;
  }

  NSNumber* location_id = CFToNSPtrCast(CFCastStrict<CFNumberRef>(
      IOHIDDeviceGetProperty(device, CFSTR(kIOHIDLocationIDKey))));
  int location_int = location_id.intValue;

  NSNumber* vendor_id = CFToNSPtrCast(CFCastStrict<CFNumberRef>(
      IOHIDDeviceGetProperty(device, CFSTR(kIOHIDVendorIDKey))));
  NSNumber* product_id = CFToNSPtrCast(CFCastStrict<CFNumberRef>(
      IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductIDKey))));
  NSNumber* version_number = CFToNSPtrCast(CFCastStrict<CFNumberRef>(
      IOHIDDeviceGetProperty(device, CFSTR(kIOHIDVersionNumberKey))));
  NSString* product = CFToNSPtrCast(CFCastStrict<CFStringRef>(
      IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey))));
  uint16_t vendor_int = vendor_id.intValue;
  uint16_t product_int = product_id.intValue;
  uint16_t version_int = version_number.intValue;
  std::string product_name = base::SysNSStringToUTF8(product);

  // Filter out devices that have gamepad-like HID usages but aren't gamepads.
  if (GamepadIsExcluded(vendor_int, product_int))
    return;

  const auto& gamepad_id_list = GamepadIdList::Get();
  DCHECK_EQ(kXInputTypeNone,
            gamepad_id_list.GetXInputType(vendor_int, product_int));

  if (base::Contains(devices_, location_int)) {
    return;
  }

  const GamepadId gamepad_id =
      gamepad_id_list.GetGamepadId(product_name, vendor_int, product_int);

  // Nintendo devices are handled by the Nintendo data fetcher.
  if (NintendoController::IsNintendoController(gamepad_id))
    return;

  // Record the device before excluding Made for iOS gamepads. This allows us to
  // recognize these devices even though the GameController API masks the vendor
  // and product IDs. XInput devices are recorded elsewhere.
  RecordConnectedGamepad(gamepad_id);

  // The SteelSeries Nimbus and other Made for iOS gamepads should be handled
  // through the GameController interface.
  if (gamepad_id == GamepadId::kSteelSeriesProduct1420) {
    return;
  }

  bool is_recognized = gamepad_id != GamepadId::kUnknownGamepad;

  PadState* state = GetPadState(location_int, is_recognized);
  if (!state)
    return;  // No available slot for this device

  state->mapper = GetGamepadStandardMappingFunction(
      product_name, vendor_int, product_int, /*hid_specification_version=*/0,
      version_int, GAMEPAD_BUS_UNKNOWN);

  UpdateGamepadStrings(product_name, vendor_int, product_int,
                       state->mapper != nullptr, state->data);

  auto new_device = std::make_unique<GamepadDeviceMac>(
      location_int, device, product_name, vendor_int, product_int);
  if (!new_device->AddButtonsAndAxes(&state->data)) {
    new_device->Shutdown();
    return;
  }

  if (GamepadIdList::Get().HasTriggerRumbleSupport(gamepad_id)) {
    state->data.vibration_actuator.type =
        GamepadHapticActuatorType::kTriggerRumble;
  } else {
    state->data.vibration_actuator.type =
        GamepadHapticActuatorType::kDualRumble;
  }
  state->data.vibration_actuator.not_null = new_device->SupportsVibration();

  state->data.connected = true;

  devices_.emplace(location_int, std::move(new_device));
}

bool GamepadPlatformDataFetcherMac::DisconnectUnrecognizedGamepad(
    int source_id) {
  auto gamepad_iter = devices_.find(source_id);
  if (gamepad_iter == devices_.end())
    return false;
  gamepad_iter->second->Shutdown();
  devices_.erase(gamepad_iter);
  return true;
}

void GamepadPlatformDataFetcherMac::DeviceRemove(IOHIDDeviceRef device) {
  if (!enabled_)
    return;

  GamepadDeviceMac* gamepad_device = GetGamepadFromHidDevice(device);

  if (!gamepad_device)
    return;

  gamepad_device->Shutdown();
  devices_.erase(gamepad_device->GetLocationId());
}

void GamepadPlatformDataFetcherMac::ValueChanged(IOHIDValueRef value) {
  if (!enabled_ || paused_)
    return;

  IOHIDElementRef element = IOHIDValueGetElement(value);
  IOHIDDeviceRef device = IOHIDElementGetDevice(element);

  GamepadDeviceMac* gamepad_device = GetGamepadFromHidDevice(device);

  if (!gamepad_device)
    return;

  PadState* state = GetPadState(gamepad_device->GetLocationId());
  if (!state)
    return;

  gamepad_device->UpdateGamepadForValue(value, &state->data);
}

void GamepadPlatformDataFetcherMac::GetGamepadData(bool) {
  if (!enabled_)
    return;

  // Loop through and GetPadState to indicate the devices are still connected.
  for (const auto& iter : devices_) {
    GetPadState(iter.first);
  }
}

void GamepadPlatformDataFetcherMac::PlayEffect(
    int source_id,
    mojom::GamepadHapticEffectType type,
    mojom::GamepadEffectParametersPtr params,
    mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  auto device_iter = devices_.find(source_id);
  if (device_iter == devices_.end()) {
    // No connected gamepad with this location. Probably the effect was issued
    // while the gamepad was still connected, so handle this as if it were
    // preempted by a disconnect.
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultPreempted);
    return;
  }
  device_iter->second->PlayEffect(type, std::move(params), std::move(callback),
                                  std::move(callback_runner));
}

void GamepadPlatformDataFetcherMac::ResetVibration(
    int source_id,
    mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  auto device_iter = devices_.find(source_id);
  if (device_iter == devices_.end()) {
    // No connected gamepad with this location. Since the gamepad is already
    // disconnected, allow the reset to report success.
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultComplete);
    return;
  }
  device_iter->second->ResetVibration(std::move(callback),
                                      std::move(callback_runner));
}

}  // namespace device
