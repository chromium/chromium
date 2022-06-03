// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/input/web_pointer_event.h"

#include "base/check_op.h"
#include "base/notreached.h"

namespace blink {

namespace {

WebInputEvent::Type PointerEventTypeForTouchPointState(
    WebTouchPoint::State state) {
  switch (state) {
    case WebTouchPoint::State::kStateReleased:
      return WebInputEvent::Type::kPointerUp;
    case WebTouchPoint::State::kStateCancelled:
      return WebInputEvent::Type::kPointerCancel;
    case WebTouchPoint::State::kStatePressed:
      return WebInputEvent::Type::kPointerDown;
    case WebTouchPoint::State::kStateMoved:
      return WebInputEvent::Type::kPointerMove;
    case WebTouchPoint::State::kStateStationary:
    default:
      NOTREACHED();
      return WebInputEvent::Type::kUndefined;
  }
}

}  // namespace

WebPointerEvent::WebPointerEvent(const WebTouchEvent& touch_event,
                                 const WebTouchPoint& touch_point)
    : WebInputEvent(PointerEventTypeForTouchPointState(touch_point.state),
                    touch_event.GetModifiers(),
                    touch_event.TimeStamp()),

      WebPointerProperties(touch_point),
      hovering(touch_event.hovering),
      width(touch_point.radius_x * 2.f),
      height(touch_point.radius_y * 2.f) {
  // WebInutEvent attributes
  SetFrameScale(touch_event.FrameScale());
  SetFrameTranslate(touch_event.FrameTranslate());
  // WebTouchEvent attributes
  dispatch_type = touch_event.dispatch_type;
  moved_beyond_slop_region = touch_event.moved_beyond_slop_region;
  touch_start_or_first_touch_move = touch_event.touch_start_or_first_touch_move;
  unique_touch_event_id = touch_event.unique_touch_event_id;
  // WebTouchPoint attributes
  rotation_angle = touch_point.rotation_angle;
  // TODO(crbug.com/816504): Touch point button is not set at this point yet.
  button = (GetType() == WebInputEvent::Type::kPointerDown ||
            GetType() == WebInputEvent::Type::kPointerUp)
               ? WebPointerProperties::Button::kLeft
               : WebPointerProperties::Button::kNoButton;
}

WebPointerEvent::WebPointerEvent(WebInputEvent::Type type,
                                 const WebMouseEvent& mouse_event)
    : WebInputEvent(type, mouse_event.GetModifiers(), mouse_event.TimeStamp()),
      WebPointerProperties(mouse_event),
      hovering(true),
      width(std::numeric_limits<float>::quiet_NaN()),
      height(std::numeric_limits<float>::quiet_NaN()) {
  DCHECK_GE(type, WebInputEvent::Type::kPointerTypeFirst);
  DCHECK_LE(type, WebInputEvent::Type::kPointerTypeLast);
  SetFrameScale(mouse_event.FrameScale());
  SetFrameTranslate(mouse_event.FrameTranslate());
}

std::unique_ptr<WebInputEvent> WebPointerEvent::Clone() const {
  return std::make_unique<WebPointerEvent>(*this);
}

bool WebPointerEvent::CanCoalesce(const WebInputEvent& event) const {
  if (!IsPointerEventType(event.GetType()))
    return false;
  const WebPointerEvent& pointer_event =
      static_cast<const WebPointerEvent&>(event);
  return (GetType() == WebInputEvent::Type::kPointerMove ||
          GetType() == WebInputEvent::Type::kPointerRawUpdate) &&
         GetType() == event.GetType() &&
         GetModifiers() == event.GetModifiers() && id == pointer_event.id &&
         pointer_type == pointer_event.pointer_type;
}

void WebPointerEvent::Coalesce(const WebInputEvent& event) {
  DCHECK(CanCoalesce(event));
  const WebPointerEvent& pointer_event =
      static_cast<const WebPointerEvent&>(event);
  // Accumulate movement deltas.
  int x = movement_x;
  int y = movement_y;
  *this = pointer_event;
  movement_x += x;
  movement_y += y;
}

WebPointerEvent WebPointerEvent::CreatePointerCausesUaActionEvent(
    WebPointerProperties::PointerType type,
    base::TimeTicks time_stamp) {
  WebPointerEvent event;
  event.pointer_type = type;
  event.SetTimeStamp(time_stamp);
  event.SetType(WebInputEvent::Type::kPointerCausedUaAction);
  return event;
}

WebPointerEvent WebPointerEvent::WebPointerEventInRootFrame() const {
  WebPointerEvent transformed_event = *this;
  if (HasWidth())
    transformed_event.width /= frame_scale_;
  if (HasHeight())
    transformed_event.height /= frame_scale_;
  transformed_event.position_in_widget_ =
      gfx::ScalePoint(transformed_event.PositionInWidget(), 1 / frame_scale_) +
      frame_translate_;
  return transformed_event;
}

}  // namespace blink
