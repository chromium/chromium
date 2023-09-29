// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/scroll_manager.h"

#include <utility>

#include "cc/base/features.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/input/snap_selection_strategy.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/events/gesture_event.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/event_handling_util.h"
#include "third_party/blink/renderer/core/input/keyboard_event_manager.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/overscroll_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/scroll_state.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/scroll_customization.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/widget/input/input_metrics.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace blink {

ScrollManager::ScrollManager(LocalFrame& frame) : frame_(frame) {
  Clear();
}

void ScrollManager::Clear() {
  scrollbar_handling_scroll_gesture_ = nullptr;
  resize_scrollable_area_ = nullptr;
  offset_from_resize_corner_ = {};
  ClearGestureScrollState();
}

void ScrollManager::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(scroll_gesture_handling_node_);
  visitor->Trace(scrollbar_handling_scroll_gesture_);
  visitor->Trace(resize_scrollable_area_);
}

void ScrollManager::ClearGestureScrollState() {
  scroll_gesture_handling_node_ = nullptr;

  if (Page* page = frame_->GetPage()) {
    bool reset_x = true;
    bool reset_y = true;
    page->GetOverscrollController().ResetAccumulated(reset_x, reset_y);
  }
}

void ScrollManager::StopAutoscroll() {
  if (AutoscrollController* controller = GetAutoscrollController())
    controller->StopAutoscroll();
}

void ScrollManager::StopMiddleClickAutoscroll() {
  if (AutoscrollController* controller = GetAutoscrollController())
    controller->StopMiddleClickAutoscroll(frame_);
}

bool ScrollManager::MiddleClickAutoscrollInProgress() const {
  return GetAutoscrollController() &&
         GetAutoscrollController()->MiddleClickAutoscrollInProgress();
}

AutoscrollController* ScrollManager::GetAutoscrollController() const {
  if (Page* page = frame_->GetPage())
    return &page->GetAutoscrollController();
  return nullptr;
}

ScrollPropagationDirection ScrollManager::ComputePropagationDirection(
    const ScrollState& scroll_state) {
  if (scroll_state.deltaXHint() == 0 && scroll_state.deltaYHint() != 0)
    return ScrollPropagationDirection::kVertical;
  if (scroll_state.deltaXHint() != 0 && scroll_state.deltaYHint() == 0)
    return ScrollPropagationDirection::kHorizontal;
  if (scroll_state.deltaXHint() != 0 && scroll_state.deltaYHint() != 0)
    return ScrollPropagationDirection::kBoth;
  return ScrollPropagationDirection::kNone;
}

bool ScrollManager::CanPropagate(const LayoutBox* layout_box,
                                 ScrollPropagationDirection direction) {
  ScrollableArea* scrollable_area = layout_box->GetScrollableArea();
  if (!scrollable_area)
    return true;

  if (!scrollable_area->UserInputScrollable(kHorizontalScrollbar) &&
      !scrollable_area->UserInputScrollable(kVerticalScrollbar))
    return true;

  switch (direction) {
    case ScrollPropagationDirection::kBoth:
      return ((layout_box->StyleRef().OverscrollBehaviorX() ==
               EOverscrollBehavior::kAuto) &&
              (layout_box->StyleRef().OverscrollBehaviorY() ==
               EOverscrollBehavior::kAuto));
    case ScrollPropagationDirection::kVertical:
      return layout_box->StyleRef().OverscrollBehaviorY() ==
             EOverscrollBehavior::kAuto;
    case ScrollPropagationDirection::kHorizontal:
      return layout_box->StyleRef().OverscrollBehaviorX() ==
             EOverscrollBehavior::kAuto;
    case ScrollPropagationDirection::kNone:
      return true;
    default:
      NOTREACHED();
  }
}

