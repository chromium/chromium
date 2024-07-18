// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/gamepad/gamepad_device_linux.h"

#include <fcntl.h>
#include <limits.h>
#include <linux/hidraw.h>
#include <linux/input.h>
#include <linux/joystick.h>
#include <sys/ioctl.h>

#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "device/gamepad/dualshock4_controller.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/hid_haptic_gamepad.h"
#include "device/gamepad/hid_writer_linux.h"
#include "device/gamepad/xbox_hid_controller.h"
#include "device/udev_linux/udev.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/dbus/permission_broker/permission_broker_client.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS)

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
    // Record is used for Xbox Series X's share button over BT.
    KEY_RECORD};
const size_t kSpecialKeysLen = std::size(kSpecialKeys);

#define LONG_BITS (CHAR_BIT * sizeof(long))
#define BITS_TO_LONGS(x) (((x) + LONG_BITS - 1) / LONG_BITS)

static inline bool test_bit(int bit, const unsigned long* data) {
  return data[bit / LONG_BITS] & (1UL << (bit % LONG_BITS));
}

GamepadBusType GetEvdevBusType(const base::ScopedFD& fd) {
  struct input_id input_info;
  if (HANDLE_EINTR(ioctl(fd.get(), EVIOCGID, &input_info)) >= 0) {
    if (input_info.bustype == BUS_USB)
      return GAMEPAD_BUS_USB;
    if (input_info.bustype == BUS_BLUETOOTH)
      return GAMEPAD_BUS_BLUETOOTH;
  }
  return GAMEPAD_BUS_UNKNOWN;
}

bool HasRumbleCapability(const base::ScopedFD& fd) {
  unsigned long evbit[BITS_TO_LONGS(EV_MAX)];
  unsigned long ffbit[BITS_TO_LONGS(FF_MAX)];

  if (HANDLE_EINTR(ioctl(fd.get(), EVIOCGBIT(0, sizeof(evbit)), evbit)) < 0 ||
      HANDLE_EINTR(ioctl(fd.get(), EVIOCGBIT(EV_FF, sizeof(ffbit)), ffbit)) <
          0) {
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
size_t CheckSpecialKeys(const base::ScopedFD& fd,
                        std::vector<bool>* has_special_key) {
  DCHECK(has_special_key);
  unsigned long evbit[BITS_TO_LONGS(EV_MAX)];
  unsigned long keybit[BITS_TO_LONGS(KEY_MAX)];
  size_t found_special_keys = 0;

  has_special_key->clear();
  if (HANDLE_EINTR(ioctl(fd.get(), EVIOCGBIT(0, sizeof(evbit)), evbit)) < 0 ||
      HANDLE_EINTR(ioctl(fd.get(), EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit)) <
          0) {
    return 0;
  }

  if (!test_bit(EV_KEY, evbit)) {
    return 0;
  }

  has_special_key->resize(kSpecialKeysLen, false);
  for (size_t special_index = 0; special_index < kSpecialKeysLen;
       ++special_index) {
    if (test_bit(kSpecialKeys[special_index], keybit)) {
      (*has_special_key)[special_index] = true;
      ++found_special_keys;
    }
  }

  return found_special_keys;
}

bool GetHidrawDevinfo(const base::ScopedFD& fd,
                      GamepadBusType* bus_type,
                      std::string* product_name,
                      uint16_t* vendor_id,
                      uint16_t* product_id) {
  struct hidraw_devinfo info;
  if (HANDLE_EINTR(ioctl(fd.get(), HIDIOCGRAWINFO, &info)) < 0)
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

  constexpr size_t kStringDescriptorMax = 256;
  if (product_name &&
      HANDLE_EINTR(ioctl(fd.get(), HIDIOCGRAWNAME(kStringDescriptorMax),
                         base::WriteInto(product_name, kStringDescriptorMax))) <
          0) {
    product_name->clear();
  }

  return true;
}

int StoreRumbleEffect(const base::ScopedFD& fd,
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

  if (HANDLE_EINTR(ioctl(fd.get(), EVIOCSFF, (const void*)&effect)) < 0)
    return kInvalidEffectId;
  return effect.id;
}

void DestroyEffect(const base::ScopedFD& fd, int effect_id) {
  HANDLE_EINTR(ioctl(fd.get(), EVIOCRMFF, effect_id));
}

bool StartOrStopEffect(const base::ScopedFD& fd, int effect_id, bool do_start) {
  struct input_event start_stop;
  memset(&start_stop, 0, sizeof(start_stop));
  start_stop.type = EV_FF;
  start_stop.code = effect_id;
  start_stop.value = do_start ? 1 : 0;
  ssize_t nbytes = HANDLE_EINTR(
      write(fd.get(), (const void*)&start_stop, sizeof(start_stop)));
  return nbytes == sizeof(start_stop);
}

uint16_t HexStringToUInt16WithDefault(std::string_view input,
                                      uint16_t default_value) {
  uint32_t out = 0;
  if (!base::HexStringToUInt(input, &out) ||
      out > std::numeric_limits<uint16_t>::max()) {
    return default_value;
  }
  return static_cast<uint16_t>(out);
}

#if BUILDFLAG(IS_CHROMEOS)
void OnOpenPathSuccess(
    chromeos::PermissionBrokerClient::OpenPathCallback callback,
    scoped_refptr<base::SequencedTaskRunner> polling_runner,
    base::ScopedFD fd) {
  polling_runner->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), std::move(fd)));
}

