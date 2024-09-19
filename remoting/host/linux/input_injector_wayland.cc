// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/input_injector_wayland.h"

#include <gtk/gtk.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "remoting/base/constants.h"
#include "remoting/base/logging.h"
#include "remoting/host/clipboard.h"
#include "remoting/host/input_injector_constants_linux.h"
#include "remoting/host/input_injector_metadata.h"
#include "remoting/host/linux/clipboard_wayland.h"
#include "remoting/host/linux/remote_desktop_portal_injector.h"
#include "remoting/host/linux/unicode_to_keysym.h"
#include "remoting/host/linux/wayland_manager.h"
#include "remoting/proto/internal.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/portal/scoped_glib.h"
#include "third_party/webrtc/modules/portal/xdg_desktop_portal_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "remoting/host/chromeos/point_transformer.h"
#endif

namespace remoting {
namespace {

using protocol::ClipboardEvent;
using protocol::KeyEvent;
using protocol::MouseEvent;
using protocol::TextEvent;
using protocol::TouchEvent;
using webrtc::xdg_portal::SessionDetails;
using xdg_portal::RemoteDesktopPortalInjector;

constexpr int BUTTON_LEFT_KEYCODE = 272;
constexpr int BUTTON_RIGHT_KEYCODE = 273;
constexpr int BUTTON_MIDDLE_KEYCODE = 274;
constexpr int BUTTON_FORWARD_KEYCODE = 277;
constexpr int BUTTON_BACK_KEYCODE = 278;
constexpr int BUTTON_UNKNOWN_KEYCODE = -1;
constexpr int MIN_KEYCODE = 8;
constexpr int SHIFT_KEY_CODE = 42;

ScrollDirection WheelDeltaToScrollDirection(float num) {
  return (num > 0)   ? ScrollDirection::UP
         : (num < 0) ? ScrollDirection::DOWN
                     : ScrollDirection::NONE;
}

bool IsDomModifierKey(ui::DomCode dom_code) {
  return dom_code == ui::DomCode::CONTROL_LEFT ||
         dom_code == ui::DomCode::SHIFT_LEFT ||
         dom_code == ui::DomCode::ALT_LEFT ||
         dom_code == ui::DomCode::META_LEFT ||
         dom_code == ui::DomCode::CONTROL_RIGHT ||
         dom_code == ui::DomCode::SHIFT_RIGHT ||
         dom_code == ui::DomCode::ALT_RIGHT ||
         dom_code == ui::DomCode::META_RIGHT;
}

int MouseButtonToEvdevCode(MouseEvent::MouseButton button) {
  switch (button) {
    case MouseEvent::BUTTON_LEFT:
      return BUTTON_LEFT_KEYCODE;
    case MouseEvent::BUTTON_RIGHT:
      return BUTTON_RIGHT_KEYCODE;
    case MouseEvent::BUTTON_MIDDLE:
      return BUTTON_MIDDLE_KEYCODE;
    case MouseEvent::BUTTON_BACK:
      return BUTTON_BACK_KEYCODE;
    case MouseEvent::BUTTON_FORWARD:
      return BUTTON_FORWARD_KEYCODE;
    case MouseEvent::BUTTON_UNDEFINED:
    default:
      return BUTTON_UNKNOWN_KEYCODE;
  }
}

// Pixel-to-wheel-ticks conversion ratio used by GTK.
// From third_party/WebKit/Source/web/gtk/WebInputEventFactory.cpp .
constexpr float kWheelTicksPerPixel = 3.0f / 160.0f;

// When the user is scrolling, generate at least one tick per time period.
constexpr base::TimeDelta kContinuousScrollTimeout = base::Milliseconds(500);

}  // namespace

InputInjectorWayland::InputInjectorWayland(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  core_ = new Core(task_runner);

  // Register callback with the wayland manager so that it can get details
  // about the desktop capture metadata (which include session details of the
  // portal).
  auto converting_cb =
      base::BindRepeating([](const webrtc::DesktopCaptureMetadata metadata) {
        return metadata.session_details;
      });
  WaylandManager::Get()->AddCapturerMetadataCallback(converting_cb.Then(
      base::BindRepeating(&Core::SetRemoteDesktopSessionDetails, core_)));
  WaylandManager::Get()->AddClipboardMetadataCallback(converting_cb.Then(
      base::BindRepeating(&Core::SetClipboardSessionDetails, core_)));
  WaylandManager::Get()->AddCapturerDestroyedCallback(
      base::BindRepeating(&Core::Shutdown, core_));
}

InputInjectorWayland::~InputInjectorWayland() = default;

void InputInjectorWayland::InjectClipboardEvent(const ClipboardEvent& event) {
  core_->InjectClipboardEvent(event);
}

void InputInjectorWayland::InjectKeyEvent(const KeyEvent& event) {
  core_->InjectKeyEvent(event);
}

void InputInjectorWayland::InjectTextEvent(const TextEvent& event) {
  core_->InjectTextEvent(event);
}

void InputInjectorWayland::InjectMouseEvent(const MouseEvent& event) {
  core_->InjectMouseEvent(event);
}

void InputInjectorWayland::InjectTouchEvent(const TouchEvent& event) {
  NOTIMPLEMENTED()
      << "Raw touch event injection is not implemented for wayland.";
}

void InputInjectorWayland::Start(
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {
  core_->Start(std::move(client_clipboard));
}

void InputInjectorWayland::SetMetadata(InputInjectorMetadata metadata) {
  core_->SetRemoteDesktopSessionDetails(std::move(metadata.session_details));
}

InputInjectorWayland::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner)
    : input_task_runner_(input_task_runner) {
  clipboard_ = std::make_unique<ClipboardWayland>();
}

void InputInjectorWayland::Core::SetCapabilityCallbacks() {
  if (!input_task_runner_->BelongsToCurrentThread()) {
    input_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::SetCapabilityCallbacks, this));
    return;
  }

  auto on_seat_acquired_keyboard_capability =
      base::BindOnce(&Core::SeatAcquiredKeyboardCapability, this);
  auto on_seat_acquired_pointer_capability =
      base::BindOnce(&Core::SeatAcquiredPointerCapability, this);
  auto on_seat_present =
      base::BindOnce(&WaylandManager::SetCapabilityCallbacks,
                     base::Unretained(WaylandManager::Get()),
                     std::move(on_seat_acquired_keyboard_capability),
                     std::move(on_seat_acquired_pointer_capability));
  WaylandManager::Get()->SetSeatPresentCallback(std::move(on_seat_present));
}

