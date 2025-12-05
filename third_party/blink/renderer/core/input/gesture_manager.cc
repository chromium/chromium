// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/gesture_manager.h"

#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_pointer_event.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/public/public_buildflags.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mouse_event_init.h"
#include "third_party/blink/renderer/core/annotation/annotation_agent_impl.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/selection_controller.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/gesture_event.h"
#include "third_party/blink/renderer/core/events/pointer_event_factory.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/event_handling_util.h"
#include "third_party/blink/renderer/core/input/input_device_capabilities.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-blink.h"
#include "ui/gfx/geometry/point_conversions.h"

#if BUILDFLAG(ENABLE_UNHANDLED_TAP)
#include "third_party/blink/public/mojom/unhandled_tap_notifier/unhandled_tap_notifier.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#endif  // BUILDFLAG(ENABLE_UNHANDLED_TAP)

namespace blink {

namespace {

// The amount of drag (in pixels) that is considered to be within a slop region.
// This allows firing touch dragend contextmenu events for shaky fingers.
const int kTouchDragSlop = 8;

bool TouchDragAndContextMenuEnabled(const LocalFrame* frame) {
  return RuntimeEnabledFeatures::TouchDragAndContextMenuEnabled() &&
         frame->GetSettings() && !frame->GetSettings()->GetModalContextMenu();
}

}  // namespace

GestureManager::GestureManager(LocalFrame& frame,
                               ScrollManager& scroll_manager,
                               MouseEventManager& mouse_event_manager,
                               PointerEventManager& pointer_event_manager,
                               SelectionController& selection_controller)
    : FocusChangedObserver(frame.GetPage()),
      frame_(frame),
      scroll_manager_(scroll_manager),
      mouse_event_manager_(mouse_event_manager),
      pointer_event_manager_(pointer_event_manager),
      selection_controller_(selection_controller) {
  Clear();
}

void GestureManager::Clear() {
  suppress_mouse_events_from_gestures_ = false;
  suppress_selection_on_repeated_tap_down_ = false;
  lost_focus_during_drag_ = false;
  ResetLongTapContextMenuStates();
}

void GestureManager::ResetLongTapContextMenuStates() {
  gesture_context_menu_deferred_ = false;
  long_press_position_in_root_frame_ = gfx::PointF();
  drag_in_progress_ = false;
}

void GestureManager::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(scroll_manager_);
  visitor->Trace(mouse_event_manager_);
  visitor->Trace(pointer_event_manager_);
  visitor->Trace(selection_controller_);
}

HitTestRequest::HitTestRequestType GestureManager::GetHitTypeForGestureType(
    WebInputEvent::Type type) {
  HitTestRequest::HitTestRequestType hit_type = HitTestRequest::kTouchEvent;
  switch (type) {
    case WebInputEvent::Type::kGestureShowPress:
    case WebInputEvent::Type::kGestureTapUnconfirmed:
      return hit_type | HitTestRequest::kActive;
    case WebInputEvent::Type::kGestureTapCancel:
      // A TapDownCancel received when no element is active shouldn't really be
      // changing hover state.
      if (!frame_->GetDocument()->GetActiveElement())
        hit_type |= HitTestRequest::kReadOnly;
      return hit_type | HitTestRequest::kRelease;
    case WebInputEvent::Type::kGestureTap:
      return hit_type | HitTestRequest::kRelease;
    case WebInputEvent::Type::kGestureTapDown:
    case WebInputEvent::Type::kGestureShortPress:
    case WebInputEvent::Type::kGestureLongPress:
    case WebInputEvent::Type::kGestureLongTap:
    case WebInputEvent::Type::kGestureTwoFingerTap:
      // FIXME: Shouldn't LongTap and TwoFingerTap clear the Active state?
      return hit_type | HitTestRequest::kActive | HitTestRequest::kReadOnly;
    default:
      NOTREACHED();
  }
}

