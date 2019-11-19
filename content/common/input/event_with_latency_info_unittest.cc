// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/event_with_latency_info.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_input_event.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using std::numeric_limits;

namespace content {
namespace {

using EventWithLatencyInfoTest = testing::Test;

TouchEventWithLatencyInfo CreateTouchEvent(WebInputEvent::Type type,
                                           double timestamp,
                                           unsigned touch_count = 1) {
  TouchEventWithLatencyInfo touch(
      type, WebInputEvent::kNoModifiers,
      base::TimeTicks() + base::TimeDelta::FromSecondsD(timestamp),
      ui::LatencyInfo());
  touch.event.touches_length = touch_count;
  return touch;
}

MouseEventWithLatencyInfo CreateMouseEvent(WebInputEvent::Type type,
                                           double timestamp) {
  return MouseEventWithLatencyInfo(
      type, WebInputEvent::kNoModifiers,
      base::TimeTicks() + base::TimeDelta::FromSecondsD(timestamp),
      ui::LatencyInfo());
}

MouseWheelEventWithLatencyInfo CreateMouseWheelEvent(
    double timestamp,
    float deltaX = 0.0f,
    float deltaY = 0.0f,
    int modifiers = WebInputEvent::kNoModifiers) {
  MouseWheelEventWithLatencyInfo mouse_wheel(
      WebInputEvent::kMouseWheel, modifiers,
      base::TimeTicks() + base::TimeDelta::FromSecondsD(timestamp),
      ui::LatencyInfo());
  mouse_wheel.event.delta_x = deltaX;
  mouse_wheel.event.delta_y = deltaY;
  return mouse_wheel;
}

GestureEventWithLatencyInfo CreateGestureEvent(WebInputEvent::Type type,
                                               double timestamp,
                                               float x = 0.0f,
                                               float y = 0.0f) {
  GestureEventWithLatencyInfo gesture(
      type, WebInputEvent::kNoModifiers,
      base::TimeTicks() + base::TimeDelta::FromSecondsD(timestamp),
      ui::LatencyInfo());
  gesture.event.SetPositionInWidget(gfx::PointF(x, y));
  return gesture;
}

TEST_F(EventWithLatencyInfoTest, TimestampCoalescingForMouseEvent) {
  MouseEventWithLatencyInfo mouse_0 =
      CreateMouseEvent(WebInputEvent::kMouseMove, 5.0);
  MouseEventWithLatencyInfo mouse_1 =
      CreateMouseEvent(WebInputEvent::kMouseMove, 10.0);

  ASSERT_TRUE(mouse_0.CanCoalesceWith(mouse_1));
  mouse_0.CoalesceWith(mouse_1);
  // Coalescing WebMouseEvent preserves newer timestamp.
  EXPECT_EQ(10.0, mouse_0.event.TimeStamp().since_origin().InSecondsF());
}

TEST_F(EventWithLatencyInfoTest, TimestampCoalescingForMouseWheelEvent) {
  MouseWheelEventWithLatencyInfo mouse_wheel_0 = CreateMouseWheelEvent(5.0);
  MouseWheelEventWithLatencyInfo mouse_wheel_1 = CreateMouseWheelEvent(10.0);

  ASSERT_TRUE(mouse_wheel_0.CanCoalesceWith(mouse_wheel_1));
  mouse_wheel_0.CoalesceWith(mouse_wheel_1);
  // Coalescing WebMouseWheelEvent preserves newer timestamp.
  EXPECT_EQ(10.0, mouse_wheel_0.event.TimeStamp().since_origin().InSecondsF());
}

TEST_F(EventWithLatencyInfoTest, TimestampCoalescingForTouchEvent) {
  TouchEventWithLatencyInfo touch_0 =
      CreateTouchEvent(WebInputEvent::kTouchMove, 5.0);
  TouchEventWithLatencyInfo touch_1 =
      CreateTouchEvent(WebInputEvent::kTouchMove, 10.0);

  ASSERT_TRUE(touch_0.CanCoalesceWith(touch_1));
  touch_0.CoalesceWith(touch_1);
  // Coalescing WebTouchEvent preserves newer timestamp.
  EXPECT_EQ(10.0, touch_0.event.TimeStamp().since_origin().InSecondsF());
}

TEST_F(EventWithLatencyInfoTest, TimestampCoalescingForGestureEvent) {
  GestureEventWithLatencyInfo scroll_0 =
      CreateGestureEvent(WebInputEvent::kGestureScrollUpdate, 5.0);
  GestureEventWithLatencyInfo scroll_1 =
      CreateGestureEvent(WebInputEvent::kGestureScrollUpdate, 10.0);

  ASSERT_TRUE(scroll_0.CanCoalesceWith(scroll_1));
  scroll_0.CoalesceWith(scroll_1);
  // Coalescing WebGestureEvent preserves newer timestamp.
  EXPECT_EQ(10.0, scroll_0.event.TimeStamp().since_origin().InSecondsF());
}

TEST_F(EventWithLatencyInfoTest, LatencyInfoCoalescing) {
  MouseEventWithLatencyInfo mouse_0 =
      CreateMouseEvent(WebInputEvent::kMouseMove, 5.0);
  mouse_0.latency.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, base::TimeTicks());
  MouseEventWithLatencyInfo mouse_1 =
      CreateMouseEvent(WebInputEvent::kMouseMove, 10.0);

