// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/input/pointer_event_manager.h"

#include "base/auto_reset.h"
#include "base/metrics/field_trial_params.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/events/event_path.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/event_handling_util.h"
#include "third_party/blink/renderer/core/input/mouse_event_manager.h"
#include "third_party/blink/renderer/core/input/touch_action_util.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/anchor_element_interaction_tracker.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/pointer_lock_controller.h"
#include "third_party/blink/renderer/core/page/touch_adjustment.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/core/timing/event_timing.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/display/screen_info.h"

namespace blink {

namespace {

// Field trial name for skipping touch filtering
const char kSkipTouchEventFilterTrial[] = "SkipTouchEventFilter";
const char kSkipTouchEventFilterTrialProcessParamName[] =
    "skip_filtering_process";
const char kSkipTouchEventFilterTrialTypeParamName[] = "type";

// Width and height of area of rectangle to hit test for potentially important
// input fields to write into. This improves the chances of writing into the
// intended input if the user starts writing close to it.
const size_t kStylusWritableAdjustmentSizeDip = 30;

size_t ToPointerTypeIndex(WebPointerProperties::PointerType t) {
  return static_cast<size_t>(t);
}

bool HasPointerEventListener(const EventHandlerRegistry& registry) {
  return registry.HasEventHandlers(EventHandlerRegistry::kPointerEvent) ||
         registry.HasEventHandlers(
             EventHandlerRegistry::kPointerRawUpdateEvent);
}

const AtomicString& MouseEventNameForPointerEventInputType(
    const WebInputEvent::Type& event_type) {
  switch (event_type) {
    case WebInputEvent::Type::kPointerDown:
      return event_type_names::kMousedown;
    case WebInputEvent::Type::kPointerUp:
      return event_type_names::kMouseup;
    case WebInputEvent::Type::kPointerMove:
      return event_type_names::kMousemove;
    default:
      NOTREACHED_IN_MIGRATION();
      return g_empty_atom;
  }
}

}  // namespace

PointerEventManager::PointerEventManager(LocalFrame& frame,
                                         MouseEventManager& mouse_event_manager)
    : frame_(frame),
      touch_event_manager_(MakeGarbageCollected<TouchEventManager>(frame)),
      mouse_event_manager_(mouse_event_manager) {
  Clear();
  if (RuntimeEnabledFeatures::SkipTouchEventFilterEnabled() &&
      base::GetFieldTrialParamValue(
          kSkipTouchEventFilterTrial,
          kSkipTouchEventFilterTrialProcessParamName) ==
          "browser_and_renderer") {
    skip_touch_filter_discrete_ = true;
    if (base::GetFieldTrialParamValue(
            kSkipTouchEventFilterTrial,
            kSkipTouchEventFilterTrialTypeParamName) == "all") {
      skip_touch_filter_all_ = true;
    }
  }
}

void PointerEventManager::Clear() {
  for (auto& entry : prevent_mouse_event_for_pointer_type_) {
    entry = false;
  }
  touch_event_manager_->Clear();
  mouse_event_manager_->Clear();
  non_hovering_pointers_canceled_ = false;
  pointer_event_factory_.Clear();
  touch_ids_for_canceled_pointerdowns_.clear();
  element_under_pointer_.clear();
  original_element_under_pointer_removed_.clear();
  pointer_capture_target_.clear();
  pending_pointer_capture_target_.clear();
  dispatching_pointer_id_ = 0;
  resize_scrollable_area_.Clear();
  offset_from_resize_corner_ = {};
  skip_touch_filter_discrete_ = false;
  skip_touch_filter_all_ = false;
  discarded_event_.target = kInvalidDOMNodeId;
  discarded_event_.time = base::TimeTicks();
  SetDocument(frame_->GetDocument());
}

void PointerEventManager::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(element_under_pointer_);
  visitor->Trace(pointer_capture_target_);
  visitor->Trace(pending_pointer_capture_target_);
  visitor->Trace(touch_event_manager_);
  visitor->Trace(mouse_event_manager_);
  visitor->Trace(captured_scrollbar_);
  visitor->Trace(resize_scrollable_area_);
  SynchronousMutationObserver::Trace(visitor);
}

PointerEventManager::PointerEventBoundaryEventDispatcher::
    PointerEventBoundaryEventDispatcher(
        PointerEventManager* pointer_event_manager,
        PointerEvent* pointer_event)
    : BoundaryEventDispatcher(event_type_names::kPointerover,
                              event_type_names::kPointerout,
                              event_type_names::kPointerenter,
                              event_type_names::kPointerleave),
      pointer_event_manager_(pointer_event_manager),
      pointer_event_(pointer_event) {}

void PointerEventManager::PointerEventBoundaryEventDispatcher::Dispatch(
    EventTarget* target,
    EventTarget* related_target,
    const AtomicString& type,
    bool check_for_listener) {
  pointer_event_manager_->DispatchPointerEvent(
      target,
      pointer_event_manager_->pointer_event_factory_.CreatePointerBoundaryEvent(
          pointer_event_, type, related_target),
      check_for_listener);
}

WebInputEventResult PointerEventManager::DispatchPointerEvent(
    EventTarget* target,
    PointerEvent* pointer_event,
    bool check_for_listener) {
  if (!target)
    return WebInputEventResult::kNotHandled;

  const PointerId pointer_id = pointer_event->pointerId();
  const AtomicString& event_type = pointer_event->type();
  bool should_filter = ShouldFilterEvent(pointer_event);
  // We are about to dispatch this event. It has to be trusted at this point.
  pointer_event->SetTrusted(true);
  std::unique_ptr<EventTiming> event_timing;
  if (frame_ && frame_->DomWindow()) {
    event_timing =
        EventTiming::Create(frame_->DomWindow(), *pointer_event, target);
  }

  if (event_type == event_type_names::kPointerdown ||
      event_type == event_type_names::kPointerover ||
      event_type == event_type_names::kPointerout) {
    AnchorElementInteractionTracker* tracker =
        frame_->GetDocument()->GetAnchorElementInteractionTracker();
    if (tracker) {
      tracker->OnPointerEvent(*target, *pointer_event);
    }
  }

  if (Node* target_node = target->ToNode()) {
    if (event_type == event_type_names::kPointerdown ||
        event_type == event_type_names::kPointerup) {
      HTMLElement::HandlePopoverLightDismiss(*pointer_event, *target_node);
    }
  }

  if (should_filter &&
      !HasPointerEventListener(frame_->GetEventHandlerRegistry()))
    return WebInputEventResult::kNotHandled;

  if (event_type == event_type_names::kPointerdown) {
    auto* html_canvas_element = DynamicTo<HTMLCanvasElement>(target->ToNode());
    if (html_canvas_element &&
        html_canvas_element->NeedsUnbufferedInputEvents()) {
      frame_->GetChromeClient().RequestUnbufferedInputEvents(frame_);
    }
  }

  bool listeners_exist =
      !check_for_listener || target->HasEventListeners(event_type);
  if (listeners_exist) {
    UseCounter::Count(frame_->GetDocument(), WebFeature::kPointerEventDispatch);
    if (event_type == event_type_names::kPointerdown) {
      UseCounter::Count(frame_->GetDocument(),
                        WebFeature::kPointerEventDispatchPointerDown);
    }
  }

  if (!should_filter || listeners_exist) {
    DCHECK(!dispatching_pointer_id_);
    base::AutoReset<PointerId> dispatch_holder(&dispatching_pointer_id_,
                                               pointer_id);
    DispatchEventResult dispatch_result = target->DispatchEvent(*pointer_event);
    return event_handling_util::ToWebInputEventResult(dispatch_result);
  }
  return WebInputEventResult::kNotHandled;
}

