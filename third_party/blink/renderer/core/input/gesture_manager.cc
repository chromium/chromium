// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/gesture_manager.h"

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/public_buildflags.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/editing/selection_controller.h"
#include "third_party/blink/renderer/core/events/gesture_event.h"
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
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"

#if BUILDFLAG(ENABLE_UNHANDLED_TAP)
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/unhandled_tap_notifier/unhandled_tap_notifier.mojom-blink.h"
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

}  // namespace

GestureManager::GestureManager(LocalFrame& frame,
                               ScrollManager& scroll_manager,
                               MouseEventManager& mouse_event_manager,
                               PointerEventManager& pointer_event_manager,
                               SelectionController& selection_controller)
    : frame_(frame),
      scroll_manager_(scroll_manager),
      mouse_event_manager_(mouse_event_manager),
      pointer_event_manager_(pointer_event_manager),
      selection_controller_(selection_controller) {
  Clear();
}

void GestureManager::Clear() {
  suppress_mouse_events_from_gestures_ = false;
  ResetLongTapContextMenuStates();
}

void GestureManager::ResetLongTapContextMenuStates() {
  gesture_context_menu_deferred_ = false;
  long_press_position_in_root_frame_ = gfx::PointF();
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
  HitTestRequest::HitTestRequestType hit_type =
      HitTestRequest::kTouchEvent | HitTestRequest::kRetargetForInert;
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
    case WebInputEvent::Type::kGestureLongPress:
    case WebInputEvent::Type::kGestureLongTap:
    case WebInputEvent::Type::kGestureTwoFingerTap:
      // FIXME: Shouldn't LongTap and TwoFingerTap clear the Active state?
      return hit_type | HitTestRequest::kActive | HitTestRequest::kReadOnly;
    default:
      NOTREACHED();
      return hit_type | HitTestRequest::kActive | HitTestRequest::kReadOnly;
  }
}

