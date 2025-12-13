// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Overview:
//
// When initialized, EiSenderSession is provided with a file descriptor, the
// other end of which should be an Emulated Input Server (EIS) implementation.
// Typically, this will be a Wayland compositor such as Mutter. EiSenderSession
// will communicate with the EIS implementation and attempt to establish an EI
// session and take the role of a sender client, meaning a client that wants to
// inject emulated input. Assuming that succeeds, the EiSenderSession instance
// can then be used to send emulated input to the EIS implementation.
//
// Unlike many input injection APIs, the EI protocol doesn't provide top-level
// methods for "move pointer here" or "inject this key". Instead the EIS
// implementation provides the EI client with one or more "seats", where a seat
// is a collection of input devices that operate together. The EI client then
// bind capabilities to the seat, informing the EIS implementation what types of
// input the client wishes to be able to inject into that seat. Finally, the EIS
// implementation exposes some number of emulated devices to the client, each
// having a set of input injection capabilities.
//
// The EIS implementation has a lot of leeway in the set of devices it exposes
// to the client. It may expose one device that has all available capabilities,
// a different device for each capability, multiple devices providing the same
// capability, and any combination thereof. Devices may be added and removed at
// any time, and the present devices need not cover all of the requested
// capabilities. Additionally, a given device may either be "physical",
// representing emulated hardware input, such as a mouse moving some number of
// millimeters, or "logical", representing emulated input as it applies to the
// desktop environment, such a the pointer moving some number of logical pixels
// across the screen. When connecting to a Wayland compositor, the devices will
// generally be "logical".
//
// Some device capabilities have extra consideration. A device with the keyboard
// capability will have an associated XKB layout. This layout is fixed for the
// lifetime of the device. If the user has multiple input languages enabled,
// they will typically all be present in the XKB layout as separate "groups".
// Thus, switching between these languages doesn't change the XKB layout, but
// merely changes the active group (considered a modifier state change).
// Since a device's layout is fixed, if the EIS implementation does want to use
// a new XKB layout (perhaps the user enables a new language that wasn't
// originally included), the implementation must create a new keyboard device
// with the new layout. In the common case, an implementation will only have one
// XKB layout at a time, and thus will remove the previous device at the same
// time it adds the new one. However, nothing precludes an implementation from
// simultaneously having multiple devices with the keyboard capability and
// different layouts.
//
// A "logical" device with the absolute pointer capalibity also requires extra
// consideration. Each such device has a set of regions into which they can
// inject input. For a Wayland compositor, these regions correspond to logical
// monitors. The EIS implementation may offer one device per region, a single
// device for all regions, some combination, or even multiple devices that can
// inject into a given region.
//
// Because there may be multiple seats and multiple devices through which a
// given input event can be injected, the question arises as to which one to
// pick. The libei demo client always uses the first-added seat, and refers to
// it as the "default seat". EiSenderSession follows suit, and always binds the
// first seat added by the EIS implementation. For devices, it does the
// opposite: when injecting, it uses the most recently added device that can
// handle the event in question. This again mirrors the behavior of the demo
// client. For keyboard events, the most recent device presumably has the most
// up-to-date layout, and for other devices it theoretically shouldn't matter.
// We can always revisit if this approach turns out not to be ideal for some
// EIS implementation.
//
// A final note about mouse button and scroll events: originally, libei included
// button presses and scroll events as part of the relative pointer and absolute
// pointer capabilities. Before reaching 1.0, the library and EI protocol were
// updated to separate those events into their own button and scroll
// capabilities. The demo app still assumes that a device with a pointer
// capability will support injecting button and scroll events. Theoretically,
// however, the button and scroll capabilities could now be provided by
// different devices. EiSenderSession thus treats these capabilities like any
// other: when it needs to inject a button or scroll event, it will use whatever
// device providing the respective capability was most recently added. That
// should be fine, since a seat only has one logical pointer and it shouldn't
// matter what device is used to trigger a click on it.

#include "remoting/host/linux/ei_sender_session.h"

