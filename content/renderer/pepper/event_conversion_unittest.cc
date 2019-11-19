// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/event_conversion.h"

#include <stddef.h>

#include <memory>

#include "base/logging.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "ppapi/shared_impl/ppb_input_event_shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class EventConversionTest : public ::testing::Test {
 protected:
  void CompareWebTouchEvents(const blink::WebTouchEvent& expected,
                             const blink::WebTouchEvent& actual) {
    EXPECT_EQ(expected.GetType(), actual.GetType());
    ASSERT_EQ(expected.touches_length, actual.touches_length);
    for (size_t i = 0; i < expected.touches_length; ++i) {
      size_t j = 0;
      for (; j < actual.touches_length; ++j) {
        if (actual.touches[j].id == expected.touches[i].id)
          break;
      }
      ASSERT_NE(j, actual.touches_length);
      EXPECT_EQ(expected.touches[i].id, actual.touches[j].id);
      EXPECT_EQ(expected.touches[i].state, actual.touches[j].state);
      EXPECT_EQ(expected.touches[i].PositionInWidget().x,
                actual.touches[j].PositionInWidget().x);
      EXPECT_EQ(expected.touches[i].PositionInWidget().y,
                actual.touches[j].PositionInWidget().y);
      EXPECT_EQ(expected.touches[i].radius_x, actual.touches[j].radius_x);
      EXPECT_EQ(expected.touches[i].radius_y, actual.touches[j].radius_y);
      EXPECT_EQ(expected.touches[i].rotation_angle,
                actual.touches[j].rotation_angle);
      EXPECT_EQ(expected.touches[i].force, actual.touches[j].force);
    }
  }
};

TEST_F(EventConversionTest, TouchStart) {
  SyntheticWebTouchEvent touch;
  touch.PressPoint(1.f, 2.f);

  std::vector<ppapi::InputEventData> pp_events;
  CreateInputEventData(touch, nullptr, &pp_events);
  ASSERT_EQ(1U, pp_events.size());

  const ppapi::InputEventData& pp_event = pp_events[0];
  ASSERT_EQ(PP_INPUTEVENT_TYPE_TOUCHSTART, pp_event.event_type);
  ASSERT_EQ(1U, pp_event.touches.size());
  ASSERT_EQ(1U, pp_event.changed_touches.size());
  ASSERT_EQ(1U, pp_event.target_touches.size());

  std::unique_ptr<blink::WebInputEvent> event_out(
      CreateWebInputEvent(pp_event));
  const blink::WebTouchEvent* touch_out =
      static_cast<const blink::WebTouchEvent*>(event_out.get());
  ASSERT_TRUE(touch_out);
  EXPECT_EQ(touch.GetType(), touch_out->GetType());
  EXPECT_EQ(touch.touches_length, touch_out->touches_length);
  CompareWebTouchEvents(touch, *touch_out);
}

TEST_F(EventConversionTest, TouchMove) {
  SyntheticWebTouchEvent touch;
  touch.PressPoint(1.f, 2.f);
  touch.ResetPoints();
  touch.PressPoint(3.f, 4.f);
  touch.ResetPoints();
  touch.MovePoint(1, 5.f, 6.f);

  std::vector<ppapi::InputEventData> pp_events;
  CreateInputEventData(touch, nullptr, &pp_events);
  ASSERT_EQ(1U, pp_events.size());

  const ppapi::InputEventData& pp_event = pp_events[0];
  ASSERT_EQ(PP_INPUTEVENT_TYPE_TOUCHMOVE, pp_event.event_type);
  ASSERT_EQ(2U, pp_event.touches.size());
  ASSERT_EQ(1U, pp_event.changed_touches.size());
  ASSERT_EQ(2U, pp_event.target_touches.size());

  std::unique_ptr<blink::WebInputEvent> event_out(
      CreateWebInputEvent(pp_event));
  const blink::WebTouchEvent* touch_out =
      static_cast<const blink::WebTouchEvent*>(event_out.get());
  ASSERT_TRUE(touch_out);
  EXPECT_EQ(touch.GetType(), touch_out->GetType());
  EXPECT_EQ(touch.touches_length, touch_out->touches_length);
  CompareWebTouchEvents(touch, *touch_out);
}

