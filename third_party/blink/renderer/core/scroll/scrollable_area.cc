/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 * Copyright (C) 2008, 2011 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/scroll/scrollable_area.h"

#include "build/build_config.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/input/scrollbar.h"
#include "cc/input/snap_selection_strategy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/scroll/programmatic_scroll_animator.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/scroll/smooth_scroll_sequencer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink {

int ScrollableArea::PixelsPerLineStep(LocalFrame* frame) {
  if (!frame)
    return kPixelsPerLineStep;
  return frame->GetPage()->GetChromeClient().WindowToViewportScalar(
      frame, kPixelsPerLineStep);
}

float ScrollableArea::MinFractionToStepWhenPaging() {
  return kMinFractionToStepWhenPaging;
}

int ScrollableArea::MaxOverlapBetweenPages() const {
  return GetPageScrollbarTheme().MaxOverlapBetweenPages();
}

// static
ScrollBehavior ScrollableArea::DetermineScrollBehavior(
    ScrollBehavior behavior_from_param,
    ScrollBehavior behavior_from_style) {
  if (behavior_from_param == kScrollBehaviorSmooth)
    return kScrollBehaviorSmooth;

  if (behavior_from_param == kScrollBehaviorAuto &&
      behavior_from_style == kScrollBehaviorSmooth) {
    return kScrollBehaviorSmooth;
  }

  return kScrollBehaviorInstant;
}

ScrollableArea::ScrollableArea()
    : scrollbar_overlay_color_theme_(kScrollbarOverlayColorThemeDark),
      horizontal_scrollbar_needs_paint_invalidation_(false),
      vertical_scrollbar_needs_paint_invalidation_(false),
      scroll_corner_needs_paint_invalidation_(false),
      scrollbars_hidden_if_overlay_(true),
      scrollbar_captured_(false),
      mouse_over_scrollbar_(false),
      has_been_disposed_(false),
      needs_show_scrollbar_layers_(false),
      uses_composited_scrolling_(false) {}

ScrollableArea::~ScrollableArea() = default;

void ScrollableArea::Dispose() {
  if (HasBeenDisposed())
    return;
  DisposeImpl();
  fade_overlay_scrollbars_timer_.reset();
  has_been_disposed_ = true;
}

void ScrollableArea::ClearScrollableArea() {
#if defined(OS_MACOSX)
  if (scroll_animator_)
    scroll_animator_->Dispose();
#endif
  scroll_animator_.Clear();
  programmatic_scroll_animator_.Clear();
  if (fade_overlay_scrollbars_timer_)
    fade_overlay_scrollbars_timer_->Stop();
}

ScrollAnimatorBase& ScrollableArea::GetScrollAnimator() const {
  if (!scroll_animator_)
    scroll_animator_ =
        ScrollAnimatorBase::Create(const_cast<ScrollableArea*>(this));

  return *scroll_animator_;
}

ProgrammaticScrollAnimator& ScrollableArea::GetProgrammaticScrollAnimator()
    const {
  if (!programmatic_scroll_animator_) {
    programmatic_scroll_animator_ =
        MakeGarbageCollected<ProgrammaticScrollAnimator>(
            const_cast<ScrollableArea*>(this));
  }

  return *programmatic_scroll_animator_;
}

ScrollbarOrientation ScrollableArea::ScrollbarOrientationFromDirection(
    ScrollDirectionPhysical direction) const {
  return (direction == kScrollUp || direction == kScrollDown)
             ? kVerticalScrollbar
             : kHorizontalScrollbar;
}

float ScrollableArea::ScrollStep(ScrollGranularity granularity,
                                 ScrollbarOrientation orientation) const {
  switch (granularity) {
    case ScrollGranularity::kScrollByLine:
      return LineStep(orientation);
    case ScrollGranularity::kScrollByPage:
      return PageStep(orientation);
    case ScrollGranularity::kScrollByDocument:
      return DocumentStep(orientation);
    case ScrollGranularity::kScrollByPixel:
    case ScrollGranularity::kScrollByPrecisePixel:
      return PixelStep(orientation);
    default:
      NOTREACHED();
      return 0.0f;
  }
}

