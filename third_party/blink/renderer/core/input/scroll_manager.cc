// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/scroll_manager.h"

#include <utility>

#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/keyboard_event_manager.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace blink {

ScrollManager::ScrollManager(LocalFrame& frame) : frame_(frame) {
  Clear();
}

void ScrollManager::Clear() {
  resize_scrollable_area_ = nullptr;
  resize_position_to_size_transform_ = {};
}

void ScrollManager::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(resize_scrollable_area_);
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

ScrollManager::ScrollChainResult ScrollManager::RecomputeScrollChain(
    const Node& start_node,
    mojom::blink::ScrollDirection direction) {
  DCHECK(start_node.GetLayoutObject());
  LayoutBox* cur_box = start_node.GetLayoutObject()->EnclosingBox();
  ScrollChainResult result;
  // Scrolling propagates along the containing block chain and ends at the
  // RootScroller node. The RootScroller node will have a custom applyScroll
  // callback that performs scrolling as well as associated "root" actions
  // like browser control movement and overscroll glow.
  while (cur_box) {
    Node* cur_node = cur_box->GetNode();

    if (cur_node) {
      if (CanScroll(*cur_node)) {
        result.chain.push_front(cur_node->GetDomNodeId());
        // If `cur_node` is scrollable, respect its overscroll-behavior to
        // determine whether the scroll should bubble to parent elements.
        if (RuntimeEnabledFeatures::
                RespectOverscrollBehaviorForScrollBubblingEnabled()) {
          ScrollDirectionPhysical physical_direction = ToPhysicalDirection(
              direction, cur_box->IsHorizontalWritingMode(),
              cur_box->Style()->IsFlippedBlocksWritingMode());
          bool is_vertical =
              physical_direction == ScrollDirectionPhysical::kScrollUp ||
              physical_direction == ScrollDirectionPhysical::kScrollDown;
          EOverscrollBehavior behavior =
              is_vertical ? cur_box->StyleRef().OverscrollBehaviorY()
                          : cur_box->StyleRef().OverscrollBehaviorX();
          if (behavior != EOverscrollBehavior::kAuto) {
            result.can_bubble = false;
            break;
          }
        }
      }

      if (cur_node->IsEffectiveRootScroller()) {
        break;
      }
    }

    cur_box = cur_box->ContainingBlock();
  }

  return result;
}

bool ScrollManager::CanScroll(const Node& current_node) {
  LayoutBox* scrolling_box = current_node.GetLayoutBox();
  if (auto* element = DynamicTo<Element>(current_node))
    scrolling_box = element->GetLayoutBoxForScrolling();
  if (!scrolling_box)
    return false;

  // We need to always add the global root scroller even if it isn't scrollable
  // since we can always pinch-zoom and scroll as well as for overscroll
  // effects.
  if (scrolling_box->IsGlobalRootScroller()) {
    return true;
  }

  // If this is the main LayoutView of an active viewport (outermost main
  // frame), and it's not the root scroller, that means we have a non-default
  // root scroller on the page.  In this case, attempts to scroll the LayoutView
  // should cause panning of the visual viewport as well so ensure it gets added
  // to the scroll chain.  See LTHI::ApplyScroll for the equivalent behavior in
  // CC. Node::NativeApplyScroll contains a special handler for this case.
  if (IsA<LayoutView>(scrolling_box) &&
      current_node.GetDocument().IsInMainFrame() &&
      frame_->GetPage()->GetVisualViewport().IsActiveViewport()) {
    return true;
  }

  return scrolling_box->GetScrollableArea() != nullptr;
}

LogicalScrollResult ScrollManager::LogicalScroll(
    mojom::blink::ScrollDirection direction,
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

  if (!node) {
    return LogicalScrollResult::kBubbled;
  }

  Document& document = node->GetDocument();

  document.UpdateStyleAndLayout(DocumentUpdateReason::kScroll);

  ScrollChainResult scroll_chain_result =
      RecomputeScrollChain(*node, direction);
  while (!scroll_chain_result.chain.empty()) {
    Node* scroll_chain_node =
        DOMNodeIds::NodeForId(scroll_chain_result.chain.TakeLast());
    DCHECK(scroll_chain_node);

    auto* box = To<LayoutBox>(scroll_chain_node->GetLayoutObject());
    DCHECK(box);

    ScrollDirectionPhysical physical_direction =
        ToPhysicalDirection(direction, box->IsHorizontalWritingMode(),
                            box->Style()->IsFlippedBlocksWritingMode());

    ScrollableArea* scrollable_area = ScrollableArea::GetForScrolling(box);
    DCHECK(scrollable_area);

    cc::ScrollSourceType source_type = cc::ScrollSourceType::kNone;
    // Pressing the arrow key is considered as a scroll with intended direction
    // only. Pressing the PgUp/PgDn key is considered as a scroll with intended
    // direction and end position. Pressing the Home/End key is considered as a
    // scroll with intended end position only.
    switch (granularity) {
      case ui::ScrollGranularity::kScrollByLine: {
        if (scrollable_area->SnapForDirection(physical_direction)) {
          return LogicalScrollResult::kScrolled;
        }
        source_type = cc::ScrollSourceType::kRelativeScroll;
        break;
      }
      case ui::ScrollGranularity::kScrollByPage: {
        if (scrollable_area->SnapForPageScroll(physical_direction)) {
          return LogicalScrollResult::kScrolled;
        }
        source_type = cc::ScrollSourceType::kRelativeScroll;
        break;
      }
      case ui::ScrollGranularity::kScrollByDocument: {
        if (scrollable_area->SnapForDocumentScroll(physical_direction)) {
          return LogicalScrollResult::kScrolled;
        }
        source_type = cc::ScrollSourceType::kAbsoluteScroll;
        break;
      }
      default:
        NOTREACHED();
    }

    ScrollableArea::ScrollCallback callback(BindOnce(
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
        granularity, ToScrollDelta(physical_direction, 1), source_type,
        std::move(callback));

    if (result.DidScroll()) {
      return LogicalScrollResult::kScrolled;
    }
  }

  return scroll_chain_result.can_bubble ? LogicalScrollResult::kBubbled
                                        : LogicalScrollResult::kContained;
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
  LogicalScrollResult result =
      LogicalScroll(direction, granularity, starting_node, mouse_press_node,
                    scrolling_via_key);
  if (result != LogicalScrollResult::kBubbled) {
    return true;
  }

  return frame_->BubbleLogicalScrollInParentFrame(direction, granularity);
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
        resize_position_to_size_transform_);
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
  resize_position_to_size_transform_ =
      resize_scrollable_area_->InitializeResizeTransform(p);
}

}  // namespace blink
