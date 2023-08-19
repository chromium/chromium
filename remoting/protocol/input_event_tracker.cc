// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/input_event_tracker.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "remoting/proto/event.pb.h"

namespace remoting::protocol {

InputEventTracker::InputEventTracker() = default;

InputEventTracker::InputEventTracker(InputStub* input_stub)
    : input_stub_(input_stub) {}

InputEventTracker::~InputEventTracker() = default;

bool InputEventTracker::IsKeyPressed(ui::DomCode usb_keycode) const {
  return base::Contains(pressed_keys_, usb_keycode);
}

int InputEventTracker::PressedKeyCount() const {
  return pressed_keys_.size();
}

void InputEventTracker::ReleaseAll() {
  DCHECK(input_stub_);

  // Release all pressed keys.
  for (auto keycode : pressed_keys_) {
    KeyEvent event;
    event.set_pressed(false);
    event.set_usb_keycode(static_cast<uint32_t>(keycode));
    input_stub_->InjectKeyEvent(event);
  }
  pressed_keys_.clear();

  // Release all mouse buttons.
  for (int i = MouseEvent::BUTTON_UNDEFINED + 1; i < MouseEvent::BUTTON_MAX;
       ++i) {
    if (mouse_button_state_ & (1 << (i - 1))) {
      MouseEvent mouse;

      // TODO(wez): EventInjectors should cope with positionless events by
      // using the current cursor position, and we wouldn't set position here.
      mouse.set_x(mouse_pos_.x());
      mouse.set_y(mouse_pos_.y());

      mouse.set_button((MouseEvent::MouseButton)i);
      mouse.set_button_down(false);
      input_stub_->InjectMouseEvent(mouse);
    }
  }
  mouse_button_state_ = 0;

  // Cancel all active touch points.
  if (!touch_point_ids_.empty()) {
    TouchEvent cancel_all_touch_event;
    cancel_all_touch_event.set_event_type(TouchEvent::TOUCH_POINT_CANCEL);
    for (uint32_t touch_point_id : touch_point_ids_) {
      TouchEventPoint* point = cancel_all_touch_event.add_touch_points();
      point->set_id(touch_point_id);
    }
    input_stub_->InjectTouchEvent(cancel_all_touch_event);
    touch_point_ids_.clear();
  }
  DCHECK(touch_point_ids_.empty());
}

void InputEventTracker::ReleaseAllIfModifiersStuck(bool alt_expected,
                                                   bool ctrl_expected,
                                                   bool os_expected,
                                                   bool shift_expected) {
  bool alt_down = base::Contains(pressed_keys_, ui::DomCode::ALT_LEFT) ||
                  base::Contains(pressed_keys_, ui::DomCode::ALT_RIGHT);
  bool ctrl_down = base::Contains(pressed_keys_, ui::DomCode::CONTROL_LEFT) ||
                   base::Contains(pressed_keys_, ui::DomCode::CONTROL_RIGHT);
  bool os_down = base::Contains(pressed_keys_, ui::DomCode::META_LEFT) ||
                 base::Contains(pressed_keys_, ui::DomCode::META_RIGHT);
  bool shift_down = base::Contains(pressed_keys_, ui::DomCode::SHIFT_LEFT) ||
                    base::Contains(pressed_keys_, ui::DomCode::SHIFT_RIGHT);

  if ((alt_down && !alt_expected) || (ctrl_down && !ctrl_expected) ||
      (os_down && !os_expected) || (shift_down && !shift_expected)) {
    ReleaseAll();
  }
}

void InputEventTracker::InjectKeyEvent(const KeyEvent& event) {
  DCHECK(input_stub_);

  // We don't need to track the keyboard lock states of key down events.
  // Pressed keys will be released with |lock_states| set to 0.
  // The lock states of auto generated key up events don't matter as long as
  // we release all the pressed keys at blurring/disconnection time.
  if (event.has_pressed()) {
    if (event.has_usb_keycode()) {
      if (event.pressed()) {
        pressed_keys_.insert(static_cast<ui::DomCode>(event.usb_keycode()));
      } else {
        pressed_keys_.erase(static_cast<ui::DomCode>(event.usb_keycode()));
      }
    }
  }
  input_stub_->InjectKeyEvent(event);
}

void InputEventTracker::InjectTextEvent(const TextEvent& event) {
  DCHECK(input_stub_);
  input_stub_->InjectTextEvent(event);
}

void InputEventTracker::InjectMouseEvent(const MouseEvent& event) {
  DCHECK(input_stub_);

  if (event.has_x() && event.has_y()) {
    mouse_pos_ = webrtc::DesktopVector(event.x(), event.y());
  }
  if (event.has_button() && event.has_button_down()) {
    // Button values are defined in remoting/proto/event.proto.
    if (event.button() >= 1 && event.button() < MouseEvent::BUTTON_MAX) {
      uint32_t button_change = 1 << (event.button() - 1);
      if (event.button_down()) {
        mouse_button_state_ |= button_change;
      } else {
        mouse_button_state_ &= ~button_change;
      }
    }
  }
  input_stub_->InjectMouseEvent(event);
}

void InputEventTracker::InjectTouchEvent(const TouchEvent& event) {
  DCHECK(input_stub_);
  // We only need the IDs to cancel all touch points in ReleaseAll(). Other
  // fields do not have to be tracked here as long as the host keeps track of
  // them.
  switch (event.event_type()) {
    case TouchEvent::TOUCH_POINT_START:
      for (const TouchEventPoint& touch_point : event.touch_points()) {
        DCHECK(!base::Contains(touch_point_ids_, touch_point.id()));
        touch_point_ids_.insert(touch_point.id());
      }
      break;
    case TouchEvent::TOUCH_POINT_END:
    case TouchEvent::TOUCH_POINT_CANCEL:
      for (const TouchEventPoint& touch_point : event.touch_points()) {
        DCHECK(base::Contains(touch_point_ids_, touch_point.id()));
        touch_point_ids_.erase(touch_point.id());
      }
      break;
    default:
      break;
  }
  input_stub_->InjectTouchEvent(event);
}

}  // namespace remoting::protocol
