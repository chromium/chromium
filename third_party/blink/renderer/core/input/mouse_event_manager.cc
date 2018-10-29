// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/mouse_event_manager.h"

#include "build/build_config.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer_access_policy.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_controller.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/events/drag_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/event_handling_util.h"
#include "third_party/blink/renderer/core/input/input_device_capabilities.h"
#include "third_party/blink/renderer/core/input/keyboard_event_manager.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/drag_controller.h"
#include "third_party/blink/renderer/core/page/drag_state.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/svg/svg_document_extensions.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/histogram.h"

namespace blink {

namespace {

String CanvasRegionId(Node* node, const WebMouseEvent& mouse_event) {
  if (!node->IsElementNode())
    return String();

  Element* element = ToElement(node);
  if (!element->IsInCanvasSubtree())
    return String();

  HTMLCanvasElement* canvas =
      Traversal<HTMLCanvasElement>::FirstAncestorOrSelf(*element);
  // In this case, the event target is canvas and mouse rerouting doesn't
  // happen.
  if (canvas == element)
    return String();
  return canvas->GetIdFromControl(element);
}

// The amount of time to wait before sending a fake mouse event triggered
// during a scroll.
constexpr TimeDelta kFakeMouseMoveIntervalDuringScroll =
    TimeDelta::FromMilliseconds(100);

// The amount of time to wait before sending a fake mouse event on style and
// layout changes sets to 50Hz, same as common screen refresh rate.
constexpr TimeDelta kFakeMouseMoveIntervalAfterLayoutChange =
    TimeDelta::FromMilliseconds(20);

// TODO(crbug.com/653490): Read these values from the OS.
#if defined(OS_MACOSX)
const int kDragThresholdX = 3;
const int kDragThresholdY = 3;
constexpr TimeDelta kTextDragDelay = TimeDelta::FromSecondsD(0.15);
#else
const int kDragThresholdX = 4;
const int kDragThresholdY = 4;
constexpr TimeDelta kTextDragDelay = TimeDelta::FromSecondsD(0.0);
#endif

}  // namespace

enum class DragInitiator { kMouse, kTouch };

MouseEventManager::MouseEventManager(LocalFrame& frame,
                                     ScrollManager& scroll_manager)
    : frame_(frame),
      scroll_manager_(scroll_manager),
      fake_mouse_move_event_timer_(
          frame.GetTaskRunner(TaskType::kUserInteraction),
          this,
          &MouseEventManager::FakeMouseMoveEventTimerFired) {
  Clear();
}

void MouseEventManager::Clear() {
  node_under_mouse_ = nullptr;
  mouse_press_node_ = nullptr;
  mouse_down_may_start_autoscroll_ = false;
  mouse_down_may_start_drag_ = false;
  captures_dragging_ = false;
  is_mouse_position_unknown_ = true;
  last_known_mouse_position_ = FloatPoint();
  last_known_mouse_global_position_ = FloatPoint();
  mouse_pressed_ = false;
  click_count_ = 0;
  click_element_ = nullptr;
  mouse_down_element_ = nullptr;
  mouse_down_pos_ = IntPoint();
  mouse_down_timestamp_ = TimeTicks();
  mouse_down_ = WebMouseEvent();
  svg_pan_ = false;
  drag_start_pos_ = LayoutPoint();
  fake_mouse_move_event_timer_.Stop();
  ResetDragSource();
  ClearDragDataTransfer();
}

MouseEventManager::~MouseEventManager() = default;

void MouseEventManager::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
  visitor->Trace(scroll_manager_);
  visitor->Trace(node_under_mouse_);
  visitor->Trace(mouse_press_node_);
  visitor->Trace(click_element_);
  visitor->Trace(mouse_down_element_);
  SynchronousMutationObserver::Trace(visitor);
}

MouseEventManager::MouseEventBoundaryEventDispatcher::
    MouseEventBoundaryEventDispatcher(MouseEventManager* mouse_event_manager,
                                      const WebMouseEvent* web_mouse_event,
                                      EventTarget* exited_target,
                                      const String& canvas_region_id)
    : mouse_event_manager_(mouse_event_manager),
      web_mouse_event_(web_mouse_event),
      exited_target_(exited_target),
      canvas_region_id_(canvas_region_id) {}

void MouseEventManager::MouseEventBoundaryEventDispatcher::DispatchOut(
    EventTarget* target,
    EventTarget* related_target) {
  Dispatch(target, related_target, EventTypeNames::mouseout,
           CanvasRegionId(exited_target_->ToNode(), *web_mouse_event_),
           *web_mouse_event_, false);
}

void MouseEventManager::MouseEventBoundaryEventDispatcher::DispatchOver(
    EventTarget* target,
    EventTarget* related_target) {
  Dispatch(target, related_target, EventTypeNames::mouseover, canvas_region_id_,
           *web_mouse_event_, false);
}

void MouseEventManager::MouseEventBoundaryEventDispatcher::DispatchLeave(
    EventTarget* target,
    EventTarget* related_target,
    bool check_for_listener) {
  Dispatch(target, related_target, EventTypeNames::mouseleave,
           CanvasRegionId(exited_target_->ToNode(), *web_mouse_event_),
           *web_mouse_event_, check_for_listener);
}

void MouseEventManager::MouseEventBoundaryEventDispatcher::DispatchEnter(
    EventTarget* target,
    EventTarget* related_target,
    bool check_for_listener) {
  Dispatch(target, related_target, EventTypeNames::mouseenter,
           canvas_region_id_, *web_mouse_event_, check_for_listener);
}

AtomicString
MouseEventManager::MouseEventBoundaryEventDispatcher::GetLeaveEvent() {
  return EventTypeNames::mouseleave;
}