void InputInjectorWayland::Core::InjectFakeKeyEvent() {
  DCHECK(input_task_runner_->BelongsToCurrentThread());
  if (seat_has_keyboard_capability_) {
    return;
  }

  // Press shift key once.
  InjectKeyPress(SHIFT_KEY_CODE, /*pressed=*/true);
  InjectKeyPress(SHIFT_KEY_CODE, /*pressed=*/false);
}

void InputInjectorWayland::Core::InjectFakePointerEvent() {
  DCHECK(input_task_runner_->BelongsToCurrentThread());
  if (seat_has_pointer_capability_) {
    return;
  }

  MovePointerTo(0, -1);
  MovePointerTo(0, 1);
}

void InputInjectorWayland::Core::SeatAcquiredKeyboardCapability() {
  if (!input_task_runner_->BelongsToCurrentThread()) {
    input_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::SeatAcquiredKeyboardCapability, this));
    return;
  }

  seat_has_keyboard_capability_ = true;
  if (seat_has_pointer_capability_) {
    current_state_ = State::CAPABILITIES_RECEIVED;
  }
  MaybeFlushPendingEvents();
}

void InputInjectorWayland::Core::SeatAcquiredPointerCapability() {
  if (!input_task_runner_->BelongsToCurrentThread()) {
    input_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::SeatAcquiredPointerCapability, this));
    return;
  }

  seat_has_pointer_capability_ = true;
  if (seat_has_keyboard_capability_) {
    current_state_ = State::CAPABILITIES_RECEIVED;
  }
  MaybeFlushPendingEvents();
}

void InputInjectorWayland::Core::InjectClipboardEvent(
    const ClipboardEvent& event) {
  if (!input_task_runner_->BelongsToCurrentThread()) {
    input_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::InjectClipboardEvent, this, event));
    return;
  }
  if (!clipboard_initialized_) {
    pending_clipboard_event_ = std::make_optional(event);
    return;
  }
  clipboard_->InjectClipboardEvent(event);
}

void InputInjectorWayland::Core::QueueKeyEvent(
    const protocol::KeyEvent& event) {
  DCHECK(input_task_runner_->BelongsToCurrentThread());

  return pending_remote_desktop_tasks_.push(
      base::BindOnce(&Core::InjectKeyEventHelper, this, event));
}