WebInputEventResult GestureManager::HandleGestureEventInFrame(
    const GestureEventWithHitTestResults& targeted_event) {
  DCHECK(!targeted_event.Event().IsScrollEvent());

  Node* event_target = targeted_event.GetHitTestResult().InnerNode();
  const WebGestureEvent& gesture_event = targeted_event.Event();

  if (scroll_manager_->CanHandleGestureEvent(targeted_event))
    return WebInputEventResult::kHandledSuppressed;

  if (event_target) {
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

  switch (gesture_event.GetType()) {
    case WebInputEvent::Type::kGestureTapDown:
      return HandleGestureTapDown(targeted_event);
    case WebInputEvent::Type::kGestureTap:
      return HandleGestureTap(targeted_event);
    case WebInputEvent::Type::kGestureShowPress:
      return HandleGestureShowPress();
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
  suppress_mouse_events_from_gestures_ =
      pointer_event_manager_->PrimaryPointerdownCanceled(
          targeted_event.Event().unique_touch_event_id);
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
  IntPoint adjusted_point = frame_view->ConvertFromRootFrame(
      FlooredIntPoint(gesture_event.PositionInRootFrame()));

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
    mouse_event_manager_->SetMousePositionAndDispatchMouseEvent(
        current_hit_test.InnerElement(), current_hit_test.CanvasRegionId(),
        event_type_names::kMousemove, fake_mouse_move);
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
        FlooredIntPoint(gesture_event.PositionInRootFrame()));
    current_hit_test = event_handling_util::HitTestResultInFrame(
        frame_, HitTestLocation(adjusted_point), hit_type);
  }

  // Capture data for showUnhandledTapUIIfNeeded.
  IntPoint tapped_position =
      FlooredIntPoint(gesture_event.PositionInRootFrame());
  Node* tapped_node = current_hit_test.InnerNode();
  Element* tapped_element = current_hit_test.InnerElement();
  LocalFrame::NotifyUserActivation(
      tapped_node ? tapped_node->GetDocument().GetFrame() : nullptr,
      mojom::blink::UserActivationNotificationType::kInteraction);

  mouse_event_manager_->SetClickElement(tapped_element);

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
  if (!suppress_mouse_events_from_gestures_) {
    mouse_event_manager_->SetClickCount(gesture_event.TapCount());

    mouse_down_event_result =
        mouse_event_manager_->SetMousePositionAndDispatchMouseEvent(
            current_hit_test.InnerElement(), current_hit_test.CanvasRegionId(),
            event_type_names::kMousedown, fake_mouse_down);
    selection_controller_->InitializeSelectionState();
    if (mouse_down_event_result == WebInputEventResult::kNotHandled) {
      mouse_down_event_result = mouse_event_manager_->HandleMouseFocus(
          current_hit_test,
          frame_->DomWindow()->GetInputDeviceCapabilities()->FiresTouchEvents(
              true));
    }
    if (mouse_down_event_result == WebInputEventResult::kNotHandled) {
      mouse_down_event_result = mouse_event_manager_->HandleMousePressEvent(
          MouseEventWithHitTestResults(
              fake_mouse_down, current_hit_test_location, current_hit_test));
    }
  }

  if (current_hit_test.InnerNode()) {
    DCHECK(gesture_event.GetType() == WebInputEvent::Type::kGestureTap);
    HitTestResult result = current_hit_test;
    result.SetToShadowHostIfInRestrictedShadowRoot();
    frame_->GetChromeClient().OnMouseDown(*result.InnerNode());
  }

  if (current_hit_test.InnerNode()) {
    LocalFrame& main_frame = frame_->LocalFrameRoot();
    if (main_frame.View()) {
      main_frame.View()->UpdateAllLifecyclePhases(
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
          : mouse_event_manager_->SetMousePositionAndDispatchMouseEvent(
                current_hit_test.InnerElement(),
                current_hit_test.CanvasRegionId(), event_type_names::kMouseup,
                fake_mouse_up);

  WebInputEventResult click_event_result = WebInputEventResult::kNotHandled;
  if (tapped_element) {
    if (current_hit_test.InnerNode()) {
      // Updates distribution because a mouseup (or mousedown) event listener
      // can make the tree dirty at dispatchMouseEvent() invocation above.
      // Unless distribution is updated, commonAncestor would hit DCHECK.  Both
      // tappedNonTextNode and currentHitTest.innerNode()) don't need to be
      // updated because commonAncestor() will exit early if their documents are
      // different.
      tapped_element->UpdateDistributionForFlatTreeTraversal();
      Node* click_target_node = current_hit_test.InnerNode()->CommonAncestor(
          *tapped_element, event_handling_util::ParentForClickEvent);
      auto* click_target_element = DynamicTo<Element>(click_target_node);

      click_event_result =
          mouse_event_manager_->SetMousePositionAndDispatchMouseEvent(
              click_target_element, String(), event_type_names::kClick,
              fake_mouse_up);
    }
    mouse_event_manager_->SetClickElement(nullptr);
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
  if (event_result == WebInputEventResult::kNotHandled && tapped_node &&
      frame_->GetPage()) {
    bool dom_tree_changed = pre_dispatch_dom_tree_version !=
                            frame_->GetDocument()->DomTreeVersion();
    bool style_changed =
        pre_dispatch_style_version != frame_->GetDocument()->StyleVersion();

    IntPoint tapped_position_in_viewport =
        frame_->GetPage()->GetVisualViewport().RootFrameToViewport(
            tapped_position);
    ShowUnhandledTapUIIfNeeded(dom_tree_changed, style_changed, tapped_node,
                               tapped_element, tapped_position_in_viewport);
  }
  return event_result;
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
      FlooredIntPoint(long_press_position_in_root_frame_)));
  HitTestResult hit_test_result =
      frame_->GetEventHandler().HitTestResultAtLocation(location);

  gesture_context_menu_deferred_ = false;

  bool hit_test_contains_links = hit_test_result.URLElement() ||
                                 !hit_test_result.AbsoluteImageURL().IsNull() ||
                                 !hit_test_result.AbsoluteMediaURL().IsNull();
  if (!hit_test_contains_links &&
      mouse_event_manager_->HandleDragDropIfPossible(targeted_event)) {
    gesture_context_menu_deferred_ = true;
    return WebInputEventResult::kHandledSystem;
  }

  Node* inner_node = hit_test_result.InnerNode();
  if (inner_node && inner_node->GetLayoutObject() &&
      selection_controller_->HandleGestureLongPress(hit_test_result)) {
    mouse_event_manager_->FocusDocumentView();
  }

  if (frame_->GetSettings() &&
      frame_->GetSettings()->GetShowContextMenuOnMouseUp()) {
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

void GestureManager::SendContextMenuEventTouchDragEnd(
    const WebMouseEvent& mouse_event) {
  if (!gesture_context_menu_deferred_ || suppress_mouse_events_from_gestures_) {
    return;
  }

  const gfx::PointF& positon_in_root_frame = mouse_event.PositionInWidget();

  // Don't send contextmenu event if tap position is not within a slop region.
  //
  // TODO(mustaq): We should be reusing gesture touch-slop region here but it
  // seems non-trivial because this code path is called at drag-end, and the
  // drag controller does not sync well with gesture recognizer.  See the
  // blocked-on bugs in https://crbug.com/1096189.
  if ((positon_in_root_frame - long_press_position_in_root_frame_).Length() >
      kTouchDragSlop)
    return;

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
    mouse_event_manager_->SetMousePositionAndDispatchMouseEvent(
        targeted_event.GetHitTestResult().InnerElement(),
        targeted_event.CanvasRegionId(), event_type_names::kMousemove,
        fake_mouse_move);
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
        FlooredIntPoint(targeted_event.Event().PositionInRootFrame())));
    MouseEventWithHitTestResults mev =
        frame_->GetDocument()->PerformMouseEventHitTest(request, document_point,
                                                        mouse_event);
    mouse_event_manager_->HandleMouseFocus(mev.GetHitTestResult(),
                                           frame_->GetDocument()
                                               ->domWindow()
                                               ->GetInputDeviceCapabilities()
                                               ->FiresTouchEvents(true));
  }
  return frame_->GetEventHandler().SendContextMenuEvent(mouse_event);
}

WebInputEventResult GestureManager::HandleGestureShowPress() {
  LocalFrameView* view = frame_->View();
  if (!view)
    return WebInputEventResult::kNotHandled;
  const LocalFrameView::ScrollableAreaSet* areas = view->ScrollableAreas();
  if (!areas)
    return WebInputEventResult::kNotHandled;
  for (const PaintLayerScrollableArea* scrollable_area : *areas) {
    ScrollAnimatorBase* animator = scrollable_area->ExistingScrollAnimator();
    if (scrollable_area->ScrollsOverflow() && animator)
      animator->CancelAnimation();
  }
  return WebInputEventResult::kNotHandled;
}

void GestureManager::ShowUnhandledTapUIIfNeeded(
    bool dom_tree_changed,
    bool style_changed,
    Node* tapped_node,
    Element* tapped_element,
    const IntPoint& tapped_position_in_viewport) {
#if BUILDFLAG(ENABLE_UNHANDLED_TAP)
  WebNode web_node(tapped_node);
  // TODO(donnd): roll in ML-identified signals for suppression once identified.
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

    // Extract text run-length.
    int text_run_length = 0;
    if (tapped_element)
      text_run_length = tapped_element->textContent().length();

    int font_size = 0;
    // Extract text characteristics from the computed style of the tapped node.
    if (const ComputedStyle* style = tapped_node->GetComputedStyle())
      font_size = style->FontSize();

    // TODO(donnd): get the text color and style and return,
    // e.g. style->GetFontWeight() to return bold.  Need italic, color, etc.

    // Notify the Browser.
    auto tapped_info = mojom::blink::UnhandledTapInfo::New(
        tapped_position_in_viewport, font_size, text_run_length);
    provider->ShowUnhandledTapUIIfNeeded(std::move(tapped_info));
  }
#endif  // BUILDFLAG(ENABLE_UNHANDLED_TAP)
}

}  // namespace blink
