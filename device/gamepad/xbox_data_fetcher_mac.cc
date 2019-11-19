// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/xbox_data_fetcher_mac.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/usb/USB.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"

namespace device {

XboxDataFetcher::PendingController::PendingController(
    XboxDataFetcher* fetcher,
    std::unique_ptr<XboxControllerMac> controller)
    : fetcher(fetcher), controller(std::move(controller)) {}

XboxDataFetcher::PendingController::~PendingController() {
  if (controller)
    controller->Shutdown();
}

XboxDataFetcher::XboxDataFetcher() = default;

XboxDataFetcher::~XboxDataFetcher() {
  while (!controllers_.empty()) {
    RemoveController(*controllers_.begin());
  }
  UnregisterFromNotifications();
}

GamepadSource XboxDataFetcher::source() {
  return Factory::static_source();
}

void XboxDataFetcher::GetGamepadData(bool devices_changed_hint) {
  // This just loops through all the connected pads and "pings" them to indicate
  // that they're still active.
  for (auto* controller : controllers_) {
    GetPadState(controller->location_id());
  }
}

void XboxDataFetcher::PlayEffect(
    int source_id,
    mojom::GamepadHapticEffectType type,
    mojom::GamepadEffectParametersPtr params,
    mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  XboxControllerMac* controller = ControllerForLocation(source_id);
  if (!controller) {
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }

  controller->PlayEffect(type, std::move(params), std::move(callback),
                         std::move(callback_runner));
}

void XboxDataFetcher::ResetVibration(
    int source_id,
    mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  XboxControllerMac* controller = ControllerForLocation(source_id);
  if (!controller) {
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }

  controller->ResetVibration(std::move(callback), std::move(callback_runner));
}

void XboxDataFetcher::OnAddedToProvider() {
  RegisterForNotifications();
}

// static
void XboxDataFetcher::DeviceAdded(void* context, io_iterator_t iterator) {
  DCHECK(context);
  XboxDataFetcher* fetcher = static_cast<XboxDataFetcher*>(context);
  io_service_t ref;
  while ((ref = IOIteratorNext(iterator))) {
    base::mac::ScopedIOObject<io_service_t> scoped_ref(ref);
    fetcher->TryOpenDevice(ref);
  }
}

// static
void XboxDataFetcher::DeviceRemoved(void* context, io_iterator_t iterator) {
  DCHECK(context);
  XboxDataFetcher* fetcher = static_cast<XboxDataFetcher*>(context);
  io_service_t ref;
  while ((ref = IOIteratorNext(iterator))) {
    base::mac::ScopedIOObject<io_service_t> scoped_ref(ref);
    base::ScopedCFTypeRef<CFNumberRef> number(
        base::mac::CFCastStrict<CFNumberRef>(IORegistryEntryCreateCFProperty(
            ref, CFSTR(kUSBDevicePropertyLocationID), kCFAllocatorDefault,
            kNilOptions)));
    UInt32 location_id = 0;
    CFNumberGetValue(number, kCFNumberSInt32Type, &location_id);
    fetcher->RemoveControllerByLocationID(location_id);
  }
}

// static
void XboxDataFetcher::InterestCallback(void* context,
                                       io_service_t service,
                                       IOMessage message_type,
                                       void* message_argument) {
  if (message_type == kIOMessageServiceWasClosed) {
    PendingController* pending = static_cast<PendingController*>(context);
    pending->fetcher->PendingControllerBecameAvailable(service, pending);
  }
}

void XboxDataFetcher::PendingControllerBecameAvailable(
    io_service_t service,
    PendingController* pending) {
  // Destroying the PendingController object unregisters our interest
  // notification.
  auto it = pending_controllers_.find(pending);
  if (it != pending_controllers_.end()) {
    pending_controllers_.erase(it);
  }
  TryOpenDevice(service);
}

bool XboxDataFetcher::TryOpenDevice(io_service_t service) {
  auto pending = std::make_unique<PendingController>(
      this, std::make_unique<XboxControllerMac>(this));
  bool did_register_interest =
      RegisterForInterestNotifications(service, pending.get());

  auto* controller = pending->controller.get();
  XboxControllerMac::OpenDeviceResult result = controller->OpenDevice(service);
  if (result == XboxControllerMac::OpenDeviceResult::OPEN_SUCCEEDED) {
    AddController(pending->controller.release());
    return true;
  }

  if (did_register_interest &&
      result ==
          XboxControllerMac::OpenDeviceResult::OPEN_FAILED_EXCLUSIVE_ACCESS) {
    pending_controllers_.insert(std::move(pending));
  }
  return false;
}

bool XboxDataFetcher::RegisterForNotifications() {
  if (listening_)
    return true;
  if (port_ == nullptr)
    port_.reset(IONotificationPortCreate(kIOMasterPortDefault));
  if (!port_.is_valid())
    return false;
  source_ = IONotificationPortGetRunLoopSource(port_.get());
  if (!source_)
    return false;
  CFRunLoopAddSource(CFRunLoopGetCurrent(), source_, kCFRunLoopDefaultMode);

  listening_ = true;

  if (!RegisterForDeviceNotifications(
          XboxControllerMac::kVendorMicrosoft,
          XboxControllerMac::kProductXboxOneEliteController,
          &xbox_one_elite_device_added_iter_,
          &xbox_one_elite_device_removed_iter_))
    return false;

  if (!RegisterForDeviceNotifications(
          XboxControllerMac::kVendorMicrosoft,
          XboxControllerMac::kProductXboxOneController2013,
          &xbox_one_2013_device_added_iter_,
          &xbox_one_2013_device_removed_iter_))
    return false;

  if (!RegisterForDeviceNotifications(
          XboxControllerMac::kVendorMicrosoft,
          XboxControllerMac::kProductXboxOneController2015,
          &xbox_one_2015_device_added_iter_,
          &xbox_one_2015_device_removed_iter_))
    return false;

  if (!RegisterForDeviceNotifications(
          XboxControllerMac::kVendorMicrosoft,
          XboxControllerMac::kProductXboxOneSController,
          &xbox_one_s_device_added_iter_, &xbox_one_s_device_removed_iter_))
    return false;

  if (!RegisterForDeviceNotifications(
          XboxControllerMac::kVendorMicrosoft,
          XboxControllerMac::kProductXbox360Controller,
          &xbox_360_device_added_iter_, &xbox_360_device_removed_iter_))
    return false;

  if (!RegisterForDeviceNotifications(
          XboxControllerMac::kVendorMicrosoft,
          XboxControllerMac::kProductXboxAdaptiveController,
          &xbox_adaptive_device_added_iter_,
          &xbox_adaptive_device_removed_iter_))
    return false;

  return true;
}

bool XboxDataFetcher::RegisterForDeviceNotifications(
    int vendor_id,
    int product_id,
    base::mac::ScopedIOObject<io_iterator_t>* added_iter,
    base::mac::ScopedIOObject<io_iterator_t>* removed_iter) {
  base::ScopedCFTypeRef<CFNumberRef> vendor_cf(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &vendor_id));
  base::ScopedCFTypeRef<CFNumberRef> product_cf(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &product_id));
  base::ScopedCFTypeRef<CFMutableDictionaryRef> matching_dict(
      IOServiceMatching(kIOUSBDeviceClassName));
  if (!matching_dict)
    return false;
  CFDictionarySetValue(matching_dict, CFSTR(kUSBVendorID), vendor_cf);
  CFDictionarySetValue(matching_dict, CFSTR(kUSBProductID), product_cf);

  // IOServiceAddMatchingNotification() releases the dictionary when it's done.
  // Retain it before each call to IOServiceAddMatchingNotification to keep
  // things balanced.
  CFRetain(matching_dict);
  IOReturn ret;
  ret = IOServiceAddMatchingNotification(port_.get(), kIOFirstMatchNotification,
                                         matching_dict, DeviceAdded, this,
                                         added_iter->InitializeInto());
  if (ret != kIOReturnSuccess) {
    LOG(ERROR) << "Error listening for Xbox controller add events: " << ret;
    return false;
  }
  DeviceAdded(this, added_iter->get());

  CFRetain(matching_dict);
  ret = IOServiceAddMatchingNotification(port_.get(), kIOTerminatedNotification,
                                         matching_dict, DeviceRemoved, this,
                                         removed_iter->InitializeInto());
  if (ret != kIOReturnSuccess) {
    LOG(ERROR) << "Error listening for Xbox controller remove events: " << ret;
    return false;
  }
  DeviceRemoved(this, removed_iter->get());
  return true;
}

