// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_injector.h"

#include <gtk/gtk.h>
#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/containers/queue.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/linux/wayland/scoped_glib.h"
#include "third_party/webrtc/modules/desktop_capture/linux/wayland/xdg_desktop_portal_utils.h"
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

// A class to generate events on wayland.
class InputInjectorWayland : public InputInjector {
 public:
  explicit InputInjectorWayland(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  InputInjectorWayland(const InputInjectorWayland&) = delete;
  InputInjectorWayland& operator=(const InputInjectorWayland&) = delete;

  ~InputInjectorWayland() override;

  // Clipboard stub interface.
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;

  // InputStub interface.
  void InjectKeyEvent(const protocol::KeyEvent& event) override;
  void InjectTextEvent(const protocol::TextEvent& event) override;
  void InjectMouseEvent(const protocol::MouseEvent& event) override;
  void InjectTouchEvent(const protocol::TouchEvent& event) override;

  void SetMetadata(InputInjectorMetadata metadata) override;

  // InputInjector interface.
  void Start(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard) override;

 private:
  // The actual implementation resides in InputInjectorWayland::Core class.
  class Core : public base::RefCountedThreadSafe<Core> {
   public:
    explicit Core(scoped_refptr<base::SingleThreadTaskRunner> task_runner);

    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;

    // Mirrors the ClipboardStub interface.
    void InjectClipboardEvent(const protocol::ClipboardEvent& event);

    // Mirrors the InputStub interface.
    void InjectKeyEvent(const protocol::KeyEvent& event);
    void InjectTextEvent(const protocol::TextEvent& event);
    void InjectMouseEvent(const protocol::MouseEvent& event);

    void SetRemoteDesktopSessionDetails(const SessionDetails& session_details);

    void SetClipboardSessionDetails(const SessionDetails& session_details);

    // Mirrors the InputInjector interface.
    void Start(std::unique_ptr<protocol::ClipboardStub> client_clipboard);

   private:
    friend class base::RefCountedThreadSafe<Core>;
    virtual ~Core();

    void InjectScrollWheelClicks(int button, int count);

    void InjectMouseButton(unsigned int code, bool pressed);
    void InjectMouseScroll(unsigned int axis, int steps);
    void MovePointerTo(int x, int y);
    void MovePointerBy(int delta_x, int delta_y);
    void InjectKeyPress(unsigned int code, bool pressed, bool is_code = true);

    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;

    std::set<int> pressed_keys_;
    absl::optional<webrtc::DesktopVector> latest_mouse_position_;
    float wheel_ticks_x_ = 0;
    float wheel_ticks_y_ = 0;
    base::TimeTicks latest_tick_y_event_;

    // The direction of the last scroll event that resulted in at least one
    // "tick" being injected.
    ScrollDirection latest_tick_y_direction_ = ScrollDirection::NONE;

#if BUILDFLAG(IS_CHROMEOS_ASH)
    PointTransformer point_transformer_;
#endif
    ClipboardWayland clipboard_;
    xdg_portal::RemoteDesktopPortalInjector remotedesktop_portal_;

    // If input is injected before complete initialization then some portal
    // APIs can crash. This flag is marked to track initialization,
    // and all inputs before the initialization is complete are added to
    // |pending_tasks| queue and injected upon initialization.
    bool remote_desktop_initialized_ = false;
    base::queue<base::OnceClosure> pending_remote_desktop_tasks_;

    // Similar to remote_desktop_initialized_, we keep the last clipboard event
    // but separated so that the remote desktop isn't blocked waiting for the
    // clipboard.
    bool clipboard_initialized_ = false;
    absl::optional<ClipboardEvent> pending_clipboard_event_;
  };

  scoped_refptr<Core> core_;
};

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
}

InputInjectorWayland::~InputInjectorWayland() {}

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
    : input_task_runner_(input_task_runner) {}

void InputInjectorWayland::Core::InjectClipboardEvent(
    const ClipboardEvent& event) {
  if (!input_task_runner_->BelongsToCurrentThread()) {
    input_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::InjectClipboardEvent, this, event));
    return;
  }
  DCHECK(input_task_runner_->BelongsToCurrentThread());
  if (!clipboard_initialized_) {
    pending_clipboard_event_ = absl::make_optional(event);
    return;
  }

  clipboard_.InjectClipboardEvent(event);
}

