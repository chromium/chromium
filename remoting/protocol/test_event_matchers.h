// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_TEST_EVENT_MATCHERS_H_
#define REMOTING_PROTOCOL_TEST_EVENT_MATCHERS_H_

#include <stdint.h>

#include <cmath>

#include "remoting/proto/event.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

// This file contains matchers for protocol events.
namespace remoting::protocol::test {

MATCHER_P2(EqualsKeyEvent, usb_keycode, pressed, "") {
  return arg.usb_keycode() == static_cast<uint32_t>(usb_keycode) &&
         arg.pressed() == pressed;
}

MATCHER_P2(EqualsKeyEventWithCapsLock, usb_keycode, pressed, "") {
  return arg.usb_keycode() == static_cast<uint32_t>(usb_keycode) &&
         arg.pressed() == pressed &&
         arg.lock_states() == KeyEvent::LOCK_STATES_CAPSLOCK;
}

MATCHER_P2(EqualsKeyEventWithNumLock, usb_keycode, pressed, "") {
  return arg.usb_keycode() == static_cast<uint32_t>(usb_keycode) &&
         arg.pressed() == pressed &&
         arg.lock_states() == KeyEvent::LOCK_STATES_NUMLOCK;
}

MATCHER_P2(EqualsKeyEventWithoutLockStates, usb_keycode, pressed, "") {
  return arg.usb_keycode() == static_cast<uint32_t>(usb_keycode) &&
         arg.pressed() == pressed && !arg.has_lock_states();
}

MATCHER_P(EqualsTextEvent, text, "") {
  return arg.text() == text;
}

MATCHER_P2(EqualsMouseMoveEvent, x, y, "") {
  return arg.x() == x && arg.y() == y;
}

MATCHER_P2(EqualsMouseButtonEvent, button, button_down, "") {
  return arg.button() == button && arg.button_down() == button_down;
}

MATCHER_P4(EqualsMouseEvent, x, y, button, down, "") {
  return arg.x() == x && arg.y() == y && arg.button() == button &&
         arg.button_down() == down;
}

MATCHER_P2(EqualsClipboardEvent, mime_type, data, "") {
  return arg.mime_type() == mime_type && arg.data() == data;
}

MATCHER_P(EqualsTouchEvent, expected_event, "") {
  if (arg.event_type() != expected_event.event_type()) {
    return false;
  }

  if (arg.touch_points().size() != expected_event.touch_points().size()) {
    return false;
  }

  for (int i = 0; i < expected_event.touch_points().size(); ++i) {
    const TouchEventPoint& expected_point = expected_event.touch_points(i);
    const TouchEventPoint& actual_point = arg.touch_points(i);

    const bool equal = expected_point.id() == actual_point.id() &&
                       expected_point.x() == actual_point.x() &&
                       expected_point.y() == actual_point.y() &&
                       expected_point.radius_x() == actual_point.radius_x() &&
                       expected_point.radius_y() == actual_point.radius_y() &&
                       expected_point.angle() == actual_point.angle() &&
                       expected_point.pressure() == actual_point.pressure();
    if (!equal) {
      return false;
    }
  }

  return true;
}

// If the rounding error for the coordinates checked in TouchPoint* matcher are
// within 1 pixel diff, it is acceptable.
const float kTestTouchErrorEpsilon = 1.0f;

MATCHER_P(EqualsTouchPointCoordinates, expected_event, "") {
  if (arg.touch_points().size() != expected_event.touch_points().size()) {
    return false;
  }

  for (int i = 0; i < expected_event.touch_points().size(); ++i) {
    const TouchEventPoint& arg_point = arg.touch_points(i);
    const TouchEventPoint& expected_point = expected_event.touch_points(i);
    if (std::abs(expected_point.x() - arg_point.x()) >=
        kTestTouchErrorEpsilon) {
      return false;
    }

    if (std::abs(expected_point.y() - arg_point.y()) >=
        kTestTouchErrorEpsilon) {
      return false;
    }
  }
  return true;
}

MATCHER_P(EqualsTouchPointRadii, expected_event, "") {
  if (arg.touch_points().size() != expected_event.touch_points().size()) {
    return false;
  }

  for (int i = 0; i < expected_event.touch_points().size(); ++i) {
    const TouchEventPoint& arg_point = arg.touch_points(i);
    const TouchEventPoint& expected_point = expected_event.touch_points(i);
    if (std::abs(expected_point.radius_x() - arg_point.radius_x()) >=
        kTestTouchErrorEpsilon) {
      return false;
    }

    if (std::abs(expected_point.radius_y() - arg_point.radius_y()) >=
        kTestTouchErrorEpsilon) {
      return false;
    }
  }
  return true;
}

MATCHER_P2(EqualsTouchEventTypeAndId, type, id, "") {
  if (arg.event_type() != type) {
    return false;
  }

  if (arg.touch_points().size() != 1) {
    return false;
  }

  return arg.touch_points(0).id() == id;
}

}  // namespace remoting::protocol::test

#endif  // REMOTING_PROTOCOL_TEST_EVENT_MATCHERS_H_