WebInputEventResult GestureManager::HandleGestureEventInFrame(
    const GestureEventWithHitTestResults& targeted_event) {
  const HitTestResult& hit_test_result = targeted_event.GetHitTestResult();
  const WebGestureEvent& gesture_event = targeted_event.Event();
  DCHECK(!gesture_event.IsScrollEvent());

  if (Scrollbar* scrollbar = hit_test_result.GetScrollbar()) {
    if (scrollbar->HandleGestureTapOrPress(gesture_event)) {
      return WebInputEventResult::kHandledSuppressed;
    }
  }

  // TODO(https://crbug.com/427503494): Investigate if this code block is
  // exposing internal gesture events as DOM events.
  if (Node* event_target = hit_test_result.InnerNode()) {
    GestureEvent* gesture_dom_event = GestureEvent::Create(
        event_target->GetDocument().domWindow(), gesture_event);
    if (gesture_dom_event) {
      DispatchEventResult gesture_dom_event_result =
          event_target->DispatchEvent(*gesture_dom_event);
      if (gesture_dom_event_result != DispatchEventResult::kNotCanceled) {
        DCHECK(gesture_dom_event_result !=
               DispatchEventResult::kCanceledByEventHandler);
        return event_handling_util::ToWebInputEventResult(
            gesture_dom_event_result);
      }
    }
  }

  // Long presses are the only gesture that could happen with an ongoing drag.
  // We clear the flag on any other gesture in case the gesture manager didn't
  // receive a drag end event for any reason.
  if (gesture_event.GetType() != WebInputEvent::Type::kGestureLongPress) {
    drag_in_progress_ = false;
  }

  switch (gesture_event.GetType()) {
    case WebInputEvent::Type::kGestureTapDown:
      return HandleGestureTapDown(targeted_event);
    case WebInputEvent::Type::kGestureTap:
      return HandleGestureTap(targeted_event);
    case WebInputEvent::Type::kGestureShowPress:
      return HandleGestureShowPress();
    case WebInputEvent::Type::kGestureShortPress:
      return HandleGestureShortPress(targeted_event);
    case WebInputEvent::Type::kGestureLongPress:
      return HandleGestureLongPress(targeted_event);
    case WebInputEvent::Type::kGestureLongTap:
      return HandleGestureLongTap(targeted_event);
    case WebInputEvent::Type::kGestureTwoFingerTap:
      return HandleGestureTwoFingerTap(targeted_event);
    case WebInputEvent::Type::kGestureTapCancel:
    case WebInputEvent::Type::kGestureTapUnconfirmed:
      break;
    default:
      NOTREACHED();
  }

  return WebInputEventResult::kNotHandled;
}

bool GestureManager::GestureContextMenuDeferred() const {
  return gesture_context_menu_deferred_;
}

WebInputEventResult GestureManager::HandleGestureTapDown(
    const GestureEventWithHitTestResults& targeted_event) {
  const WebGestureEvent& gesture_event = targeted_event.Event();
  suppress_mouse_events_from_gestures_ =
      pointer_event_manager_->PrimaryPointerdownCanceled(
          gesture_event.unique_touch_event_id);
  lost_focus_during_drag_ = false;

  if (!RuntimeEnabledFeatures::TouchTextEditingRedesignEnabled() ||
      suppress_mouse_events_from_gestures_ ||
      suppress_selection_on_repeated_tap_down_ ||
      gesture_event.TapDownCount() <= 1) {
    return WebInputEventResult::kNotHandled;
  }

  const WebMouseEvent fake_mouse_down(
      WebInputEvent::Type::kMouseDown, gesture_event,
      WebPointerProperties::Button::kLeft, gesture_event.TapDownCount(),
      static_cast<WebInputEvent::Modifiers>(
          gesture_event.GetModifiers() |
          WebInputEvent::Modifiers::kLeftButtonDown |
          WebInputEvent::Modifiers::kIsCompatibilityEventForTouch),
      gesture_event.TimeStamp());
  const HitTestResult& current_hit_test = targeted_event.GetHitTestResult();
  const HitTestLocation& current_hit_test_location =
      targeted_event.GetHitTestLocation();
  selection_controller_->HandleMousePressEvent(MouseEventWithHitTestResults(
      fake_mouse_down, current_hit_test_location, current_hit_test));

  return WebInputEventResult::kNotHandled;
}