ScrollResult ScrollableArea::UserScroll(ScrollGranularity granularity,
                                        const ScrollOffset& delta,
                                        ScrollCallback on_finish) {
  TRACE_EVENT2("input", "ScrollableArea::UserScroll", "x", delta.Width(), "y",
               delta.Height());

  if (on_finish)
    RegisterScrollCompleteCallback(std::move(on_finish));

  base::ScopedClosureRunner run_on_return(WTF::Bind(
      &ScrollableArea::RunScrollCompleteCallbacks, WrapWeakPersistent(this)));

  float step_x = ScrollStep(granularity, kHorizontalScrollbar);
  float step_y = ScrollStep(granularity, kVerticalScrollbar);

  ScrollOffset pixel_delta(delta);
  pixel_delta.Scale(step_x, step_y);

  ScrollOffset scrollable_axis_delta(
      UserInputScrollable(kHorizontalScrollbar) ? pixel_delta.Width() : 0,
      UserInputScrollable(kVerticalScrollbar) ? pixel_delta.Height() : 0);

  if (scrollable_axis_delta.IsZero()) {
    return ScrollResult(false, false, pixel_delta.Width(),
                        pixel_delta.Height());
  }

  CancelProgrammaticScrollAnimation();
  if (SmoothScrollSequencer* sequencer = GetSmoothScrollSequencer())
    sequencer->AbortAnimations();

  ScrollResult result = GetScrollAnimator().UserScroll(
      granularity, scrollable_axis_delta, run_on_return.Release());

  // Delta that wasn't scrolled because the axis is !userInputScrollable
  // should count as unusedScrollDelta.
  ScrollOffset unscrollable_axis_delta = pixel_delta - scrollable_axis_delta;
  result.unused_scroll_delta_x += unscrollable_axis_delta.Width();
  result.unused_scroll_delta_y += unscrollable_axis_delta.Height();

  return result;
}

void ScrollableArea::SetScrollOffset(const ScrollOffset& offset,
                                     ScrollType scroll_type,
                                     ScrollBehavior behavior,
                                     ScrollCallback on_finish) {
  if (on_finish)
    RegisterScrollCompleteCallback(std::move(on_finish));

  base::ScopedClosureRunner run_on_return(WTF::Bind(
      &ScrollableArea::RunScrollCompleteCallbacks, WrapWeakPersistent(this)));

  if (SmoothScrollSequencer* sequencer = GetSmoothScrollSequencer()) {
    if (sequencer->FilterNewScrollOrAbortCurrent(scroll_type)) {
      return;
    }
  }

  ScrollOffset clamped_offset = ClampScrollOffset(offset);
  if (clamped_offset == GetScrollOffset()) {
    return;
  }

  TRACE_EVENT2("blink", "ScrollableArea::SetScrollOffset", "x", offset.Width(),
               "y", offset.Height());
  TRACE_EVENT_INSTANT1("blink", "Type", TRACE_EVENT_SCOPE_THREAD, "type",
                       scroll_type);
  TRACE_EVENT_INSTANT1("blink", "Behavior", TRACE_EVENT_SCOPE_THREAD,
                       "behavior", behavior);

  if (behavior == kScrollBehaviorAuto)
    behavior = ScrollBehaviorStyle();

  switch (scroll_type) {
    case kCompositorScroll:
    case kClampingScroll:
      ScrollOffsetChanged(clamped_offset, scroll_type);
      break;
    case kAnchoringScroll:
      GetScrollAnimator().AdjustAnimationAndSetScrollOffset(clamped_offset,
                                                            scroll_type);
      break;
    case kProgrammaticScroll:
      ProgrammaticScrollHelper(clamped_offset, behavior, false,
                               run_on_return.Release());
      break;
    case kSequencedScroll:
      ProgrammaticScrollHelper(clamped_offset, behavior, true,
                               run_on_return.Release());
      break;
    case kUserScroll:
      UserScrollHelper(clamped_offset, behavior);
      break;
    default:
      NOTREACHED();
  }
}

void ScrollableArea::SetScrollOffset(const ScrollOffset& offset,
                                     ScrollType type,
                                     ScrollBehavior behavior) {
  SetScrollOffset(offset, type, behavior, ScrollCallback());
}

void ScrollableArea::ScrollBy(const ScrollOffset& delta,
                              ScrollType type,
                              ScrollBehavior behavior) {
  SetScrollOffset(GetScrollOffset() + delta, type, behavior);
}

