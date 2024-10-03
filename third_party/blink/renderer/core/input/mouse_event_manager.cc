// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/mouse_event_manager.h"

#include "build/build_config.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_drag_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_pointer_event_init.h"
#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer_access_policy.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_controller.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/events/drag_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/pointer_event_factory.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
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
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/pointer_lock_controller.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/svg/svg_document_extensions.h"
#include "third_party/blink/renderer/core/timing/event_timing.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace blink {

namespace {

void UpdateMouseMovementXY(const WebMouseEvent& mouse_event,
                           const gfx::PointF* last_position,
                           LocalDOMWindow* dom_window,
                           MouseEventInit* initializer) {
  if (!mouse_event.is_raw_movement_event &&
      mouse_event.GetType() == WebInputEvent::Type::kMouseMove &&
      last_position) {
    // movementX/Y is type int for now, so we need to truncated the coordinates
    // before calculate movement.
    initializer->setMovementX(
        base::saturated_cast<int>(mouse_event.PositionInScreen().x()) -
        base::saturated_cast<int>(last_position->x()));
    initializer->setMovementY(
        base::saturated_cast<int>(mouse_event.PositionInScreen().y()) -
        base::saturated_cast<int>(last_position->y()));
  }
}

void SetMouseEventAttributes(MouseEventInit* initializer,
                             Node* target_node,
                             const AtomicString& mouse_event_type,
                             const WebMouseEvent& mouse_event,
                             const gfx::PointF* last_position,
                             EventTarget* related_target,
                             int click_count) {
  bool is_mouse_enter_or_leave =
      mouse_event_type == event_type_names::kMouseenter ||
      mouse_event_type == event_type_names::kMouseleave;

  initializer->setBubbles(!is_mouse_enter_or_leave);
  initializer->setCancelable(!is_mouse_enter_or_leave);
  MouseEvent::SetCoordinatesFromWebPointerProperties(
      mouse_event.FlattenTransform(), target_node->GetDocument().domWindow(),
      initializer);
  UpdateMouseMovementXY(mouse_event, last_position,
                        target_node->GetDocument().domWindow(), initializer);
  initializer->setButton(static_cast<int16_t>(mouse_event.button));
  initializer->setButtons(
      MouseEvent::WebInputEventModifiersToButtons(mouse_event.GetModifiers()));
  initializer->setView(target_node->GetDocument().domWindow());
  initializer->setComposed(!is_mouse_enter_or_leave);
  initializer->setDetail(click_count);
  initializer->setRelatedTarget(related_target);
  UIEventWithKeyState::SetFromWebInputEventModifiers(
      initializer,
      static_cast<WebInputEvent::Modifiers>(mouse_event.GetModifiers()));
  initializer->setSourceCapabilities(
      target_node->GetDocument().domWindow()
          ? target_node->GetDocument()
                .domWindow()
                ->GetInputDeviceCapabilities()
                ->FiresTouchEvents(mouse_event.FromTouch())
          : nullptr);
}

// TODO(crbug.com/653490): Read these values from the OS.
#if BUILDFLAG(IS_MAC)
const int kDragThresholdX = 3;
const int kDragThresholdY = 3;
constexpr base::TimeDelta kTextDragDelay = base::Seconds(0.15);
#else
const int kDragThresholdX = 4;
const int kDragThresholdY = 4;
constexpr base::TimeDelta kTextDragDelay = base::Seconds(0.0);
#endif

}  // namespace

enum class DragInitiator { kMouse, kTouch };

MouseEventManager::MouseEventManager(LocalFrame& frame,
                                     ScrollManager& scroll_manager)
    : frame_(frame),
      scroll_manager_(scroll_manager),
      is_mouse_position_unknown_(true) {
  Clear();
}

void MouseEventManager::Clear() {
  element_under_mouse_ = nullptr;
  original_element_under_mouse_removed_ = false;
  mouse_press_node_ = nullptr;
  mouse_down_may_start_autoscroll_ = false;
  mouse_down_may_start_drag_ = false;
  mouse_pressed_ = false;
  click_count_ = 0;
  mousedown_element_ = nullptr;
  mouse_down_pos_ = gfx::Point();
  mouse_down_timestamp_ = base::TimeTicks();
  mouse_down_ = WebMouseEvent();
  svg_pan_ = false;
  drag_start_pos_in_root_frame_ = PhysicalOffset();
  hover_state_dirty_ = false;

  // We deliberately avoid clearing mouse position fields (last_known_mouse_*
  // and is_mouse_position_unknown_) so that we can apply hover effects in the
  // new document after a navigation.  See crbug.com/354649089.

  ResetDragSource();
  ClearDragDataTransfer();
}

MouseEventManager::~MouseEventManager() = default;

void MouseEventManager::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(scroll_manager_);
  visitor->Trace(element_under_mouse_);
  visitor->Trace(mouse_press_node_);
  visitor->Trace(mousedown_element_);
  SynchronousMutationObserver::Trace(visitor);
}