bool XboxDataFetcher::RegisterForInterestNotifications(
    io_service_t service,
    PendingController* pending) {
  if (port_ == nullptr)
    port_.reset(IONotificationPortCreate(kIOMasterPortDefault));
  if (!port_.is_valid())
    return false;

  kern_return_t kr = IOServiceAddInterestNotification(
      port_.get(), service, kIOGeneralInterest, InterestCallback, pending,
      pending->notify.InitializeInto());
  return kr == KERN_SUCCESS;
}

void XboxDataFetcher::UnregisterFromNotifications() {
  if (!listening_)
    return;
  listening_ = false;
  if (source_)
    CFRunLoopSourceInvalidate(source_);
  port_.reset();
  pending_controllers_.clear();
}

XboxControllerMac* XboxDataFetcher::ControllerForLocation(UInt32 location_id) {
  for (std::set<XboxControllerMac*>::iterator i = controllers_.begin();
       i != controllers_.end(); ++i) {
    if ((*i)->location_id() == location_id)
      return *i;
  }
  return NULL;
}

void XboxDataFetcher::AddController(XboxControllerMac* controller) {
  DCHECK(controller);
  DCHECK(!ControllerForLocation(controller->location_id()))
      << "Controller with location ID " << controller->location_id()
      << " already exists in the set of controllers.";
  PadState* state = GetPadState(controller->location_id());
  if (!state) {
    delete controller;
    return;  // No available slot for this device
  }

  controllers_.insert(controller);

  controller->SetLEDPattern((XboxControllerMac::LEDPattern)(
      XboxControllerMac::LED_FLASH_TOP_LEFT + controller->location_id()));

  state->data.SetID(base::UTF8ToUTF16(controller->GetIdString()));
  state->data.mapping = GamepadMapping::kStandard;
  state->data.connected = true;
  state->data.axes_length = 4;
  state->data.buttons_length = 17;
  state->data.timestamp = CurrentTimeInMicroseconds();
  state->mapper = 0;
  state->axis_mask = 0;
  state->button_mask = 0;

  state->data.vibration_actuator.type = GamepadHapticActuatorType::kDualRumble;
  state->data.vibration_actuator.not_null = controller->SupportsVibration();
}

