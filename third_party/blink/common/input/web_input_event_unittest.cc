// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/input/web_input_event.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_pointer_event.h"

namespace blink {

namespace {

WebMouseEvent CreateWebMouseMoveEvent() {
  WebMouseEvent mouse_event;
  mouse_event.SetType(WebInputEvent::Type::kMouseMove);
  mouse_event.id = 1;
  mouse_event.pointer_type = WebPointerProperties::PointerType::kMouse;
  return mouse_event;
}

WebPointerEvent CreateWebPointerMoveEvent() {
  WebPointerEvent pointer_event;
  pointer_event.SetType(WebInputEvent::Type::kPointerMove);
  pointer_event.id = 1;
  pointer_event.pointer_type = WebPointerProperties::PointerType::kMouse;
  return pointer_event;
}

WebTouchEvent CreateWebTouchMoveEvent() {
  WebTouchPoint touch_point;
  touch_point.id = 1;
  touch_point.state = WebTouchPoint::State::kStateMoved;
  touch_point.pointer_type = WebPointerProperties::PointerType::kTouch;

  WebTouchEvent touch_event;
  touch_event.SetType(WebInputEvent::Type::kTouchMove);
  touch_event.touches[touch_event.touches_length++] = touch_point;
  return touch_event;
}

}  // namespace

TEST(WebInputEventTest, TouchEventCoalescing) {
  WebTouchEvent coalesced_event = CreateWebTouchMoveEvent();
  coalesced_event.SetType(WebInputEvent::Type::kTouchMove);
  coalesced_event.touches[0].movement_x = 5;
  coalesced_event.touches[0].movement_y = 10;

  WebTouchEvent event_to_be_coalesced = CreateWebTouchMoveEvent();
  event_to_be_coalesced.touches[0].movement_x = 3;
  event_to_be_coalesced.touches[0].movement_y = -4;

  EXPECT_TRUE(coalesced_event.CanCoalesce(event_to_be_coalesced));
  coalesced_event.Coalesce(event_to_be_coalesced);
  EXPECT_EQ(8, coalesced_event.touches[0].movement_x);
  EXPECT_EQ(6, coalesced_event.touches[0].movement_y);

  coalesced_event.touches[0].pointer_type =
      WebPointerProperties::PointerType::kPen;
  EXPECT_FALSE(coalesced_event.CanCoalesce(event_to_be_coalesced));

  coalesced_event = CreateWebTouchMoveEvent();
  event_to_be_coalesced = CreateWebTouchMoveEvent();
  event_to_be_coalesced.SetModifiers(WebInputEvent::kControlKey);
  EXPECT_FALSE(coalesced_event.CanCoalesce(event_to_be_coalesced));
}

TEST(WebInputEventTest, WebMouseWheelEventCoalescing) {
  WebMouseWheelEvent coalesced_event(
      WebInputEvent::Type::kMouseWheel, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests());
  coalesced_event.delta_x = 1;
  coalesced_event.delta_y = 1;

  WebMouseWheelEvent event_to_be_coalesced(
      WebInputEvent::Type::kMouseWheel, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests());
  event_to_be_coalesced.delta_x = 3;
  event_to_be_coalesced.delta_y = 4;

  EXPECT_TRUE(coalesced_event.CanCoalesce(event_to_be_coalesced));
  coalesced_event.Coalesce(event_to_be_coalesced);
  EXPECT_EQ(4, coalesced_event.delta_x);
  EXPECT_EQ(5, coalesced_event.delta_y);

  event_to_be_coalesced.phase = WebMouseWheelEvent::kPhaseBegan;
  coalesced_event.phase = WebMouseWheelEvent::kPhaseEnded;
  EXPECT_FALSE(coalesced_event.CanCoalesce(event_to_be_coalesced));

  // With timer based wheel scroll latching, we break the latching sequence on
  // direction change when all prior GSU events in the current sequence are
  // ignored. To do so we dispatch the pending wheel event with phaseEnded and
  // the first wheel event in the opposite direction will have phaseBegan. The
  // GSB generated from this wheel event will cause a new hittesting. To make
  // sure that a GSB will actually get created we should not coalesce the wheel
  // event with synthetic kPhaseBegan to one with synthetic kPhaseEnded.
  event_to_be_coalesced.has_synthetic_phase = true;
  coalesced_event.has_synthetic_phase = true;
  EXPECT_FALSE(coalesced_event.CanCoalesce(event_to_be_coalesced));

  event_to_be_coalesced.phase = WebMouseWheelEvent::kPhaseChanged;
  coalesced_event.phase = WebMouseWheelEvent::kPhaseBegan;
  EXPECT_TRUE(coalesced_event.CanCoalesce(event_to_be_coalesced));
  coalesced_event.Coalesce(event_to_be_coalesced);
  EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, coalesced_event.phase);
  EXPECT_EQ(7, coalesced_event.delta_x);
  EXPECT_EQ(9, coalesced_event.delta_y);
}