#include <linux/input-event-codes.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/loggable.h"
#include "remoting/host/linux/ei_input_injector.h"
#include "remoting/host/linux/ei_keyboard_layout_monitor.h"
#include "remoting/host/linux/ei_keymap.h"
#include "remoting/proto/event.pb.h"
#include "third_party/libei/cipd/include/libei-1.0/libei.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace remoting {

namespace {

constexpr int kPixelsPerTick = 120;

// This functionality is copied from fractional_input_filter.cc to maintain
// an equivalent functionality for now.
// TODO(rkjnsn): Once we are sure the client calculations indeed generate a 1.0
// fractional coordinate at the right and bottom edges for the cases we care
// about, and the discussion at
// https://gitlab.freedesktop.org/libinput/libei/-/issues/76 is resolved, this
// should be updated to convert to floating-point logical pixels such that 1.0
// is always translated to whatever value actually represents the
// right-/bottom-most absolute position for the libei region.
std::uint32_t ScaleAndClamp(float fraction,
                            std::uint32_t minimum,
                            std::uint32_t size) {
  std::uint32_t scaled = base::ClampRound<std::uint32_t>(fraction * size);
  scaled = std::clamp(scaled, std::uint32_t{0}, size - 1);
  return scaled + minimum;
}

std::pair<std::uint32_t, std::uint32_t> CalculateXY(ei_region* region,
                                                    float fractional_x,
                                                    float fractional_y) {
  std::uint32_t x = ScaleAndClamp(fractional_x, ei_region_get_x(region),
                                  ei_region_get_width(region));
  std::uint32_t y = ScaleAndClamp(fractional_y, ei_region_get_y(region),
                                  ei_region_get_height(region));
  return std::pair(x, y);
}

}  // namespace

EiSenderSession::~EiSenderSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(INFO) << "Ei session shutting down";
  // Calling ei_disconnect will cause REMOVED events to be generated for all
  // devices and seats, allowing us to free our per-device state.
  ei_disconnect(ei_.get());
  ProcessEvents(true);
}

base::WeakPtr<EiSenderSession> EiSenderSession::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void EiSenderSession::SetKeyboardLayoutMonitor(
    base::WeakPtr<EiKeyboardLayoutMonitor> monitor) {
  keyboard_layout_monitor_ = monitor;
  keyboard_layout_monitor_->OnKeymapChanged(
      keyboards_.empty() ? nullptr : std::get<1>(keyboards_.back()).get());
}

void EiSenderSession::SetInputInjector(
    base::WeakPtr<EiInputInjector> input_injector) {
  input_injector_ = input_injector;
  input_injector_->SetKeymap(
      keyboards_.empty() ? nullptr : std::get<1>(keyboards_.back())->GetWeakPtr());
}

void EiSenderSession::InjectKeyEvent(std::uint32_t usb_keycode, bool is_press) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (keyboards_.empty()) {
    LOG(ERROR) << "Received key event but there's no virtual keyboard";
    return;
  }

  // Assume the most-recently-received keyboard is the one we should use.
  auto& [keyboard, _] = keyboards_.back();

  ei_device_keyboard_key(
      keyboard.get(),
      ui::KeycodeConverter::DomCodeToEvdevCode(
          ui::KeycodeConverter::UsbKeycodeToDomCode(usb_keycode)),
      is_press);
  ei_device_frame(keyboard.get(), ei_now(ei_.get()));
}

void EiSenderSession::InjectAbsolutePointerMove(std::string_view region_id,
                                                float fractional_x,
                                                float fractional_y) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto [first_equal, first_greater] = absolute_pointers_.equal_range(region_id);
  if (first_equal == first_greater) {
    LOG(ERROR) << "No absolute pointer for the requested region";
    return;
  }

  // Assume the most-recently-received pointer is the one we should use.
  auto& [region, device] = (--first_greater)->second;

  auto [x, y] = CalculateXY(region.get(), fractional_x, fractional_y);

  ei_device_pointer_motion_absolute(device.get(), x, y);
  ei_device_frame(device.get(), ei_now(ei_.get()));
}

