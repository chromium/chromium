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

#include "third_party/blink/public/platform/web_input_event.h"
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

void PageWidgetDelegate::UpdateLifecycle(
    Page& page,
    LocalFrame& root,
    WebWidget::LifecycleUpdate requested_update,
    WebWidget::LifecycleUpdateReason reason) {
  if (requested_update == WebWidget::LifecycleUpdate::kLayout) {
    page.Animator().UpdateLifecycleToLayoutClean(root);
  } else if (requested_update == WebWidget::LifecycleUpdate::kPrePaint) {
    page.Animator().UpdateAllLifecyclePhasesExceptPaint(root);
  } else {
    page.Animator().UpdateAllLifecyclePhases(
        root, static_cast<DocumentLifecycle::LifecycleUpdateReason>(reason));
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
    case WebInputEvent::kMouseMove:
      if (!root || !root->View())
        return WebInputEventResult::kHandledSuppressed;
      handler.HandleMouseMove(*root, static_cast<const WebMouseEvent&>(event),
                              coalesced_event.GetCoalescedEventsPointers(),
                              coalesced_event.GetPredictedEventsPointers());
      return WebInputEventResult::kHandledSystem;
    case WebInputEvent::kMouseLeave:
      if (!root || !root->View())
        return WebInputEventResult::kHandledSuppressed;
      handler.HandleMouseLeave(*root, static_cast<const WebMouseEvent&>(event));
      return WebInputEventResult::kHandledSystem;
    case WebInputEvent::kMouseDown:
      if (!root || !root->View())
        return WebInputEventResult::kHandledSuppressed;
      handler.HandleMouseDown(*root, static_cast<const WebMouseEvent&>(event));
      return WebInputEventResult::kHandledSystem;
    case WebInputEvent::kMouseUp:
      if (!root || !root->View())
        return WebInputEventResult::kHandledSuppressed;
      return handler.HandleMouseUp(*root,
                                   static_cast<const WebMouseEvent&>(event));
    case WebInputEvent::kMouseWheel:
      if (!root || !root->View())
        return WebInputEventResult::kNotHandled;
      return handler.HandleMouseWheel(
          *root, static_cast<const WebMouseWheelEvent&>(event));

    case WebInputEvent::kRawKeyDown:
    case WebInputEvent::kKeyDown:
    case WebInputEvent::kKeyUp:
      return handler.HandleKeyEvent(
          static_cast<const WebKeyboardEvent&>(event));

    case WebInputEvent::kChar:
      return handler.HandleCharEvent(
          static_cast<const WebKeyboardEvent&>(event));
    case WebInputEvent::kGestureScrollBegin:
    case WebInputEvent::kGestureScrollEnd:
    case WebInputEvent::kGestureScrollUpdate:
    case WebInputEvent::kGestureFlingStart:
    case WebInputEvent::kGestureFlingCancel:
    case WebInputEvent::kGestureTap:
    case WebInputEvent::kGestureTapUnconfirmed:
    case WebInputEvent::kGestureTapDown:
    case WebInputEvent::kGestureShowPress:
    case WebInputEvent::kGestureTapCancel:
    case WebInputEvent::kGestureDoubleTap:
    case WebInputEvent::kGestureTwoFingerTap:
    case WebInputEvent::kGestureLongPress:
    case WebInputEvent::kGestureLongTap:
      return handler.HandleGestureEvent(
          static_cast<const WebGestureEvent&>(event));

    case WebInputEvent::kPointerDown:
    case WebInputEvent::kPointerUp:
    case WebInputEvent::kPointerMove:
    case WebInputEvent::kPointerRawUpdate:
    case WebInputEvent::kPointerCancel:
    case WebInputEvent::kPointerCausedUaAction:
      if (!root || !root->View())
        return WebInputEventResult::kNotHandled;
      return handler.HandlePointerEvent(
          *root, static_cast<const WebPointerEvent&>(event),
          coalesced_event.GetCoalescedEventsPointers(),
          coalesced_event.GetPredictedEventsPointers());

    case WebInputEvent::kTouchStart:
    case WebInputEvent::kTouchMove:
    case WebInputEvent::kTouchEnd:
    case WebInputEvent::kTouchCancel:
    case WebInputEvent::kTouchScrollStarted:
      NOTREACHED();
      return WebInputEventResult::kNotHandled;

    case WebInputEvent::kGesturePinchBegin:
      // Gesture pinch events are handled entirely on the compositor.
      DLOG(INFO) << "Gesture pinch ignored by main thread.";
      FALLTHROUGH;
    case WebInputEvent::kGesturePinchEnd:
    case WebInputEvent::kGesturePinchUpdate:
      return WebInputEventResult::kNotHandled;
    default:
      return WebInputEventResult::kNotHandled;
  }
}

// ----------------------------------------------------------------
// Default handlers for PageWidgetEventHandler

void PageWidgetEventHandler::HandleMouseMove(
    LocalFrame& main_frame,
    const WebMouseEvent& event,
    const WebVector<const WebInputEvent*>& coalesced_events,
    const WebVector<const WebInputEvent*>& predicted_events) {
  WebMouseEvent transformed_event =
      TransformWebMouseEvent(main_frame.View(), event);
  main_frame.GetEventHandler().HandleMouseMoveEvent(
      transformed_event,
      TransformWebMouseEventVector(main_frame.View(), coalesced_events),
      TransformWebMouseEventVector(main_frame.View(), predicted_events));
}

void PageWidgetEventHandler::HandleMouseLeave(LocalFrame& main_frame,
                                              const WebMouseEvent& event) {
  WebMouseEvent transformed_event =
      TransformWebMouseEvent(main_frame.View(), event);
  main_frame.GetEventHandler().HandleMouseLeaveEvent(transformed_event);
}

void PageWidgetEventHandler::HandleMouseDown(LocalFrame& main_frame,
                                             const WebMouseEvent& event) {
  WebMouseEvent transformed_event =
      TransformWebMouseEvent(main_frame.View(), event);
  main_frame.GetEventHandler().HandleMousePressEvent(transformed_event);
}

WebInputEventResult PageWidgetEventHandler::HandleMouseUp(
    LocalFrame& main_frame,
    const WebMouseEvent& event) {
  WebMouseEvent transformed_event =
      TransformWebMouseEvent(main_frame.View(), event);
  return main_frame.GetEventHandler().HandleMouseReleaseEvent(
      transformed_event);
}

WebInputEventResult PageWidgetEventHandler::HandleMouseWheel(
    LocalFrame& frame,
    const WebMouseWheelEvent& event) {
  WebMouseWheelEvent transformed_event =
      TransformWebMouseWheelEvent(frame.View(), event);
  return frame.GetEventHandler().HandleWheelEvent(transformed_event);
}

WebInputEventResult PageWidgetEventHandler::HandlePointerEvent(
    LocalFrame& main_frame,
    const WebPointerEvent& event,
    const WebVector<const WebInputEvent*>& coalesced_events,
    const WebVector<const WebInputEvent*>& predicted_events) {
  WebPointerEvent transformed_event =
      TransformWebPointerEvent(main_frame.View(), event);
  return main_frame.GetEventHandler().HandlePointerEvent(
      transformed_event,
      TransformWebPointerEventVector(main_frame.View(), coalesced_events),
      TransformWebPointerEventVector(main_frame.View(), predicted_events));
}

}  // namespace blink