void OnOpenPathError(
    chromeos::PermissionBrokerClient::OpenPathCallback callback,
    scoped_refptr<base::SequencedTaskRunner> polling_runner,
    const std::string& error_name,
    const std::string& error_message) {
  polling_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), base::ScopedFD()));
}

void OpenPathWithPermissionBroker(
    const std::string& path,
    chromeos::PermissionBrokerClient::OpenPathCallback callback,
    scoped_refptr<base::SequencedTaskRunner> polling_runner) {
  auto* client = chromeos::PermissionBrokerClient::Get();
  DCHECK(client) << "Could not get permission broker client.";
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  auto success_callback = base::BindOnce(
      &OnOpenPathSuccess, std::move(split_callback.first), polling_runner);
  auto error_callback = base::BindOnce(
      &OnOpenPathError, std::move(split_callback.second), polling_runner);
  client->OpenPath(path, std::move(success_callback),
                   std::move(error_callback));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// Small helper to avoid constructing a std::string_view from nullptr.
std::string_view ToStringView(const char* str) {
  return str ? std::string_view(str) : std::string_view();
}

}  // namespace

GamepadDeviceLinux::GamepadDeviceLinux(
    const std::string& syspath_prefix,
    scoped_refptr<base::SequencedTaskRunner> dbus_runner)
    : syspath_prefix_(syspath_prefix),
      button_indices_used_(Gamepad::kButtonsLengthCap, false),
      dbus_runner_(dbus_runner),
      polling_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

GamepadDeviceLinux::~GamepadDeviceLinux() = default;

void GamepadDeviceLinux::DoShutdown() {
  CloseJoydevNode();
  CloseEvdevNode();
  CloseHidrawNode();
}

bool GamepadDeviceLinux::IsEmpty() const {
  return !joydev_fd_.is_valid() && !evdev_fd_.is_valid() &&
         !hidraw_fd_.is_valid();
}

bool GamepadDeviceLinux::SupportsVibration() const {
  static constexpr auto kNoVibration = base::MakeFixedFlatSet<GamepadId>({
      // The Xbox Adaptive Controller reports force feedback capability, but
      // the device itself does not have any vibration actuators.
      GamepadId::kMicrosoftProduct0b0a,
      // SteelSeries Stratus Duo is XInput but does not support vibration.
      GamepadId::kSteelSeriesProduct1430,
      GamepadId::kSteelSeriesProduct1431,
  });

  if (dualshock4_ || xbox_hid_ || hid_haptics_)
    return true;

  if (kNoVibration.contains(gamepad_id_))
    return false;

  return supports_force_feedback_ && evdev_fd_.is_valid();
}