void EiSenderSession::InjectRelativePointerMove(std::int32_t delta_x,
                                                std::int32_t delta_y) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (relative_pointers_.empty()) {
    LOG(ERROR) << "Received relative motion but there's no relative pointer";
    return;
  }

  // Assume the most-recently-received pointer is the one we should use.
  auto& pointer = relative_pointers_.back();

  ei_device_pointer_motion(pointer.get(), delta_x, delta_y);
  ei_device_frame(pointer.get(), ei_now(ei_.get()));
}

void EiSenderSession::InjectButton(protocol::MouseEvent::MouseButton button,
                                   bool is_press) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  uint32_t button_code;

  switch (button) {
    case protocol::MouseEvent_MouseButton_BUTTON_UNDEFINED:
      return;
    case protocol::MouseEvent_MouseButton_BUTTON_LEFT:
      button_code = BTN_LEFT;
      break;
    case protocol::MouseEvent_MouseButton_BUTTON_MIDDLE:
      button_code = BTN_MIDDLE;
      break;
    case protocol::MouseEvent_MouseButton_BUTTON_RIGHT:
      button_code = BTN_RIGHT;
      break;
    case protocol::MouseEvent_MouseButton_BUTTON_BACK:
      button_code = BTN_BACK;
      break;
    case protocol::MouseEvent_MouseButton_BUTTON_FORWARD:
      button_code = BTN_FORWARD;
      break;
    default:
      LOG(WARNING) << "Unknown mouse button: " << button;
      return;
  }

  if (button_devices_.empty()) {
    LOG(ERROR) << "Received button event but there's no button device";
    return;
  }

  // The button capability might appear on multiple pointer devices, or on a
  // separate device altogether. Since each seat only has one logical pointer,
  // it should be fine to inject buttons on any device that supports them, so
  // just use the most recent one like with other devices.
  auto& button_device = button_devices_.back();

  ei_device_button_button(button_device.get(), button_code, is_press);
  ei_device_frame(button_device.get(), ei_now(ei_.get()));
}

void EiSenderSession::InjectScrollDelta(double delta_x, double delta_y) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (scroll_devices_.empty()) {
    LOG(ERROR) << "Received scroll event but there's no scroll device";
    return;
  }

  // Don't use ei_device_scroll_delta because Chrome overscrolls by about 16x.
  // Instead accumulate pixels until there's enough for one "tick", which is
  // what we do on X11.

  // Discard any accumulated pixels if the scroll direction changes.
  if (delta_x != 0) {
    if ((delta_x > 0) != (subtick_pixels_x_ > 0)) {
      subtick_pixels_x_ = 0;
    }
  }
  if (delta_y != 0) {
    if ((delta_y > 0) != (subtick_pixels_y_ > 0)) {
      subtick_pixels_y_ = 0;
    }
  }

  subtick_pixels_x_ += delta_x;
  subtick_pixels_y_ += delta_y;
  int ticks_x = subtick_pixels_x_ / kPixelsPerTick;
  int ticks_y = subtick_pixels_y_ / kPixelsPerTick;
  subtick_pixels_x_ %= kPixelsPerTick;
  subtick_pixels_y_ %= kPixelsPerTick;

  if (ticks_x == 0 && ticks_y == 0) {
    return;
  }

  // The scroll capability might appear on multiple pointer devices, or on a
  // separate device altogether. Since each seat only has one logical pointer,
  // it should be fine to inject scroll events on any device that supports them,
  // so just use the most recent one like with other devices.
  auto& scroll_device = button_devices_.back();

  // This function takes values representing 120ths of a tick, so 120 would be
  // one wheel tick, 240 would be two ticks, and 60 would be half of a tick.
  // Additionally, positive value as scroll down or to the right (the opposite
  // of the Chromoting protocol), so we need to flip the sign.
  ei_device_scroll_discrete(scroll_device.get(), -ticks_x * 120,
                            -ticks_y * 120);
  ei_device_frame(scroll_device.get(), ei_now(ei_.get()));
}