MouseEventManager::MouseEventBoundaryEventDispatcher::
    MouseEventBoundaryEventDispatcher(MouseEventManager* mouse_event_manager,
                                      const WebMouseEvent* web_mouse_event)
    : BoundaryEventDispatcher(event_type_names::kMouseover,
                              event_type_names::kMouseout,
                              event_type_names::kMouseenter,
                              event_type_names::kMouseleave),
      mouse_event_manager_(mouse_event_manager),
      web_mouse_event_(web_mouse_event) {}

void MouseEventManager::MouseEventBoundaryEventDispatcher::Dispatch(
    EventTarget* target,
    EventTarget* related_target,
    const AtomicString& type,
    bool check_for_listener) {
  mouse_event_manager_->DispatchMouseEvent(target, type, *web_mouse_event_,
                                           nullptr, related_target,
                                           check_for_listener);
}

void MouseEventManager::SendBoundaryEvents(EventTarget* exited_target,
                                           bool original_exited_target_removed,
                                           EventTarget* entered_target,
                                           const WebMouseEvent& mouse_event) {
  MouseEventBoundaryEventDispatcher boundary_event_dispatcher(this,
                                                              &mouse_event);
  boundary_event_dispatcher.SendBoundaryEvents(
      exited_target, original_exited_target_removed, entered_target);
}

WebInputEventResult MouseEventManager::DispatchMouseEvent(
    EventTarget* target,
    const AtomicString& mouse_event_type,
    const WebMouseEvent& mouse_event,
    const gfx::PointF* last_position,
    EventTarget* related_target,
    bool check_for_listener,
    const PointerId& pointer_id,
    const String& pointer_type) {
  DCHECK(mouse_event_type == event_type_names::kMouseup ||
         mouse_event_type == event_type_names::kMousedown ||
         mouse_event_type == event_type_names::kMousemove ||
         mouse_event_type == event_type_names::kMouseout ||
         mouse_event_type == event_type_names::kMouseover ||
         mouse_event_type == event_type_names::kMouseleave ||
         mouse_event_type == event_type_names::kMouseenter ||
         mouse_event_type == event_type_names::kContextmenu ||
         mouse_event_type == event_type_names::kClick ||
         mouse_event_type == event_type_names::kAuxclick);

  WebInputEventResult input_event_result = WebInputEventResult::kNotHandled;

  if (target && target->ToNode()) {
    Node* target_node = target->ToNode();
    int click_count = 0;
    if (mouse_event_type == event_type_names::kMouseup ||
        mouse_event_type == event_type_names::kMousedown ||
        mouse_event_type == event_type_names::kClick ||
        mouse_event_type == event_type_names::kAuxclick) {
      click_count = click_count_;
    }
    std::unique_ptr<EventTiming> event_timing;
    bool should_dispatch =
        !check_for_listener || target->HasEventListeners(mouse_event_type);
    if (mouse_event_type == event_type_names::kContextmenu ||
        mouse_event_type == event_type_names::kClick ||
        mouse_event_type == event_type_names::kAuxclick) {
      PointerEventInit* initializer = PointerEventInit::Create();
      SetMouseEventAttributes(initializer, target_node, mouse_event_type,
                              mouse_event, last_position, related_target,
                              click_count);
      initializer->setPointerId(pointer_id);
      initializer->setPointerType(pointer_type);
      PointerEvent* event = PointerEvent::Create(
          mouse_event_type, initializer, mouse_event.TimeStamp(),
          mouse_event.FromTouch() ? MouseEvent::kFromTouch
                                  : MouseEvent::kRealOrIndistinguishable,
          mouse_event.menu_source_type);
      if (frame_ && frame_->DomWindow())
        event_timing = EventTiming::Create(frame_->DomWindow(), *event, target);
      if (should_dispatch) {
        input_event_result = event_handling_util::ToWebInputEventResult(
            target->DispatchEvent(*event));
      }
    } else {
      MouseEventInit* initializer = MouseEventInit::Create();
      SetMouseEventAttributes(initializer, target_node, mouse_event_type,
                              mouse_event, last_position, related_target,
                              click_count);
      MouseEvent* event = MouseEvent::Create(
          mouse_event_type, initializer, mouse_event.TimeStamp(),
          mouse_event.FromTouch() ? MouseEvent::kFromTouch
                                  : MouseEvent::kRealOrIndistinguishable,
          mouse_event.menu_source_type);
      if (frame_ && frame_->DomWindow())
        event_timing = EventTiming::Create(frame_->DomWindow(), *event, target);
      if (should_dispatch) {
        input_event_result = event_handling_util::ToWebInputEventResult(
            target->DispatchEvent(*event));
      }
    }
  }

  return input_event_result;
}