WebInputEventResult GestureManager::HandleGestureTap(
    const GestureEventWithHitTestResults& targeted_event) {
  LocalFrameView* frame_view(frame_->View());
  const WebGestureEvent& gesture_event = targeted_event.Event();
  HitTestRequest::HitTestRequestType hit_type =
      GetHitTypeForGestureType(gesture_event.GetType());
  uint64_t pre_dispatch_dom_tree_version =
      frame_->GetDocument()->DomTreeVersion();
  uint64_t pre_dispatch_style_version = frame_->GetDocument()->StyleVersion();

  HitTestResult current_hit_test = targeted_event.GetHitTestResult();
  const HitTestLocation& current_hit_test_location =
      targeted_event.GetHitTestLocation();

  // We use the adjusted position so the application isn't surprised to see a
  // event with co-ordinates outside the target's bounds.
  gfx::Point adjusted_point = frame_view->ConvertFromRootFrame(
      gfx::ToFlooredPoint(gesture_event.PositionInRootFrame()));

  const unsigned modifiers = gesture_event.GetModifiers();

  if (!suppress_mouse_events_from_gestures_) {
    WebMouseEvent fake_mouse_move(
        WebInputEvent::Type::kMouseMove, gesture_event,
        WebPointerProperties::Button::kNoButton,
        /* clickCount */ 0,
        static_cast<WebInputEvent::Modifiers>(
            modifiers |
            WebInputEvent::Modifiers::kIsCompatibilityEventForTouch),
        gesture_event.TimeStamp());

    // This updates hover state to the location of the tap, but does NOT update
    // MouseEventManager::last_known_mouse_position_*. That's deliberate, since
    // we don't want the page to continue to act as if this point is hovered
    // (if the user scrolls for example).
    //
    // TODO(crbug.com/368256331): When we've applied a tap-based hover state, we
    // should actually suppress RecomputeMouseHoverState until the user moves
    // the mouse or navigates away.
    mouse_event_manager_->SetElementUnderMouseAndDispatchMouseEvent(
        current_hit_test.InnerElement(), event_type_names::kMousemove,
        fake_mouse_move);
  }

  // Do a new hit-test in case the mousemove event changed the DOM.
  // Note that if the original hit test wasn't over an element (eg. was over a
  // scrollbar) we don't want to re-hit-test because it may be in the wrong
  // frame (and there's no way the page could have seen the event anyway).  Also
  // note that the position of the frame may have changed, so we need to
  // recompute the content co-ordinates (updating layout/style as
  // hitTestResultAtPoint normally would).
  if (current_hit_test.InnerNode()) {
    LocalFrame& main_frame = frame_->LocalFrameRoot();
    if (!main_frame.View() ||
        !main_frame.View()->UpdateAllLifecyclePhasesExceptPaint(
            DocumentUpdateReason::kHitTest))
      return WebInputEventResult::kNotHandled;
    adjusted_point = frame_view->ConvertFromRootFrame(
        gfx::ToFlooredPoint(gesture_event.PositionInRootFrame()));
    current_hit_test = event_handling_util::HitTestResultInFrame(
        frame_, HitTestLocation(adjusted_point), hit_type);
  }

  // Capture data for showUnhandledTapUIIfNeeded.
  gfx::Point tapped_position =
      gfx::ToFlooredPoint(gesture_event.PositionInRootFrame());
  Node* tapped_node = current_hit_test.InnerNode();
  Element* tapped_element = current_hit_test.InnerElement();
  LocalFrame::NotifyUserActivation(
      tapped_node ? tapped_node->GetDocument().GetFrame() : nullptr,
      mojom::blink::UserActivationNotificationType::kInteraction);

  mouse_event_manager_->SetMouseDownElement(tapped_element);

  WebMouseEvent fake_mouse_down(
      WebInputEvent::Type::kMouseDown, gesture_event,
      WebPointerProperties::Button::kLeft, gesture_event.TapCount(),
      static_cast<WebInputEvent::Modifiers>(
          modifiers | WebInputEvent::Modifiers::kLeftButtonDown |
          WebInputEvent::Modifiers::kIsCompatibilityEventForTouch),
      gesture_event.TimeStamp());

  // TODO(mustaq): We suppress MEs plus all it's side effects. What would that
  // mean for for TEs?  What's the right balance here? crbug.com/617255
  WebInputEventResult mouse_down_event_result =
      WebInputEventResult::kHandledSuppressed;
  suppress_selection_on_repeated_tap_down_ = true;
  if (!suppress_mouse_events_from_gestures_) {
    mouse_event_manager_->SetClickCount(gesture_event.TapCount());

    mouse_down_event_result =
        mouse_event_manager_->SetElementUnderMouseAndDispatchMouseEvent(
            current_hit_test.InnerElement(), event_type_names::kMousedown,
            fake_mouse_down);
    selection_controller_->InitializeSelectionState();
    if (mouse_down_event_result == WebInputEventResult::kNotHandled) {
      mouse_down_event_result = mouse_event_manager_->HandleMouseFocus(
          current_hit_test,
          frame_->DomWindow()->GetInputDeviceCapabilities()->FiresTouchEvents(
              true));
    }
    if (mouse_down_event_result == WebInputEventResult::kNotHandled) {
      suppress_selection_on_repeated_tap_down_ = false;
      mouse_down_event_result = mouse_event_manager_->HandleMousePressEvent(
          MouseEventWithHitTestResults(
              fake_mouse_down, current_hit_test_location, current_hit_test));
    }
  }

  if (current_hit_test.InnerNode()) {
    DCHECK(gesture_event.GetType() == WebInputEvent::Type::kGestureTap);
    HitTestResult result = current_hit_test;
    result.SetToShadowHostIfInUAShadowRoot();
    frame_->GetChromeClient().OnMouseDown(*result.InnerNode());
  }

  if (current_hit_test.InnerNode()) {
    LocalFrame& main_frame = frame_->LocalFrameRoot();
    if (main_frame.View()) {
      main_frame.View()->UpdateAllLifecyclePhasesExceptPaint(
          DocumentUpdateReason::kHitTest);
    }
    adjusted_point = frame_view->ConvertFromRootFrame(tapped_position);
    current_hit_test = event_handling_util::HitTestResultInFrame(
        frame_, HitTestLocation(adjusted_point), hit_type);
  }

  WebMouseEvent fake_mouse_up(
      WebInputEvent::Type::kMouseUp, gesture_event,
      WebPointerProperties::Button::kLeft, gesture_event.TapCount(),
      static_cast<WebInputEvent::Modifiers>(
          modifiers | WebInputEvent::Modifiers::kIsCompatibilityEventForTouch),
      gesture_event.TimeStamp());
  WebInputEventResult mouse_up_event_result =
      suppress_mouse_events_from_gestures_
          ? WebInputEventResult::kHandledSuppressed
          : mouse_event_manager_->SetElementUnderMouseAndDispatchMouseEvent(
                current_hit_test.InnerElement(), event_type_names::kMouseup,
                fake_mouse_up);

  WebInputEventResult click_event_result = WebInputEventResult::kNotHandled;
  if (tapped_element) {
    if (current_hit_test.InnerNode()) {
      Node* click_target_node = current_hit_test.InnerNode()->CommonAncestor(
          *tapped_element, event_handling_util::ParentForClickEvent);
      auto* click_target_element = DynamicTo<Element>(click_target_node);
      PointerId pointer_id = GetPointerIdFromWebGestureEvent(gesture_event);
      fake_mouse_up.id = pointer_id;
      fake_mouse_up.pointer_type = gesture_event.primary_pointer_type;

      PointerEventFactory::PointerTarget* pointer_down_target =
          pointer_event_manager_->GetPointerDownTarget(pointer_id);
      PointerEventFactory::PointerTarget* pointer_up_target =
          pointer_event_manager_->GetPointerUpTarget(pointer_id);
      if (!pointer_down_target) {
        CHECK(!pointer_up_target);
        // The browser didn't send any pointer events for this touch, so we need
        // to fake the information for light dismiss. This can happen when the
        // page has no pointerdown/pointerup event handlers.
        // TODO(crbug.com/465787221): We should prevent this from happening by
        // making pointerdown/pointerup get fired when there is a popover or
        // dialog in the page which may be light dismissed.
        MouseEventInit* init_for_coords =
            MakeGarbageCollected<MouseEventInit>();
        MouseEvent::SetCoordinatesFromWebPointerProperties(
            fake_mouse_up.FlattenTransform(),
            frame_->GetDocument()->domWindow(), init_for_coords);
        pointer_down_target =
            MakeGarbageCollected<PointerEventFactory::PointerTarget>(
                click_target_element, init_for_coords->clientX(),
                init_for_coords->clientY());
        pointer_up_target =
            MakeGarbageCollected<PointerEventFactory::PointerTarget>(
                click_target_element, init_for_coords->clientX(),
                init_for_coords->clientY());
      } else {
        CHECK(pointer_up_target);
      }

      click_event_result =
          mouse_event_manager_->SetElementUnderMouseAndDispatchMouseEvent(
              click_target_element, event_type_names::kClick, fake_mouse_up,
              pointer_down_target, pointer_up_target);
      pointer_event_manager_->RemovePointerTargets(pointer_id);

      // Dispatching a JS event could have detached the frame.
      if (frame_->View())
        frame_->View()->RegisterTapEvent(tapped_element);
    }
    mouse_event_manager_->SetMouseDownElement(nullptr);
  }

  if (mouse_up_event_result == WebInputEventResult::kNotHandled) {
    mouse_up_event_result = mouse_event_manager_->HandleMouseReleaseEvent(
        MouseEventWithHitTestResults(fake_mouse_up, current_hit_test_location,
                                     current_hit_test));
  }
  mouse_event_manager_->ClearDragHeuristicState();

  WebInputEventResult event_result = event_handling_util::MergeEventResult(
      event_handling_util::MergeEventResult(mouse_down_event_result,
                                            mouse_up_event_result),
      click_event_result);

  if (RuntimeEnabledFeatures::TextFragmentTapOpensContextMenuEnabled() &&
      current_hit_test.InnerNodeFrame()) {
    current_hit_test.InnerNodeFrame()
        ->View()
        ->UpdateAllLifecyclePhasesExceptPaint(DocumentUpdateReason::kHitTest);
    current_hit_test = event_handling_util::HitTestResultInFrame(
        frame_, HitTestLocation(adjusted_point), hit_type);
    if (AnnotationAgentImpl::IsOverAnnotation(current_hit_test) ==
            mojom::blink::AnnotationType::kSharedHighlight &&
        event_result == WebInputEventResult::kNotHandled) {
      return SendContextMenuEventForGesture(targeted_event);
    }
  }

  // Default case when tap that is not handled.
  if (event_result == WebInputEventResult::kNotHandled && tapped_node &&
      frame_->GetPage()) {
    bool dom_tree_changed = pre_dispatch_dom_tree_version !=
                            frame_->GetDocument()->DomTreeVersion();
    bool style_changed =
        pre_dispatch_style_version != frame_->GetDocument()->StyleVersion();

    gfx::Point tapped_position_in_viewport =
        frame_->GetPage()->GetVisualViewport().RootFrameToViewport(
            tapped_position);
    ShowUnhandledTapUIIfNeeded(dom_tree_changed, style_changed, tapped_node,
                               tapped_position_in_viewport);
  }

  return event_result;
}

