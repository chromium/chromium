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
namespace {

cc::SnapFlingController::GestureScrollType ToGestureScrollType(
    WebInputEvent::Type web_event_type) {
  switch (web_event_type) {
    case WebInputEvent::Type::kGestureScrollBegin:
      return cc::SnapFlingController::GestureScrollType::kBegin;
    case WebInputEvent::Type::kGestureScrollUpdate:
      return cc::SnapFlingController::GestureScrollType::kUpdate;
    case WebInputEvent::Type::kGestureScrollEnd:
      return cc::SnapFlingController::GestureScrollType::kEnd;
    default:
      NOTREACHED();
      return cc::SnapFlingController::GestureScrollType::kBegin;
  }
}

cc::SnapFlingController::GestureScrollUpdateInfo GetGestureScrollUpdateInfo(
    const WebGestureEvent& event) {
  return {
      .delta = gfx::Vector2dF(-event.data.scroll_update.delta_x,
                              -event.data.scroll_update.delta_y),
      .is_in_inertial_phase = event.data.scroll_update.inertial_phase ==
                              WebGestureEvent::InertialPhaseState::kMomentum,
      .event_time = event.TimeStamp()};
}
}  // namespace

ScrollManager::ScrollManager(LocalFrame& frame) : frame_(frame) {
  snap_fling_controller_ = std::make_unique<cc::SnapFlingController>(this);

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
  visitor->Trace(previous_gesture_scrolled_node_);
  visitor->Trace(scrollbar_handling_scroll_gesture_);
  visitor->Trace(resize_scrollable_area_);
}

void ScrollManager::ClearGestureScrollState() {
  last_gesture_scroll_over_embedded_content_view_ = false;
  scroll_gesture_handling_node_ = nullptr;
  previous_gesture_scrolled_node_ = nullptr;
  last_scroll_delta_for_scroll_gesture_ = ScrollOffset();
  delta_consumed_for_scroll_sequence_ = false;
  did_scroll_x_for_scroll_gesture_ = false;
  did_scroll_y_for_scroll_gesture_ = false;
  current_scroll_chain_.clear();

  if (Page* page = frame_->GetPage()) {
    bool reset_x = true;
    bool reset_y = true;
    page->GetOverscrollController().ResetAccumulated(reset_x, reset_y);
  }
}