// TODO(https://crbug.com/1147674): This bypasses PointerEventManager states!
// This method is called only from GestureManager, and that's one of the reasons
// PointerEvents are incomplete for touch gesture.
WebInputEventResult
MouseEventManager::SetElementUnderMouseAndDispatchMouseEvent(
    Element* target_element,
    const AtomicString& event_type,
    const WebMouseEvent& web_mouse_event) {
  // This method is used by GestureManager::HandleGestureTap to apply hover
  // states based on the tap. Note that we do not want to update the cached
  // mouse position here (using SetLastKnownMousePosition), since that would
  // cause the hover state to stick to the tap's viewport coordinates after a
  // scroll.
  //
  // TODO(crbug.com/368256331): If there IS a cached mouse position, the hover
  // state will revert to it as soon as somebody calls MarkHoverStateDirty,
  // which isn't ideal.

  SetElementUnderMouse(target_element, web_mouse_event);
  return DispatchMouseEvent(
      element_under_mouse_, event_type, web_mouse_event, nullptr, nullptr,
      false, web_mouse_event.id,
      PointerEventFactory::PointerTypeNameForWebPointPointerType(
          web_mouse_event.pointer_type));
}

namespace {

bool HasClickListenersInAncestor(Node* node) {
  for (; node; node = FlatTreeTraversal::Parent(*node)) {
    if (node->HasEventListeners(event_type_names::kClick)) {
      return true;
    }
  }
  return false;
}

}  // namespace

WebInputEventResult MouseEventManager::DispatchMouseClickIfNeeded(
    Element* mouse_release_target,
    Element* captured_click_target,
    const WebMouseEvent& mouse_event,
    const PointerId& pointer_id,
    const String& pointer_type) {
  // We only prevent click event when the click may cause contextmenu to popup.
  // However, we always send auxclick.
  bool context_menu_event = false;
#if BUILDFLAG(IS_MAC)
  // FIXME: The Mac port achieves the same behavior by checking whether the
  // context menu is currently open in WebPage::mouseEvent(). Consider merging
  // the implementations.
  if (mouse_event.button == WebPointerProperties::Button::kLeft &&
      mouse_event.GetModifiers() & WebInputEvent::Modifiers::kControlKey)
    context_menu_event = true;
#endif

  const bool should_dispatch_click_event =
      click_count_ > 0 && !context_menu_event && mousedown_element_ &&
      mouse_release_target && mousedown_element_->isConnected();
  if (!should_dispatch_click_event)
    return WebInputEventResult::kNotHandled;

  Node* click_target_node = nullptr;
  Node* common_ancestor = mouse_release_target->CommonAncestor(
      *mousedown_element_, event_handling_util::ParentForClickEvent);

  if (RuntimeEnabledFeatures::ClickToCapturedPointerEnabled() &&
      captured_click_target) {
    click_target_node = captured_click_target;
  } else if (mousedown_element_->GetDocument() ==
             mouse_release_target->GetDocument()) {
    click_target_node = common_ancestor;
  }

  if (!click_target_node)
    return WebInputEventResult::kNotHandled;

  const AtomicString click_event_type =
      (mouse_event.button == WebPointerProperties::Button::kLeft)
          ? event_type_names::kClick
          : event_type_names::kAuxclick;

  if (captured_click_target && (common_ancestor != captured_click_target) &&
      (click_event_type == event_type_names::kClick) &&
      (HasClickListenersInAncestor(common_ancestor) ||
       HasClickListenersInAncestor(captured_click_target))) {
    UseCounter::Count(frame_->GetDocument(),
                      WebFeature::kExplicitPointerCaptureClickTargetDiff);
  }

  return DispatchMouseEvent(click_target_node, click_event_type, mouse_event,
                            nullptr, nullptr, false, pointer_id, pointer_type);
}

void MouseEventManager::RecomputeMouseHoverStateIfNeeded() {
  // |RecomputeMouseHoverState| may set |hover_state_dirty_| to be true.
  if (HoverStateDirty()) {
    hover_state_dirty_ = false;
    RecomputeMouseHoverState();
  }
}

void MouseEventManager::RecomputeMouseHoverState() {
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

  // Don't dispatch a synthetic event if pointer is locked.
  if (frame_->GetPage()->GetPointerLockController().GetElement())
    return;

  WebPointerEvent::Button button = WebPointerProperties::Button::kNoButton;
  int modifiers = KeyboardEventManager::GetCurrentModifierState() |
                  WebInputEvent::kRelativeMotionEvent;
  if (mouse_pressed_) {
    button = WebPointerProperties::Button::kLeft;
    modifiers |= WebInputEvent::kLeftButtonDown;
  }
  WebMouseEvent fake_mouse_move_event(WebInputEvent::Type::kMouseMove,
                                      last_known_mouse_position_,
                                      last_known_mouse_screen_position_, button,
                                      0, modifiers, base::TimeTicks::Now());
  Vector<WebMouseEvent> coalesced_events, predicted_events;
  frame_->GetEventHandler().HandleMouseMoveEvent(
      TransformWebMouseEvent(view, fake_mouse_move_event), coalesced_events,
      predicted_events);
}

void MouseEventManager::MarkHoverStateDirty() {
  DCHECK(frame_->IsLocalRoot());
  hover_state_dirty_ = true;
}

