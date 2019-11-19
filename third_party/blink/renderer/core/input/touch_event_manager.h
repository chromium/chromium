// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_TOUCH_EVENT_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_TOUCH_EVENT_MANAGER_H_

#include "base/macros.h"
#include "third_party/blink/public/platform/web_coalesced_input_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/platform/web_pointer_event.h"
#include "third_party/blink/public/platform/web_touch_event.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/events/pointer_event_factory.h"
#include "third_party/blink/renderer/core/input/event_handling_util.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class LocalFrame;
class Document;
class Touch;

// This class takes care of dispatching all touch events and
// maintaining related states.
class CORE_EXPORT TouchEventManager final
    : public GarbageCollected<TouchEventManager> {
 public:

  explicit TouchEventManager(LocalFrame&);
  void Trace(blink::Visitor*);

  void HandleTouchPoint(const WebPointerEvent&,
                        const Vector<WebPointerEvent>&,
                        const event_handling_util::PointerEventTarget&);

  WebInputEventResult FlushEvents();

  // Resets the internal state of this object.
  void Clear();

  // Returns whether there is any touch on the screen.
  bool IsAnyTouchActive() const;

 private:
  // Class represending one touch point event with its coalesced events and
  // related attributes.
  class TouchPointAttributes final
      : public GarbageCollected<TouchPointAttributes> {
   public:
    void Trace(blink::Visitor* visitor) { visitor->Trace(target_); }

    TouchPointAttributes() = default;
    explicit TouchPointAttributes(WebPointerEvent event)
        : event_(event), stale_(false) {}

    // Last state of the touch point.
    WebPointerEvent event_;
    // The list of coalesced events of the touch point represented by this class
    // if there is any. Note that at the end of each frame this list gets
    // cleared and the touch point |stale_| flag will be true for the next frame
    // unless more new events arrives for this touch point.
    Vector<WebPointerEvent> coalesced_events_;
    Member<Node> target_;  // The target of each active touch point.
    String region_;        //  // The region of each active touch point.
    bool stale_;
  };

  WebCoalescedInputEvent GenerateWebCoalescedInputEvent();
  Touch* CreateDomTouch(const TouchPointAttributes*, bool* known_target);
  void AllTouchesReleasedCleanup();

  // Keeps track of attributes of the touch point in the
  // |touch_points_attributes_| map and does the hit-testing if the original hit
  // test result was not inside capturing frame |touch_sequence_document_| for
  // touch events.
  void UpdateTouchAttributeMapsForPointerDown(
      const WebPointerEvent&,
      const event_handling_util::PointerEventTarget&);

  // This is triggered either by VSync signal to send one touch event per frame
  // accumulating all move events or by discrete events pointerdown/up/cancel.
  WebInputEventResult DispatchTouchEventFromAccumulatdTouchPoints();

  // Used only if |should_enforce_vertical_scroll_| is set.
  WebInputEventResult EnsureVerticalScrollIsPossible(WebInputEventResult);

  // NOTE: If adding a new field to this class please ensure that it is
  // cleared in |TouchEventManager::clear()|.

  const Member<LocalFrame> frame_;

  // The attributes of each active touch point indexed by the touch ID.
  using TouchAttributeMap =
      HeapHashMap<int,
                  Member<TouchPointAttributes>,
                  WTF::IntHash<int>,
                  WTF::UnsignedWithZeroKeyHashTraits<int>>;
  TouchAttributeMap touch_attribute_map_;

  // If set, the document of the active touch sequence. Unset if no touch
  // sequence active.
  Member<Document> touch_sequence_document_;

  bool suppressing_touchmoves_within_slop_;

  // This is used to created a consistent sequence of coalesced events compared
  // to the last frame.
  WebTouchEvent last_coalesced_touch_event_;

  // The current touch action, computed on each touch start and is
  // a union of all touches. Reset when all touches are released.
  TouchAction current_touch_action_;

  // TODO(ekaramad): Send the update after 'touchmove' to make sure we are only
  // enforcing vertical scroll.
  // When true, the current touch sequence should be handled such that vertical
  // scrolling is always possible. To this end, the output of event handlers for
  // 'touchstart' and 'touchmove' is overwritten to not handled so that
  // scrolling cannot be blocked. However, to ensure only vertical scrolling is
  // possible, the update for effective 'touch-action' is postponed to after
  // handling 'touchstart' handlers and potentially overwritten to 'pan-y' so
  // that only horizontal scrolling is blocked.
  bool should_enforce_vertical_scroll_ = false;
  // When set to a value, the effective touch-action is sent to the browser
  // after all 'touchstart' handlers have been invoked. This is used by feature
  // policy to enforce specific directions of scroll in spite of scroll-blocking
  // events being prevent defaulted. When multiple pointer down events occur
  // during the same touch sequence, the value of effective touch action which
  // is sent to the browser after handling each dispatched 'touchstart' is the
  // intersection of all the previously calculated effective touch action values
  // during the sequence.
  base::Optional<TouchAction> delayed_effective_touch_action_;

  DISALLOW_COPY_AND_ASSIGN(TouchEventManager);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_TOUCH_EVENT_MANAGER_H_