WebInputEventResult GestureManager::HandleGestureShortPress(
    const GestureEventWithHitTestResults& targeted_event) {
  if (frame_->GetSettings() &&
      frame_->GetSettings()->GetTouchDragDropEnabled() &&
      RuntimeEnabledFeatures::TouchDragOnShortPressEnabled() &&
      HandleDragDropIfPossible(targeted_event) !=
          DragHandlingResult::kNotHandled) {
    return WebInputEventResult::kHandledSystem;
  }
  return WebInputEventResult::kNotHandled;
}

WebInputEventResult GestureManager::HandleGestureLongPress(
    const GestureEventWithHitTestResults& targeted_event) {
  const WebGestureEvent& gesture_event = targeted_event.Event();

  // FIXME: Ideally we should try to remove the extra mouse-specific hit-tests
  // here (re-using the supplied HitTestResult), but that will require some
  // overhaul of the touch drag-and-drop code and LongPress is such a special
  // scenario that it's unlikely to matter much in practice.

  long_press_position_in_root_frame_ = gesture_event.PositionInRootFrame();
  HitTestLocation location(frame_->View()->ConvertFromRootFrame(
      gfx::ToFlooredPoint(long_press_position_in_root_frame_)));
  HitTestResult hit_test_result =
      frame_->GetEventHandler().HitTestResultAtLocation(location);

  gesture_context_menu_deferred_ = false;

  if (RuntimeEnabledFeatures::TouchDragOnShortPressEnabled() &&
      drag_in_progress_) {
    if (DragEndOpensContextMenu()) {
      gesture_context_menu_deferred_ = true;
      return WebInputEventResult::kNotHandled;
    }
  } else if (TouchDragAndContextMenuEnabled(frame_)) {
    HandleDragDropIfPossible(targeted_event);
  } else if (frame_->GetSettings() &&
             frame_->GetSettings()->GetTouchDragDropEnabled() &&
             frame_->View()) {
    // Dragging is suppressed on links and images in favor of opening a
    // context menu on long press. In Windows, a drag is started and the
    // context menu is opened if the drop happens in the same spot.
    const bool should_open_context_menu_now =
        !frame_->GetSettings()->GetTouchDragEndContextMenu() &&
        (hit_test_result.URLElement() ||
         !hit_test_result.AbsoluteImageURL().IsNull() ||
         !hit_test_result.AbsoluteMediaURL().IsNull());
    if (!should_open_context_menu_now &&
        HandleDragDropIfPossible(targeted_event) !=
            DragHandlingResult::kNotHandled) {
      gesture_context_menu_deferred_ = true;
      return WebInputEventResult::kHandledSystem;
    }
  }

  Node* inner_node = hit_test_result.InnerNode();
  if (!(drag_in_progress_ && TouchDragAndContextMenuEnabled(frame_)) &&
      inner_node && inner_node->GetLayoutObject() &&
      selection_controller_->HandleGestureLongPress(hit_test_result)) {
    mouse_event_manager_->FocusDocumentView();
  }

  if (frame_->GetSettings() &&
      frame_->GetSettings()->GetShowContextMenuOnMouseUp()) {
    // TODO(https://crbug.com/1290905): Prevent a contextmenu after a
    // finger-drag when TouchDragAndContextMenu is enabled.
    gesture_context_menu_deferred_ = true;
    return WebInputEventResult::kNotHandled;
  }

  LocalFrame::NotifyUserActivation(
      inner_node ? inner_node->GetDocument().GetFrame() : nullptr,
      mojom::blink::UserActivationNotificationType::kInteraction);
  return SendContextMenuEventForGesture(targeted_event);
}

