// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_POINTER_EVENT_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_POINTER_EVENT_MANAGER_H_

#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/events/pointer_event_factory.h"
#include "third_party/blink/renderer/core/input/boundary_event_dispatcher.h"
#include "third_party/blink/renderer/core/input/touch_event_manager.h"
#include "third_party/blink/renderer/core/page/touch_adjustment.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class LocalFrame;
class MouseEventManager;

// This class takes care of dispatching all pointer events and keeps track of
// properties of active pointer events.
class CORE_EXPORT PointerEventManager final
    : public GarbageCollected<PointerEventManager> {
 public:
  PointerEventManager(LocalFrame&, MouseEventManager&);
  PointerEventManager(const PointerEventManager&) = delete;
  PointerEventManager& operator=(const PointerEventManager&) = delete;
  void Trace(Visitor*) const;

  // This is the unified path for handling all input device events. This may
  // cause firing DOM pointerevents, mouseevent, and touch events accordingly.
  // TODO(crbug.com/625841): We need to get all event handling path to go
  // through this function.
  WebInputEventResult HandlePointerEvent(
      const WebPointerEvent&,
      const Vector<WebPointerEvent>& coalesced_events,
      const Vector<WebPointerEvent>& predicted_events);

  // Sends the mouse pointer events and the boundary events
  // that it may cause. It also sends the compat mouse events
  // and sets the newNodeUnderMouse if the capturing is set
  // in this function.
  WebInputEventResult SendMousePointerEvent(
      Element* target,
      const String& canvas_region_id,
      const WebInputEvent::Type,
      const WebMouseEvent&,
      const Vector<WebMouseEvent>& coalesced_events,
      const Vector<WebMouseEvent>& predicted_events,
      bool skip_click_dispatch);

  // Sends boundary events pointerout/leave/over/enter and
  // mouseout/leave/over/enter to the corresponding targets.
  // inside the document. This functions handles the cases that pointer is
  // leaving a frame. Note that normal mouse events (e.g. mousemove/down/up)
  // and their corresponding boundary events will be handled altogether by
  // sendMousePointerEvent function.
  void SendMouseAndPointerBoundaryEvents(Element* entered_element,
                                         const String& canvas_region_id,
                                         const WebMouseEvent&);

  WebInputEventResult DirectDispatchMousePointerEvent(
      Element* target,
      const WebMouseEvent&,
      const AtomicString& event_type,
      const Vector<WebMouseEvent>& coalesced_events,
      const Vector<WebMouseEvent>& predicted_events,
      const String& canvas_node_id = String());

  // Resets the internal state of this object.
  void Clear();

  void ElementRemoved(Element*);

  bool SetPointerCapture(PointerId, Element*);
  bool ReleasePointerCapture(PointerId, Element*);
  void ReleaseMousePointerCapture();

  // See Element::hasPointerCapture(PointerId).
  bool HasPointerCapture(PointerId, const Element*) const;

  bool IsActive(const PointerId) const;

  // Returns whether there is any touch on the screen.
  bool IsAnyTouchActive() const;

  // Returns whether pointerId is for an active touch pointerevent and whether
  // the last event was sent to the given frame.
  bool IsPointerIdActiveOnFrame(PointerId, LocalFrame*) const;

  // Returns true if the primary pointerdown corresponding to the given
  // |uniqueTouchEventId| was canceled. Also drops stale ids from
  // |m_touchIdsForCanceledPointerdowns|.
  bool PrimaryPointerdownCanceled(uint32_t unique_touch_event_id);

  void RemoveLastMousePosition();

  Element* GetMouseCaptureTarget();

  // Sends any outstanding events. For example it notifies TouchEventManager
  // to group any changes to touch since last FlushEvents and send the touch
  // event out to js. Since after this function any outstanding event is sent,
  // it also clears any state that might have kept since the last call to this
  // function.
  WebInputEventResult FlushEvents();

 private:
  class EventTargetAttributes : public GarbageCollected<EventTargetAttributes> {
   public:
    void Trace(Visitor* visitor) const { visitor->Trace(target); }
    Member<Element> target;
    EventTargetAttributes() : target(nullptr) {}
    EventTargetAttributes(Element* target) : target(target) {}
  };
  // We use int64_t to cover the whole range for PointerId with no
  // deleted hash value.
  template <typename T>
  using PointerIdKeyMap =
      HeapHashMap<int64_t,
                  T,
                  WTF::IntHash<int64_t>,
                  WTF::UnsignedWithZeroKeyHashTraits<int64_t>>;
  using PointerCapturingMap = PointerIdKeyMap<Member<Element>>;
  using ElementUnderPointerMap = PointerIdKeyMap<Member<EventTargetAttributes>>;

  class PointerEventBoundaryEventDispatcher : public BoundaryEventDispatcher {
   public:
    PointerEventBoundaryEventDispatcher(PointerEventManager*, PointerEvent*);
    PointerEventBoundaryEventDispatcher(
        const PointerEventBoundaryEventDispatcher&) = delete;
    PointerEventBoundaryEventDispatcher& operator=(
        const PointerEventBoundaryEventDispatcher&) = delete;

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
                  bool check_for_listener);
    PointerEventManager* pointer_event_manager_;
    PointerEvent* pointer_event_;
  };

  // Sends pointercancels for existing PointerEvents that are interrupted.
  // For example when browser starts dragging with mouse or when we start
  // scrolling with scroll capable pointers pointercancel events should be
  // dispatched for those. Also sets initial states accordingly so the
  // following events in that stream don't generate pointerevents (e.g.
  // in the scrolling case which scroll starts and pointerevents stop and
  // touchevents continue to fire).
  void HandlePointerInterruption(const WebPointerEvent&);

  WebInputEventResult CreateAndDispatchPointerEvent(
      Element* target,
      const AtomicString& mouse_event_name,
      const WebMouseEvent&,
      const Vector<WebMouseEvent>& coalesced_events,
      const Vector<WebMouseEvent>& predicted_events,
      const String& canvas_region_id);

  // Returns PointerEventTarget for a WebTouchPoint, hit-testing as necessary.
  event_handling_util::PointerEventTarget ComputePointerEventTarget(
      const WebPointerEvent&);

  WebInputEventResult DispatchTouchPointerEvent(
      const WebPointerEvent&,
      const Vector<WebPointerEvent>& coalesced_events,
      const Vector<WebPointerEvent>& predicted_events,
      const event_handling_util::PointerEventTarget&);

  // Returns whether the event is consumed or not.
  WebInputEventResult SendTouchPointerEvent(Element*,
                                            PointerEvent*,
                                            bool hovering);

  void SendBoundaryEvents(EventTarget* exited_target,
                          EventTarget* entered_target,
                          PointerEvent*);
  void SetElementUnderPointer(PointerEvent*, Element*);

  // First movement after entering a new frame should be 0 as the new frame
  // doesn't have the info for the previous events. This function sets the
  // LastPosition to be same as current event position when target is in
  // different frame, so that movement_x/y will be 0.
  void SetLastPointerPositionForFrameBoundary(const WebPointerEvent& event,
                                              Element* target);

  // Processes the assignment of |m_pointerCaptureTarget| from
  // |m_pendingPointerCaptureTarget| and sends the got/lostpointercapture
  // events, as per the spec:
  // https://w3c.github.io/pointerevents/#process-pending-pointer-capture
  void ProcessPendingPointerCapture(PointerEvent*);

  // Processes the capture state of a pointer, updates element under
  // pointer, and sends corresponding boundary events for pointer if
  // setPointerPosition is true. It also sends corresponding boundary events
  // for mouse if sendMouseEvent is true.
  // Returns the target that the pointer event is supposed to be fired at.
  Element* ProcessCaptureAndPositionOfPointerEvent(
      PointerEvent*,
      Element* hit_test_target,
      const String& canvas_region_id = String(),
      const WebMouseEvent* = nullptr);

  void RemoveTargetFromPointerCapturingMapping(PointerCapturingMap&,
                                               const Element*);
  Element* GetEffectiveTargetForPointerEvent(Element*, PointerId);
  Element* GetCapturingElement(PointerId);
  void RemovePointer(PointerEvent*);
  WebInputEventResult DispatchPointerEvent(EventTarget*,
                                           PointerEvent*,
                                           bool check_for_listener = false);
  void ReleasePointerCapture(PointerId);
  // Returns true if capture target and pending capture target were different.
  bool GetPointerCaptureState(PointerId pointer_id,
                              Element** pointer_capture_target,
                              Element** pending_pointer_capture_target);

  // Only adjust touch type primary pointer down.
  bool ShouldAdjustPointerEvent(const WebPointerEvent&) const;
  // Adjust coordinates so it can be used to find the best clickable target.
  void AdjustTouchPointerEvent(WebPointerEvent&);

  // Check if the SkipTouchEventFilter experiment is configured to skip
  // filtering on the given event.
  bool ShouldFilterEvent(PointerEvent* pointer_event);

  // NOTE: If adding a new field to this class please ensure that it is
  // cleared in |PointerEventManager::clear()|.

  const Member<LocalFrame> frame_;

  // Prevents firing mousedown, mousemove & mouseup in-between a canceled
  // pointerdown and next pointerup/pointercancel.
  // See "PREVENT MOUSE EVENT flag" in the spec:
  //   https://w3c.github.io/pointerevents/#compatibility-mapping-with-mouse-events
  bool prevent_mouse_event_for_pointer_type_
      [static_cast<size_t>(WebPointerProperties::PointerType::kMaxValue) + 1];

  // Set upon scrolling starts when sending a pointercancel, prevents PE
  // dispatches for non-hovering pointers until all of them become inactive.
  bool non_hovering_pointers_canceled_;

  Deque<uint32_t> touch_ids_for_canceled_pointerdowns_;

  // Note that this map keeps track of element under pointer with id=1 as well
  // which might be different than m_nodeUnderMouse in EventHandler. That one
  // keeps track of any compatibility mouse event positions but this map for
  // the pointer with id=1 is only taking care of true mouse related events.
  ElementUnderPointerMap element_under_pointer_;

  PointerCapturingMap pointer_capture_target_;
  PointerCapturingMap pending_pointer_capture_target_;

  PointerEventFactory pointer_event_factory_;
  Member<TouchEventManager> touch_event_manager_;
  Member<MouseEventManager> mouse_event_manager_;

  // The pointerId of the PointerEvent currently being dispatched within this
  // frame or 0 if none.
  PointerId dispatching_pointer_id_;

  // These flags are set for the SkipTouchEventFilter experiment. The
  // experiment either skips filtering discrete (touch start/end) events to the
  // main thread, or all events (touch start/end/move).
  bool skip_touch_filter_discrete_ = false;
  bool skip_touch_filter_all_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_POINTER_EVENT_MANAGER_H_