AtomicString
MouseEventManager::MouseEventBoundaryEventDispatcher::GetEnterEvent() {
  return EventTypeNames::mouseenter;
}

void MouseEventManager::MouseEventBoundaryEventDispatcher::Dispatch(
    EventTarget* target,
    EventTarget* related_target,
    const AtomicString& type,
    const String& canvas_region_id,
    const WebMouseEvent& web_mouse_event,
    bool check_for_listener) {
  mouse_event_manager_->DispatchMouseEvent(target, type, web_mouse_event,
                                           canvas_region_id, related_target,
                                           check_for_listener);
}

void MouseEventManager::SendBoundaryEvents(EventTarget* exited_target,
                                           EventTarget* entered_target,
                                           const String& canvas_region_id,
                                           const WebMouseEvent& mouse_event) {
  MouseEventBoundaryEventDispatcher boundary_event_dispatcher(
      this, &mouse_event, exited_target, canvas_region_id);
  boundary_event_dispatcher.SendBoundaryEvents(exited_target, entered_target);
}

WebInputEventResult MouseEventManager::DispatchMouseEvent(
    EventTarget* target,
    const AtomicString& mouse_event_type,
    const WebMouseEvent& mouse_event,
    const String& canvas_region_id,
    EventTarget* related_target,
    bool check_for_listener) {
  if (target && target->ToNode() &&
      (!check_for_listener || target->HasEventListeners(mouse_event_type))) {
    Node* target_node = target->ToNode();
    int click_count = 0;
    if (mouse_event_type == EventTypeNames::mouseup ||
        mouse_event_type == EventTypeNames::mousedown ||
        mouse_event_type == EventTypeNames::click ||
        mouse_event_type == EventTypeNames::auxclick ||
        mouse_event_type == EventTypeNames::dblclick) {
      click_count = click_count_;
    }
    bool is_mouse_enter_or_leave =
        mouse_event_type == EventTypeNames::mouseenter ||
        mouse_event_type == EventTypeNames::mouseleave;
    MouseEventInit initializer;
    initializer.setBubbles(!is_mouse_enter_or_leave);
    initializer.setCancelable(!is_mouse_enter_or_leave);
    MouseEvent::SetCoordinatesFromWebPointerProperties(
        mouse_event.FlattenTransform(), target_node->GetDocument().domWindow(),
        initializer);
    initializer.setButton(static_cast<short>(mouse_event.button));
    initializer.setButtons(MouseEvent::WebInputEventModifiersToButtons(
        mouse_event.GetModifiers()));
    initializer.setView(target_node->GetDocument().domWindow());
    initializer.setComposed(true);
    initializer.setDetail(click_count);
    initializer.setRegion(canvas_region_id);
    initializer.setRelatedTarget(related_target);
    UIEventWithKeyState::SetFromWebInputEventModifiers(
        initializer,
        static_cast<WebInputEvent::Modifiers>(mouse_event.GetModifiers()));
    initializer.setSourceCapabilities(
        target_node->GetDocument().domWindow()
            ? target_node->GetDocument()
                  .domWindow()
                  ->GetInputDeviceCapabilities()
                  ->FiresTouchEvents(mouse_event.FromTouch())
            : nullptr);

    MouseEvent* event = MouseEvent::Create(
        mouse_event_type, initializer, mouse_event.TimeStamp(),
        mouse_event.FromTouch() ? MouseEvent::kFromTouch
                                : MouseEvent::kRealOrIndistinguishable,
        mouse_event.menu_source_type);

    DispatchEventResult dispatch_result = target->DispatchEvent(*event);
    return event_handling_util::ToWebInputEventResult(dispatch_result);
  }
  return WebInputEventResult::kNotHandled;
}

WebInputEventResult MouseEventManager::SetMousePositionAndDispatchMouseEvent(
    Node* target_node,
    const String& canvas_region_id,
    const AtomicString& event_type,
    const WebMouseEvent& web_mouse_event) {
  // If the target node is a text node, dispatch on the parent node.
  if (target_node && target_node->IsTextNode())
    target_node = FlatTreeTraversal::Parent(*target_node);

  SetNodeUnderMouse(target_node, canvas_region_id, web_mouse_event);

  return DispatchMouseEvent(node_under_mouse_, event_type, web_mouse_event,
                            canvas_region_id, nullptr);
}

WebInputEventResult MouseEventManager::DispatchMouseClickIfNeeded(
    const MouseEventWithHitTestResults& mev,
    Element& mouse_release_target) {
  // We only prevent click event when the click may cause contextmenu to popup.
  // However, we always send auxclick.
  bool context_menu_event = false;
#if defined(OS_MACOSX)
  // FIXME: The Mac port achieves the same behavior by checking whether the
  // context menu is currently open in WebPage::mouseEvent(). Consider merging
  // the implementations.
  if (mev.Event().button == WebPointerProperties::Button::kLeft &&
      mev.Event().GetModifiers() & WebInputEvent::Modifiers::kControlKey)
    context_menu_event = true;
#endif

  const bool should_dispatch_click_event =
      click_count_ > 0 && !context_menu_event && mouse_down_element_ &&
      mouse_release_target.CanParticipateInFlatTree() &&
      mouse_down_element_->CanParticipateInFlatTree() &&
      mouse_down_element_->isConnected() &&
      !(frame_->GetEventHandler()
            .GetSelectionController()
            .HasExtendedSelection() &&
        IsLinkSelection(mev));
  if (!should_dispatch_click_event)
    return WebInputEventResult::kNotHandled;

  Node* click_target_node = nullptr;
  if (mouse_down_element_ == mouse_release_target) {
    click_target_node = mouse_down_element_;
  } else if (mouse_down_element_->GetDocument() ==
             mouse_release_target.GetDocument()) {
    // Updates distribution because a 'mouseup' event listener can make the
    // tree dirty at dispatchMouseEvent() invocation above.
    // Unless distribution is updated, commonAncestor would hit ASSERT.
    mouse_down_element_->UpdateDistributionForFlatTreeTraversal();
    mouse_release_target.UpdateDistributionForFlatTreeTraversal();
    click_target_node = mouse_release_target.CommonAncestor(
        *mouse_down_element_, event_handling_util::ParentForClickEvent);
  }
  if (!click_target_node)
    return WebInputEventResult::kNotHandled;

  DEFINE_STATIC_LOCAL(BooleanHistogram, histogram,
                      ("Event.ClickNotFiredDueToDomManipulation"));

  if (click_element_ && click_element_->CanParticipateInFlatTree() &&
      click_element_->isConnected()) {
    DCHECK(click_element_ == mouse_down_element_);
    histogram.Count(false);
  } else {
    histogram.Count(true);
  }

  if ((click_element_ && click_element_->CanParticipateInFlatTree() &&
       click_element_->isConnected()) ||
      RuntimeEnabledFeatures::ClickRetargettingEnabled()) {
    return DispatchMouseEvent(
        click_target_node,
        (mev.Event().button == WebPointerProperties::Button::kLeft)
            ? EventTypeNames::click
            : EventTypeNames::auxclick,
        mev.Event(), mev.CanvasRegionId(), nullptr);
  }

  return WebInputEventResult::kNotHandled;
}