TEST(WebInputEventTest, WebGestureEventCoalescing) {
  WebGestureEvent coalesced_event(WebInputEvent::Type::kGestureScrollUpdate,
                                  WebInputEvent::kNoModifiers,
                                  WebInputEvent::GetStaticTimeStampForTests());
  coalesced_event.data.scroll_update.delta_x = 1;
  coalesced_event.data.scroll_update.delta_y = 1;

  WebGestureEvent event_to_be_coalesced(
      WebInputEvent::Type::kGestureScrollUpdate, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests());
  event_to_be_coalesced.data.scroll_update.delta_x = 3;
  event_to_be_coalesced.data.scroll_update.delta_y = 4;

  EXPECT_TRUE(coalesced_event.CanCoalesce(event_to_be_coalesced));
  coalesced_event.Coalesce(event_to_be_coalesced);
  EXPECT_EQ(4, coalesced_event.data.scroll_update.delta_x);
  EXPECT_EQ(5, coalesced_event.data.scroll_update.delta_y);
}

TEST(WebInputEventTest, GesturePinchUpdateCoalescing) {
  gfx::PointF position(10.f, 10.f);
  WebGestureEvent coalesced_event(
      WebInputEvent::Type::kGesturePinchUpdate, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(), WebGestureDevice::kTouchpad);
  coalesced_event.data.pinch_update.scale = 1.1f;
  coalesced_event.SetPositionInWidget(position);

  WebGestureEvent event_to_be_coalesced(coalesced_event);

  ASSERT_TRUE(coalesced_event.CanCoalesce(event_to_be_coalesced));
  coalesced_event.Coalesce(event_to_be_coalesced);
  EXPECT_FLOAT_EQ(1.21, coalesced_event.data.pinch_update.scale);

  // Allow the updates to be coalesced if the anchors are nearly equal.
  position.Offset(0.1f, 0.1f);
  event_to_be_coalesced.SetPositionInWidget(position);
  coalesced_event.data.pinch_update.scale = 1.1f;
  ASSERT_TRUE(coalesced_event.CanCoalesce(event_to_be_coalesced));
  coalesced_event.Coalesce(event_to_be_coalesced);
  EXPECT_FLOAT_EQ(1.21, coalesced_event.data.pinch_update.scale);

  // The anchors are no longer considered equal, so don't coalesce.
  position.Offset(1.f, 1.f);
  event_to_be_coalesced.SetPositionInWidget(position);
  EXPECT_FALSE(coalesced_event.CanCoalesce(event_to_be_coalesced));

  // Don't logically coalesce touchpad pinch events as touchpad pinch events
  // don't occur within a gesture scroll sequence.
  EXPECT_FALSE(WebGestureEvent::IsCompatibleScrollorPinch(event_to_be_coalesced,
                                                          coalesced_event));

  // Touchscreen pinch events can be logically coalesced.
  coalesced_event.SetSourceDevice(WebGestureDevice::kTouchscreen);
  event_to_be_coalesced.SetSourceDevice(WebGestureDevice::kTouchscreen);
  coalesced_event.data.pinch_update.scale = 1.1f;
  ASSERT_TRUE(WebGestureEvent::IsCompatibleScrollorPinch(event_to_be_coalesced,
                                                         coalesced_event));

  std::unique_ptr<WebGestureEvent> logical_scroll, logical_pinch;
  std::tie(logical_scroll, logical_pinch) =
      WebGestureEvent::CoalesceScrollAndPinch(nullptr, coalesced_event,
                                              event_to_be_coalesced);
  ASSERT_NE(nullptr, logical_scroll);
  ASSERT_NE(nullptr, logical_pinch);
  ASSERT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            logical_scroll->GetType());
  ASSERT_EQ(WebInputEvent::Type::kGesturePinchUpdate, logical_pinch->GetType());
  EXPECT_FLOAT_EQ(1.21, logical_pinch->data.pinch_update.scale);
}