void InputInjectorWayland::Core::InjectKeyEvent(const KeyEvent& event) {
  if (!input_task_runner_->BelongsToCurrentThread()) {
    input_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::InjectKeyEvent, this, event));
    return;
  }
  DCHECK(input_task_runner_->BelongsToCurrentThread());
  if (!remote_desktop_initialized_) {
    pending_remote_desktop_tasks_.push(
        base::BindOnce(&Core::InjectKeyEvent, this, event));
    return;
  }
  // HostEventDispatcher should filter events missing the pressed field.
  if (!event.has_pressed() || !event.has_usb_keycode())
    return;

  int keycode =
      ui::KeycodeConverter::UsbKeycodeToNativeKeycode(event.usb_keycode()) -
      MIN_KEYCODE;

  // Ignore events which can't be mapped.
  if (keycode == ui::KeycodeConverter::InvalidNativeKeycode()) {
    LOG(ERROR) << __func__ << " : Invalid key code: " << keycode;
    return;
  }

  if (event.pressed()) {
    if (pressed_keys_.find(keycode) != pressed_keys_.end()) {
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
}

void InputInjectorWayland::Core::InjectScrollWheelClicks(int button,
                                                         int count) {
  DCHECK(input_task_runner_->BelongsToCurrentThread());

  if (button < 0) {
    LOG(WARNING) << __func__ << " : Ignoring unmapped scroll wheel button";
    return;
  }
  for (int i = 0; i < count; i++) {
    // Generate a button-down and a button-up to simulate a wheel click.
    InjectMouseButton(button, /*pressed=*/true);
    InjectMouseButton(button, /*pressed=*/false);
  }
}

void InputInjectorWayland::Core::InjectMouseEvent(const MouseEvent& event) {
  if (!input_task_runner_->BelongsToCurrentThread()) {
    input_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::InjectMouseEvent, this, event));
    return;
  }
  DCHECK(input_task_runner_->BelongsToCurrentThread());
  if (!remote_desktop_initialized_) {
    pending_remote_desktop_tasks_.push(
        base::BindOnce(&Core::InjectMouseEvent, this, event));
    return;
  }

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
    // TODO(crbug.com/439287): Remove this when Chrome OS has completely
    // migrated to Ozone.
    gfx::PointF screen_location = point_transformer_.ToScreenCoordinates(
        gfx::PointF(event.x(), event.y()));
    new_mouse_position.set(screen_location.x(), screen_location.y());
#endif
    if (event.has_button() && event.has_button_down() && !event.button_down()) {
      if (latest_mouse_position_ &&
          new_mouse_position.equals(*latest_mouse_position_))
        inject_motion = false;
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

void InputInjectorWayland::Core::SetRemoteDesktopSessionDetails(
    const SessionDetails& session_details) {
  if (!input_task_runner_->BelongsToCurrentThread()) {
    input_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::SetRemoteDesktopSessionDetails, this,
                                  session_details));
    return;
  }
  DCHECK(input_task_runner_->BelongsToCurrentThread());
  remotedesktop_portal_.SetSessionDetails(session_details);
  remote_desktop_initialized_ = true;

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

  DCHECK(input_task_runner_->BelongsToCurrentThread());
  clipboard_.SetSessionDetails(session_details);
  clipboard_initialized_ = true;

  // rerun the last pending clipboard task
  if (pending_clipboard_event_.has_value()) {
    clipboard_.InjectClipboardEvent(pending_clipboard_event_.value());
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
  DCHECK(input_task_runner_->BelongsToCurrentThread());
  clipboard_.Start(std::move(client_clipboard));
}

}  // namespace

// static
std::unique_ptr<InputInjector> InputInjector::Create(
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  return std::make_unique<InputInjectorWayland>(input_task_runner);
}

// static
bool InputInjector::SupportsTouchEvents() {
  return false;
}

}  // namespace remoting
