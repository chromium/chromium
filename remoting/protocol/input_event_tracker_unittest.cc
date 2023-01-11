// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/input_event_tracker.h"

#include <stdint.h>

#include "remoting/proto/event.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/test_event_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ExpectationSet;
using ::testing::InSequence;

namespace remoting::protocol {

using test::EqualsKeyEventWithCapsLock;
using test::EqualsKeyEventWithoutLockStates;
using test::EqualsMouseEvent;

namespace {

static const MouseEvent::MouseButton BUTTON_LEFT = MouseEvent::BUTTON_LEFT;
static const MouseEvent::MouseButton BUTTON_RIGHT = MouseEvent::BUTTON_RIGHT;

MATCHER_P2(TouchPointIdsAndTypeEqual, ids, type, "") {
  if (arg.event_type() != type) {
    return false;
  }

  std::set<uint32_t> touch_ids;
  for (const TouchEventPoint& point : arg.touch_points()) {
    touch_ids.insert(point.id());
  }
  return touch_ids == ids;
}

static KeyEvent NewUsbEvent(uint32_t usb_keycode, bool pressed) {
  KeyEvent event;
  event.set_usb_keycode(usb_keycode);
  event.set_pressed(pressed);
  // Create all key events with the hardcoded |lock_state| in this test.
  event.set_lock_states(KeyEvent::LOCK_STATES_CAPSLOCK);
  return event;
}

static void PressAndReleaseUsb(InputStub* input_stub, uint32_t usb_keycode) {
  input_stub->InjectKeyEvent(NewUsbEvent(usb_keycode, true));
  input_stub->InjectKeyEvent(NewUsbEvent(usb_keycode, false));
}

static MouseEvent NewMouseEvent(int x,
                                int y,
                                MouseEvent::MouseButton button,
                                bool down) {
  MouseEvent event;
  event.set_x(x);
  event.set_y(y);
  event.set_button(button);
  event.set_button_down(down);
  return event;
}

void AddTouchPoint(uint32_t id, TouchEvent* event) {
  TouchEventPoint* p = event->add_touch_points();
  p->set_id(id);
}

}  // namespace

// Verify that keys that were pressed and released aren't re-released.
TEST(InputEventTrackerTest, NothingToRelease) {
  MockInputStub mock_stub;
  InputEventTracker input_tracker(&mock_stub);

  {
    InSequence s;

    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEventWithCapsLock(1, true)));
    EXPECT_CALL(mock_stub,
                InjectKeyEvent(EqualsKeyEventWithCapsLock(1, false)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEventWithCapsLock(2, true)));
    EXPECT_CALL(mock_stub,
                InjectKeyEvent(EqualsKeyEventWithCapsLock(2, false)));

    EXPECT_CALL(mock_stub,
                InjectMouseEvent(EqualsMouseEvent(0, 0, BUTTON_LEFT, true)));
    EXPECT_CALL(mock_stub,
                InjectMouseEvent(EqualsMouseEvent(0, 0, BUTTON_LEFT, false)));
  }

  PressAndReleaseUsb(&input_tracker, 1);
  PressAndReleaseUsb(&input_tracker, 2);

  input_tracker.InjectMouseEvent(NewMouseEvent(0, 0, BUTTON_LEFT, true));
  input_tracker.InjectMouseEvent(NewMouseEvent(0, 0, BUTTON_LEFT, false));

  input_tracker.ReleaseAll();
}

