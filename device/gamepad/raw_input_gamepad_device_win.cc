// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <string_view>

#include "raw_input_gamepad_device_win.h"

// NOTE: <hidsdi.h> must be included before <hidpi.h>. clang-format will want to
// reorder them.
// clang-format off
extern "C" {
#include <hidsdi.h>
#include <hidpi.h>
}
// clang-format on

#include <algorithm>
#include <optional>

#include "base/strings/string_util_win.h"
#include "base/strings/sys_string_conversions.h"
#include "device/gamepad/dualshock4_controller.h"
#include "device/gamepad/gamepad_blocklist.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/hid_haptic_gamepad.h"
#include "device/gamepad/hid_writer_win.h"
#include "device/gamepad/public/cpp/gamepad_features.h"

namespace device {

namespace {

constexpr uint32_t kGenericDesktopUsagePage = 0x01;
constexpr uint32_t kGameControlsUsagePage = 0x05;
constexpr uint32_t kButtonUsagePage = 0x09;
constexpr uint32_t kConsumerUsagePage = 0x0c;

constexpr uint32_t kAxisMinimumUsageNumber = 0x30;
constexpr uint32_t kSystemMainMenuUsageNumber = 0x85;
constexpr uint32_t kPowerUsageNumber = 0x30;
constexpr uint32_t kSearchUsageNumber = 0x0221;
constexpr uint32_t kHomeUsageNumber = 0x0223;
constexpr uint32_t kBackUsageNumber = 0x0224;

// The fetcher will collect all HID usages from the Button usage page and any
// additional usages listed below.
constexpr struct SpecialUsages {
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
constexpr size_t kSpecialUsagesLen = std::size(kSpecialUsages);

// Scales |value| from the range |min| <= x <= |max| to a Standard Gamepad axis
// value in the range -1.0 <= x <= 1.0.
template <class T>
float NormalizeAxis(T value, T min, T max) {
  if (min == max)
    return 0.0f;

  return (2.0f * (value - min) / static_cast<float>(max - min)) - 1.0f;
}

// Returns a 32-bit mask with the lowest |bits| bits set.
unsigned long GetBitmask(unsigned short bits) {
  return (1 << bits) - 1;
}

// Interprets `value` as a signed value with `bits` bits and extends the sign
// bit as needed. Assumes all higher-order bits in `value` are already zero.
int32_t SignExtend(uint32_t value, size_t bits) {
  const int32_t mask = 1U << (bits - 1);
  return (value ^ mask) - mask;
}

}  // namespace

RawInputGamepadDeviceWin::RawInputGamepadDeviceWin(HANDLE device_handle,
                                                   int source_id)
    : handle_(device_handle),
      source_id_(source_id),
      last_update_timestamp_(GamepadDataFetcher::CurrentTimeInMicroseconds()),
      button_report_id_(Gamepad::kButtonsLengthCap, std::nullopt) {
  ::ZeroMemory(buttons_, sizeof(buttons_));
  ::ZeroMemory(axes_, sizeof(axes_));

  is_valid_ = QueryDeviceInfo();
  if (!is_valid_)
    return;

  const std::string product_name = base::SysWideToUTF8(product_string_);
  const GamepadId gamepad_id =
      GamepadIdList::Get().GetGamepadId(product_name, vendor_id_, product_id_);
  if (Dualshock4Controller::IsDualshock4(gamepad_id)) {
    // Dualshock4 has different behavior over USB and Bluetooth, but the
    // RawInput API does not indicate which transport is in use. Detect the
    // transport type by inspecting the version number reported by the device.
    GamepadBusType bus_type =
        Dualshock4Controller::BusTypeFromVersionNumber(version_number_);
    dualshock4_ = std::make_unique<Dualshock4Controller>(
        gamepad_id, bus_type, std::make_unique<HidWriterWin>(handle_));
  } else if (HidHapticGamepad::IsHidHaptic(vendor_id_, product_id_)) {
    hid_haptics_ = HidHapticGamepad::Create(
        vendor_id_, product_id_, std::make_unique<HidWriterWin>(handle_));
  }
}

RawInputGamepadDeviceWin::~RawInputGamepadDeviceWin() = default;

// static
bool RawInputGamepadDeviceWin::IsGamepadUsageId(uint16_t usage) {
  return usage == kGenericDesktopJoystick || usage == kGenericDesktopGamePad ||
         usage == kGenericDesktopMultiAxisController;
}

void RawInputGamepadDeviceWin::DoShutdown() {
  if (dualshock4_)
    dualshock4_->Shutdown();
  dualshock4_.reset();
  if (hid_haptics_)
    hid_haptics_->Shutdown();
  hid_haptics_.reset();
}

void RawInputGamepadDeviceWin::UpdateGamepad(RAWINPUT* input) {
  NTSTATUS status;

  if (dualshock4_) {
    // Handle Dualshock4 input reports that do not specify HID gamepad usages in
    // the report descriptor.
    uint8_t report_id = input->data.hid.bRawData[0];
    auto report = base::make_span(input->data.hid.bRawData + 1,
                                  input->data.hid.dwSizeHid);
    Gamepad pad;
    bool is_multitouch_enabled = features::IsGamepadMultitouchEnabled();
    if (dualshock4_->ProcessInputReport(report_id, report, &pad, false,
                                        is_multitouch_enabled)) {
      for (size_t i = 0; i < Gamepad::kAxesLengthCap; ++i)
        axes_[i].value = pad.axes[i];
      for (size_t i = 0; i < Gamepad::kButtonsLengthCap; ++i)
        buttons_[i] = pad.buttons[i].pressed;

      if (is_multitouch_enabled) {
        const GamepadTouch* touches = pad.touch_events;
        for (size_t i = 0; i < Gamepad::kTouchEventsLengthCap; ++i) {
          touches_[i].touch_id = touches[i].touch_id;
          touches_[i].surface_id = touches[i].surface_id;
          touches_[i].x = touches[i].x;
          touches_[i].y = touches[i].y;
          touches_[i].surface_width = touches[i].surface_width;
          touches_[i].surface_height = touches[i].surface_height;
        }
        touches_length_ = pad.touch_events_length;
        supports_touch_events_ = pad.supports_touch_events_;
      }

      last_update_timestamp_ = GamepadDataFetcher::CurrentTimeInMicroseconds();
      return;
    }
  }

  // Query button state.
  if (buttons_length_ > 0) {
    ULONG buttons_length = 0;

    HidP_GetUsagesEx(HidP_Input, 0, nullptr, &buttons_length, preparsed_data_,
                     reinterpret_cast<PCHAR>(input->data.hid.bRawData),
                     input->data.hid.dwSizeHid);

    auto usages = base::HeapArray<USAGE_AND_PAGE>::Uninit(buttons_length);
    status = HidP_GetUsagesEx(HidP_Input, 0, usages.data(), &buttons_length,
                              preparsed_data_,
                              reinterpret_cast<PCHAR>(input->data.hid.bRawData),
                              input->data.hid.dwSizeHid);

    uint8_t report_id = input->data.hid.bRawData[0];
    // Clear the button state of buttons contained in this report
    for (size_t j = 0; j < button_report_id_.size(); j++) {
      if (button_report_id_[j].has_value() &&
          button_report_id_[j].value() == report_id) {
        buttons_[j] = false;
      }
    }

    if (status == HIDP_STATUS_SUCCESS) {
      // Set each reported button to true.
      for (size_t j = 0; j < buttons_length; j++) {
        uint16_t usage_page = usages[j].UsagePage;
        uint16_t usage = usages[j].Usage;
        if (usage_page == kButtonUsagePage && usage > 0) {
          size_t button_index = static_cast<size_t>(usage - 1);
          if (button_index < Gamepad::kButtonsLengthCap)
            buttons_[button_index] = true;
        } else if (usage_page != kButtonUsagePage &&
                   !special_button_map_.empty()) {
          for (size_t special_index = 0; special_index < kSpecialUsagesLen;
               ++special_index) {
            int button_index = special_button_map_[special_index];
            if (button_index < 0)
              continue;
            const auto& special = kSpecialUsages[special_index];
            if (usage_page == special.usage_page && usage == special.usage)
              buttons_[button_index] = true;
          }
        }
      }
    }
  }

  // Update axis state.
  for (uint32_t axis_index = 0; axis_index < axes_length_; ++axis_index)
    UpdateAxisValue(axis_index, *input);

  last_update_timestamp_ = GamepadDataFetcher::CurrentTimeInMicroseconds();
}

void RawInputGamepadDeviceWin::ReadPadState(Gamepad* pad) const {
  DCHECK(pad);

  pad->timestamp = last_update_timestamp_;
  pad->buttons_length = buttons_length_;
  pad->axes_length = axes_length_;
  pad->axes_used = axes_used_;

  for (uint32_t i = 0u; i < buttons_length_; i++) {
    pad->buttons[i].used = button_report_id_[i].has_value();
    pad->buttons[i].pressed = buttons_[i];
    pad->buttons[i].value = buttons_[i] ? 1.0 : 0.0;
  }

  for (uint32_t i = 0u; i < axes_length_; i++) {
    pad->axes[i] = axes_[i].value;
  }

  if (features::IsGamepadMultitouchEnabled()) {
    pad->supports_touch_events_ = supports_touch_events_;
    pad->touch_events_length = touches_length_;
    for (uint32_t i = 0u; i < touches_length_; i++) {
      pad->touch_events[i].touch_id = touches_[i].touch_id;
      pad->touch_events[i].surface_id = touches_[i].surface_id;
      pad->touch_events[i].x = touches_[i].x;
      pad->touch_events[i].y = touches_[i].y;
      pad->touch_events[i].surface_width = touches_[i].surface_width;
      pad->touch_events[i].surface_height = touches_[i].surface_height;
    }
  }
}

bool RawInputGamepadDeviceWin::SupportsVibration() const {
  return dualshock4_ || hid_haptics_;
}

void RawInputGamepadDeviceWin::SetVibration(
    mojom::GamepadEffectParametersPtr params) {
  if (dualshock4_)
    dualshock4_->SetVibration(std::move(params));
  else if (hid_haptics_)
    hid_haptics_->SetVibration(std::move(params));
}

bool RawInputGamepadDeviceWin::QueryDeviceInfo() {
  // Fetch HID properties (RID_DEVICE_INFO_HID) for this device. This includes
  // |vendor_id_|, |product_id_|, |version_number_|, and |usage_|.
  if (!QueryHidInfo())
    return false;

  // Make sure this device is of a type that we want to observe.
  if (!IsGamepadUsageId(usage_))
    return false;

  // Filter out devices that have gamepad-like HID usages but aren't gamepads.
  if (GamepadIsExcluded(vendor_id_, product_id_))
    return false;

  // Fetch the device's |name_| (RIDI_DEVICENAME).
  if (!QueryDeviceName())
    return false;

  // From the name we can guess at the bus type. PCI HID devices have "VEN" and
  // "DEV" instead of "VID" and "PID". PCI HID devices are typically not
  // gamepads and are ignored.
  // Example PCI device name: \\?\HID#VEN_1234&DEV_ABCD
  // TODO(crbug.com/41412324): Potentially allow PCI HID devices to be
  // enumerated, but prefer known gamepads when there is contention.
  std::wstring pci_prefix = L"\\\\?\\HID#VEN_";
  if (!name_.compare(0, pci_prefix.size(), pci_prefix))
    return false;

  // Filter out the virtual digitizer, which was observed after remote desktop.
  // See https://crbug.com/961774 for details.
  if (name_ == L"\\\\?\\VIRTUAL_DIGITIZER")
    return false;

  // We can now use the name to query the OS for a file handle that is used to
  // read the product string from the device. If the OS does not return a valid
  // handle this gamepad is invalid.
  auto hid_handle = OpenHidHandle();
  if (!hid_handle.IsValid())
    return false;

  // Fetch the human-friendly |product_string_|, if available.
  if (!QueryProductString(hid_handle))
    product_string_ = L"Unknown Gamepad";

  // Fetch information about the buttons and axes on this device. This sets
  // |buttons_length_| and |axes_length_| to their correct values and populates
  // |axes_| with capabilities info.
  if (!QueryDeviceCapabilities())
    return false;

  // Gamepads must have at least one button or axis.
  if (buttons_length_ == 0 && axes_length_ == 0)
    return false;

  return true;
}

bool RawInputGamepadDeviceWin::QueryHidInfo() {
  UINT size = 0;

  UINT result =
      ::GetRawInputDeviceInfo(handle_, RIDI_DEVICEINFO, nullptr, &size);
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputDeviceInfo() failed";
    return false;
  }
  DCHECK_EQ(0u, result);

