// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"

#include "base/barrier_closure.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/input/snap_selection_strategy.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/scroll_anchor.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/smooth_scroll_sequencer.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {
namespace {
// Computes the rect of valid scroll offsets reachable by user scrolls for the
// scrollable area.
gfx::RectF GetUserScrollableRect(const ScrollableArea& area) {
  gfx::RectF user_scrollable_rect;
  ScrollOffset scrollable_size =
      area.MaximumScrollOffset() - area.MinimumScrollOffset();
  if (area.UserInputScrollable(kHorizontalScrollbar)) {
    user_scrollable_rect.set_x(area.MinimumScrollOffset().x());
    user_scrollable_rect.set_width(scrollable_size.x());
  } else {
    user_scrollable_rect.set_x(area.GetScrollOffset().x());
    user_scrollable_rect.set_width(0);
  }

  if (area.UserInputScrollable(kVerticalScrollbar)) {
    user_scrollable_rect.set_y(area.MinimumScrollOffset().y());
    user_scrollable_rect.set_height(scrollable_size.y());
  } else {
    user_scrollable_rect.set_y(area.GetScrollOffset().y());
    user_scrollable_rect.set_height(0);
  }
  return user_scrollable_rect;
}

}  // namespace
RootFrameViewport::RootFrameViewport(ScrollableArea& visual_viewport,
                                     ScrollableArea& layout_viewport)
    : ScrollableArea(visual_viewport.GetCompositorTaskRunner()),
      visual_viewport_(visual_viewport),
      should_restore_scroll_(false) {
  SetLayoutViewport(layout_viewport);
}

void RootFrameViewport::SetLayoutViewport(ScrollableArea& new_layout_viewport) {
  if (layout_viewport_.Get() == &new_layout_viewport)
    return;

  if (layout_viewport_ && layout_viewport_->GetScrollAnchor())
    layout_viewport_->GetScrollAnchor()->SetScroller(layout_viewport_.Get());

  layout_viewport_ = &new_layout_viewport;

  if (layout_viewport_->GetScrollAnchor())
    layout_viewport_->GetScrollAnchor()->SetScroller(this);
}

ScrollableArea& RootFrameViewport::LayoutViewport() const {
  DCHECK(layout_viewport_);
  return *layout_viewport_;
}

PhysicalRect RootFrameViewport::RootContentsToLayoutViewportContents(
    LocalFrameView& root_frame_view,
    const PhysicalRect& rect) const {
  PhysicalRect ret = rect;

  // If the root LocalFrameView is the layout viewport then coordinates in the
  // root LocalFrameView's content space are already in the layout viewport's
  // content space.
  if (root_frame_view.LayoutViewport() == &LayoutViewport())
    return ret;

  // Make the given rect relative to the top of the layout viewport's content
  // by adding the scroll position.
  // TODO(bokan): This will have to be revisited if we ever remove the
  // restriction that a root scroller must be exactly screen filling.
  ret.Move(
      PhysicalOffset::FromVector2dFRound(LayoutViewport().GetScrollOffset()));

  return ret;
}

void RootFrameViewport::RestoreToAnchor(const ScrollOffset& target_offset) {
  // Clamp the scroll offset of each viewport now so that we force any invalid
  // offsets to become valid so we can compute the correct deltas.
  GetVisualViewport().SetScrollOffset(GetVisualViewport().GetScrollOffset(),
                                      mojom::blink::ScrollType::kProgrammatic);
  LayoutViewport().SetScrollOffset(LayoutViewport().GetScrollOffset(),
                                   mojom::blink::ScrollType::kProgrammatic);

  ScrollOffset delta = target_offset - GetScrollOffset();

  GetVisualViewport().SetScrollOffset(
      GetVisualViewport().GetScrollOffset() + delta,
      mojom::blink::ScrollType::kProgrammatic);

  delta = target_offset - GetScrollOffset();

  if (RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled()) {
    LayoutViewport().SetScrollOffset(LayoutViewport().GetScrollOffset() + delta,
                                     mojom::blink::ScrollType::kProgrammatic);
  } else {
    gfx::Vector2d layout_delta(
        delta.x() < 0 ? floor(delta.x()) : ceil(delta.x()),
        delta.y() < 0 ? floor(delta.y()) : ceil(delta.y()));

    LayoutViewport().SetScrollOffset(
        ScrollOffset(LayoutViewport().ScrollOffsetInt() + layout_delta),
        mojom::blink::ScrollType::kProgrammatic);
  }

  delta = target_offset - GetScrollOffset();
  GetVisualViewport().SetScrollOffset(
      GetVisualViewport().GetScrollOffset() + delta,
      mojom::blink::ScrollType::kProgrammatic);
}