bool MouseEventManager::HoverStateDirty() {
  DCHECK(frame_->IsLocalRoot());
  return hover_state_dirty_;
}

void MouseEventManager::SetElementUnderMouse(
    Element* target,
    const WebMouseEvent& web_mouse_event) {
  CHECK(
      !original_element_under_mouse_removed_ ||
      RuntimeEnabledFeatures::BoundaryEventDispatchTracksNodeRemovalEnabled());

  Element* last_element_under_mouse = element_under_mouse_;
  bool original_last_element_under_mouse_removed =
      original_element_under_mouse_removed_;

  element_under_mouse_ = target;
  // Clear the "removed" state for the updated `element_under_mouse_`.
  original_element_under_mouse_removed_ = false;

  // TODO(mustaq): Why do we need the `ScrollableArea` code below and not in
  // `PointerEventManager::SetElementUnderPointer()`?
  PaintLayer* layer_for_last_node =
      event_handling_util::LayerForNode(last_element_under_mouse);
  PaintLayer* layer_for_node_under_mouse =
      event_handling_util::LayerForNode(element_under_mouse_.Get());
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

  if (last_element_under_mouse &&
      last_element_under_mouse->GetDocument() != frame_->GetDocument()) {
    last_element_under_mouse = nullptr;
  }

  SendBoundaryEvents(last_element_under_mouse,
                     original_last_element_under_mouse_removed,
                     element_under_mouse_, web_mouse_event);
}

void MouseEventManager::NodeChildrenWillBeRemoved(ContainerNode& container) {
  if (RuntimeEnabledFeatures::BoundaryEventDispatchTracksNodeRemovalEnabled()) {
    return;
  }
  if (container == mousedown_element_) {
    return;
  }
  if (!mousedown_element_ ||
      !container.IsShadowIncludingInclusiveAncestorOf(*mousedown_element_)) {
    return;
  }
  mousedown_element_ = nullptr;
}

void MouseEventManager::NodeWillBeRemoved(Node& node_to_be_removed) {
  if (mousedown_element_ &&
      node_to_be_removed.IsShadowIncludingInclusiveAncestorOf(
          *mousedown_element_)) {
    // We don't dispatch click events if the mousedown node is removed
    // before a mouseup event. It is compatible with IE and Firefox.
    mousedown_element_ = nullptr;
  }
  if (mouse_press_node_ &&
      node_to_be_removed.IsShadowIncludingInclusiveAncestorOf(
          *mouse_press_node_)) {
    // If the mouse_press_node_ is removed, we should dispatch future default
    // keyboard actions (i.e. scrolling) to the still connected parent.
    mouse_press_node_ = node_to_be_removed.parentNode();
  }
  if (RuntimeEnabledFeatures::BoundaryEventDispatchTracksNodeRemovalEnabled() &&
      element_under_mouse_ &&
      node_to_be_removed.IsShadowIncludingInclusiveAncestorOf(
          *element_under_mouse_)) {
    element_under_mouse_ = node_to_be_removed.parentElement();
    original_element_under_mouse_removed_ = true;
  }
}

Element* MouseEventManager::GetElementUnderMouse() {
  return element_under_mouse_.Get();
}