  auto buffer = base::HeapArray<uint8_t>::Uninit(size);
  result =
      ::GetRawInputDeviceInfo(handle_, RIDI_DEVICEINFO, buffer.data(), &size);
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputDeviceInfo() failed";
    return false;
  }
  DCHECK_EQ(size, result);
  RID_DEVICE_INFO* device_info =
      reinterpret_cast<RID_DEVICE_INFO*>(buffer.data());

  DCHECK_EQ(device_info->dwType, static_cast<DWORD>(RIM_TYPEHID));
  vendor_id_ = static_cast<uint16_t>(device_info->hid.dwVendorId);
  product_id_ = static_cast<uint16_t>(device_info->hid.dwProductId);
  version_number_ = static_cast<uint16_t>(device_info->hid.dwVersionNumber);
  usage_ = device_info->hid.usUsage;

  return true;
}

bool RawInputGamepadDeviceWin::QueryDeviceName() {
  UINT size = 0;

  UINT result =
      ::GetRawInputDeviceInfo(handle_, RIDI_DEVICENAME, nullptr, &size);
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputDeviceInfo() failed";
    return false;
  }
  DCHECK_EQ(0u, result);

  std::wstring buffer;
  result = ::GetRawInputDeviceInfo(handle_, RIDI_DEVICENAME,
                                   base::WriteInto(&buffer, size), &size);
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputDeviceInfo() failed";
    return false;
  }
  DCHECK_EQ(size, result);

  name_ = std::move(buffer);

  return true;
}

