// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/motion_event_web.h"

#include <stddef.h>

#include "base/numerics/math_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/gesture_detection/motion_event_generic.h"
#include "ui/events/test/motion_event_test_utils.h"

using ui::MotionEvent;
using ui::MotionEventGeneric;
using ui::PointerProperties;

namespace content {

TEST(MotionEventWebTest, Constructor) {
  const float pi = base::kPiFloat;
  const float orientations[] = {-pi, -2.f * pi / 3, -pi / 2};
  const float tilts_x[] = {0.f, -180 / 4, -180 / 3};
  const float tilts_y[] = {0.5f, 180 / 2, 180 / 3};
  const float twists[] = {60, 160, 260};
  const float tangential_pressures[] = {0.3f, 0.5f, 0.9f};
  const MotionEvent::ToolType tool_types[] = {MotionEvent::ToolType::FINGER,
                                              MotionEvent::ToolType::STYLUS,
                                              MotionEvent::ToolType::MOUSE};

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
        EXPECT_NEAR(
            fmod(orientation + base::kPiFloat + 1e-4, base::kPiFloat / 2) -
                1e-4,
            event.GetOrientation(pointer_index), 1e-4);
      }

      generic_event.RemovePointerAt(pointer_index);
    }
}
}

}  // namespace content
