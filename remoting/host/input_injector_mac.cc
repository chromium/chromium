// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_injector.h"

#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "remoting/host/clipboard.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/message_decoder.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mac/desktop_configuration.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace remoting {

namespace {

void SetOrClearBit(uint64_t &value, uint64_t bit, bool set_bit) {
  value = set_bit ? (value | bit) : (value & ~bit);
}

void CreateAndPostKeyEvent(int keycode,
                           bool pressed,
                           uint64_t flags,
                           const base::string16& unicode) {
  base::ScopedCFTypeRef<CGEventRef> eventRef(
      CGEventCreateKeyboardEvent(nullptr, keycode, pressed));
  if (eventRef) {
    CGEventSetFlags(eventRef, static_cast<CGEventFlags>(flags));
    if (!unicode.empty())
      CGEventKeyboardSetUnicodeString(eventRef, unicode.size(), &(unicode[0]));
    CGEventPost(kCGSessionEventTap, eventRef);
  }
}

// This value is not defined. Give it the obvious name so that if it is ever
// added there will be a handy compilation error to remind us to remove this
// definition.
const int kVK_RightCommand = 0x36;

// Determines the minimum amount of time between attempts to waken the display
// in response to an input event.
const int kWakeUpDisplayIntervalMs = 1000;

using protocol::ClipboardEvent;
using protocol::KeyEvent;
using protocol::TextEvent;
using protocol::MouseEvent;
using protocol::TouchEvent;

// A class to generate events on Mac.
class InputInjectorMac : public InputInjector {
 public:
  explicit InputInjectorMac(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~InputInjectorMac() override;

  // ClipboardStub interface.
  void InjectClipboardEvent(const ClipboardEvent& event) override;

  // InputStub interface.
  void InjectKeyEvent(const KeyEvent& event) override;
  void InjectTextEvent(const TextEvent& event) override;
  void InjectMouseEvent(const MouseEvent& event) override;
  void InjectTouchEvent(const TouchEvent& event) override;

  // InputInjector interface.
  void Start(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard) override;

 private:
  // The actual implementation resides in InputInjectorMac::Core class.
  class Core : public base::RefCountedThreadSafe<Core> {
   public:
    explicit Core(scoped_refptr<base::SingleThreadTaskRunner> task_runner);

    // Mirrors the ClipboardStub interface.
    void InjectClipboardEvent(const ClipboardEvent& event);

    // Mirrors the InputStub interface.
    void InjectKeyEvent(const KeyEvent& event);
    void InjectTextEvent(const TextEvent& event);
    void InjectMouseEvent(const MouseEvent& event);

    // Mirrors the InputInjector interface.
    void Start(std::unique_ptr<protocol::ClipboardStub> client_clipboard);

    void Stop();

   private:
    friend class base::RefCountedThreadSafe<Core>;
    virtual ~Core();

    void WakeUpDisplay();

    scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
    webrtc::DesktopVector mouse_pos_;
    uint32_t mouse_button_state_;
    std::unique_ptr<Clipboard> clipboard_;
    uint64_t left_modifiers_;
    uint64_t right_modifiers_;
    base::TimeTicks last_time_display_woken_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(InputInjectorMac);
};

InputInjectorMac::InputInjectorMac(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  core_ = new Core(task_runner);
}

InputInjectorMac::~InputInjectorMac() {
  core_->Stop();
}

void InputInjectorMac::InjectClipboardEvent(const ClipboardEvent& event) {
  core_->InjectClipboardEvent(event);
}

void InputInjectorMac::InjectKeyEvent(const KeyEvent& event) {
  core_->InjectKeyEvent(event);
}

void InputInjectorMac::InjectTextEvent(const TextEvent& event) {
  core_->InjectTextEvent(event);
}

void InputInjectorMac::InjectMouseEvent(const MouseEvent& event) {
  core_->InjectMouseEvent(event);
}

void InputInjectorMac::InjectTouchEvent(const TouchEvent& event) {
  NOTIMPLEMENTED() << "Raw touch event injection not implemented for Mac.";
}

void InputInjectorMac::Start(
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {
  core_->Start(std::move(client_clipboard));
}

InputInjectorMac::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner),
      mouse_button_state_(0),
      clipboard_(Clipboard::Create()),
      left_modifiers_(0),
      right_modifiers_(0) {
  // Ensure that local hardware events are not suppressed after injecting
  // input events.  This allows LocalInputMonitor to detect if the local mouse
  // is being moved whilst a remote user is connected.
  // This API is deprecated, but it is needed when using the deprecated
  // injection APIs.
  // If the non-deprecated injection APIs were used instead, the equivalent of
  // this line would not be needed, as OS X defaults to _not_ suppressing local
  // inputs in that case.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  CGSetLocalEventsSuppressionInterval(0.0);
#pragma clang diagnostic pop
}

void InputInjectorMac::Core::InjectClipboardEvent(const ClipboardEvent& event) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::InjectClipboardEvent, this, event));
    return;
  }

  // |clipboard_| will ignore unknown MIME-types, and verify the data's format.
  clipboard_->InjectClipboardEvent(event);
}