void InputInjectorWayland::Core::QueueMouseEvent(
    const protocol::MouseEvent& event) {
  DCHECK(input_task_runner_->BelongsToCurrentThread());

  return pending_remote_desktop_tasks_.push(
      base::BindOnce(&Core::InjectMouseEventHelper, this, event));
}

void InputInjectorWayland::Core::ProcessKeyEvent(
    const protocol::KeyEvent& event) {
  DCHECK(input_task_runner_->BelongsToCurrentThread());

  switch (current_state_) {
    case State::UNINITIALIZED:
    case State::SESSION_INITIALIZED:
      QueueKeyEvent(event);
      break;
    case State::CAPABILITIES_RECEIVED:
      InjectKeyEventHelper(event);
      break;
    case State::STOPPED:
      break;
    default:
      NOTREACHED();
  }
}
void InputInjectorWayland::Core::ProcessMouseEvent(
    const protocol::MouseEvent& event) {
  DCHECK(input_task_runner_->BelongsToCurrentThread());

  switch (current_state_) {
    case State::UNINITIALIZED:
    case State::SESSION_INITIALIZED:
      QueueMouseEvent(event);
      break;
    case State::CAPABILITIES_RECEIVED:
      InjectMouseEventHelper(event);
      break;
    case State::STOPPED:
      break;
    default:
      NOTREACHED();
  }
}

void InputInjectorWayland::Core::InjectKeyEvent(const KeyEvent& event) {
  if (!input_task_runner_->BelongsToCurrentThread()) {
    input_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::InjectKeyEvent, this, event));
    return;
  }

  ProcessKeyEvent(event);
}

void InputInjectorWayland::Core::InjectKeyEventHelper(const KeyEvent& event) {
  DCHECK(input_task_runner_->BelongsToCurrentThread());

  // HostEventDispatcher should filter events missing the pressed field.
  if (!event.has_pressed() || !event.has_usb_keycode()) {
    return;
  }

  int keycode =
      ui::KeycodeConverter::UsbKeycodeToNativeKeycode(event.usb_keycode()) -
      MIN_KEYCODE;

  // Ignore events which can't be mapped.
  if (keycode == ui::KeycodeConverter::InvalidNativeKeycode()) {
    LOG(ERROR) << __func__ << " : Invalid key code";
    return;
  }

  if (event.pressed()) {
    if (base::Contains(pressed_keys_, keycode)) {
      // Ignore repeats for modifier keys.
      if (IsDomModifierKey(static_cast<ui::DomCode>(event.usb_keycode()))) {
        return;
      }
      // Key is already held down, so lift the key up to ensure this repeated
      // press takes effect.
      InjectKeyPress(keycode, /*pressed=*/false);
    }
    pressed_keys_.insert(keycode);
  } else {
    pressed_keys_.erase(keycode);
  }

  InjectKeyPress(keycode, event.pressed());
}

void InputInjectorWayland::Core::InjectTextEvent(const TextEvent& event) {
  NOTIMPLEMENTED() << "Text event injection is not implemented for wayland.";
}

InputInjectorWayland::Core::~Core() {
  CHECK(pressed_keys_.empty());

  // may be called from the network thread when the session is closed
  if (!input_task_runner_->BelongsToCurrentThread()) {
    input_task_runner_->DeleteSoon(FROM_HERE, clipboard_.release());
  }
}

void InputInjectorWayland::Core::InjectMouseEvent(const MouseEvent& event) {
  if (!input_task_runner_->BelongsToCurrentThread()) {
    input_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::InjectMouseEvent, this, event));
    return;
  }
  ProcessMouseEvent(event);
}