WebInputEventResult MouseEventManager::HandleMouseFocus(
    const HitTestResult& hit_test_result,
    InputDeviceCapabilities* source_capabilities) {
  // If clicking on a frame scrollbar, do not mess up with content focus.
  if (auto* layout_view = frame_->ContentLayoutObject()) {
    if (hit_test_result.GetScrollbar() && frame_->ContentLayoutObject()) {
      if (hit_test_result.GetScrollbar()->GetLayoutBox() == layout_view) {
        return WebInputEventResult::kNotHandled;
      }
    }
  }

  // The layout needs to be up to date to determine if an element is focusable.
  frame_->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kFocus);

  Element* element = element_under_mouse_;

  if (!RuntimeEnabledFeatures::LabelAndDelegatesFocusNewHandlingEnabled()) {
    // When clicking on a <label> for a form associated custom element with
    // delegatesFocus, we should focus the custom element's focus delegate.
    if (auto* label = DynamicTo<HTMLLabelElement>(element)) {
      auto* control = label->Control();
      if (control && control->IsShadowHostWithDelegatesFocus()) {
        element = control;
      }
    }
  }

  for (; element; element = element->ParentOrShadowHostElement()) {
    if (element->IsMouseFocusable() && element->IsFocusedElementInDocument()) {
      return WebInputEventResult::kNotHandled;
    }
    if (element->IsMouseFocusable() ||
        element->IsShadowHostWithDelegatesFocus()) {
      break;
    }
  }
  DCHECK(!element || element->IsMouseFocusable() ||
         element->IsShadowHostWithDelegatesFocus());

  // To fix <rdar://problem/4895428> Can't drag selected ToDo, we don't focus
  // a node on mouse down if it's selected and inside a focused node. It will
  // be focused if the user does a mouseup over it, however, because the
  // mouseup will set a selection inside it, which will call
  // FrameSelection::setFocusedNodeIfNeeded.
  // TODO(editing-dev): The use of VisibleSelection should be audited. See
  // crbug.com/657237 for details.
  if (element &&
      frame_->Selection().ComputeVisibleSelectionInDOMTree().IsRange()) {
    // Don't check for scroll controls pseudo elements, since they can't
    // be in selection, until we support selecting their content.
    // Just clear the selection, since it won't be cleared otherwise.
    if (RuntimeEnabledFeatures::PseudoElementsFocusableEnabled() &&
        element->IsScrollControlPseudoElement()) {
      frame_->Selection().Clear();
    } else {
      const EphemeralRange& range = frame_->Selection()
                                        .ComputeVisibleSelectionInDOMTree()
                                        .ToNormalizedEphemeralRange();
      if (IsNodeFullyContained(range, *element) &&
          element->IsDescendantOf(frame_->GetDocument()->FocusedElement())) {
        return WebInputEventResult::kNotHandled;
      }
    }
  }

  // Only change the focus when clicking scrollbars if it can transfered to a
  // mouse focusable node.
  if (!element && hit_test_result.GetScrollbar())
    return WebInputEventResult::kHandledSystem;

  Page* const page = frame_->GetPage();
  if (!page)
    return WebInputEventResult::kNotHandled;

  // If focus shift is blocked, we eat the event. Note we should never
  // clear swallowEvent if the page already set it (e.g., by canceling
  // default behavior).
  if (element && !element->IsMouseFocusable() &&
      SlideFocusOnShadowHostIfNecessary(*element)) {
    return WebInputEventResult::kHandledSystem;
  }

  // We call setFocusedElement even with !element in order to blur
  // current focus element when a link is clicked; this is expected by
  // some sites that rely on onChange handlers running from form
  // fields before the button click is processed.
  if (!page->GetFocusController().SetFocusedElement(
          element, frame_,
          FocusParams(SelectionBehaviorOnFocus::kNone,
                      mojom::blink::FocusType::kMouse, source_capabilities)))
    return WebInputEventResult::kHandledSystem;
  return WebInputEventResult::kNotHandled;
}

bool MouseEventManager::SlideFocusOnShadowHostIfNecessary(
    const Element& element) {
  if (Element* delegated_target = element.GetFocusableArea()) {
    // Use FocusType::kMouse instead of FocusType::kForward
    // in order to prevent :focus-visible from being set
    delegated_target->Focus(FocusParams(
        SelectionBehaviorOnFocus::kReset, mojom::blink::FocusType::kMouse,
        nullptr, FocusOptions::Create(), FocusTrigger::kUserGesture));
    return true;
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
  mouse_pressed_ = true;
  SetLastKnownMousePosition(mouse_event);
  mouse_down_may_start_drag_ = false;
  mouse_down_may_start_autoscroll_ = false;
  mouse_down_timestamp_ = mouse_event.TimeStamp();

  if (LocalFrameView* view = frame_->View()) {
    mouse_down_pos_ = view->ConvertFromRootFrame(
        gfx::ToFlooredPoint(mouse_event.PositionInRootFrame()));
  } else {
    InvalidateClick();
  }

  frame_->GetEventHandler().GetSelectionController().SetMouseDownMayStartSelect(
      false);
}

bool MouseEventManager::IsMousePositionUnknown() {
  return is_mouse_position_unknown_;
}

gfx::PointF MouseEventManager::LastKnownMousePositionInViewport() {
  return last_known_mouse_position_;
}

gfx::PointF MouseEventManager::LastKnownMouseScreenPosition() {
  return last_known_mouse_screen_position_;
}

void MouseEventManager::SetLastKnownMousePosition(const WebMouseEvent& event) {
  is_mouse_position_unknown_ =
      event.GetType() == WebInputEvent::Type::kMouseLeave;
  last_known_mouse_position_in_root_frame_ =
      PhysicalOffset(gfx::ToFlooredPoint(event.PositionInRootFrame()));
  last_known_mouse_position_ = event.PositionInWidget();
  last_known_mouse_screen_position_ = event.PositionInScreen();
}

void MouseEventManager::SetLastMousePositionAsUnknown() {
  is_mouse_position_unknown_ = true;
}

WebInputEventResult MouseEventManager::HandleMousePressEvent(
    const MouseEventWithHitTestResults& event) {
  TRACE_EVENT0("blink", "MouseEventManager::handleMousePressEvent");

  ResetDragSource();

  frame_->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kInput);

  bool single_click = event.Event().click_count <= 1;

  mouse_down_may_start_drag_ = single_click && !IsSelectionOverLink(event) &&
                               !IsExtendingSelection(event);

  mouse_down_ = event.Event();

  if (frame_->GetDocument()->IsSVGDocument() &&
      frame_->GetDocument()->AccessSVGExtensions().ZoomAndPanEnabled()) {
    if ((event.Event().GetModifiers() & WebInputEvent::Modifiers::kShiftKey) &&
        single_click) {
      svg_pan_ = true;
      frame_->GetDocument()->AccessSVGExtensions().StartPan(
          frame_->View()->ConvertFromRootFrame(gfx::PointF(
              gfx::ToFlooredPoint(event.Event().PositionInRootFrame()))));
      return WebInputEventResult::kHandledSystem;
    }
  }

  // We don't do this at the start of mouse down handling,
  // because we don't want to do it until we know we didn't hit a widget.
  if (single_click)
    FocusDocumentView();

  // |SelectionController| calls |PositionForPoint()| which requires
  // |kPrePaintClean|. |FocusDocumentView| above is the last possible
  // modifications before we call |SelectionController|.
  if (LocalFrameView* frame_view = frame_->View())
    frame_view->UpdateLifecycleToPrePaintClean(DocumentUpdateReason::kInput);

  Node* inner_node = event.InnerNode();

  mouse_press_node_ = inner_node;
  frame_->GetDocument()->SetSequentialFocusNavigationStartingPoint(inner_node);
  drag_start_pos_in_root_frame_ =
      PhysicalOffset(gfx::ToFlooredPoint(event.Event().PositionInRootFrame()));

  mouse_pressed_ = true;

  bool swallow_event =
      frame_->GetEventHandler().GetSelectionController().HandleMousePressEvent(
          event);

  // TODO(crbug.com/1324667): Ensure that autoscroll handles mouse_press_node_
  // removal correctly, allowing scrolling the still attached ancestor.
  mouse_down_may_start_autoscroll_ =
      frame_->GetEventHandler()
          .GetSelectionController()
          .MouseDownMayStartSelect() ||
      (mouse_press_node_ && mouse_press_node_->GetLayoutBox() &&
       mouse_press_node_->GetLayoutBox()->IsUserScrollable());

  return swallow_event ? WebInputEventResult::kHandledSystem
                       : WebInputEventResult::kNotHandled;
}