void XboxDataFetcher::RemoveController(XboxControllerMac* controller) {
  DCHECK(controller);
  controller->Shutdown();
  controllers_.erase(controller);
  delete controller;
}

void XboxDataFetcher::RemoveControllerByLocationID(uint32_t location_id) {
  XboxControllerMac* controller = NULL;
  for (std::set<XboxControllerMac*>::iterator i = controllers_.begin();
       i != controllers_.end(); ++i) {
    if ((*i)->location_id() == location_id) {
      controller = *i;
      break;
    }
  }
  if (controller)
    RemoveController(controller);
}

void XboxDataFetcher::XboxControllerGotData(
    XboxControllerMac* controller,
    const XboxControllerMac::Data& data) {
  PadState* state = GetPadState(controller->location_id());
  if (!state)
    return;  // No available slot for this device

  Gamepad& pad = state->data;

  for (size_t i = 0; i < 6; i++) {
    pad.buttons[i].pressed = data.buttons[i];
    pad.buttons[i].value = data.buttons[i] ? 1.0f : 0.0f;
  }
  pad.buttons[6].pressed =
      data.triggers[0] > GamepadButton::kDefaultButtonPressedThreshold;
  pad.buttons[6].value = data.triggers[0];
  pad.buttons[7].pressed =
      data.triggers[1] > GamepadButton::kDefaultButtonPressedThreshold;
  pad.buttons[7].value = data.triggers[1];
  for (size_t i = 8; i < 16; i++) {
    pad.buttons[i].pressed = data.buttons[i - 2];
    pad.buttons[i].value = data.buttons[i - 2] ? 1.0f : 0.0f;
  }
  if (controller->GetControllerType() ==
      XboxControllerMac::XBOX_360_CONTROLLER) {
    pad.buttons[16].pressed = data.buttons[14];
    pad.buttons[16].value = data.buttons[14] ? 1.0f : 0.0f;
  }
  for (size_t i = 0; i < base::size(data.axes); i++) {
    pad.axes[i] = data.axes[i];
  }

  pad.timestamp = CurrentTimeInMicroseconds();
}

void XboxDataFetcher::XboxControllerGotGuideData(XboxControllerMac* controller,
                                                 bool guide) {
  PadState* state = GetPadState(controller->location_id());
  if (!state)
    return;  // No available slot for this device

  Gamepad& pad = state->data;

  pad.buttons[16].pressed = guide;
  pad.buttons[16].value = guide ? 1.0f : 0.0f;

  pad.timestamp = CurrentTimeInMicroseconds();
}

void XboxDataFetcher::XboxControllerError(XboxControllerMac* controller) {
  RemoveController(controller);
}

}  // namespace device