bool RawInputGamepadDeviceWin::QueryProductString(
    base::win::ScopedHandle& hid_handle) {
  DCHECK(hid_handle.IsValid());
  // HidD_GetProductString may return successfully even if it didn't write to
  // the buffer. Ensure the buffer is zeroed before calling
  // HidD_GetProductString. See https://crbug.com/1205511.
  std::wstring buffer;
  if (!HidD_GetProductString(hid_handle.Get(),
                             base::WriteInto(&buffer, Gamepad::kIdLengthCap),
                             Gamepad::kIdLengthCap)) {
    return false;
  }

  // Remove trailing NUL characters.
  buffer = std::wstring(base::TrimString(buffer, std::wstring_view(L"\0", 1),
                                         base::TRIM_TRAILING));

  // The product string cannot be empty.
  if (buffer.empty())
    return false;

  product_string_ = std::move(buffer);
  return true;
}

base::win::ScopedHandle RawInputGamepadDeviceWin::OpenHidHandle() {
  return base::win::ScopedHandle(::CreateFile(
      name_.c_str(), GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE, /*lpSecurityAttributes=*/nullptr,
      OPEN_EXISTING, /*dwFlagsAndAttributes=*/0, /*hTemplateFile=*/nullptr));
}

bool RawInputGamepadDeviceWin::QueryDeviceCapabilities() {
  UINT size = 0;

  UINT result =
      ::GetRawInputDeviceInfo(handle_, RIDI_PREPARSEDDATA, nullptr, &size);
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputDeviceInfo() failed";
    return false;
  }
  DCHECK_EQ(0u, result);

  ppd_buffer_ = base::HeapArray<uint8_t>::Uninit(size);
  preparsed_data_ = reinterpret_cast<PHIDP_PREPARSED_DATA>(ppd_buffer_.data());
  result = ::GetRawInputDeviceInfo(handle_, RIDI_PREPARSEDDATA,
                                   ppd_buffer_.data(), &size);
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputDeviceInfo() failed";
    return false;
  }
  DCHECK_EQ(size, result);

  HIDP_CAPS caps;
  NTSTATUS status = HidP_GetCaps(preparsed_data_, &caps);
  DCHECK_EQ(HIDP_STATUS_SUCCESS, status);

  QueryButtonCapabilities(caps.NumberInputButtonCaps);
  QueryAxisCapabilities(caps.NumberInputValueCaps);

  return true;
}