WebInputEventResult MouseEventManager::HandleMouseReleaseEvent(
    const MouseEventWithHitTestResults& event) {
  AutoscrollController* controller = scroll_manager_->GetAutoscrollController();
  if (controller && controller->SelectionAutoscrollInProgress())
    scroll_manager_->StopAutoscroll();

  // |SelectionController| calls |PositionForPoint()| which requires
  // |kPrePaintClean|. |FocusDocumentView| above is the last possible
  // modifications before we call |SelectionController|.
  if (LocalFrameView* frame_view = frame_->View())
    frame_view->UpdateLifecycleToPrePaintClean(DocumentUpdateReason::kInput);

  return frame_->GetEventHandler()
                 .GetSelectionController()
                 .HandleMouseReleaseEvent(event, drag_start_pos_in_root_frame_)
             ? WebInputEventResult::kHandledSystem
             : WebInputEventResult::kNotHandled;
}

void MouseEventManager::UpdateSelectionForMouseDrag() {
  frame_->GetEventHandler()
      .GetSelectionController()
      .UpdateSelectionForMouseDrag(drag_start_pos_in_root_frame_,
                                   last_known_mouse_position_in_root_frame_);
}

bool MouseEventManager::HandleDragDropIfPossible(
    const GestureEventWithHitTestResults& targeted_event) {
  const WebGestureEvent& gesture_event = targeted_event.Event();
  unsigned modifiers = gesture_event.GetModifiers();

  mouse_down_ =
      WebMouseEvent(WebInputEvent::Type::kMouseDown, gesture_event,
                    WebPointerProperties::Button::kLeft, 1,
                    modifiers | WebInputEvent::Modifiers::kLeftButtonDown |
                        WebInputEvent::Modifiers::kIsCompatibilityEventForTouch,
                    base::TimeTicks::Now());

  WebMouseEvent mouse_drag_event(
      WebInputEvent::Type::kMouseMove, gesture_event,
      WebPointerProperties::Button::kLeft, 1,
      modifiers | WebInputEvent::Modifiers::kLeftButtonDown |
          WebInputEvent::Modifiers::kIsCompatibilityEventForTouch,
      base::TimeTicks::Now());
  HitTestRequest request(HitTestRequest::kReadOnly);
  MouseEventWithHitTestResults mev =
      event_handling_util::PerformMouseEventHitTest(frame_, request,
                                                    mouse_drag_event);
  mouse_down_may_start_drag_ = true;
  ResetDragSource();
  mouse_down_pos_ = frame_->View()->ConvertFromRootFrame(
      gfx::ToFlooredPoint(mouse_drag_event.PositionInRootFrame()));
  return HandleDrag(mev, DragInitiator::kTouch);
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

  // Only handles dragging for mouse left button drag and pen drag button.
  if ((!is_pen &&
       event.Event().button != WebPointerProperties::Button::kLeft) ||
      (is_pen && event.Event().button != pen_drag_button)) {
    mouse_down_may_start_drag_ = false;
    return WebInputEventResult::kNotHandled;
  }

  //  When pressing Esc key while dragging and the object is outside of the
  //  we get a mouse leave event here.
  if (!mouse_pressed_ ||
      event.Event().GetType() == WebInputEvent::Type::kMouseLeave)
    return WebInputEventResult::kNotHandled;

  // We disable the drag and drop actions on pen input on windows.
  bool should_handle_drag = true;
#if BUILDFLAG(IS_WIN)
  should_handle_drag = !is_pen;
#endif

  if (should_handle_drag && HandleDrag(event, DragInitiator::kMouse)) {
    // `HandleDrag()` returns true for both kHandledApplication and
    // kHandledSystem.  We are returning kHandledApplication here to make the
    // UseCounter in the caller work.
    return WebInputEventResult::kHandledApplication;
  }

  Node* target_node = event.InnerNode();
  if (!target_node)
    return WebInputEventResult::kNotHandled;

  LayoutObject* layout_object = target_node->GetLayoutObject();
  if (!layout_object) {
    Node* parent = FlatTreeTraversal::Parent(*target_node);
    if (!parent)
      return WebInputEventResult::kNotHandled;

    layout_object = parent->GetLayoutObject();
    if (!layout_object || !layout_object->IsListBox()) {
      return WebInputEventResult::kNotHandled;
    }
  }

  // |SelectionController| calls |PositionForPoint()| which requires
  // |kPrePaintClean|.
  if (LocalFrameView* frame_view = frame_->View())
    frame_view->UpdateLifecycleToPrePaintClean(DocumentUpdateReason::kInput);

  mouse_down_may_start_drag_ = false;

  WebInputEventResult selection_controller_drag_result =
      frame_->GetEventHandler()
          .GetSelectionController()
          .HandleMouseDraggedEvent(event, mouse_down_pos_,
                                   last_known_mouse_position_in_root_frame_);

  // The call into HandleMouseDraggedEvent may have caused a re-layout,
  // so get the LayoutObject again.
  layout_object = target_node->GetLayoutObject();

  if (layout_object && mouse_down_may_start_autoscroll_ &&
      !scroll_manager_->MiddleClickAutoscrollInProgress() &&
      !frame_->Selection().SelectedHTMLForClipboard().empty()) {
    if (AutoscrollController* controller =
            scroll_manager_->GetAutoscrollController()) {
      // Avoid updating the lifecycle unless it's possible to autoscroll.
      layout_object->GetFrameView()->UpdateAllLifecyclePhasesExceptPaint(
          DocumentUpdateReason::kScroll);

      // The lifecycle update above may have invalidated the previous layout.
      layout_object = target_node->GetLayoutObject();
      if (layout_object) {
        controller->StartAutoscrollForSelection(layout_object);
        mouse_down_may_start_autoscroll_ = false;
      }
    }
  }

  return selection_controller_drag_result;
}