WebInputEventResult GestureManager::HandleGestureLongTap(
    const GestureEventWithHitTestResults& targeted_event) {
  if (gesture_context_menu_deferred_) {
    gesture_context_menu_deferred_ = false;
    return SendContextMenuEventForGesture(targeted_event);
  }
  return WebInputEventResult::kNotHandled;
}

WebInputEventResult GestureManager::HandleGestureTwoFingerTap(
    const GestureEventWithHitTestResults& targeted_event) {
  Node* inner_node = targeted_event.GetHitTestResult().InnerNode();
  if (inner_node && inner_node->GetLayoutObject())
    selection_controller_->HandleGestureTwoFingerTap(targeted_event);
  return SendContextMenuEventForGesture(targeted_event);
}

void GestureManager::HandleTouchDragEnd(
    const WebMouseEvent& event,
    ui::mojom::blink::DragOperation operation) {
  if (!drag_in_progress_) {
    return;
  }
  drag_in_progress_ = false;
  if (DragEndOpensContextMenu()) {
    SendContextMenuEventTouchDragEnd(event, operation);
  }
}

void GestureManager::SendContextMenuEventTouchDragEnd(
    const WebMouseEvent& mouse_event,
    ui::mojom::blink::DragOperation operation) {
  if (!gesture_context_menu_deferred_ || suppress_mouse_events_from_gestures_) {
    return;
  }

  const gfx::PointF& positon_in_root_frame = mouse_event.PositionInWidget();

  // There are three conditions that need to be met for the context menu to be
  // open after a drag end:
  // 1) The drop happened inside the `kTouchDragSlop` region.
  // 2) The drop happened the same page that initiated the drag and the page
  // never lost focus.
  // 3) The drop did not have an effect (drag operation result was `kNone`).
  // TODO(crbug.com/417245719): Ideally a CM wouldn't open if the drag was moved
  // drastically outside of the slop region before being dropped; but the way
  // mouse translation works for touch drag and drop right now makes the pointer
  // lag behind a few frames when being synced, which causes the drag move
  // events to always start far away from the original drag position. This makes
  // tracking the drag to ensure it never left the slop region very difficult.
  // When crbug.com/418025705 is implemented we will see if this sync issue is
  // fixed and we can enforce this restriction.
  // TODO(mustaq): We should be reusing gesture touch-slop region here but it
  // seems non-trivial because this code path is called at drag-end, and the
  // drag controller does not sync well with gesture recognizer.  See the
  // blocked-on bugs in https://crbug.com/1096189.
  const bool should_open_context_menu =
      (positon_in_root_frame - long_press_position_in_root_frame_).Length() <=
          kTouchDragSlop &&
      !lost_focus_during_drag_ &&
      operation == ui::mojom::blink::DragOperation::kNone;
  if (!should_open_context_menu) {
    ResetLongTapContextMenuStates();
    return;
  }

  ContextMenuAllowedScope scope;
  frame_->GetEventHandler().SendContextMenuEvent(mouse_event);
  ResetLongTapContextMenuStates();
}