  ASSERT_TRUE(mouse_0.CanCoalesceWith(mouse_1));

  EXPECT_FALSE(mouse_1.latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, nullptr));

  mouse_0.CoalesceWith(mouse_1);

  // Coalescing WebMouseEvent preservers older LatencyInfo.
  EXPECT_TRUE(mouse_1.latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, nullptr));
}

WebTouchPoint CreateTouchPoint(WebTouchPoint::State state, int id) {
  WebTouchPoint touch;
  touch.state = state;
  touch.id = id;
  return touch;
}

TouchEventWithLatencyInfo CreateTouch(WebInputEvent::Type type,
                                      unsigned touch_count = 1) {
  return CreateTouchEvent(type, 0.0, touch_count);
}

GestureEventWithLatencyInfo CreateGesture(WebInputEvent::Type type,
                                          float x,
                                          float y) {
  return CreateGestureEvent(type, 0.0, x, y);
}

MouseWheelEventWithLatencyInfo CreateMouseWheel(float deltaX, float deltaY) {
  return CreateMouseWheelEvent(0.0, deltaX, deltaY);
}

template <class T>
bool CanCoalesce(const T& event_to_coalesce, const T& event) {
  return event.CanCoalesceWith(event_to_coalesce);
}

template <class T>
void Coalesce(const T& event_to_coalesce, T* event) {
  return event->CoalesceWith(event_to_coalesce);
}