void RootFrameViewport::DidUpdateVisualViewport() {
  if (ScrollAnchor* anchor = LayoutViewport().GetScrollAnchor())
    anchor->Clear();
}

LayoutBox* RootFrameViewport::GetLayoutBox() const {
  return LayoutViewport().GetLayoutBox();
}

gfx::QuadF RootFrameViewport::LocalToVisibleContentQuad(
    const gfx::QuadF& quad,
    const LayoutObject* local_object,
    unsigned flags) const {
  if (!layout_viewport_)
    return quad;
  gfx::QuadF viewport_quad =
      layout_viewport_->LocalToVisibleContentQuad(quad, local_object, flags);
  if (visual_viewport_) {
    viewport_quad = visual_viewport_->LocalToVisibleContentQuad(
        viewport_quad, local_object, flags);
  }
  return viewport_quad;
}

scoped_refptr<base::SingleThreadTaskRunner>
RootFrameViewport::GetTimerTaskRunner() const {
  return LayoutViewport().GetTimerTaskRunner();
}

int RootFrameViewport::HorizontalScrollbarHeight(
    OverlayScrollbarClipBehavior behavior) const {
  return LayoutViewport().HorizontalScrollbarHeight(behavior);
}

int RootFrameViewport::VerticalScrollbarWidth(
    OverlayScrollbarClipBehavior behavior) const {
  return LayoutViewport().VerticalScrollbarWidth(behavior);
}

void RootFrameViewport::UpdateScrollAnimator() {
  GetScrollAnimator().SetCurrentOffset(ScrollOffsetFromScrollAnimators());
}

ScrollOffset RootFrameViewport::ScrollOffsetFromScrollAnimators() const {
  return GetVisualViewport().GetScrollAnimator().CurrentOffset() +
         LayoutViewport().GetScrollAnimator().CurrentOffset();
}

gfx::Rect RootFrameViewport::VisibleContentRect(
    IncludeScrollbarsInRect scrollbar_inclusion) const {
  return gfx::Rect(
      gfx::PointAtOffsetFromOrigin(ScrollOffsetInt()),
      GetVisualViewport().VisibleContentRect(scrollbar_inclusion).size());
}

PhysicalRect RootFrameViewport::VisibleScrollSnapportRect(
    IncludeScrollbarsInRect scrollbar_inclusion) const {
  // The effective viewport is the intersection of the visual viewport with the
  // layout viewport.
  PhysicalRect frame_rect_in_content(
      PhysicalOffset::FromVector2dFRound(LayoutViewport().GetScrollOffset()),
      PhysicalSize(
          LayoutViewport().VisibleContentRect(scrollbar_inclusion).size()));
  PhysicalRect visual_rect_in_content(
      PhysicalOffset::FromVector2dFRound(
          LayoutViewport().GetScrollOffset() +
          GetVisualViewport().GetScrollAnimator().CurrentOffset()),
      PhysicalSize(
          GetVisualViewport().VisibleContentRect(scrollbar_inclusion).size()));

  PhysicalRect visible_scroll_snapport =
      Intersection(visual_rect_in_content, frame_rect_in_content);
  if (!LayoutViewport().GetLayoutBox())
    return visible_scroll_snapport;

  const ComputedStyle* style = LayoutViewport().GetLayoutBox()->Style();
  LayoutRectOutsets padding(
      MinimumValueForLength(style->ScrollPaddingTop(),
                            visible_scroll_snapport.Height()),
      MinimumValueForLength(style->ScrollPaddingRight(),
                            visible_scroll_snapport.Width()),
      MinimumValueForLength(style->ScrollPaddingBottom(),
                            visible_scroll_snapport.Height()),
      MinimumValueForLength(style->ScrollPaddingLeft(),
                            visible_scroll_snapport.Width()));
  visible_scroll_snapport.Contract(padding);

  return visible_scroll_snapport;
}