void MouseEventManager::FakeMouseMoveEventTimerFired(TimerBase* timer) {
  TRACE_EVENT0("input", "MouseEventManager::fakeMouseMoveEventTimerFired");
  DCHECK(timer == &fake_mouse_move_event_timer_);

  if (is_mouse_position_unknown_)
    return;

  LocalFrameView* view = frame_->View();
  if (!view)
    return;

  if (!frame_->GetPage() || !frame_->GetPage()->GetFocusController().IsActive())
    return;

  // Don't dispatch a synthetic mouse move event if the mouse cursor is not
  // visible to the user.
  if (!frame_->GetPage()->IsCursorVisible())
    return;

  WebPointerEvent::Button button = WebPointerProperties::Button::kNoButton;
  int modifiers = KeyboardEventManager::GetCurrentModifierState() |
                  WebInputEvent::kRelativeMotionEvent;
  if (mouse_pressed_) {
    button = WebPointerProperties::Button::kLeft;
    modifiers |= WebInputEvent::kLeftButtonDown;
  }
  WebMouseEvent fake_mouse_move_event(WebInputEvent::kMouseMove,
                                      last_known_mouse_position_,
                                      last_known_mouse_global_position_, button,
                                      0, modifiers, CurrentTimeTicks());
  Vector<WebMouseEvent> coalesced_events, predicted_events;
  frame_->GetEventHandler().HandleMouseMoveEvent(
      TransformWebMouseEvent(view, fake_mouse_move_event), coalesced_events,
      predicted_events);
}

void MouseEventManager::CancelFakeMouseMoveEvent() {
  fake_mouse_move_event_timer_.Stop();
}

void MouseEventManager::SetNodeUnderMouse(
    Node* target,
    const String& canvas_region_id,
    const WebMouseEvent& web_mouse_event) {
  Node* last_node_under_mouse = node_under_mouse_;
  node_under_mouse_ = target;

  PaintLayer* layer_for_last_node =
      event_handling_util::LayerForNode(last_node_under_mouse);
  PaintLayer* layer_for_node_under_mouse =
      event_handling_util::LayerForNode(node_under_mouse_.Get());
  Page* page = frame_->GetPage();

  if (page && (layer_for_last_node &&
               (!layer_for_node_under_mouse ||
                layer_for_node_under_mouse != layer_for_last_node))) {
    // The mouse has moved between layers.
    if (ScrollableArea* scrollable_area_for_last_node =
            event_handling_util::AssociatedScrollableArea(layer_for_last_node))
      scrollable_area_for_last_node->MouseExitedContentArea();
  }

  if (page && (layer_for_node_under_mouse &&
               (!layer_for_last_node ||
                layer_for_node_under_mouse != layer_for_last_node))) {
    // The mouse has moved between layers.
    if (ScrollableArea* scrollable_area_for_node_under_mouse =
            event_handling_util::AssociatedScrollableArea(
                layer_for_node_under_mouse))
      scrollable_area_for_node_under_mouse->MouseEnteredContentArea();
  }

  if (last_node_under_mouse &&
      last_node_under_mouse->GetDocument() != frame_->GetDocument()) {
    last_node_under_mouse = nullptr;
  }

  SendBoundaryEvents(last_node_under_mouse, node_under_mouse_, canvas_region_id,
                     web_mouse_event);
}

void MouseEventManager::NodeChildrenWillBeRemoved(ContainerNode& container) {
  if (container == click_element_)
    return;
  if (!container.IsShadowIncludingInclusiveAncestorOf(click_element_.Get()))
    return;
  click_element_ = nullptr;

  // TODO(crbug.com/716694): Do not reset mouse_down_element_ for the purpose of
  // gathering data.
}

void MouseEventManager::NodeWillBeRemoved(Node& node_to_be_removed) {
  if (node_to_be_removed.IsShadowIncludingInclusiveAncestorOf(
          click_element_.Get())) {
    // We don't dispatch click events if the mousedown node is removed
    // before a mouseup event. It is compatible with IE and Firefox.
    click_element_ = nullptr;

    // TODO(crbug.com/716694): Do not reset mouse_down_element_ for the purpose
    // of gathering data.
  }
}

Node* MouseEventManager::GetNodeUnderMouse() {
  return node_under_mouse_;
}