Element* PointerEventManager::GetEffectiveTargetForPointerEvent(
    Element* target,
    PointerId pointer_id) {
  if (pointer_capture_target_.Contains(pointer_id)) {
    return pointer_capture_target_.at(pointer_id);
  }
  return target;
}

void PointerEventManager::SendMouseAndPointerBoundaryEvents(
    Element* entered_element,
    const WebMouseEvent& mouse_event) {
  // Mouse event type does not matter as this pointerevent will only be used
  // to create boundary pointer events and its type will be overridden in
  // `SendBoundaryEvents` function.
  const WebPointerEvent web_pointer_event(WebInputEvent::Type::kPointerMove,
                                          mouse_event);
  PointerEvent* dummy_pointer_event = pointer_event_factory_.Create(
      web_pointer_event, Vector<WebPointerEvent>(), Vector<WebPointerEvent>(),
      frame_->GetDocument()->domWindow());
  DCHECK(dummy_pointer_event);

  // TODO(crbug/545647): This state should reset with pointercancel too.
  // This function also gets called for compat mouse events of touch at this
  // stage. So if the event is not frame boundary transition it is only a
  // compatibility mouse event and we do not need to change pointer event
  // behavior regarding preventMouseEvent state in that case.
  if (dummy_pointer_event->buttons() == 0 && dummy_pointer_event->isPrimary()) {
    prevent_mouse_event_for_pointer_type_[ToPointerTypeIndex(
        mouse_event.pointer_type)] = false;
  }

  ProcessCaptureAndPositionOfPointerEvent(dummy_pointer_event, entered_element,
                                          &mouse_event);
}

void PointerEventManager::SendBoundaryEvents(
    EventTarget* exited_target,
    bool original_exited_target_removed,
    EventTarget* entered_target,
    PointerEvent* pointer_event) {
  PointerEventBoundaryEventDispatcher boundary_event_dispatcher(this,
                                                                pointer_event);
  boundary_event_dispatcher.SendBoundaryEvents(
      exited_target, original_exited_target_removed, entered_target);
}

void PointerEventManager::SetElementUnderPointer(PointerEvent* pointer_event,
                                                 Element* target) {
  const PointerId pointer_id = pointer_event->pointerId();

  CHECK(
      !original_element_under_pointer_removed_.Contains(pointer_id) ||
      RuntimeEnabledFeatures::BoundaryEventDispatchTracksNodeRemovalEnabled());

  Element* exited_target = element_under_pointer_.Contains(pointer_id)
                               ? element_under_pointer_.at(pointer_id)
                               : nullptr;
  bool original_exited_target_removed =
      original_element_under_pointer_removed_.Contains(pointer_id);

  if (exited_target) {
    if (!target) {
      element_under_pointer_.erase(pointer_id);
    } else if (target != exited_target) {
      element_under_pointer_.Set(pointer_id, target);
    }
  } else if (target) {
    element_under_pointer_.insert(pointer_id, target);
  }
  // Clear the "removed" state for the updated `element_under_pointer_`.
  original_element_under_pointer_removed_.erase(pointer_id);

  SendBoundaryEvents(exited_target, original_exited_target_removed, target,
                     pointer_event);
}

void PointerEventManager::NodeWillBeRemoved(Node& node_to_be_removed) {
  if (!RuntimeEnabledFeatures::
          BoundaryEventDispatchTracksNodeRemovalEnabled()) {
    return;
  }
  for (const auto& [pointer_id, element] : element_under_pointer_) {
    if (element &&
        node_to_be_removed.IsShadowIncludingInclusiveAncestorOf(*element)) {
      element_under_pointer_.Set(pointer_id,
                                 node_to_be_removed.parentElement());
      original_element_under_pointer_removed_.insert(pointer_id);
      // TODO(https://crbug.com/1496482): Do we need something similar to the
      // logic in EventPath::CalculatePath()?
    }
  }
}

void PointerEventManager::HandlePointerInterruption(
    const WebPointerEvent& web_pointer_event) {
  DCHECK(web_pointer_event.GetType() ==
         WebInputEvent::Type::kPointerCausedUaAction);

  HeapVector<Member<PointerEvent>> canceled_pointer_events;
  if (web_pointer_event.pointer_type ==
      WebPointerProperties::PointerType::kMouse) {
    canceled_pointer_events.push_back(
        pointer_event_factory_.CreatePointerCancelEvent(
            PointerEventFactory::kMouseId, web_pointer_event.TimeStamp(),
            web_pointer_event.device_id));
  } else {
    // TODO(nzolghadr): Maybe canceling all the non-hovering pointers is not
    // the best strategy here. See the github issue for more details:
    // https://github.com/w3c/pointerevents/issues/226

    // Cancel all non-hovering pointers if the pointer is not mouse.
    if (!non_hovering_pointers_canceled_) {
      Vector<PointerId> non_hovering_pointer_ids =
          pointer_event_factory_.GetPointerIdsOfNonHoveringPointers();

      for (PointerId pointer_id : non_hovering_pointer_ids) {
        canceled_pointer_events.push_back(
            pointer_event_factory_.CreatePointerCancelEvent(
                pointer_id, web_pointer_event.TimeStamp(),
                web_pointer_event.device_id));
      }

      non_hovering_pointers_canceled_ = true;
    }
  }

  for (auto pointer_event : canceled_pointer_events) {
    // If we are sending a pointercancel we have sent the pointerevent to some
    // target before.
    Element* target = nullptr;
    if (element_under_pointer_.Contains(pointer_event->pointerId()))
      target = element_under_pointer_.at(pointer_event->pointerId());

    DispatchPointerEvent(
        GetEffectiveTargetForPointerEvent(target, pointer_event->pointerId()),
        pointer_event);

    ReleasePointerCapture(pointer_event->pointerId());

    // Send the leave/out events and lostpointercapture if needed.
    // Note that for mouse due to the web compat we still don't send the
    // boundary events and for now only send lostpointercapture if needed.
    // Sending boundary events and possibly updating hover for mouse
    // in this case may cause some of the existing pages to break.
    if (web_pointer_event.pointer_type ==
        WebPointerProperties::PointerType::kMouse) {
      ProcessPendingPointerCapture(pointer_event);
    } else {
      ProcessCaptureAndPositionOfPointerEvent(pointer_event, nullptr);
    }

    RemovePointer(pointer_event);
  }
}

bool PointerEventManager::ShouldAdjustPointerEvent(
    const WebPointerEvent& pointer_event) const {
  return (pointer_event.pointer_type ==
              WebPointerProperties::PointerType::kTouch ||
          ShouldAdjustStylusPointerEvent(pointer_event)) &&
         pointer_event.GetType() == WebInputEvent::Type::kPointerDown &&
         pointer_event_factory_.IsPrimary(pointer_event);
}

