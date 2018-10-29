// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/ui_events_helper.h"

#include <stdint.h>

#include "content/common/input/web_touch_event_traits.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace {

ui::EventType WebTouchPointStateToEventType(
    blink::WebTouchPoint::State state) {
  switch (state) {
    case blink::WebTouchPoint::kStateReleased:
      return ui::ET_TOUCH_RELEASED;

    case blink::WebTouchPoint::kStatePressed:
      return ui::ET_TOUCH_PRESSED;

    case blink::WebTouchPoint::kStateMoved:
      return ui::ET_TOUCH_MOVED;

    case blink::WebTouchPoint::kStateCancelled:
      return ui::ET_TOUCH_CANCELLED;

    default:
      return ui::ET_UNKNOWN;
  }
}

}  // namespace

namespace content {

bool MakeUITouchEventsFromWebTouchEvents(
    const TouchEventWithLatencyInfo& touch_with_latency,
    std::vector<std::unique_ptr<ui::TouchEvent>>* list,
    TouchEventCoordinateSystem coordinate_system) {
  const blink::WebTouchEvent& touch = touch_with_latency.event;
  ui::EventType type = ui::ET_UNKNOWN;
  switch (touch.GetType()) {
    case blink::WebInputEvent::kTouchStart:
      type = ui::ET_TOUCH_PRESSED;
      break;
    case blink::WebInputEvent::kTouchEnd:
      type = ui::ET_TOUCH_RELEASED;
      break;
    case blink::WebInputEvent::kTouchMove:
      type = ui::ET_TOUCH_MOVED;
      break;
    case blink::WebInputEvent::kTouchCancel:
      type = ui::ET_TOUCH_CANCELLED;
      break;
    default:
      NOTREACHED();
      return false;
  }

  int flags = ui::WebEventModifiersToEventFlags(touch.GetModifiers());
  base::TimeTicks timestamp = touch.TimeStamp();
  for (unsigned i = 0; i < touch.touches_length; ++i) {
    const blink::WebTouchPoint& point = touch.touches[i];
    if (WebTouchPointStateToEventType(point.state) != type)
      continue;
    // ui events start in the co-ordinate space of the EventDispatcher.
    gfx::PointF location;
    if (coordinate_system == LOCAL_COORDINATES)
      location = point.PositionInWidget();
    else
      location = point.PositionInScreen();
    auto uievent = std::make_unique<ui::TouchEvent>(
        type, gfx::Point(), timestamp,
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, point.id,
                           point.radius_x, point.radius_y, point.force,
                           point.rotation_angle),
        flags);
    uievent->set_location_f(location);
    uievent->set_root_location_f(location);
    uievent->set_latency(touch_with_latency.latency);
    list->push_back(std::move(uievent));
  }
  return true;
}

bool InputEventAckStateIsSetNonBlocking(InputEventAckState ack_state) {
  switch (ack_state) {
    case INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING:
    case INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING:
      return true;
    default:
      return false;
  }
}

}  // namespace content