void ScrollManager::RecomputeScrollChain(const Node& start_node,
                                         const ScrollState& scroll_state,
                                         Deque<DOMNodeId>& scroll_chain,
                                         bool is_autoscroll) {
  DCHECK(scroll_chain.empty());
  scroll_chain.clear();

  DCHECK(start_node.GetLayoutObject());

  if (is_autoscroll) {
    // Propagate the autoscroll along the layout object chain, and
    // append only the first node which is able to consume the scroll delta.
    // The scroll node is computed differently to regular scrolls in order to
    // maintain consistency with the autoscroll controller.
    LayoutBox* autoscrollable = LayoutBox::FindAutoscrollable(
        start_node.GetLayoutObject(), is_autoscroll);
    if (autoscrollable) {
      Node* cur_node = autoscrollable->GetNode();
      LayoutObject* layout_object = cur_node->GetLayoutObject();
      while (layout_object &&
             !CanScroll(scroll_state, *cur_node, is_autoscroll)) {
        ScrollPropagationDirection direction =
            ComputePropagationDirection(scroll_state);

        if (!CanPropagate(cur_node->GetLayoutBox(), direction))
          break;

        if (!layout_object->Parent() &&
            layout_object->GetNode() == layout_object->GetDocument() &&
            layout_object->GetDocument().LocalOwner()) {
          layout_object =
              layout_object->GetDocument().LocalOwner()->GetLayoutObject();
        } else {
          layout_object = layout_object->Parent();
        }
        LayoutBox* new_autoscrollable =
            LayoutBox::FindAutoscrollable(layout_object, is_autoscroll);
        if (new_autoscrollable)
          cur_node = new_autoscrollable->GetNode();
      }
      scroll_chain.push_front(cur_node->GetDomNodeId());
    }
  } else {
    LayoutBox* cur_box = start_node.GetLayoutObject()->EnclosingBox();

    // Scrolling propagates along the containing block chain and ends at the
    // RootScroller node. The RootScroller node will have a custom applyScroll
    // callback that performs scrolling as well as associated "root" actions
    // like browser control movement and overscroll glow.
    while (cur_box) {
      Node* cur_node = cur_box->GetNode();

      if (cur_node) {
        if (CanScroll(scroll_state, *cur_node, /* for_autoscroll */ false))
          scroll_chain.push_front(cur_node->GetDomNodeId());

        if (cur_node->IsEffectiveRootScroller())
          break;

        ScrollPropagationDirection direction =
            ComputePropagationDirection(scroll_state);
        if (!CanPropagate(cur_node->GetLayoutBox(), direction)) {
          // We should add the first node with non-auto overscroll-behavior to
          // the scroll chain regardlessly, as it's the only node we can latch
          // to.
          if (scroll_chain.empty() ||
              scroll_chain.front() != cur_node->GetDomNodeId()) {
            scroll_chain.push_front(cur_node->GetDomNodeId());
          }
          break;
        }
      }

      cur_box = cur_box->ContainingBlock();
    }
  }
}

bool ScrollManager::CanScroll(const ScrollState& scroll_state,
                              const Node& current_node,
                              bool for_autoscroll) {
  LayoutBox* scrolling_box = current_node.GetLayoutBox();
  if (auto* element = DynamicTo<Element>(current_node))
    scrolling_box = element->GetLayoutBoxForScrolling();
  if (!scrolling_box)
    return false;

  // We need to always add the global root scroller even if it isn't scrollable
  // since we can always pinch-zoom and scroll as well as for overscroll
  // effects. If autoscrolling, ignore this condition because we latch on
  // to the deepest autoscrollable node.
  if (scrolling_box->IsGlobalRootScroller() && !for_autoscroll)
    return true;

  // If this is the main LayoutView of an active viewport (outermost main
  // frame, portal), and it's not the root scroller, that means we have a
  // non-default root scroller on the page.  In this case, attempts to scroll
  // the LayoutView should cause panning of the visual viewport as well so
  // ensure it gets added to the scroll chain.  See LTHI::ApplyScroll for the
  // equivalent behavior in CC.  Node::NativeApplyScroll contains a special
  // handler for this case. If autoscrolling, ignore this condition because we
  // latch on to the deepest autoscrollable node.
  if (IsA<LayoutView>(scrolling_box) &&
      current_node.GetDocument().IsInMainFrame() &&
      frame_->GetPage()->GetVisualViewport().IsActiveViewport() &&
      !for_autoscroll) {
    return true;
  }

  ScrollableArea* scrollable_area = scrolling_box->GetScrollableArea();

  if (!scrollable_area)
    return false;

  double delta_x = scroll_state.isBeginning() ? scroll_state.deltaXHint()
                                              : scroll_state.deltaX();
  double delta_y = scroll_state.isBeginning() ? scroll_state.deltaYHint()
                                              : scroll_state.deltaY();
  if (!delta_x && !delta_y)
    return true;

  if (!scrollable_area->UserInputScrollable(kHorizontalScrollbar))
    delta_x = 0;
  if (!scrollable_area->UserInputScrollable(kVerticalScrollbar))
    delta_y = 0;

  if (scroll_state.deltaGranularity() ==
      static_cast<double>(ui::ScrollGranularity::kScrollByPercentage)) {
    delta_x *= scrollable_area->ScrollStep(
        ui::ScrollGranularity::kScrollByPercentage, kHorizontalScrollbar);
    delta_y *= scrollable_area->ScrollStep(
        ui::ScrollGranularity::kScrollByPercentage, kVerticalScrollbar);
  }

  ScrollOffset current_offset = scrollable_area->GetScrollOffset();
  ScrollOffset target_offset = current_offset + ScrollOffset(delta_x, delta_y);
  ScrollOffset clamped_offset =
      scrollable_area->ClampScrollOffset(target_offset);
  return clamped_offset != current_offset;
}