void ScrollableArea::SetScrollOffsetSingleAxis(ScrollbarOrientation orientation,
                                               float offset,
                                               ScrollType scroll_type,
                                               ScrollBehavior behavior) {
  ScrollOffset new_offset;
  if (orientation == kHorizontalScrollbar)
    new_offset =
        ScrollOffset(offset, GetScrollAnimator().CurrentOffset().Height());
  else
    new_offset =
        ScrollOffset(GetScrollAnimator().CurrentOffset().Width(), offset);

  // TODO(bokan): Note, this doesn't use the derived class versions since this
  // method is currently used exclusively by code that adjusts the position by
  // the scroll origin and the derived class versions differ on whether they
  // take that into account or not.
  ScrollableArea::SetScrollOffset(new_offset, scroll_type, behavior);
}

void ScrollableArea::ProgrammaticScrollHelper(const ScrollOffset& offset,
                                              ScrollBehavior scroll_behavior,
                                              bool is_sequenced_scroll,
                                              ScrollCallback on_finish) {
  CancelScrollAnimation();

  ScrollCallback callback = std::move(on_finish);
  if (RuntimeEnabledFeatures::UpdateHoverAtBeginFrameEnabled() ||
      RuntimeEnabledFeatures::OverscrollCustomizationEnabled()) {
    callback = ScrollCallback(WTF::Bind(
        [](ScrollCallback original_callback,
           WeakPersistent<ScrollableArea> area) {
          if (area)
            area->OnScrollFinished();
          if (original_callback)
            std::move(original_callback).Run();
        },
        std::move(callback), WrapWeakPersistent(this)));
  }

  if (scroll_behavior == kScrollBehaviorSmooth) {
    GetProgrammaticScrollAnimator().AnimateToOffset(offset, is_sequenced_scroll,
                                                    std::move(callback));
  } else {
    GetProgrammaticScrollAnimator().ScrollToOffsetWithoutAnimation(
        offset, is_sequenced_scroll);
    if (callback)
      std::move(callback).Run();
  }
}

void ScrollableArea::UserScrollHelper(const ScrollOffset& offset,
                                      ScrollBehavior scroll_behavior) {
  CancelProgrammaticScrollAnimation();
  if (SmoothScrollSequencer* sequencer = GetSmoothScrollSequencer())
    sequencer->AbortAnimations();

  float x = UserInputScrollable(kHorizontalScrollbar)
                ? offset.Width()
                : GetScrollAnimator().CurrentOffset().Width();
  float y = UserInputScrollable(kVerticalScrollbar)
                ? offset.Height()
                : GetScrollAnimator().CurrentOffset().Height();

  // Smooth user scrolls (keyboard, wheel clicks) are handled via the userScroll
  // method.
  // TODO(bokan): The userScroll method should probably be modified to call this
  //              method and ScrollAnimatorBase to have a simpler
  //              animateToOffset method like the ProgrammaticScrollAnimator.
  DCHECK_EQ(scroll_behavior, kScrollBehaviorInstant);
  GetScrollAnimator().ScrollToOffsetWithoutAnimation(ScrollOffset(x, y));
}

PhysicalRect ScrollableArea::ScrollIntoView(
    const PhysicalRect& rect_in_absolute,
    const WebScrollIntoViewParams& params) {
  // TODO(bokan): This should really be implemented here but ScrollAlignment is
  // in Core which is a dependency violation.
  NOTREACHED();
  return PhysicalRect();
}

void ScrollableArea::ScrollOffsetChanged(const ScrollOffset& offset,
                                         ScrollType scroll_type) {
  TRACE_EVENT0("blink", "ScrollableArea::scrollOffsetChanged");

  ScrollOffset old_offset = GetScrollOffset();
  ScrollOffset truncated_offset = ShouldUseIntegerScrollOffset()
                                      ? ScrollOffset(FlooredIntSize(offset))
                                      : offset;

  // Tell the derived class to scroll its contents.
  UpdateScrollOffset(truncated_offset, scroll_type);

  // If the layout object has been detached as a result of updating the scroll
  // this object will be cleaned up shortly.
  if (HasBeenDisposed())
    return;

  // Tell the scrollbars to update their thumb postions.
  // If the scrollbar does not have its own layer, it must always be
  // invalidated to reflect the new thumb offset, even if the theme did not
  // invalidate any individual part.
  if (Scrollbar* horizontal_scrollbar = this->HorizontalScrollbar())
    horizontal_scrollbar->OffsetDidChange();
  if (Scrollbar* vertical_scrollbar = this->VerticalScrollbar())
    vertical_scrollbar->OffsetDidChange();

  ScrollOffset delta = GetScrollOffset() - old_offset;
  // TODO(skobes): Should we exit sooner when the offset has not changed?
  bool offset_changed = !delta.IsZero();

  if (offset_changed) {
    GetScrollAnimator().NotifyContentAreaScrolled(
        GetScrollOffset() - old_offset, scroll_type);
  }

  if (GetLayoutBox()) {
    if (offset_changed && GetLayoutBox()->GetFrameView() &&
        GetLayoutBox()
            ->GetFrameView()
            ->GetPaintTimingDetector()
            .NeedToNotifyInputOrScroll()) {
      GetLayoutBox()->GetFrameView()->GetPaintTimingDetector().NotifyScroll(
          scroll_type);
    }
  }

  if (offset_changed && GetLayoutBox() && GetLayoutBox()->GetFrameView()) {
    GetLayoutBox()->GetFrameView()->GetLayoutShiftTracker().NotifyScroll(
        scroll_type, delta);
  }

  GetScrollAnimator().SetCurrentOffset(offset);
}