TEST_F(EventConversionTest, TouchEnd) {
  SyntheticWebTouchEvent touch;
  touch.PressPoint(1.f, 2.f);
  touch.ResetPoints();
  touch.PressPoint(3.f, 4.f);
  touch.ResetPoints();
  touch.ReleasePoint(0);

  std::vector<ppapi::InputEventData> pp_events;
  CreateInputEventData(touch, nullptr, &pp_events);
  ASSERT_EQ(1U, pp_events.size());

  const ppapi::InputEventData& pp_event = pp_events[0];
  ASSERT_EQ(PP_INPUTEVENT_TYPE_TOUCHEND, pp_event.event_type);
  ASSERT_EQ(1U, pp_event.touches.size());
  ASSERT_EQ(1U, pp_event.changed_touches.size());
  ASSERT_EQ(2U, pp_event.target_touches.size());

  std::unique_ptr<blink::WebInputEvent> event_out(
      CreateWebInputEvent(pp_event));
  const blink::WebTouchEvent* touch_out =
      static_cast<const blink::WebTouchEvent*>(event_out.get());
  ASSERT_TRUE(touch_out);
  EXPECT_EQ(touch.GetType(), touch_out->GetType());
  ASSERT_EQ(touch.touches_length, touch_out->touches_length);
  CompareWebTouchEvents(touch, *touch_out);
}

TEST_F(EventConversionTest, TouchCancel) {
  SyntheticWebTouchEvent touch;
  touch.PressPoint(1.f, 2.f);
  touch.ResetPoints();
  touch.PressPoint(3.f, 4.f);
  touch.ResetPoints();
  touch.CancelPoint(1);
  touch.CancelPoint(0);

  std::vector<ppapi::InputEventData> pp_events;
  CreateInputEventData(touch, nullptr, &pp_events);
  ASSERT_EQ(1U, pp_events.size());

  const ppapi::InputEventData& pp_event = pp_events[0];
  ASSERT_EQ(PP_INPUTEVENT_TYPE_TOUCHCANCEL, pp_event.event_type);
  ASSERT_EQ(0U, pp_event.touches.size());
  ASSERT_EQ(2U, pp_event.changed_touches.size());
  ASSERT_EQ(2U, pp_event.target_touches.size());

  std::unique_ptr<blink::WebInputEvent> event_out(
      CreateWebInputEvent(pp_event));
  const blink::WebTouchEvent* touch_out =
      static_cast<const blink::WebTouchEvent*>(event_out.get());
  ASSERT_TRUE(touch_out);
  EXPECT_EQ(touch.GetType(), touch_out->GetType());
  EXPECT_EQ(touch.touches_length, touch_out->touches_length);
  CompareWebTouchEvents(touch, *touch_out);
}

TEST_F(EventConversionTest, MouseMove) {
  std::unique_ptr<gfx::PointF> last_mouse_position;
  blink::WebMouseEvent mouse_event = SyntheticWebMouseEventBuilder::Build(
      blink::WebInputEvent::kMouseMove, 100, 200, 0);

  std::vector<ppapi::InputEventData> pp_events;
  CreateInputEventData(mouse_event, &last_mouse_position, &pp_events);
  ASSERT_EQ(1U, pp_events.size());
  const ppapi::InputEventData& pp_event = pp_events[0];
  ASSERT_EQ(PP_INPUTEVENT_TYPE_MOUSEMOVE, pp_event.event_type);
  ASSERT_EQ(pp_event.mouse_position.x, mouse_event.PositionInWidget().x);
  ASSERT_EQ(pp_event.mouse_position.y, mouse_event.PositionInWidget().y);
  ASSERT_EQ(pp_event.mouse_movement.x, 0);
  ASSERT_EQ(pp_event.mouse_movement.y, 0);
  if (last_mouse_position) {
    ASSERT_EQ(*last_mouse_position.get(),
              gfx::PointF(mouse_event.PositionInScreen()));
  }

  mouse_event = SyntheticWebMouseEventBuilder::Build(
      blink::WebInputEvent::kMouseMove, 123, 188, 0);
  CreateInputEventData(mouse_event, &last_mouse_position, &pp_events);
  ASSERT_EQ(PP_INPUTEVENT_TYPE_MOUSEMOVE, pp_event.event_type);
  ASSERT_EQ(pp_event.mouse_position.x, mouse_event.PositionInWidget().x);
  ASSERT_EQ(pp_event.mouse_position.y, mouse_event.PositionInWidget().y);
  ASSERT_EQ(pp_event.mouse_movement.x, 23);
  ASSERT_EQ(pp_event.mouse_movement.y, -12);
}

}  // namespace content