// TODO(mustaq@chromium.org): The return value here is questionable.  Why even a
// failing `TryStartDrag()` below returns a `true` here?
bool MouseEventManager::HandleDrag(const MouseEventWithHitTestResults& event,
                                   DragInitiator initiator) {
  DCHECK(event.Event().GetType() == WebInputEvent::Type::kMouseMove);
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
          gfx::ToFlooredPoint(event.Event().PositionInRootFrame()))) {
    ResetDragSource();
    return true;
  }

  if (!TryStartDrag(event)) {
    // Something failed to start the drag, clean up.
    ClearDragDataTransfer();
    ResetDragSource();
  } else {
    // Once we're past the drag threshold, we don't want to treat this gesture
    // as a click.
    InvalidateClick();

    // Since drag operation started we need to send a pointercancel for the
    // corresponding pointer.
    if (initiator == DragInitiator::kMouse) {
      frame_->GetEventHandler().HandlePointerEvent(
          WebPointerEvent::CreatePointerCausesUaActionEvent(
              WebPointerProperties::PointerType::kMouse,
              event.Event().TimeStamp()),
          Vector<WebPointerEvent>(), Vector<WebPointerEvent>());
    }
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
                                                mouse_down_pos_)) {
    return false;
  }

  if (DispatchDragSrcEvent(event_type_names::kDragstart, mouse_down_) !=
      WebInputEventResult::kNotHandled) {
    return false;
  }

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
  frame_->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kInput);
  if (GetDragState().drag_type_ == kDragSourceActionSelection &&
      IsInPasswordField(
          frame_->Selection().ComputeVisibleSelectionInDOMTree().Start())) {
    return false;
  }

  // Set the clipboard access policy to protected
  // (https://html.spec.whatwg.org/multipage/dnd.html#concept-dnd-p) to
  // prevent changes in the clipboard after dragstart event has been fired:
  // https://html.spec.whatwg.org/multipage/dnd.html#dndevents
  // According to
  // https://html.spec.whatwg.org/multipage/dnd.html#dom-datatransfer-setdragimage,
  // drag image is only allowed to be changed during dragstart event.
  GetDragState().drag_data_transfer_->SetAccessPolicy(
      DataTransferAccessPolicy::kTypesReadable);

  if (drag_controller.StartDrag(frame_, GetDragState(), event.Event(),
                                mouse_down_pos_)) {
    return true;
  }

  // Drag was canned at the last minute - we owe m_dragSrc a DRAGEND event
  DispatchDragSrcEvent(event_type_names::kDragend, event.Event());

  return false;
}