bool PointerEventManager::ShouldAdjustStylusPointerEvent(
    const WebPointerEvent& pointer_event) const {
  return base::FeatureList::IsEnabled(
             blink::features::kStylusPointerAdjustment) &&
         (pointer_event.pointer_type ==
              WebPointerProperties::PointerType::kPen ||
          pointer_event.pointer_type ==
              WebPointerProperties::PointerType::kEraser);
}

void PointerEventManager::AdjustPointerEvent(WebPointerEvent& pointer_event) {
  DCHECK(
      pointer_event.pointer_type == WebPointerProperties::PointerType::kTouch ||
      pointer_event.pointer_type == WebPointerProperties::PointerType::kPen ||
      pointer_event.pointer_type == WebPointerProperties::PointerType::kEraser);

  Node* adjusted_node = nullptr;
  AdjustPointerEvent(pointer_event, adjusted_node);
}

void PointerEventManager::AdjustPointerEvent(WebPointerEvent& pointer_event,
                                             Node*& adjusted_node) {
  float adjustment_width = 0.0f;
  float adjustment_height = 0.0f;
  if (pointer_event.pointer_type == WebPointerProperties::PointerType::kTouch) {
    adjustment_width = pointer_event.width;
    adjustment_height = pointer_event.height;
  } else {
    // Calculate adjustment size for stylus tool types.
    ChromeClient& chrome_client = frame_->GetChromeClient();
    float device_scale_factor =
        chrome_client.GetScreenInfo(*frame_).device_scale_factor;

    float page_scale_factor = frame_->GetPage()->PageScaleFactor();
    adjustment_width = adjustment_height =
        kStylusWritableAdjustmentSizeDip *
        (device_scale_factor / page_scale_factor);
  }

  PhysicalSize hit_rect_size = GetHitTestRectForAdjustment(
      *frame_, PhysicalSize(LayoutUnit(adjustment_width),
                            LayoutUnit(adjustment_height)));

  if (hit_rect_size.IsEmpty())
    return;

  HitTestRequest::HitTestRequestType hit_type =
      HitTestRequest::kTouchEvent | HitTestRequest::kReadOnly |
      HitTestRequest::kActive | HitTestRequest::kListBased;
  LocalFrame& root_frame = frame_->LocalFrameRoot();
  // TODO(szager): Shouldn't this be PositionInScreen() ?
  PhysicalOffset hit_test_point =
      PhysicalOffset::FromPointFRound(pointer_event.PositionInWidget());
  hit_test_point -= PhysicalOffset(LayoutUnit(hit_rect_size.width * 0.5f),
                                   LayoutUnit(hit_rect_size.height * 0.5f));
  HitTestLocation location(PhysicalRect(hit_test_point, hit_rect_size));
  HitTestResult hit_test_result =
      root_frame.GetEventHandler().HitTestResultAtLocation(location, hit_type);
  gfx::Point adjusted_point;

  if (pointer_event.pointer_type == WebPointerProperties::PointerType::kTouch) {
    bool adjusted = frame_->GetEventHandler().BestNodeForHitTestResult(
        TouchAdjustmentCandidateType::kClickable, location, hit_test_result,
        adjusted_point, adjusted_node);

    if (adjusted)
      pointer_event.SetPositionInWidget(adjusted_point.x(), adjusted_point.y());

    frame_->GetEventHandler().CacheTouchAdjustmentResult(
        pointer_event.unique_touch_event_id, pointer_event.PositionInWidget());
  } else if (pointer_event.pointer_type ==
                 WebPointerProperties::PointerType::kPen ||
             pointer_event.pointer_type ==
                 WebPointerProperties::PointerType::kEraser) {
    // We don't cache the adjusted point for Stylus in EventHandler to avoid
    // taps being adjusted; this is intended only for stylus handwriting.
    bool adjusted = frame_->GetEventHandler().BestNodeForHitTestResult(
        TouchAdjustmentCandidateType::kStylusWritable, location,
        hit_test_result, adjusted_point, adjusted_node);

    if (adjusted)
      pointer_event.SetPositionInWidget(adjusted_point.x(), adjusted_point.y());
  }
}

bool PointerEventManager::ShouldFilterEvent(PointerEvent* pointer_event) {
  // Filter as normal if the experiment is disabled.
  if (!skip_touch_filter_discrete_)
    return true;

  // If the experiment is enabled and the event is pointer up/down, do not
  // filter.
  if (pointer_event->type() == event_type_names::kPointerdown ||
      pointer_event->type() == event_type_names::kPointerup) {
    return false;
  }
  // If the experiment is "all", do not filter pointermove.
  if (skip_touch_filter_all_ &&
      pointer_event->type() == event_type_names::kPointermove)
    return false;

  // Continue filtering other types of events, even thought the experiment is
  // enabled.
  return true;
}

event_handling_util::PointerEventTarget
PointerEventManager::ComputePointerEventTarget(
    const WebPointerEvent& web_pointer_event) {
  event_handling_util::PointerEventTarget pointer_event_target;

  PointerId pointer_id =
      pointer_event_factory_.GetPointerEventId(web_pointer_event);
  // Do the hit test either when the touch first starts or when the touch
  // is not captured. |m_pendingPointerCaptureTarget| indicates the target
  // that will be capturing this event. |m_pointerCaptureTarget| may not
  // have this target yet since the processing of that will be done right
  // before firing the event.
  if (web_pointer_event.GetType() == WebInputEvent::Type::kPointerDown ||
      !pending_pointer_capture_target_.Contains(pointer_id)) {
    HitTestRequest::HitTestRequestType hit_type = HitTestRequest::kTouchEvent |
                                                  HitTestRequest::kReadOnly |
                                                  HitTestRequest::kActive;
    HitTestLocation location(frame_->View()->ConvertFromRootFrame(
        PhysicalOffset::FromPointFRound(web_pointer_event.PositionInWidget())));
    HitTestResult hit_test_result =
        frame_->GetEventHandler().HitTestResultAtLocation(location, hit_type);
    Element* target = hit_test_result.InnerElement();
    if (target) {
      pointer_event_target.target_frame = target->GetDocument().GetFrame();
      pointer_event_target.target_element = target;
      pointer_event_target.scrollbar = hit_test_result.GetScrollbar();
    }
  } else {
    // Set the target of pointer event to the captured element as this
    // pointer is captured otherwise it would have gone to the |if| block
    // and perform a hit-test.
    pointer_event_target.target_element =
        pending_pointer_capture_target_.at(pointer_id);
    pointer_event_target.target_frame =
        pointer_event_target.target_element->GetDocument().GetFrame();
  }
  return pointer_event_target;
}

