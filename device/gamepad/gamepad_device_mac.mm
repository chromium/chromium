// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_device_mac.h"

#import <Foundation/Foundation.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "device/gamepad/dualshock4_controller.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/hid_haptic_gamepad.h"
#include "device/gamepad/hid_writer_mac.h"
#include "device/gamepad/xbox_hid_controller.h"

namespace device {

namespace {
// http://www.usb.org/developers/hidpage
const uint16_t kGenericDesktopUsagePage = 0x01;
const uint16_t kGameControlsUsagePage = 0x05;
const uint16_t kButtonUsagePage = 0x09;
const uint16_t kConsumerUsagePage = 0x0c;

const uint16_t kJoystickUsageNumber = 0x04;
const uint16_t kGameUsageNumber = 0x05;
const uint16_t kMultiAxisUsageNumber = 0x08;
const uint16_t kAxisMinimumUsageNumber = 0x30;
const uint16_t kSystemMainMenuUsageNumber = 0x85;
const uint16_t kPowerUsageNumber = 0x30;
const uint16_t kSearchUsageNumber = 0x0221;
const uint16_t kHomeUsageNumber = 0x0223;
const uint16_t kBackUsageNumber = 0x0224;

const int kRumbleMagnitudeMax = 10000;

struct SpecialUsages {
  const uint16_t usage_page;
  const uint16_t usage;
} kSpecialUsages[] = {
    // Xbox One S pre-FW update reports Xbox button as SystemMainMenu over BT.
    {kGenericDesktopUsagePage, kSystemMainMenuUsageNumber},
    // Power is used for the Guide button on the Nvidia Shield 2015 gamepad.
    {kConsumerUsagePage, kPowerUsageNumber},
    // Search is used for the Guide button on the Nvidia Shield 2017 gamepad.
    {kConsumerUsagePage, kSearchUsageNumber},
    // Start, Back, and Guide buttons are often reported as Consumer Home or
    // Back.
    {kConsumerUsagePage, kHomeUsageNumber},
    {kConsumerUsagePage, kBackUsageNumber},
};
const size_t kSpecialUsagesLen = base::size(kSpecialUsages);

float NormalizeAxis(CFIndex value, CFIndex min, CFIndex max) {
  return (2.f * (value - min) / static_cast<float>(max - min)) - 1.f;
}

float NormalizeUInt8Axis(uint8_t value, uint8_t min, uint8_t max) {
  return (2.f * (value - min) / static_cast<float>(max - min)) - 1.f;
}

float NormalizeUInt16Axis(uint16_t value, uint16_t min, uint16_t max) {
  return (2.f * (value - min) / static_cast<float>(max - min)) - 1.f;
}

float NormalizeUInt32Axis(uint32_t value, uint32_t min, uint32_t max) {
  return (2.f * (value - min) / static_cast<float>(max - min)) - 1.f;
}

GamepadBusType QueryBusType(IOHIDDeviceRef device) {
  CFStringRef transport_cf = base::mac::CFCast<CFStringRef>(
      IOHIDDeviceGetProperty(device, CFSTR(kIOHIDTransportKey)));
  if (transport_cf) {
    std::string transport = base::SysCFStringRefToUTF8(transport_cf);
    if (transport == kIOHIDTransportUSBValue)
      return GAMEPAD_BUS_USB;
    if (transport == kIOHIDTransportBluetoothValue ||
        transport == kIOHIDTransportBluetoothLowEnergyValue) {
      return GAMEPAD_BUS_BLUETOOTH;
    }
  }
  return GAMEPAD_BUS_UNKNOWN;
}

}  // namespace

GamepadDeviceMac::GamepadDeviceMac(int location_id,
                                   IOHIDDeviceRef device_ref,
                                   int vendor_id,
                                   int product_id)
    : location_id_(location_id),
      device_ref_(device_ref),
      bus_type_(QueryBusType(device_ref_)),
      ff_device_ref_(nullptr),
      ff_effect_ref_(nullptr) {
  if (Dualshock4Controller::IsDualshock4(vendor_id, product_id)) {
    dualshock4_ = std::make_unique<Dualshock4Controller>(
        vendor_id, product_id, bus_type_,
        std::make_unique<HidWriterMac>(device_ref));
    return;
  }

  if (XboxHidController::IsXboxHid(vendor_id, product_id)) {
    xbox_hid_ = std::make_unique<XboxHidController>(
        std::make_unique<HidWriterMac>(device_ref));
    return;
  }

  if (HidHapticGamepad::IsHidHaptic(vendor_id, product_id)) {
    hid_haptics_ = HidHapticGamepad::Create(
        vendor_id, product_id, std::make_unique<HidWriterMac>(device_ref));
    return;
  }

  if (device_ref) {
    ff_device_ref_ = CreateForceFeedbackDevice(device_ref);
    if (ff_device_ref_) {
      ff_effect_ref_ = CreateForceFeedbackEffect(ff_device_ref_, &ff_effect_,
                                                 &ff_custom_force_, force_data_,
                                                 axes_data_, direction_data_);
    }
  }
}

GamepadDeviceMac::~GamepadDeviceMac() = default;

void GamepadDeviceMac::DoShutdown() {
  if (ff_device_ref_) {
    if (ff_effect_ref_) {
      FFDeviceReleaseEffect(ff_device_ref_, ff_effect_ref_);
      ff_effect_ref_ = nullptr;
    }
    FFReleaseDevice(ff_device_ref_);
    ff_device_ref_ = nullptr;
  }
  if (dualshock4_)
    dualshock4_->Shutdown();
  dualshock4_.reset();
  if (xbox_hid_)
    xbox_hid_->Shutdown();
  xbox_hid_.reset();
  if (hid_haptics_)
    hid_haptics_->Shutdown();
  hid_haptics_.reset();
}

// static
bool GamepadDeviceMac::CheckCollection(IOHIDElementRef element) {
  // Check that a parent collection of this element matches one of the usage
  // numbers that we are looking for.
  while ((element = IOHIDElementGetParent(element)) != nullptr) {
    uint32_t usage_page = IOHIDElementGetUsagePage(element);
    uint32_t usage = IOHIDElementGetUsage(element);
    if (usage_page == kGenericDesktopUsagePage) {
      if (usage == kJoystickUsageNumber || usage == kGameUsageNumber ||
          usage == kMultiAxisUsageNumber) {
        return true;
      }
    }
  }
  return false;
}

bool GamepadDeviceMac::AddButtonsAndAxes(Gamepad* gamepad) {
  bool has_buttons = AddButtons(gamepad);
  bool has_axes = AddAxes(gamepad);
  gamepad->timestamp = GamepadDataFetcher::CurrentTimeInMicroseconds();
  return (has_buttons || has_axes);
}

bool GamepadDeviceMac::AddButtons(Gamepad* gamepad) {
  base::ScopedCFTypeRef<CFArrayRef> elements_cf(IOHIDDeviceCopyMatchingElements(
      device_ref_, nullptr, kIOHIDOptionsTypeNone));
  NSArray* elements = base::mac::CFToNSCast(elements_cf);
  DCHECK(elements);
  DCHECK(gamepad);
  memset(gamepad->buttons, 0, sizeof(gamepad->buttons));
  std::fill(button_elements_, button_elements_ + Gamepad::kButtonsLengthCap,
            nullptr);

  std::vector<IOHIDElementRef> special_element(kSpecialUsagesLen, nullptr);
  size_t button_count = 0;
  size_t unmapped_button_count = 0;
  for (id elem in elements) {
    IOHIDElementRef element = reinterpret_cast<IOHIDElementRef>(elem);
    if (!CheckCollection(element))
      continue;

    uint32_t usage_page = IOHIDElementGetUsagePage(element);
    uint32_t usage = IOHIDElementGetUsage(element);
    if (IOHIDElementGetType(element) == kIOHIDElementTypeInput_Button) {
      if (usage_page == kButtonUsagePage && usage > 0) {
        size_t button_index = size_t{usage - 1};

        // Ignore buttons with large usage values.
        if (button_index >= Gamepad::kButtonsLengthCap)
          continue;

        // Button index already assigned, ignore.
        if (button_elements_[button_index])
          continue;

        button_elements_[button_index] = element;
        button_count = std::max(button_count, button_index + 1);
      } else {
        // Check for common gamepad buttons that are not on the Button usage
        // page. Button indices are assigned in a second pass.
        for (size_t special_index = 0; special_index < kSpecialUsagesLen;
             ++special_index) {
          const auto& special = kSpecialUsages[special_index];
          if (usage_page == special.usage_page && usage == special.usage) {
            special_element[special_index] = element;
            ++unmapped_button_count;
          }
        }
      }
    }
  }

  if (unmapped_button_count > 0) {
    // Insert unmapped buttons at unused button indices.
    size_t button_index = 0;
    for (size_t special_index = 0; special_index < kSpecialUsagesLen;
         ++special_index) {
      if (!special_element[special_index])
        continue;

      // Advance to the next unused button index.
      while (button_index < Gamepad::kButtonsLengthCap &&
             button_elements_[button_index]) {
        ++button_index;
      }
      if (button_index >= Gamepad::kButtonsLengthCap)
        break;

      button_elements_[button_index] = special_element[special_index];
      button_count = std::max(button_count, button_index + 1);

      if (--unmapped_button_count == 0)
        break;
    }
  }

  gamepad->buttons_length = button_count;
  return gamepad->buttons_length > 0;
}

bool GamepadDeviceMac::AddAxes(Gamepad* gamepad) {
  base::ScopedCFTypeRef<CFArrayRef> elements_cf(IOHIDDeviceCopyMatchingElements(
      device_ref_, nullptr, kIOHIDOptionsTypeNone));
  NSArray* elements = base::mac::CFToNSCast(elements_cf);
  DCHECK(elements);
  DCHECK(gamepad);
  memset(gamepad->axes, 0, sizeof(gamepad->axes));
  std::fill(axis_elements_, axis_elements_ + Gamepad::kAxesLengthCap, nullptr);
  std::fill(axis_minimums_, axis_minimums_ + Gamepad::kAxesLengthCap, 0);
  std::fill(axis_maximums_, axis_maximums_ + Gamepad::kAxesLengthCap, 0);
  std::fill(axis_report_sizes_, axis_report_sizes_ + Gamepad::kAxesLengthCap,
            0);

  // Most axes are mapped so that their index in the Gamepad axes array
  // corresponds to the usage ID. However, this is not possible when the usage
  // ID would cause the axis index to exceed the bounds of the axes array.
  // Axes with large usage IDs are mapped in a second pass.
  size_t axis_count = 0;
  size_t unmapped_axis_count = 0;

  for (id elem in elements) {
    IOHIDElementRef element = reinterpret_cast<IOHIDElementRef>(elem);
    if (!CheckCollection(element))
      continue;

    uint32_t usage_page = IOHIDElementGetUsagePage(element);
    uint32_t usage = IOHIDElementGetUsage(element);
    if (IOHIDElementGetType(element) != kIOHIDElementTypeInput_Misc ||
        usage < kAxisMinimumUsageNumber) {
      continue;
    }

    size_t axis_index = size_t{usage - kAxisMinimumUsageNumber};
    if (axis_index < Gamepad::kAxesLengthCap) {
      // Axis index already assigned, ignore.
      if (axis_elements_[axis_index])
        continue;
      axis_elements_[axis_index] = element;
      axis_count = std::max(axis_count, axis_index + 1);
    } else if (usage_page <= kGameControlsUsagePage) {
      // Assign an index for this axis in the second pass.
      ++unmapped_axis_count;
    }
  }

  if (unmapped_axis_count > 0) {
    // Insert unmapped axes at unused axis indices.
    size_t axis_index = 0;
    for (id elem in elements) {
      IOHIDElementRef element = reinterpret_cast<IOHIDElementRef>(elem);
      if (!CheckCollection(element))
        continue;

      uint32_t usage_page = IOHIDElementGetUsagePage(element);
      uint32_t usage = IOHIDElementGetUsage(element);
      if (IOHIDElementGetType(element) != kIOHIDElementTypeInput_Misc ||
          usage < kAxisMinimumUsageNumber ||
          usage_page > kGameControlsUsagePage) {
        continue;
      }

      // Ignore axes with small usage IDs that should have been mapped in the
      // initial pass.
      if (size_t{usage - kAxisMinimumUsageNumber} < Gamepad::kAxesLengthCap)
        continue;

      // Advance to the next unused axis index.
      while (axis_index < Gamepad::kAxesLengthCap &&
             axis_elements_[axis_index]) {
        ++axis_index;
      }
      if (axis_index >= Gamepad::kAxesLengthCap)
        break;

      axis_elements_[axis_index] = element;
      axis_count = std::max(axis_count, axis_index + 1);

      if (--unmapped_axis_count == 0)
        break;
    }
  }

  // Fetch the logical range and report size for each axis.
  for (size_t axis_index = 0; axis_index < axis_count; ++axis_index) {
    IOHIDElementRef element = axis_elements_[axis_index];
    if (element != nullptr) {
      CFIndex axis_min = IOHIDElementGetLogicalMin(element);
      CFIndex axis_max = IOHIDElementGetLogicalMax(element);

      // Some HID axes report a logical range of -1 to 0 signed, which must be
      // interpreted as 0 to -1 unsigned for correct normalization behavior.
      if (axis_min == -1 && axis_max == 0) {
        axis_max = -1;
        axis_min = 0;
      }

      axis_minimums_[axis_index] = axis_min;
      axis_maximums_[axis_index] = axis_max;
      axis_report_sizes_[axis_index] = IOHIDElementGetReportSize(element);
    }
  }

  gamepad->axes_length = axis_count;
  return gamepad->axes_length > 0;
}

void GamepadDeviceMac::UpdateGamepadForValue(IOHIDValueRef value,
                                             Gamepad* gamepad) {
  DCHECK(gamepad);
  IOHIDElementRef element = IOHIDValueGetElement(value);
  uint32_t value_length = IOHIDValueGetLength(value);

  if (dualshock4_) {
    // Handle Dualshock4 input reports that do not specify HID gamepad usages
    // in the report descriptor.
    uint32_t report_id = IOHIDElementGetReportID(element);
    auto report = base::make_span(IOHIDValueGetBytePtr(value), value_length);
    if (dualshock4_->ProcessInputReport(report_id, report, gamepad))
      return;
  }

  // Values larger than 4 bytes cannot be handled by IOHIDValueGetIntegerValue.
  if (value_length > 4)
    return;

  // Find and fill in the associated button event, if any.
  for (size_t i = 0; i < gamepad->buttons_length; ++i) {
    if (button_elements_[i] == element) {
      bool pressed = IOHIDValueGetIntegerValue(value);
      gamepad->buttons[i].pressed = pressed;
      gamepad->buttons[i].value = pressed ? 1.f : 0.f;
      gamepad->timestamp = GamepadDataFetcher::CurrentTimeInMicroseconds();
      return;
    }
  }

  // Find and fill in the associated axis event, if any.
  for (size_t i = 0; i < gamepad->axes_length; ++i) {
    if (axis_elements_[i] == element) {
      CFIndex axis_min = axis_minimums_[i];
      CFIndex axis_max = axis_maximums_[i];
      CFIndex axis_value = IOHIDValueGetIntegerValue(value);

      if (axis_min > axis_max) {
        // We'll need to interpret this axis as unsigned during normalization.
        switch (axis_report_sizes_[i]) {
          case 8:
            gamepad->axes[i] =
                NormalizeUInt8Axis(axis_value, axis_min, axis_max);
            break;
          case 16:
            gamepad->axes[i] =
                NormalizeUInt16Axis(axis_value, axis_min, axis_max);
            break;
          case 32:
            gamepad->axes[i] =
                NormalizeUInt32Axis(axis_value, axis_min, axis_max);
            break;
        }
      } else {
        gamepad->axes[i] = NormalizeAxis(axis_value, axis_min, axis_max);
      }
      gamepad->timestamp = GamepadDataFetcher::CurrentTimeInMicroseconds();
      return;
    }
  }
}

bool GamepadDeviceMac::SupportsVibration() {
  return dualshock4_ || xbox_hid_ || hid_haptics_ || ff_device_ref_;
}

void GamepadDeviceMac::SetVibration(double strong_magnitude,
                                    double weak_magnitude) {
  if (dualshock4_) {
    dualshock4_->SetVibration(strong_magnitude, weak_magnitude);
    return;
  }

  if (xbox_hid_) {
    xbox_hid_->SetVibration(strong_magnitude, weak_magnitude);
    return;
  }

  if (hid_haptics_) {
    hid_haptics_->SetVibration(strong_magnitude, weak_magnitude);
    return;
  }

  if (ff_device_ref_) {
    FFCUSTOMFORCE* ff_custom_force =
        static_cast<FFCUSTOMFORCE*>(ff_effect_.lpvTypeSpecificParams);
    DCHECK(ff_custom_force);
    DCHECK(ff_custom_force->rglForceData);

    ff_custom_force->rglForceData[0] =
        static_cast<LONG>(strong_magnitude * kRumbleMagnitudeMax);
    ff_custom_force->rglForceData[1] =
        static_cast<LONG>(weak_magnitude * kRumbleMagnitudeMax);

    // Download the effect to the device and start the effect.
    HRESULT res = FFEffectSetParameters(
        ff_effect_ref_, &ff_effect_,
        FFEP_DURATION | FFEP_STARTDELAY | FFEP_TYPESPECIFICPARAMS);
    if (res == FF_OK)
      FFEffectStart(ff_effect_ref_, 1, FFES_SOLO);
  }
}

void GamepadDeviceMac::SetZeroVibration() {
  if (dualshock4_) {
    dualshock4_->SetZeroVibration();
    return;
  }

  if (xbox_hid_) {
    xbox_hid_->SetZeroVibration();
    return;
  }

  if (hid_haptics_) {
    hid_haptics_->SetZeroVibration();
    return;
  }

  if (ff_effect_ref_)
    FFEffectStop(ff_effect_ref_);
}

// static
FFDeviceObjectReference GamepadDeviceMac::CreateForceFeedbackDevice(
    IOHIDDeviceRef device_ref) {
  io_service_t service = IOHIDDeviceGetService(device_ref);

  if (service == MACH_PORT_NULL)
    return nullptr;

  HRESULT res = FFIsForceFeedback(service);
  if (res != FF_OK)
    return nullptr;

  FFDeviceObjectReference ff_device_ref;
  res = FFCreateDevice(service, &ff_device_ref);
  if (res != FF_OK)
    return nullptr;

  return ff_device_ref;
}

// static
FFEffectObjectReference GamepadDeviceMac::CreateForceFeedbackEffect(
    FFDeviceObjectReference ff_device_ref,
    FFEFFECT* ff_effect,
    FFCUSTOMFORCE* ff_custom_force,
    LONG* force_data,
    DWORD* axes_data,
    LONG* direction_data) {
  DCHECK(ff_effect);
  DCHECK(ff_custom_force);
  DCHECK(force_data);
  DCHECK(axes_data);
  DCHECK(direction_data);

  FFCAPABILITIES caps;
  HRESULT res = FFDeviceGetForceFeedbackCapabilities(ff_device_ref, &caps);
  if (res != FF_OK)
    return nullptr;

  if ((caps.supportedEffects & FFCAP_ET_CUSTOMFORCE) == 0)
    return nullptr;

  force_data[0] = 0;
  force_data[1] = 0;
  axes_data[0] = caps.ffAxes[0];
  axes_data[1] = caps.ffAxes[1];
  direction_data[0] = 0;
  direction_data[1] = 0;
  ff_custom_force->cChannels = 2;
  ff_custom_force->cSamples = 2;
  ff_custom_force->rglForceData = force_data;
  ff_custom_force->dwSamplePeriod = 100000;  // 100 ms
  ff_effect->dwSize = sizeof(FFEFFECT);
  ff_effect->dwFlags = FFEFF_OBJECTOFFSETS | FFEFF_SPHERICAL;
  ff_effect->dwDuration = 5000000;     // 5 seconds
  ff_effect->dwSamplePeriod = 100000;  // 100 ms
  ff_effect->dwGain = 10000;
  ff_effect->dwTriggerButton = FFEB_NOTRIGGER;
  ff_effect->dwTriggerRepeatInterval = 0;
  ff_effect->cAxes = caps.numFfAxes;
  ff_effect->rgdwAxes = axes_data;
  ff_effect->rglDirection = direction_data;
  ff_effect->lpEnvelope = nullptr;
  ff_effect->cbTypeSpecificParams = sizeof(FFCUSTOMFORCE);
  ff_effect->lpvTypeSpecificParams = ff_custom_force;
  ff_effect->dwStartDelay = 0;

  FFEffectObjectReference ff_effect_ref;
  res = FFDeviceCreateEffect(ff_device_ref, kFFEffectType_CustomForce_ID,
                             ff_effect, &ff_effect_ref);
  if (res != FF_OK)
    return nullptr;

  return ff_effect_ref;
}

base::WeakPtr<AbstractHapticGamepad> GamepadDeviceMac::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
