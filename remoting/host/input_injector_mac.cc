// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_injector_mac.h"

#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <stdint.h>

#include <algorithm>
#include <string_view>
#include <utility>

#include "base/apple/scoped_cftyperef.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/i18n/break_iterator.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notimplemented.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "remoting/host/clipboard.h"
#include "remoting/proto/internal.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mac/desktop_configuration.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace remoting {

namespace {

void SetOrClearBit(uint64_t& value, uint64_t bit, bool set_bit) {
  value = set_bit ? (value | bit) : (value & ~bit);
}

// Must be called on UI thread.
void CreateAndPostKeyEvent(int keycode,
                           bool pressed,
                           uint64_t flags,
                           const std::u16string& unicode) {
  base::apple::ScopedCFTypeRef<CGEventRef> eventRef(
      CGEventCreateKeyboardEvent(nullptr, keycode, pressed));
  if (eventRef) {
    CGEventSetFlags(eventRef.get(), static_cast<CGEventFlags>(flags));
    if (!unicode.empty()) {
      CGEventKeyboardSetUnicodeString(
          eventRef.get(), unicode.size(),
          reinterpret_cast<const UniChar*>(unicode.data()));
    }
    VLOG(3) << "Injecting key " << (pressed ? "down" : "up") << " event.";
    CGEventPost(kCGSessionEventTap, eventRef.get());
  }
}

// Must be called on UI thread.
void PostMouseEvent(int32_t x,
                    int32_t y,
                    bool left_down,
                    bool right_down,
                    bool middle_down) {
  // We use the deprecated CGPostMouseEvent API because we receive low-level
  // mouse events, whereas CGEventCreateMouseEvent is for injecting higher-level
  // events. For example, the deprecated APIs will detect double-clicks or drags
  // in a way that is consistent with how they would be generated using a local
  // mouse, whereas the new APIs expect us to inject these higher-level events
  // directly.
  //
  // See crbug.com/677857 for more details.
  CGPoint position = CGPointMake(x, y);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  CGError error =
      CGPostMouseEvent(position, true, 3, left_down, right_down, middle_down);
#pragma clang diagnostic pop
  if (error != kCGErrorSuccess) {
    LOG(WARNING) << "CGPostMouseEvent error " << error;
  }
}

// Must be called on UI thread.
void CreateAndPostScrollWheelEvent(int32_t delta_x, int32_t delta_y) {
  base::apple::ScopedCFTypeRef<CGEventRef> eventRef(
      CGEventCreateScrollWheelEvent(nullptr, kCGScrollEventUnitPixel, 2,
                                    delta_y, delta_x));
  if (eventRef) {
    CGEventPost(kCGSessionEventTap, eventRef.get());
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
using protocol::MouseEvent;
using protocol::TextEvent;
using protocol::TouchEvent;

}  // namespace

InputInjectorMac::InputInjectorMac(
    scoped_refptr<base::SingleThreadTaskRunner> input_thread_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner)
    : ui_thread_task_runner_(std::move(ui_thread_task_runner)),
      clipboard_(std::move(input_thread_task_runner), Clipboard::Create()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

InputInjectorMac::~InputInjectorMac() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void InputInjectorMac::InjectClipboardEvent(const ClipboardEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // |clipboard_| will ignore unknown MIME-types, and verify the data's format.
  clipboard_.AsyncCall(&Clipboard::InjectClipboardEvent).WithArgs(event);
}

void InputInjectorMac::InjectKeyEvent(const KeyEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // HostEventDispatcher should filter events missing the pressed field.
  if (!event.has_pressed() || !event.has_usb_keycode()) {
    return;
  }

  WakeUpDisplay();

  int keycode =
      ui::KeycodeConverter::UsbKeycodeToNativeKeycode(event.usb_keycode());

  // If we couldn't determine the Mac virtual key code then ignore the event.
  if (keycode == ui::KeycodeConverter::InvalidNativeKeycode()) {
    return;
  }

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

  ui_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(CreateAndPostKeyEvent, keycode, event.pressed(),
                                flags, std::u16string()));
}

void InputInjectorMac::InjectTextEvent(const TextEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(event.has_text());

  WakeUpDisplay();

  std::u16string text = base::UTF8ToUTF16(event.text());

  // CGEventKeyboardSetUnicodeString appears to only process up to 20 code
  // units (and key presses are generally expected to generate a single
  // character), so split the input text into graphemes.
  base::i18n::BreakIterator grapheme_iterator(
      text, base::i18n::BreakIterator::BREAK_CHARACTER);

  if (!grapheme_iterator.Init()) {
    LOG(ERROR) << "Failed to init grapheme iterator.";
    return;
  }

  while (grapheme_iterator.Advance()) {
    std::u16string_view grapheme = grapheme_iterator.GetString();

    if (grapheme.length() == 1 && grapheme[0] == '\n') {
      // On Mac, the return key sends "\r" rather than "\n", so handle it
      // specially.
      ui_thread_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(CreateAndPostKeyEvent, kVK_Return,
                                    /*pressed=*/true, 0, std::u16string()));
      ui_thread_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(CreateAndPostKeyEvent, kVK_Return,
                                    /*pressed=*/false, 0, std::u16string()));
    } else {
      // Applications that ignore UnicodeString field will see the text event as
      // Space key.
      ui_thread_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(CreateAndPostKeyEvent, kVK_Space,
                         /*pressed=*/true, 0, std::u16string(grapheme)));
      ui_thread_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(CreateAndPostKeyEvent, kVK_Space,
                         /*pressed=*/false, 0, std::u16string(grapheme)));
    }
  }
}