bool ScrollManager::LogicalScroll(mojom::blink::ScrollDirection direction,
                                  ui::ScrollGranularity granularity,
                                  Node* start_node,
                                  Node* mouse_press_node,
                                  bool scrolling_via_key) {
  Node* node = start_node;

  if (!node)
    node = frame_->GetDocument()->FocusedElement();

  if (!node)
    node = mouse_press_node;

  if ((!node || !node->GetLayoutObject()) && frame_->View() &&
      frame_->View()->GetLayoutView())
    node = frame_->View()->GetLayoutView()->GetNode();

  if (!node)
    return false;

  Document& document = node->GetDocument();

  document.UpdateStyleAndLayout(DocumentUpdateReason::kScroll);

  Deque<DOMNodeId> scroll_chain;
  std::unique_ptr<ScrollStateData> scroll_state_data =
      std::make_unique<ScrollStateData>();
  auto* scroll_state =
      MakeGarbageCollected<ScrollState>(std::move(scroll_state_data));
  RecomputeScrollChain(*node, *scroll_state, scroll_chain,
                       /* is_autoscroll */ false);

  while (!scroll_chain.empty()) {
    Node* scroll_chain_node = DOMNodeIds::NodeForId(scroll_chain.TakeLast());
    DCHECK(scroll_chain_node);

    auto* box = To<LayoutBox>(scroll_chain_node->GetLayoutObject());
    DCHECK(box);

    ScrollDirectionPhysical physical_direction =
        ToPhysicalDirection(direction, box->IsHorizontalWritingMode(),
                            box->Style()->IsFlippedBlocksWritingMode());

    ScrollableArea* scrollable_area = ScrollableArea::GetForScrolling(box);
    DCHECK(scrollable_area);

    ScrollOffset delta =
        ToScrollDelta(physical_direction,
                      ScrollableArea::DirectionBasedScrollDelta(granularity));
    delta.Scale(scrollable_area->ScrollStep(granularity, kHorizontalScrollbar),
                scrollable_area->ScrollStep(granularity, kVerticalScrollbar));
    // Pressing the arrow key is considered as a scroll with intended direction
    // only (this results in kScrollByLine or kScrollByPercentage, depending on
    // REF::PercentBasedScrollingEnabled). Pressing the PgUp/PgDn key is
    // considered as a scroll with intended direction and end position. Pressing
    // the Home/End key is considered as a scroll with intended end position
    // only.
    switch (granularity) {
      case ui::ScrollGranularity::kScrollByLine:
      case ui::ScrollGranularity::kScrollByPercentage: {
        if (scrollable_area->SnapForDirection(delta))
          return true;
        break;
      }
      case ui::ScrollGranularity::kScrollByPage: {
        if (scrollable_area->SnapForEndAndDirection(delta))
          return true;
        break;
      }
      case ui::ScrollGranularity::kScrollByDocument: {
        gfx::PointF end_position = scrollable_area->ScrollPosition() + delta;
        bool scrolled_x = physical_direction == kScrollLeft ||
                          physical_direction == kScrollRight;
        bool scrolled_y = physical_direction == kScrollUp ||
                          physical_direction == kScrollDown;
        if (scrollable_area->SnapForEndPosition(end_position, scrolled_x,
                                                scrolled_y))
          return true;
        break;
      }
      default:
        NOTREACHED();
    }

    ScrollableArea::ScrollCallback callback(WTF::BindOnce(
        [](WeakPersistent<ScrollableArea> area,
           WeakPersistent<KeyboardEventManager> keyboard_event_manager,
           bool is_key_scroll,
           ScrollableArea::ScrollCompletionMode completion_mode) {
          if (area) {
            bool enqueue_scrollend =
                completion_mode ==
                ScrollableArea::ScrollCompletionMode::kFinished;

            // Viewport scrolls should only fire scrollend if the
            // LayoutViewport was scrolled.
            if (enqueue_scrollend && IsA<RootFrameViewport>(area.Get())) {
              auto* root_frame_viewport = To<RootFrameViewport>(area.Get());
              if (!root_frame_viewport->ScrollAffectsLayoutViewport()) {
                enqueue_scrollend = false;
              }
            }

            // For key-triggered scrolls, we defer firing scrollend till the
            // accompanying keyup fires, unless the keyup happens before the
            // scroll finishes. (Instant scrolls always finish before the
            // keyup event.)
            if (is_key_scroll && enqueue_scrollend && keyboard_event_manager) {
              if (keyboard_event_manager->HasPendingScrollendOnKeyUp() ||
                  !area->ScrollAnimatorEnabled()) {
                keyboard_event_manager->SetScrollendEventTarget(area);
                enqueue_scrollend = false;
              }
            }
            area->OnScrollFinished(enqueue_scrollend);
          }
        },
        WrapWeakPersistent(scrollable_area),
        WrapWeakPersistent(
            &(frame_->GetEventHandler().GetKeyboardEventManager())),
        scrolling_via_key));
    ScrollResult result = scrollable_area->UserScroll(
        granularity,
        ToScrollDelta(physical_direction,
                      ScrollableArea::DirectionBasedScrollDelta(granularity)),
        std::move(callback));

    if (result.DidScroll())
      return true;
  }

  return false;
}