WebInputEventResult MouseEventManager::HandleMouseFocus(
    const HitTestResult& hit_test_result,
    InputDeviceCapabilities* source_capabilities) {
  // If clicking on a frame scrollbar, do not mess up with content focus.
  if (auto* layout_view = frame_->ContentLayoutObject()) {
    if (hit_test_result.GetScrollbar() && frame_->ContentLayoutObject()) {
      if (hit_test_result.GetScrollbar()->GetScrollableArea() ==
          layout_view->GetScrollableArea())
        return WebInputEventResult::kNotHandled;
    }
  }

  // The layout needs to be up to date to determine if an element is focusable.
  frame_->GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  Element* element = nullptr;
  if (node_under_mouse_) {
    element = node_under_mouse_->IsElementNode()
                  ? ToElement(node_under_mouse_)
                  : node_under_mouse_->ParentOrShadowHostElement();
  }
  for (; element; element = element->ParentOrShadowHostElement()) {
    if (element->IsFocusable() && element->IsFocusedElementInDocument())
      return WebInputEventResult::kNotHandled;
    if (element->IsMouseFocusable())
      break;
  }
  DCHECK(!element || element->IsMouseFocusable());

  // To fix <rdar://problem/4895428> Can't drag selected ToDo, we don't focus
  // a node on mouse down if it's selected and inside a focused node. It will
  // be focused if the user does a mouseup over it, however, because the
  // mouseup will set a selection inside it, which will call
  // FrameSelection::setFocusedNodeIfNeeded.
  // TODO(editing-dev): The use of VisibleSelection should be audited. See
  // crbug.com/657237 for details.
  if (element &&
      frame_->Selection().ComputeVisibleSelectionInDOMTree().IsRange()) {
    const EphemeralRange& range = frame_->Selection()
                                      .ComputeVisibleSelectionInDOMTree()
                                      .ToNormalizedEphemeralRange();
    if (IsNodeFullyContained(range, *element) &&
        element->IsDescendantOf(frame_->GetDocument()->FocusedElement()))
      return WebInputEventResult::kNotHandled;
  }

  // Only change the focus when clicking scrollbars if it can transfered to a
  // mouse focusable node.
  if (!element && hit_test_result.GetScrollbar())
    return WebInputEventResult::kHandledSystem;

  if (Page* page = frame_->GetPage()) {
    // If focus shift is blocked, we eat the event. Note we should never
    // clear swallowEvent if the page already set it (e.g., by canceling
    // default behavior).
    if (element) {
      if (SlideFocusOnShadowHostIfNecessary(*element))
        return WebInputEventResult::kHandledSystem;
      if (!page->GetFocusController().SetFocusedElement(
              element, frame_,
              FocusParams(SelectionBehaviorOnFocus::kNone, kWebFocusTypeMouse,
                          source_capabilities)))
        return WebInputEventResult::kHandledSystem;
    } else {
      // We call setFocusedElement even with !element in order to blur
      // current focus element when a link is clicked; this is expected by
      // some sites that rely on onChange handlers running from form
      // fields before the button click is processed.
      if (!page->GetFocusController().SetFocusedElement(
              nullptr, frame_,
              FocusParams(SelectionBehaviorOnFocus::kNone, kWebFocusTypeNone,
                          source_capabilities)))
        return WebInputEventResult::kHandledSystem;
    }
  }

  return WebInputEventResult::kNotHandled;
}

bool MouseEventManager::SlideFocusOnShadowHostIfNecessary(
    const Element& element) {
  if (element.AuthorShadowRoot() &&
      element.AuthorShadowRoot()->delegatesFocus()) {
    Document* doc = frame_->GetDocument();
    if (element.IsShadowIncludingInclusiveAncestorOf(doc->FocusedElement())) {
      // If the inner element is already focused, do nothing.
      return true;
    }

    // If the host has a focusable inner element, focus it. Otherwise, the host
    // takes focus.
    Page* page = frame_->GetPage();
    DCHECK(page);
    Element* found =
        page->GetFocusController().FindFocusableElementInShadowHost(element);
    if (found && element.IsShadowIncludingInclusiveAncestorOf(found)) {
      // Use WebFocusTypeForward instead of WebFocusTypeMouse here to mean the
      // focus has slided.
      found->focus(FocusParams(SelectionBehaviorOnFocus::kReset,
                               kWebFocusTypeForward, nullptr));
      return true;
    }
  }
  return false;
}

void MouseEventManager::HandleMouseReleaseEventUpdateStates() {
  ClearDragHeuristicState();
  InvalidateClick();
  frame_->GetEventHandler().GetSelectionController().SetMouseDownMayStartSelect(
      false);
}

void MouseEventManager::HandleMousePressEventUpdateStates(
    const WebMouseEvent& mouse_event) {
  CancelFakeMouseMoveEvent();
  mouse_pressed_ = true;
  captures_dragging_ = true;
  SetLastKnownMousePosition(mouse_event);
  mouse_down_may_start_drag_ = false;
  mouse_down_may_start_autoscroll_ = false;
  mouse_down_timestamp_ = mouse_event.TimeStamp();

  if (LocalFrameView* view = frame_->View()) {
    mouse_down_pos_ = view->ConvertFromRootFrame(
        FlooredIntPoint(mouse_event.PositionInRootFrame()));
  } else {
    InvalidateClick();
  }

  frame_->GetEventHandler().GetSelectionController().SetMouseDownMayStartSelect(
      false);
}

bool MouseEventManager::IsMousePositionUnknown() {
  return is_mouse_position_unknown_;
}

IntPoint MouseEventManager::LastKnownMousePosition() {
  return FlooredIntPoint(last_known_mouse_position_);
}

FloatPoint MouseEventManager::LastKnownMousePositionGlobal() {
  return last_known_mouse_global_position_;
}