void EiSenderSession::InjectScrollDiscrete(float ticks_x, float ticks_y) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (scroll_devices_.empty()) {
    LOG(ERROR) << "Received scroll event but there's no scroll device";
    return;
  }

  subtick_pixels_x_ = 0;
  subtick_pixels_y_ = 0;

  // The scroll capability might appear on multiple pointer devices, or on a
  // separate device altogether. Since each seat only has one logical pointer,
  // it should be fine to inject scroll events on any device that supports them,
  // so just use the most recent one like with other devices.
  auto& scroll_device = button_devices_.back();

  // This function takes values representing 120ths of a tick, so 120 would be
  // one wheel tick, 240 would be two ticks, and 60 would be half of a tick.
  // Additionally, positive value as scroll down or to the right (the opposite
  // of the Chromoting protocol), so we need to flip the sign.
  ei_device_scroll_discrete(scroll_device.get(), -ticks_x * 120,
                            -ticks_y * 120);
  ei_device_frame(scroll_device.get(), ei_now(ei_.get()));
}

void EiSenderSession::CreateWithFd(base::ScopedFD fd, CreateCallback callback) {
  auto sender_session = base::WrapUnique(new EiSenderSession());
  auto* raw = sender_session.get();
  raw->InitWithFd(
      std::move(fd),
      base::BindOnce(
          [](std::unique_ptr<EiSenderSession> sender_session,
             CreateCallback callback, base::expected<void, Loggable> result) {
            std::move(callback).Run(
                result.transform([&]() { return std::move(sender_session); })
                    .transform_error([](Loggable&& error) {
                      error.AddContext(FROM_HERE,
                                       "While creating new EI session");
                      return std::move(error);
                    }));
          },
          std::move(sender_session), std::move(callback)));
}

EiSenderSession::EiSenderSession() = default;

void EiSenderSession::InitWithFd(base::ScopedFD fd, InitCallback callback) {
  init_callback_ = std::move(callback);
  ei_ = EiPtr::Take(ei_new_sender(nullptr));
  int result = ei_setup_backend_fd(ei_.get(), fd.release());
  if (result != 0) {
    std::move(init_callback_)
        .Run(base::unexpected(Loggable(
            FROM_HERE,
            base::StrCat({"Failed to set up libei: ",
                          logging::SystemErrorCodeToString(-result)}))));
    return;
  }

  // Unretained is safe because no callback will occur after the returned
  // controller is destroyed.
  fd_watcher_ = base::FileDescriptorWatcher::WatchReadable(
      ei_get_fd(ei_.get()), base::BindRepeating(&EiSenderSession::OnFdReadable,
                                                base::Unretained(this)));
}

void EiSenderSession::OnFdReadable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ei_dispatch(ei_.get());
  ProcessEvents(false);
}

void EiSenderSession::OnConnected() {
  HOST_LOG << "Connected to EIS";
}

void EiSenderSession::OnDisconnected(bool shutting_down) {
  if (shutting_down) {
    return;
  }

  if (init_callback_) {
    std::move(init_callback_)
        .Run(base::unexpected(
            Loggable(FROM_HERE, "EIS denied connection request")));
    return;
  }
  LOG(ERROR) << "Unexpectedly disconnected from EIS";
}

void EiSenderSession::OnSeatAdded(EiSeatPtr seat) {
  if (default_seat_.get()) {
    HOST_LOG << "Ignoring additional seat";
    return;
  }

  if (!ei_seat_has_capability(seat.get(), EI_DEVICE_CAP_KEYBOARD)) {
    LOG(WARNING) << "EIS does not offer keyboard input";
  }
  if (!ei_seat_has_capability(seat.get(), EI_DEVICE_CAP_POINTER_ABSOLUTE)) {
    LOG(WARNING) << "EIS does not offer an absolute pointer device";
  }

  supports_touch_ = ei_seat_has_capability(seat.get(), EI_DEVICE_CAP_TOUCH);
  supports_relative_pointer_ =
      ei_seat_has_capability(seat.get(), EI_DEVICE_CAP_POINTER);

  ei_seat_bind_capabilities(seat.get(), EI_DEVICE_CAP_POINTER,
                            EI_DEVICE_CAP_KEYBOARD,
                            EI_DEVICE_CAP_POINTER_ABSOLUTE, EI_DEVICE_CAP_TOUCH,
                            EI_DEVICE_CAP_BUTTON, EI_DEVICE_CAP_SCROLL, NULL);

  if (init_callback_) {
    std::move(init_callback_).Run(base::ok());
  } else {
    HOST_LOG << "EIS seat readded";
  }
}

