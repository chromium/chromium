// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_device_linux.h"

#include <fcntl.h>
#include <limits.h>
#include <linux/hidraw.h>
#include <linux/input.h>
#include <linux/joystick.h>
#include <sys/ioctl.h>

#include "base/posix/eintr_wrapper.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/udev_linux/udev_linux.h"

namespace device {

namespace {

const char kInputSubsystem[] = "input";
const char kUsbSubsystem[] = "usb";
const char kUsbDeviceType[] = "usb_device";
const float kMaxLinuxAxisValue = 32767.0;
const int kInvalidEffectId = -1;
const uint16_t kRumbleMagnitudeMax = 0xffff;

const size_t kSpecialKeys[] = {
    // Xbox One S pre-FW update reports Xbox button as SystemMainMenu over BT.
    KEY_MENU,
    // Power is used for the Guide button on the Nvidia Shield 2015 gamepad.
    KEY_POWER,
    // Search is used for the Guide button on the Nvidia Shield 2015 gamepad.
    KEY_SEARCH,
    // Start, Back, and Guide buttons are often reported as Consumer Home or
    // Back.
    KEY_HOMEPAGE, KEY_BACK,
};
const size_t kSpecialKeysLen = base::size(kSpecialKeys);

#define LONG_BITS (CHAR_BIT * sizeof(long))
#define BITS_TO_LONGS(x) (((x) + LONG_BITS - 1) / LONG_BITS)

static inline bool test_bit(int bit, const unsigned long* data) {
  return data[bit / LONG_BITS] & (1UL << (bit % LONG_BITS));
}

GamepadBusType GetEvdevBusType(int fd) {
  struct input_id input_info;
  if (HANDLE_EINTR(ioctl(fd, EVIOCGID, &input_info)) >= 0) {
    if (input_info.bustype == BUS_USB)
      return GAMEPAD_BUS_USB;
    if (input_info.bustype == BUS_BLUETOOTH)
      return GAMEPAD_BUS_BLUETOOTH;
  }
  return GAMEPAD_BUS_UNKNOWN;
}

bool HasRumbleCapability(int fd) {
  unsigned long evbit[BITS_TO_LONGS(EV_MAX)];
  unsigned long ffbit[BITS_TO_LONGS(FF_MAX)];

  if (HANDLE_EINTR(ioctl(fd, EVIOCGBIT(0, EV_MAX), evbit)) < 0 ||
      HANDLE_EINTR(ioctl(fd, EVIOCGBIT(EV_FF, FF_MAX), ffbit)) < 0) {
    return false;
  }

  if (!test_bit(EV_FF, evbit)) {
    return false;
  }

  return test_bit(FF_RUMBLE, ffbit);
}

// Check an evdev device for key codes which sometimes appear on gamepads but
// aren't reported by joydev. If a special key is found, the corresponding entry
// of the |has_special_key| vector is set to true. Returns the number of
// special keys found.
size_t CheckSpecialKeys(int fd, std::vector<bool>* has_special_key) {
  DCHECK(has_special_key);
  unsigned long evbit[BITS_TO_LONGS(EV_MAX)];
  unsigned long keybit[BITS_TO_LONGS(KEY_MAX)];
  size_t found_special_keys = 0;

  has_special_key->clear();
  if (HANDLE_EINTR(ioctl(fd, EVIOCGBIT(0, EV_MAX), evbit)) < 0 ||
      HANDLE_EINTR(ioctl(fd, EVIOCGBIT(EV_KEY, KEY_MAX), keybit)) < 0) {
    return 0;
  }

  if (!test_bit(EV_KEY, evbit)) {
    return 0;
  }

  has_special_key->resize(kSpecialKeysLen, false);
  for (size_t special_index = 0; special_index < kSpecialKeysLen;
       ++special_index) {
    (*has_special_key)[special_index] =
        test_bit(kSpecialKeys[special_index], keybit);
    ++found_special_keys;
  }

  return found_special_keys;
}

bool GetHidrawDevinfo(int fd,
                      GamepadBusType* bus_type,
                      uint16_t* vendor_id,
                      uint16_t* product_id) {
  struct hidraw_devinfo info;
  if (HANDLE_EINTR(ioctl(fd, HIDIOCGRAWINFO, &info)) < 0)
    return false;
  if (bus_type) {
    if (info.bustype == BUS_USB)
      *bus_type = GAMEPAD_BUS_USB;
    else if (info.bustype == BUS_BLUETOOTH)
      *bus_type = GAMEPAD_BUS_BLUETOOTH;
    else
      *bus_type = GAMEPAD_BUS_UNKNOWN;
  }
  if (vendor_id)
    *vendor_id = static_cast<uint16_t>(info.vendor);
  if (product_id)
    *product_id = static_cast<uint16_t>(info.product);
  return true;
}

int StoreRumbleEffect(int fd,
                      int effect_id,
                      uint16_t duration,
                      uint16_t start_delay,
                      uint16_t strong_magnitude,
                      uint16_t weak_magnitude) {
  struct ff_effect effect;
  memset(&effect, 0, sizeof(effect));
  effect.type = FF_RUMBLE;
  effect.id = effect_id;
  effect.replay.length = duration;
  effect.replay.delay = start_delay;
  effect.u.rumble.strong_magnitude = strong_magnitude;
  effect.u.rumble.weak_magnitude = weak_magnitude;

  if (HANDLE_EINTR(ioctl(fd, EVIOCSFF, (const void*)&effect)) < 0)
    return kInvalidEffectId;
  return effect.id;
}

void DestroyEffect(int fd, int effect_id) {
  HANDLE_EINTR(ioctl(fd, EVIOCRMFF, effect_id));
}

bool StartOrStopEffect(int fd, int effect_id, bool do_start) {
  struct input_event start_stop;
  memset(&start_stop, 0, sizeof(start_stop));
  start_stop.type = EV_FF;
  start_stop.code = effect_id;
  start_stop.value = do_start ? 1 : 0;
  ssize_t nbytes =
      HANDLE_EINTR(write(fd, (const void*)&start_stop, sizeof(start_stop)));
  return nbytes == sizeof(start_stop);
}

uint16_t HexStringToUInt16WithDefault(base::StringPiece input,
                                      uint16_t default_value) {
  uint32_t out = 0;
  if (!base::HexStringToUInt(input, &out) ||
      out > std::numeric_limits<uint16_t>::max()) {
    return default_value;
  }
  return static_cast<uint16_t>(out);
}

}  // namespace

GamepadDeviceLinux::GamepadDeviceLinux(const std::string& syspath_prefix)
    : syspath_prefix_(syspath_prefix),
      button_indices_used_(Gamepad::kButtonsLengthCap, false) {}

GamepadDeviceLinux::~GamepadDeviceLinux() = default;

void GamepadDeviceLinux::DoShutdown() {
  CloseJoydevNode();
  CloseEvdevNode();
  CloseHidrawNode();
}

bool GamepadDeviceLinux::IsEmpty() const {
  return joydev_fd_ < 0 && evdev_fd_ < 0 && hidraw_fd_ < 0;
}

bool GamepadDeviceLinux::SupportsVibration() const {
  if (dualshock4_)
    return true;

  // Vibration is only supported over USB.
  // TODO(mattreynolds): add support for Switch Pro vibration over Bluetooth.
  if (switch_pro_)
    return bus_type_ == GAMEPAD_BUS_USB;

  return supports_force_feedback_ && evdev_fd_ >= 0;
}

void GamepadDeviceLinux::ReadPadState(Gamepad* pad) {
  if (switch_pro_ && bus_type_ == GAMEPAD_BUS_USB) {
    // When connected over USB, the Switch Pro controller does not correctly
    // report its state over USB HID. Instead, fetch the state using the
    // device's vendor-specific USB protocol.
    switch_pro_->ReadUsbPadState(pad);
    return;
  }

  DCHECK_GE(joydev_fd_, 0);

  // Read button and axis events from the joydev device.
  bool pad_updated = ReadJoydevState(pad);

  // Evdev special buttons must be initialized after we have read from joydev
  // at least once to ensure we do not assign a button index already in use by
  // joydev.
  if (!evdev_special_keys_initialized_)
    InitializeEvdevSpecialKeys();

  // Read button events from the evdev device.
  if (!special_button_map_.empty()) {
    if (ReadEvdevSpecialKeys(pad))
      pad_updated = true;
  }

  if (pad_updated)
    pad->timestamp = GamepadDataFetcher::CurrentTimeInMicroseconds();
}

bool GamepadDeviceLinux::ReadJoydevState(Gamepad* pad) {
  DCHECK(pad);

  if (joydev_fd_ < 0)
    return false;

  // Read button and axis events from the joydev device.
  bool pad_updated = false;
  js_event event;
  while (HANDLE_EINTR(read(joydev_fd_, &event, sizeof(struct js_event))) > 0) {
    size_t item = event.number;
    if (event.type & JS_EVENT_AXIS) {
      if (item >= Gamepad::kAxesLengthCap)
        continue;

      pad->axes[item] = event.value / kMaxLinuxAxisValue;

      if (item >= pad->axes_length)
        pad->axes_length = item + 1;
      pad_updated = true;
    } else if (event.type & JS_EVENT_BUTTON) {
      if (item >= Gamepad::kButtonsLengthCap)
        continue;

      pad->buttons[item].pressed = event.value;
      pad->buttons[item].value = event.value ? 1.0 : 0.0;

      // When a joydev device is opened, synthetic events are generated for
      // each joystick button and axis with the JS_EVENT_INIT flag set on the
      // event type. Use this signal to mark these button indices as used.
      if (event.type & JS_EVENT_INIT)
        button_indices_used_[item] = true;

      if (item >= pad->buttons_length)
        pad->buttons_length = item + 1;
      pad_updated = true;
    }
  }
  return pad_updated;
}

void GamepadDeviceLinux::InitializeEvdevSpecialKeys() {
  if (evdev_fd_ < 0)
    return;

  // Do some one-time initialization to decide indices for the evdev special
  // buttons.
  evdev_special_keys_initialized_ = true;
  std::vector<bool> special_key_present;
  size_t unmapped_button_count =
      CheckSpecialKeys(evdev_fd_, &special_key_present);

  special_button_map_.clear();
  if (unmapped_button_count > 0) {
    // Insert special buttons at unused button indices.
    special_button_map_.resize(kSpecialKeysLen, -1);
    size_t button_index = 0;
    for (size_t special_index = 0; special_index < kSpecialKeysLen;
         ++special_index) {
      if (!special_key_present[special_index])
        continue;

      // Advance to the next unused button index.
      while (button_indices_used_[button_index] &&
             button_index < Gamepad::kButtonsLengthCap) {
        ++button_index;
      }
      if (button_index >= Gamepad::kButtonsLengthCap)
        break;

      special_button_map_[special_index] = button_index;
      button_indices_used_[button_index] = true;
      ++button_index;

      if (--unmapped_button_count == 0)
        break;
    }
  }
}

bool GamepadDeviceLinux::ReadEvdevSpecialKeys(Gamepad* pad) {
  DCHECK(pad);

  if (evdev_fd_ < 0)
    return false;

  // Read special button events through evdev.
  bool pad_updated = false;
  input_event ev;
  ssize_t bytes_read;
  while ((bytes_read =
              HANDLE_EINTR(read(evdev_fd_, &ev, sizeof(input_event)))) > 0) {
    if (size_t{bytes_read} < sizeof(input_event))
      break;
    if (ev.type != EV_KEY)
      continue;

    for (size_t special_index = 0; special_index < kSpecialKeysLen;
         ++special_index) {
      int button_index = special_button_map_[special_index];
      if (button_index < 0)
        continue;
      if (ev.code == kSpecialKeys[special_index]) {
        pad->buttons[button_index].pressed = ev.value;
        pad->buttons[button_index].value = ev.value ? 1.0 : 0.0;
        pad_updated = true;
      }
    }
  }

  return pad_updated;
}

GamepadStandardMappingFunction GamepadDeviceLinux::GetMappingFunction() const {
  return GetGamepadStandardMappingFunction(vendor_id_, product_id_,
                                           version_number_, bus_type_);
}

bool GamepadDeviceLinux::IsSameDevice(const UdevGamepadLinux& pad_info) {
  return pad_info.syspath_prefix == syspath_prefix_;
}

bool GamepadDeviceLinux::OpenJoydevNode(const UdevGamepadLinux& pad_info,
                                        udev_device* device) {
  DCHECK(pad_info.type == UdevGamepadLinux::Type::JOYDEV);
  DCHECK(pad_info.syspath_prefix == syspath_prefix_);

  CloseJoydevNode();
  joydev_fd_ = open(pad_info.path.c_str(), O_RDONLY | O_NONBLOCK);
  if (joydev_fd_ < 0)
    return false;

  udev_device* parent_device =
      device::udev_device_get_parent_with_subsystem_devtype(
          device, kInputSubsystem, nullptr);

  const char* vendor_id =
      udev_device_get_sysattr_value(parent_device, "id/vendor");
  const char* product_id =
      udev_device_get_sysattr_value(parent_device, "id/product");
  const char* version_number =
      udev_device_get_sysattr_value(parent_device, "id/version");
  const char* name = udev_device_get_sysattr_value(parent_device, "name");
  std::string name_string(name ? name : "");

  uint16_t vendor_id_int = HexStringToUInt16WithDefault(vendor_id, 0);
  uint16_t product_id_int = HexStringToUInt16WithDefault(product_id, 0);
  uint16_t version_number_int = HexStringToUInt16WithDefault(version_number, 0);

  // In many cases the information the input subsystem contains isn't
  // as good as the information that the device bus has, walk up further
  // to the subsystem/device type "usb"/"usb_device" and if this device
  // has the same vendor/product id, prefer the description from that.
  struct udev_device* usb_device =
      udev_device_get_parent_with_subsystem_devtype(
          parent_device, kUsbSubsystem, kUsbDeviceType);
  if (usb_device) {
    const char* usb_vendor_id =
        udev_device_get_sysattr_value(usb_device, "idVendor");
    const char* usb_product_id =
        udev_device_get_sysattr_value(usb_device, "idProduct");

    if (vendor_id && product_id && strcmp(vendor_id, usb_vendor_id) == 0 &&
        strcmp(product_id, usb_product_id) == 0) {
      const char* manufacturer =
          udev_device_get_sysattr_value(usb_device, "manufacturer");
      const char* product =
          udev_device_get_sysattr_value(usb_device, "product");

      // Replace the previous name string with one containing the better
      // information.
      name_string = base::StringPrintf("%s %s", manufacturer, product);
    }
  }

  joydev_index_ = pad_info.index;
  vendor_id_ = vendor_id_int;
  product_id_ = product_id_int;
  version_number_ = version_number_int;
  name_ = name_string;

  return true;
}

void GamepadDeviceLinux::CloseJoydevNode() {
  if (joydev_fd_ >= 0) {
    close(joydev_fd_);
    joydev_fd_ = -1;
  }
  joydev_index_ = -1;
  vendor_id_ = 0;
  product_id_ = 0;
  version_number_ = 0;
  name_.clear();

  // Button indices must be recomputed once the joydev node is closed.
  button_indices_used_.clear();
  special_button_map_.clear();
  evdev_special_keys_initialized_ = false;
}

bool GamepadDeviceLinux::OpenEvdevNode(const UdevGamepadLinux& pad_info) {
  DCHECK(pad_info.type == UdevGamepadLinux::Type::EVDEV);
  DCHECK(pad_info.syspath_prefix == syspath_prefix_);

  CloseEvdevNode();
  evdev_fd_ = open(pad_info.path.c_str(), O_RDWR | O_NONBLOCK);
  if (evdev_fd_ < 0)
    return false;

  supports_force_feedback_ = HasRumbleCapability(evdev_fd_);
  bus_type_ = GetEvdevBusType(evdev_fd_);

  return true;
}

void GamepadDeviceLinux::CloseEvdevNode() {
  if (evdev_fd_ >= 0) {
    if (effect_id_ != kInvalidEffectId) {
      DestroyEffect(evdev_fd_, effect_id_);
      effect_id_ = kInvalidEffectId;
    }
    close(evdev_fd_);
    evdev_fd_ = -1;
  }
  supports_force_feedback_ = false;

  // Clear any entries in |button_indices_used_| that were taken by evdev.
  if (!special_button_map_.empty()) {
    for (int button_index : special_button_map_) {
      if (button_index >= 0)
        button_indices_used_[button_index] = false;
    }
  }
  special_button_map_.clear();
  evdev_special_keys_initialized_ = false;
}

bool GamepadDeviceLinux::OpenHidrawNode(const UdevGamepadLinux& pad_info) {
  DCHECK(pad_info.type == UdevGamepadLinux::Type::HIDRAW);
  DCHECK(pad_info.syspath_prefix == syspath_prefix_);

  CloseHidrawNode();
  hidraw_fd_ = open(pad_info.path.c_str(), O_RDWR | O_NONBLOCK);
  if (hidraw_fd_ < 0)
    return false;

  uint16_t vendor_id;
  uint16_t product_id;
  bool is_dualshock4 = false;
  bool is_switch_pro = false;
  if (GetHidrawDevinfo(hidraw_fd_, &bus_type_, &vendor_id, &product_id)) {
    is_dualshock4 =
        Dualshock4ControllerLinux::IsDualshock4(vendor_id, product_id);
    is_switch_pro =
        SwitchProControllerLinux::IsSwitchPro(vendor_id, product_id);
    DCHECK(!is_dualshock4 || !is_switch_pro);
  }

  if (is_dualshock4 && !dualshock4_)
    dualshock4_ = std::make_unique<Dualshock4ControllerLinux>(hidraw_fd_);

  if (is_switch_pro && !switch_pro_) {
    switch_pro_ = std::make_unique<SwitchProControllerLinux>(hidraw_fd_);

    if (bus_type_ == GAMEPAD_BUS_USB)
      switch_pro_->SendConnectionStatusQuery();
  }

  return true;
}

void GamepadDeviceLinux::CloseHidrawNode() {
  if (dualshock4_)
    dualshock4_->Shutdown();
  dualshock4_.reset();
  if (switch_pro_)
    switch_pro_->Shutdown();
  switch_pro_.reset();
  if (hidraw_fd_ >= 0) {
    close(hidraw_fd_);
    hidraw_fd_ = -1;
  }
}

void GamepadDeviceLinux::SetVibration(double strong_magnitude,
                                      double weak_magnitude) {
  if (dualshock4_) {
    dualshock4_->SetVibration(strong_magnitude, weak_magnitude);
    return;
  }

  if (switch_pro_) {
    switch_pro_->SetVibration(strong_magnitude, weak_magnitude);
    return;
  }

  uint16_t strong_magnitude_scaled =
      static_cast<uint16_t>(strong_magnitude * kRumbleMagnitudeMax);
  uint16_t weak_magnitude_scaled =
      static_cast<uint16_t>(weak_magnitude * kRumbleMagnitudeMax);

  // AbstractHapticGamepad will call SetZeroVibration when the effect is
  // complete, so we don't need to set the duration here except to make sure it
  // is at least as long as the maximum duration.
  uint16_t duration_millis =
      static_cast<uint16_t>(GamepadHapticActuator::kMaxEffectDurationMillis);

  // Upload the effect and get the new effect ID. If we already created an
  // effect on this device, reuse its ID.
  effect_id_ =
      StoreRumbleEffect(evdev_fd_, effect_id_, duration_millis, 0,
                        strong_magnitude_scaled, weak_magnitude_scaled);

  if (effect_id_ != kInvalidEffectId)
    StartOrStopEffect(evdev_fd_, effect_id_, true);
}

void GamepadDeviceLinux::SetZeroVibration() {
  if (dualshock4_) {
    dualshock4_->SetZeroVibration();
    return;
  }

  if (switch_pro_) {
    switch_pro_->SetZeroVibration();
    return;
  }

  if (effect_id_ != kInvalidEffectId)
    StartOrStopEffect(evdev_fd_, effect_id_, false);
}

}  // namespace device