bool ScrollManager::BubblingScroll(mojom::blink::ScrollDirection direction,
                                   ui::ScrollGranularity granularity,
                                   Node* starting_node,
                                   Node* mouse_press_node,
                                   bool scrolling_via_key) {
  // The layout needs to be up to date to determine if we can scroll. We may be
  // here because of an onLoad event, in which case the final layout hasn't been
  // performed yet.
  frame_->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kScroll);
  if (LogicalScroll(direction, granularity, starting_node, mouse_press_node,
                    scrolling_via_key)) {
    return true;
  }

  return frame_->BubbleLogicalScrollInParentFrame(direction, granularity);
}

ScrollOffset GetScrollDirection(ScrollOffset delta) {
  delta.SetToMax(ScrollOffset(-1, -1));
  delta.SetToMin(ScrollOffset(1, 1));
  return delta;
}

WebInputEventResult ScrollManager::HandleGestureScrollEvent(
    const WebGestureEvent& gesture_event) {
  // TODO(crbug.com/1369739): This method is probably dead code.
  if (!frame_->View())
    return WebInputEventResult::kNotHandled;

  TRACE_EVENT0("input", "ScrollManager::handleGestureScrollEvent");

  Node* event_target = nullptr;
  Scrollbar* scrollbar = nullptr;
  if (gesture_event.GetType() != WebInputEvent::Type::kGestureScrollBegin) {
    scrollbar = scrollbar_handling_scroll_gesture_.Get();
    event_target = scroll_gesture_handling_node_.Get();
  } else if (gesture_event.GetType() ==
                 WebInputEvent::Type::kGestureScrollBegin &&
             gesture_event.data.scroll_begin.scrollable_area_element_id) {
    CompositorElementId element_id = CompositorElementId(
        gesture_event.data.scroll_begin.scrollable_area_element_id);
    event_target = NodeTargetForScrollableAreaElementId(element_id);
    if (!event_target) {
      // If we couldn't find a node associated with the targeted scrollable
      // area, just drop the gesture. This may be due to the fact that the
      // targeted has been removed from the tree between when the gesture
      // was queued and when we handle it.
      return WebInputEventResult::kNotHandled;
    }

    ClearGestureScrollState();
    scroll_gesture_handling_node_ = event_target;
  }

  if (!event_target) {
    Document* document = frame_->GetDocument();
    if (!document->GetLayoutView())
      return WebInputEventResult::kNotHandled;

    TRACE_EVENT_INSTANT0("input", "Retargeting Scroll",
                         TRACE_EVENT_SCOPE_THREAD);

    LocalFrameView* view = frame_->View();
    PhysicalOffset view_point(view->ConvertFromRootFrame(
        gfx::ToFlooredPoint(gesture_event.PositionInRootFrame())));
    HitTestRequest request(HitTestRequest::kReadOnly);
    HitTestLocation location(view_point);
    HitTestResult result(request, location);
    document->GetLayoutView()->HitTest(location, result);

    event_target = result.InnerNode();

    scroll_gesture_handling_node_ = event_target;

    if (!scrollbar)
      scrollbar = result.GetScrollbar();
  }

  // Gesture scroll events injected by scrollbars should not be routed back to
  // the scrollbar itself as they are intended to perform the scroll action on
  // the scrollable area. Scrollbar injected gestures don't clear
  // scrollbar_handling_scroll_gesture_ because touch-based scroll gestures need
  // to continue going to the scrollbar first so that the scroll direction
  // can be made proportional to the scroll thumb/ScrollableArea size and
  // inverted.
  if (scrollbar &&
      gesture_event.SourceDevice() != WebGestureDevice::kScrollbar) {
    bool should_update_capture = false;
    if (scrollbar->GestureEvent(gesture_event, &should_update_capture)) {
      if (should_update_capture)
        scrollbar_handling_scroll_gesture_ = scrollbar;
      return WebInputEventResult::kHandledSuppressed;
    }

    scrollbar_handling_scroll_gesture_ = nullptr;
  }

  if (event_target) {
    if (HandleScrollGestureOnResizer(event_target, gesture_event))
      return WebInputEventResult::kHandledSuppressed;
  }
  return WebInputEventResult::kNotHandled;
}