void EiSenderSession::OnSeatRemoved(EiSeatPtr seat) {
  if (seat == default_seat_) {
    default_seat_.reset();
    LOG(WARNING) << "EIS seat removed";
  }
}

void EiSenderSession::OnDeviceAdded(EiDevicePtr device) {
  AllocDeviceState(device);
  // The compositor might provide a device with multiple capabilities, in which
  // case it will be inserted in multiple lists.
  if (ei_device_has_capability(device.get(), EI_DEVICE_CAP_KEYBOARD)) {
    keyboards_.push_back(
        std::make_tuple(device, std::make_unique<EiKeymap>(device)));
    std::get<1>(keyboards_.back())
        ->Load(base::BindOnce(&EiSenderSession::OnKeymapLoaded, GetWeakPtr(),
                              device));
  }
  if (ei_device_has_capability(device.get(), EI_DEVICE_CAP_POINTER)) {
    relative_pointers_.push_back({device});
  }
  if (ei_device_has_capability(device.get(), EI_DEVICE_CAP_BUTTON)) {
    button_devices_.push_back({device});
  }
  if (ei_device_has_capability(device.get(), EI_DEVICE_CAP_SCROLL)) {
    scroll_devices_.push_back({device});
  }
  if (ei_device_has_capability(device.get(), EI_DEVICE_CAP_POINTER_ABSOLUTE)) {
    AddDeviceRegions(absolute_pointers_, {device});
  }
  if (ei_device_has_capability(device.get(), EI_DEVICE_CAP_TOUCH)) {
    AddDeviceRegions(touch_devices_, {device});
  }
}

void EiSenderSession::OnDeviceRemoved(EiDevicePtr device) {
  if (ei_device_has_capability(device.get(), EI_DEVICE_CAP_KEYBOARD)) {
    bool is_current =
        (!keyboards_.empty() && std::get<0>(keyboards_.back()) == device);
    std::erase_if(keyboards_, [&device](auto& item) {
      return std::get<0>(item) == device;
    });
    if (is_current && keyboard_layout_monitor_) {
      keyboard_layout_monitor_->OnKeymapChanged(
          keyboards_.empty() ? nullptr : std::get<1>(keyboards_.back()).get());
    }
  }
  if (ei_device_has_capability(device.get(), EI_DEVICE_CAP_POINTER)) {
    std::erase_if(relative_pointers_,
                  [&device](auto& item) { return item == device; });
  }
  if (ei_device_has_capability(device.get(), EI_DEVICE_CAP_BUTTON)) {
    std::erase_if(button_devices_,
                  [&device](auto& item) { return item == device; });
  }
  if (ei_device_has_capability(device.get(), EI_DEVICE_CAP_SCROLL)) {
    std::erase_if(scroll_devices_,
                  [&device](auto& item) { return item == device; });
  }
  if (ei_device_has_capability(device.get(), EI_DEVICE_CAP_POINTER_ABSOLUTE)) {
    std::erase_if(absolute_pointers_, [&device](auto& item) {
      return item.second.second == device;
    });
  }
  if (ei_device_has_capability(device.get(), EI_DEVICE_CAP_TOUCH)) {
    std::erase_if(touch_devices_, [&device](auto& item) {
      return item.second.second == device;
    });
  }
  FreeDeviceState(device);
}

void EiSenderSession::OnDevicePaused(EiDevicePtr device) {
  GetDeviceState(device).resumed = false;
}

void EiSenderSession::OnDeviceResumed(EiDevicePtr device) {
  GetDeviceState(device).resumed = true;
  // TODO(rkjnsn): Only call this on devices we expect to use.
  // TODO(rkjnsn): In the future, we'll want the host to keep the session open
  // between connections to preserve virtual monitors, et cetera. At that point,
  // we should probably call stop_emulating on disconnect and start_emulating on
  // connection.
  ei_device_start_emulating(device.get(), ++start_emulating_sequence_);
}