bool ScrollableArea::ScrollBehaviorFromString(const String& behavior_string,
                                              ScrollBehavior& behavior) {
  if (behavior_string == "auto")
    behavior = kScrollBehaviorAuto;
  else if (behavior_string == "instant")
    behavior = kScrollBehaviorInstant;
  else if (behavior_string == "smooth")
    behavior = kScrollBehaviorSmooth;
  else
    return false;

  return true;
}

// NOTE: Only called from Internals for testing.
void ScrollableArea::UpdateScrollOffsetFromInternals(const IntSize& offset) {
  ScrollOffsetChanged(ScrollOffset(offset), kProgrammaticScroll);
}

void ScrollableArea::RegisterScrollCompleteCallback(ScrollCallback callback) {
  DCHECK(!HasBeenDisposed());
  pending_scroll_complete_callbacks_.push_back(std::move(callback));
}

void ScrollableArea::RunScrollCompleteCallbacks() {
  Vector<ScrollCallback> callbacks(
      std::move(pending_scroll_complete_callbacks_));
  for (auto& callback : callbacks)
    std::move(callback).Run();
}

void ScrollableArea::ContentAreaWillPaint() const {
  if (ScrollAnimatorBase* scroll_animator = ExistingScrollAnimator())
    scroll_animator->ContentAreaWillPaint();
}

void ScrollableArea::MouseEnteredContentArea() const {
  if (ScrollAnimatorBase* scroll_animator = ExistingScrollAnimator())
    scroll_animator->MouseEnteredContentArea();
}

void ScrollableArea::MouseExitedContentArea() const {
  if (ScrollAnimatorBase* scroll_animator = ExistingScrollAnimator())
    scroll_animator->MouseExitedContentArea();
}

void ScrollableArea::MouseMovedInContentArea() const {
  if (ScrollAnimatorBase* scroll_animator = ExistingScrollAnimator())
    scroll_animator->MouseMovedInContentArea();
}

void ScrollableArea::MouseEnteredScrollbar(Scrollbar& scrollbar) {
  mouse_over_scrollbar_ = true;
  GetScrollAnimator().MouseEnteredScrollbar(scrollbar);
  ShowOverlayScrollbars();
  if (fade_overlay_scrollbars_timer_)
    fade_overlay_scrollbars_timer_->Stop();
}

void ScrollableArea::MouseExitedScrollbar(Scrollbar& scrollbar) {
  mouse_over_scrollbar_ = false;
  GetScrollAnimator().MouseExitedScrollbar(scrollbar);
  if (HasOverlayScrollbars() && !scrollbars_hidden_if_overlay_) {
    // This will kick off the fade out timer.
    ShowOverlayScrollbars();
  }
}

void ScrollableArea::MouseCapturedScrollbar() {
  scrollbar_captured_ = true;
  ShowOverlayScrollbars();
  if (fade_overlay_scrollbars_timer_)
    fade_overlay_scrollbars_timer_->Stop();
}

void ScrollableArea::MouseReleasedScrollbar() {
  scrollbar_captured_ = false;
  // This will kick off the fade out timer.
  ShowOverlayScrollbars();
}

void ScrollableArea::ContentAreaDidShow() const {
  if (ScrollAnimatorBase* scroll_animator = ExistingScrollAnimator())
    scroll_animator->ContentAreaDidShow();
}