bool RootFrameViewport::ShouldUseIntegerScrollOffset() const {
  // Fractionals are floored in the ScrollAnimatorBase but it's important that
  // the ScrollAnimators of the visual and layout viewports get the precise
  // fractional number so never use integer scrolling for RootFrameViewport,
  // we'll let the truncation happen in the subviewports.
  return false;
}

bool RootFrameViewport::IsActive() const {
  return LayoutViewport().IsActive();
}

int RootFrameViewport::ScrollSize(ScrollbarOrientation orientation) const {
  gfx::Vector2d scroll_dimensions =
      MaximumScrollOffsetInt() - MinimumScrollOffsetInt();
  return (orientation == kHorizontalScrollbar) ? scroll_dimensions.x()
                                               : scroll_dimensions.y();
}

bool RootFrameViewport::IsScrollCornerVisible() const {
  return LayoutViewport().IsScrollCornerVisible();
}

gfx::Rect RootFrameViewport::ScrollCornerRect() const {
  return LayoutViewport().ScrollCornerRect();
}

void RootFrameViewport::ApplyPendingHistoryRestoreScrollOffset() {
  if (!pending_view_state_)
    return;

  bool should_restore_scale = pending_view_state_->page_scale_factor_;

  // For main frame restore scale and visual viewport position
  ScrollOffset visual_viewport_offset(
      pending_view_state_->visual_viewport_scroll_offset_);

  // If the visual viewport's offset is (-1, -1) it means the history item
  // is an old version of HistoryItem so distribute the scroll between
  // the main frame and the visual viewport as best as we can.
  if (visual_viewport_offset.x() == -1 && visual_viewport_offset.y() == -1) {
    visual_viewport_offset = pending_view_state_->scroll_offset_ -
                             LayoutViewport().GetScrollOffset();
  }

  auto* visual_viewport = static_cast<VisualViewport*>(&GetVisualViewport());
  if (should_restore_scale && should_restore_scroll_) {
    visual_viewport->SetScaleAndLocation(
        pending_view_state_->page_scale_factor_,
        visual_viewport->IsPinchGestureActive(),
        gfx::PointAtOffsetFromOrigin(visual_viewport_offset));
  } else if (should_restore_scale) {
    visual_viewport->SetScale(pending_view_state_->page_scale_factor_);
  } else if (should_restore_scroll_) {
    visual_viewport->SetLocation(
        gfx::PointAtOffsetFromOrigin(visual_viewport_offset));
  }

  should_restore_scroll_ = false;

  pending_view_state_.reset();
}

void RootFrameViewport::SetScrollOffset(
    const ScrollOffset& offset,
    mojom::blink::ScrollType scroll_type,
    mojom::blink::ScrollBehavior scroll_behavior,
    ScrollCallback on_finish) {
  UpdateScrollAnimator();

  if (scroll_behavior == mojom::blink::ScrollBehavior::kAuto)
    scroll_behavior = ScrollBehaviorStyle();

  if (scroll_type == mojom::blink::ScrollType::kAnchoring) {
    DistributeScrollBetweenViewports(offset, scroll_type, scroll_behavior,
                                     kLayoutViewport, std::move(on_finish));
    return;
  }

  if (scroll_behavior == mojom::blink::ScrollBehavior::kSmooth) {
    DistributeScrollBetweenViewports(offset, scroll_type, scroll_behavior,
                                     kVisualViewport, std::move(on_finish));
    return;
  }

  ScrollOffset clamped_offset = ClampScrollOffset(offset);
  ScrollableArea::SetScrollOffset(clamped_offset, scroll_type, scroll_behavior,
                                  std::move(on_finish));
}

mojom::blink::ScrollBehavior RootFrameViewport::ScrollBehaviorStyle() const {
  return LayoutViewport().ScrollBehaviorStyle();
}