void MouseEventManager::SetLastKnownMousePosition(const WebMouseEvent& event) {
  is_mouse_position_unknown_ = event.GetType() == WebInputEvent::kMouseLeave;
  last_known_mouse_position_ = event.PositionInWidget();
  last_known_mouse_global_position_ = event.PositionInScreen();
}

void MouseEventManager::SetLastMousePositionAsUnknown() {
  is_mouse_position_unknown_ = true;
}

void MouseEventManager::MayUpdateHoverWhenContentUnderMouseChanged(
    MouseEventManager::UpdateHoverReason update_hover_reason) {
  if (RuntimeEnabledFeatures::NoHoverAfterLayoutChangeEnabled() &&
      update_hover_reason ==
          MouseEventManager::UpdateHoverReason::kLayoutOrStyleChanged) {
    return;
  }

  if (update_hover_reason ==
          MouseEventManager::UpdateHoverReason::kScrollOffsetChanged &&
      (RuntimeEnabledFeatures::NoHoverDuringScrollEnabled() ||
       mouse_pressed_)) {
    return;
  }

  // TODO(lanwei): When the mouse position is unknown, we do not send the fake
  // mousemove event for now, so we cannot update the hover states and mouse
  // cursor. We should keep the last mouse position somewhere in browser.
  // Please see crbug.com/307375, crbug.com/714378.
  if (is_mouse_position_unknown_)
    return;

  // Reschedule the timer, to prevent dispatching mouse move events
  // during a scroll. This avoids a potential source of scroll jank.
  // Or dispatch a fake mouse move to update hover states when the layout
  // changes.
  TimeDelta interval =
      update_hover_reason ==
              MouseEventManager::UpdateHoverReason::kScrollOffsetChanged
          ? kFakeMouseMoveIntervalDuringScroll
          : kFakeMouseMoveIntervalAfterLayoutChange;
  fake_mouse_move_event_timer_.StartOneShot(interval, FROM_HERE);
}

void MouseEventManager::MayUpdateHoverAfterScroll(
    const FloatQuad& scroller_rect_in_frame) {
  LocalFrameView* view = frame_->View();
  if (!view)
    return;

  if (!scroller_rect_in_frame.ContainsPoint(
          view->ViewportToFrame(last_known_mouse_position_)))
    return;

  MayUpdateHoverWhenContentUnderMouseChanged(
      MouseEventManager::UpdateHoverReason::kScrollOffsetChanged);
}

WebInputEventResult MouseEventManager::HandleMousePressEvent(
    const MouseEventWithHitTestResults& event) {
  TRACE_EVENT0("blink", "MouseEventManager::handleMousePressEvent");

  ResetDragSource();
  CancelFakeMouseMoveEvent();

  frame_->GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  bool single_click = event.Event().click_count <= 1;

  mouse_down_may_start_drag_ =
      single_click && !IsLinkSelection(event) && !IsExtendingSelection(event);

  mouse_down_ = event.Event();

  if (frame_->GetDocument()->IsSVGDocument() &&
      frame_->GetDocument()->AccessSVGExtensions().ZoomAndPanEnabled()) {
    if ((event.Event().GetModifiers() & WebInputEvent::Modifiers::kShiftKey) &&
        single_click) {
      svg_pan_ = true;
      frame_->GetDocument()->AccessSVGExtensions().StartPan(
          frame_->View()->ConvertFromRootFrame(FloatPoint(
              FlooredIntPoint(event.Event().PositionInRootFrame()))));
      return WebInputEventResult::kHandledSystem;
    }
  }

  // We don't do this at the start of mouse down handling,
  // because we don't want to do it until we know we didn't hit a widget.
  if (single_click)
    FocusDocumentView();

  Node* inner_node = event.InnerNode();

  mouse_press_node_ = inner_node;
  frame_->GetDocument()->SetSequentialFocusNavigationStartingPoint(inner_node);
  drag_start_pos_ = FlooredIntPoint(event.Event().PositionInRootFrame());

  mouse_pressed_ = true;

  bool swallow_event =
      frame_->GetEventHandler().GetSelectionController().HandleMousePressEvent(
          event);

  mouse_down_may_start_autoscroll_ =
      frame_->GetEventHandler()
          .GetSelectionController()
          .MouseDownMayStartSelect() ||
      (mouse_press_node_ && mouse_press_node_->GetLayoutBox() &&
       mouse_press_node_->GetLayoutBox()->CanBeProgramaticallyScrolled());

  return swallow_event ? WebInputEventResult::kHandledSystem
                       : WebInputEventResult::kNotHandled;
}

WebInputEventResult MouseEventManager::HandleMouseReleaseEvent(
    const MouseEventWithHitTestResults& event) {
  AutoscrollController* controller = scroll_manager_->GetAutoscrollController();
  if (controller && controller->SelectionAutoscrollInProgress())
    scroll_manager_->StopAutoscroll();

  return frame_->GetEventHandler()
                 .GetSelectionController()
                 .HandleMouseReleaseEvent(event, drag_start_pos_)
             ? WebInputEventResult::kHandledSystem
             : WebInputEventResult::kNotHandled;
}

void MouseEventManager::UpdateSelectionForMouseDrag() {
  frame_->GetEventHandler()
      .GetSelectionController()
      .UpdateSelectionForMouseDrag(drag_start_pos_,
                                   LayoutPoint(last_known_mouse_position_));
}