WebInputEventResult PointerEventManager::DispatchTouchPointerEvent(
    const WebPointerEvent& web_pointer_event,
    const Vector<WebPointerEvent>& coalesced_events,
    const Vector<WebPointerEvent>& predicted_events,
    const event_handling_util::PointerEventTarget& pointer_event_target) {
  DCHECK_NE(web_pointer_event.GetType(),
            WebInputEvent::Type::kPointerCausedUaAction);

  WebInputEventResult result = WebInputEventResult::kHandledSystem;
  if (pointer_event_target.target_element &&
      pointer_event_target.target_frame && !non_hovering_pointers_canceled_) {
    SetLastPointerPositionForFrameBoundary(web_pointer_event,
                                           pointer_event_target.target_element);

    PointerEvent* pointer_event = pointer_event_factory_.Create(
        web_pointer_event, coalesced_events, predicted_events,
        pointer_event_target.target_element
            ? pointer_event_target.target_element->GetDocument().domWindow()
            : nullptr);

    if (pointer_event) {
      result = SendTouchPointerEvent(pointer_event_target.target_element,
                                     pointer_event, web_pointer_event.hovering);
    } else {
      result = WebInputEventResult::kNotHandled;
    }

    // If a pointerdown has been canceled, queue the unique id to allow
    // suppressing mouse events from gesture events. For mouse events
    // fired from GestureTap & GestureLongPress (which are triggered by
    // single touches only), it is enough to queue the ids only for
    // primary pointers.
    // TODO(mustaq): What about other cases (e.g. GestureTwoFingerTap)?
    if (result != WebInputEventResult::kNotHandled &&
        pointer_event->type() == event_type_names::kPointerdown &&
        pointer_event->isPrimary()) {
      touch_ids_for_canceled_pointerdowns_.push_back(
          web_pointer_event.unique_touch_event_id);
    }
  }
  return result;
}

WebInputEventResult PointerEventManager::SendTouchPointerEvent(
    Element* target,
    PointerEvent* pointer_event,
    bool hovering) {
  if (non_hovering_pointers_canceled_)
    return WebInputEventResult::kNotHandled;

  ProcessCaptureAndPositionOfPointerEvent(pointer_event, target);

  // Setting the implicit capture for touch
  if (pointer_event->type() == event_type_names::kPointerdown) {
    SetPointerCapture(pointer_event->pointerId(), target,
                      /* explicit_capture */ false);
  }

  WebInputEventResult result = DispatchPointerEvent(
      GetEffectiveTargetForPointerEvent(target, pointer_event->pointerId()),
      pointer_event);

  if (pointer_event->type() == event_type_names::kPointerup ||
      pointer_event->type() == event_type_names::kPointercancel) {
    ReleasePointerCapture(pointer_event->pointerId());

    // If the pointer is not hovering it implies that pointerup also means
    // leaving the screen and the end of the stream for that pointer. So
    // we should send boundary events as well.
    if (!hovering) {
      // Sending the leave/out events and lostpointercapture because the next
      // touch event will have a different id.
      ProcessCaptureAndPositionOfPointerEvent(pointer_event, nullptr);

      RemovePointer(pointer_event);
    }
  }

  return result;
}

WebInputEventResult PointerEventManager::FlushEvents() {
  WebInputEventResult result = touch_event_manager_->FlushEvents();
  return result;
}

WebInputEventResult PointerEventManager::HandlePointerEvent(
    const WebPointerEvent& event,
    const Vector<WebPointerEvent>& coalesced_events,
    const Vector<WebPointerEvent>& predicted_events) {
  if (event.GetType() == WebInputEvent::Type::kPointerRawUpdate) {
    if (!frame_->GetEventHandlerRegistry().HasEventHandlers(
            EventHandlerRegistry::kPointerRawUpdateEvent))
      return WebInputEventResult::kHandledSystem;

    // If the page has pointer lock active and the event was from
    // mouse use the locked target as the target.
    // TODO(nzolghadr): Consideration for locked element might fit
    // better in ComputerPointerEventTarget but at this point it is
    // not quite possible as we haven't merged the locked event
    // dispatch with this path.
    Node* target;
    Element* pointer_locked_element =
        PointerLockController::GetPointerLockedElement(frame_);
    if (pointer_locked_element &&
        event.pointer_type == WebPointerProperties::PointerType::kMouse) {
      // The locked element could be in another frame. So we need to delegate
      // sending the event to that frame.
      LocalFrame* target_frame =
          pointer_locked_element->GetDocument().GetFrame();
      if (!target_frame)
        return WebInputEventResult::kHandledSystem;
      if (target_frame != frame_) {
        target_frame->GetEventHandler().HandlePointerEvent(
            event, coalesced_events, predicted_events);
        return WebInputEventResult::kHandledSystem;
      }
      target = pointer_locked_element;
    } else {
      target = ComputePointerEventTarget(event).target_element;
    }

    PointerEvent* pointer_event =
        pointer_event_factory_.Create(event, coalesced_events, predicted_events,
                                      frame_->GetDocument()->domWindow());
    // The conditional return below is deliberately placed after the Create()
    // call above because of some side-effects of Create() (in particular
    // SetLastPosition()) is needed even with the early return below.  See
    // crbug.com/1066544.
    //
    // Sometimes the Browser process tags events with kRelativeMotionEvent.
    // E.g. during pointer lock, it recenters cursor by warping so that cursor
    // does not hit the screen boundary.  Those fake events should not be
    // forwarded to the DOM.
    if (event.GetModifiers() & WebInputEvent::Modifiers::kRelativeMotionEvent)
      return WebInputEventResult::kHandledSuppressed;

    if (pointer_event) {
      // TODO(crbug.com/1141595): We should handle this case further upstream.
      DispatchPointerEvent(target, pointer_event);
    }
    return WebInputEventResult::kHandledSystem;
  }

  if (event.GetType() == WebInputEvent::Type::kPointerCausedUaAction) {
    HandlePointerInterruption(event);
    return WebInputEventResult::kHandledSystem;
  }

  // The rest of this function doesn't handle hovering (i.e. mouse like) events.

  WebPointerEvent pointer_event = event.WebPointerEventInRootFrame();
  if (ShouldAdjustPointerEvent(event))
    AdjustPointerEvent(pointer_event);
  event_handling_util::PointerEventTarget pointer_event_target =
      ComputePointerEventTarget(pointer_event);

  bool is_pointer_down = event.GetType() == WebInputEvent::Type::kPointerDown;
  if (is_pointer_down && discarded_event_.target != kInvalidDOMNodeId &&
      discarded_event_.target ==
          pointer_event_target.target_element->GetDomNodeId() &&
      pointer_event.TimeStamp() - discarded_event_.time <
          event_handling_util::kDiscardedEventMistakeInterval) {
    pointer_event_target.target_element->GetDocument().CountUse(
        WebFeature::kInputEventToRecentlyMovedIframeMistakenlyDiscarded);
  }
  bool discard = pointer_event_target.target_frame &&
                 event_handling_util::ShouldDiscardEventTargetingFrame(
                     event, *pointer_event_target.target_frame);
  if (discard) {
    if (is_pointer_down) {
      discarded_event_.target =
          pointer_event_target.target_element->GetDomNodeId();
      discarded_event_.time = pointer_event.TimeStamp();
    }
    PointerEvent* core_pointer_event = pointer_event_factory_.Create(
        event, coalesced_events, predicted_events,
        pointer_event_target.target_element
            ? pointer_event_target.target_element->GetDocument().domWindow()
            : nullptr);
    if (core_pointer_event) {
      // TODO(crbug.com/1141595): We should handle this case further upstream.
      SendTouchPointerEvent(
          pointer_event_target.target_element,
          pointer_event_factory_.CreatePointerCancelEvent(
              core_pointer_event->pointerId(), event.TimeStamp(),
              core_pointer_event->persistentDeviceId()),
          event.hovering);
    }

    WebPointerEvent pointer_cancel_event;
    pointer_cancel_event.pointer_type = event.pointer_type;
    pointer_cancel_event.SetTimeStamp(event.TimeStamp());
    pointer_cancel_event.SetType(WebInputEvent::Type::kPointerCancel);
    touch_event_manager_->HandleTouchPoint(
        pointer_cancel_event, coalesced_events, pointer_event_target);

    return WebInputEventResult::kHandledSuppressed;
  }

  if (is_pointer_down) {
    discarded_event_.target = kInvalidDOMNodeId;
    discarded_event_.time = base::TimeTicks();
  }

  if (HandleScrollbarTouchDrag(event, pointer_event_target.scrollbar))
    return WebInputEventResult::kHandledSuppressed;

  if (HandleResizerDrag(pointer_event, pointer_event_target))
    return WebInputEventResult::kHandledSuppressed;

  // Any finger lifting is a user gesture only when it wasn't associated with a
  // scroll.
  // https://docs.google.com/document/d/1oF1T3O7_E4t1PYHV6gyCwHxOi3ystm0eSL5xZu7nvOg/edit#
  //
  // For the rare case of multi-finger scenarios spanning documents, it
  // seems extremely unlikely to matter which document the gesture is
  // associated with so just pick the pointer event that comes.
  if (event.GetType() == WebInputEvent::Type::kPointerUp &&
      !non_hovering_pointers_canceled_ && pointer_event_target.target_frame) {
    LocalFrame::NotifyUserActivation(
        pointer_event_target.target_frame,
        mojom::blink::UserActivationNotificationType::kInteraction);
  }

  if (!event.hovering && !IsAnyTouchActive()) {
    non_hovering_pointers_canceled_ = false;
  }
  Node* pointerdown_node = nullptr;
  if (is_pointer_down) {
    pointerdown_node =
        touch_event_manager_->GetTouchPointerNode(event, pointer_event_target);
  }

  if (pointerdown_node) {
    TouchAction touch_action =
        touch_action_util::EffectiveTouchActionAtPointerDown(event,
                                                             pointerdown_node);
    touch_event_manager_->UpdateTouchAttributeMapsForPointerDown(
        event, pointerdown_node, touch_action);
  }

  WebInputEventResult result = DispatchTouchPointerEvent(
      event, coalesced_events, predicted_events, pointer_event_target);

  touch_event_manager_->HandleTouchPoint(event, coalesced_events,
                                         pointer_event_target);

  return result;
}

