// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_input_filter.h"

#include <stdint.h>

#include "remoting/proto/event.pb.h"
#include "remoting/protocol/input_event_tracker.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/test_event_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"

using ::testing::_;
using ::testing::ExpectationSet;
using ::testing::InSequence;

namespace remoting {

using protocol::InputEventTracker;
using protocol::MockInputStub;
using protocol::test::EqualsKeyEvent;
using protocol::test::EqualsTouchEventTypeAndId;

namespace {

static protocol::MouseEvent MouseMoveEvent(int x, int y) {
  protocol::MouseEvent event;
  event.set_x(x);
  event.set_y(y);
  return event;
}

static protocol::KeyEvent UsbKeyEvent(int usb_keycode, bool pressed) {
  protocol::KeyEvent event;
  event.set_usb_keycode(usb_keycode);
  event.set_pressed(pressed);
  return event;
}

protocol::TouchEvent TouchStartEvent(uint32_t id) {
  protocol::TouchEvent event;
  event.set_event_type(protocol::TouchEvent::TOUCH_POINT_START);

  protocol::TouchEventPoint* point = event.add_touch_points();
  point->set_id(id);
  point->set_x(0.0f);
  point->set_y(0.0f);
  point->set_radius_x(0.0f);
  point->set_radius_y(0.0f);
  point->set_angle(0.0f);
  point->set_pressure(0.0f);
  return event;
}

}  // namespace

// Verify that events get through if there is no local activity.
TEST(RemoteInputFilterTest, NoLocalActivity) {
  MockInputStub mock_stub;
  InputEventTracker input_tracker(&mock_stub);
  RemoteInputFilter input_filter(&input_tracker);

  EXPECT_CALL(mock_stub, InjectMouseEvent(_)).Times(10);

  for (int i = 0; i < 10; ++i)
    input_filter.InjectMouseEvent(MouseMoveEvent(0, 0));
}

// Verify that events get through until there is local activity.
TEST(RemoteInputFilterTest, MismatchedLocalActivity) {
  MockInputStub mock_stub;
  InputEventTracker input_tracker(&mock_stub);
  RemoteInputFilter input_filter(&input_tracker);

  EXPECT_CALL(mock_stub, InjectMouseEvent(_)).Times(5);

  for (int i = 0; i < 10; ++i) {
    input_filter.InjectMouseEvent(MouseMoveEvent(0, 0));
    if (i == 4)
      input_filter.LocalPointerMoved(webrtc::DesktopVector(1, 1),
                                     ui::ET_MOUSE_MOVED);
  }
}

// Verify that touch events are not considered as echoes.
TEST(RemoteInputFilterTest, TouchEventsAreNotCheckedForEcho) {
  MockInputStub mock_stub;
  InputEventTracker input_tracker(&mock_stub);
  RemoteInputFilter input_filter(&input_tracker);

  EXPECT_CALL(mock_stub, InjectMouseEvent(_));

  input_filter.InjectMouseEvent(MouseMoveEvent(0, 0));
  input_filter.LocalPointerMoved(webrtc::DesktopVector(0, 0),
                                 ui::ET_TOUCH_MOVED);
  input_filter.InjectMouseEvent(MouseMoveEvent(1, 1));
}

// Verify that echos of injected mouse events don't block activity.
TEST(RemoteInputFilterTest, LocalEchoesOfRemoteActivity) {
  MockInputStub mock_stub;
  InputEventTracker input_tracker(&mock_stub);
  RemoteInputFilter input_filter(&input_tracker);

  EXPECT_CALL(mock_stub, InjectMouseEvent(_)).Times(10);

  for (int i = 0; i < 10; ++i) {
    input_filter.InjectMouseEvent(MouseMoveEvent(0, 0));
    input_filter.LocalPointerMoved(webrtc::DesktopVector(0, 0),
                                   ui::ET_MOUSE_MOVED);
  }
}

// Verify that echos followed by a mismatch blocks activity.
TEST(RemoteInputFilterTest, LocalEchosAndLocalActivity) {
  MockInputStub mock_stub;
  InputEventTracker input_tracker(&mock_stub);
  RemoteInputFilter input_filter(&input_tracker);

  EXPECT_CALL(mock_stub, InjectMouseEvent(_)).Times(5);

  for (int i = 0; i < 10; ++i) {
    input_filter.InjectMouseEvent(MouseMoveEvent(0, 0));
    input_filter.LocalPointerMoved(webrtc::DesktopVector(0, 0),
                                   ui::ET_MOUSE_MOVED);
    if (i == 4)
      input_filter.LocalPointerMoved(webrtc::DesktopVector(1, 1),
                                     ui::ET_MOUSE_MOVED);
  }
}

// Verify that local keyboard input blocks activity.
TEST(RemoteInputFilterTest, LocalKeyPressEventBlocksInput) {
  MockInputStub mock_stub;
  InputEventTracker input_tracker(&mock_stub);
  RemoteInputFilter input_filter(&input_tracker);
  input_filter.LocalKeyPressed(0);
  input_filter.InjectKeyEvent(UsbKeyEvent(1, true));
}

// Verify that local echoes of remote keyboard activity does not block input
TEST(RemoteInputFilterTest, LocalEchoOfKeyPressEventDoesNotBlockInput) {
  MockInputStub mock_stub;
  InputEventTracker input_tracker(&mock_stub);
  RemoteInputFilter input_filter(&input_tracker);
  EXPECT_CALL(mock_stub, InjectKeyEvent(_)).Times(4);
  input_filter.InjectKeyEvent(UsbKeyEvent(1, true));
  input_filter.InjectKeyEvent(UsbKeyEvent(1, false));
  input_filter.LocalKeyPressed(1);
  input_filter.InjectKeyEvent(UsbKeyEvent(2, true));
  input_filter.InjectKeyEvent(UsbKeyEvent(2, false));
}

// Verify that local input matching remote keyboard activity that has already
// been discarded as an echo blocks input.
TEST(RemoteInputFilterTest, LocalKeyPressEventMatchingPreviousEchoBlocksInput) {
  MockInputStub mock_stub;
  InputEventTracker input_tracker(&mock_stub);
  RemoteInputFilter input_filter(&input_tracker);
  EXPECT_CALL(mock_stub, InjectKeyEvent(_)).Times(2);
  input_filter.InjectKeyEvent(UsbKeyEvent(1, true));
  input_filter.InjectKeyEvent(UsbKeyEvent(1, false));
  input_filter.LocalKeyPressed(1);
  input_filter.LocalKeyPressed(1);
  input_filter.InjectKeyEvent(UsbKeyEvent(2, true));
  input_filter.InjectKeyEvent(UsbKeyEvent(2, false));
}

// Verify that local input matching remote keyboard activity blocks input if
// local echo is not expected
TEST(RemoteInputFilterTest,
     LocalDuplicateKeyPressEventBlocksInputIfEchoDisabled) {
  MockInputStub mock_stub;
  InputEventTracker input_tracker(&mock_stub);
  RemoteInputFilter input_filter(&input_tracker);
  input_filter.SetExpectLocalEcho(false);
  EXPECT_CALL(mock_stub, InjectKeyEvent(_)).Times(2);
  input_filter.InjectKeyEvent(UsbKeyEvent(1, true));
  input_filter.InjectKeyEvent(UsbKeyEvent(1, false));
  input_filter.LocalKeyPressed(1);
  input_filter.InjectKeyEvent(UsbKeyEvent(2, true));
  input_filter.InjectKeyEvent(UsbKeyEvent(2, false));
}

// Verify that local activity also causes buttons, keys, and touches to be
// released.
TEST(RemoteInputFilterTest, LocalActivityReleasesAll) {
  MockInputStub mock_stub;
  InputEventTracker input_tracker(&mock_stub);
  RemoteInputFilter input_filter(&input_tracker);

  EXPECT_CALL(mock_stub, InjectMouseEvent(_)).Times(5);

  // Use release of a key as a proxy for InputEventTracker::ReleaseAll()
  // having been called, rather than mocking it.
  EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEvent(0, true)));
  EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEvent(0, false)));
  input_filter.InjectKeyEvent(UsbKeyEvent(0, true));

  // Touch points that are down should be canceled.
  EXPECT_CALL(mock_stub, InjectTouchEvent(EqualsTouchEventTypeAndId(
                             protocol::TouchEvent::TOUCH_POINT_START, 0u)));
  EXPECT_CALL(mock_stub, InjectTouchEvent(EqualsTouchEventTypeAndId(
                             protocol::TouchEvent::TOUCH_POINT_CANCEL, 0u)));
  input_filter.InjectTouchEvent(TouchStartEvent(0u));

  for (int i = 0; i < 10; ++i) {
    input_filter.InjectMouseEvent(MouseMoveEvent(0, 0));
    input_filter.LocalPointerMoved(webrtc::DesktopVector(0, 0),
                                   ui::ET_MOUSE_MOVED);
    if (i == 4)
      input_filter.LocalPointerMoved(webrtc::DesktopVector(1, 1),
                                     ui::ET_MOUSE_MOVED);
  }
}

}  // namespace remoting