WebInputEventResult GestureManager::SendContextMenuEventForGesture(
    const GestureEventWithHitTestResults& targeted_event) {
  const WebGestureEvent& gesture_event = targeted_event.Event();
  unsigned modifiers = gesture_event.GetModifiers();

  if (!suppress_mouse_events_from_gestures_) {
    // Send MouseMove event prior to handling (https://crbug.com/485290).
    WebMouseEvent fake_mouse_move(
        WebInputEvent::Type::kMouseMove, gesture_event,
        WebPointerProperties::Button::kNoButton,
        /* clickCount */ 0,
        static_cast<WebInputEvent::Modifiers>(
            modifiers | WebInputEvent::kIsCompatibilityEventForTouch),
        gesture_event.TimeStamp());
    mouse_event_manager_->SetElementUnderMouseAndDispatchMouseEvent(
        targeted_event.GetHitTestResult().InnerElement(),
        event_type_names::kMousemove, fake_mouse_move);
  }

  WebInputEvent::Type event_type = WebInputEvent::Type::kMouseDown;
  if (frame_->GetSettings() &&
      frame_->GetSettings()->GetShowContextMenuOnMouseUp())
    event_type = WebInputEvent::Type::kMouseUp;

  WebMouseEvent mouse_event(
      event_type, gesture_event, WebPointerProperties::Button::kNoButton,
      /* clickCount */ 0,
      static_cast<WebInputEvent::Modifiers>(
          modifiers | WebInputEvent::kIsCompatibilityEventForTouch),
      gesture_event.TimeStamp());

  if (!suppress_mouse_events_from_gestures_ && frame_->View()) {
    HitTestRequest request(HitTestRequest::kActive);
    PhysicalOffset document_point(frame_->View()->ConvertFromRootFrame(
        gfx::ToFlooredPoint(targeted_event.Event().PositionInRootFrame())));
    MouseEventWithHitTestResults mev =
        frame_->GetDocument()->PerformMouseEventHitTest(request, document_point,
                                                        mouse_event);
    mouse_event_manager_->HandleMouseFocus(mev.GetHitTestResult(),
                                           frame_->GetDocument()
                                               ->domWindow()
                                               ->GetInputDeviceCapabilities()
                                               ->FiresTouchEvents(true));
  }
  mouse_event.id = GetPointerIdFromWebGestureEvent(gesture_event);
  mouse_event.pointer_type = gesture_event.primary_pointer_type;
  return frame_->GetEventHandler().SendContextMenuEvent(mouse_event);
}