TEST(WebInputEventTest, MouseEventCoalescing) {
  WebMouseEvent coalesced_event = CreateWebMouseMoveEvent();
  WebMouseEvent event_to_be_coalesced = CreateWebMouseMoveEvent();
  EXPECT_TRUE(coalesced_event.CanCoalesce(event_to_be_coalesced));

  // Test coalescing movements.
  coalesced_event.movement_x = 5;
  coalesced_event.movement_y = 10;

  event_to_be_coalesced.movement_x = 3;
  event_to_be_coalesced.movement_y = -4;
  EXPECT_TRUE(coalesced_event.CanCoalesce(event_to_be_coalesced));
  coalesced_event.Coalesce(event_to_be_coalesced);
  EXPECT_EQ(8, coalesced_event.movement_x);
  EXPECT_EQ(6, coalesced_event.movement_y);

  // Test id.
  coalesced_event = CreateWebMouseMoveEvent();
  event_to_be_coalesced = CreateWebMouseMoveEvent();
  event_to_be_coalesced.id = 3;
  EXPECT_FALSE(coalesced_event.CanCoalesce(event_to_be_coalesced));

  // Test pointer_type.
  coalesced_event = CreateWebMouseMoveEvent();
  event_to_be_coalesced = CreateWebMouseMoveEvent();
  event_to_be_coalesced.pointer_type = WebPointerProperties::PointerType::kPen;
  EXPECT_FALSE(coalesced_event.CanCoalesce(event_to_be_coalesced));

  // Test modifiers
  coalesced_event = CreateWebMouseMoveEvent();
  event_to_be_coalesced = CreateWebMouseMoveEvent();
  event_to_be_coalesced.SetModifiers(WebInputEvent::kControlKey);
  EXPECT_FALSE(coalesced_event.CanCoalesce(event_to_be_coalesced));
}

TEST(WebInputEventTest, PointerEventCoalescing) {
  WebPointerEvent coalesced_event = CreateWebPointerMoveEvent();
  WebPointerEvent event_to_be_coalesced = CreateWebPointerMoveEvent();
  EXPECT_TRUE(coalesced_event.CanCoalesce(event_to_be_coalesced));

  // Test coalescing movements.
  coalesced_event.movement_x = 5;
  coalesced_event.movement_y = 10;

  event_to_be_coalesced.movement_x = 3;
  event_to_be_coalesced.movement_y = -4;
  EXPECT_TRUE(coalesced_event.CanCoalesce(event_to_be_coalesced));
  coalesced_event.Coalesce(event_to_be_coalesced);
  EXPECT_EQ(8, coalesced_event.movement_x);
  EXPECT_EQ(6, coalesced_event.movement_y);

  // Test id.
  coalesced_event = CreateWebPointerMoveEvent();
  event_to_be_coalesced = CreateWebPointerMoveEvent();
  event_to_be_coalesced.id = 3;
  EXPECT_FALSE(coalesced_event.CanCoalesce(event_to_be_coalesced));

  // Test pointer_type.
  coalesced_event = CreateWebPointerMoveEvent();
  event_to_be_coalesced = CreateWebPointerMoveEvent();
  event_to_be_coalesced.pointer_type = WebPointerProperties::PointerType::kPen;
  EXPECT_FALSE(coalesced_event.CanCoalesce(event_to_be_coalesced));

  // Test modifiers
  coalesced_event = CreateWebPointerMoveEvent();
  event_to_be_coalesced = CreateWebPointerMoveEvent();
  event_to_be_coalesced.SetModifiers(WebInputEvent::kControlKey);
  EXPECT_FALSE(coalesced_event.CanCoalesce(event_to_be_coalesced));
}

}  // namespace blink