void ScrollableArea::ContentAreaDidHide() const {
  if (ScrollAnimatorBase* scroll_animator = ExistingScrollAnimator())
    scroll_animator->ContentAreaDidHide();
}

void ScrollableArea::FinishCurrentScrollAnimations() const {
  if (ScrollAnimatorBase* scroll_animator = ExistingScrollAnimator())
    scroll_animator->FinishCurrentScrollAnimations();
}

void ScrollableArea::DidAddScrollbar(Scrollbar& scrollbar,
                                     ScrollbarOrientation orientation) {
  if (orientation == kVerticalScrollbar)
    GetScrollAnimator().DidAddVerticalScrollbar(scrollbar);
  else
    GetScrollAnimator().DidAddHorizontalScrollbar(scrollbar);

  // <rdar://problem/9797253> AppKit resets the scrollbar's style when you
  // attach a scrollbar
  SetScrollbarOverlayColorTheme(GetScrollbarOverlayColorTheme());
}

void ScrollableArea::WillRemoveScrollbar(Scrollbar& scrollbar,
                                         ScrollbarOrientation orientation) {
  if (ScrollAnimatorBase* scroll_animator = ExistingScrollAnimator()) {
    if (orientation == kVerticalScrollbar)
      scroll_animator->WillRemoveVerticalScrollbar(scrollbar);
    else
      scroll_animator->WillRemoveHorizontalScrollbar(scrollbar);
  }
}

void ScrollableArea::ContentsResized() {
  if (ScrollAnimatorBase* scroll_animator = ExistingScrollAnimator())
    scroll_animator->ContentsResized();
}

bool ScrollableArea::HasOverlayScrollbars() const {
  Scrollbar* v_scrollbar = VerticalScrollbar();
  if (v_scrollbar && v_scrollbar->IsOverlayScrollbar())
    return true;
  Scrollbar* h_scrollbar = HorizontalScrollbar();
  return h_scrollbar && h_scrollbar->IsOverlayScrollbar();
}

void ScrollableArea::SetScrollbarOverlayColorTheme(
    ScrollbarOverlayColorTheme overlay_theme) {
  scrollbar_overlay_color_theme_ = overlay_theme;

  if (Scrollbar* scrollbar = HorizontalScrollbar()) {
    GetPageScrollbarTheme().UpdateScrollbarOverlayColorTheme(*scrollbar);
    scrollbar->SetNeedsPaintInvalidation(kAllParts);
  }

  if (Scrollbar* scrollbar = VerticalScrollbar()) {
    GetPageScrollbarTheme().UpdateScrollbarOverlayColorTheme(*scrollbar);
    scrollbar->SetNeedsPaintInvalidation(kAllParts);
  }
}

void ScrollableArea::RecalculateScrollbarOverlayColorTheme(
    Color background_color) {
  ScrollbarOverlayColorTheme old_overlay_theme =
      GetScrollbarOverlayColorTheme();
  ScrollbarOverlayColorTheme overlay_theme = kScrollbarOverlayColorThemeDark;

  // Reduce the background color from RGB to a lightness value
  // and determine which scrollbar style to use based on a lightness
  // heuristic.
  double hue, saturation, lightness;
  background_color.GetHSL(hue, saturation, lightness);
  if (lightness <= .5 && background_color.Alpha())
    overlay_theme = kScrollbarOverlayColorThemeLight;

  if (old_overlay_theme != overlay_theme)
    SetScrollbarOverlayColorTheme(overlay_theme);
}

void ScrollableArea::SetScrollbarNeedsPaintInvalidation(
    ScrollbarOrientation orientation) {
  if (orientation == kHorizontalScrollbar) {
    if (cc::Layer* layer = LayerForHorizontalScrollbar())
      layer->SetNeedsDisplay();
    horizontal_scrollbar_needs_paint_invalidation_ = true;
  } else {
    if (cc::Layer* layer = LayerForVerticalScrollbar())
      layer->SetNeedsDisplay();
    vertical_scrollbar_needs_paint_invalidation_ = true;
  }

  ScrollControlWasSetNeedsPaintInvalidation();
}

void ScrollableArea::SetScrollCornerNeedsPaintInvalidation() {
  if (cc::Layer* layer = LayerForScrollCorner()) {
    layer->SetNeedsDisplay();
    return;
  }
  scroll_corner_needs_paint_invalidation_ = true;
  ScrollControlWasSetNeedsPaintInvalidation();
}