TEST_F(EventWithLatencyInfoTest, TouchEventCoalescing) {
  TouchEventWithLatencyInfo touch0 = CreateTouch(WebInputEvent::kTouchStart);
  TouchEventWithLatencyInfo touch1 = CreateTouch(WebInputEvent::kTouchMove);

  // Non touch-moves won't coalesce.
  EXPECT_FALSE(CanCoalesce(touch0, touch0));

  // Touches of different types won't coalesce.
  EXPECT_FALSE(CanCoalesce(touch0, touch1));

  // Touch moves with idential touch lengths and touch ids should coalesce.
  EXPECT_TRUE(CanCoalesce(touch1, touch1));

  // Touch moves with different touch ids should not coalesce.
  touch0 = CreateTouch(WebInputEvent::kTouchMove);
  touch1 = CreateTouch(WebInputEvent::kTouchMove);
  touch0.event.touches[0].id = 7;
  EXPECT_FALSE(CanCoalesce(touch0, touch1));
  touch0 = CreateTouch(WebInputEvent::kTouchMove, 2);
  touch1 = CreateTouch(WebInputEvent::kTouchMove, 2);
  touch0.event.touches[0].id = 1;
  touch1.event.touches[0].id = 0;
  EXPECT_FALSE(CanCoalesce(touch0, touch1));

  // Touch moves with different touch lengths should not coalesce.
  touch0 = CreateTouch(WebInputEvent::kTouchMove, 1);
  touch1 = CreateTouch(WebInputEvent::kTouchMove, 2);
  EXPECT_FALSE(CanCoalesce(touch0, touch1));

  // Touch moves with identical touch ids in different orders should coalesce.
  touch0 = CreateTouch(WebInputEvent::kTouchMove, 2);
  touch1 = CreateTouch(WebInputEvent::kTouchMove, 2);
  touch0.event.touches[0] = touch1.event.touches[1] =
      CreateTouchPoint(WebTouchPoint::kStateMoved, 1);
  touch0.event.touches[1] = touch1.event.touches[0] =
      CreateTouchPoint(WebTouchPoint::kStateMoved, 0);
  EXPECT_TRUE(CanCoalesce(touch0, touch1));

  // Pointers with the same ID's should coalesce.
  touch0 = CreateTouch(WebInputEvent::kTouchMove, 2);
  touch1 = CreateTouch(WebInputEvent::kTouchMove, 2);
  touch0.event.touches[0] = touch1.event.touches[1] =
      CreateTouchPoint(WebTouchPoint::kStateMoved, 1);
  Coalesce(touch0, &touch1);
  ASSERT_EQ(1, touch1.event.touches[0].id);
  ASSERT_EQ(0, touch1.event.touches[1].id);
  EXPECT_EQ(WebTouchPoint::kStateUndefined, touch1.event.touches[1].state);
  EXPECT_EQ(WebTouchPoint::kStateMoved, touch1.event.touches[0].state);

  // Movement from now-stationary pointers should be preserved.
  touch0 = touch1 = CreateTouch(WebInputEvent::kTouchMove, 2);
  touch0.event.touches[0] = CreateTouchPoint(WebTouchPoint::kStateMoved, 1);
  touch1.event.touches[1] =
      CreateTouchPoint(WebTouchPoint::kStateStationary, 1);
  touch0.event.touches[1] =
      CreateTouchPoint(WebTouchPoint::kStateStationary, 0);
  touch1.event.touches[0] = CreateTouchPoint(WebTouchPoint::kStateMoved, 0);
  Coalesce(touch0, &touch1);
  ASSERT_EQ(1, touch1.event.touches[0].id);
  ASSERT_EQ(0, touch1.event.touches[1].id);
  EXPECT_EQ(WebTouchPoint::kStateMoved, touch1.event.touches[0].state);
  EXPECT_EQ(WebTouchPoint::kStateMoved, touch1.event.touches[1].state);

  // Touch moves with different dispatchTypes coalesce.
  touch0 = CreateTouch(WebInputEvent::kTouchMove, 2);
  touch0.event.dispatch_type = WebInputEvent::DispatchType::kBlocking;
  touch1 = CreateTouch(WebInputEvent::kTouchMove, 2);
  touch1.event.dispatch_type = WebInputEvent::DispatchType::kEventNonBlocking;
  touch0.event.touches[0] = touch1.event.touches[1] =
      CreateTouchPoint(WebTouchPoint::kStateMoved, 1);
  touch0.event.touches[1] = touch1.event.touches[0] =
      CreateTouchPoint(WebTouchPoint::kStateMoved, 0);
  EXPECT_TRUE(CanCoalesce(touch0, touch1));
  Coalesce(touch0, &touch1);
  ASSERT_EQ(WebInputEvent::DispatchType::kBlocking, touch1.event.dispatch_type);

  touch0 = CreateTouch(WebInputEvent::kTouchMove, 2);
  touch0.event.dispatch_type =
      WebInputEvent::DispatchType::kListenersForcedNonBlockingDueToFling;
  touch1 = CreateTouch(WebInputEvent::kTouchMove, 2);
  touch1.event.dispatch_type =
      WebInputEvent::DispatchType::kListenersNonBlockingPassive;
  touch0.event.touches[0] = touch1.event.touches[1] =
      CreateTouchPoint(WebTouchPoint::kStateMoved, 1);
  touch0.event.touches[1] = touch1.event.touches[0] =
      CreateTouchPoint(WebTouchPoint::kStateMoved, 0);
  EXPECT_TRUE(CanCoalesce(touch0, touch1));
  Coalesce(touch0, &touch1);
  ASSERT_EQ(WebInputEvent::DispatchType::kListenersNonBlockingPassive,
            touch1.event.dispatch_type);
}