// Returns if we should continue "default processing", i.e., whether
// eventhandler canceled.
WebInputEventResult MouseEventManager::DispatchDragSrcEvent(
    const AtomicString& event_type,
    const WebMouseEvent& event) {
  CHECK(event_type == event_type_names::kDrag ||
        event_type == event_type_names::kDragend ||
        event_type == event_type_names::kDragstart);

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
  // https://html.spec.whatwg.org/C/#dragevent
  // At the same time this should prevent exposing a node from another document.
  if (related_target &&
      related_target->GetDocument() != drag_target->GetDocument())
    related_target = nullptr;

  DragEventInit* initializer = DragEventInit::Create();
  initializer->setBubbles(true);
  initializer->setCancelable(event_type != event_type_names::kDragleave &&
                             event_type != event_type_names::kDragend);
  MouseEvent::SetCoordinatesFromWebPointerProperties(
      event.FlattenTransform(), frame_->GetDocument()->domWindow(),
      initializer);
  initializer->setButton(0);
  initializer->setButtons(
      MouseEvent::WebInputEventModifiersToButtons(event.GetModifiers()));
  initializer->setRelatedTarget(related_target);
  initializer->setView(frame_->GetDocument()->domWindow());
  initializer->setComposed(true);
  initializer->setGetDataTransfer(data_transfer);
  initializer->setSourceCapabilities(
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

void MouseEventManager::DragSourceEndedAt(
    const WebMouseEvent& event,
    ui::mojom::blink::DragOperation operation) {
  if (GetDragState().drag_src_) {
    GetDragState().drag_data_transfer_->SetDestinationOperation(operation);
    // The return value is ignored because dragend is not cancelable.
    DispatchDragSrcEvent(event_type_names::kDragend, event);
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
  // Check validity of drag source.
  if (!frame_->GetPage())
    return;

  Node* drag_src = GetDragState().drag_src_;
  if (!drag_src)
    return;

  Frame* drag_src_frame = drag_src->GetDocument().GetFrame();
  if (!drag_src_frame) {
    // The frame containing the drag_src has been navigated away, so the
    // drag_src is no longer has an owning frame and is invalid.
    // See https://crbug.com/903705 for more details.
    GetDragState().drag_src_ = nullptr;
    return;
  }

  // Only allow resetting drag_src_ if the frame requesting reset is above the
  // drag_src_ node's frame in the frame hierarchy. This way, unrelated frames
  // can't reset a drag state.
  if (!drag_src_frame->Tree().IsDescendantOf(frame_))
    return;

  GetDragState().drag_src_ = nullptr;
}

bool MouseEventManager::DragThresholdExceeded(
    const gfx::Point& drag_location_in_root_frame) const {
  LocalFrameView* view = frame_->View();
  if (!view)
    return false;
  gfx::Point drag_location =
      view->ConvertFromRootFrame(drag_location_in_root_frame);
  gfx::Vector2d delta = drag_location - mouse_down_pos_;

  // WebKit's drag thresholds depend on the type of object being dragged. If we
  // want to revive that behavior, we can multiply the threshold constants with
  // a number based on dragState().m_dragType.

  return abs(delta.x()) >= kDragThresholdX || abs(delta.y()) >= kDragThresholdY;
}

void MouseEventManager::ClearDragHeuristicState() {
  // Used to prevent mouseMoveEvent from initiating a drag before
  // the mouse is pressed again.
  mouse_pressed_ = false;
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
  mousedown_element_ = nullptr;
}

bool MouseEventManager::MousePressed() {
  return mouse_pressed_;
}

void MouseEventManager::ReleaseMousePress() {
  mouse_pressed_ = false;
}

Node* MouseEventManager::MousePressNode() {
  return mouse_press_node_.Get();
}

void MouseEventManager::SetMousePressNode(Node* node) {
  mouse_press_node_ = node;
}

void MouseEventManager::SetMouseDownElement(Element* element) {
  // TODO(mustaq): Why is SetDocument() not called earlier at
  // LocalFrame::DidAttachDocument()?  Because this is delayed call, the methods
  // MouseEventManager::WillBeRemoved() are not called until a mouse-press or
  // tap!
  SetDocument(element ? element->ownerDocument() : nullptr);
  mousedown_element_ = element;
}

void MouseEventManager::SetClickCount(int click_count) {
  click_count_ = click_count;
}

bool MouseEventManager::MouseDownMayStartDrag() {
  return mouse_down_may_start_drag_;
}

}  // namespace blink
