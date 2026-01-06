// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_selection_controller_input_observer.h"

#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/browser/render_widget_host_view.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "ui/events/types/event_type.h"

namespace content {

namespace {

#define UI_EVENT_TYPE_CASE(x)         \
  case blink::WebInputEvent::Type::x: \
    return ui::EventType::x

ui::EventType FromBlinkScrollTypeToUiEventType(
    blink::WebInputEvent::Type type) {
  switch (type) {
    UI_EVENT_TYPE_CASE(kGestureScrollBegin);
    UI_EVENT_TYPE_CASE(kGestureScrollUpdate);
    UI_EVENT_TYPE_CASE(kGestureScrollEnd);
    default:
      NOTREACHED();
  }
}

#undef UI_EVENT_TYPE_CASE

}  // namespace

TouchSelectionControllerInputObserver::TouchSelectionControllerInputObserver(
    ui::TouchSelectionController* controller,
    TouchSelectionControllerClientManager* manager)
    : controller_(controller), manager_(manager) {}

void TouchSelectionControllerInputObserver::OnInputEvent(
    const RenderWidgetHost& widget,
    const blink::WebInputEvent& input_event,
    InputEventSource source) {
  if (!blink::WebInputEvent::IsGestureEventType(input_event.GetType())) {
    return;
  }

  const blink::WebGestureEvent& gesture_event =
      *(static_cast<const blink::WebGestureEvent*>(&input_event));

  if (gesture_event.SourceDevice() != blink::WebGestureDevice::kTouchscreen) {
    return;
  }

  CHECK(widget.GetView());
  const gfx::PointF point_in_root =
      widget.GetView()->TransformPointToRootCoordSpaceF(
          gesture_event.PositionInWidget());

  switch (gesture_event.GetType()) {
    case blink::WebInputEvent::Type::kGestureLongPress:
      controller_->HandleLongPressEvent(gesture_event.TimeStamp(),
                                        point_in_root);
      return;
    case blink::WebInputEvent::Type::kGestureTapDown:
      if (gesture_event.data.tap_down.tap_down_count == 2) {
        controller_->HandleDoublePressEvent(gesture_event.TimeStamp(),
                                            point_in_root);
      }
      return;
    case blink::WebInputEvent::Type::kGestureTap:
      controller_->HandleTapEvent(point_in_root,
                                  gesture_event.data.tap.tap_count);
      return;
    case blink::WebInputEvent::Type::kGestureScrollBegin:
      controller_->OnScrollBeginEvent();
      return;
    default:
      // These are events we don't care about like ShowPress, TapCancel, etc.
      return;
  }
}

void TouchSelectionControllerInputObserver::OnInputEventAck(
    const RenderWidgetHost& widget,
    blink::mojom::InputEventResultSource source,
    blink::mojom::InputEventResultState state,
    const blink::WebInputEvent& input_event) {
  if (!input_event.IsGestureScroll()) {
    return;
  }

  if (input_event.GetType() ==
      blink::WebInputEvent::Type::kGestureScrollBegin) {
    has_seen_scroll_begin_ack_ = true;
  }

  const blink::WebGestureEvent& event =
      *(static_cast<const blink::WebGestureEvent*>(&input_event));

  const ui::EventType type = FromBlinkScrollTypeToUiEventType(event.GetType());
  const gfx::PointF& point = event.PositionInWidget();

  std::optional<bool> cursor_control = std::nullopt;
  if (type == ui::EventType::kGestureScrollBegin) {
    cursor_control = event.data.scroll_begin.cursor_control;
  }

  CHECK(widget.GetView());
  const auto* view =
      static_cast<const RenderWidgetHostViewBase*>(widget.GetView());
  const bool is_root_view = !view->IsRenderWidgetHostViewChildFrame();
  controller_->HandleSwipeToMoveCursorGestureAck(type, point, cursor_control,
                                                 is_root_view);
}

}  // namespace content