WebInputEventResult GestureManager::HandleGestureShowPress() {
  LocalFrameView* view = frame_->View();
  if (!view)
    return WebInputEventResult::kNotHandled;
  for (auto& scrollable_area : view->ScrollableAreas().Values()) {
    if (scrollable_area->ScrollsOverflow())
      scrollable_area->CancelScrollAnimation();
  }
  return WebInputEventResult::kNotHandled;
}

void GestureManager::ShowUnhandledTapUIIfNeeded(
    bool dom_tree_changed,
    bool style_changed,
    Node* tapped_node,
    const gfx::Point& tapped_position_in_viewport) {
#if BUILDFLAG(ENABLE_UNHANDLED_TAP)
  WebNode web_node(tapped_node);
  bool should_trigger = !dom_tree_changed && !style_changed &&
                        tapped_node->IsTextNode() &&
                        !web_node.IsContentEditable() &&
                        !web_node.IsInsideFocusableElementOrARIAWidget();
  // Renderer-side trigger-filtering to minimize messaging.
  // The Browser may do additional trigger-filtering.
  if (should_trigger) {
    // Start setting up the Mojo interface connection.
    mojo::Remote<mojom::blink::UnhandledTapNotifier> provider;
    frame_->GetBrowserInterfaceBroker().GetInterface(
        provider.BindNewPipeAndPassReceiver());

    // Notify the Browser.
    auto tapped_info =
        mojom::blink::UnhandledTapInfo::New(tapped_position_in_viewport);
    provider->ShowUnhandledTapUIIfNeeded(std::move(tapped_info));
  }
#endif  // BUILDFLAG(ENABLE_UNHANDLED_TAP)
}