bool ScrollableArea::HasLayerForHorizontalScrollbar() const {
  return LayerForHorizontalScrollbar();
}

bool ScrollableArea::HasLayerForVerticalScrollbar() const {
  return LayerForVerticalScrollbar();
}

bool ScrollableArea::HasLayerForScrollCorner() const {
  return LayerForScrollCorner();
}

void ScrollableArea::LayerForScrollingDidChange(
    CompositorAnimationTimeline* timeline) {
  if (ProgrammaticScrollAnimator* programmatic_scroll_animator =
          ExistingProgrammaticScrollAnimator())
    programmatic_scroll_animator->LayerForCompositedScrollingDidChange(
        timeline);
  if (ScrollAnimatorBase* scroll_animator = ExistingScrollAnimator())
    scroll_animator->LayerForCompositedScrollingDidChange(timeline);
}

void ScrollableArea::ServiceScrollAnimations(double monotonic_time) {
  bool requires_animation_service = false;
  if (ScrollAnimatorBase* scroll_animator = ExistingScrollAnimator()) {
    scroll_animator->TickAnimation(monotonic_time);
    if (scroll_animator->HasAnimationThatRequiresService())
      requires_animation_service = true;
  }
  if (ProgrammaticScrollAnimator* programmatic_scroll_animator =
          ExistingProgrammaticScrollAnimator()) {
    programmatic_scroll_animator->TickAnimation(monotonic_time);
    if (programmatic_scroll_animator->HasAnimationThatRequiresService())
      requires_animation_service = true;
  }
  if (!requires_animation_service)
    DeregisterForAnimation();
}

void ScrollableArea::UpdateCompositorScrollAnimations() {
  if (ProgrammaticScrollAnimator* programmatic_scroll_animator =
          ExistingProgrammaticScrollAnimator())
    programmatic_scroll_animator->UpdateCompositorAnimations();

  if (ScrollAnimatorBase* scroll_animator = ExistingScrollAnimator())
    scroll_animator->UpdateCompositorAnimations();
}

void ScrollableArea::CancelScrollAnimation() {
  if (ScrollAnimatorBase* scroll_animator = ExistingScrollAnimator())
    scroll_animator->CancelAnimation();
}

void ScrollableArea::CancelProgrammaticScrollAnimation() {
  if (ProgrammaticScrollAnimator* programmatic_scroll_animator =
          ExistingProgrammaticScrollAnimator())
    programmatic_scroll_animator->CancelAnimation();
}

bool ScrollableArea::ScrollbarsHiddenIfOverlay() const {
  return HasOverlayScrollbars() && scrollbars_hidden_if_overlay_;
}

void ScrollableArea::SetScrollbarsHiddenIfOverlay(bool hidden) {
  // If scrollable area has been disposed, we can not get the page scrollbar
  // theme setting. Should early return here.
  if (HasBeenDisposed())
    return;

  if (!GetPageScrollbarTheme().UsesOverlayScrollbars())
    return;

  if (scrollbars_hidden_if_overlay_ == static_cast<unsigned>(hidden))
    return;

  scrollbars_hidden_if_overlay_ = hidden;
  ScrollbarVisibilityChanged();
}

void ScrollableArea::FadeOverlayScrollbarsTimerFired(TimerBase*) {
  SetScrollbarsHiddenIfOverlay(true);
}

void ScrollableArea::ShowOverlayScrollbars() {
  if (!GetPageScrollbarTheme().UsesOverlayScrollbars())
    return;

  SetScrollbarsHiddenIfOverlay(false);
  needs_show_scrollbar_layers_ = true;

  const base::TimeDelta time_until_disable =
      GetPageScrollbarTheme().OverlayScrollbarFadeOutDelay() +
      GetPageScrollbarTheme().OverlayScrollbarFadeOutDuration();

  // If the overlay scrollbars don't fade out, don't do anything. This is the
  // case for the mock overlays used in tests and on Mac, where the fade-out is
  // animated in ScrollAnimatorMac.
  // We also don't fade out overlay scrollbar for popup since we don't create
  // compositor for popup and thus they don't appear on hover so users without
  // a wheel can't scroll if they fade out.
  if (time_until_disable.is_zero() || GetChromeClient()->IsPopup())
    return;

  if (!fade_overlay_scrollbars_timer_) {
    fade_overlay_scrollbars_timer_.reset(new TaskRunnerTimer<ScrollableArea>(
        ThreadScheduler::Current()->CompositorTaskRunner(), this,
        &ScrollableArea::FadeOverlayScrollbarsTimerFired));
  }

  if (!scrollbar_captured_ && !mouse_over_scrollbar_) {
    fade_overlay_scrollbars_timer_->StartOneShot(time_until_disable, FROM_HERE);
  }
}

