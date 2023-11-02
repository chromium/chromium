// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(SyntheticWebInputEventBuilders, BuildWebTouchEvent) {
  SyntheticWebTouchEvent event;

  event.PressPoint(1, 2);
  EXPECT_EQ(1U, event.touches_length);
  EXPECT_EQ(0, event.touches[0].id);
  EXPECT_EQ(WebTouchPoint::State::kStatePressed, event.touches[0].state);
  EXPECT_EQ(gfx::PointF(1, 2), event.touches[0].PositionInWidget());
  event.ResetPoints();

  event.PressPoint(3, 4);
  EXPECT_EQ(2U, event.touches_length);
  EXPECT_EQ(1, event.touches[1].id);
  EXPECT_EQ(WebTouchPoint::State::kStatePressed, event.touches[1].state);
  EXPECT_EQ(gfx::PointF(3, 4), event.touches[1].PositionInWidget());
  event.ResetPoints();

  event.MovePoint(1, 5, 6);
  EXPECT_EQ(2U, event.touches_length);
  EXPECT_EQ(1, event.touches[1].id);
  EXPECT_EQ(WebTouchPoint::State::kStateMoved, event.touches[1].state);
  EXPECT_EQ(gfx::PointF(5, 6), event.touches[1].PositionInWidget());
  event.ResetPoints();

  event.ReleasePoint(0);
  EXPECT_EQ(2U, event.touches_length);
  EXPECT_EQ(0, event.touches[0].id);
  EXPECT_EQ(WebTouchPoint::State::kStateReleased, event.touches[0].state);
  event.ResetPoints();

  event.MovePoint(1, 7, 8);
  EXPECT_EQ(1U, event.touches_length);
  EXPECT_EQ(1, event.touches[1].id);
  EXPECT_EQ(WebTouchPoint::State::kStateMoved, event.touches[1].state);
  EXPECT_EQ(gfx::PointF(7, 8), event.touches[1].PositionInWidget());
  EXPECT_EQ(WebTouchPoint::State::kStateUndefined, event.touches[0].state);
  event.ResetPoints();

  event.PressPoint(9, 10);
  EXPECT_EQ(2U, event.touches_length);
  EXPECT_EQ(2, event.touches[0].id);
  EXPECT_EQ(WebTouchPoint::State::kStatePressed, event.touches[0].state);
  EXPECT_EQ(gfx::PointF(9, 10), event.touches[0].PositionInWidget());
  EXPECT_EQ(0.5, event.touches[0].force);
  EXPECT_EQ(0.5, event.touches[1].force);
  event.ResetPoints();

  event.ReleasePoint(0);
  event.ReleasePoint(1);
  event.ResetPoints();

  // Set radius, rotation angle, force for touch start event
  event.PressPoint(9, 10, 10, 20, 36, 0.62);
  EXPECT_EQ(1U, event.touches_length);
  EXPECT_EQ(3, event.touches[0].id);
  EXPECT_EQ(WebTouchPoint::State::kStatePressed, event.touches[0].state);
  EXPECT_EQ(gfx::PointF(9, 10), event.touches[0].PositionInWidget());
  EXPECT_EQ(10, event.touches[0].radius_x);
  EXPECT_EQ(20, event.touches[0].radius_y);
  EXPECT_EQ(36, event.touches[0].rotation_angle);
  EXPECT_EQ(0.62f, event.touches[0].force);
  EXPECT_EQ(0, event.touches[0].tilt_x);
  EXPECT_EQ(0, event.touches[0].tilt_y);
  EXPECT_EQ(0, event.touches[0].twist);

  // Set radius, rotation angle, force for touch move event
  event.MovePoint(0, 11, 15, 8, 16, 28, 0.73);
  EXPECT_EQ(1U, event.touches_length);
  EXPECT_EQ(3, event.touches[0].id);
  EXPECT_EQ(WebTouchPoint::State::kStateMoved, event.touches[0].state);
  EXPECT_EQ(gfx::PointF(11, 15), event.touches[0].PositionInWidget());
  EXPECT_EQ(8, event.touches[0].radius_x);
  EXPECT_EQ(16, event.touches[0].radius_y);
  EXPECT_EQ(28, event.touches[0].rotation_angle);
  EXPECT_EQ(0.73f, event.touches[0].force);
  EXPECT_EQ(0, event.touches[0].tilt_x);
  EXPECT_EQ(0, event.touches[0].tilt_y);
  EXPECT_EQ(0, event.touches[0].twist);
  event.ResetPoints();
}

}  // namespace blink