bool PointerEventManager::HandleScrollbarTouchDrag(const WebPointerEvent& event,
                                                   Scrollbar* scrollbar) {
  if (!scrollbar ||
      (event.pointer_type != WebPointerProperties::PointerType::kTouch &&
       event.pointer_type != WebPointerProperties::PointerType::kPen)) {
    return false;
  }

  if (event.GetType() == WebInputEvent::Type::kPointerDown) {
    captured_scrollbar_ = scrollbar;
    frame_->GetPage()->GetChromeClient().SetTouchAction(frame_,
                                                        TouchAction::kNone);
  }

  if (!captured_scrollbar_)
    return false;

  bool handled = captured_scrollbar_->HandlePointerEvent(event);
  if (event.GetType() == WebInputEvent::Type::kPointerUp)
    captured_scrollbar_ = nullptr;

  return handled;
}

bool PointerEventManager::HandleResizerDrag(
    const WebPointerEvent& event,
    const event_handling_util::PointerEventTarget& pointer_event_target) {
  switch (event.GetType()) {
    case WebPointerEvent::Type::kPointerDown: {
      Node* node = pointer_event_target.target_element;
      if (!node || !node->GetLayoutObject() ||
          !node->GetLayoutObject()->EnclosingLayer())
        return false;

      PaintLayer* layer = node->GetLayoutObject()->EnclosingLayer();
      if (!layer->GetScrollableArea())
        return false;

      gfx::Point p =
          pointer_event_target.target_frame->View()->ConvertFromRootFrame(
              gfx::ToFlooredPoint(event.PositionInWidget()));
      if (layer->GetScrollableArea()->IsAbsolutePointInResizeControl(
              p, kResizerForTouch)) {
        resize_scrollable_area_ = layer->GetScrollableArea();
        resize_scrollable_area_->SetInResizeMode(true);
        frame_->GetPage()->GetChromeClient().SetTouchAction(frame_,
                                                            TouchAction::kNone);
        offset_from_resize_corner_ =
            resize_scrollable_area_->OffsetFromResizeCorner(p);
        return true;
      }
      break;
    }
    case WebInputEvent::Type::kPointerMove: {
      if (resize_scrollable_area_ && resize_scrollable_area_->Layer() &&
          resize_scrollable_area_->Layer()->GetLayoutBox() &&
          resize_scrollable_area_->InResizeMode()) {
        gfx::Point pos = gfx::ToRoundedPoint(event.PositionInWidget());
        resize_scrollable_area_->Resize(pos, offset_from_resize_corner_);
        return true;
      }
      break;
    }
    case WebInputEvent::Type::kPointerUp: {
      if (resize_scrollable_area_ && resize_scrollable_area_->InResizeMode()) {
        resize_scrollable_area_->SetInResizeMode(false);
        resize_scrollable_area_.Clear();
        offset_from_resize_corner_ = {};
        return true;
      }
      break;
    }
    default:
      return false;
  }
  return false;
}

WebInputEventResult PointerEventManager::CreateAndDispatchPointerEvent(
    Element* target,
    const AtomicString& mouse_event_name,
    const WebMouseEvent& mouse_event,
    const Vector<WebMouseEvent>& coalesced_events,
    const Vector<WebMouseEvent>& predicted_events) {
  WebInputEvent::Type event_type;
  // TODO(crbug.com/665924): The following ifs skip the mouseover/leave cases,
  // we should fixed them when further merge the code path.
  if (mouse_event_name == event_type_names::kMousemove)
    event_type = WebInputEvent::Type::kPointerMove;
  else if (mouse_event_name == event_type_names::kMousedown)
    event_type = WebInputEvent::Type::kPointerDown;
  else if (mouse_event_name == event_type_names::kMouseup)
    event_type = WebInputEvent::Type::kPointerUp;
  else
    return WebInputEventResult::kNotHandled;

  const WebPointerEvent web_pointer_event(event_type, mouse_event);
  Vector<WebPointerEvent> pointer_coalesced_events;
  for (const WebMouseEvent& e : coalesced_events)
    pointer_coalesced_events.push_back(WebPointerEvent(event_type, e));
  Vector<WebPointerEvent> pointer_predicted_events;
  for (const WebMouseEvent& e : predicted_events)
    pointer_predicted_events.push_back(WebPointerEvent(event_type, e));

  PointerEvent* pointer_event = pointer_event_factory_.Create(
      web_pointer_event, pointer_coalesced_events, pointer_predicted_events,
      target->GetDocument().domWindow());
  DCHECK(pointer_event);

  ProcessCaptureAndPositionOfPointerEvent(pointer_event, target, &mouse_event);

  return DispatchPointerEvent(target, pointer_event);
}