mojom::blink::ColorScheme RootFrameViewport::UsedColorScheme() const {
  return LayoutViewport().UsedColorScheme();
}

ScrollOffset RootFrameViewport::ClampToUserScrollableOffset(
    const ScrollOffset& offset) const {
  ScrollOffset scroll_offset = offset;
  gfx::RectF layout_scrollable = GetUserScrollableRect(LayoutViewport());
  gfx::RectF visual_scrollable = GetUserScrollableRect(GetVisualViewport());
  gfx::RectF user_scrollable(
      layout_scrollable.origin() + visual_scrollable.OffsetFromOrigin(),
      layout_scrollable.size() + visual_scrollable.size());
  scroll_offset.set_x(
      ClampTo(scroll_offset.x(), user_scrollable.x(), user_scrollable.right()));
  scroll_offset.set_y(ClampTo(scroll_offset.y(), user_scrollable.y(),
                              user_scrollable.bottom()));
  return scroll_offset;
}

PhysicalRect RootFrameViewport::ScrollIntoView(
    const PhysicalRect& rect_in_absolute,
    const mojom::blink::ScrollIntoViewParamsPtr& params) {
  PhysicalRect scroll_snapport_rect = VisibleScrollSnapportRect();

  PhysicalRect rect_in_document = rect_in_absolute;
  rect_in_document.Move(
      PhysicalOffset::FromVector2dFFloor(LayoutViewport().GetScrollOffset()));

  ScrollOffset new_scroll_offset =
      ClampScrollOffset(ScrollAlignment::GetScrollOffsetToExpose(
          scroll_snapport_rect, rect_in_document, *params->align_x.get(),
          *params->align_y.get(), GetScrollOffset()));
  if (params->type == mojom::blink::ScrollType::kUser)
    new_scroll_offset = ClampToUserScrollableOffset(new_scroll_offset);

  gfx::PointF end_point = ScrollOffsetToPosition(new_scroll_offset);
  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForEndPosition(end_point, true, true);
  if (GetLayoutBox()) {
    end_point = GetSnapPositionAndSetTarget(*strategy).value_or(end_point);
    new_scroll_offset = ScrollPositionToOffset(end_point);
  }

  if (new_scroll_offset != GetScrollOffset()) {
    if (params->is_for_scroll_sequence) {
      DCHECK(params->type == mojom::blink::ScrollType::kProgrammatic ||
             params->type == mojom::blink::ScrollType::kUser);
      mojom::blink::ScrollBehavior behavior = DetermineScrollBehavior(
          params->behavior, GetLayoutBox()->StyleRef().GetScrollBehavior());
      GetSmoothScrollSequencer()->QueueAnimation(this, new_scroll_offset,
                                                 behavior);
    } else {
      ScrollableArea::SetScrollOffset(new_scroll_offset, params->type);
    }
  }

  // Return the newly moved rect to absolute coordinates.
  // TODO(szager): PaintLayerScrollableArea::ScrollIntoView clips the return
  // value to the visible content rect, but this does not.
  // TODO(bokan): This returns an unchanged rect for scroll sequences (the PLSA
  // version correctly computes what the rect will be when the sequence is
  // executed) and we can't just adjust by `new_scroll_offset` since, to get to
  // absolute coordinates, we must offset by only the layout viewport's scroll.
  rect_in_document.Move(
      -PhysicalOffset::FromVector2dFRound(LayoutViewport().GetScrollOffset()));
  return rect_in_document;
}

void RootFrameViewport::UpdateScrollOffset(
    const ScrollOffset& offset,
    mojom::blink::ScrollType scroll_type) {
  DistributeScrollBetweenViewports(offset, scroll_type,
                                   mojom::blink::ScrollBehavior::kInstant,
                                   kVisualViewport);
}