void InputInjectorMac::Core::InjectKeyEvent(const KeyEvent& event) {
  // HostEventDispatcher should filter events missing the pressed field.
  if (!event.has_pressed() || !event.has_usb_keycode())
    return;

  WakeUpDisplay();

  int keycode =
      ui::KeycodeConverter::UsbKeycodeToNativeKeycode(event.usb_keycode());

  VLOG(3) << "Converting USB keycode: " << std::hex << event.usb_keycode()
          << " to keycode: " << keycode << std::dec;

  // If we couldn't determine the Mac virtual key code then ignore the event.
  if (keycode == ui::KeycodeConverter::InvalidNativeKeycode())
    return;

  // If this is a modifier key, remember its new state so that it can be
  // correctly applied to subsequent events.
  if (keycode == kVK_Command) {
    SetOrClearBit(left_modifiers_, kCGEventFlagMaskCommand, event.pressed());
  } else if (keycode == kVK_Shift) {
    SetOrClearBit(left_modifiers_, kCGEventFlagMaskShift, event.pressed());
  } else if (keycode == kVK_Control) {
    SetOrClearBit(left_modifiers_, kCGEventFlagMaskControl, event.pressed());
  } else if (keycode == kVK_Option) {
    SetOrClearBit(left_modifiers_, kCGEventFlagMaskAlternate, event.pressed());
  } else if (keycode == kVK_RightCommand) {
    SetOrClearBit(right_modifiers_, kCGEventFlagMaskCommand, event.pressed());
  } else if (keycode == kVK_RightShift) {
    SetOrClearBit(right_modifiers_, kCGEventFlagMaskShift, event.pressed());
  } else if (keycode == kVK_RightControl) {
    SetOrClearBit(right_modifiers_, kCGEventFlagMaskControl, event.pressed());
  } else if (keycode == kVK_RightOption) {
    SetOrClearBit(right_modifiers_, kCGEventFlagMaskAlternate, event.pressed());
  }

  // In addition to the modifier keys pressed right now, we also need to set
  // AlphaShift if caps lock was active at the client (Mac ignores NumLock).
  uint64_t flags = left_modifiers_ | right_modifiers_;
  if ((event.has_caps_lock_state() && event.caps_lock_state()) ||
      (event.has_lock_states() &&
       (event.lock_states() & protocol::KeyEvent::LOCK_STATES_CAPSLOCK) != 0)) {
    flags |= kCGEventFlagMaskAlphaShift;
  }

  CreateAndPostKeyEvent(keycode, event.pressed(), flags, base::string16());
}

void InputInjectorMac::Core::InjectTextEvent(const TextEvent& event) {
  DCHECK(event.has_text());

  WakeUpDisplay();

  base::string16 text = base::UTF8ToUTF16(event.text());

  // Applications that ignore UnicodeString field will see the text event as
  // Space key.
  CreateAndPostKeyEvent(kVK_Space, /*pressed=*/true, 0, text);
  CreateAndPostKeyEvent(kVK_Space, /*pressed=*/false, 0, text);
}

