// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/motion_event_web.h"

#include <stddef.h>

#include <numbers>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/test/motion_event_test_utils.h"
#include "ui/events/velocity_tracker/motion_event_generic.h"

using ui::MotionEvent;
using ui::MotionEventGeneric;
using ui::PointerProperties;

namespace content {

TEST(MotionEventWebTest, Constructor) {
  const auto orientations = std::to_array<float>(
      {-std::numbers::pi_v<float>, -2 * std::numbers::pi_v<float> / 3,
       -std::numbers::pi_v<float> / 2});
  const auto tilts_x = std::to_array<float>({0.f, -180 / 4, -180 / 3});
  const auto tilts_y = std::to_array<float>({0.5f, 180 / 2, 180 / 3});
  const auto twists = std::to_array<const float>({60, 160, 260});
  const auto tangential_pressures =
      std::to_array<const float>({0.3f, 0.5f, 0.9f});
  const auto tool_types = std::to_array<const MotionEvent::ToolType>(
      {MotionEvent::ToolType::FINGER, MotionEvent::ToolType::STYLUS,
       MotionEvent::ToolType::MOUSE});

  base::TimeTicks event_time = base::TimeTicks::Now();
  PointerProperties pp;
  MotionEventGeneric generic_event(MotionEvent::Action::MOVE, event_time, pp);
  for (MotionEvent::ToolType tool_type : tool_types) {
    for (size_t i = 0; i < std::size(tilts_x); ++i) {
      const float tilt_x = tilts_x[i];
      const float tilt_y = tilts_y[i];
      const float orientation = orientations[i];
      const float twist = twists[i];
      const float tangential_pressure = tangential_pressures[i];
      PointerProperties pp2;
      pp2.orientation = orientation;
      pp2.tilt_x = tilt_x;
      pp2.tilt_y = tilt_y;
      pp2.twist = twist;
      pp2.tangential_pressure = tangential_pressure;
      pp2.tool_type = tool_type;
      size_t pointer_index = generic_event.PushPointer(pp2);
      EXPECT_GT(pointer_index, 0u);

      blink::WebTouchEvent web_touch_event = CreateWebTouchEventFromMotionEvent(
          generic_event, true /* may_cause_scrolling */, false /* hovering */);

      MotionEventWeb event(web_touch_event);
      EXPECT_EQ(tool_type, event.GetToolType(pointer_index));
      if (tool_type == MotionEvent::ToolType::STYLUS) {
        // Web touch event touch point tilt plane angles are stored as ints,
        // thus the tilt precision is 1 degree and the error should not be
        // greater than 0.5 degrees.
        EXPECT_NEAR(tilt_x, event.GetTiltX(pointer_index), 0.5)
            << " orientation=" << orientation;
        EXPECT_NEAR(tilt_y, event.GetTiltY(pointer_index), 0.5)
            << " orientation=" << orientation;
        EXPECT_EQ(twist, event.GetTwist(pointer_index));
        EXPECT_EQ(tangential_pressure,
                  event.GetTangentialPressure(pointer_index));
      } else {
        EXPECT_EQ(0.f, event.GetTiltX(pointer_index));
        EXPECT_EQ(0.f, event.GetTiltY(pointer_index));
      }
      if (tool_type == MotionEvent::ToolType::STYLUS && tilt_x > 0.f) {
        // Full stylus tilt orientation information survives above event
        // conversions only if there is a non-zero stylus tilt angle.
        // See: http://crbug.com/251330
        EXPECT_NEAR(orientation, event.GetOrientation(pointer_index), 1e-4)
            << " tilt_x=" << tilt_x << " tilt_y=" << tilt_y;
      } else {
        // For non-stylus pointers and for styluses with a zero tilt angle,
        // orientation quadrant information is lost.
        EXPECT_NEAR(fmod(orientation + std::numbers::pi_v<float> + 1e-4,
                         std::numbers::pi_v<float> / 2) -
                        1e-4,
                    event.GetOrientation(pointer_index), 1e-4);
      }

      generic_event.RemovePointerAt(pointer_index);
    }
}
}

}  // namespace content