bool MouseEventManager::HandleDragDropIfPossible(
    const GestureEventWithHitTestResults& targeted_event) {
  if (frame_->GetSettings() &&
      frame_->GetSettings()->GetTouchDragDropEnabled() && frame_->View()) {
    const WebGestureEvent& gesture_event = targeted_event.Event();
    unsigned modifiers = gesture_event.GetModifiers();

    // TODO(mustaq): Suppressing long-tap MouseEvents could break
    // drag-drop. Will do separately because of the risk. crbug.com/606938.
    WebMouseEvent mouse_down_event(
        WebInputEvent::kMouseDown, gesture_event,
        WebPointerProperties::Button::kLeft, 1,
        modifiers | WebInputEvent::Modifiers::kLeftButtonDown |
            WebInputEvent::Modifiers::kIsCompatibilityEventForTouch,
        CurrentTimeTicks());
    mouse_down_ = mouse_down_event;

    WebMouseEvent mouse_drag_event(
        WebInputEvent::kMouseMove, gesture_event,
        WebPointerProperties::Button::kLeft, 1,
        modifiers | WebInputEvent::Modifiers::kLeftButtonDown |
            WebInputEvent::Modifiers::kIsCompatibilityEventForTouch,
        CurrentTimeTicks());
    HitTestRequest request(HitTestRequest::kReadOnly);
    MouseEventWithHitTestResults mev =
        event_handling_util::PerformMouseEventHitTest(frame_, request,
                                                      mouse_drag_event);
    mouse_down_may_start_drag_ = true;
    ResetDragSource();
    mouse_down_pos_ = frame_->View()->ConvertFromRootFrame(
        FlooredIntPoint(mouse_drag_event.PositionInRootFrame()));
    return HandleDrag(mev, DragInitiator::kTouch);
  }
  return false;
}

void MouseEventManager::FocusDocumentView() {
  Page* page = frame_->GetPage();
  if (!page)
    return;
  page->GetFocusController().FocusDocumentView(frame_);
}

WebInputEventResult MouseEventManager::HandleMouseDraggedEvent(
    const MouseEventWithHitTestResults& event) {
  TRACE_EVENT0("blink", "MouseEventManager::handleMouseDraggedEvent");

  bool is_pen = event.Event().pointer_type ==
                blink::WebPointerProperties::PointerType::kPen;

  WebPointerProperties::Button pen_drag_button =
      WebPointerProperties::Button::kLeft;
  if (frame_->GetSettings() &&
      frame_->GetSettings()->GetBarrelButtonForDragEnabled())
    pen_drag_button = WebPointerProperties::Button::kBarrel;

  // While resetting m_mousePressed here may seem out of place, it turns out
  // to be needed to handle some bugs^Wfeatures in Blink mouse event handling:
  // 1. Certain elements, such as <embed>, capture mouse events. They do not
  //    bubble back up. One way for a <embed> to start capturing mouse events
  //    is on a mouse press. The problem is the <embed> node only starts
  //    capturing mouse events *after* m_mousePressed for the containing frame
  //    has already been set to true. As a result, the frame's EventHandler
  //    never sees the mouse release event, which is supposed to reset
  //    m_mousePressed... so m_mousePressed ends up remaining true until the
  //    event handler finally gets another mouse released event. Oops.
  // 2. Dragging doesn't start until after a mouse press event, but a drag
  //    that ends as a result of a mouse release does not send a mouse release
  //    event. As a result, m_mousePressed also ends up remaining true until
  //    the next mouse release event seen by the EventHandler.
  if ((!is_pen &&
       event.Event().button != WebPointerProperties::Button::kLeft) ||
      (is_pen && event.Event().button != pen_drag_button)) {
    mouse_pressed_ = false;
  }

  //  When pressing Esc key while dragging and the object is outside of the
  //  we get a mouse leave event here.
  if (!mouse_pressed_ || event.Event().GetType() == WebInputEvent::kMouseLeave)
    return WebInputEventResult::kNotHandled;

  // We disable the drag and drop actions on pen input on windows.
  bool should_handle_drag = true;
#if defined(OS_WIN)
  should_handle_drag = !is_pen;
#endif

  if (should_handle_drag && HandleDrag(event, DragInitiator::kMouse))
    return WebInputEventResult::kHandledSystem;

  Node* target_node = event.InnerNode();
  if (!target_node)
    return WebInputEventResult::kNotHandled;

  LayoutObject* layout_object = target_node->GetLayoutObject();
  if (!layout_object) {
    Node* parent = FlatTreeTraversal::Parent(*target_node);
    if (!parent)
      return WebInputEventResult::kNotHandled;

    layout_object = parent->GetLayoutObject();
    if (!layout_object || !layout_object->IsListBox())
      return WebInputEventResult::kNotHandled;
  }

  mouse_down_may_start_drag_ = false;

  frame_->GetEventHandler().GetSelectionController().HandleMouseDraggedEvent(
      event, mouse_down_pos_, drag_start_pos_,
      LayoutPoint(last_known_mouse_position_));

  // The call into HandleMouseDraggedEvent may have caused a re-layout,
  // so get the LayoutObject again.
  layout_object = target_node->GetLayoutObject();

  if (layout_object && mouse_down_may_start_autoscroll_ &&
      !scroll_manager_->MiddleClickAutoscrollInProgress() &&
      !frame_->Selection().SelectedHTMLForClipboard().IsEmpty()) {
    if (AutoscrollController* controller =
            scroll_manager_->GetAutoscrollController()) {
      // Avoid updating the lifecycle unless it's possible to autoscroll.
      layout_object->GetFrameView()->UpdateAllLifecyclePhasesExceptPaint();

      // The lifecycle update above may have invalidated the previous layout.
      layout_object = target_node->GetLayoutObject();
      if (layout_object) {
        controller->StartAutoscrollForSelection(layout_object);
        mouse_down_may_start_autoscroll_ = false;
      }
    }
  }

  return WebInputEventResult::kHandledSystem;
}

