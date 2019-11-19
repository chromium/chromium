// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_web_input_event_builders.h"

#include "base/logging.h"
#include "content/common/input/web_mouse_wheel_event_traits.h"
#include "content/common/input/web_touch_event_traits.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace content {

using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebGestureEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

WebMouseEvent SyntheticWebMouseEventBuilder::Build(
    blink::WebInputEvent::Type type) {
  return WebMouseEvent(type, WebInputEvent::kNoModifiers,
                       ui::EventTimeForNow());
}

WebMouseEvent SyntheticWebMouseEventBuilder::Build(
    blink::WebInputEvent::Type type,
    float window_x,
    float window_y,
    int modifiers,
    blink::WebPointerProperties::PointerType pointer_type) {
  DCHECK(WebInputEvent::IsMouseEventType(type));
  WebMouseEvent result(type, modifiers, ui::EventTimeForNow());
  result.SetPositionInWidget(window_x, window_y);
  result.SetPositionInScreen(window_x, window_y);
  result.SetModifiers(modifiers);
  result.pointer_type = pointer_type;
  result.id = ui::kPointerIdMouse;
  return result;
}

WebMouseWheelEvent SyntheticWebMouseWheelEventBuilder::Build(
    WebMouseWheelEvent::Phase phase) {
  WebMouseWheelEvent result(WebInputEvent::kMouseWheel,
                            WebInputEvent::kNoModifiers, ui::EventTimeForNow());
  result.phase = phase;
  result.event_action = WebMouseWheelEventTraits::GetEventAction(result);
  return result;
}

WebMouseWheelEvent SyntheticWebMouseWheelEventBuilder::Build(
    float x,
    float y,
    float dx,
    float dy,
    int modifiers,
    ui::input_types::ScrollGranularity delta_units) {
  return Build(x, y, 0, 0, dx, dy, modifiers, delta_units);
}

WebMouseWheelEvent SyntheticWebMouseWheelEventBuilder::Build(
    float x,
    float y,
    float global_x,
    float global_y,
    float dx,
    float dy,
    int modifiers,
    ui::input_types::ScrollGranularity delta_units) {
  WebMouseWheelEvent result(WebInputEvent::kMouseWheel, modifiers,
                            ui::EventTimeForNow());
  result.SetPositionInScreen(global_x, global_y);
  result.SetPositionInWidget(x, y);
  result.delta_units = delta_units;
  result.delta_x = dx;
  result.delta_y = dy;
  if (dx)
    result.wheel_ticks_x = dx > 0.0f ? 1.0f : -1.0f;
  if (dy)
    result.wheel_ticks_y = dy > 0.0f ? 1.0f : -1.0f;

  result.event_action = WebMouseWheelEventTraits::GetEventAction(result);
  return result;
}

WebKeyboardEvent SyntheticWebKeyboardEventBuilder::Build(
    WebInputEvent::Type type) {
  DCHECK(WebInputEvent::IsKeyboardEventType(type));
  WebKeyboardEvent result(type, WebInputEvent::kNoModifiers,
                          ui::EventTimeForNow());
  result.windows_key_code = ui::VKEY_L;  // non-null made up value.
  return result;
}

WebGestureEvent SyntheticWebGestureEventBuilder::Build(
    WebInputEvent::Type type,
    blink::WebGestureDevice source_device,
    int modifiers) {
  DCHECK(WebInputEvent::IsGestureEventType(type));
  WebGestureEvent result(type, modifiers, ui::EventTimeForNow(), source_device);
  if (type == WebInputEvent::kGestureTap ||
      type == WebInputEvent::kGestureTapUnconfirmed ||
      type == WebInputEvent::kGestureDoubleTap) {
    result.data.tap.tap_count = 1;
    result.data.tap.width = 10;
    result.data.tap.height = 10;
  }

  result.SetNeedsWheelEvent(result.IsTouchpadZoomEvent());

  return result;
}

WebGestureEvent SyntheticWebGestureEventBuilder::BuildScrollBegin(
    float dx_hint,
    float dy_hint,
    blink::WebGestureDevice source_device,
    int pointer_count) {
  WebGestureEvent result =
      Build(WebInputEvent::kGestureScrollBegin, source_device);
  result.data.scroll_begin.delta_x_hint = dx_hint;
  result.data.scroll_begin.delta_y_hint = dy_hint;
  result.data.scroll_begin.pointer_count = pointer_count;
  return result;
}

WebGestureEvent SyntheticWebGestureEventBuilder::BuildScrollUpdate(
    float dx,
    float dy,
    int modifiers,
    blink::WebGestureDevice source_device) {
  WebGestureEvent result =
      Build(WebInputEvent::kGestureScrollUpdate, source_device, modifiers);
  result.data.scroll_update.delta_x = dx;
  result.data.scroll_update.delta_y = dy;
  return result;
}