void RawInputGamepadDeviceWin::QueryButtonCapabilities(uint16_t button_count) {
  if (button_count > 0) {
    std::vector<HIDP_BUTTON_CAPS> button_caps(button_count);
    NTSTATUS status = HidP_GetButtonCaps(HidP_Input, button_caps.data(),
                                         &button_count, preparsed_data_);
    DCHECK_EQ(HIDP_STATUS_SUCCESS, status);

    // Collect all inputs from the Button usage page.
    QueryNormalButtonCapabilities(button_caps);

    // Check for common gamepad buttons that are not on the Button usage page.
    QuerySpecialButtonCapabilities(button_caps);
  }
}

void RawInputGamepadDeviceWin::QueryNormalButtonCapabilities(
    base::span<const HIDP_BUTTON_CAPS> button_caps) {
  // Collect all inputs from the Button usage page and assign button indices
  // based on the usage value.
  for (const auto& item : button_caps) {
    uint16_t usage_min = item.Range.UsageMin;
    uint16_t usage_max = item.Range.UsageMax;
    if (usage_min == 0 || usage_max == 0)
      continue;
    size_t button_index_min = static_cast<size_t>(usage_min - 1);
    size_t button_index_max = static_cast<size_t>(usage_max - 1);
    if (item.UsagePage == kButtonUsagePage &&
        button_index_min < Gamepad::kButtonsLengthCap) {
      button_index_max =
          std::min(Gamepad::kButtonsLengthCap - 1, button_index_max);
      buttons_length_ = std::max(buttons_length_, button_index_max + 1);
      for (size_t button_index = button_index_min;
           button_index <= button_index_max; ++button_index) {
        button_report_id_[button_index] = item.ReportID;
      }
    }
  }
}