void InputInjectorWayland::Core::InjectMouseEventHelper(
    const MouseEvent& event) {
  DCHECK(input_task_runner_->BelongsToCurrentThread());

  if (event.has_delta_x() && event.has_delta_y() &&
      (event.delta_x() != 0 || event.delta_y() != 0)) {
    MovePointerBy(event.delta_x(), event.delta_y());
  } else if (event.has_x() && event.has_y()) {
    // Injecting a motion event immediately before a button release results in
    // a MotionNotify even if the mouse position hasn't changed, which confuses
    // apps which assume MotionNotify implies movement. See crbug.com/138075.
    bool inject_motion = true;
    webrtc::DesktopVector new_mouse_position(event.x(), event.y());
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Interim hack to handle display rotation on Chrome OS.
    // TODO(crbug.com/40396937): Remove this when Chrome OS has completely
    // migrated to Ozone.
    gfx::PointF screen_location = point_transformer_.ToScreenCoordinates(
        gfx::PointF(event.x(), event.y()));
    new_mouse_position.set(screen_location.x(), screen_location.y());
#endif
    if (event.has_button() && event.has_button_down() && !event.button_down()) {
      if (latest_mouse_position_ &&
          new_mouse_position.equals(*latest_mouse_position_)) {
        inject_motion = false;
      }
    }

    if (inject_motion) {
      webrtc::DesktopVector target_mouse_position{
          std::max(0, new_mouse_position.x()),
          std::max(0, new_mouse_position.y())};

      MovePointerTo(target_mouse_position.x(), target_mouse_position.y());
      latest_mouse_position_ = {target_mouse_position.x(),
                                target_mouse_position.y()};
    }
  }

  if (event.has_button() && event.has_button_down()) {
    int button_number = MouseButtonToEvdevCode(event.button());

    if (button_number < 0) {
      LOG(WARNING) << "Ignoring unknown button type: " << event.button();
      return;
    }
    VLOG(3) << "Pressing mouse button: " << event.button()
            << ", number: " << button_number;
    InjectMouseButton(button_number, event.button_down());
  }

  // remotedesktop.google.com currently sends scroll events in pixels, which
  // are accumulated host-side.
  int ticks_y = 0;
  if (event.has_wheel_ticks_y()) {
    ticks_y = static_cast<int>(event.wheel_ticks_y());
  } else if (event.has_wheel_delta_y()) {
    wheel_ticks_y_ += event.wheel_delta_y() * kWheelTicksPerPixel;
    ticks_y = static_cast<int>(wheel_ticks_y_);
    wheel_ticks_y_ -= ticks_y;
  }
  auto now = base::TimeTicks::Now();
  if (ticks_y == 0 && event.has_wheel_delta_y()) {
    // For the y-direction only (the common case), try to ensure that a tick is
    // injected when the user would expect one, regardless of how many pixels
    // the client sends per tick (even if it accelerates wheel events). To do
    // this, generate a tick if one has not occurred recently in the current
    // scroll direction. The accumulated pixels are not reset in this case.
    //
    // The effect when a physical mouse is used is as follows:
    //
    // Client sends slightly too few pixels per tick (e.g. Linux):
    // * First scroll in either direction synthesizes a tick.
    // * Subsequent scrolls in the same direction are unaffected (their
    //   accumulated pixel deltas mostly meet the threshold for a regular
    //   tick; occasionally a tick will be dropped if the user is scrolling
    //   quickly).
    //
    // Client sends far too few pixels per tick, but applies acceleration
    // (e.g. macOs, ChromeOS):
    // * First scroll in either direction synthesizes a tick.
    // * Slow scrolling will synthesize a tick a few times per second.
    // * Fast scrolling is unaffected (acceleration means that enough pixels
    //   are accumulated naturally).
    //
    // Client sends too many pixels per tick (e.g. Windows):
    // * Scrolling is unaffected (most scroll events generate one tick; every
    //   so often one generates two ticks).
    //
    // The effect when a trackpad is used is that the first tick in either
    // direction occurs sooner; subsequent ticks are mostly unaffected.
    const ScrollDirection current_tick_y_direction =
        WheelDeltaToScrollDirection(event.wheel_delta_y());
    if ((now - latest_tick_y_event_ > kContinuousScrollTimeout) ||
        latest_tick_y_direction_ != current_tick_y_direction) {
      ticks_y = static_cast<int>(current_tick_y_direction);
    }
  }
  if (ticks_y != 0) {
    latest_tick_y_direction_ = WheelDeltaToScrollDirection(ticks_y);
    latest_tick_y_event_ = now;
    InjectMouseScroll(RemoteDesktopPortalInjector::ScrollType::VERTICAL_SCROLL,
                      -ticks_y);
  }

  int ticks_x = 0;
  if (event.has_wheel_ticks_x()) {
    ticks_x = event.wheel_ticks_x();
  } else if (event.has_wheel_delta_x()) {
    wheel_ticks_x_ += event.wheel_delta_x() * kWheelTicksPerPixel;
    ticks_x = static_cast<int>(wheel_ticks_x_);
    wheel_ticks_x_ -= ticks_x;
  }
  if (ticks_x != 0) {
    InjectMouseScroll(
        RemoteDesktopPortalInjector::ScrollType::HORIZONTAL_SCROLL, -ticks_x);
  }
}