void EiSenderSession::OnKeymapLoaded(EiDevicePtr keyboard) {
  // If the load corresponds to the most recently-added keyboard, notify the
  // client.
  if (!keyboards_.empty()) {
    auto& [most_recent_keyboard, keymap] = keyboards_.back();
    if (keyboard == most_recent_keyboard) {
      if (keyboard_layout_monitor_) {
        keyboard_layout_monitor_->OnKeymapChanged(keymap.get());
      }
      if (input_injector_) {
        input_injector_->SetKeymap(keymap->GetWeakPtr());
      }
    }
  }
}

void EiSenderSession::ProcessEvents(bool shutting_down) {
  while (auto event = EiEventPtr(ei_get_event(ei_.get()))) {
    switch (ei_event_get_type(event.get())) {
      case EI_EVENT_CONNECT:
        OnConnected();
        break;
      case EI_EVENT_DISCONNECT:
        OnDisconnected(shutting_down);
        return;
      case EI_EVENT_SEAT_ADDED:
        OnSeatAdded(EiSeatPtr::Ref(ei_event_get_seat(event.get())));
        break;
      case EI_EVENT_SEAT_REMOVED:
        OnSeatRemoved(EiSeatPtr::Ref(ei_event_get_seat(event.get())));
        break;
      case EI_EVENT_DEVICE_ADDED:
        OnDeviceAdded(EiDevicePtr::Ref(ei_event_get_device(event.get())));
        break;
      case EI_EVENT_DEVICE_REMOVED:
        OnDeviceRemoved(EiDevicePtr::Ref(ei_event_get_device(event.get())));
        break;
      case EI_EVENT_DEVICE_PAUSED:
        OnDevicePaused(EiDevicePtr::Ref(ei_event_get_device(event.get())));
        break;
      case EI_EVENT_DEVICE_RESUMED:
        OnDeviceResumed(EiDevicePtr::Ref(ei_event_get_device(event.get())));
        break;
      case EI_EVENT_KEYBOARD_MODIFIERS:
        // TODO(rkjnsn): Track changes to the group for the layout monitor.
        break;
      case EI_EVENT_SYNC:
        break;
      default:
        std::string message = base::StringPrintf(
            "Unexpected libei event type: %d", ei_event_get_type(event.get()));
        if (init_callback_) {
          std::move(init_callback_)
              .Run(base::unexpected(Loggable(FROM_HERE, std::move(message))));
          return;
        }
        LOG(ERROR) << message;
        break;
    }
  }
}

void EiSenderSession::AddDeviceRegions(
    std::multimap<std::string,
                  std::pair<EiRegionPtr, EiDevicePtr>,
                  std::less<>>& map,
    EiDevicePtr device) {
  for (size_t i = 0; ei_region* region = ei_device_get_region(device.get(), i);
       ++i) {
    const char* mapping_id = ei_region_get_mapping_id(region);
    // Some DEs do not support mapping IDs, and will pass an empty string to
    // InjectAbsolutePointerMove().
    std::string_view mapping_id_view =
        mapping_id ? mapping_id : std::string_view{};
    if (mapping_id_view.empty()) {
      HOST_LOG << "Region found without mapping id";
    }
    map.emplace(std::piecewise_construct, std::tuple(mapping_id_view),
                std::forward_as_tuple(EiRegionPtr::Ref(region), device));
  }
}

void EiSenderSession::AllocDeviceState(const EiDevicePtr& device) {
  ei_device_set_user_data(device.get(), new DeviceState());
}

EiSenderSession::DeviceState& EiSenderSession::GetDeviceState(
    const EiDevicePtr& device) {
  return *static_cast<DeviceState*>(ei_device_get_user_data(device.get()));
}

void EiSenderSession::FreeDeviceState(const EiDevicePtr& device) {
  delete &GetDeviceState(device);
}

}  // namespace remoting