void RawInputGamepadDeviceWin::QuerySpecialButtonCapabilities(
    base::span<const HIDP_BUTTON_CAPS> button_caps) {
  // Check for common gamepad buttons that are not on the Button usage page.
  std::vector<bool> has_special_usage(kSpecialUsagesLen, false);
  std::vector<uint8_t> special_report_id(kSpecialUsagesLen, 0);
  size_t unmapped_button_count = 0;
  for (const auto& item : button_caps) {
    uint16_t usage_min = item.Range.UsageMin;
    uint16_t usage_max = item.Range.UsageMax;
    for (size_t special_index = 0; special_index < kSpecialUsagesLen;
         ++special_index) {
      const auto& special = kSpecialUsages[special_index];
      if (item.UsagePage == special.usage_page && usage_min <= special.usage &&
          usage_max >= special.usage) {
        has_special_usage[special_index] = true;
        special_report_id[special_index] = item.ReportID;
        ++unmapped_button_count;
      }
    }
  }

  special_button_map_.clear();
  if (unmapped_button_count > 0) {
    // Insert special buttons at unused button indices.
    special_button_map_.resize(kSpecialUsagesLen, -1);
    size_t button_index = 0;
    for (size_t special_index = 0; special_index < kSpecialUsagesLen;
         ++special_index) {
      if (!has_special_usage[special_index])
        continue;

      // Advance to the next unused button index.
      while (button_index < Gamepad::kButtonsLengthCap &&
             button_report_id_[button_index].has_value()) {
        ++button_index;
      }
      if (button_index >= Gamepad::kButtonsLengthCap)
        break;

      special_button_map_[special_index] = button_index;
      button_report_id_[button_index] = special_report_id[special_index];
      ++button_index;

      if (--unmapped_button_count == 0)
        break;
    }
    buttons_length_ = std::max(buttons_length_, button_index);
  }
}

void RawInputGamepadDeviceWin::QueryAxisCapabilities(uint16_t axis_count) {
  auto axes_caps = base::HeapArray<HIDP_VALUE_CAPS>::Uninit(axis_count);
  HidP_GetValueCaps(HidP_Input, axes_caps.data(), &axis_count, preparsed_data_);

  bool mapped_all_axes = true;

  for (size_t i = 0; i < axis_count; i++) {
    size_t axis_index = axes_caps[i].Range.UsageMin - kAxisMinimumUsageNumber;
    if (axis_index < Gamepad::kAxesLengthCap && !axes_[axis_index].active) {
      axes_[axis_index].caps = axes_caps[i];
      axes_[axis_index].value = 0;
      axes_[axis_index].active = true;
      axes_[axis_index].bitmask = GetBitmask(axes_caps[i].BitSize);
      axes_length_ = std::max(axes_length_, axis_index + 1);
      axes_used_ |= 1 << axis_index;
    } else {
      mapped_all_axes = false;
    }
  }

  if (!mapped_all_axes) {
    // For axes whose usage puts them outside the standard axesLengthCap range.
    size_t next_index = 0;
    for (size_t i = 0; i < axis_count; i++) {
      size_t usage = axes_caps[i].Range.UsageMin - kAxisMinimumUsageNumber;
      if (usage >= Gamepad::kAxesLengthCap &&
          axes_caps[i].UsagePage <= kGameControlsUsagePage) {
        for (; next_index < Gamepad::kAxesLengthCap; ++next_index) {
          if (!axes_[next_index].active)
            break;
        }
        if (next_index < Gamepad::kAxesLengthCap) {
          axes_[next_index].caps = axes_caps[i];
          axes_[next_index].value = 0;
          axes_[next_index].active = true;
          axes_[next_index].bitmask = GetBitmask(axes_caps[i].BitSize);
          axes_length_ = std::max(axes_length_, next_index + 1);
          axes_used_ |= 1 << next_index;
        }
      }

      if (next_index >= Gamepad::kAxesLengthCap)
        break;
    }
  }
}