// TODO(crbug.com/665924): Because this code path might have boundary events,
// it is different from SendMousePointerEvent. We should merge them.
WebInputEventResult PointerEventManager::DirectDispatchMousePointerEvent(
    Element* target,
    const WebMouseEvent& event,
    const AtomicString& mouse_event_type,
    const Vector<WebMouseEvent>& coalesced_events,
    const Vector<WebMouseEvent>& predicted_events) {
  if (!(event.GetModifiers() &
        WebInputEvent::Modifiers::kRelativeMotionEvent)) {
    // Fetch the last_mouse_position for creating MouseEvent before
    // pointer_event_factory updates it.
    gfx::PointF last_mouse_position =
        pointer_event_factory_.GetLastPointerPosition(
            PointerEventFactory::kMouseId, event, event.GetType());

    WebInputEventResult result = CreateAndDispatchPointerEvent(
        target, mouse_event_type, event, coalesced_events, predicted_events);

    result = event_handling_util::MergeEventResult(
        result,
        mouse_event_manager_->DispatchMouseEvent(
            target, mouse_event_type, event, &last_mouse_position, nullptr));
    return result;
  }
  pointer_event_factory_.SetLastPosition(
      pointer_event_factory_.GetPointerEventId(event), event.PositionInScreen(),
      event.GetType());

  return WebInputEventResult::kHandledSuppressed;
}

void PointerEventManager::SendEffectivePanActionAtPointer(
    const WebPointerEvent& event,
    const Node* node_at_pointer) {
  if (IsAnyTouchActive())
    return;

  if (ShouldAdjustStylusPointerEvent(event)) {
    Node* adjusted_node = nullptr;
    // Check if node adjustment allows stylus writing. Use a cloned event to
    // avoid adjusting actual pointer's position.
    std::unique_ptr<WebInputEvent> cloned_event = event.Clone();
    WebPointerEvent& cloned_pointer_event =
        static_cast<WebPointerEvent&>(*cloned_event);
    AdjustPointerEvent(cloned_pointer_event, adjusted_node);
    if (adjusted_node) {
      node_at_pointer = adjusted_node;
    }
  }

  TouchAction effective_touch_action = TouchAction::kAuto;
  if (node_at_pointer) {
    effective_touch_action = touch_action_util::EffectiveTouchActionAtPointer(
        event, node_at_pointer);
  }

  mojom::blink::PanAction effective_pan_action;
  if ((effective_touch_action & TouchAction::kPan) == TouchAction::kNone) {
    // Stylus writing or move cursor are applicable only when touch action
    // allows panning in at least one direction.
    effective_pan_action = mojom::blink::PanAction::kNone;
  } else if ((effective_touch_action & TouchAction::kInternalNotWritable) !=
             TouchAction::kInternalNotWritable) {
    // kInternalNotWritable bit is re-enabled, if tool type is not stylus.
    // Hence, if this bit is not set, stylus writing is possible.
    effective_pan_action = mojom::blink::PanAction::kStylusWritable;
  } else if ((effective_touch_action & TouchAction::kInternalPanXScrolls) !=
             TouchAction::kInternalPanXScrolls) {
    effective_pan_action = mojom::blink::PanAction::kMoveCursorOrScroll;
  } else {
    effective_pan_action = mojom::blink::PanAction::kScroll;
  }

  frame_->GetChromeClient().SetPanAction(frame_, effective_pan_action);
}

namespace {

Element* NonDeletedElementTarget(Element* target,
                                 PointerEvent* dispatched_pointer_event) {
  // Event path could be null if the pointer event failed to get dispatched.
  bool has_event_path = dispatched_pointer_event->HasEventPath();

  if (!event_handling_util::IsInDocument(target) && has_event_path) {
    for (const auto& context :
         dispatched_pointer_event->GetEventPath().NodeEventContexts()) {
      auto* element = DynamicTo<Element>(&context.GetNode());
      if (element && event_handling_util::IsInDocument(element)) {
        return element;
      }
    }
  }
  return target;
}

}  // namespace