TEST_F(EventWithLatencyInfoTest, PinchEventCoalescing) {
  GestureEventWithLatencyInfo pinch0 =
      CreateGesture(WebInputEvent::kGesturePinchBegin, 1, 1);
  GestureEventWithLatencyInfo pinch1 =
      CreateGesture(WebInputEvent::kGesturePinchUpdate, 2, 2);

  // Only GesturePinchUpdate's coalesce.
  EXPECT_FALSE(CanCoalesce(pinch0, pinch0));

  // Pinch gestures of different types should not coalesce.
  EXPECT_FALSE(CanCoalesce(pinch0, pinch1));

  // Pinches with different focal points should not coalesce.
  pinch0 = CreateGesture(WebInputEvent::kGesturePinchUpdate, 1, 1);
  pinch1 = CreateGesture(WebInputEvent::kGesturePinchUpdate, 2, 2);
  EXPECT_FALSE(CanCoalesce(pinch0, pinch1));
  EXPECT_TRUE(CanCoalesce(pinch0, pinch0));

  // Coalesced scales are multiplicative.
  pinch0 = CreateGesture(WebInputEvent::kGesturePinchUpdate, 1, 1);
  pinch0.event.data.pinch_update.scale = 2.f;
  pinch1 = CreateGesture(WebInputEvent::kGesturePinchUpdate, 1, 1);
  pinch1.event.data.pinch_update.scale = 3.f;
  EXPECT_TRUE(CanCoalesce(pinch0, pinch0));
  Coalesce(pinch0, &pinch1);
  EXPECT_EQ(2.f * 3.f, pinch1.event.data.pinch_update.scale);

  // Scales have a minimum value and can never reach 0.
  ASSERT_GT(numeric_limits<float>::min(), 0);
  pinch0 = CreateGesture(WebInputEvent::kGesturePinchUpdate, 1, 1);
  pinch0.event.data.pinch_update.scale = numeric_limits<float>::min() * 2.0f;
  pinch1 = CreateGesture(WebInputEvent::kGesturePinchUpdate, 1, 1);
  pinch1.event.data.pinch_update.scale = numeric_limits<float>::min() * 5.0f;
  EXPECT_TRUE(CanCoalesce(pinch0, pinch1));
  Coalesce(pinch0, &pinch1);
  EXPECT_EQ(numeric_limits<float>::min(), pinch1.event.data.pinch_update.scale);

  // Scales have a maximum value and can never reach Infinity.
  pinch0 = CreateGesture(WebInputEvent::kGesturePinchUpdate, 1, 1);
  pinch0.event.data.pinch_update.scale = numeric_limits<float>::max() / 2.0f;
  pinch1 = CreateGesture(WebInputEvent::kGesturePinchUpdate, 1, 1);
  pinch1.event.data.pinch_update.scale = 10.0f;
  EXPECT_TRUE(CanCoalesce(pinch0, pinch1));
  Coalesce(pinch0, &pinch1);
  EXPECT_EQ(numeric_limits<float>::max(), pinch1.event.data.pinch_update.scale);
}