const Document* ScrollableArea::GetDocument() const {
  return &GetLayoutBox()->GetDocument();
}

IntSize ScrollableArea::ClampScrollOffset(const IntSize& scroll_offset) const {
  return scroll_offset.ShrunkTo(MaximumScrollOffsetInt())
      .ExpandedTo(MinimumScrollOffsetInt());
}

ScrollOffset ScrollableArea::ClampScrollOffset(
    const ScrollOffset& scroll_offset) const {
  return scroll_offset.ShrunkTo(MaximumScrollOffset())
      .ExpandedTo(MinimumScrollOffset());
}

int ScrollableArea::LineStep(ScrollbarOrientation) const {
  return PixelsPerLineStep(GetLayoutBox()->GetFrame());
}

int ScrollableArea::PageStep(ScrollbarOrientation orientation) const {
  // Paging scroll operations should take scroll-padding into account [1]. So we
  // use the snapport rect to calculate the page step instead of the visible
  // rect.
  // [1] https://drafts.csswg.org/css-scroll-snap/#scroll-padding
  IntSize snapport_size =
      VisibleScrollSnapportRect(kIncludeScrollbars).PixelSnappedSize();
  int length = (orientation == kHorizontalScrollbar) ? snapport_size.Width()
                                                     : snapport_size.Height();
  int min_page_step =
      static_cast<float>(length) * MinFractionToStepWhenPaging();
  int page_step = std::max(min_page_step, length - MaxOverlapBetweenPages());

  return std::max(page_step, 1);
}

int ScrollableArea::DocumentStep(ScrollbarOrientation orientation) const {
  return ScrollSize(orientation);
}

float ScrollableArea::PixelStep(ScrollbarOrientation) const {
  return 1;
}

int ScrollableArea::VerticalScrollbarWidth(
    OverlayScrollbarClipBehavior behavior) const {
  DCHECK_EQ(behavior, kIgnorePlatformOverlayScrollbarSize);
  if (Scrollbar* vertical_bar = VerticalScrollbar())
    return !vertical_bar->IsOverlayScrollbar() ? vertical_bar->Width() : 0;
  return 0;
}

int ScrollableArea::HorizontalScrollbarHeight(
    OverlayScrollbarClipBehavior behavior) const {
  DCHECK_EQ(behavior, kIgnorePlatformOverlayScrollbarSize);
  if (Scrollbar* horizontal_bar = HorizontalScrollbar())
    return !horizontal_bar->IsOverlayScrollbar() ? horizontal_bar->Height() : 0;
  return 0;
}

FloatQuad ScrollableArea::LocalToVisibleContentQuad(const FloatQuad& quad,
                                                    const LayoutObject*,
                                                    unsigned) const {
  FloatQuad result(quad);
  result.Move(-GetScrollOffset());
  return result;
}

IntSize ScrollableArea::ExcludeScrollbars(const IntSize& size) const {
  return IntSize(std::max(0, size.Width() - VerticalScrollbarWidth()),
                 std::max(0, size.Height() - HorizontalScrollbarHeight()));
}

void ScrollableArea::DidScroll(const FloatPoint& position) {
  ScrollOffset new_offset(ScrollPositionToOffset(position));
  SetScrollOffset(new_offset, kCompositorScroll);
}

CompositorElementId ScrollableArea::GetScrollbarElementId(
    ScrollbarOrientation orientation) {
  CompositorElementId scrollable_element_id = GetCompositorElementId();
  DCHECK(scrollable_element_id);
  CompositorElementIdNamespace element_id_namespace =
      orientation == kHorizontalScrollbar
          ? CompositorElementIdNamespace::kHorizontalScrollbar
          : CompositorElementIdNamespace::kVerticalScrollbar;
  return CompositorElementIdFromUniqueObjectId(
      scrollable_element_id.GetStableId(), element_id_namespace);
}

