// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/widget_event_handler.h"

#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace blink {

WebInputEventResult WidgetEventHandler::HandleInputEvent(
    const WebCoalescedInputEvent& coalesced_event,
    LocalFrame* root) {
  const WebInputEvent& event = coalesced_event.Event();
  if (root) {
    Document* document = root->GetDocument();
    DCHECK(document);
    if (LocalFrameView* view = document->View())
      view->GetLayoutShiftTracker().NotifyInput(event);
    if (WebInputEvent::IsWebInteractionEvent(event.GetType())) {
      WindowPerformance* performance =
          DOMWindowPerformance::performance(*root->DomWindow());
      performance->GetResponsivenessMetrics()
          .SetCurrentInteractionEventQueuedTimestamp(event.QueuedTimeStamp());
    }
  }

  if (event.GetModifiers() & WebInputEvent::kIsTouchAccessibility &&
      WebInputEvent::IsMouseEventType(event.GetType())) {
    WebMouseEvent mouse_event = TransformWebMouseEvent(
        root->View(), static_cast<const WebMouseEvent&>(event));

    HitTestLocation location(root->View()->ConvertFromRootFrame(
        gfx::ToFlooredPoint(mouse_event.PositionInRootFrame())));
    HitTestResult result = root->GetEventHandler().HitTestResultAtLocation(
        location, HitTestRequest::kReadOnly | HitTestRequest::kActive);
    result.SetToShadowHostIfInUAShadowRoot();
    if (result.InnerNodeFrame()) {
      Document* document = result.InnerNodeFrame()->GetDocument();
      if (document) {
        AXObjectCache* cache = document->ExistingAXObjectCache();
        if (cache) {
          cache->OnTouchAccessibilityHover(
              result.RoundedPointInInnerNodeFrame());
        }
      }
    }
  }

  switch (event.GetType()) {
    // FIXME: WebKit seems to always return false on mouse events processing
    // methods. For now we'll assume it has processed them (as we are only
    // interested in whether keyboard events are processed).
    // FIXME: Why do we return HandleSuppressed when there is no root or
    // the root is detached?
    case WebInputEvent::Type::kMouseMove:
      if (!root || !root->View())
        return WebInputEventResult::kHandledSuppressed;
      HandleMouseMove(*root, static_cast<const WebMouseEvent&>(event),
                      coalesced_event.GetCoalescedEventsPointers(),
                      coalesced_event.GetPredictedEventsPointers());
      return WebInputEventResult::kHandledSystem;
    case WebInputEvent::Type::kMouseLeave:
      if (!root || !root->View())
        return WebInputEventResult::kHandledSuppressed;
      HandleMouseLeave(*root, static_cast<const WebMouseEvent&>(event));
      return WebInputEventResult::kHandledSystem;
    case WebInputEvent::Type::kMouseDown:
      if (!root || !root->View())
        return WebInputEventResult::kHandledSuppressed;
      HandleMouseDown(*root, static_cast<const WebMouseEvent&>(event));
      return WebInputEventResult::kHandledSystem;
    case WebInputEvent::Type::kMouseUp:
      if (!root || !root->View())
        return WebInputEventResult::kHandledSuppressed;
      return HandleMouseUp(*root, static_cast<const WebMouseEvent&>(event));
    case WebInputEvent::Type::kMouseWheel:
      if (!root || !root->View())
        return WebInputEventResult::kNotHandled;
      return HandleMouseWheel(*root,
                              static_cast<const WebMouseWheelEvent&>(event));

    case WebInputEvent::Type::kRawKeyDown:
    case WebInputEvent::Type::kKeyDown:
    case WebInputEvent::Type::kKeyUp:
      return HandleKeyEvent(static_cast<const WebKeyboardEvent&>(event));

    case WebInputEvent::Type::kChar:
      return HandleCharEvent(static_cast<const WebKeyboardEvent&>(event));
    case WebInputEvent::Type::kGestureScrollBegin:
    case WebInputEvent::Type::kGestureScrollEnd:
    case WebInputEvent::Type::kGestureScrollUpdate:
    case WebInputEvent::Type::kGestureFlingStart:
    case WebInputEvent::Type::kGestureFlingCancel:
    case WebInputEvent::Type::kGestureTap:
    case WebInputEvent::Type::kGestureTapUnconfirmed:
    case WebInputEvent::Type::kGestureTapDown:
    case WebInputEvent::Type::kGestureShowPress:
    case WebInputEvent::Type::kGestureTapCancel:
    case WebInputEvent::Type::kGestureDoubleTap:
    case WebInputEvent::Type::kGestureTwoFingerTap:
    case WebInputEvent::Type::kGestureShortPress:
    case WebInputEvent::Type::kGestureLongPress:
    case WebInputEvent::Type::kGestureLongTap:
      return HandleGestureEvent(static_cast<const WebGestureEvent&>(event));

    case WebInputEvent::Type::kPointerDown:
    case WebInputEvent::Type::kPointerUp:
    case WebInputEvent::Type::kPointerMove:
    case WebInputEvent::Type::kPointerRawUpdate:
    case WebInputEvent::Type::kPointerCancel:
    case WebInputEvent::Type::kPointerCausedUaAction:
      if (!root || !root->View())
        return WebInputEventResult::kNotHandled;
      return HandlePointerEvent(*root,
                                static_cast<const WebPointerEvent&>(event),
                                coalesced_event.GetCoalescedEventsPointers(),
                                coalesced_event.GetPredictedEventsPointers());

    case WebInputEvent::Type::kTouchStart:
    case WebInputEvent::Type::kTouchMove:
    case WebInputEvent::Type::kTouchEnd:
    case WebInputEvent::Type::kTouchCancel:
    case WebInputEvent::Type::kTouchScrollStarted:
      NOTREACHED_IN_MIGRATION();
      return WebInputEventResult::kNotHandled;

    case WebInputEvent::Type::kGesturePinchBegin:
      // Gesture pinch events are handled entirely on the compositor.
      DLOG(INFO) << "Gesture pinch ignored by main thread.";
      [[fallthrough]];
    case WebInputEvent::Type::kGesturePinchEnd:
    case WebInputEvent::Type::kGesturePinchUpdate:
      return WebInputEventResult::kNotHandled;
    default:
      return WebInputEventResult::kNotHandled;
  }
}