TEST_F(EventWithLatencyInfoTest, WebMouseWheelEventCoalescing) {
  MouseWheelEventWithLatencyInfo mouse_wheel_0 = CreateMouseWheel(1, 1);
  MouseWheelEventWithLatencyInfo mouse_wheel_1 = CreateMouseWheel(2, 2);
  // WebMouseWheelEvent objects with same values except different deltaX and
  // deltaY should coalesce.
  EXPECT_TRUE(CanCoalesce(mouse_wheel_0, mouse_wheel_1));

  // WebMouseWheelEvent objects with different modifiers should not coalesce.
  mouse_wheel_0 = CreateMouseWheelEvent(2.0, 1, 1, WebInputEvent::kControlKey);
  mouse_wheel_1 = CreateMouseWheelEvent(2.0, 1, 1, WebInputEvent::kShiftKey);
  EXPECT_FALSE(CanCoalesce(mouse_wheel_0, mouse_wheel_1));

  // Coalesce old and new events.
  mouse_wheel_0 = CreateMouseWheel(1, 1);
  mouse_wheel_0.event.SetPositionInWidget(1, 1);
  mouse_wheel_1 = CreateMouseWheel(2, 2);
  mouse_wheel_1.event.SetPositionInWidget(2, 2);
  MouseWheelEventWithLatencyInfo mouse_wheel_1_copy = mouse_wheel_1;
  EXPECT_TRUE(CanCoalesce(mouse_wheel_0, mouse_wheel_1));
  EXPECT_EQ(mouse_wheel_0.event.GetModifiers(),
            mouse_wheel_1.event.GetModifiers());
  EXPECT_EQ(mouse_wheel_0.event.delta_units, mouse_wheel_1.event.delta_units);
  EXPECT_EQ(mouse_wheel_0.event.phase, mouse_wheel_1.event.phase);
  EXPECT_EQ(mouse_wheel_0.event.momentum_phase,
            mouse_wheel_1.event.momentum_phase);
  Coalesce(mouse_wheel_0, &mouse_wheel_1);

  // Coalesced event has the position of the most recent event.
  EXPECT_EQ(1, mouse_wheel_1.event.PositionInWidget().x);
  EXPECT_EQ(1, mouse_wheel_1.event.PositionInWidget().y);

  // deltaX/Y, wheelTicksX/Y, and movementX/Y of the coalesced event are
  // calculated properly.
  EXPECT_EQ(mouse_wheel_1_copy.event.delta_x + mouse_wheel_0.event.delta_x,
            mouse_wheel_1.event.delta_x);
  EXPECT_EQ(mouse_wheel_1_copy.event.delta_y + mouse_wheel_0.event.delta_y,
            mouse_wheel_1.event.delta_y);
  EXPECT_EQ(mouse_wheel_1_copy.event.wheel_ticks_x +
                mouse_wheel_0.event.wheel_ticks_x,
            mouse_wheel_1.event.wheel_ticks_x);
  EXPECT_EQ(mouse_wheel_1_copy.event.wheel_ticks_y +
                mouse_wheel_0.event.wheel_ticks_y,
            mouse_wheel_1.event.wheel_ticks_y);
  EXPECT_EQ(
      mouse_wheel_1_copy.event.movement_x + mouse_wheel_0.event.movement_x,
      mouse_wheel_1.event.movement_x);
  EXPECT_EQ(
      mouse_wheel_1_copy.event.movement_y + mouse_wheel_0.event.movement_y,
      mouse_wheel_1.event.movement_y);
}

// Coalescing preserves the newer timestamp.
TEST_F(EventWithLatencyInfoTest, TimestampCoalescing) {
  MouseWheelEventWithLatencyInfo mouse_wheel_0 =
      CreateMouseWheelEvent(5.0, 1, 1);
  MouseWheelEventWithLatencyInfo mouse_wheel_1 =
      CreateMouseWheelEvent(10.0, 2, 2);

  EXPECT_TRUE(CanCoalesce(mouse_wheel_0, mouse_wheel_1));
  Coalesce(mouse_wheel_1, &mouse_wheel_0);
  EXPECT_EQ(10.0, mouse_wheel_0.event.TimeStamp().since_origin().InSecondsF());
}

}  // namespace
}  // namespace content
