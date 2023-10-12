// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_SCROLL_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_SCROLL_MANAGER_H_

#include "base/functional/callback_helpers.h"
#include "cc/input/snap_fling_controller.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/page/event_with_hit_test_results.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace blink {

class AutoscrollController;
class LayoutBox;
class LocalFrame;
class PaintLayer;
class PaintLayerScrollableArea;
class Scrollbar;
class WebGestureEvent;

// Scroll directions used to check whether propagation is possible in a given
// direction. Used in CanPropagate.
enum class ScrollPropagationDirection { kHorizontal, kVertical, kBoth, kNone };

// This class assists certain main-thread scroll operations such as keyboard
// scrolls and middle-click autoscroll, as well as resizer-control interactions.
// User scrolls from pointer devices (wheel/touch) are handled on the compositor
// (cc::InputHandler). For Javascript scrolls, see ProgrammaticScrollAnimator
// and the ScrollableArea APIs.
// TODO(crbug.com/1369739): Now that scroll unification has launched, much of
// this class can be deleted.
class CORE_EXPORT ScrollManager : public GarbageCollected<ScrollManager> {
 public:
  explicit ScrollManager(LocalFrame&);
  ScrollManager(const ScrollManager&) = delete;
  ScrollManager& operator=(const ScrollManager&) = delete;
  virtual ~ScrollManager() = default;
  void Trace(Visitor*) const;

  void Clear();

  bool MiddleClickAutoscrollInProgress() const;
  void StopMiddleClickAutoscroll();
  AutoscrollController* GetAutoscrollController() const;
  void StopAutoscroll();

  // Performs a chaining logical scroll, within a *single* frame, starting
  // from either a provided starting node or a default based on the focused or
  // most recently clicked node, falling back to the frame.
  // Returns true if the scroll was consumed.
  // direction - The logical direction to scroll in. This will be converted to
  //             a physical direction for each LayoutBox we try to scroll
  //             based on that box's writing mode.
  // granularity - The units that the  scroll delta parameter is in.
  // startNode - Optional. If provided, start chaining from the given node.
  //             If not, use the current focus or last clicked node.
  bool LogicalScroll(mojom::blink::ScrollDirection,
                     ui::ScrollGranularity,
                     Node* start_node,
                     Node* mouse_press_node,
                     bool scrolling_via_key = false);

  // Performs a logical scroll that chains, crossing frames, starting from
  // the given node or a reasonable default (focus/last clicked).
  bool BubblingScroll(mojom::blink::ScrollDirection,
                      ui::ScrollGranularity,
                      Node* starting_node,
                      Node* mouse_press_node,
                      bool scrolling_via_key = false);

  // TODO(crbug.com/616491): Consider moving all gesture related functions to
  // another class.

  // Handle the provided scroll gesture event, propagating down to child frames
  // as necessary.
  WebInputEventResult HandleGestureScrollEvent(const WebGestureEvent&);

  bool IsScrollbarHandlingGestures() const;

  // Returns true if the gesture event should be handled in ScrollManager.
  bool CanHandleGestureEvent(const GestureEventWithHitTestResults&);

  // These functions are related to |m_resizeScrollableArea|.
  bool InResizeMode() const;
  void Resize(const WebMouseEvent&);
  // Clears |m_resizeScrollableArea|. if |shouldNotBeNull| is true this
  // function DCHECKs to make sure that variable is indeed not null.
  void ClearResizeScrollableArea(bool should_not_be_null);
  void SetResizeScrollableArea(PaintLayer*, gfx::Point);

  // Determines whether the scroll-chain should be propagated upwards given a
  // scroll direction.
  static bool CanPropagate(const LayoutBox* layout_box,
                           ScrollPropagationDirection direction);

 private:
  Node* NodeTargetForScrollableAreaElementId(
      CompositorElementId scrollable_area_element_id) const;

  bool HandleScrollGestureOnResizer(Node*, const WebGestureEvent&);

  void RecomputeScrollChain(const Node& start_node,
                            Deque<DOMNodeId>& scroll_chain,
                            bool is_autoscroll);
  bool CanScroll(const Node& current_node, bool for_autoscroll);

  // NOTE: If adding a new field to this class please ensure that it is
  // cleared in |ScrollManager::clear()|.

  const Member<LocalFrame> frame_;

  Member<Node> scroll_gesture_handling_node_;

  Member<Scrollbar> scrollbar_handling_scroll_gesture_;

  Member<PaintLayerScrollableArea> resize_scrollable_area_;

  // In the coords of resize_scrollable_area_.
  gfx::Vector2d offset_from_resize_corner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_SCROLL_MANAGER_H_
