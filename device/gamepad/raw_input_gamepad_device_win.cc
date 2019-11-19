// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "raw_input_gamepad_device_win.h"

#include "base/stl_util.h"
#include "device/gamepad/dualshock4_controller.h"
#include "device/gamepad/gamepad_blocklist.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/hid_haptic_gamepad.h"
#include "device/gamepad/hid_writer_win.h"

namespace device {

namespace {

float NormalizeAxis(long value, long min, long max) {
  return (2.f * (value - min) / static_cast<float>(max - min)) - 1.f;
}

unsigned long GetBitmask(unsigned short bits) {
  return (1 << bits) - 1;
}

const uint32_t kGenericDesktopUsagePage = 0x01;
const uint32_t kGameControlsUsagePage = 0x05;
const uint32_t kButtonUsagePage = 0x09;
const uint32_t kConsumerUsagePage = 0x0c;

const uint32_t kAxisMinimumUsageNumber = 0x30;
const uint32_t kSystemMainMenuUsageNumber = 0x85;
const uint32_t kPowerUsageNumber = 0x30;
const uint32_t kSearchUsageNumber = 0x0221;
const uint32_t kHomeUsageNumber = 0x0223;
const uint32_t kBackUsageNumber = 0x0224;

// The fetcher will collect all HID usages from the Button usage page and any
// additional usages listed below.
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

}  // namespace

RawInputGamepadDeviceWin::RawInputGamepadDeviceWin(
    HANDLE device_handle,
    int source_id,
    HidDllFunctionsWin* hid_functions)
    : handle_(device_handle),
      source_id_(source_id),
      last_update_timestamp_(GamepadDataFetcher::CurrentTimeInMicroseconds()),
      hid_functions_(hid_functions) {
  ::ZeroMemory(buttons_, sizeof(buttons_));
  ::ZeroMemory(axes_, sizeof(axes_));

  if (hid_functions_->IsValid())
    is_valid_ = QueryDeviceInfo();

  if (is_valid_) {
    if (Dualshock4Controller::IsDualshock4(vendor_id_, product_id_)) {
      // Dualshock4 has different behavior over USB and Bluetooth, but the
      // RawInput API does not indicate which transport is in use. Detect the
      // transport type by inspecting the version number reported by the device.
      GamepadBusType bus_type =
          Dualshock4Controller::BusTypeFromVersionNumber(version_number_);
      dualshock4_ = std::make_unique<Dualshock4Controller>(
          vendor_id_, product_id_, bus_type,
          std::make_unique<HidWriterWin>(handle_));
    } else if (HidHapticGamepad::IsHidHaptic(vendor_id_, product_id_)) {
      hid_haptics_ = HidHapticGamepad::Create(
          vendor_id_, product_id_, std::make_unique<HidWriterWin>(handle_));
    }
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
  DCHECK(hid_functions_->IsValid());
  NTSTATUS status;

  if (dualshock4_) {
    // Handle Dualshock4 input reports that do not specify HID gamepad usages in
    // the report descriptor.
    uint8_t report_id = input->data.hid.bRawData[0];
    auto report = base::make_span(input->data.hid.bRawData + 1,
                                  input->data.hid.dwSizeHid);
    Gamepad pad;
    if (dualshock4_->ProcessInputReport(report_id, report, &pad)) {
      for (size_t i = 0; i < Gamepad::kAxesLengthCap; ++i)
        axes_[i].value = pad.axes[i];
      for (size_t i = 0; i < Gamepad::kButtonsLengthCap; ++i)
        buttons_[i] = pad.buttons[i].pressed;
      last_update_timestamp_ = GamepadDataFetcher::CurrentTimeInMicroseconds();
      return;
    }
  }

  // Query button state.
  if (buttons_length_ > 0) {
    // Clear the button state
    ::ZeroMemory(buttons_, sizeof(buttons_));
    ULONG buttons_length = 0;

    hid_functions_->HidPGetUsagesEx()(
        HidP_Input, 0, nullptr, &buttons_length, preparsed_data_,
        reinterpret_cast<PCHAR>(input->data.hid.bRawData),
        input->data.hid.dwSizeHid);

    std::unique_ptr<USAGE_AND_PAGE[]> usages(
        new USAGE_AND_PAGE[buttons_length]);
    status = hid_functions_->HidPGetUsagesEx()(
        HidP_Input, 0, usages.get(), &buttons_length, preparsed_data_,
        reinterpret_cast<PCHAR>(input->data.hid.bRawData),
        input->data.hid.dwSizeHid);

    if (status == HIDP_STATUS_SUCCESS) {
      // Set each reported button to true.
      for (size_t j = 0; j < buttons_length; j++) {
        uint16_t usage_page = usages[j].UsagePage;
        uint16_t usage = usages[j].Usage;
        if (usage_page == kButtonUsagePage && usage > 0) {
          size_t button_index = size_t{usage - 1};
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

  // Query axis state.
  ULONG axis_value = 0;
  LONG scaled_axis_value = 0;
  for (uint32_t i = 0; i < axes_length_; i++) {
    RawGamepadAxis* axis = &axes_[i];

    // If the min is < 0 we have to query the scaled value, otherwise we need
    // the normal unscaled value.
    if (axis->caps.LogicalMin < 0) {
      status = hid_functions_->HidPGetScaledUsageValue()(
          HidP_Input, axis->caps.UsagePage, 0, axis->caps.Range.UsageMin,
          &scaled_axis_value, preparsed_data_,
          reinterpret_cast<PCHAR>(input->data.hid.bRawData),
          input->data.hid.dwSizeHid);
      if (status == HIDP_STATUS_SUCCESS) {
        axis->value = NormalizeAxis(scaled_axis_value, axis->caps.PhysicalMin,
                                    axis->caps.PhysicalMax);
      }
    } else {
      status = hid_functions_->HidPGetUsageValue()(
          HidP_Input, axis->caps.UsagePage, 0, axis->caps.Range.UsageMin,
          &axis_value, preparsed_data_,
          reinterpret_cast<PCHAR>(input->data.hid.bRawData),
          input->data.hid.dwSizeHid);
      if (status == HIDP_STATUS_SUCCESS) {
        axis->value = NormalizeAxis(axis_value & axis->bitmask,
                                    axis->caps.LogicalMin & axis->bitmask,
                                    axis->caps.LogicalMax & axis->bitmask);
      }
    }
  }

  last_update_timestamp_ = GamepadDataFetcher::CurrentTimeInMicroseconds();
}

void RawInputGamepadDeviceWin::ReadPadState(Gamepad* pad) const {
  DCHECK(pad);

  pad->timestamp = last_update_timestamp_;
  pad->buttons_length = buttons_length_;
  pad->axes_length = axes_length_;

  for (unsigned int i = 0; i < buttons_length_; i++) {
    pad->buttons[i].pressed = buttons_[i];
    pad->buttons[i].value = buttons_[i] ? 1.0 : 0.0;
  }

  for (unsigned int i = 0; i < axes_length_; i++)
    pad->axes[i] = axes_[i].value;
}

bool RawInputGamepadDeviceWin::SupportsVibration() const {
  return dualshock4_ || hid_haptics_;
}

void RawInputGamepadDeviceWin::SetVibration(double strong_magnitude,
                                            double weak_magnitude) {
  if (dualshock4_)
    dualshock4_->SetVibration(strong_magnitude, weak_magnitude);
  else if (hid_haptics_)
    hid_haptics_->SetVibration(strong_magnitude, weak_magnitude);
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
  // TODO(crbug/881539): Potentially allow PCI HID devices to be enumerated, but
  // prefer known gamepads when there is contention.
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

  std::unique_ptr<uint8_t[]> buffer(new uint8_t[size]);
  result =
      ::GetRawInputDeviceInfo(handle_, RIDI_DEVICEINFO, buffer.get(), &size);
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputDeviceInfo() failed";
    return false;
  }
  DCHECK_EQ(size, result);
  RID_DEVICE_INFO* device_info =
      reinterpret_cast<RID_DEVICE_INFO*>(buffer.get());

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

  std::unique_ptr<wchar_t[]> buffer(new wchar_t[size]);
  result =
      ::GetRawInputDeviceInfo(handle_, RIDI_DEVICENAME, buffer.get(), &size);
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputDeviceInfo() failed";
    return false;
  }
  DCHECK_EQ(size, result);

  name_ = buffer.get();

  return true;
}

bool RawInputGamepadDeviceWin::QueryProductString(
    base::win::ScopedHandle& hid_handle) {
  DCHECK(hid_handle.IsValid());
  product_string_.resize(Gamepad::kIdLengthCap);
  return hid_functions_->HidDGetProductString()(
      hid_handle.Get(), &product_string_.front(), Gamepad::kIdLengthCap);
}

base::win::ScopedHandle RawInputGamepadDeviceWin::OpenHidHandle() {
  return base::win::ScopedHandle(::CreateFile(
      name_.c_str(), GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE, /*lpSecurityAttributes=*/nullptr,
      OPEN_EXISTING, /*dwFlagsAndAttributes=*/0, /*hTemplateFile=*/nullptr));
}

bool RawInputGamepadDeviceWin::QueryDeviceCapabilities() {
  DCHECK(hid_functions_);
  DCHECK(hid_functions_->IsValid());
  UINT size = 0;

  UINT result =
      ::GetRawInputDeviceInfo(handle_, RIDI_PREPARSEDDATA, nullptr, &size);
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputDeviceInfo() failed";
    return false;
  }
  DCHECK_EQ(0u, result);

  ppd_buffer_.reset(new uint8_t[size]);
  preparsed_data_ = reinterpret_cast<PHIDP_PREPARSED_DATA>(ppd_buffer_.get());
  result = ::GetRawInputDeviceInfo(handle_, RIDI_PREPARSEDDATA,
                                   ppd_buffer_.get(), &size);
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputDeviceInfo() failed";
    return false;
  }
  DCHECK_EQ(size, result);

  HIDP_CAPS caps;
  NTSTATUS status = hid_functions_->HidPGetCaps()(preparsed_data_, &caps);
  DCHECK_EQ(HIDP_STATUS_SUCCESS, status);

  QueryButtonCapabilities(caps.NumberInputButtonCaps);
  QueryAxisCapabilities(caps.NumberInputValueCaps);

  return true;
}

void RawInputGamepadDeviceWin::QueryButtonCapabilities(uint16_t button_count) {
  DCHECK(hid_functions_);
  DCHECK(hid_functions_->IsValid());
  if (button_count > 0) {
    std::unique_ptr<HIDP_BUTTON_CAPS[]> button_caps(
        new HIDP_BUTTON_CAPS[button_count]);
    NTSTATUS status = hid_functions_->HidPGetButtonCaps()(
        HidP_Input, button_caps.get(), &button_count, preparsed_data_);
    DCHECK_EQ(HIDP_STATUS_SUCCESS, status);

    // Keep track of which button indices are in use.
    std::vector<bool> button_indices_used(Gamepad::kButtonsLengthCap, false);

    // Collect all inputs from the Button usage page.
    QueryNormalButtonCapabilities(button_caps.get(), button_count,
                                  &button_indices_used);

    // Check for common gamepad buttons that are not on the Button usage page.
    QuerySpecialButtonCapabilities(button_caps.get(), button_count,
                                   &button_indices_used);
  }
}

void RawInputGamepadDeviceWin::QueryNormalButtonCapabilities(
    HIDP_BUTTON_CAPS button_caps[],
    uint16_t button_count,
    std::vector<bool>* button_indices_used) {
  DCHECK(button_caps);
  DCHECK(button_indices_used);

  // Collect all inputs from the Button usage page and assign button indices
  // based on the usage value.
  for (size_t i = 0; i < button_count; ++i) {
    uint16_t usage_page = button_caps[i].UsagePage;
    uint16_t usage_min = button_caps[i].Range.UsageMin;
    uint16_t usage_max = button_caps[i].Range.UsageMax;
    if (usage_min == 0 || usage_max == 0)
      continue;
    size_t button_index_min = size_t{usage_min - 1};
    size_t button_index_max = size_t{usage_max - 1};
    if (usage_page == kButtonUsagePage &&
        button_index_min < Gamepad::kButtonsLengthCap) {
      button_index_max =
          std::min(Gamepad::kButtonsLengthCap - 1, button_index_max);
      buttons_length_ = std::max(buttons_length_, button_index_max + 1);
      for (size_t j = button_index_min; j <= button_index_max; ++j)
        (*button_indices_used)[j] = true;
    }
  }
}

void RawInputGamepadDeviceWin::QuerySpecialButtonCapabilities(
    HIDP_BUTTON_CAPS button_caps[],
    uint16_t button_count,
    std::vector<bool>* button_indices_used) {
  DCHECK(button_caps);
  DCHECK(button_indices_used);

  // Check for common gamepad buttons that are not on the Button usage page.
  std::vector<bool> has_special_usage(kSpecialUsagesLen, false);
  size_t unmapped_button_count = 0;
  for (size_t i = 0; i < button_count; ++i) {
    uint16_t usage_page = button_caps[i].UsagePage;
    uint16_t usage_min = button_caps[i].Range.UsageMin;
    uint16_t usage_max = button_caps[i].Range.UsageMax;
    for (size_t special_index = 0; special_index < kSpecialUsagesLen;
         ++special_index) {
      const auto& special = kSpecialUsages[special_index];
      if (usage_page == special.usage_page && usage_min <= special.usage &&
          usage_max >= special.usage) {
        has_special_usage[special_index] = true;
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
             (*button_indices_used)[button_index]) {
        ++button_index;
      }
      if (button_index >= Gamepad::kButtonsLengthCap)
        break;

      special_button_map_[special_index] = button_index;
      (*button_indices_used)[button_index] = true;
      ++button_index;

      if (--unmapped_button_count == 0)
        break;
    }
  }
}

void RawInputGamepadDeviceWin::QueryAxisCapabilities(uint16_t axis_count) {
  DCHECK(hid_functions_);
  DCHECK(hid_functions_->IsValid());
  std::unique_ptr<HIDP_VALUE_CAPS[]> axes_caps(new HIDP_VALUE_CAPS[axis_count]);
  hid_functions_->HidPGetValueCaps()(HidP_Input, axes_caps.get(), &axis_count,
                                     preparsed_data_);

  bool mapped_all_axes = true;

  for (size_t i = 0; i < axis_count; i++) {
    size_t axis_index = axes_caps[i].Range.UsageMin - kAxisMinimumUsageNumber;
    if (axis_index < Gamepad::kAxesLengthCap && !axes_[axis_index].active) {
      axes_[axis_index].caps = axes_caps[i];
      axes_[axis_index].value = 0;
      axes_[axis_index].active = true;
      axes_[axis_index].bitmask = GetBitmask(axes_caps[i].BitSize);
      axes_length_ = std::max(axes_length_, axis_index + 1);
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
        }
      }

      if (next_index >= Gamepad::kAxesLengthCap)
        break;
    }
  }
}

base::WeakPtr<AbstractHapticGamepad> RawInputGamepadDeviceWin::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
