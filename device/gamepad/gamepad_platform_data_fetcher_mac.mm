// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_platform_data_fetcher_mac.h"

#include <stdint.h>
#include <string.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/time/time.h"

#import <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDKeys.h>

namespace device {

namespace {

// http://www.usb.org/developers/hidpage
const uint16_t kGenericDesktopUsagePage = 0x01;
const uint16_t kJoystickUsageNumber = 0x04;
const uint16_t kGameUsageNumber = 0x05;
const uint16_t kMultiAxisUsageNumber = 0x08;

const uint16_t kVendorSteelSeries = 0x1038;
const uint16_t kProductNimbus = 0x1420;

void CopyNSStringAsUTF16LittleEndian(NSString* src,
                                     UChar* dest,
                                     size_t dest_len) {
  NSData* as16 = [src dataUsingEncoding:NSUTF16LittleEndianStringEncoding];
  memset(dest, 0, dest_len);
  [as16 getBytes:dest length:dest_len - sizeof(UChar)];
}

NSDictionary* DeviceMatching(uint32_t usage_page, uint32_t usage) {
  return [NSDictionary
      dictionaryWithObjectsAndKeys:[NSNumber numberWithUnsignedInt:usage_page],
                                   base::mac::CFToNSCast(
                                       CFSTR(kIOHIDDeviceUsagePageKey)),
                                   [NSNumber numberWithUnsignedInt:usage],
                                   base::mac::CFToNSCast(
                                       CFSTR(kIOHIDDeviceUsageKey)),
                                   nil];
}

}  // namespace

GamepadPlatformDataFetcherMac::GamepadPlatformDataFetcherMac()
    : enabled_(true), paused_(false) {}

GamepadSource GamepadPlatformDataFetcherMac::source() {
  return Factory::static_source();
}

void GamepadPlatformDataFetcherMac::OnAddedToProvider() {
  hid_manager_ref_.reset(
      IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone));
  if (CFGetTypeID(hid_manager_ref_) != IOHIDManagerGetTypeID()) {
    enabled_ = false;
    return;
  }

  base::scoped_nsobject<NSArray> criteria(
      [[NSArray alloc] initWithObjects:DeviceMatching(kGenericDesktopUsagePage,
                                                      kJoystickUsageNumber),
                                       DeviceMatching(kGenericDesktopUsagePage,
                                                      kGameUsageNumber),
                                       DeviceMatching(kGenericDesktopUsagePage,
                                                      kMultiAxisUsageNumber),
                                       nil]);
  IOHIDManagerSetDeviceMatchingMultiple(hid_manager_ref_,
                                        base::mac::NSToCFCast(criteria));

  RegisterForNotifications();
}

void GamepadPlatformDataFetcherMac::RegisterForNotifications() {
  // Register for plug/unplug notifications.
  IOHIDManagerRegisterDeviceMatchingCallback(hid_manager_ref_,
                                             DeviceAddCallback, this);
  IOHIDManagerRegisterDeviceRemovalCallback(hid_manager_ref_,
                                            DeviceRemoveCallback, this);

  // Register for value change notifications.
  IOHIDManagerRegisterInputValueCallback(hid_manager_ref_, ValueChangedCallback,
                                         this);

  IOHIDManagerScheduleWithRunLoop(hid_manager_ref_, CFRunLoopGetMain(),
                                  kCFRunLoopDefaultMode);

  enabled_ = IOHIDManagerOpen(hid_manager_ref_, kIOHIDOptionsTypeNone) ==
             kIOReturnSuccess;
}

void GamepadPlatformDataFetcherMac::UnregisterFromNotifications() {
  IOHIDManagerUnscheduleFromRunLoop(hid_manager_ref_, CFRunLoopGetCurrent(),
                                    kCFRunLoopDefaultMode);
  IOHIDManagerClose(hid_manager_ref_, kIOHIDOptionsTypeNone);
}

void GamepadPlatformDataFetcherMac::PauseHint(bool pause) {
  paused_ = pause;
}