WebGestureEvent SyntheticWebGestureEventBuilder::BuildPinchUpdate(
    float scale,
    float anchor_x,
    float anchor_y,
    int modifiers,
    blink::WebGestureDevice source_device) {
  WebGestureEvent result =
      Build(WebInputEvent::kGesturePinchUpdate, source_device, modifiers);
  result.data.pinch_update.scale = scale;
  result.SetPositionInWidget(blink::WebFloatPoint(anchor_x, anchor_y));
  result.SetPositionInScreen(blink::WebFloatPoint(anchor_x, anchor_y));
  return result;
}

WebGestureEvent SyntheticWebGestureEventBuilder::BuildFling(
    float velocity_x,
    float velocity_y,
    blink::WebGestureDevice source_device) {
  WebGestureEvent result =
      Build(WebInputEvent::kGestureFlingStart, source_device);
  result.data.fling_start.velocity_x = velocity_x;
  result.data.fling_start.velocity_y = velocity_y;
  return result;
}

SyntheticWebTouchEvent::SyntheticWebTouchEvent() : WebTouchEvent() {
  unique_touch_event_id = ui::GetNextTouchEventId();
  SetTimestamp(ui::EventTimeForNow());
  pointer_id_ = 0;
}

void SyntheticWebTouchEvent::ResetPoints() {
  int activePointCount = 0;
  unsigned count = 0;
  for (unsigned int i = 0; i < kTouchesLengthCap; ++i) {
    switch (touches[i].state) {
      case WebTouchPoint::kStatePressed:
      case WebTouchPoint::kStateMoved:
      case WebTouchPoint::kStateStationary:
        touches[i].state = WebTouchPoint::kStateStationary;
        ++activePointCount;
        ++count;
        break;
      case WebTouchPoint::kStateReleased:
      case WebTouchPoint::kStateCancelled:
        touches[i] = WebTouchPoint();
        ++count;
        break;
      case WebTouchPoint::kStateUndefined:
        break;
    }
    if (count >= touches_length)
      break;
  }
  touches_length = activePointCount;
  type_ = WebInputEvent::kUndefined;
  moved_beyond_slop_region = false;
  unique_touch_event_id = ui::GetNextTouchEventId();
}

int SyntheticWebTouchEvent::PressPoint(float x,
                                       float y,
                                       float radius_x,
                                       float radius_y,
                                       float rotation_angle,
                                       float force) {
  int index = FirstFreeIndex();
  if (index == -1)
    return -1;
  WebTouchPoint& point = touches[index];
  point.id = pointer_id_++;
  point.SetPositionInWidget(x, y);
  point.SetPositionInScreen(x, y);
  point.state = WebTouchPoint::kStatePressed;
  point.radius_x = radius_x;
  point.radius_y = radius_y;
  point.rotation_angle = rotation_angle;
  point.force = force;
  point.tilt_x = point.tilt_y = 0;
  point.pointer_type = blink::WebPointerProperties::PointerType::kTouch;
  ++touches_length;
  WebTouchEventTraits::ResetType(WebInputEvent::kTouchStart, TimeStamp(), this);
  return index;
}

void SyntheticWebTouchEvent::MovePoint(int index,
                                       float x,
                                       float y,
                                       float radius_x,
                                       float radius_y,
                                       float rotation_angle,
                                       float force) {
  CHECK_GE(index, 0);
  CHECK_LT(index, kTouchesLengthCap);
  // Always set this bit to avoid otherwise unexpected touchmove suppression.
  // The caller can opt-out explicitly, if necessary.
  moved_beyond_slop_region = true;
  WebTouchPoint& point = touches[index];
  point.SetPositionInWidget(x, y);
  point.SetPositionInScreen(x, y);
  point.state = WebTouchPoint::kStateMoved;
  point.radius_x = radius_x;
  point.radius_y = radius_y;
  point.rotation_angle = rotation_angle;
  point.force = force;
  WebTouchEventTraits::ResetType(WebInputEvent::kTouchMove, TimeStamp(), this);
}

void SyntheticWebTouchEvent::ReleasePoint(int index) {
  CHECK_GE(index, 0);
  CHECK_LT(index, kTouchesLengthCap);
  touches[index].state = WebTouchPoint::kStateReleased;
  touches[index].force = 0.f;
  WebTouchEventTraits::ResetType(WebInputEvent::kTouchEnd, TimeStamp(), this);
}

void SyntheticWebTouchEvent::CancelPoint(int index) {
  CHECK_GE(index, 0);
  CHECK_LT(index, kTouchesLengthCap);
  touches[index].state = WebTouchPoint::kStateCancelled;
  WebTouchEventTraits::ResetType(WebInputEvent::kTouchCancel, TimeStamp(),
                                 this);
}

void SyntheticWebTouchEvent::SetTimestamp(base::TimeTicks timestamp) {
  SetTimeStamp(timestamp);
}

int SyntheticWebTouchEvent::FirstFreeIndex() {
  for (size_t i = 0; i < kTouchesLengthCap; ++i) {
    if (touches[i].state == WebTouchPoint::kStateUndefined)
      return i;
  }
  return -1;
}

}  // namespace content
