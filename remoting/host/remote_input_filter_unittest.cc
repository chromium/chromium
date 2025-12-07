// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_input_filter.h"

#include <stdint.h>

#include "base/functional/bind.h"
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

class RemoteInputFilterTest : public testing::Test {
 protected:
  MockInputStub mock_stub_;
  InputEventTracker input_tracker_{&mock_stub_};
  RemoteInputFilter input_filter_{
      &input_tracker_, base::BindRepeating(&InputEventTracker::ReleaseAll,
                                           base::Unretained(&input_tracker_))};
};

// Verify that events get through if there is no local activity.
TEST_F(RemoteInputFilterTest, NoLocalActivity) {
  EXPECT_CALL(mock_stub_, InjectMouseEvent(_)).Times(10);

  for (int i = 0; i < 10; ++i) {
    input_filter_.InjectMouseEvent(MouseMoveEvent(0, 0));
  }
}

// Verify that events get through until there is local activity.
TEST_F(RemoteInputFilterTest, MismatchedLocalActivity) {
  EXPECT_CALL(mock_stub_, InjectMouseEvent(_)).Times(5);

  for (int i = 0; i < 10; ++i) {
    input_filter_.InjectMouseEvent(MouseMoveEvent(0, 0));
    if (i == 4) {
      input_filter_.LocalPointerMoved(webrtc::DesktopVector(10, 10),
                                      ui::EventType::kMouseMoved);
    }
  }
}

// Verify that touch events are not considered as echoes.
TEST_F(RemoteInputFilterTest, TouchEventsAreNotCheckedForEcho) {
  EXPECT_CALL(mock_stub_, InjectMouseEvent(_));

  input_filter_.InjectMouseEvent(MouseMoveEvent(0, 0));
  input_filter_.LocalPointerMoved(webrtc::DesktopVector(0, 0),
                                  ui::EventType::kTouchMoved);
  input_filter_.InjectMouseEvent(MouseMoveEvent(1, 1));
}

// Verify that echos of injected mouse events don't block activity.
TEST_F(RemoteInputFilterTest, LocalEchoesOfRemoteActivity) {
  EXPECT_CALL(mock_stub_, InjectMouseEvent(_)).Times(10);

  for (int i = 0; i < 10; ++i) {
    input_filter_.InjectMouseEvent(MouseMoveEvent(0, 0));
    // Echoes can be off by one pixel in each dimension.
    input_filter_.LocalPointerMoved(webrtc::DesktopVector(1, 1),
                                    ui::EventType::kMouseMoved);
  }
}

// Verify that echos followed by a mismatch blocks activity.
TEST_F(RemoteInputFilterTest, LocalEchosAndLocalActivity) {
  EXPECT_CALL(mock_stub_, InjectMouseEvent(_)).Times(5);

  for (int i = 0; i < 10; ++i) {
    input_filter_.InjectMouseEvent(MouseMoveEvent(0, 0));
    // Echoes can be off by one pixel in each dimension.
    input_filter_.LocalPointerMoved(webrtc::DesktopVector(1, 1),
                                    ui::EventType::kMouseMoved);
    if (i == 4) {
      input_filter_.LocalPointerMoved(webrtc::DesktopVector(10, 10),
                                      ui::EventType::kMouseMoved);
    }
  }
}

// Verify that local keyboard input blocks activity.
TEST_F(RemoteInputFilterTest, LocalKeyPressEventBlocksInput) {
  input_filter_.LocalKeyPressed(0);
  input_filter_.InjectKeyEvent(UsbKeyEvent(1, true));
}

// Verify that local echoes of remote keyboard activity does not block input
TEST_F(RemoteInputFilterTest, LocalEchoOfKeyPressEventDoesNotBlockInput) {
  EXPECT_CALL(mock_stub_, InjectKeyEvent(_)).Times(4);
  input_filter_.InjectKeyEvent(UsbKeyEvent(1, true));
  input_filter_.InjectKeyEvent(UsbKeyEvent(1, false));
  input_filter_.LocalKeyPressed(1);
  input_filter_.InjectKeyEvent(UsbKeyEvent(2, true));
  input_filter_.InjectKeyEvent(UsbKeyEvent(2, false));
}

// Verify that local input matching remote keyboard activity that has already
// been discarded as an echo blocks input.
TEST_F(RemoteInputFilterTest,
       LocalKeyPressEventMatchingPreviousEchoBlocksInput) {
  EXPECT_CALL(mock_stub_, InjectKeyEvent(_)).Times(2);
  input_filter_.InjectKeyEvent(UsbKeyEvent(1, true));
  input_filter_.InjectKeyEvent(UsbKeyEvent(1, false));
  input_filter_.LocalKeyPressed(1);
  input_filter_.LocalKeyPressed(1);
  input_filter_.InjectKeyEvent(UsbKeyEvent(2, true));
  input_filter_.InjectKeyEvent(UsbKeyEvent(2, false));
}

// Verify that local input matching remote keyboard activity blocks input if
// local echo is not expected
TEST_F(RemoteInputFilterTest,
       LocalDuplicateKeyPressEventBlocksInputIfEchoDisabled) {
  input_filter_.SetExpectLocalEcho(false);
  EXPECT_CALL(mock_stub_, InjectKeyEvent(_)).Times(2);
  input_filter_.InjectKeyEvent(UsbKeyEvent(1, true));
  input_filter_.InjectKeyEvent(UsbKeyEvent(1, false));
  input_filter_.LocalKeyPressed(1);
  input_filter_.InjectKeyEvent(UsbKeyEvent(2, true));
  input_filter_.InjectKeyEvent(UsbKeyEvent(2, false));
}

// Verify that local activity also causes buttons, keys, and touches to be
// released.
TEST_F(RemoteInputFilterTest, LocalActivityReleasesAll) {
  EXPECT_CALL(mock_stub_, InjectMouseEvent(_)).Times(5);

  // Use release of a key as a proxy for InputEventTracker::ReleaseAll()
  // having been called, rather than mocking it.
  EXPECT_CALL(mock_stub_, InjectKeyEvent(EqualsKeyEvent(0, true)));
  EXPECT_CALL(mock_stub_, InjectKeyEvent(EqualsKeyEvent(0, false)));
  input_filter_.InjectKeyEvent(UsbKeyEvent(0, true));

  // Touch points that are down should be canceled.
  EXPECT_CALL(mock_stub_, InjectTouchEvent(EqualsTouchEventTypeAndId(
                              protocol::TouchEvent::TOUCH_POINT_START, 0u)));
  EXPECT_CALL(mock_stub_, InjectTouchEvent(EqualsTouchEventTypeAndId(
                              protocol::TouchEvent::TOUCH_POINT_CANCEL, 0u)));
  input_filter_.InjectTouchEvent(TouchStartEvent(0u));

  for (int i = 0; i < 10; ++i) {
    input_filter_.InjectMouseEvent(MouseMoveEvent(0, 0));
    // Echoes can be off by one pixel in each dimension.
    input_filter_.LocalPointerMoved(webrtc::DesktopVector(1, 1),
                                    ui::EventType::kMouseMoved);
    if (i == 4) {
      input_filter_.LocalPointerMoved(webrtc::DesktopVector(10, 10),
                                      ui::EventType::kMouseMoved);
    }
  }
}

}  // namespace remoting