PointerId GestureManager::GetPointerIdFromWebGestureEvent(
    const WebGestureEvent& gesture_event) const {
  if (!frame_->IsLocalRoot()) {
    return frame_->LocalFrameRoot()
        .GetEventHandler()
        .GetGestureManager()
        .GetPointerIdFromWebGestureEvent(gesture_event);
  }

  // When tests send Tap, LongTap, LongPress, TwoFingerTap directly
  // (e.g. from eventSender) there is no primary_unique_touch_event_id
  // populated.
  if (gesture_event.primary_unique_touch_event_id == 0)
    return PointerEventFactory::kInvalidId;

  return pointer_event_manager_->GetPointerIdForTouchGesture(
      gesture_event.primary_unique_touch_event_id);
}

DragHandlingResult GestureManager::HandleDragDropIfPossible(
    const GestureEventWithHitTestResults& targeted_event) {
  const DragHandlingResult result =
      mouse_event_manager_->HandleDragDropIfPossible(
          targeted_event,
          GetPointerIdFromWebGestureEvent(targeted_event.Event()));
  drag_in_progress_ = result == DragHandlingResult::kHandledDragStarted;
  return result;
}

bool GestureManager::DragEndOpensContextMenu() {
  return frame_->GetSettings() &&
         frame_->GetSettings()->GetTouchDragEndContextMenu();
}

}  // namespace blink