void RootFrameViewport::DistributeScrollBetweenViewports(
    const ScrollOffset& offset,
    mojom::blink::ScrollType scroll_type,
    mojom::blink::ScrollBehavior behavior,
    ViewportToScrollFirst scroll_first,
    ScrollCallback on_finish) {
  // Make sure we use the scroll offsets as reported by each viewport's
  // ScrollAnimatorBase, since its ScrollableArea's offset may have the
  // fractional part truncated off.
  // TODO(szager): Now that scroll offsets are stored as floats, can we take the
  // scroll offset directly from the ScrollableArea's rather than the animators?
  ScrollOffset old_offset = ScrollOffsetFromScrollAnimators();

  ScrollOffset delta = offset - old_offset;

  if (delta.IsZero()) {
    if (on_finish)
      std::move(on_finish).Run();
    return;
  }

  ScrollableArea& primary =
      scroll_first == kVisualViewport ? GetVisualViewport() : LayoutViewport();
  ScrollableArea& secondary =
      scroll_first == kVisualViewport ? LayoutViewport() : GetVisualViewport();

  // Compute the clamped offsets for both viewports before performing any
  // scrolling since the order of distribution can vary (and is typically
  // visualViewport-first) but, per-spec, if we scroll both viewports the
  // scroll event must be sent to the DOMWindow first, then to the
  // VisualViewport. Thus, we'll always perform the scrolls in that order,
  // regardless of the order of distribution.
  ScrollOffset primary_offset = primary.ClampScrollOffset(
      primary.GetScrollAnimator().CurrentOffset() + delta);
  ScrollOffset unconsumed_by_primary =
      (primary.GetScrollAnimator().CurrentOffset() + delta) - primary_offset;
  ScrollOffset secondary_offset = secondary.ClampScrollOffset(
      secondary.GetScrollAnimator().CurrentOffset() + unconsumed_by_primary);

  auto all_done = on_finish ? base::BarrierClosure(2, std::move(on_finish))
                            : base::RepeatingClosure();

  // DistributeScrollBetweenViewports can be called from SetScrollOffset,
  // so we assume that aborting sequenced smooth scrolls has been handled.
  // It can also be called from inside an animation to set the offset in
  // each frame. In that case, we shouldn't abort sequenced smooth scrolls.

  // Actually apply the scroll the layout viewport first so that the DOM event
  // is dispatched to the DOMWindow before the VisualViewport.
  LayoutViewport().SetScrollOffset(
      scroll_first == kLayoutViewport ? primary_offset : secondary_offset,
      scroll_type, behavior, all_done);
  GetVisualViewport().SetScrollOffset(
      scroll_first == kVisualViewport ? primary_offset : secondary_offset,
      scroll_type, behavior, all_done);
}

gfx::Vector2d RootFrameViewport::ScrollOffsetInt() const {
  return gfx::ToFlooredVector2d(GetScrollOffset());
}

ScrollOffset RootFrameViewport::GetScrollOffset() const {
  return LayoutViewport().GetScrollOffset() +
         GetVisualViewport().GetScrollOffset();
}

gfx::Vector2d RootFrameViewport::MinimumScrollOffsetInt() const {
  return LayoutViewport().MinimumScrollOffsetInt() +
         GetVisualViewport().MinimumScrollOffsetInt();
}

gfx::Vector2d RootFrameViewport::MaximumScrollOffsetInt() const {
  return LayoutViewport().MaximumScrollOffsetInt() +
         GetVisualViewport().MaximumScrollOffsetInt();
}

ScrollOffset RootFrameViewport::MaximumScrollOffset() const {
  return LayoutViewport().MaximumScrollOffset() +
         GetVisualViewport().MaximumScrollOffset();
}

gfx::Size RootFrameViewport::ContentsSize() const {
  return LayoutViewport().ContentsSize();
}

bool RootFrameViewport::UsesCompositedScrolling() const {
  return LayoutViewport().UsesCompositedScrolling();
}

bool RootFrameViewport::ShouldScrollOnMainThread() const {
  return LayoutViewport().ShouldScrollOnMainThread();
}

bool RootFrameViewport::ScrollbarsCanBeActive() const {
  return LayoutViewport().ScrollbarsCanBeActive();
}

bool RootFrameViewport::UserInputScrollable(
    ScrollbarOrientation orientation) const {
  return GetVisualViewport().UserInputScrollable(orientation) ||
         LayoutViewport().UserInputScrollable(orientation);
}

