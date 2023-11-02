// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/input/touch_input_scaler.h"

#include <stdint.h>

#include <cmath>

#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/test_event_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::PrintToString;

namespace remoting {

using protocol::MockInputStub;
using protocol::TouchEvent;
using protocol::TouchEventPoint;
using protocol::test::EqualsTouchPointCoordinates;
using protocol::test::EqualsTouchPointRadii;

namespace {

const float kDefaultRadius = 30.0f;
const float kDefaultXCoord = 1.0f;
const float kDefaultYCoord = 1.0f;

struct PointInfo {
  PointInfo(float x, float y)
      : PointInfo(x, y, kDefaultRadius, kDefaultRadius) {}
  PointInfo(float x, float y, float radius_x, float radius_y)
      : x(x), y(y), radius_x(radius_x), radius_y(radius_y) {}

  float x;
  float y;
  float radius_x;
  float radius_y;
};

const PointInfo kDefaultPointInfo = {kDefaultXCoord, kDefaultYCoord,
                                     kDefaultRadius, kDefaultRadius};

}  // namespace

class TouchInputScalerTest : public ::testing::Test {
 public:
  TouchInputScalerTest(const TouchInputScalerTest&) = delete;
  TouchInputScalerTest& operator=(const TouchInputScalerTest&) = delete;

 protected:
  TouchInputScalerTest() : touch_input_scaler_(&mock_stub_) {}

  void AddInputCoordinate(const PointInfo& point_info) {
    point_infos_.push_back(point_info);
  }

  void AddDefaultTestPoint() { point_infos_.push_back(kDefaultPointInfo); }

  void InjectTestTouchEvent() {
    TouchEvent e;
    e.set_event_type(TouchEvent::TOUCH_POINT_MOVE);

    uint32_t id = 1;
    for (const PointInfo& point_info : point_infos_) {
      TouchEventPoint* point = e.add_touch_points();
      point->set_id(id++);
      point->set_x(point_info.x);
      point->set_y(point_info.y);
      point->set_radius_x(point_info.radius_x);
      point->set_radius_y(point_info.radius_y);
    }

    touch_input_scaler_.InjectTouchEvent(e);
  }

  void SetInputDimensions(int width, int height) {
    touch_input_scaler_.set_input_size(webrtc::DesktopSize(width, height));
  }

  void SetOutputDimensions(int width, int height) {
    touch_input_scaler_.set_output_size(webrtc::DesktopSize(width, height));
  }

  MockInputStub mock_stub_;
  TouchInputScaler touch_input_scaler_;

