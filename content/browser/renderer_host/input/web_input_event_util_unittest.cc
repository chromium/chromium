// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <numbers>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_detection/gesture_event_data.h"
#include "ui/events/gesture_event_details.h"
#include "ui/events/types/event_type.h"
#include "ui/events/velocity_tracker/motion_event_generic.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using ui::MotionEvent;
using ui::MotionEventGeneric;

namespace content {

TEST(WebInputEventUtilTest, MotionEventConversion) {
  const MotionEvent::ToolType tool_types[] = {MotionEvent::ToolType::FINGER,
                                              MotionEvent::ToolType::STYLUS,
                                              MotionEvent::ToolType::MOUSE};
  ui::PointerProperties pointer(5, 10, 40);
  pointer.id = 15;
  pointer.raw_x = 20;
  pointer.raw_y = 25;
  pointer.pressure = 30;
  pointer.touch_minor = 35;
  pointer.orientation = -std::numbers::pi_v<float> / 2;
  pointer.tilt_x = 60;
  pointer.tilt_y = 70;
  pointer.twist = 160;
  pointer.tangential_pressure = 0;
  for (MotionEvent::ToolType tool_type : tool_types) {
    pointer.tool_type = tool_type;
    MotionEventGeneric event(MotionEvent::Action::DOWN, base::TimeTicks::Now(),
                             pointer);
    event.set_flags(ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
    event.set_unique_event_id(123456U);

    WebTouchEvent expected_event(
        WebInputEvent::Type::kTouchStart,
        WebInputEvent::kShiftKey | WebInputEvent::kAltKey,
        event.GetEventTime());
    expected_event.touches_length = 1;
    WebTouchPoint expected_pointer;
    expected_pointer.id = pointer.id;
    expected_pointer.state = WebTouchPoint::State::kStatePressed;
    expected_pointer.SetPositionInWidget(pointer.x, pointer.y);
    expected_pointer.SetPositionInScreen(pointer.raw_x, pointer.raw_y);
    expected_pointer.radius_x = pointer.touch_major / 2.f;
    expected_pointer.radius_y = pointer.touch_minor / 2.f;
    expected_pointer.rotation_angle = 0.f;
    expected_pointer.force = pointer.pressure;
    if (tool_type == MotionEvent::ToolType::STYLUS) {
      expected_pointer.tilt_x = 60;
      expected_pointer.tilt_y = 70;
      expected_pointer.twist = 160;
      expected_pointer.tangential_pressure = 0;
    } else {
      expected_pointer.tilt_x = 0;
      expected_pointer.tilt_y = 0;
    }
    expected_event.touches[0] = expected_pointer;
    expected_event.unique_touch_event_id = 123456U;
    expected_event.hovering = true;

    WebTouchEvent actual_event = ui::CreateWebTouchEventFromMotionEvent(
        event, false /* may_cause_scrolling */, true /* hovering */);
    EXPECT_EQ(ui::WebInputEventTraits::ToString(expected_event),
              ui::WebInputEventTraits::ToString(actual_event));
  }
}

TEST(WebInputEventUtilTest, ScrollUpdateConversion) {
  int motion_event_id = 0;
  MotionEvent::ToolType tool_type = MotionEvent::ToolType::UNKNOWN;
  base::TimeTicks timestamp = base::TimeTicks::Now();
  gfx::Vector2dF delta(-5.f, 10.f);
  gfx::PointF pos(1.f, 2.f);
  gfx::PointF raw_pos(11.f, 12.f);
  size_t touch_points = 1;
  gfx::RectF rect(pos, gfx::SizeF());
  int flags = 0;
  ui::GestureEventDetails details(ui::EventType::kGestureScrollUpdate,
                                  delta.x(), delta.y());
  details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEventData event(details,
                             motion_event_id,
                             tool_type,
                             timestamp,
                             pos.x(),
                             pos.y(),
                             raw_pos.x(),
                             raw_pos.y(),
                             touch_points,
                             rect,
                             flags,
                             0U);

  blink::WebGestureEvent web_event =
      ui::CreateWebGestureEventFromGestureEventData(event);
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollUpdate, web_event.GetType());
  EXPECT_EQ(0, web_event.GetModifiers());
  EXPECT_EQ(timestamp, web_event.TimeStamp());
  EXPECT_EQ(pos, web_event.PositionInWidget());
  EXPECT_EQ(raw_pos, web_event.PositionInScreen());
  EXPECT_EQ(blink::WebGestureDevice::kTouchscreen, web_event.SourceDevice());
  EXPECT_EQ(delta.x(), web_event.data.scroll_update.delta_x);
  EXPECT_EQ(delta.y(), web_event.data.scroll_update.delta_y);
}

}  // namespace content
