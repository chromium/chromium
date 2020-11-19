/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/page/page_widget_delegate.h"

#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/validation_message_client.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

void PageWidgetDelegate::Animate(Page& page,
                                 base::TimeTicks monotonic_frame_begin_time) {
  page.GetAutoscrollController().Animate();
  page.Animator().ServiceScriptedAnimations(monotonic_frame_begin_time);
  // The ValidationMessage overlay manages its own internal Page that isn't
  // hooked up the normal BeginMainFrame flow, so we manually tick its
  // animations here.
  page.GetValidationMessageClient().ServiceScriptedAnimations(
      monotonic_frame_begin_time);
}

void PageWidgetDelegate::PostAnimate(Page& page) {
  page.Animator().PostAnimate();
}

void PageWidgetDelegate::UpdateLifecycle(Page& page,
                                         LocalFrame& root,
                                         WebLifecycleUpdate requested_update,
                                         DocumentUpdateReason reason) {
  if (requested_update == WebLifecycleUpdate::kLayout) {
    page.Animator().UpdateLifecycleToLayoutClean(root, reason);
  } else if (requested_update == WebLifecycleUpdate::kPrePaint) {
    page.Animator().UpdateLifecycleToPrePaintClean(root, reason);
  } else {
    page.Animator().UpdateAllLifecyclePhases(root, reason);
  }
}

void PageWidgetDelegate::DidBeginFrame(LocalFrame& root) {
  if (LocalFrameView* frame_view = root.View())
    frame_view->RunPostLifecycleSteps();
  if (Page* page = root.GetPage())
    PostAnimate(*page);
}

WebInputEventResult PageWidgetDelegate::HandleInputEvent(
    PageWidgetEventHandler& handler,
    const WebCoalescedInputEvent& coalesced_event,
    LocalFrame* root) {
  const WebInputEvent& event = coalesced_event.Event();
  if (root) {
    Document* document = root->GetDocument();
    DCHECK(document);
    if (LocalFrameView* view = document->View())
      view->GetLayoutShiftTracker().NotifyInput(event);
  }

  if (event.GetModifiers() & WebInputEvent::kIsTouchAccessibility &&
      WebInputEvent::IsMouseEventType(event.GetType())) {
    WebMouseEvent mouse_event = TransformWebMouseEvent(
        root->View(), static_cast<const WebMouseEvent&>(event));

    HitTestLocation location(root->View()->ConvertFromRootFrame(
        FlooredIntPoint(mouse_event.PositionInRootFrame())));
    HitTestResult result = root->GetEventHandler().HitTestResultAtLocation(
        location, HitTestRequest::kReadOnly | HitTestRequest::kActive);
    result.SetToShadowHostIfInRestrictedShadowRoot();
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
      handler.HandleMouseMove(*root, static_cast<const WebMouseEvent&>(event),
                              coalesced_event.GetCoalescedEventsPointers(),
                              coalesced_event.GetPredictedEventsPointers());
      return WebInputEventResult::kHandledSystem;
    case WebInputEvent::Type::kMouseLeave:
      if (!root || !root->View())
        return WebInputEventResult::kHandledSuppressed;
      handler.HandleMouseLeave(*root, static_cast<const WebMouseEvent&>(event));
      return WebInputEventResult::kHandledSystem;
    case WebInputEvent::Type::kMouseDown:
      if (!root || !root->View())
        return WebInputEventResult::kHandledSuppressed;
      handler.HandleMouseDown(*root, static_cast<const WebMouseEvent&>(event));
      return WebInputEventResult::kHandledSystem;
    case WebInputEvent::Type::kMouseUp:
      if (!root || !root->View())
        return WebInputEventResult::kHandledSuppressed;
      return handler.HandleMouseUp(*root,
                                   static_cast<const WebMouseEvent&>(event));
    case WebInputEvent::Type::kMouseWheel:
      if (!root || !root->View())
        return WebInputEventResult::kNotHandled;
      return handler.HandleMouseWheel(
          *root, static_cast<const WebMouseWheelEvent&>(event));

    case WebInputEvent::Type::kRawKeyDown:
    case WebInputEvent::Type::kKeyDown:
    case WebInputEvent::Type::kKeyUp:
      return handler.HandleKeyEvent(
          static_cast<const WebKeyboardEvent&>(event));

    case WebInputEvent::Type::kChar:
      return handler.HandleCharEvent(
          static_cast<const WebKeyboardEvent&>(event));
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
    case WebInputEvent::Type::kGestureLongPress:
    case WebInputEvent::Type::kGestureLongTap:
      return handler.HandleGestureEvent(
          static_cast<const WebGestureEvent&>(event));

    case WebInputEvent::Type::kPointerDown:
    case WebInputEvent::Type::kPointerUp:
    case WebInputEvent::Type::kPointerMove:
    case WebInputEvent::Type::kPointerRawUpdate:
    case WebInputEvent::Type::kPointerCancel:
    case WebInputEvent::Type::kPointerCausedUaAction:
      if (!root || !root->View())
        return WebInputEventResult::kNotHandled;
      return handler.HandlePointerEvent(
          *root, static_cast<const WebPointerEvent&>(event),
          coalesced_event.GetCoalescedEventsPointers(),
          coalesced_event.GetPredictedEventsPointers());

    case WebInputEvent::Type::kTouchStart:
    case WebInputEvent::Type::kTouchMove:
    case WebInputEvent::Type::kTouchEnd:
    case WebInputEvent::Type::kTouchCancel:
    case WebInputEvent::Type::kTouchScrollStarted:
      NOTREACHED();
      return WebInputEventResult::kNotHandled;

    case WebInputEvent::Type::kGesturePinchBegin:
      // Gesture pinch events are handled entirely on the compositor.
      DLOG(INFO) << "Gesture pinch ignored by main thread.";
      FALLTHROUGH;
    case WebInputEvent::Type::kGesturePinchEnd:
    case WebInputEvent::Type::kGesturePinchUpdate:
      return WebInputEventResult::kNotHandled;
    default:
      return WebInputEventResult::kNotHandled;
  }
}

// ----------------------------------------------------------------
// Default handlers for PageWidgetEventHandler

void PageWidgetEventHandler::HandleMouseMove(
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

void PageWidgetEventHandler::HandleMouseLeave(LocalFrame& local_root,
                                              const WebMouseEvent& event) {
  WebMouseEvent transformed_event =
      TransformWebMouseEvent(local_root.View(), event);
  local_root.GetEventHandler().HandleMouseLeaveEvent(transformed_event);
}

void PageWidgetEventHandler::HandleMouseDown(LocalFrame& local_root,
                                             const WebMouseEvent& event) {
  WebMouseEvent transformed_event =
      TransformWebMouseEvent(local_root.View(), event);
  local_root.GetEventHandler().HandleMousePressEvent(transformed_event);
}

WebInputEventResult PageWidgetEventHandler::HandleMouseUp(
    LocalFrame& local_root,
    const WebMouseEvent& event) {
  WebMouseEvent transformed_event =
      TransformWebMouseEvent(local_root.View(), event);
  return local_root.GetEventHandler().HandleMouseReleaseEvent(
      transformed_event);
}

WebInputEventResult PageWidgetEventHandler::HandleMouseWheel(
    LocalFrame& local_root,
    const WebMouseWheelEvent& event) {
  WebMouseWheelEvent transformed_event =
      TransformWebMouseWheelEvent(local_root.View(), event);
  return local_root.GetEventHandler().HandleWheelEvent(transformed_event);
}

WebInputEventResult PageWidgetEventHandler::HandlePointerEvent(
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