// Verify that keys that were left pressed get released.
TEST(InputEventTrackerTest, ReleaseAllKeys) {
  MockInputStub mock_stub;
  InputEventTracker input_tracker(&mock_stub);
  ExpectationSet injects;

  {
    InSequence s;

    injects += EXPECT_CALL(mock_stub,
                           InjectKeyEvent(EqualsKeyEventWithCapsLock(3, true)));
    injects += EXPECT_CALL(mock_stub,
                           InjectKeyEvent(EqualsKeyEventWithCapsLock(1, true)));
    injects += EXPECT_CALL(
        mock_stub, InjectKeyEvent(EqualsKeyEventWithCapsLock(1, false)));
    injects += EXPECT_CALL(mock_stub,
                           InjectKeyEvent(EqualsKeyEventWithCapsLock(2, true)));
    injects += EXPECT_CALL(
        mock_stub, InjectKeyEvent(EqualsKeyEventWithCapsLock(2, false)));

    injects += EXPECT_CALL(mock_stub, InjectMouseEvent(EqualsMouseEvent(
                                          0, 0, BUTTON_RIGHT, true)));
    injects += EXPECT_CALL(
        mock_stub, InjectMouseEvent(EqualsMouseEvent(0, 0, BUTTON_LEFT, true)));
    injects += EXPECT_CALL(mock_stub, InjectMouseEvent(EqualsMouseEvent(
                                          1, 1, BUTTON_LEFT, false)));
  }

  // The key should be released but |lock_states| should not be set.
  EXPECT_CALL(mock_stub,
              InjectKeyEvent(EqualsKeyEventWithoutLockStates(3, false)))
      .After(injects);
  EXPECT_CALL(mock_stub,
              InjectMouseEvent(EqualsMouseEvent(1, 1, BUTTON_RIGHT, false)))
      .After(injects);

  input_tracker.InjectKeyEvent(NewUsbEvent(3, true));
  PressAndReleaseUsb(&input_tracker, 1);
  PressAndReleaseUsb(&input_tracker, 2);

  input_tracker.InjectMouseEvent(NewMouseEvent(0, 0, BUTTON_RIGHT, true));
  input_tracker.InjectMouseEvent(NewMouseEvent(0, 0, BUTTON_LEFT, true));
  input_tracker.InjectMouseEvent(NewMouseEvent(1, 1, BUTTON_LEFT, false));

  EXPECT_FALSE(input_tracker.IsKeyPressed(static_cast<ui::DomCode>(1)));
  EXPECT_FALSE(input_tracker.IsKeyPressed(static_cast<ui::DomCode>(2)));
  EXPECT_TRUE(input_tracker.IsKeyPressed(static_cast<ui::DomCode>(3)));
  EXPECT_EQ(1, input_tracker.PressedKeyCount());

  input_tracker.ReleaseAll();
}

// Verify that we track both USB-based key events correctly.
TEST(InputEventTrackerTest, TrackUsbKeyEvents) {
  MockInputStub mock_stub;
  InputEventTracker input_tracker(&mock_stub);
  ExpectationSet injects;

  {
    InSequence s;

    injects += EXPECT_CALL(mock_stub,
                           InjectKeyEvent(EqualsKeyEventWithCapsLock(3, true)));
    injects += EXPECT_CALL(mock_stub,
                           InjectKeyEvent(EqualsKeyEventWithCapsLock(6, true)));
    injects += EXPECT_CALL(mock_stub,
                           InjectKeyEvent(EqualsKeyEventWithCapsLock(7, true)));
    injects += EXPECT_CALL(mock_stub,
                           InjectKeyEvent(EqualsKeyEventWithCapsLock(5, true)));
    injects += EXPECT_CALL(mock_stub,
                           InjectKeyEvent(EqualsKeyEventWithCapsLock(5, true)));
    injects += EXPECT_CALL(mock_stub,
                           InjectKeyEvent(EqualsKeyEventWithCapsLock(2, true)));
    injects += EXPECT_CALL(
        mock_stub, InjectKeyEvent(EqualsKeyEventWithCapsLock(2, false)));
  }

  // The key should be auto released with no |lock_states|.
  EXPECT_CALL(mock_stub,
              InjectKeyEvent(EqualsKeyEventWithoutLockStates(3, false)))
      .After(injects);
  EXPECT_CALL(mock_stub,
              InjectKeyEvent(EqualsKeyEventWithoutLockStates(6, false)))
      .After(injects);
  EXPECT_CALL(mock_stub,
              InjectKeyEvent(EqualsKeyEventWithoutLockStates(7, false)))
      .After(injects);
  EXPECT_CALL(mock_stub,
              InjectKeyEvent(EqualsKeyEventWithoutLockStates(5, false)))
      .After(injects);

  input_tracker.InjectKeyEvent(NewUsbEvent(3, true));
  input_tracker.InjectKeyEvent(NewUsbEvent(6, true));
  input_tracker.InjectKeyEvent(NewUsbEvent(7, true));
  input_tracker.InjectKeyEvent(NewUsbEvent(5, true));
  input_tracker.InjectKeyEvent(NewUsbEvent(5, true));
  PressAndReleaseUsb(&input_tracker, 2);

  EXPECT_FALSE(input_tracker.IsKeyPressed(static_cast<ui::DomCode>(1)));
  EXPECT_FALSE(input_tracker.IsKeyPressed(static_cast<ui::DomCode>(2)));
  EXPECT_TRUE(input_tracker.IsKeyPressed(static_cast<ui::DomCode>(3)));
  EXPECT_TRUE(input_tracker.IsKeyPressed(static_cast<ui::DomCode>(5)));
  EXPECT_TRUE(input_tracker.IsKeyPressed(static_cast<ui::DomCode>(6)));
  EXPECT_TRUE(input_tracker.IsKeyPressed(static_cast<ui::DomCode>(7)));
  EXPECT_EQ(4, input_tracker.PressedKeyCount());

  input_tracker.ReleaseAll();
}

