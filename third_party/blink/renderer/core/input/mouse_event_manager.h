// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_MOUSE_EVENT_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_MOUSE_EVENT_MANAGER_H_

#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/synchronous_mutation_observer.h"
#include "third_party/blink/renderer/core/input/boundary_event_dispatcher.h"
#include "third_party/blink/renderer/core/page/drag_actions.h"
#include "third_party/blink/renderer/core/page/event_with_hit_test_results.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ContainerNode;
class DragState;
class DataTransfer;
class Element;
class HitTestResult;
class InputDeviceCapabilities;
class LocalFrame;
class ScrollManager;

enum class DragInitiator;

// This class takes care of dispatching all mouse events and keeps track of
// positions and states of mouse.
class CORE_EXPORT MouseEventManager final
    : public GarbageCollected<MouseEventManager>,
      public SynchronousMutationObserver {
 public:
  MouseEventManager(LocalFrame&, ScrollManager&);
  MouseEventManager(const MouseEventManager&) = delete;
  MouseEventManager& operator=(const MouseEventManager&) = delete;
  virtual ~MouseEventManager();
  void Trace(Visitor*) const override;

  WebInputEventResult DispatchMouseEvent(EventTarget*,
                                         const AtomicString&,
                                         const WebMouseEvent&,
                                         const String& canvas_region_id,
                                         const FloatPoint* last_position,
                                         EventTarget* related_target,
                                         bool check_for_listener = false,
                                         const PointerId& pointer_id = 0,
                                         const String& pointer_type = "");

  WebInputEventResult SetMousePositionAndDispatchMouseEvent(
      Element* target_element,
      const String& canvas_region_id,
      const AtomicString& event_type,
      const WebMouseEvent&);

  WebInputEventResult DispatchMouseClickIfNeeded(
      Element* mouse_release_target,
      const WebMouseEvent& mouse_event,
      const String& canvas_region_id,
      const PointerId& pointer_id,
      const String& pointer_type);

  WebInputEventResult DispatchDragSrcEvent(const AtomicString& event_type,
                                           const WebMouseEvent&);
  WebInputEventResult DispatchDragEvent(const AtomicString& event_type,
                                        Node* target,
                                        Node* related_target,
                                        const WebMouseEvent&,
                                        DataTransfer*);

  // Resets the internal state of this object.
  void Clear();

  void SendBoundaryEvents(EventTarget* exited_target,
                          EventTarget* entered_target,
                          const String& canvas_region_id,
                          const WebMouseEvent&);

  void SetElementUnderMouse(Element*,
                            const String& canvas_region_id,
                            const WebMouseEvent&);

  WebInputEventResult HandleMouseFocus(
      const HitTestResult&,
      InputDeviceCapabilities* source_capabilities);

  void SetLastKnownMousePosition(const WebMouseEvent&);
  void SetLastMousePositionAsUnknown();

  bool HandleDragDropIfPossible(const GestureEventWithHitTestResults&);

  WebInputEventResult HandleMouseDraggedEvent(
      const MouseEventWithHitTestResults&);
  WebInputEventResult HandleMousePressEvent(
      const MouseEventWithHitTestResults&);
  WebInputEventResult HandleMouseReleaseEvent(
      const MouseEventWithHitTestResults&);

  DragState& GetDragState();

  void FocusDocumentView();

  // Resets the state that indicates the next events could cause a drag. It is
  // called when we realize the next events should not cause drag based on the
  // drag heuristics.
  void ClearDragHeuristicState();

  void DragSourceEndedAt(const WebMouseEvent&, DragOperation);

  void UpdateSelectionForMouseDrag();

  void HandleMousePressEventUpdateStates(const WebMouseEvent&);
  void HandleMouseReleaseEventUpdateStates();

  // Returns whether pan is handled and resets the state on release.
  bool HandleSvgPanIfNeeded(bool is_release_event);

  void InvalidateClick();

  // TODO: These functions ideally should be private but the code needs more
  // refactoring to be able to remove the dependency from EventHandler.
  Element* GetElementUnderMouse();
  bool IsMousePositionUnknown();
  FloatPoint LastKnownMousePositionInViewport();
  FloatPoint LastKnownMouseScreenPosition();

  bool MousePressed();
  void ReleaseMousePress();

  bool CapturesDragging() const;
  void SetCapturesDragging(bool);

  void SetMouseDownMayStartAutoscroll() {
    mouse_down_may_start_autoscroll_ = true;
  }

  Node* MousePressNode();
  void SetMousePressNode(Node*);

  void SetClickElement(Element*);
  void SetClickCount(int);

  bool MouseDownMayStartDrag();

  void RecomputeMouseHoverStateIfNeeded();
  void RecomputeMouseHoverState();

  void MarkHoverStateDirty();

 private:
  class MouseEventBoundaryEventDispatcher : public BoundaryEventDispatcher {
   public:
    MouseEventBoundaryEventDispatcher(MouseEventManager*,
                                      const WebMouseEvent*,
                                      EventTarget* exited_target,
                                      const String& canvas_region_id);
    MouseEventBoundaryEventDispatcher(
        const MouseEventBoundaryEventDispatcher&) = delete;
    MouseEventBoundaryEventDispatcher& operator=(
        const MouseEventBoundaryEventDispatcher&) = delete;

   protected:
    void DispatchOut(EventTarget*, EventTarget* related_target) override;
    void DispatchOver(EventTarget*, EventTarget* related_target) override;
    void DispatchLeave(EventTarget*,
                       EventTarget* related_target,
                       bool check_for_listener) override;
    void DispatchEnter(EventTarget*,
                       EventTarget* related_target,
                       bool check_for_listener) override;
    AtomicString GetLeaveEvent() override;
    AtomicString GetEnterEvent() override;

   private:
    void Dispatch(EventTarget*,
                  EventTarget* related_target,
                  const AtomicString&,
                  const String& canvas_region_id,
                  const WebMouseEvent&,
                  bool check_for_listener);
    MouseEventManager* mouse_event_manager_;
    const WebMouseEvent* web_mouse_event_;
    EventTarget* exited_target_;
    String canvas_region_id_;
  };

  // If the given element is a shadow host and its root has delegatesFocus=false
  // flag, slide focus to its inner element. Returns true if the resulting focus
  // is different from the given element.
  bool SlideFocusOnShadowHostIfNecessary(const Element&);

  bool DragThresholdExceeded(const IntPoint&) const;
  bool HandleDrag(const MouseEventWithHitTestResults&, DragInitiator);
  bool TryStartDrag(const MouseEventWithHitTestResults&);
  void ClearDragDataTransfer();
  DataTransfer* CreateDraggingDataTransfer() const;

  void ResetDragSource();
  bool HoverStateDirty();

  // Implementations of |SynchronousMutationObserver|
  void NodeChildrenWillBeRemoved(ContainerNode&) final;
  void NodeWillBeRemoved(Node& node_to_be_removed) final;

  // NOTE: If adding a new field to this class please ensure that it is
  // cleared in |MouseEventManager::clear()|.

  const Member<LocalFrame> frame_;
  Member<ScrollManager> scroll_manager_;

  // The effective position of the mouse pointer.
  // See
  // https://w3c.github.io/pointerevents/#dfn-tracking-the-effective-position-of-the-legacy-mouse-pointer.
  Member<Element> element_under_mouse_;

  // The last mouse movement position this frame has seen in viewport
  // coordinates.
  FloatPoint last_known_mouse_position_;
  FloatPoint last_known_mouse_screen_position_;

  unsigned is_mouse_position_unknown_ : 1;
  // Current button-press state for mouse/mouse-like-stylus.
  // TODO(crbug.com/563676): Buggy for chorded buttons.
  unsigned mouse_pressed_ : 1;

  unsigned mouse_down_may_start_autoscroll_ : 1;
  unsigned svg_pan_ : 1;
  unsigned captures_dragging_ : 1;
  unsigned mouse_down_may_start_drag_ : 1;

  Member<Node> mouse_press_node_;

  int click_count_;
  Member<Element> click_element_;
  // This element should be mostly the same as click_element_. Only when
  // click_element_ is set to null due to DOM manipulation mouse_down_element_
  // remains unchanged.
  Member<Element> mouse_down_element_;

  IntPoint mouse_down_pos_;  // In our view's coords.
  base::TimeTicks mouse_down_timestamp_;
  WebMouseEvent mouse_down_;

  PhysicalOffset drag_start_pos_;
  // This indicates that whether we should update the hover at each begin
  // frame. This is set to be true after the compositor or main thread scroll
  // ends, and at each begin frame, we will dispatch a fake mouse move event to
  // update hover when this is true.
  bool hover_state_dirty_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_MOUSE_EVENT_MANAGER_H_