bool MouseEventManager::HandleDrag(const MouseEventWithHitTestResults& event,
                                   DragInitiator initiator) {
  DCHECK(event.Event().GetType() == WebInputEvent::kMouseMove);
  // Callers must protect the reference to LocalFrameView, since this function
  // may dispatch DOM events, causing page/LocalFrameView to go away.
  DCHECK(frame_);
  DCHECK(frame_->View());
  if (!frame_->GetPage())
    return false;

  if (mouse_down_may_start_drag_) {
    HitTestRequest request(HitTestRequest::kReadOnly);
    HitTestLocation location(mouse_down_pos_);
    HitTestResult result(request, location);
    frame_->ContentLayoutObject()->HitTest(location, result);
    Node* node = result.InnerNode();
    if (node) {
      DragController::SelectionDragPolicy selection_drag_policy =
          event.Event().TimeStamp() - mouse_down_timestamp_ < kTextDragDelay
              ? DragController::kDelayedSelectionDragResolution
              : DragController::kImmediateSelectionDragResolution;
      GetDragState().drag_src_ =
          frame_->GetPage()->GetDragController().DraggableNode(
              frame_, node, mouse_down_pos_, selection_drag_policy,
              GetDragState().drag_type_);
    } else {
      ResetDragSource();
    }

    if (!GetDragState().drag_src_)
      mouse_down_may_start_drag_ = false;  // no element is draggable
  }

  if (!mouse_down_may_start_drag_) {
    return initiator == DragInitiator::kMouse &&
           !frame_->GetEventHandler()
                .GetSelectionController()
                .MouseDownMayStartSelect() &&
           !mouse_down_may_start_autoscroll_;
  }

  if (initiator == DragInitiator::kMouse &&
      !DragThresholdExceeded(
          FlooredIntPoint(event.Event().PositionInRootFrame()))) {
    ResetDragSource();
    return true;
  }

  // Once we're past the drag threshold, we don't want to treat this gesture as
  // a click.
  InvalidateClick();

  if (!TryStartDrag(event)) {
    // Something failed to start the drag, clean up.
    ClearDragDataTransfer();
    ResetDragSource();
  } else {
    // Since drag operation started we need to send a pointercancel for the
    // corresponding pointer.
    if (initiator == DragInitiator::kMouse) {
      frame_->GetEventHandler().HandlePointerEvent(
          WebPointerEvent::CreatePointerCausesUaActionEvent(
              WebPointerProperties::PointerType::kMouse,
              event.Event().TimeStamp()),
          Vector<WebPointerEvent>(), Vector<WebPointerEvent>());
    }
    // TODO(crbug.com/708278): If the drag starts with touch the touch cancel
    // should trigger the release of pointer capture.
  }

  mouse_down_may_start_drag_ = false;
  // Whether or not the drag actually started, no more default handling (like
  // selection).
  return true;
}

DataTransfer* MouseEventManager::CreateDraggingDataTransfer() const {
  return DataTransfer::Create(DataTransfer::kDragAndDrop,
                              DataTransferAccessPolicy::kWritable,
                              DataObject::Create());
}

bool MouseEventManager::TryStartDrag(
    const MouseEventWithHitTestResults& event) {
  // The DataTransfer would only be non-empty if we missed a dragEnd.
  // Clear it anyway, just to make sure it gets numbified.
  ClearDragDataTransfer();

  GetDragState().drag_data_transfer_ = CreateDraggingDataTransfer();

  DragController& drag_controller = frame_->GetPage()->GetDragController();
  if (!drag_controller.PopulateDragDataTransfer(frame_, GetDragState(),
                                                mouse_down_pos_))
    return false;

  if (DispatchDragSrcEvent(EventTypeNames::dragstart, mouse_down_) !=
      WebInputEventResult::kNotHandled)
    return false;

  // Dispatching the event could cause |frame_| to be detached.
  if (!frame_->GetPage())
    return false;

  // If dispatching dragstart brings about another mouse down -- one way
  // this will happen is if a DevTools user breaks within a dragstart
  // handler and then clicks on the suspended page -- the drag state is
  // reset. Hence, need to check if this particular drag operation can
  // continue even if dispatchEvent() indicates no (direct) cancellation.
  // Do that by checking if m_dragSrc is still set.
  if (!GetDragState().drag_src_)
    return false;

  // Do not start dragging in password field.
  // TODO(editing-dev): The use of
  // updateStyleAndLayoutIgnorePendingStylesheets needs to be audited.  See
  // http://crbug.com/590369 for more details.
  frame_->GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();
  if (IsInPasswordField(
          frame_->Selection().ComputeVisibleSelectionInDOMTree().Start()))
    return false;

  // Invalidate clipboard here against anymore pasteboard writing for
  // security. The drag image can still be changed as we drag, but not
  // the pasteboard data.
  GetDragState().drag_data_transfer_->SetAccessPolicy(
      DataTransferAccessPolicy::kImageWritable);

  if (drag_controller.StartDrag(frame_, GetDragState(), event.Event(),
                                mouse_down_pos_))
    return true;

  // Drag was canned at the last minute - we owe m_dragSrc a DRAGEND event
  DispatchDragSrcEvent(EventTypeNames::dragend, event.Event());

  return false;
}

// Returns if we should continue "default processing", i.e., whether
// eventhandler canceled.
WebInputEventResult MouseEventManager::DispatchDragSrcEvent(
    const AtomicString& event_type,
    const WebMouseEvent& event) {
  return DispatchDragEvent(event_type, GetDragState().drag_src_.Get(), nullptr,
                           event, GetDragState().drag_data_transfer_.Get());
}