void GamepadDeviceLinux::ReadPadState(Gamepad* pad) {
  DCHECK(joydev_fd_.is_valid());

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

  // Mark used buttons.
  for (size_t button_index = 0; button_index < Gamepad::kButtonsLengthCap;
       ++button_index) {
    pad->buttons[button_index].used = button_indices_used_[button_index];
  }

  if (pad_updated)
    pad->timestamp = GamepadDataFetcher::CurrentTimeInMicroseconds();
}

bool GamepadDeviceLinux::ReadJoydevState(Gamepad* pad) {
  DCHECK(polling_runner_->RunsTasksInCurrentSequence());
  DCHECK(pad);

  if (!joydev_fd_.is_valid())
    return false;

  // Read button and axis events from the joydev device.
  bool pad_updated = false;
  js_event event;
  while (HANDLE_EINTR(read(joydev_fd_.get(), &event, sizeof(struct js_event))) >
         0) {
    size_t item = event.number;
    if (event.type & JS_EVENT_AXIS) {
      if (item >= Gamepad::kAxesLengthCap)
        continue;

      pad->axes[item] = event.value / kMaxLinuxAxisValue;
      pad->axes_used |= 1 << item;

      if (item >= pad->axes_length)
        pad->axes_length = item + 1;
      pad_updated = true;
    } else if (event.type & JS_EVENT_BUTTON) {
      if (item >= Gamepad::kButtonsLengthCap)
        continue;

      pad->buttons[item].used = true;
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
  DCHECK(polling_runner_->RunsTasksInCurrentSequence());
  if (!evdev_fd_.is_valid())
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
  DCHECK(polling_runner_->RunsTasksInCurrentSequence());
  DCHECK(pad);

  if (!evdev_fd_.is_valid())
    return false;

  // Read special button events through evdev.
  bool pad_updated = false;
  input_event ev;
  ssize_t bytes_read;
  while ((bytes_read = HANDLE_EINTR(
              read(evdev_fd_.get(), &ev, sizeof(input_event)))) > 0) {
    if (static_cast<size_t>(bytes_read) < sizeof(input_event))
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
  return GetGamepadStandardMappingFunction(name_, vendor_id_, product_id_,
                                           hid_specification_version_,
                                           version_number_, bus_type_);
}

bool GamepadDeviceLinux::IsSameDevice(const UdevGamepadLinux& pad_info) {
  return pad_info.syspath_prefix == syspath_prefix_;
}

bool GamepadDeviceLinux::OpenJoydevNode(const UdevGamepadLinux& pad_info,
                                        udev_device* device) {
  DCHECK(polling_runner_->RunsTasksInCurrentSequence());
  DCHECK(pad_info.type == UdevGamepadLinux::Type::JOYDEV);
  DCHECK(pad_info.syspath_prefix == syspath_prefix_);

  CloseJoydevNode();
  joydev_fd_ =
      base::ScopedFD(open(pad_info.path.c_str(), O_RDONLY | O_NONBLOCK));
  if (!joydev_fd_.is_valid())
    return false;

  udev_device* parent_device =
      device::udev_device_get_parent_with_subsystem_devtype(
          device, kInputSubsystem, nullptr);

  const std::string_view vendor_id =
      ToStringView(udev_device_get_sysattr_value(parent_device, "id/vendor"));
  const std::string_view product_id =
      ToStringView(udev_device_get_sysattr_value(parent_device, "id/product"));
  const std::string_view hid_version =
      ToStringView(udev_device_get_sysattr_value(parent_device, "id/version"));
  const std::string_view name =
      ToStringView(udev_device_get_sysattr_value(parent_device, "name"));

  uint16_t vendor_id_int = HexStringToUInt16WithDefault(vendor_id, 0);
  uint16_t product_id_int = HexStringToUInt16WithDefault(product_id, 0);
  uint16_t hid_version_int = HexStringToUInt16WithDefault(hid_version, 0);
  uint16_t version_number_int = 0;

  // In many cases the information the input subsystem contains isn't
  // as good as the information that the device bus has, walk up further
  // to the subsystem/device type "usb"/"usb_device" and if this device
  // has the same vendor/product id, prefer the description from that.
  struct udev_device* usb_device =
      udev_device_get_parent_with_subsystem_devtype(
          parent_device, kUsbSubsystem, kUsbDeviceType);
  std::string name_string(name);
  if (usb_device) {
    const std::string_view usb_vendor_id =
        ToStringView(udev_device_get_sysattr_value(usb_device, "idVendor"));
    const std::string_view usb_product_id =
        ToStringView(udev_device_get_sysattr_value(usb_device, "idProduct"));

    if (vendor_id == usb_vendor_id && product_id == usb_product_id) {
      const char* manufacturer =
          udev_device_get_sysattr_value(usb_device, "manufacturer");
      const char* product =
          udev_device_get_sysattr_value(usb_device, "product");

      if (manufacturer && product) {
        // Replace the previous name string with one containing the better
        // information.
        name_string = base::StringPrintf("%s %s", manufacturer, product);
      }
    }

    const std::string_view version_number =
        ToStringView(udev_device_get_sysattr_value(usb_device, "bcdDevice"));
    version_number_int = HexStringToUInt16WithDefault(version_number, 0);
  }

  joydev_index_ = pad_info.index;
  vendor_id_ = vendor_id_int;
  product_id_ = product_id_int;
  hid_specification_version_ = hid_version_int;
  version_number_ = version_number_int;
  name_ = name_string;
  gamepad_id_ =
      GamepadIdList::Get().GetGamepadId(name_, vendor_id_, product_id_);

  return true;
}

void GamepadDeviceLinux::CloseJoydevNode() {
  DCHECK(polling_runner_->RunsTasksInCurrentSequence());
  joydev_fd_.reset();
  joydev_index_ = -1;
  vendor_id_ = 0;
  product_id_ = 0;
  version_number_ = 0;
  name_.clear();
  gamepad_id_ = GamepadId::kUnknownGamepad;

  // Button indices must be recomputed once the joydev node is closed.
  button_indices_used_.clear();
  special_button_map_.clear();
  evdev_special_keys_initialized_ = false;
}

bool GamepadDeviceLinux::OpenEvdevNode(const UdevGamepadLinux& pad_info) {
  DCHECK(polling_runner_->RunsTasksInCurrentSequence());
  DCHECK(pad_info.type == UdevGamepadLinux::Type::EVDEV);
  DCHECK(pad_info.syspath_prefix == syspath_prefix_);

  CloseEvdevNode();
  evdev_fd_ = base::ScopedFD(open(pad_info.path.c_str(), O_RDWR | O_NONBLOCK));
  if (!evdev_fd_.is_valid())
    return false;

  supports_force_feedback_ = HasRumbleCapability(evdev_fd_);
  bus_type_ = GetEvdevBusType(evdev_fd_);

  return true;
}

void GamepadDeviceLinux::CloseEvdevNode() {
  DCHECK(polling_runner_->RunsTasksInCurrentSequence());
  if (evdev_fd_.is_valid()) {
    if (effect_id_ != kInvalidEffectId) {
      DestroyEffect(evdev_fd_, effect_id_);
      effect_id_ = kInvalidEffectId;
    }
  }
  evdev_fd_.reset();
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

void GamepadDeviceLinux::OpenHidrawNode(const UdevGamepadLinux& pad_info,
                                        OpenDeviceNodeCallback callback) {
  DCHECK(polling_runner_->RunsTasksInCurrentSequence());
  DCHECK(pad_info.type == UdevGamepadLinux::Type::HIDRAW);
  DCHECK(pad_info.syspath_prefix == syspath_prefix_);

  CloseHidrawNode();

  auto fd = base::ScopedFD(open(pad_info.path.c_str(), O_RDWR | O_NONBLOCK));

#if BUILDFLAG(IS_CHROMEOS)
  // If we failed to open the device it may be due to insufficient permissions.
  // Try again using the PermissionBrokerClient.
  if (!fd.is_valid()) {
    DCHECK(dbus_runner_);
    DCHECK(polling_runner_);
    auto open_path_callback =
        base::BindOnce(&GamepadDeviceLinux::OnOpenHidrawNodeComplete,
                       weak_factory_.GetWeakPtr(), std::move(callback));
    dbus_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&OpenPathWithPermissionBroker, pad_info.path,
                       std::move(open_path_callback), polling_runner_));
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  OnOpenHidrawNodeComplete(std::move(callback), std::move(fd));
}

void GamepadDeviceLinux::OnOpenHidrawNodeComplete(
    OpenDeviceNodeCallback callback,
    base::ScopedFD fd) {
  DCHECK(polling_runner_->RunsTasksInCurrentSequence());
  if (fd.is_valid())
    InitializeHidraw(std::move(fd));
  std::move(callback).Run(this);
}

void GamepadDeviceLinux::InitializeHidraw(base::ScopedFD fd) {
  DCHECK(polling_runner_->RunsTasksInCurrentSequence());
  DCHECK(fd.is_valid());
  hidraw_fd_ = std::move(fd);

  std::string product_name;
  uint16_t vendor_id;
  uint16_t product_id;
  GamepadId gamepad_id;
  bool is_dualshock4 = false;
  bool is_xbox_hid = false;
  bool is_hid_haptic = false;
  if (GetHidrawDevinfo(hidraw_fd_, &bus_type_, &product_name, &vendor_id,
                       &product_id)) {
    gamepad_id =
        GamepadIdList::Get().GetGamepadId(product_name, vendor_id, product_id);
    is_dualshock4 = Dualshock4Controller::IsDualshock4(gamepad_id);
    is_xbox_hid = XboxHidController::IsXboxHid(gamepad_id);
    is_hid_haptic = HidHapticGamepad::IsHidHaptic(vendor_id, product_id);
    DCHECK_LE(is_dualshock4 + is_xbox_hid + is_hid_haptic, 1);
  }

  if (is_dualshock4 && !dualshock4_) {
    dualshock4_ = std::make_unique<Dualshock4Controller>(
        gamepad_id, bus_type_, std::make_unique<HidWriterLinux>(hidraw_fd_));
  }

  if (is_xbox_hid && !xbox_hid_) {
    xbox_hid_ = std::make_unique<XboxHidController>(
        std::make_unique<HidWriterLinux>(hidraw_fd_));
  }

  if (is_hid_haptic && !hid_haptics_) {
    hid_haptics_ = HidHapticGamepad::Create(
        vendor_id, product_id, std::make_unique<HidWriterLinux>(hidraw_fd_));
  }
}

void GamepadDeviceLinux::CloseHidrawNode() {
  DCHECK(polling_runner_->RunsTasksInCurrentSequence());
  if (dualshock4_)
    dualshock4_->Shutdown();
  dualshock4_.reset();
  if (xbox_hid_)
    xbox_hid_->Shutdown();
  xbox_hid_.reset();
  if (hid_haptics_)
    hid_haptics_->Shutdown();
  hid_haptics_.reset();
  hidraw_fd_.reset();
}

void GamepadDeviceLinux::SetVibration(
    mojom::GamepadEffectParametersPtr params) {
  DCHECK(polling_runner_->RunsTasksInCurrentSequence());
  if (dualshock4_) {
    dualshock4_->SetVibration(std::move(params));
    return;
  }

  if (xbox_hid_) {
    xbox_hid_->SetVibration(std::move(params));
    return;
  }

  if (hid_haptics_) {
    hid_haptics_->SetVibration(std::move(params));
    return;
  }

  uint16_t strong_magnitude_scaled =
      static_cast<uint16_t>(params->strong_magnitude * kRumbleMagnitudeMax);
  uint16_t weak_magnitude_scaled =
      static_cast<uint16_t>(params->weak_magnitude * kRumbleMagnitudeMax);

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
  DCHECK(polling_runner_->RunsTasksInCurrentSequence());
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

  if (effect_id_ != kInvalidEffectId)
    StartOrStopEffect(evdev_fd_, effect_id_, false);
}

base::WeakPtr<AbstractHapticGamepad> GamepadDeviceLinux::GetWeakPtr() {
  DCHECK(polling_runner_->RunsTasksInCurrentSequence());
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