void InputInjectorMac::Core::InjectMouseEvent(const MouseEvent& event) {
  WakeUpDisplay();

  if (event.has_x() && event.has_y()) {
    // On multi-monitor systems (0,0) refers to the top-left of the "main"
    // display, whereas our coordinate scheme places (0,0) at the top-left of
    // the bounding rectangle around all the displays, so we need to translate
    // accordingly.

    // Set the mouse position assuming single-monitor.
    mouse_pos_.set(event.x(), event.y());

    // Fetch the desktop configuration.
    // TODO(wez): Optimize this out, or at least only enumerate displays in
    // response to display-changed events. VideoFrameCapturer's VideoFrames
    // could be augmented to include native cursor coordinates for use by
    // MouseClampingFilter, removing the need for translation here.
    webrtc::MacDesktopConfiguration desktop_config =
        webrtc::MacDesktopConfiguration::GetCurrent(
            webrtc::MacDesktopConfiguration::TopLeftOrigin);

    // Translate the mouse position into desktop coordinates.
    mouse_pos_ = mouse_pos_.add(
        webrtc::DesktopVector(desktop_config.pixel_bounds.left(),
                              desktop_config.pixel_bounds.top()));

    // Constrain the mouse position to the desktop coordinates.
    mouse_pos_.set(
       std::max(desktop_config.pixel_bounds.left(),
           std::min(desktop_config.pixel_bounds.right(), mouse_pos_.x())),
       std::max(desktop_config.pixel_bounds.top(),
           std::min(desktop_config.pixel_bounds.bottom(), mouse_pos_.y())));

    // Convert from pixel to Density Independent Pixel coordinates.
    mouse_pos_.set(mouse_pos_.x() / desktop_config.dip_to_pixel_scale,
                   mouse_pos_.y() / desktop_config.dip_to_pixel_scale);

    VLOG(3) << "Moving mouse to " << mouse_pos_.x() << "," << mouse_pos_.y();
  }
  if (event.has_button() && event.has_button_down()) {
    if (event.button() >= 1 && event.button() <= 3) {
      VLOG(2) << "Button " << event.button()
              << (event.button_down() ? " down" : " up");
      int button_change = 1 << (event.button() - 1);
      if (event.button_down())
        mouse_button_state_ |= button_change;
      else
        mouse_button_state_ &= ~button_change;
    } else {
      VLOG(1) << "Unknown mouse button: " << event.button();
    }
  }
  // We use the deprecated CGPostMouseEvent API because we receive low-level
  // mouse events, whereas CGEventCreateMouseEvent is for injecting higher-level
  // events. For example, the deprecated APIs will detect double-clicks or drags
  // in a way that is consistent with how they would be generated using a local
  // mouse, whereas the new APIs expect us to inject these higher-level events
  // directly.
  //
  // See crbug.com/677857 for more details.
  CGPoint position = CGPointMake(mouse_pos_.x(), mouse_pos_.y());
  enum {
    LeftBit = 1 << (MouseEvent::BUTTON_LEFT - 1),
    MiddleBit = 1 << (MouseEvent::BUTTON_MIDDLE - 1),
    RightBit = 1 << (MouseEvent::BUTTON_RIGHT - 1)
  };
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  CGError error = CGPostMouseEvent(position, true, 3,
                                   (mouse_button_state_ & LeftBit) != 0,
                                   (mouse_button_state_ & RightBit) != 0,
                                   (mouse_button_state_ & MiddleBit) != 0);
#pragma clang diagnostic pop
  if (error != kCGErrorSuccess)
    LOG(WARNING) << "CGPostMouseEvent error " << error;

  if (event.has_wheel_delta_x() && event.has_wheel_delta_y()) {
    int delta_x = static_cast<int>(event.wheel_delta_x());
    int delta_y = static_cast<int>(event.wheel_delta_y());
    base::ScopedCFTypeRef<CGEventRef> event(CGEventCreateScrollWheelEvent(
        nullptr, kCGScrollEventUnitPixel, 2, delta_y, delta_x));
    if (event)
      CGEventPost(kCGSessionEventTap, event);
  }
}

void InputInjectorMac::Core::Start(
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&Core::Start, this, std::move(client_clipboard)));
    return;
  }

  clipboard_->Start(std::move(client_clipboard));
}

void InputInjectorMac::Core::Stop() {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(FROM_HERE, base::BindOnce(&Core::Stop, this));
    return;
  }

  clipboard_.reset();
}

void InputInjectorMac::Core::WakeUpDisplay() {
  base::TimeTicks now = base::TimeTicks::Now();
  if (now - last_time_display_woken_ <
      base::TimeDelta::FromMilliseconds(kWakeUpDisplayIntervalMs)) {
    return;
  }

  last_time_display_woken_ = now;

  // TODO(dcaiafa): Consolidate power management with webrtc::ScreenCapturer
  // (crbug.com/535769)

  // Normally one would want to create a power assertion and hold it for the
  // duration of the session. An active power assertion prevents the display
  // from going to sleep automatically, but it doesn't prevent the user from
  // forcing it to sleep (e.g. by going to a hot corner). The display is only
  // re-awaken at the moment the assertion is created.
  IOPMAssertionID power_assertion_id = kIOPMNullAssertionID;
  IOReturn result = IOPMAssertionCreateWithName(
      CFSTR("UserIsActive"),
      kIOPMAssertionLevelOn,
      CFSTR("Chrome Remote Desktop connection active"),
      &power_assertion_id);
  if (result == kIOReturnSuccess) {
    IOPMAssertionRelease(power_assertion_id);
  }
}

InputInjectorMac::Core::~Core() {}

}  // namespace

// static
std::unique_ptr<InputInjector> InputInjector::Create(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  return base::WrapUnique(new InputInjectorMac(main_task_runner));
}

// static
bool InputInjector::SupportsTouchEvents() {
  return false;
}

}  // namespace remoting