WebInputEventResult MouseEventManager::DispatchDragEvent(
    const AtomicString& event_type,
    Node* drag_target,
    Node* related_target,
    const WebMouseEvent& event,
    DataTransfer* data_transfer) {
  LocalFrameView* view = frame_->View();
  // FIXME: We might want to dispatch a dragleave even if the view is gone.
  if (!view)
    return WebInputEventResult::kNotHandled;

  // We should be setting relatedTarget correctly following the spec:
  // https://html.spec.whatwg.org/multipage/interaction.html#dragevent
  // At the same time this should prevent exposing a node from another document.
  if (related_target &&
      related_target->GetDocument() != drag_target->GetDocument())
    related_target = nullptr;

  DragEventInit initializer;
  initializer.setBubbles(true);
  initializer.setCancelable(event_type != EventTypeNames::dragleave &&
                            event_type != EventTypeNames::dragend);
  MouseEvent::SetCoordinatesFromWebPointerProperties(
      event.FlattenTransform(), frame_->GetDocument()->domWindow(),
      initializer);
  initializer.setButton(0);
  initializer.setButtons(
      MouseEvent::WebInputEventModifiersToButtons(event.GetModifiers()));
  initializer.setRelatedTarget(related_target);
  initializer.setView(frame_->GetDocument()->domWindow());
  initializer.setComposed(true);
  initializer.setGetDataTransfer(data_transfer);
  initializer.setSourceCapabilities(
      frame_->GetDocument()->domWindow()
          ? frame_->GetDocument()
                ->domWindow()
                ->GetInputDeviceCapabilities()
                ->FiresTouchEvents(event.FromTouch())
          : nullptr);
  UIEventWithKeyState::SetFromWebInputEventModifiers(
      initializer, static_cast<WebInputEvent::Modifiers>(event.GetModifiers()));

  DragEvent* me = DragEvent::Create(event_type, initializer, event.TimeStamp(),
                                    event.FromTouch()
                                        ? MouseEvent::kFromTouch
                                        : MouseEvent::kRealOrIndistinguishable);

  return event_handling_util::ToWebInputEventResult(
      drag_target->DispatchEvent(*me));
}

void MouseEventManager::ClearDragDataTransfer() {
  if (!frame_->GetPage())
    return;
  if (GetDragState().drag_data_transfer_) {
    GetDragState().drag_data_transfer_->ClearDragImage();
    GetDragState().drag_data_transfer_->SetAccessPolicy(
        DataTransferAccessPolicy::kNumb);
  }
}

void MouseEventManager::DragSourceEndedAt(const WebMouseEvent& event,
                                          DragOperation operation) {
  if (GetDragState().drag_src_) {
    GetDragState().drag_data_transfer_->SetDestinationOperation(operation);
    // The return value is ignored because dragend is not cancelable.
    DispatchDragSrcEvent(EventTypeNames::dragend, event);
  }
  ClearDragDataTransfer();
  ResetDragSource();
  // In case the drag was ended due to an escape key press we need to ensure
  // that consecutive mousemove events don't reinitiate the drag and drop.
  mouse_down_may_start_drag_ = false;
}

DragState& MouseEventManager::GetDragState() {
  DCHECK(frame_->GetPage());
  return frame_->GetPage()->GetDragController().GetDragState();
}

void MouseEventManager::ResetDragSource() {
  if (!frame_->GetPage())
    return;
  GetDragState().drag_src_ = nullptr;
}

bool MouseEventManager::DragThresholdExceeded(
    const IntPoint& drag_location_in_root_frame) const {
  LocalFrameView* view = frame_->View();
  if (!view)
    return false;
  IntPoint drag_location =
      view->ConvertFromRootFrame(drag_location_in_root_frame);
  IntSize delta = drag_location - mouse_down_pos_;

  // WebKit's drag thresholds depend on the type of object being dragged. If we
  // want to revive that behavior, we can multiply the threshold constants with
  // a number based on dragState().m_dragType.

  return abs(delta.Width()) >= kDragThresholdX ||
         abs(delta.Height()) >= kDragThresholdY;
}

void MouseEventManager::ClearDragHeuristicState() {
  // Used to prevent mouseMoveEvent from initiating a drag before
  // the mouse is pressed again.
  mouse_pressed_ = false;
  captures_dragging_ = false;
  mouse_down_may_start_drag_ = false;
  mouse_down_may_start_autoscroll_ = false;
}

bool MouseEventManager::HandleSvgPanIfNeeded(bool is_release_event) {
  if (!svg_pan_)
    return false;
  svg_pan_ = !is_release_event;
  frame_->GetDocument()->AccessSVGExtensions().UpdatePan(
      frame_->View()->ViewportToFrame(last_known_mouse_position_));
  return true;
}

void MouseEventManager::InvalidateClick() {
  click_count_ = 0;
  click_element_ = nullptr;
  mouse_down_element_ = nullptr;
}

bool MouseEventManager::MousePressed() {
  return mouse_pressed_;
}

void MouseEventManager::ReleaseMousePress() {
  mouse_pressed_ = false;
}

bool MouseEventManager::CapturesDragging() const {
  return captures_dragging_;
}

void MouseEventManager::SetCapturesDragging(bool captures_dragging) {
  captures_dragging_ = captures_dragging;
}

Node* MouseEventManager::MousePressNode() {
  return mouse_press_node_;
}

void MouseEventManager::SetMousePressNode(Node* node) {
  mouse_press_node_ = node;
}

void MouseEventManager::SetClickElement(Element* element) {
  SetContext(element ? element->ownerDocument() : nullptr);
  click_element_ = element;
  mouse_down_element_ = element;
}

void MouseEventManager::SetClickCount(int click_count) {
  click_count_ = click_count;
}

bool MouseEventManager::MouseDownMayStartDrag() {
  return mouse_down_may_start_drag_;
}

bool MouseEventManager::FakeMouseMovePending() const {
  return fake_mouse_move_event_timer_.IsActive();
}

}  // namespace blink