bool RootFrameViewport::ShouldPlaceVerticalScrollbarOnLeft() const {
  return LayoutViewport().ShouldPlaceVerticalScrollbarOnLeft();
}

void RootFrameViewport::ScrollControlWasSetNeedsPaintInvalidation() {
  LayoutViewport().ScrollControlWasSetNeedsPaintInvalidation();
}

cc::Layer* RootFrameViewport::LayerForHorizontalScrollbar() const {
  return LayoutViewport().LayerForHorizontalScrollbar();
}

cc::Layer* RootFrameViewport::LayerForVerticalScrollbar() const {
  return LayoutViewport().LayerForVerticalScrollbar();
}

cc::Layer* RootFrameViewport::LayerForScrollCorner() const {
  return LayoutViewport().LayerForScrollCorner();
}

// This method distributes the scroll between the visual and layout viewport.
ScrollResult RootFrameViewport::UserScroll(
    ui::ScrollGranularity granularity,
    const ScrollOffset& delta,
    ScrollableArea::ScrollCallback on_finish) {
  base::ScopedClosureRunner run_on_return(std::move(on_finish));

  // TODO(bokan/ymalik): Once smooth scrolling is permanently enabled we
  // should be able to remove this method override and use the base class
  // version: ScrollableArea::userScroll.

  UpdateScrollAnimator();

  ScrollOffset pixel_delta = ResolveScrollDelta(granularity, delta);

  // Precompute the amount of possible scrolling since, when animated,
  // ScrollAnimator::userScroll will report having consumed the total given
  // scroll delta, regardless of how much will actually scroll, but we need to
  // know how much to leave for the layout viewport.
  ScrollOffset visual_consumed_delta =
      GetVisualViewport().GetScrollAnimator().ComputeDeltaToConsume(
          pixel_delta);

  // Split the remaining delta between scrollable and unscrollable axes of the
  // layout viewport. We only pass a delta to the scrollable axes and remember
  // how much was held back so we can add it to the unused delta in the
  // result.
  ScrollOffset layout_delta = pixel_delta - visual_consumed_delta;
  ScrollOffset scrollable_axis_delta(
      LayoutViewport().UserInputScrollable(kHorizontalScrollbar)
          ? layout_delta.x()
          : 0,
      LayoutViewport().UserInputScrollable(kVerticalScrollbar)
          ? layout_delta.y()
          : 0);

  // If there won't be any scrolling, bail early so we don't produce any side
  // effects like cancelling existing animations.
  if (visual_consumed_delta.IsZero() && scrollable_axis_delta.IsZero()) {
    return ScrollResult(false, false, pixel_delta.x(), pixel_delta.y());
  }

  CancelProgrammaticScrollAnimation();
  if (SmoothScrollSequencer* sequencer = GetSmoothScrollSequencer())
    sequencer->AbortAnimations();

  // TODO(bokan): Why do we call userScroll on the animators directly and
  // not through the ScrollableAreas?
  if (visual_consumed_delta == pixel_delta) {
    ScrollResult visual_result =
        GetVisualViewport().GetScrollAnimator().UserScroll(
            granularity, visual_consumed_delta, run_on_return.Release());
    return visual_result;
  }

  ScrollableArea::ScrollCallback callback = run_on_return.Release();
  auto all_done = callback ? base::BarrierClosure(2, std::move(callback))
                           : base::RepeatingClosure();
  ScrollResult visual_result =
      GetVisualViewport().GetScrollAnimator().UserScroll(
          granularity, visual_consumed_delta, all_done);

  ScrollResult layout_result = LayoutViewport().GetScrollAnimator().UserScroll(
      granularity, scrollable_axis_delta, all_done);

  // Remember to add any delta not used because of !userInputScrollable to the
  // unusedScrollDelta in the result.
  ScrollOffset unscrollable_axis_delta = layout_delta - scrollable_axis_delta;

  return ScrollResult(
      visual_result.did_scroll_x || layout_result.did_scroll_x,
      visual_result.did_scroll_y || layout_result.did_scroll_y,
      layout_result.unused_scroll_delta_x + unscrollable_axis_delta.x(),
      layout_result.unused_scroll_delta_y + unscrollable_axis_delta.y());
}