void WidgetEventHandler::HandleMouseMove(
    LocalFrame& local_root,
    const WebMouseEvent& event,
    const std::vector<std::unique_ptr<WebInputEvent>>& coalesced_events,
    const std::vector<std::unique_ptr<WebInputEvent>>& predicted_events) {
  WebMouseEvent transformed_event =
      TransformWebMouseEvent(local_root.View(), event);
  local_root.GetEventHandler().HandleMouseMoveEvent(
      transformed_event,
      TransformWebMouseEventVector(local_root.View(), coalesced_events),
      TransformWebMouseEventVector(local_root.View(), predicted_events));
}

void WidgetEventHandler::HandleMouseLeave(LocalFrame& local_root,
                                          const WebMouseEvent& event) {
  WebMouseEvent transformed_event =
      TransformWebMouseEvent(local_root.View(), event);
  local_root.GetEventHandler().HandleMouseLeaveEvent(transformed_event);
}

namespace {

bool IsDoubleAltClick(const blink::WebMouseEvent& mouse_event) {
  bool is_alt_pressed =
      mouse_event.GetModifiers() & blink::WebInputEvent::kAltKey;
  if (!is_alt_pressed) {
    return false;
  }
  return mouse_event.click_count == 2;
}

}  // namespace

void WidgetEventHandler::HandleMouseDown(LocalFrame& local_root,
                                         const WebMouseEvent& event) {
  if (IsDoubleAltClick(event)) {
    local_root.GetEventHandler().GetDelayedNavigationTaskHandle().Cancel();
  }

  WebMouseEvent transformed_event =
      TransformWebMouseEvent(local_root.View(), event);
  local_root.GetEventHandler().HandleMousePressEvent(transformed_event);
}

WebInputEventResult WidgetEventHandler::HandleMouseUp(
    LocalFrame& local_root,
    const WebMouseEvent& event) {
  WebMouseEvent transformed_event =
      TransformWebMouseEvent(local_root.View(), event);
  return local_root.GetEventHandler().HandleMouseReleaseEvent(
      transformed_event);
}

WebInputEventResult WidgetEventHandler::HandleMouseWheel(
    LocalFrame& local_root,
    const WebMouseWheelEvent& event) {
  WebMouseWheelEvent transformed_event =
      TransformWebMouseWheelEvent(local_root.View(), event);
  return local_root.GetEventHandler().HandleWheelEvent(transformed_event);
}

WebInputEventResult WidgetEventHandler::HandlePointerEvent(
    LocalFrame& local_root,
    const WebPointerEvent& event,
    const std::vector<std::unique_ptr<WebInputEvent>>& coalesced_events,
    const std::vector<std::unique_ptr<WebInputEvent>>& predicted_events) {
  WebPointerEvent transformed_event =
      TransformWebPointerEvent(local_root.View(), event);
  return local_root.GetEventHandler().HandlePointerEvent(
      transformed_event,
      TransformWebPointerEventVector(local_root.View(), coalesced_events),
      TransformWebPointerEventVector(local_root.View(), predicted_events));
}

}  // namespace blink