void InputInjectorMac::InjectMouseEvent(const MouseEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  WakeUpDisplay();

  if (event.has_x() && event.has_y()) {
    mouse_pos_.set(event.x(), event.y());
    VLOG(3) << "Moving mouse to " << mouse_pos_.x() << "," << mouse_pos_.y();
  }
  if (event.has_button() && event.has_button_down()) {
    if (event.button() >= 1 && event.button() <= 3) {
      VLOG(2) << "Button " << event.button()
              << (event.button_down() ? " down" : " up");
      int button_change = 1 << (event.button() - 1);
      if (event.button_down()) {
        mouse_button_state_ |= button_change;
      } else {
        mouse_button_state_ &= ~button_change;
      }
    } else {
      VLOG(1) << "Unknown mouse button: " << event.button();
    }
  }
  enum {
    LeftBit = 1 << (MouseEvent::BUTTON_LEFT - 1),
    MiddleBit = 1 << (MouseEvent::BUTTON_MIDDLE - 1),
    RightBit = 1 << (MouseEvent::BUTTON_RIGHT - 1)
  };
  ui_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(PostMouseEvent, mouse_pos_.x(), mouse_pos_.y(),
                                (mouse_button_state_ & LeftBit) != 0,
                                (mouse_button_state_ & RightBit) != 0,
                                (mouse_button_state_ & MiddleBit) != 0));

  if (event.has_wheel_delta_x() && event.has_wheel_delta_y()) {
    ui_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(CreateAndPostScrollWheelEvent, event.wheel_delta_x(),
                       event.wheel_delta_y()));
  }
}

void InputInjectorMac::InjectTouchEvent(const TouchEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED() << "Raw touch event injection not implemented for Mac.";
}

void InputInjectorMac::Start(
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  clipboard_.AsyncCall(&Clipboard::Start).WithArgs(std::move(client_clipboard));
}

void InputInjectorMac::WakeUpDisplay() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::TimeTicks now = base::TimeTicks::Now();
  if (now - last_time_display_woken_ <
      base::Milliseconds(kWakeUpDisplayIntervalMs)) {
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
      CFSTR("UserIsActive"), kIOPMAssertionLevelOn,
      CFSTR("Chrome Remote Desktop connection active"), &power_assertion_id);
  if (result == kIOReturnSuccess) {
    IOPMAssertionRelease(power_assertion_id);
  }
}

// static
std::unique_ptr<InputInjector> InputInjector::Create(
    scoped_refptr<base::SingleThreadTaskRunner> input_thread_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner) {
  return base::WrapUnique(
      new InputInjectorMac(input_thread_task_runner, ui_thread_task_runner));
}

// static
bool InputInjector::SupportsTouchEvents() {
  return false;
}

}  // namespace remoting