GamepadPlatformDataFetcherMac::~GamepadPlatformDataFetcherMac() {
  UnregisterFromNotifications();
  for (size_t slot = 0; slot < Gamepads::kItemsLengthCap; ++slot) {
    if (devices_[slot] != nullptr)
      devices_[slot]->Shutdown();
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

size_t GamepadPlatformDataFetcherMac::GetEmptySlot() {
  // Find a free slot for this device.
  for (size_t slot = 0; slot < Gamepads::kItemsLengthCap; ++slot) {
    if (devices_[slot] == nullptr)
      return slot;
  }
  return Gamepads::kItemsLengthCap;
}

size_t GamepadPlatformDataFetcherMac::GetSlotForDevice(IOHIDDeviceRef device) {
  for (size_t slot = 0; slot < Gamepads::kItemsLengthCap; ++slot) {
    // If we already have this device, and it's already connected, don't do
    // anything now.
    if (devices_[slot] != nullptr && devices_[slot]->IsSameDevice(device))
      return Gamepads::kItemsLengthCap;
  }
  return GetEmptySlot();
}

size_t GamepadPlatformDataFetcherMac::GetSlotForLocation(int location_id) {
  for (size_t slot = 0; slot < Gamepads::kItemsLengthCap; ++slot) {
    if (devices_[slot] && devices_[slot]->GetLocationId() == location_id)
      return slot;
  }
  return Gamepads::kItemsLengthCap;
}

void GamepadPlatformDataFetcherMac::DeviceAdd(IOHIDDeviceRef device) {
  using base::mac::CFToNSCast;
  using base::mac::CFCastStrict;

  if (!enabled_)
    return;

  NSNumber* location_id = CFToNSCast(CFCastStrict<CFNumberRef>(
      IOHIDDeviceGetProperty(device, CFSTR(kIOHIDLocationIDKey))));
  int location_int = [location_id intValue];

  // Find an index for this device.
  size_t slot = GetSlotForDevice(device);

  // We can't handle this many connected devices.
  if (slot == Gamepads::kItemsLengthCap)
    return;

  NSNumber* vendor_id = CFToNSCast(CFCastStrict<CFNumberRef>(
      IOHIDDeviceGetProperty(device, CFSTR(kIOHIDVendorIDKey))));
  NSNumber* product_id = CFToNSCast(CFCastStrict<CFNumberRef>(
      IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductIDKey))));
  NSNumber* version_number = CFToNSCast(CFCastStrict<CFNumberRef>(
      IOHIDDeviceGetProperty(device, CFSTR(kIOHIDVersionNumberKey))));
  NSString* product = CFToNSCast(CFCastStrict<CFStringRef>(
      IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey))));
  uint16_t vendor_int = [vendor_id intValue];
  uint16_t product_int = [product_id intValue];
  uint16_t version_int = [version_number intValue];

  // The SteelSeries Nimbus and other Made for iOS gamepads should be handled
  // through the GameController interface. Blacklist it here so it doesn't
  // take up an additional gamepad slot.
  if (vendor_int == kVendorSteelSeries && product_int == kProductNimbus)
    return;

  PadState* state = GetPadState(location_int);
  if (!state)
    return;  // No available slot for this device

  state->mapper = GetGamepadStandardMappingFunction(
      vendor_int, product_int, version_int, GAMEPAD_BUS_UNKNOWN);

  NSString* ident =
      [NSString stringWithFormat:@"%@ (%sVendor: %04x Product: %04x)", product,
                                 state->mapper ? "STANDARD GAMEPAD " : "",
                                 vendor_int, product_int];
  CopyNSStringAsUTF16LittleEndian(ident, state->data.id,
                                  sizeof(state->data.id));

  if (state->mapper) {
    CopyNSStringAsUTF16LittleEndian(@"standard", state->data.mapping,
                                    sizeof(state->data.mapping));
  } else {
    state->data.mapping[0] = 0;
  }

  devices_[slot] = std::make_unique<GamepadDeviceMac>(location_int, device,
                                                      vendor_int, product_int);
  if (!devices_[slot]->AddButtonsAndAxes(&state->data)) {
    devices_[slot]->Shutdown();
    devices_[slot] = nullptr;
    return;
  }

  state->data.vibration_actuator.type = GamepadHapticActuatorType::kDualRumble;
  state->data.vibration_actuator.not_null = devices_[slot]->SupportsVibration();

  state->data.connected = true;
}

void GamepadPlatformDataFetcherMac::DeviceRemove(IOHIDDeviceRef device) {
  if (!enabled_)
    return;

  // Find the index for this device.
  size_t slot;
  for (slot = 0; slot < Gamepads::kItemsLengthCap; ++slot) {
    if (devices_[slot] != nullptr && devices_[slot]->IsSameDevice(device))
      break;
  }
  if (slot < Gamepads::kItemsLengthCap) {
    devices_[slot]->Shutdown();
    devices_[slot] = nullptr;
  }
}

void GamepadPlatformDataFetcherMac::ValueChanged(IOHIDValueRef value) {
  if (!enabled_ || paused_)
    return;

  IOHIDElementRef element = IOHIDValueGetElement(value);
  IOHIDDeviceRef device = IOHIDElementGetDevice(element);

  // Find device slot.
  size_t slot;
  for (slot = 0; slot < Gamepads::kItemsLengthCap; ++slot) {
    if (devices_[slot] != nullptr && devices_[slot]->IsSameDevice(device))
      break;
  }
  if (slot == Gamepads::kItemsLengthCap)
    return;

  PadState* state = GetPadState(devices_[slot]->GetLocationId());
  if (!state)
    return;

  devices_[slot]->UpdateGamepadForValue(value, &state->data);
}

void GamepadPlatformDataFetcherMac::GetGamepadData(bool) {
  if (!enabled_)
    return;

  // Loop through and GetPadState to indicate the devices are still connected.
  for (size_t slot = 0; slot < Gamepads::kItemsLengthCap; ++slot) {
    if (devices_[slot])
      GetPadState(devices_[slot]->GetLocationId());
  }
}

void GamepadPlatformDataFetcherMac::PlayEffect(
    int source_id,
    mojom::GamepadHapticEffectType type,
    mojom::GamepadEffectParametersPtr params,
    mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback) {
  size_t slot = GetSlotForLocation(source_id);
  if (slot == Gamepads::kItemsLengthCap) {
    // No connected gamepad with this location. Probably the effect was issued
    // while the gamepad was still connected, so handle this as if it were
    // preempted by a disconnect.
    std::move(callback).Run(
        mojom::GamepadHapticsResult::GamepadHapticsResultPreempted);
    return;
  }
  devices_[slot]->PlayEffect(type, std::move(params), std::move(callback));
}

void GamepadPlatformDataFetcherMac::ResetVibration(
    int source_id,
    mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback) {
  size_t slot = GetSlotForLocation(source_id);
  if (slot == Gamepads::kItemsLengthCap) {
    // No connected gamepad with this location. Since the gamepad is already
    // disconnected, allow the reset to report success.
    std::move(callback).Run(
        mojom::GamepadHapticsResult::GamepadHapticsResultComplete);
    return;
  }
  devices_[slot]->ResetVibration(std::move(callback));
}

}  // namespace device