WebInputEventResult PointerEventManager::SendMousePointerEvent(
    Element* target,
    const WebInputEvent::Type event_type,
    const WebMouseEvent& mouse_event,
    const Vector<WebMouseEvent>& coalesced_events,
    const Vector<WebMouseEvent>& predicted_events,
    bool skip_click_dispatch) {
  DCHECK(event_type == WebInputEvent::Type::kPointerDown ||
         event_type == WebInputEvent::Type::kPointerMove ||
         event_type == WebInputEvent::Type::kPointerUp);

  const WebPointerEvent web_pointer_event(event_type, mouse_event);
  Vector<WebPointerEvent> pointer_coalesced_events;
  for (const WebMouseEvent& e : coalesced_events)
    pointer_coalesced_events.push_back(WebPointerEvent(event_type, e));
  Vector<WebPointerEvent> pointer_predicted_events;
  for (const WebMouseEvent& e : predicted_events)
    pointer_predicted_events.push_back(WebPointerEvent(event_type, e));

  // Fetch the last_mouse_position for creating MouseEvent before
  // pointer_event_factory updates it.
  gfx::PointF last_mouse_position =
      pointer_event_factory_.GetLastPointerPosition(
          pointer_event_factory_.GetPointerEventId(mouse_event), mouse_event,
          event_type);

  bool fake_event = (web_pointer_event.GetModifiers() &
                     WebInputEvent::Modifiers::kRelativeMotionEvent);

  // Fake events should only be move events.
  DCHECK(!fake_event || event_type == WebInputEvent::Type::kPointerMove);

  PointerEvent* pointer_event = pointer_event_factory_.Create(
      web_pointer_event, pointer_coalesced_events, pointer_predicted_events,
      frame_->GetDocument()->domWindow());
  DCHECK(pointer_event);

  // This is for when the mouse is released outside of the page.
  if (!fake_event && event_type == WebInputEvent::Type::kPointerMove &&
      !pointer_event->buttons()) {
    ReleasePointerCapture(pointer_event->pointerId());
    // Send got/lostpointercapture rightaway if necessary.
    ProcessPendingPointerCapture(pointer_event);

    if (pointer_event->isPrimary()) {
      prevent_mouse_event_for_pointer_type_[ToPointerTypeIndex(
          web_pointer_event.pointer_type)] = false;
    }
  }

  // TODO(https://crbug.com/1500354): We should not pass the `mouse_event`
  // parameter in the call below because we don't want to send the boundary
  // MouseEvents before dispatching the PointerEvent.  Otherwise, a DOM
  // modification through the PointerEvent handler gives a wrong sequence of
  // boundary MouseEvent.
  Element* effective_target = ProcessCaptureAndPositionOfPointerEvent(
      pointer_event, target, &mouse_event);

  // Don't send fake mouse event to the DOM.
  if (fake_event)
    return WebInputEventResult::kHandledSuppressed;

  if ((event_type == WebInputEvent::Type::kPointerDown ||
       event_type == WebInputEvent::Type::kPointerUp) &&
      pointer_event->type() == event_type_names::kPointermove &&
      frame_->GetEventHandlerRegistry().HasEventHandlers(
          EventHandlerRegistry::kPointerRawUpdateEvent)) {
    // This is a chorded button move event. We need to also send a
    // pointerrawupdate for it.
    DispatchPointerEvent(
        effective_target,
        pointer_event_factory_.CreatePointerRawUpdateEvent(pointer_event));
  }

  WebInputEventResult result =
      DispatchPointerEvent(effective_target, pointer_event);

  if (result != WebInputEventResult::kNotHandled &&
      pointer_event->type() == event_type_names::kPointerdown &&
      pointer_event->isPrimary()) {
    prevent_mouse_event_for_pointer_type_[ToPointerTypeIndex(
        mouse_event.pointer_type)] = true;
  }

  bool send_compat_mouse =
      pointer_event->isPrimary() &&
      !prevent_mouse_event_for_pointer_type_[ToPointerTypeIndex(
          mouse_event.pointer_type)];
  bool consider_click_dispatch = !skip_click_dispatch &&
                                 pointer_event->isPrimary() &&
                                 event_type == WebInputEvent::Type::kPointerUp;

  // Calculate mouse target if either compatibility mouse event or click event
  // or both should be sent.
  Element* mouse_target = nullptr;
  if (send_compat_mouse || consider_click_dispatch) {
    mouse_target =
        RuntimeEnabledFeatures::BoundaryEventDispatchTracksNodeRemovalEnabled()
            ? mouse_event_manager_->GetElementUnderMouse()
            : NonDeletedElementTarget(effective_target, pointer_event);
  }

  // Dispatch compat mouse events.
  if (send_compat_mouse) {
    result = event_handling_util::MergeEventResult(
        result,
        mouse_event_manager_->DispatchMouseEvent(
            mouse_target, MouseEventNameForPointerEventInputType(event_type),
            mouse_event, &last_mouse_position, nullptr));
  }

  if (!mouse_target) {
    consider_click_dispatch = false;
  }

  Element* captured_click_target = nullptr;
  if (consider_click_dispatch) {
    // Remember the capture target for the click dispatch later, if applicable.
    captured_click_target =
        GetEffectiveTargetForPointerEvent(nullptr, pointer_event->pointerId());
    // Dispatch the click event only when the flag is disabled.
    if (!RuntimeEnabledFeatures::ClickToCapturedPointerEnabled()) {
      mouse_event_manager_->DispatchMouseClickIfNeeded(
          mouse_target, captured_click_target, mouse_event,
          pointer_event->pointerId(), pointer_event->pointerType());
    }
  }

  if (pointer_event->type() == event_type_names::kPointerup ||
      pointer_event->type() == event_type_names::kPointercancel) {
    ReleasePointerCapture(pointer_event->pointerId());

    // Send got/lostpointercapture rightaway if necessary.
    if (pointer_event->type() == event_type_names::kPointerup) {
      // We also send boundary events here rightaway.  To find the new position
      // under the pointer, we perform a hit-test again if a pointer-capture is
      // going to be released now; otherwise we use the original hit-test target
      // (or its ancestor in the event-path if it has been removed from DOM).
      if (pointer_capture_target_.find(pointer_event->pointerId()) !=
          pointer_capture_target_.end()) {
        HitTestRequest::HitTestRequestType hit_type = HitTestRequest::kRelease;
        HitTestRequest request(hit_type);
        MouseEventWithHitTestResults mev =
            event_handling_util::PerformMouseEventHitTest(frame_, request,
                                                          mouse_event);
        target = mev.InnerElement();
      } else if (RuntimeEnabledFeatures::
                     BoundaryEventDispatchTracksNodeRemovalEnabled()) {
        target = NonDeletedElementTarget(target, pointer_event);
      }

      // Dispatch the click event if applicable, when the flag is enabled.
      if (consider_click_dispatch &&
          RuntimeEnabledFeatures::ClickToCapturedPointerEnabled()) {
        ProcessPendingPointerCapture(pointer_event);
        mouse_event_manager_->DispatchMouseClickIfNeeded(
            mouse_target, captured_click_target, mouse_event,
            pointer_event->pointerId(), pointer_event->pointerType());
        // TODO(https://crbug.com/40851596): The following call to
        // `ProcessCaptureAndPositionOfPointerEvent()` does not see any pending
        // capture.  Clean this up after the flag is enabled.
      }

      ProcessCaptureAndPositionOfPointerEvent(pointer_event, target,
                                              &mouse_event);
    } else {
      // Don't send boundary events in this case as it is a little tricky.
      // This case happens for the drag operation and currently we don't
      // let the page know that the pointer left the page while dragging.
      ProcessPendingPointerCapture(pointer_event);
    }

    if (pointer_event->isPrimary()) {
      prevent_mouse_event_for_pointer_type_[ToPointerTypeIndex(
          mouse_event.pointer_type)] = false;
    }
  }

  if (mouse_event.GetType() == WebInputEvent::Type::kMouseLeave &&
      mouse_event.pointer_type == WebPointerProperties::PointerType::kPen) {
    pointer_event_factory_.Remove(pointer_event->pointerId());
  }
  return result;
}

bool PointerEventManager::GetPointerCaptureState(
    PointerId pointer_id,
    Element** pointer_capture_target,
    Element** pending_pointer_capture_target) {
  DCHECK(pointer_capture_target);
  DCHECK(pending_pointer_capture_target);

  PointerCapturingMap::const_iterator it;

  it = pointer_capture_target_.find(pointer_id);
  Element* pointer_capture_target_temp =
      (it != pointer_capture_target_.end()) ? it->value : nullptr;
  it = pending_pointer_capture_target_.find(pointer_id);
  Element* pending_pointercapture_target_temp =
      (it != pending_pointer_capture_target_.end()) ? it->value : nullptr;

  *pointer_capture_target = pointer_capture_target_temp;
  *pending_pointer_capture_target = pending_pointercapture_target_temp;

  return pointer_capture_target_temp != pending_pointercapture_target_temp;
}

Element* PointerEventManager::ProcessCaptureAndPositionOfPointerEvent(
    PointerEvent* pointer_event,
    Element* hit_test_target,
    const WebMouseEvent* mouse_event) {
  ProcessPendingPointerCapture(pointer_event);

  Element* effective_target = GetEffectiveTargetForPointerEvent(
      hit_test_target, pointer_event->pointerId());

  SetElementUnderPointer(pointer_event, effective_target);
  if (mouse_event) {
    mouse_event_manager_->SetElementUnderMouse(effective_target, *mouse_event);
  }
  return effective_target;
}