void InputInjectorWayland::Core::InjectPendingEvents(bool libei_succeeded) {
  if (!input_task_runner_->BelongsToCurrentThread()) {
    input_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&Core::InjectPendingEvents, this, libei_succeeded));
    return;
  }

  if (current_state_ == State::STOPPED) {
    return;
  }

  if (!libei_succeeded) {
    LOG(WARNING) << "Setting up libei failed, going to rely on slower "
                 << "input injection path";

    // These are needed so that we can acquire keyboard/pointer capability.
    InjectFakeKeyEvent();
    InjectFakePointerEvent();
  } else {
    // With libei we don't have to inject fake events and wait for
    // capabilities, so we mark the capabilities ready here.
    seat_has_keyboard_capability_ = true;
    seat_has_pointer_capability_ = true;
    current_state_ = State::CAPABILITIES_RECEIVED;
  }

  MaybeFlushPendingEvents();
}

void InputInjectorWayland::Core::SetRemoteDesktopSessionDetails(
    const SessionDetails& session_details) {
  if (!input_task_runner_->BelongsToCurrentThread()) {
    input_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::SetRemoteDesktopSessionDetails, this,
                                  session_details));
    return;
  }
  remotedesktop_portal_.SetSessionDetails(session_details);
  SetCapabilityCallbacks();

  current_state_ = State::SESSION_INITIALIZED;

  remotedesktop_portal_.SetupLibei(
      base::BindOnce(&Core::InjectPendingEvents, this));
}

void InputInjectorWayland::Core::MaybeFlushPendingEvents() {
  DCHECK(input_task_runner_->BelongsToCurrentThread());

  if (current_state_ != State::CAPABILITIES_RECEIVED ||
      current_state_ == State::STOPPED) {
    return;
  }

  while (!pending_remote_desktop_tasks_.empty()) {
    base::OnceClosure task = std::move(pending_remote_desktop_tasks_.front());
    pending_remote_desktop_tasks_.pop();
    std::move(task).Run();
  }
}

void InputInjectorWayland::Core::SetClipboardSessionDetails(
    const SessionDetails& session_details) {
  if (!input_task_runner_->BelongsToCurrentThread()) {
    input_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::SetClipboardSessionDetails, this,
                                  session_details));
    return;
  }

  clipboard_->SetSessionDetails(session_details);
  clipboard_initialized_ = true;

  // rerun the last pending clipboard task
  if (pending_clipboard_event_.has_value()) {
    clipboard_->InjectClipboardEvent(pending_clipboard_event_.value());
  }
}

void InputInjectorWayland::Core::InjectMouseButton(unsigned int code,
                                                   bool pressed) {
  DCHECK(input_task_runner_->BelongsToCurrentThread());

  remotedesktop_portal_.InjectMouseButton(code, pressed);
}

void InputInjectorWayland::Core::InjectMouseScroll(unsigned int axis,
                                                   int steps) {
  DCHECK(input_task_runner_->BelongsToCurrentThread());

  remotedesktop_portal_.InjectMouseScroll(axis, steps);
}

void InputInjectorWayland::Core::MovePointerTo(int x, int y) {
  DCHECK(input_task_runner_->BelongsToCurrentThread());

  remotedesktop_portal_.MovePointerTo(x, y);
}

void InputInjectorWayland::Core::MovePointerBy(int delta_x, int delta_y) {
  DCHECK(input_task_runner_->BelongsToCurrentThread());

  remotedesktop_portal_.MovePointerBy(delta_x, delta_y);
}

void InputInjectorWayland::Core::InjectKeyPress(unsigned int code,
                                                bool pressed,
                                                bool is_code) {
  DCHECK(input_task_runner_->BelongsToCurrentThread());

  remotedesktop_portal_.InjectKeyPress(code, pressed, is_code);
}

void InputInjectorWayland::Core::Start(
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {
  if (!input_task_runner_->BelongsToCurrentThread()) {
    input_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&Core::Start, this, std::move(client_clipboard)));
    return;
  }
  clipboard_->Start(std::move(client_clipboard));
}

void InputInjectorWayland::Core::Shutdown() {
  if (!input_task_runner_->BelongsToCurrentThread()) {
    input_task_runner_->PostTask(FROM_HERE,
                                 base::BindOnce(&Core::Shutdown, this));
    return;
  }

  seat_has_keyboard_capability_ = false;
  seat_has_pointer_capability_ = false;
  clipboard_initialized_ = false;
  current_state_ = State::STOPPED;
  remotedesktop_portal_.Shutdown();
}

}  // namespace remoting