Node* ScrollManager::NodeTargetForScrollableAreaElementId(
    CompositorElementId element_id) const {
  Page* page = frame_->GetPage();
  DCHECK(page);
  ScrollableArea* scrollable_area = nullptr;
  if (page->GetVisualViewport().GetScrollElementId() == element_id) {
    // If the element_id is the visual viewport, redirect to the
    // root LocalFrameView's scrollable area (i.e. the RootFrameViewport).
    scrollable_area = frame_->LocalFrameRoot().View()->GetScrollableArea();
  } else {
    ScrollingCoordinator* scrolling_coordinator =
        page->GetScrollingCoordinator();
    scrollable_area =
        scrolling_coordinator->ScrollableAreaWithElementIdInAllLocalFrames(
            element_id);
  }

  // It is possible for an unrelated task to run between the time that
  // the gesture targeting element_id is queued and when we're processing
  // the gesture here, so we must validate the scrollable_area still
  // exists along with it's layout information. If not, just drop this
  // gesture since there is no relevant data on where to target the gesture.
  LayoutBox* layout_box =
      scrollable_area ? scrollable_area->GetLayoutBox() : nullptr;
  if (!layout_box) {
    return nullptr;
  }

  Node* event_target = nullptr;
  if (layout_box->GetDocument().GetFrame() == frame_) {
    event_target = scrollable_area->EventTargetNode();
  } else {
    // The targeted ScrollableArea may not belong to this frame. If that
    // is the case, target its ancestor HTMLFrameOwnerElement that exists
    // in this view, so that the gesture handling can be passed down to
    // the appropriate event handler.
    LocalFrame* current_frame = layout_box->GetDocument().GetFrame();
    while (current_frame) {
      HTMLFrameOwnerElement* owner = current_frame->GetDocument()->LocalOwner();
      // If the hosting element has no layout box, don't return it for targeting
      // since there's nothing to scroll.
      if (!owner->GetLayoutBox())
        break;

      LocalFrame* owner_frame =
          owner ? owner->GetDocument().GetFrame() : nullptr;
      if (owner_frame == frame_) {
        event_target = owner;
        break;
      }
      current_frame = owner_frame;
    }
  }

  return event_target;
}