void PointerEventManager::ProcessPendingPointerCapture(
    PointerEvent* pointer_event) {
  Element* pointer_capture_target = nullptr;
  Element* pending_pointer_capture_target = nullptr;

  const PointerId pointer_id = pointer_event->pointerId();
  const bool is_capture_changed = GetPointerCaptureState(
      pointer_id, &pointer_capture_target, &pending_pointer_capture_target);

  if (!is_capture_changed)
    return;

  // We have to check whether the pointerCaptureTarget is null or not because
  // we are checking whether it is still connected to its document or not.
  if (pointer_capture_target) {
    // Re-target lostpointercapture to the document when the element is
    // no longer participating in the tree.
    EventTarget* target = pointer_capture_target;
    if (!pointer_capture_target->isConnected()) {
      target = pointer_capture_target->ownerDocument();
    }
    pointer_capture_target_.erase(pointer_id);
    DispatchPointerEvent(
        target, pointer_event_factory_.CreatePointerCaptureEvent(
                    pointer_event, event_type_names::kLostpointercapture));
  }

  if (pending_pointer_capture_target &&
      pending_pointer_capture_target->isConnected()) {
    SetElementUnderPointer(pointer_event, pending_pointer_capture_target);
    DispatchPointerEvent(
        pending_pointer_capture_target,
        pointer_event_factory_.CreatePointerCaptureEvent(
            pointer_event, event_type_names::kGotpointercapture));
    if (pending_pointer_capture_target->isConnected()) {
      pointer_capture_target_.Set(pointer_id, pending_pointer_capture_target);
    } else {
      // As a result of dispatching gotpointercapture the capture node was
      // removed.
      DispatchPointerEvent(
          pending_pointer_capture_target->ownerDocument(),
          pointer_event_factory_.CreatePointerCaptureEvent(
              pointer_event, event_type_names::kLostpointercapture));
    }
  }
}

void PointerEventManager::RemoveTargetFromPointerCapturingMapping(
    PointerCapturingMap& map,
    const Element* target) {
  // We could have kept a reverse mapping to make this deletion possibly
  // faster but it adds some code complication which might not be worth of
  // the performance improvement considering there might not be a lot of
  // active pointer or pointer captures at the same time.
  PointerCapturingMap tmp = map;
  for (PointerCapturingMap::iterator it = tmp.begin(); it != tmp.end(); ++it) {
    if (it->value == target)
      map.erase(it->key);
  }
}

void PointerEventManager::RemovePointer(PointerEvent* pointer_event) {
  PointerId pointer_id = pointer_event->pointerId();
  if (pointer_event_factory_.Remove(pointer_id)) {
    pending_pointer_capture_target_.erase(pointer_id);
    pointer_capture_target_.erase(pointer_id);
    element_under_pointer_.erase(pointer_id);
    original_element_under_pointer_removed_.erase(pointer_id);
  }
}

void PointerEventManager::ElementRemoved(Element* target) {
  RemoveTargetFromPointerCapturingMapping(pending_pointer_capture_target_,
                                          target);
}

bool PointerEventManager::SetPointerCapture(PointerId pointer_id,
                                            Element* target,
                                            bool explicit_capture) {
  if (explicit_capture) {
    UseCounter::Count(frame_->GetDocument(),
                      WebFeature::kPointerEventSetCapture);
  }
  if (pointer_event_factory_.IsActiveButtonsState(pointer_id)) {
    if (pointer_id != dispatching_pointer_id_) {
      UseCounter::Count(frame_->GetDocument(),
                        WebFeature::kPointerEventSetCaptureOutsideDispatch);
    }
    pending_pointer_capture_target_.Set(pointer_id, target);
    return true;
  }
  return false;
}

bool PointerEventManager::ReleasePointerCapture(PointerId pointer_id,
                                                Element* target) {
  // Only the element that is going to get the next pointer event can release
  // the capture. Note that this might be different from
  // |m_pointercaptureTarget|. |m_pointercaptureTarget| holds the element
  // that had the capture until now and has been receiving the pointerevents
  // but |m_pendingPointerCaptureTarget| indicated the element that gets the
  // very next pointer event. They will be the same if there was no change in
  // capturing of a particular |pointerId|. See crbug.com/614481.
  if (HasPointerCapture(pointer_id, target)) {
    ReleasePointerCapture(pointer_id);
    return true;
  }
  return false;
}

void PointerEventManager::ReleaseMousePointerCapture() {
  ReleasePointerCapture(PointerEventFactory::kMouseId);
}

bool PointerEventManager::HasPointerCapture(PointerId pointer_id,
                                            const Element* target) const {
  const auto it = pending_pointer_capture_target_.find(pointer_id);
  return it != pending_pointer_capture_target_.end() && it->value == target;
}

void PointerEventManager::ReleasePointerCapture(PointerId pointer_id) {
  pending_pointer_capture_target_.erase(pointer_id);
}

Element* PointerEventManager::GetMouseCaptureTarget() {
  if (pending_pointer_capture_target_.Contains(PointerEventFactory::kMouseId))
    return pending_pointer_capture_target_.at(PointerEventFactory::kMouseId);
  return nullptr;
}

bool PointerEventManager::IsActive(const PointerId pointer_id) const {
  return pointer_event_factory_.IsActive(pointer_id);
}

// This function checks the type of the pointer event to be touch as touch
// pointer events are the only ones that are directly dispatched from the main
// page managers to their target (event if target is in an iframe) and only
// those managers will keep track of these pointer events.
bool PointerEventManager::IsPointerIdActiveOnFrame(PointerId pointer_id,
                                                   LocalFrame* frame) const {
  Element* last_element_receiving_event =
      element_under_pointer_.Contains(pointer_id)
          ? element_under_pointer_.at(pointer_id)
          : nullptr;
  return last_element_receiving_event &&
         last_element_receiving_event->GetDocument().GetFrame() == frame;
}

bool PointerEventManager::IsAnyTouchActive() const {
  // TODO(mustaq@chromium.org): Rely on PEF's states instead of TEM's.
  return touch_event_manager_->IsAnyTouchActive();
}

bool PointerEventManager::PrimaryPointerdownCanceled(
    uint32_t unique_touch_event_id) {
  // It's safe to assume that uniqueTouchEventIds won't wrap back to 0 from
  // 2^32-1 (>4.2 billion): even with a generous 100 unique ids per touch
  // sequence & one sequence per 10 second, it takes 13+ years to wrap back.
  while (!touch_ids_for_canceled_pointerdowns_.empty()) {
    uint32_t first_id = touch_ids_for_canceled_pointerdowns_.front();
    if (first_id > unique_touch_event_id)
      return false;
    touch_ids_for_canceled_pointerdowns_.TakeFirst();
    if (first_id == unique_touch_event_id)
      return true;
  }
  return false;
}

void PointerEventManager::SetLastPointerPositionForFrameBoundary(
    const WebPointerEvent& web_pointer_event,
    Element* new_target) {
  PointerId pointer_id =
      pointer_event_factory_.GetPointerEventId(web_pointer_event);
  Element* last_target = element_under_pointer_.Contains(pointer_id)
                             ? element_under_pointer_.at(pointer_id)
                             : nullptr;
  if (!new_target) {
    pointer_event_factory_.RemoveLastPosition(pointer_id);
  } else if (!last_target || new_target->GetDocument().GetFrame() !=
                                 last_target->GetDocument().GetFrame()) {
    pointer_event_factory_.SetLastPosition(pointer_id,
                                           web_pointer_event.PositionInScreen(),
                                           web_pointer_event.GetType());
  }
}

void PointerEventManager::RemoveLastMousePosition() {
  pointer_event_factory_.RemoveLastPosition(PointerEventFactory::kMouseId);
}

PointerId PointerEventManager::GetPointerIdForTouchGesture(
    const uint32_t unique_touch_event_id) {
  return pointer_event_factory_.GetPointerIdForTouchGesture(
      unique_touch_event_id);
}

Element* PointerEventManager::CurrentTouchDownElement() {
  return touch_event_manager_->CurrentTouchDownElement();
}

}  // namespace blink
