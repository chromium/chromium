// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fractional_input_filter.h"

#include "base/containers/span.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/test_event_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace remoting::protocol {

using test::EqualsMouseMoveEvent;
using test::EqualsTouchEvent;

namespace {

struct Screen {
  int id;
  int x;
  int y;
  int width;
  int height;
};

// o--------------+
// | screen_id 1  |
// | 200x100      |
// +--------------+------------------+
//                | screen_id 2      |
//                | 300x200          |
//                |                  |
//                +------------------+
constexpr Screen kSimpleLayout[] = {{1, 0, 0, 200, 100},
                                    {2, 200, 100, 300, 200}};

VideoLayout BuildLayout(base::span<const Screen> screens) {
  VideoLayout layout;
  for (const Screen& screen : screens) {
    VideoTrackLayout* track = layout.add_video_track();
    track->set_screen_id(screen.id);
    track->set_position_x(screen.x);
    track->set_position_y(screen.y);
    track->set_width(screen.width);
    track->set_height(screen.height);
  }
  return layout;
}

FractionalCoordinate BuildFractionalCoordinates(int id, float x, float y) {
  FractionalCoordinate result;
  if (id != 0) {
    result.set_screen_id(id);
  }
  result.set_x(x);
  result.set_y(y);
  return result;
}

MouseEvent MouseMoveEvent(int id, float x, float y) {
  MouseEvent event;
  *event.mutable_fractional_coordinate() = BuildFractionalCoordinates(id, x, y);
  return event;
}

}  // namespace

class FractionalInputFilterTest : public testing::Test {
 public:
  FractionalInputFilterTest() : filter_(&mock_stub_) {}

 protected:
  FractionalInputFilter filter_;
  MockInputStub mock_stub_;
};

TEST_F(FractionalInputFilterTest, MouseEventCalculation) {
  filter_.set_video_layout(BuildLayout(kSimpleLayout));

  EXPECT_CALL(mock_stub_, InjectMouseEvent(EqualsMouseMoveEvent(0, 0)));
  filter_.InjectMouseEvent(MouseMoveEvent(1, 0.0, 0.0));

  EXPECT_CALL(mock_stub_, InjectMouseEvent(EqualsMouseMoveEvent(100, 50)));
  filter_.InjectMouseEvent(MouseMoveEvent(1, 0.5, 0.5));

  EXPECT_CALL(mock_stub_, InjectMouseEvent(EqualsMouseMoveEvent(199, 99)));
  filter_.InjectMouseEvent(MouseMoveEvent(1, 1.0, 1.0));

  EXPECT_CALL(mock_stub_, InjectMouseEvent(EqualsMouseMoveEvent(200, 100)));
  filter_.InjectMouseEvent(MouseMoveEvent(2, 0.0, 0.0));

  EXPECT_CALL(mock_stub_, InjectMouseEvent(EqualsMouseMoveEvent(350, 200)));
  filter_.InjectMouseEvent(MouseMoveEvent(2, 0.5, 0.5));

  EXPECT_CALL(mock_stub_, InjectMouseEvent(EqualsMouseMoveEvent(499, 299)));
  filter_.InjectMouseEvent(MouseMoveEvent(2, 1.0, 1.0));
}

TEST_F(FractionalInputFilterTest, MouseEventNoCoordinatesIsPassedThrough) {
  MouseEvent event;
  event.set_x(42);
  event.set_y(99);

  EXPECT_CALL(mock_stub_, InjectMouseEvent(EqualsMouseMoveEvent(42, 99)));
  filter_.InjectMouseEvent(event);
}

TEST_F(FractionalInputFilterTest, MouseEventInvalidScreenIsDropped) {
  filter_.set_video_layout(BuildLayout(kSimpleLayout));

  EXPECT_CALL(mock_stub_, InjectMouseEvent(_)).Times(0);
  filter_.InjectMouseEvent(MouseMoveEvent(9, 0.5, 0.5));
}

TEST_F(FractionalInputFilterTest, TouchEventCalculation) {
  filter_.set_video_layout(BuildLayout(kSimpleLayout));

  TouchEvent event;
  TouchEventPoint* p = event.add_touch_points();
  *p->mutable_fractional_coordinate() = BuildFractionalCoordinates(1, 0.5, 0.5);
  p = event.add_touch_points();
  *p->mutable_fractional_coordinate() = BuildFractionalCoordinates(2, 0.5, 0.5);

  TouchEvent expected_event(event);
  expected_event.mutable_touch_points(0)->set_x(100);
  expected_event.mutable_touch_points(0)->set_y(50);
  expected_event.mutable_touch_points(1)->set_x(350);
  expected_event.mutable_touch_points(1)->set_y(200);

  EXPECT_CALL(mock_stub_, InjectTouchEvent(EqualsTouchEvent(expected_event)));
  filter_.InjectTouchEvent(event);
}

TEST_F(FractionalInputFilterTest, TouchEventNoCoordinatesPassedThrough) {
  TouchEvent event;
  TouchEventPoint* p = event.add_touch_points();
  p->set_x(10);
  p->set_y(20);
  p = event.add_touch_points();
  p->set_x(30);
  p->set_y(40);

  EXPECT_CALL(mock_stub_, InjectTouchEvent(EqualsTouchEvent(event)));
  filter_.InjectTouchEvent(event);
}

TEST_F(FractionalInputFilterTest, TouchEventInvalidScreenIsDropped) {
  filter_.set_video_layout(BuildLayout(kSimpleLayout));

  TouchEvent event;
  TouchEventPoint* p = event.add_touch_points();
  *p->mutable_fractional_coordinate() = BuildFractionalCoordinates(1, 0.5, 0.5);
  p = event.add_touch_points();
  *p->mutable_fractional_coordinate() = BuildFractionalCoordinates(9, 0.5, 0.5);

  EXPECT_CALL(mock_stub_, InjectTouchEvent(_)).Times(0);
  filter_.InjectTouchEvent(event);
}

TEST_F(FractionalInputFilterTest, FallbackUsedIfNoScreenId) {
  filter_.set_video_layout(BuildLayout(kSimpleLayout));
  filter_.set_fallback_geometry(
      webrtc::DesktopRect::MakeXYWH(200, 100, 300, 200));

  EXPECT_CALL(mock_stub_, InjectMouseEvent(EqualsMouseMoveEvent(350, 200)));
  filter_.InjectMouseEvent(MouseMoveEvent(0, 0.5, 0.5));
}

TEST_F(FractionalInputFilterTest, EventWithNoScreenIdAndNoFallbackIsDropped) {
  filter_.set_video_layout(BuildLayout(kSimpleLayout));
  filter_.set_fallback_geometry({});

  EXPECT_CALL(mock_stub_, InjectMouseEvent(_)).Times(0);
  filter_.InjectMouseEvent(MouseMoveEvent(0, 0.5, 0.5));
}

}  // namespace remoting::protocol