Node* ScrollManager::GetScrollEventTarget() {
  // Send the overscroll event to the node that scrolling is latched to which
  // is either previously scrolled node or the last node in the scroll chain.
  Node* scroll_target = previous_gesture_scrolled_node_;
  if (!scroll_target && !current_scroll_chain_.empty())
    scroll_target = DOMNodeIds::NodeForId(current_scroll_chain_.front());
  return scroll_target;
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
      scroll_chain.push_front(DOMNodeIds::IdForNode(cur_node));
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
          scroll_chain.push_front(DOMNodeIds::IdForNode(cur_node));

        if (cur_node->IsEffectiveRootScroller())
          break;

        ScrollPropagationDirection direction =
            ComputePropagationDirection(scroll_state);
        if (!CanPropagate(cur_node->GetLayoutBox(), direction)) {
          // We should add the first node with non-auto overscroll-behavior to
          // the scroll chain regardlessly, as it's the only node we can latch
          // to.
          if (scroll_chain.empty() ||
              scroll_chain.front() != DOMNodeIds::IdForNode(cur_node)) {
            scroll_chain.push_front(DOMNodeIds::IdForNode(cur_node));
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
      GetPage()->GetVisualViewport().IsActiveViewport() && !for_autoscroll) {
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

void ScrollManager::CustomizedScroll(ScrollState& scroll_state) {
  TRACE_EVENT0("input", "ScrollManager::CustomizedScroll");
  if (scroll_state.FullyConsumed())
    return;

  if (scroll_state.deltaX() || scroll_state.deltaY()) {
    frame_->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kScroll);
  }

  DCHECK(!current_scroll_chain_.empty());

  scroll_state.SetScrollChain(current_scroll_chain_);

  scroll_state.distributeToScrollChainDescendant();
}

uint32_t ScrollManager::GetNonCompositedMainThreadScrollingReasons() const {
  // When scrolling on the main thread, the scrollableArea may or may not be
  // composited. Either way, we have recorded either the reasons stored in
  // its layer or the reason NonFastScrollableRegion from the compositor
  // side. Here we record scrolls that occurred on main thread due to a
  // non-composited scroller.
  if (!scroll_gesture_handling_node_->GetLayoutObject())
    return 0;

  uint32_t non_composited_main_thread_scrolling_reasons = 0;
  for (auto* cur_box =
           scroll_gesture_handling_node_->GetLayoutObject()->EnclosingBox();
       cur_box; cur_box = cur_box->ContainingBlock()) {
    PaintLayerScrollableArea* scrollable_area = cur_box->GetScrollableArea();

    if (!scrollable_area || !scrollable_area->ScrollsOverflow())
      continue;

    if (auto* compositor =
            cur_box->GetFrameView()->GetPaintArtifactCompositor()) {
      non_composited_main_thread_scrolling_reasons |=
          (compositor->GetMainThreadScrollingReasons(
               *cur_box->FirstFragment().PaintProperties()->Scroll()) &
           cc::MainThreadScrollingReason::kNonCompositedReasons);
    }
  }
  return non_composited_main_thread_scrolling_reasons;
}

void ScrollManager::RecordScrollRelatedMetrics(WebGestureDevice device) const {
  if (device != WebGestureDevice::kTouchpad &&
      device != WebGestureDevice::kTouchscreen) {
    return;
  }

  if (uint32_t non_composited_main_thread_scrolling_reasons =
          GetNonCompositedMainThreadScrollingReasons()) {
    RecordScrollReasonsMetric(device,
                              non_composited_main_thread_scrolling_reasons);
  }
}

WebInputEventResult ScrollManager::HandleGestureScrollBegin(
    const WebGestureEvent& gesture_event) {
  TRACE_EVENT0("input", "ScrollManager::handleGestureScrollBegin");
  DCHECK(!base::FeatureList::IsEnabled(::features::kScrollUnification));
  if (base::FeatureList::IsEnabled(::features::kScrollUnification)) {
    return WebInputEventResult::kNotHandled;
  }

  Document* document = frame_->GetDocument();

  if (!document->GetLayoutView())
    return WebInputEventResult::kNotHandled;

  // If there's no layoutObject on the node, send the event to the nearest
  // ancestor with a layoutObject.  Needed for <option> and <optgroup> elements
  // so we can touch scroll <select>s
  while (scroll_gesture_handling_node_ &&
         !scroll_gesture_handling_node_->GetLayoutObject())
    scroll_gesture_handling_node_ =
        scroll_gesture_handling_node_->ParentOrShadowHostNode();

  if (!scroll_gesture_handling_node_)
    scroll_gesture_handling_node_ = frame_->GetDocument();

  if (!scroll_gesture_handling_node_ ||
      !scroll_gesture_handling_node_->GetLayoutObject()) {
    TRACE_EVENT_INSTANT0("input", "Dropping: No LayoutObject",
                         TRACE_EVENT_SCOPE_THREAD);
    return WebInputEventResult::kNotHandled;
  }

  WebInputEventResult child_result = PassScrollGestureEvent(
      gesture_event, scroll_gesture_handling_node_->GetLayoutObject());

  RecordScrollRelatedMetrics(gesture_event.SourceDevice());

  current_scroll_chain_.clear();
  std::unique_ptr<ScrollStateData> scroll_state_data =
      std::make_unique<ScrollStateData>();
  gfx::Point position =
      gfx::ToFlooredPoint(gesture_event.PositionInRootFrame());
  scroll_state_data->position_x = position.x();
  scroll_state_data->position_y = position.y();
  scroll_state_data->delta_x_hint = -gesture_event.DeltaXInRootFrame();
  scroll_state_data->delta_y_hint = -gesture_event.DeltaYInRootFrame();
  scroll_state_data->delta_granularity = gesture_event.DeltaUnits();
  scroll_state_data->is_beginning = true;
  scroll_state_data->from_user_input = true;
  scroll_state_data->is_direct_manipulation =
      gesture_event.SourceDevice() == WebGestureDevice::kTouchscreen;
  scroll_state_data->delta_consumed_for_scroll_sequence =
      delta_consumed_for_scroll_sequence_;
  auto* scroll_state =
      MakeGarbageCollected<ScrollState>(std::move(scroll_state_data));

  RecomputeScrollChain(
      *scroll_gesture_handling_node_.Get(), *scroll_state,
      current_scroll_chain_,
      gesture_event.SourceDevice() == WebGestureDevice::kSyntheticAutoscroll);

  TRACE_EVENT_INSTANT1("input", "Computed Scroll Chain",
                       TRACE_EVENT_SCOPE_THREAD, "length",
                       current_scroll_chain_.size());

  if (current_scroll_chain_.empty()) {
    // If a child has a non-empty scroll chain, we need to consider that instead
    // of simply returning WebInputEventResult::kNotHandled.
    return child_result;
  }

  NotifyScrollPhaseBeginForCustomizedScroll(*scroll_state);

  CustomizedScroll(*scroll_state);

  if (gesture_event.SourceDevice() == WebGestureDevice::kTouchpad &&
      gesture_event.data.scroll_begin.delta_hint_units ==
          ui::ScrollGranularity::kScrollByPrecisePixel) {
    UseCounter::Count(document, WebFeature::kScrollByPrecisionTouchPad);
  } else if (gesture_event.SourceDevice() == WebGestureDevice::kTouchscreen) {
    UseCounter::Count(document, WebFeature::kScrollByTouch);
  } else {
    UseCounter::Count(document, WebFeature::kScrollByWheel);
  }

  return WebInputEventResult::kHandledSystem;
}

WebInputEventResult ScrollManager::HandleGestureScrollUpdate(
    const WebGestureEvent& gesture_event) {
  TRACE_EVENT0("input", "ScrollManager::handleGestureScrollUpdate");
  DCHECK_EQ(gesture_event.GetType(), WebInputEvent::Type::kGestureScrollUpdate);

  DCHECK(!base::FeatureList::IsEnabled(::features::kScrollUnification));
  if (base::FeatureList::IsEnabled(::features::kScrollUnification)) {
    return WebInputEventResult::kNotHandled;
  }

  if (snap_fling_controller_) {
    if (snap_fling_controller_->HandleGestureScrollUpdate(
            GetGestureScrollUpdateInfo(gesture_event))) {
      return WebInputEventResult::kHandledSystem;
    }
  }

  // Negate the deltas since the gesture event stores finger movement and
  // scrolling occurs in the direction opposite the finger's movement
  // direction. e.g. Finger moving up has negative event delta but causes the
  // page to scroll down causing positive scroll delta.
  ScrollOffset delta(-gesture_event.DeltaXInRootFrame(),
                     -gesture_event.DeltaYInRootFrame());
  gfx::Vector2dF velocity(-gesture_event.VelocityX(),
                          -gesture_event.VelocityY());
  gfx::PointF position = gesture_event.PositionInRootFrame();

  if (delta.IsZero())
    return WebInputEventResult::kNotHandled;

  LayoutObject* layout_object =
      scroll_gesture_handling_node_
          ? scroll_gesture_handling_node_->GetLayoutObject()
          : nullptr;

  // Try to send the event to the correct view.
  WebInputEventResult result =
      PassScrollGestureEvent(gesture_event, layout_object);
  if (result != WebInputEventResult::kNotHandled) {
    // FIXME: we should allow simultaneous scrolling of nested
    // iframes along perpendicular axes. See crbug.com/466991.
    delta_consumed_for_scroll_sequence_ = true;
    return result;
  }

  if (current_scroll_chain_.empty()) {
    TRACE_EVENT_INSTANT0("input", "Empty Scroll Chain",
                         TRACE_EVENT_SCOPE_THREAD);
    return WebInputEventResult::kNotHandled;
  }

  std::unique_ptr<ScrollStateData> scroll_state_data =
      std::make_unique<ScrollStateData>();
  scroll_state_data->delta_x = delta.x();
  scroll_state_data->delta_y = delta.y();
  scroll_state_data->delta_granularity = gesture_event.DeltaUnits();
  scroll_state_data->velocity_x = velocity.x();
  scroll_state_data->velocity_y = velocity.y();
  scroll_state_data->position_x = position.x();
  scroll_state_data->position_y = position.y();
  scroll_state_data->is_in_inertial_phase =
      gesture_event.InertialPhase() ==
      WebGestureEvent::InertialPhaseState::kMomentum;
  scroll_state_data->is_direct_manipulation =
      gesture_event.SourceDevice() == WebGestureDevice::kTouchscreen;
  scroll_state_data->from_user_input = true;
  scroll_state_data->delta_consumed_for_scroll_sequence =
      delta_consumed_for_scroll_sequence_;
  // Scrollbar interactions trigger scroll snap depending on the scrollbar part.
  if (gesture_event.SourceDevice() == WebGestureDevice::kScrollbar)
    AdjustForSnapAtScrollUpdate(gesture_event, scroll_state_data.get());
  auto* scroll_state =
      MakeGarbageCollected<ScrollState>(std::move(scroll_state_data));
  if (previous_gesture_scrolled_node_) {
    // The ScrollState needs to know what the current
    // native scrolling element is, so that for an
    // inertial scroll that shouldn't propagate, only the
    // currently scrolling element responds.
    scroll_state->SetCurrentNativeScrollingNode(
        previous_gesture_scrolled_node_);
  }

  CustomizedScroll(*scroll_state);
  previous_gesture_scrolled_node_ = scroll_state->CurrentNativeScrollingNode();
  delta_consumed_for_scroll_sequence_ =
      scroll_state->DeltaConsumedForScrollSequence();

  last_scroll_delta_for_scroll_gesture_ = delta;

  bool did_scroll_x = scroll_state->deltaX() != delta.x();
  bool did_scroll_y = scroll_state->deltaY() != delta.y();

  did_scroll_x_for_scroll_gesture_ |= did_scroll_x;
  did_scroll_y_for_scroll_gesture_ |= did_scroll_y;

  // TODO(bokan): This looks like it resets overscroll if any *effective* root
  // scroller is scrolled. This should probably be if the *global* root
  // scroller is scrolled.
  if (!previous_gesture_scrolled_node_ ||
      !previous_gesture_scrolled_node_->IsEffectiveRootScroller())
    GetPage()->GetOverscrollController().ResetAccumulated(did_scroll_x,
                                                          did_scroll_y);

  if (did_scroll_x || did_scroll_y)
    return WebInputEventResult::kHandledSystem;

  if (RuntimeEnabledFeatures::OverscrollCustomizationEnabled()) {
    if (Node* overscroll_target = GetScrollEventTarget()) {
      overscroll_target->GetDocument().EnqueueOverscrollEventForNode(
          overscroll_target, delta.x(), delta.y());
    }
  }

  return WebInputEventResult::kNotHandled;
}

void ScrollManager::AdjustForSnapAtScrollUpdate(
    const WebGestureEvent& gesture_event,
    ScrollStateData* scroll_state_data) {
  // Scrollbar interactions outside of the scroll thumb behave like logical
  // scrolls with respect to scroll snap behavior.
  // Scrollbar arrows trigger scroll by line, whereas the scrollbar track
  // triggers scroll by page.
  DCHECK(gesture_event.SourceDevice() == WebGestureDevice::kScrollbar);
  ui::ScrollGranularity granularity = gesture_event.DeltaUnits();
  if (granularity != ui::ScrollGranularity::kScrollByLine &&
      granularity != ui::ScrollGranularity::kScrollByPage) {
    return;
  }

  Node* node = scroll_gesture_handling_node_;
  ScrollableArea* scrollable_area = nullptr;
  if (node && node->GetLayoutObject()) {
    LayoutBox* layout_box = node->GetLayoutBox();
    if (!layout_box)
      return;
    scrollable_area = ScrollableArea::GetForScrolling(layout_box);
  }

  if (!scrollable_area)
    return;

  gfx::PointF current_position = scrollable_area->ScrollPosition();
  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForDirection(
          current_position,
          gfx::Vector2dF(scroll_state_data->delta_x,
                         scroll_state_data->delta_y),
          RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled());

  absl::optional<gfx::PointF> snap_point =
      scrollable_area->GetSnapPositionAndSetTarget(*strategy);
  if (!snap_point)
    return;

  scroll_state_data->delta_x = (snap_point->x() - current_position.x());
  scroll_state_data->delta_y = (snap_point->y() - current_position.y());
  scroll_state_data->delta_granularity = ui::ScrollGranularity::kScrollByPixel;
}

// This method is used as a ScrollCallback which requires a void return. Since
// this call to HandleGestureScrollEnd is async, we can just ignore the return
// value.
void ScrollManager::HandleDeferredGestureScrollEnd(
    const WebGestureEvent& gesture_event,
    ScrollableArea::ScrollCompletionMode mode) {
  HandleGestureScrollEnd(gesture_event);
}

WebInputEventResult ScrollManager::HandleGestureScrollEnd(
    const WebGestureEvent& gesture_event) {
  TRACE_EVENT0("input", "ScrollManager::handleGestureScrollEnd");

  DCHECK(!base::FeatureList::IsEnabled(::features::kScrollUnification));
  if (base::FeatureList::IsEnabled(::features::kScrollUnification)) {
    return WebInputEventResult::kNotHandled;
  }

  GetPage()->GetBrowserControls().ScrollEnd();

  // It's possible to get here due to a deferred scroll end having its waiting
  // animation "completed" by the document being stopped. In that case we're
  // about to be destroyed so early return.
  if (!frame_->GetDocument()->IsActive())
    return WebInputEventResult::kNotHandled;

  Node* node = scroll_gesture_handling_node_;

  if (node && node->GetLayoutObject()) {
    // If the GSE is for a scrollable area that has an in-progress animation,
    // defer the handling of the gesture event until the scroll animation is
    // complete. This will allow things like scroll snapping to be deferred so
    // that users can see the result of their scroll before the snap is applied.
    LayoutBox* layout_box = node->GetLayoutBox();
    ScrollableArea* scrollable_area =
        layout_box ? layout_box->GetScrollableArea() : nullptr;
    if (scrollable_area && scrollable_area->ExistingScrollAnimator() &&
        scrollable_area->ExistingScrollAnimator()->HasRunningAnimation()) {
      scrollable_area->RegisterScrollCompleteCallback(
          WTF::BindOnce(&ScrollManager::HandleDeferredGestureScrollEnd,
                        WrapWeakPersistent(this), gesture_event));
      return WebInputEventResult::kNotHandled;
    }

    PassScrollGestureEvent(gesture_event, node->GetLayoutObject());
    if (current_scroll_chain_.empty()) {
      ClearGestureScrollState();
      return WebInputEventResult::kNotHandled;
    }
    std::unique_ptr<ScrollStateData> scroll_state_data =
        std::make_unique<ScrollStateData>();
    scroll_state_data->is_ending = true;
    scroll_state_data->is_in_inertial_phase =
        gesture_event.InertialPhase() ==
        WebGestureEvent::InertialPhaseState::kMomentum;
    scroll_state_data->from_user_input = true;
    scroll_state_data->is_direct_manipulation =
        gesture_event.SourceDevice() == WebGestureDevice::kTouchscreen;
    scroll_state_data->delta_consumed_for_scroll_sequence =
        delta_consumed_for_scroll_sequence_;
    auto* scroll_state =
        MakeGarbageCollected<ScrollState>(std::move(scroll_state_data));
    CustomizedScroll(*scroll_state);

    // We add a callback to set the hover state dirty and send a scroll end
    // event when the scroll ends without snap or the snap point is the same as
    // the scroll position.
    base::ScopedClosureRunner callback(WTF::BindOnce(
        [](WeakPersistent<LocalFrame> local_frame,
           WeakPersistent<ScrollManager> scroll_manager) {
          if (local_frame) {
            local_frame->GetEventHandler().MarkHoverStateDirty();
          }

          Node* scroll_end_target = scroll_manager->GetScrollEventTarget();
          if (RuntimeEnabledFeatures::ScrollEndEventsEnabled() &&
              scroll_end_target) {
            scroll_end_target->GetDocument().EnqueueScrollEndEventForNode(
                scroll_end_target);
          }
        },
        WrapWeakPersistent(&(frame_->LocalFrameRoot())),
        WrapWeakPersistent(this)));

    SnapAtGestureScrollEnd(gesture_event, std::move(callback));
    NotifyScrollPhaseEndForCustomizedScroll();
  }

  ClearGestureScrollState();

  return WebInputEventResult::kNotHandled;
}

LayoutBox* ScrollManager::LayoutBoxForSnapping() const {
  if (!previous_gesture_scrolled_node_)
    return nullptr;
  return previous_gesture_scrolled_node_->GetLayoutBox();
}

ScrollOffset GetScrollDirection(ScrollOffset delta) {
  delta.SetToMax(ScrollOffset(-1, -1));
  delta.SetToMin(ScrollOffset(1, 1));
  return delta;
}

bool ScrollManager::SnapAtGestureScrollEnd(
    const WebGestureEvent& end_event,
    base::ScopedClosureRunner on_finish) {
  if (!previous_gesture_scrolled_node_ ||
      (!did_scroll_x_for_scroll_gesture_ && !did_scroll_y_for_scroll_gesture_))
    return false;
  ScrollableArea* scrollable_area =
      ScrollableArea::GetForScrolling(LayoutBoxForSnapping());
  if (!scrollable_area)
    return false;

  bool is_mouse_wheel =
      end_event.SourceDevice() == WebGestureDevice::kTouchpad &&
      end_event.data.scroll_end.delta_units !=
          ui::ScrollGranularity::kScrollByPrecisePixel;

  // Treat mouse wheel scrolls as direction only scroll with its last scroll
  // delta amout. This means each wheel tick will prefer the next snap position
  // in the given direction. This leads to a much better UX for wheels.
  //
  // Precise wheel and trackpads continue to be treated similar as end position
  // scrolling.
  if (is_mouse_wheel && !last_scroll_delta_for_scroll_gesture_.IsZero()) {
    // TODO(majidvp): Currently DirectionStrategy uses current offset + delta as
    // the intended offset and chooses snap offset closest to that intended
    // offset. In this case, this is not correct because we are already at the
    // intended position. DirectionStategy should be updated to use current
    // offset as intended position. This requires changing how we snap in
    // |ScrollManager::LogicalScroll()|. For now use a unit scroll offset to
    // limit the miscalculation to 1px.
    ScrollOffset scroll_direction =
        GetScrollDirection(last_scroll_delta_for_scroll_gesture_);
    return scrollable_area->SnapForDirection(scroll_direction,
                                             std::move(on_finish));
  }

  return scrollable_area->SnapAtCurrentPosition(
      did_scroll_x_for_scroll_gesture_, did_scroll_y_for_scroll_gesture_,
      std::move(on_finish));
}

bool ScrollManager::GetSnapFlingInfoAndSetAnimatingSnapTarget(
    const gfx::Vector2dF& current_delta,
    const gfx::Vector2dF& natural_displacement,
    gfx::PointF* out_initial_position,
    gfx::PointF* out_target_position) const {
  ScrollableArea* scrollable_area =
      ScrollableArea::GetForScrolling(LayoutBoxForSnapping());
  if (!scrollable_area)
    return false;

  gfx::PointF current_position = scrollable_area->ScrollPosition();
  *out_initial_position = current_position;
  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForEndAndDirection(
          *out_initial_position, natural_displacement,
          RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled());
  absl::optional<gfx::PointF> snap_end =
      scrollable_area->GetSnapPositionAndSetTarget(*strategy);
  if (!snap_end.has_value())
    return false;
  *out_target_position = snap_end.value();
  return true;
}

gfx::PointF ScrollManager::ScrollByForSnapFling(const gfx::Vector2dF& delta) {
  ScrollableArea* scrollable_area =
      ScrollableArea::GetForScrolling(LayoutBoxForSnapping());
  if (!scrollable_area)
    return gfx::PointF();

  std::unique_ptr<ScrollStateData> scroll_state_data =
      std::make_unique<ScrollStateData>();

  // TODO(sunyunjia): Plumb the velocity of the snap curve as well.
  scroll_state_data->delta_x = delta.x();
  scroll_state_data->delta_y = delta.y();
  scroll_state_data->is_in_inertial_phase = true;
  scroll_state_data->from_user_input = true;
  scroll_state_data->delta_granularity =
      ui::ScrollGranularity::kScrollByPrecisePixel;
  scroll_state_data->delta_consumed_for_scroll_sequence =
      delta_consumed_for_scroll_sequence_;
  auto* scroll_state =
      MakeGarbageCollected<ScrollState>(std::move(scroll_state_data));
  scroll_state->SetCurrentNativeScrollingNode(previous_gesture_scrolled_node_);

  CustomizedScroll(*scroll_state);
  // Return the end scroll position.
  return scrollable_area->ScrollPosition();
}

void ScrollManager::ScrollEndForSnapFling(bool did_finish) {
  if (RuntimeEnabledFeatures::ScrollEndEventsEnabled()) {
    if (Node* scroll_end_target = GetScrollEventTarget()) {
      scroll_end_target->GetDocument().EnqueueScrollEndEventForNode(
          scroll_end_target);
    }
  }
  if (current_scroll_chain_.empty()) {
    NotifyScrollPhaseEndForCustomizedScroll();
    ClearGestureScrollState();
    return;
  }
  std::unique_ptr<ScrollStateData> scroll_state_data =
      std::make_unique<ScrollStateData>();
  scroll_state_data->is_ending = true;
  scroll_state_data->is_in_inertial_phase = true;
  scroll_state_data->from_user_input = true;
  scroll_state_data->delta_consumed_for_scroll_sequence =
      delta_consumed_for_scroll_sequence_;
  auto* scroll_state =
      MakeGarbageCollected<ScrollState>(std::move(scroll_state_data));
  CustomizedScroll(*scroll_state);
  NotifyScrollPhaseEndForCustomizedScroll();
  ClearGestureScrollState();
}

void ScrollManager::RequestAnimationForSnapFling() {
  if (Page* page = frame_->GetPage())
    page->GetChromeClient().ScheduleAnimation(frame_->View());
}

void ScrollManager::AnimateSnapFling(base::TimeTicks monotonic_time) {
  DCHECK(snap_fling_controller_);
  snap_fling_controller_->Animate(monotonic_time);
}

Page* ScrollManager::GetPage() const {
  return frame_->GetPage();
}

WebInputEventResult ScrollManager::PassScrollGestureEvent(
    const WebGestureEvent& gesture_event,
    LayoutObject* layout_object) {
  DCHECK(gesture_event.IsScrollEvent());

  auto* embedded = DynamicTo<LayoutEmbeddedContent>(layout_object);
  if (!last_gesture_scroll_over_embedded_content_view_ || !embedded)
    return WebInputEventResult::kNotHandled;

  FrameView* frame_view = embedded->ChildFrameView();

  if (!frame_view)
    return WebInputEventResult::kNotHandled;

  auto* local_frame_view = DynamicTo<LocalFrameView>(frame_view);
  if (!local_frame_view)
    return WebInputEventResult::kNotHandled;

  return local_frame_view->GetFrame()
      .GetEventHandler()
      .HandleGestureScrollEvent(gesture_event);
}

WebInputEventResult ScrollManager::HandleGestureScrollEvent(
    const WebGestureEvent& gesture_event) {
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
    if (event_target->GetLayoutObject()->IsLayoutEmbeddedContent()) {
      last_gesture_scroll_over_embedded_content_view_ = true;
    }
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

    last_gesture_scroll_over_embedded_content_view_ =
        result.IsOverEmbeddedContentView();

    scroll_gesture_handling_node_ = event_target;
    previous_gesture_scrolled_node_ = nullptr;
    delta_consumed_for_scroll_sequence_ = false;
    did_scroll_x_for_scroll_gesture_ = false;
    did_scroll_y_for_scroll_gesture_ = false;

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

  if (snap_fling_controller_) {
    if (gesture_event.IsGestureScroll() &&
        (snap_fling_controller_->FilterEventForSnap(
            ToGestureScrollType(gesture_event.GetType())))) {
      return WebInputEventResult::kNotHandled;
    }
  }
  switch (gesture_event.GetType()) {
    case WebInputEvent::Type::kGestureScrollBegin:
      return HandleGestureScrollBegin(gesture_event);
    case WebInputEvent::Type::kGestureScrollUpdate:
      return HandleGestureScrollUpdate(gesture_event);
    case WebInputEvent::Type::kGestureScrollEnd:
      return HandleGestureScrollEnd(gesture_event);
    case WebInputEvent::Type::kGestureFlingStart:
    case WebInputEvent::Type::kGestureFlingCancel:
      return WebInputEventResult::kNotHandled;
    default:
      NOTREACHED();
      return WebInputEventResult::kNotHandled;
  }
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

void ScrollManager::NotifyScrollPhaseBeginForCustomizedScroll(
    const ScrollState& scroll_state) {
  scroll_customization::ScrollDirection direction =
      scroll_customization::GetScrollDirectionFromDeltas(
          scroll_state.deltaXHint(), scroll_state.deltaYHint());
  for (auto id : current_scroll_chain_) {
    Node* node = DOMNodeIds::NodeForId(id);
    if (node)
      node->WillBeginCustomizedScrollPhase(direction);
  }
}

void ScrollManager::NotifyScrollPhaseEndForCustomizedScroll() {
  for (auto id : current_scroll_chain_) {
    Node* node = DOMNodeIds::NodeForId(id);
    if (node)
      node->DidEndCustomizedScrollPhase();
  }
}

}  // namespace blink