void RawInputGamepadDeviceWin::UpdateAxisValue(size_t axis_index,
                                               RAWINPUT& input) {
  DCHECK_LT(axis_index, Gamepad::kAxesLengthCap);
  // RawInput gamepad axes are normalized according to the information provided
  // in the HID report descriptor. Each HID report item must specify a Logical
  // Minimum and Logical Maximum to define the domain of allowable values for
  // the item. An item may optionally specify a Units definition to indicate
  // that the item represents a real-world value measured in those units. Items
  // with a Units definition should also specify Physical Minimum and Physical
  // Maximum, which are the Logical bounds transformed into Physical units.
  //
  // For gamepads, it is common for joystick and trigger axis items to not
  // specify Units. However, D-pad items typically do specify Units. An 8-way
  // directional pad, when implemented as a Hat Switch axis, reports a logical
  // value from 0 to 7 that corresponds to a physical value from 0 degrees (N)
  // clockwise to 315 degrees (NW).
  //
  // When a Hat Switch is in its Null State (no interaction) it reports a value
  // outside the Logical range, often 8. Normalizing the out-of-bounds Null
  // State value yields an invalid axis value greater than +1.0. This invalid
  // axis value must be preserved so that downstream consumers can detect when
  // the Hat Switch is reporting a Null State value.
  //
  // When an item provides Physical bounds, prefer to use
  // HidP_GetScaledUsageValue to retrieve the item's value in real-world units
  // and normalize using the Physical bounds. If the Physical bounds are invalid
  // or HidP_GetScaledUsageValue fails, use HidP_GetUsageValue to retrieve the
  // logical value and normalize using the Logical bounds.
  auto& axis = axes_[axis_index];
  if (axis.caps.PhysicalMin < axis.caps.PhysicalMax) {
    LONG scaled_axis_value = 0;
    if (HidP_GetScaledUsageValue(
            HidP_Input, axis.caps.UsagePage, /*LinkCollection=*/0,
            axis.caps.Range.UsageMin, &scaled_axis_value, preparsed_data_,
            reinterpret_cast<PCHAR>(input.data.hid.bRawData),
            input.data.hid.dwSizeHid) == HIDP_STATUS_SUCCESS) {
      axis.value = NormalizeAxis(scaled_axis_value, axis.caps.PhysicalMin,
                                 axis.caps.PhysicalMax);
      return;
    }
  }

  ULONG axis_value = 0;
  if (HidP_GetUsageValue(HidP_Input, axis.caps.UsagePage, /*LinkCollection=*/0,
                         axis.caps.Range.UsageMin, &axis_value, preparsed_data_,
                         reinterpret_cast<PCHAR>(input.data.hid.bRawData),
                         input.data.hid.dwSizeHid) == HIDP_STATUS_SUCCESS) {
    ULONG logical_min = axis.caps.LogicalMin & axis.bitmask;
    ULONG logical_max = axis.caps.LogicalMax & axis.bitmask;
    if (logical_min < logical_max) {
      // If the unsigned logical min is less than the unsigned logical max then
      // assume `axis_value` is always non-negative.
      axis.value =
          NormalizeAxis(axis_value & axis.bitmask, logical_min, logical_max);
    } else {
      // Sign-extend before normalizing.
      axis.value = NormalizeAxis(
          SignExtend(axis_value & axis.bitmask, axis.caps.BitSize),
          SignExtend(logical_min, axis.caps.BitSize),
          SignExtend(logical_max, axis.caps.BitSize));
    }
    return;
  }
}

base::WeakPtr<AbstractHapticGamepad> RawInputGamepadDeviceWin::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
