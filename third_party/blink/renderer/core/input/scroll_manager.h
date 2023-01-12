// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_SCROLL_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_SCROLL_MANAGER_H_

#include <memory>

#include "base/functional/callback_helpers.h"
#include "cc/input/snap_fling_controller.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/page/event_with_hit_test_results.h"
#include "third_party/blink/renderer/core/page/scrolling/scroll_state.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace blink {

class AutoscrollController;
class LayoutBox;
class LayoutObject;
class LocalFrame;
class PaintLayer;
class PaintLayerScrollableArea;
class Page;
class Scrollbar;
class ScrollState;
class WebGestureEvent;

// Scroll directions used to check whether propagation is possible in a given
// direction. Used in CanPropagate.
enum class ScrollPropagationDirection { kHorizontal, kVertical, kBoth, kNone };

// This class takes care of scrolling and resizing and the related states. The
// user action that causes scrolling or resizing is determined in other *Manager
// classes and they call into this class for doing the work.
class CORE_EXPORT ScrollManager : public GarbageCollected<ScrollManager>,
                                  public cc::SnapFlingClient {
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
                     Node* mouse_press_node);

  // Performs a logical scroll that chains, crossing frames, starting from
  // the given node or a reasonable default (focus/last clicked).
  bool BubblingScroll(mojom::blink::ScrollDirection,
                      ui::ScrollGranularity,
                      Node* starting_node,
                      Node* mouse_press_node);

  // TODO(crbug.com/616491): Consider moving all gesture related functions to
  // another class.

  // Handle the provided scroll gesture event, propagating down to child frames
  // as necessary.
  WebInputEventResult HandleGestureScrollEvent(const WebGestureEvent&);

  WebInputEventResult HandleGestureScrollEnd(const WebGestureEvent&);

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

  // SnapFlingClient implementation.
  bool GetSnapFlingInfoAndSetAnimatingSnapTarget(
      const gfx::Vector2dF& natural_displacement,
      gfx::PointF* out_initial_position,
      gfx::PointF* out_target_position) const override;
  gfx::PointF ScrollByForSnapFling(const gfx::Vector2dF& delta) override;
  void ScrollEndForSnapFling(bool did_finish) override;
  void RequestAnimationForSnapFling() override;

  void AnimateSnapFling(base::TimeTicks monotonic_time);

  // Determines whether the scroll-chain should be propagated upwards given a
  // scroll direction.
  static bool CanPropagate(const LayoutBox* layout_box,
                           ScrollPropagationDirection direction);
  static ScrollPropagationDirection ComputePropagationDirection(
      const ScrollState&);

 private:
  Node* NodeTargetForScrollableAreaElementId(
      CompositorElementId scrollable_area_element_id) const;
  WebInputEventResult HandleGestureScrollUpdate(const WebGestureEvent&);
  WebInputEventResult HandleGestureScrollBegin(const WebGestureEvent&);

  // Handling of GestureScrollEnd may be deferred if there's an outstanding
  // scroll animation. This is the callback that invokes the deferred operation.
  void HandleDeferredGestureScrollEnd(const WebGestureEvent& gesture_event);

  WebInputEventResult PassScrollGestureEvent(const WebGestureEvent&,
                                             LayoutObject*);

  Node* GetScrollEventTarget();

  void ClearGestureScrollState();

  void CustomizedScroll(ScrollState&);

  Page* GetPage() const;

  bool HandleScrollGestureOnResizer(Node*, const WebGestureEvent&);

  void RecomputeScrollChain(const Node& start_node,
                            const ScrollState&,
                            Deque<DOMNodeId>& scroll_chain,
                            bool is_autoscroll);
  bool CanScroll(const ScrollState&,
                 const Node& current_node,
                 bool for_autoscroll);

  uint32_t GetNonCompositedMainThreadScrollingReasons() const;
  void RecordScrollRelatedMetrics(WebGestureDevice) const;

  bool SnapAtGestureScrollEnd(const WebGestureEvent& end_event,
                              base::ScopedClosureRunner callback);

  void AdjustForSnapAtScrollUpdate(const WebGestureEvent& gesture_event,
                                   ScrollStateData* scroll_state_data);

  void NotifyScrollPhaseBeginForCustomizedScroll(const ScrollState&);
  void NotifyScrollPhaseEndForCustomizedScroll();

  LayoutBox* LayoutBoxForSnapping() const;

  // NOTE: If adding a new field to this class please ensure that it is
  // cleared in |ScrollManager::clear()|.

  const Member<LocalFrame> frame_;

  // Only used with the ScrollCustomization runtime enabled feature.
  Deque<DOMNodeId> current_scroll_chain_;

  Member<Node> scroll_gesture_handling_node_;

  bool last_gesture_scroll_over_embedded_content_view_;

  // The most recent Node to scroll natively during this scroll
  // sequence. Null if no native element has scrolled this scroll
  // sequence, or if the most recent element to scroll used scroll
  // customization.
  Member<Node> previous_gesture_scrolled_node_;

  ScrollOffset last_scroll_delta_for_scroll_gesture_;

  // True iff some of the delta has been consumed for the current
  // scroll sequence in this frame, or any child frames. Only used
  // with ScrollCustomization. If some delta has been consumed, a
  // scroll which shouldn't propagate can't cause any element to
  // scroll other than the |m_previousGestureScrolledNode|.
  bool delta_consumed_for_scroll_sequence_;

  // True iff some of the delta has been consumed for the current
  // scroll sequence on the specific axis.
  bool did_scroll_x_for_scroll_gesture_;
  bool did_scroll_y_for_scroll_gesture_;

  Member<Scrollbar> scrollbar_handling_scroll_gesture_;

  Member<PaintLayerScrollableArea> resize_scrollable_area_;

  std::unique_ptr<cc::SnapFlingController> snap_fling_controller_;

  LayoutSize
      offset_from_resize_corner_;  // In the coords of m_resizeScrollableArea.
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_SCROLL_MANAGER_H_