// Verify that invalid events get passed through but not tracked.
TEST(InputEventTrackerTest, InvalidEventsNotTracked) {
  MockInputStub mock_stub;
  InputEventTracker input_tracker(&mock_stub);
  ExpectationSet injects;

  {
    InSequence s;

    injects += EXPECT_CALL(mock_stub,
                           InjectKeyEvent(EqualsKeyEventWithCapsLock(3, true)));
    injects += EXPECT_CALL(mock_stub,
                           InjectKeyEvent(EqualsKeyEventWithCapsLock(1, true)));
    injects += EXPECT_CALL(
        mock_stub, InjectKeyEvent(EqualsKeyEventWithCapsLock(1, false)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(_)).Times(2);
    injects += EXPECT_CALL(mock_stub,
                           InjectKeyEvent(EqualsKeyEventWithCapsLock(2, true)));
    injects += EXPECT_CALL(
        mock_stub, InjectKeyEvent(EqualsKeyEventWithCapsLock(2, false)));
  }

  EXPECT_CALL(mock_stub,
              InjectKeyEvent(EqualsKeyEventWithoutLockStates(3, false)))
      .After(injects);

  input_tracker.InjectKeyEvent(NewUsbEvent(3, true));
  PressAndReleaseUsb(&input_tracker, 1);

  KeyEvent invalid_event1;
  invalid_event1.set_pressed(true);
  input_tracker.InjectKeyEvent(invalid_event1);

  KeyEvent invalid_event2;
  invalid_event2.set_usb_keycode(6);
  input_tracker.InjectKeyEvent(invalid_event2);

  PressAndReleaseUsb(&input_tracker, 2);

  EXPECT_FALSE(input_tracker.IsKeyPressed(static_cast<ui::DomCode>(1)));
  EXPECT_FALSE(input_tracker.IsKeyPressed(static_cast<ui::DomCode>(2)));
  EXPECT_TRUE(input_tracker.IsKeyPressed(static_cast<ui::DomCode>(3)));
  EXPECT_EQ(1, input_tracker.PressedKeyCount());

  input_tracker.ReleaseAll();
}

// All touch points added with multiple touch events should be released as a
// cancel event.
TEST(InputEventTrackerTest, ReleaseAllTouchPoints) {
  MockInputStub mock_stub;
  InputEventTracker input_tracker(&mock_stub);

  std::set<uint32_t> expected_ids1;
  expected_ids1.insert(1);
  expected_ids1.insert(2);
  std::set<uint32_t> expected_ids2;
  expected_ids2.insert(3);
  expected_ids2.insert(5);
  expected_ids2.insert(8);

  std::set<uint32_t> all_touch_point_ids;
  all_touch_point_ids.insert(expected_ids1.begin(), expected_ids1.end());
  all_touch_point_ids.insert(expected_ids2.begin(), expected_ids2.end());

  InSequence s;
  EXPECT_CALL(mock_stub, InjectTouchEvent(TouchPointIdsAndTypeEqual(
                             expected_ids1, TouchEvent::TOUCH_POINT_START)));
  EXPECT_CALL(mock_stub, InjectTouchEvent(TouchPointIdsAndTypeEqual(
                             expected_ids2, TouchEvent::TOUCH_POINT_START)));

  EXPECT_CALL(mock_stub,
              InjectTouchEvent(TouchPointIdsAndTypeEqual(
                  all_touch_point_ids, TouchEvent::TOUCH_POINT_CANCEL)));

  TouchEvent start_event1;
  start_event1.set_event_type(TouchEvent::TOUCH_POINT_START);
  AddTouchPoint(1, &start_event1);
  AddTouchPoint(2, &start_event1);
  input_tracker.InjectTouchEvent(start_event1);

  TouchEvent start_event2;
  start_event2.set_event_type(TouchEvent::TOUCH_POINT_START);
  AddTouchPoint(3, &start_event2);
  AddTouchPoint(5, &start_event2);
  AddTouchPoint(8, &start_event2);
  input_tracker.InjectTouchEvent(start_event2);

  input_tracker.ReleaseAll();
}

// Add some touch points and remove only a subset of them. ReleaseAll() should
// cancel all the remaining touch points.
TEST(InputEventTrackerTest, ReleaseAllRemainingTouchPoints) {
  MockInputStub mock_stub;
  InputEventTracker input_tracker(&mock_stub);

  std::set<uint32_t> start_expected_ids;
  start_expected_ids.insert(1);
  start_expected_ids.insert(2);
  start_expected_ids.insert(3);

  std::set<uint32_t> end_expected_ids;
  end_expected_ids.insert(1);
  std::set<uint32_t> cancel_expected_ids;
  cancel_expected_ids.insert(3);

  std::set<uint32_t> all_remaining_touch_point_ids;
  all_remaining_touch_point_ids.insert(2);

  InSequence s;
  EXPECT_CALL(mock_stub,
              InjectTouchEvent(TouchPointIdsAndTypeEqual(
                  start_expected_ids, TouchEvent::TOUCH_POINT_START)));
  EXPECT_CALL(mock_stub, InjectTouchEvent(TouchPointIdsAndTypeEqual(
                             end_expected_ids, TouchEvent::TOUCH_POINT_END)));
  EXPECT_CALL(mock_stub,
              InjectTouchEvent(TouchPointIdsAndTypeEqual(
                  cancel_expected_ids, TouchEvent::TOUCH_POINT_CANCEL)));

  EXPECT_CALL(mock_stub, InjectTouchEvent(TouchPointIdsAndTypeEqual(
                             all_remaining_touch_point_ids,
                             TouchEvent::TOUCH_POINT_CANCEL)));

  TouchEvent start_event;
  start_event.set_event_type(TouchEvent::TOUCH_POINT_START);
  AddTouchPoint(1, &start_event);
  AddTouchPoint(2, &start_event);
  AddTouchPoint(3, &start_event);
  input_tracker.InjectTouchEvent(start_event);

  TouchEvent end_event;
  end_event.set_event_type(TouchEvent::TOUCH_POINT_END);
  AddTouchPoint(1, &end_event);
  input_tracker.InjectTouchEvent(end_event);

  TouchEvent cancel_event;
  cancel_event.set_event_type(TouchEvent::TOUCH_POINT_CANCEL);
  AddTouchPoint(3, &cancel_event);
  input_tracker.InjectTouchEvent(cancel_event);

  input_tracker.ReleaseAll();
}

}  // namespace remoting::protocol