 private:
  std::vector<PointInfo> point_infos_;
};

// TouchInputFilter require both input and output dimensions.
// These test verify that no events are forwarded to the next InputStub if
// either dimensions are not set or 0.
TEST_F(TouchInputScalerTest, NoDimensionsSet) {
  AddDefaultTestPoint();
  EXPECT_CALL(mock_stub_, InjectTouchEvent(_)).Times(0);
  InjectTestTouchEvent();
}

TEST_F(TouchInputScalerTest, BothDimensionsZero) {
  SetInputDimensions(0, 0);
  SetOutputDimensions(0, 0);
  AddDefaultTestPoint();
  EXPECT_CALL(mock_stub_, InjectTouchEvent(_)).Times(0);
  InjectTestTouchEvent();
}

TEST_F(TouchInputScalerTest, SetOnlyInputDimensions) {
  SetInputDimensions(50, 60);
  AddDefaultTestPoint();
  EXPECT_CALL(mock_stub_, InjectTouchEvent(_)).Times(0);
  InjectTestTouchEvent();
}

TEST_F(TouchInputScalerTest, SetOnlyOutputDimensions) {
  SetOutputDimensions(50, 60);
  AddDefaultTestPoint();
  EXPECT_CALL(mock_stub_, InjectTouchEvent(_)).Times(0);
  InjectTestTouchEvent();
}

// The x,y coordinate fall in the desktop size.
TEST_F(TouchInputScalerTest, NoClampingNoScaling) {
  SetInputDimensions(50, 60);
  SetOutputDimensions(50, 60);

  AddInputCoordinate({10.0f, 15.0f});
  TouchEvent expected_out;
  TouchEventPoint* point = expected_out.add_touch_points();
  point->set_x(10.0f);
  point->set_y(15.0f);

  EXPECT_CALL(mock_stub_,
              InjectTouchEvent(EqualsTouchPointCoordinates(expected_out)));
  InjectTestTouchEvent();
}

// Make sure clamping works.
TEST_F(TouchInputScalerTest, ClampingNoScaling) {
  SetInputDimensions(50, 60);
  SetOutputDimensions(50, 60);

  // Note that this could happen if touch started in the chromoting window but
  // the finger moved off the windows.
  AddInputCoordinate({-1.0f, 1.0f});
  TouchEvent expected_out;
  TouchEventPoint* point = expected_out.add_touch_points();
  point->set_x(0.0f);
  point->set_y(1.0f);

  EXPECT_CALL(mock_stub_,
              InjectTouchEvent(EqualsTouchPointCoordinates(expected_out)));
  InjectTestTouchEvent();
}

TEST_F(TouchInputScalerTest, ClampingMultiplePointsNoScaling) {
  SetInputDimensions(50, 60);
  SetOutputDimensions(50, 60);

  AddInputCoordinate({-1.0f, 1.0f});  // Fall off left.
  TouchEvent expected_out;
  TouchEventPoint* point = expected_out.add_touch_points();
  point->set_x(0.0f);
  point->set_y(1.0f);

  AddInputCoordinate({100.0f, 1.0f});  // Fall off right.
  point = expected_out.add_touch_points();
  point->set_x(49.0f);
  point->set_y(1.0f);

  AddInputCoordinate({20.0f, 15.0f});  // Should not be clamped.
  point = expected_out.add_touch_points();
  point->set_x(20.0f);
  point->set_y(15.0f);

  AddInputCoordinate({3.0f, -1.0f});  // Fall off above.
  point = expected_out.add_touch_points();
  point->set_x(3.0f);
  point->set_y(0.0f);

  AddInputCoordinate({10.0f, 200.0f});  // Fall off below.
  point = expected_out.add_touch_points();
  point->set_x(10.0f);
  point->set_y(59.0f);

  EXPECT_CALL(mock_stub_,
              InjectTouchEvent(EqualsTouchPointCoordinates(expected_out)));
  InjectTestTouchEvent();
}

// Verify up-scaling works. All coordinates should fall inside the output
// dimensions after scaling.
TEST_F(TouchInputScalerTest, UpScalingNoClamping) {
  SetInputDimensions(20, 20);
  SetOutputDimensions(40, 40);

  AddInputCoordinate({1.0f, 1.0f});
  TouchEvent expected_out;
  TouchEventPoint* point = expected_out.add_touch_points();
  point->set_x(2.0f);
  point->set_y(2.0f);

  AddInputCoordinate({1.2f, 4.2f});
  point = expected_out.add_touch_points();
  point->set_x(2.4f);
  point->set_y(8.4f);

  EXPECT_CALL(mock_stub_,
              InjectTouchEvent(EqualsTouchPointCoordinates(expected_out)));
  InjectTestTouchEvent();
}

// Verify up-scaling works with clamping.
TEST_F(TouchInputScalerTest, UpScalingWithClamping) {
  SetInputDimensions(20, 20);
  SetOutputDimensions(40, 40);

  AddInputCoordinate({25.0f, 25.0f});
  TouchEvent expected_out;
  TouchEventPoint* point = expected_out.add_touch_points();
  point->set_x(39.0f);
  point->set_y(39.0f);

  EXPECT_CALL(mock_stub_,
              InjectTouchEvent(EqualsTouchPointCoordinates(expected_out)));
  InjectTestTouchEvent();
}

// Verify down scaling works. All coordinates should fall inside the output
// dimensions after scaling.
TEST_F(TouchInputScalerTest, DownScalingNoClamping) {
  SetInputDimensions(40, 40);
  SetOutputDimensions(20, 20);

  AddInputCoordinate({2.0f, 2.0f});
  TouchEvent expected_out;
  TouchEventPoint* point = expected_out.add_touch_points();
  point->set_x(1.0f);
  point->set_y(1.0f);

  AddInputCoordinate({6.0f, 3.0f});
  point = expected_out.add_touch_points();
  point->set_x(3.0);
  point->set_y(1.5f);

  EXPECT_CALL(mock_stub_,
              InjectTouchEvent(EqualsTouchPointCoordinates(expected_out)));
  InjectTestTouchEvent();
}

// Verify down scaling works with clamping.
TEST_F(TouchInputScalerTest, DownScalingWithClamping) {
  SetInputDimensions(40, 40);
  SetOutputDimensions(20, 20);

  AddInputCoordinate({-20.0f, 10.0f});
  TouchEvent expected_out;
  TouchEventPoint* point = expected_out.add_touch_points();
  point->set_x(0.0f);
  point->set_y(5.0f);

  AddInputCoordinate({10.0f, -20.0f});
  point = expected_out.add_touch_points();
  point->set_x(5.0f);
  point->set_y(0.0f);

  AddInputCoordinate({6.0f, 80.0f});
  point = expected_out.add_touch_points();
  point->set_x(3.0f);
  point->set_y(19.0f);

  AddInputCoordinate({80.0f, 6.0f});
  point = expected_out.add_touch_points();
  point->set_x(19.0f);
  point->set_y(3.0f);

  EXPECT_CALL(mock_stub_,
              InjectTouchEvent(EqualsTouchPointCoordinates(expected_out)));
  InjectTestTouchEvent();
}

// Verify that the radii are up-scaled.
TEST_F(TouchInputScalerTest, UpScaleRadii) {
  SetInputDimensions(20, 20);
  SetOutputDimensions(40, 40);

  AddInputCoordinate({0.0f, 0.0f, 1.0f, 2.0f});
  TouchEvent expected_out;
  TouchEventPoint* point = expected_out.add_touch_points();
  point->set_radius_x(2.0f);
  point->set_radius_y(4.0f);

  EXPECT_CALL(mock_stub_,
              InjectTouchEvent(EqualsTouchPointRadii(expected_out)));
  InjectTestTouchEvent();
}

// Verify that the radii are down-scaled.
TEST_F(TouchInputScalerTest, DownScaleRadii) {
  SetInputDimensions(20, 20);
  SetOutputDimensions(10, 10);

  AddInputCoordinate({0.0f, 0.0f, 5.0f, 4.0f});
  TouchEvent expected_out;
  TouchEventPoint* point = expected_out.add_touch_points();
  point->set_radius_x(2.5f);
  point->set_radius_y(2.0f);

  EXPECT_CALL(mock_stub_,
              InjectTouchEvent(EqualsTouchPointRadii(expected_out)));
  InjectTestTouchEvent();
}

// Verify that up-scaling with clamping works for x,y coordinates and radii all
// work together.
TEST_F(TouchInputScalerTest, UpScaleCoordinatesAndRadii) {
  SetInputDimensions(20, 20);
  SetOutputDimensions(40, 40);

  AddInputCoordinate({5.0f, 12.0f, 3.0f, 2.0f});
  TouchEvent expected_out;
  TouchEventPoint* point = expected_out.add_touch_points();
  point->set_x(10.0f);
  point->set_y(24.0f);
  point->set_radius_x(6.0f);
  point->set_radius_y(4.0f);

  // Make sure clamping and scaling all work.
  AddInputCoordinate({22.0f, -1.0f, 8.0f, 3.0f});
  point = expected_out.add_touch_points();
  point->set_x(39.0f);
  point->set_y(0.0f);
  point->set_radius_x(16.0f);
  point->set_radius_y(6.0f);

  EXPECT_CALL(mock_stub_,
              InjectTouchEvent(AllOf(EqualsTouchPointCoordinates(expected_out),
                                     EqualsTouchPointRadii(expected_out))));
  InjectTestTouchEvent();
}

// Verify that down-scaling with clamping works for x,y coordinates and radii
// all work together.
TEST_F(TouchInputScalerTest, DownScaleCoordinatesAndRadii) {
  SetInputDimensions(60, 60);
  SetOutputDimensions(20, 20);

  AddInputCoordinate({50.0f, 24.0f, 10.0f, 9.0f});
  TouchEvent expected_out;
  TouchEventPoint* point = expected_out.add_touch_points();
  point->set_x(16.666f);
  point->set_y(8.0f);
  point->set_radius_x(3.333f);
  point->set_radius_y(3.0f);

  // Make sure clamping and scaling all work.
  AddInputCoordinate({70.0f, 82.0f, 8.0f, 3.0f});
  point = expected_out.add_touch_points();
  point->set_x(19.0f);
  point->set_y(19.0f);
  point->set_radius_x(2.666f);
  point->set_radius_y(1.0f);

  EXPECT_CALL(mock_stub_,
              InjectTouchEvent(AllOf(EqualsTouchPointCoordinates(expected_out),
                                     EqualsTouchPointRadii(expected_out))));
  InjectTestTouchEvent();
}

}  // namespace remoting