void ScrollableArea::OnScrollFinished() {
  if (GetLayoutBox()) {
    if (RuntimeEnabledFeatures::OverscrollCustomizationEnabled()) {
      if (Node* node = GetLayoutBox()->GetNode())
        node->GetDocument().EnqueueScrollEndEventForNode(node);
    }
    if (RuntimeEnabledFeatures::UpdateHoverAtBeginFrameEnabled()) {
      GetLayoutBox()
          ->GetFrame()
          ->LocalFrameRoot()
          .GetEventHandler()
          .MarkHoverStateDirty();
    }
  }
}

void ScrollableArea::SnapAfterScrollbarScrolling(
    ScrollbarOrientation orientation) {
  SnapAtCurrentPosition(orientation == kHorizontalScrollbar,
                        orientation == kVerticalScrollbar);
}

bool ScrollableArea::SnapAtCurrentPosition(
    bool scrolled_x,
    bool scrolled_y,
    base::ScopedClosureRunner on_finish) {
  DCHECK(IsRootFrameViewport() || !GetLayoutBox()->IsGlobalRootScroller());
  FloatPoint current_position = ScrollPosition();
  return SnapForEndPosition(current_position, scrolled_x, scrolled_y,
                            std::move(on_finish));
}

bool ScrollableArea::SnapForEndPosition(const FloatPoint& end_position,
                                        bool scrolled_x,
                                        bool scrolled_y,
                                        base::ScopedClosureRunner on_finish) {
  DCHECK(IsRootFrameViewport() || !GetLayoutBox()->IsGlobalRootScroller());
  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForEndPosition(
          gfx::ScrollOffset(end_position), scrolled_x, scrolled_y);
  return PerformSnapping(*strategy, std::move(on_finish));
}

bool ScrollableArea::SnapForDirection(const ScrollOffset& delta,
                                      base::ScopedClosureRunner on_finish) {
  DCHECK(IsRootFrameViewport() || !GetLayoutBox()->IsGlobalRootScroller());
  FloatPoint current_position = ScrollPosition();
  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForDirection(
          gfx::ScrollOffset(current_position),
          gfx::ScrollOffset(delta.Width(), delta.Height()));
  return PerformSnapping(*strategy, std::move(on_finish));
}

bool ScrollableArea::SnapForEndAndDirection(const ScrollOffset& delta) {
  DCHECK(IsRootFrameViewport() || !GetLayoutBox()->IsGlobalRootScroller());
  FloatPoint current_position = ScrollPosition();
  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForEndAndDirection(
          gfx::ScrollOffset(current_position),
          gfx::ScrollOffset(delta.Width(), delta.Height()));
  return PerformSnapping(*strategy);
}

bool ScrollableArea::PerformSnapping(const cc::SnapSelectionStrategy& strategy,
                                     base::ScopedClosureRunner on_finish) {
  base::Optional<FloatPoint> snap_point = GetSnapPositionAndSetTarget(strategy);
  if (!snap_point)
    return false;
  CancelScrollAnimation();
  CancelProgrammaticScrollAnimation();
  SetScrollOffset(ScrollPositionToOffset(snap_point.value()),
                  kProgrammaticScroll, kScrollBehaviorSmooth,
                  on_finish.Release());
  return true;
}

void ScrollableArea::Trace(blink::Visitor* visitor) {
  visitor->Trace(scroll_animator_);
  visitor->Trace(programmatic_scroll_animator_);
}

void ScrollableArea::InjectGestureScrollEvent(
    WebGestureDevice device,
    ScrollOffset delta,
    ScrollGranularity granularity,
    WebInputEvent::Type gesture_type) const {
  // All ScrollableArea's have a layout box, except for the VisualViewport.
  // We shouldn't be injecting scrolls for the visual viewport scrollbar, since
  // it is not hit-testable.
  DCHECK(GetLayoutBox());
  GetChromeClient()->InjectGestureScrollEvent(
      *GetLayoutBox()->GetFrame(), device, delta, granularity,
      GetCompositorElementId(), gesture_type);
}

ScrollableArea* ScrollableArea::GetForScrolling(const LayoutBox* layout_box) {
  if (!layout_box)
    return nullptr;

  if (!layout_box->IsGlobalRootScroller())
    return layout_box->GetScrollableArea();

  // The global root scroller should be scrolled by the root frame view's
  // ScrollableArea.
  LocalFrame& root_frame = layout_box->GetFrame()->LocalFrameRoot();
  return root_frame.View()->GetScrollableArea();
}

}  // namespace blink