bool RootFrameViewport::ScrollAnimatorEnabled() const {
  return LayoutViewport().ScrollAnimatorEnabled();
}

CompositorElementId RootFrameViewport::GetScrollElementId() const {
  return LayoutViewport().GetScrollElementId();
}

CompositorElementId RootFrameViewport::GetScrollbarElementId(
    ScrollbarOrientation orientation) {
  return GetVisualViewport().VisualViewportSuppliesScrollbars()
             ? GetVisualViewport().GetScrollbarElementId(orientation)
             : LayoutViewport().GetScrollbarElementId(orientation);
}

ChromeClient* RootFrameViewport::GetChromeClient() const {
  return LayoutViewport().GetChromeClient();
}

SmoothScrollSequencer* RootFrameViewport::GetSmoothScrollSequencer() const {
  return LayoutViewport().GetSmoothScrollSequencer();
}

void RootFrameViewport::ServiceScrollAnimations(double monotonic_time) {
  ScrollableArea::ServiceScrollAnimations(monotonic_time);
  LayoutViewport().ServiceScrollAnimations(monotonic_time);
  GetVisualViewport().ServiceScrollAnimations(monotonic_time);
}

void RootFrameViewport::UpdateCompositorScrollAnimations() {
  ScrollableArea::UpdateCompositorScrollAnimations();
  LayoutViewport().UpdateCompositorScrollAnimations();
  GetVisualViewport().UpdateCompositorScrollAnimations();
}

void RootFrameViewport::CancelProgrammaticScrollAnimation() {
  ScrollableArea::CancelProgrammaticScrollAnimation();
  LayoutViewport().CancelProgrammaticScrollAnimation();
  GetVisualViewport().CancelProgrammaticScrollAnimation();
}

void RootFrameViewport::ClearScrollableArea() {
  ScrollableArea::ClearScrollableArea();
  LayoutViewport().ClearScrollableArea();
  GetVisualViewport().ClearScrollableArea();
}

ScrollbarTheme& RootFrameViewport::GetPageScrollbarTheme() const {
  return LayoutViewport().GetPageScrollbarTheme();
}

const cc::SnapContainerData* RootFrameViewport::GetSnapContainerData() const {
  return LayoutViewport().GetSnapContainerData();
}

void RootFrameViewport::SetSnapContainerData(
    absl::optional<cc::SnapContainerData> data) {
  LayoutViewport().SetSnapContainerData(data);
}

bool RootFrameViewport::SetTargetSnapAreaElementIds(
    cc::TargetSnapAreaElementIds snap_target_ids) {
  return LayoutViewport().SetTargetSnapAreaElementIds(snap_target_ids);
}

bool RootFrameViewport::SnapContainerDataNeedsUpdate() const {
  return LayoutViewport().SnapContainerDataNeedsUpdate();
}

void RootFrameViewport::SetSnapContainerDataNeedsUpdate(bool needs_update) {
  LayoutViewport().SetSnapContainerDataNeedsUpdate(needs_update);
}

bool RootFrameViewport::NeedsResnap() const {
  return LayoutViewport().NeedsResnap();
}

void RootFrameViewport::SetNeedsResnap(bool needs_resnap) {
  LayoutViewport().SetNeedsResnap(needs_resnap);
}

absl::optional<gfx::PointF> RootFrameViewport::GetSnapPositionAndSetTarget(
    const cc::SnapSelectionStrategy& strategy) {
  return LayoutViewport().GetSnapPositionAndSetTarget(strategy);
}

gfx::PointF RootFrameViewport::ScrollOffsetToPosition(
    const ScrollOffset& offset) const {
  return LayoutViewport().ScrollOffsetToPosition(offset);
}

ScrollOffset RootFrameViewport::ScrollPositionToOffset(
    const gfx::PointF& position) const {
  return LayoutViewport().ScrollPositionToOffset(position);
}

void RootFrameViewport::Trace(Visitor* visitor) const {
  visitor->Trace(visual_viewport_);
  visitor->Trace(layout_viewport_);
  ScrollableArea::Trace(visitor);
}

}  // namespace blink