bool ScrollManager::IsScrollbarHandlingGestures() const {
  return scrollbar_handling_scroll_gesture_.Get();
}

bool ScrollManager::HandleScrollGestureOnResizer(
    Node* event_target,
    const WebGestureEvent& gesture_event) {
  if (gesture_event.SourceDevice() != WebGestureDevice::kTouchscreen)
    return false;

  if (gesture_event.GetType() == WebInputEvent::Type::kGestureScrollBegin) {
    PaintLayer* layer = event_target->GetLayoutObject()
                            ? event_target->GetLayoutObject()->EnclosingLayer()
                            : nullptr;
    gfx::Point p = frame_->View()->ConvertFromRootFrame(
        gfx::ToFlooredPoint(gesture_event.PositionInRootFrame()));
    if (layer && layer->GetScrollableArea() &&
        layer->GetScrollableArea()->IsAbsolutePointInResizeControl(
            p, kResizerForTouch)) {
      resize_scrollable_area_ = layer->GetScrollableArea();
      resize_scrollable_area_->SetInResizeMode(true);
      offset_from_resize_corner_ =
          resize_scrollable_area_->OffsetFromResizeCorner(p);
      return true;
    }
  } else if (gesture_event.GetType() ==
             WebInputEvent::Type::kGestureScrollUpdate) {
    if (resize_scrollable_area_ && resize_scrollable_area_->InResizeMode()) {
      gfx::Point pos = gfx::ToRoundedPoint(gesture_event.PositionInRootFrame());
      pos.Offset(gesture_event.DeltaXInRootFrame(),
                 gesture_event.DeltaYInRootFrame());
      resize_scrollable_area_->Resize(pos, offset_from_resize_corner_);
      return true;
    }
  } else if (gesture_event.GetType() ==
             WebInputEvent::Type::kGestureScrollEnd) {
    if (resize_scrollable_area_ && resize_scrollable_area_->InResizeMode()) {
      resize_scrollable_area_->SetInResizeMode(false);
      resize_scrollable_area_ = nullptr;
      return false;
    }
  }

  return false;
}

bool ScrollManager::InResizeMode() const {
  return resize_scrollable_area_ && resize_scrollable_area_->InResizeMode();
}

void ScrollManager::Resize(const WebMouseEvent& evt) {
  if (evt.GetType() == WebInputEvent::Type::kMouseMove) {
    if (!frame_->GetEventHandler().MousePressed())
      return;
    resize_scrollable_area_->Resize(
        gfx::ToFlooredPoint(evt.PositionInRootFrame()),
        offset_from_resize_corner_);
  }
}

void ScrollManager::ClearResizeScrollableArea(bool should_not_be_null) {
  if (should_not_be_null)
    DCHECK(resize_scrollable_area_);

  if (resize_scrollable_area_)
    resize_scrollable_area_->SetInResizeMode(false);
  resize_scrollable_area_ = nullptr;
}

void ScrollManager::SetResizeScrollableArea(PaintLayer* layer, gfx::Point p) {
  resize_scrollable_area_ = layer->GetScrollableArea();
  resize_scrollable_area_->SetInResizeMode(true);
  offset_from_resize_corner_ =
      resize_scrollable_area_->OffsetFromResizeCorner(p);
}

bool ScrollManager::CanHandleGestureEvent(
    const GestureEventWithHitTestResults& targeted_event) {
  Scrollbar* scrollbar = targeted_event.GetHitTestResult().GetScrollbar();

  if (scrollbar) {
    bool should_update_capture = false;
    if (scrollbar->GestureEvent(targeted_event.Event(),
                                &should_update_capture)) {
      if (should_update_capture)
        scrollbar_handling_scroll_gesture_ = scrollbar;
      return true;
    }
  }
  return false;
}

}  // namespace blink
