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

#include <limits>

#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/input/scroll_snap_data.h"
#include "cc/input/scroll_utils.h"
#include "cc/input/scrollbar.h"
#include "cc/input/snap_selection_strategy.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink-forward.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_group_pseudo_element.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/anchor_element_viewport_position_tracker.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/scroll/mac_scrollbar_animator.h"
#include "third_party/blink/renderer/core/scroll/programmatic_scroll_animator.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace blink {

int ScrollableArea::PixelsPerLineStep(LocalFrame* frame) {
  if (!frame)
    return cc::kPixelsPerLineStep;
  return frame->GetPage()->GetChromeClient().WindowToViewportScalar(
      frame, cc::kPixelsPerLineStep);
}

// static
mojom::blink::ScrollBehavior ScrollableArea::DetermineScrollBehavior(
    mojom::blink::ScrollBehavior behavior_from_param,
    mojom::blink::ScrollBehavior behavior_from_style) {
  if (behavior_from_param == mojom::blink::ScrollBehavior::kSmooth)
    return mojom::blink::ScrollBehavior::kSmooth;

  if (behavior_from_param == mojom::blink::ScrollBehavior::kAuto &&
      behavior_from_style == mojom::blink::ScrollBehavior::kSmooth) {
    return mojom::blink::ScrollBehavior::kSmooth;
  }

  return mojom::blink::ScrollBehavior::kInstant;
}

ScrollableArea::ScrollableArea(
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner)
    : overlay_scrollbar_color_scheme__(
          static_cast<unsigned>(mojom::blink::ColorScheme::kLight)),
      horizontal_scrollbar_needs_paint_invalidation_(false),
      vertical_scrollbar_needs_paint_invalidation_(false),
      scroll_corner_needs_paint_invalidation_(false),
      scrollbars_hidden_if_overlay_(true),
      scrollbar_captured_(false),
      mouse_over_scrollbar_(false),
      has_been_disposed_(false),
      compositor_task_runner_(std::move(compositor_task_runner)) {
  DCHECK(compositor_task_runner_);
}

ScrollableArea::~ScrollableArea() = default;

void ScrollableArea::Dispose() {
  if (HasBeenDisposed())
    return;
  DisposeImpl();
  fade_overlay_scrollbars_timer_ = nullptr;
  has_been_disposed_ = true;
}

void ScrollableArea::ClearScrollableArea() {
  if (mac_scrollbar_animator_)
    mac_scrollbar_animator_->Dispose();
  if (scroll_animator_) {
    scroll_animator_->DetachElement();
    scroll_animator_.Clear();
  }
  if (programmatic_scroll_animator_) {
    programmatic_scroll_animator_->DetachElement();
    programmatic_scroll_animator_.Clear();
  }
  if (fade_overlay_scrollbars_timer_)
    fade_overlay_scrollbars_timer_->Value().Stop();
}

MacScrollbarAnimator* ScrollableArea::GetMacScrollbarAnimator() const {
#if BUILDFLAG(IS_MAC)
  if (!mac_scrollbar_animator_) {
    mac_scrollbar_animator_ =
        MacScrollbarAnimator::Create(const_cast<ScrollableArea*>(this));
  }
#endif
  return mac_scrollbar_animator_.Get();
}

void ScrollableArea::SetMacScrollbarAnimatorForTesting(
    MacScrollbarAnimator* animator_for_testing) {
  mac_scrollbar_animator_ = animator_for_testing;
}

bool ScrollableArea::FadeInScrollbarIfExists(bool horizontal, bool vertical) {
  if (GetMacScrollbarAnimator()) {
    return GetMacScrollbarAnimator()->FadeInScrollbarIfExists(horizontal,
                                                              vertical);
  }
  return false;
}

ScrollAnimatorBase& ScrollableArea::GetScrollAnimator() const {
  if (!scroll_animator_) {
    scroll_animator_ =
        ScrollAnimatorBase::Create(const_cast<ScrollableArea*>(this));
  }

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

float ScrollableArea::ScrollStep(ui::ScrollGranularity granularity,
                                 ScrollbarOrientation orientation) const {
  switch (granularity) {
    case ui::ScrollGranularity::kScrollByLine:
      return LineStep(orientation);
    case ui::ScrollGranularity::kScrollByPage:
      return PageStep(orientation);
    case ui::ScrollGranularity::kScrollByDocument:
      return DocumentStep(orientation);
    case ui::ScrollGranularity::kScrollByPixel:
    case ui::ScrollGranularity::kScrollByPrecisePixel:
      return PixelStep(orientation);
    default:
      NOTREACHED();
  }
}

ScrollOffset ScrollableArea::ResolveScrollDelta(
    ui::ScrollGranularity granularity,
    const ScrollOffset& delta) {
  gfx::SizeF step(ScrollStep(granularity, kHorizontalScrollbar),
                  ScrollStep(granularity, kVerticalScrollbar));
  return gfx::ScaleVector2d(delta, step.width(), step.height());
}

ScrollResult ScrollableArea::UserScroll(ui::ScrollGranularity granularity,
                                        const ScrollOffset& delta,
                                        cc::ScrollSourceType source_type,
                                        ScrollCallback on_finish) {
  TRACE_EVENT2("input", "ScrollableArea::UserScroll", "x", delta.x(), "y",
               delta.y());

  // This callback runs ScrollableArea::RunScrollCompleteCallbacks which
  // will run all the callbacks in the Vector pending_scroll_complete_callbacks_
  // and ScrollAnimator::UserScroll may run this callback for a previous scroll
  // animation. Delay queuing up this |on_finish| so that it is run when the
  // callback for this scroll animation is run and not when the callback
  // for a previous scroll animation is run.
  ScrollCallback run_scroll_complete_callbacks(blink::BindOnce(
      [](WeakPersistent<ScrollableArea> area, ScrollCallback callback,
         ScrollCompletionMode mode) {
        if (area) {
          if (callback) {
            area->RegisterScrollCompleteCallback(std::move(callback));
          }
          area->RunScrollCompleteCallbacks(mode);
        }
      },
      WrapWeakPersistent(this), std::move(on_finish)));

  ScrollOffset pixel_delta = ResolveScrollDelta(granularity, delta);

  ScrollOffset scrollable_axis_delta(
      UserInputScrollable(kHorizontalScrollbar) ? pixel_delta.x() : 0,
      UserInputScrollable(kVerticalScrollbar) ? pixel_delta.y() : 0);
  ScrollOffset delta_to_consume =
      GetScrollAnimator().ComputeDeltaToConsume(scrollable_axis_delta);

  if (delta_to_consume.IsZero()) {
    std::move(run_scroll_complete_callbacks)
        .Run(ScrollCompletionMode::kZeroDelta);
    return ScrollResult(false, false, pixel_delta.x(), pixel_delta.y());
  }

  CancelProgrammaticScrollAnimation();

  ScrollResult result = GetScrollAnimator().UserScroll(
      granularity, scrollable_axis_delta, source_type,
      std::move(run_scroll_complete_callbacks));
  if (result.DidScroll()) {
    if (ScrollMarkerGroupPseudoElement* group = GetScrollMarkerGroup()) {
      group->UnPinSelectedMarker();
    }
    UpdateScrollMarkers();
  }

  // Delta that wasn't scrolled because the axis is !userInputScrollable
  // should count as unusedScrollDelta.
  ScrollOffset unscrollable_axis_delta = pixel_delta - scrollable_axis_delta;
  result.unused_scroll_delta_x += unscrollable_axis_delta.x();
  result.unused_scroll_delta_y += unscrollable_axis_delta.y();

  return result;
}

ScrollOffset ScrollableArea::PendingScrollAnchorAdjustment() const {
  return pending_scroll_anchor_adjustment_;
}

void ScrollableArea::ClearPendingScrollAnchorAdjustment() {
  pending_scroll_anchor_adjustment_ = ScrollOffset();
}

bool ScrollableArea::SetScrollOffset(const ScrollOffset& offset,
                                     mojom::blink::ScrollType scroll_type,
                                     cc::ScrollSourceType source_type,
                                     mojom::blink::ScrollBehavior behavior,
                                     ScrollCallback on_finish,
                                     bool targeted_scroll) {
  if (on_finish)
    RegisterScrollCompleteCallback(std::move(on_finish));

  ScrollableArea::ScrollCallback run_scroll_complete_callbacks(BindOnce(
      [](WeakPersistent<ScrollableArea> area, ScrollCompletionMode mode) {
        if (area) {
          area->RunScrollCompleteCallbacks(mode);
        }
      },
      WrapWeakPersistent(this)));
  bool filter_scroll = false;
  if (active_smooth_scroll_type_.has_value()) {
    filter_scroll = ShouldFilterIncomingScroll(scroll_type);
  }

  if (filter_scroll) {
    std::move(run_scroll_complete_callbacks)
        .Run(ScrollCompletionMode::kFinished);
    return false;
  }

  // If this was not a targeted scroll, the associated scroll-marker-group
  // should stop pinning its selected scroll-marker.
  if (!targeted_scroll) {
    if (ScrollMarkerGroupPseudoElement* marker_group = GetScrollMarkerGroup()) {
      marker_group->UnPinSelectedMarker();
    }
  }

  ScrollOffset previous_offset = GetScrollOffset();

  ScrollOffset clamped_offset = ClampScrollOffset(offset);
  if (ScrollOffsetIsNoop(clamped_offset) &&
      scroll_type != mojom::blink::ScrollType::kProgrammatic) {
    std::move(run_scroll_complete_callbacks)
        .Run(ScrollCompletionMode::kZeroDelta);
    return false;
  }

  TRACE_EVENT("blink", "ScrollableArea::SetScrollOffset", "offset",
              offset.ToString());
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("blink.debug"),
                       "SetScrollOffset", TRACE_EVENT_SCOPE_THREAD,
                       "current_offset", GetScrollOffset().ToString());
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("blink.debug"),
                       "SetScrollOffset", TRACE_EVENT_SCOPE_THREAD, "type",
                       scroll_type);
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("blink.debug"),
                       "SetScrollOffset", TRACE_EVENT_SCOPE_THREAD, "behavior",
                       behavior);

  if (behavior == mojom::blink::ScrollBehavior::kAuto)
    behavior = ScrollBehaviorStyle();

  gfx::Vector2d animation_adjustment = gfx::ToRoundedVector2d(clamped_offset) -
                                       gfx::ToRoundedVector2d(previous_offset);

  // After a scroller has been explicitly scrolled, we should no longer apply
  // scroll-start or scroll-initial-target.
  if (IsExplicitScrollType(scroll_type)) {
    StopApplyingScrollStart();
  }

  switch (scroll_type) {
    case mojom::blink::ScrollType::kCompositor:
      ScrollOffsetChanged(clamped_offset, scroll_type, source_type);
      break;
    case mojom::blink::ScrollType::kClamping:
      DCHECK_EQ(source_type, cc::ScrollSourceType::kStationaryScroll);
      ScrollOffsetChanged(clamped_offset, scroll_type,
                          cc::ScrollSourceType::kNone);
      GetScrollAnimator().AdjustAnimation(animation_adjustment);
      break;
    case mojom::blink::ScrollType::kAnchoring:
      DCHECK_EQ(source_type, cc::ScrollSourceType::kStationaryScroll);
      ScrollOffsetChanged(clamped_offset, scroll_type, source_type);
      GetScrollAnimator().AdjustAnimation(animation_adjustment);
      pending_scroll_anchor_adjustment_ += clamped_offset - previous_offset;
      break;
    case mojom::blink::ScrollType::kScrollStart:
      DCHECK_EQ(source_type, cc::ScrollSourceType::kAbsoluteScroll);
      ScrollOffsetChanged(clamped_offset, scroll_type, source_type);
      GetScrollAnimator().AdjustAnimation(animation_adjustment);
      break;
    case mojom::blink::ScrollType::kProgrammatic:
      if (ProgrammaticScrollHelper(
              clamped_offset, behavior, animation_adjustment,
              std::move(run_scroll_complete_callbacks), source_type)) {
        if (behavior == mojom::blink::ScrollBehavior::kSmooth) {
          active_smooth_scroll_type_ = scroll_type;
        }
        return true;
      }
      return false;
    case mojom::blink::ScrollType::kUser:
      if (behavior == mojom::blink::ScrollBehavior::kSmooth) {
        if (ProgrammaticScrollHelper(
                clamped_offset, behavior, animation_adjustment,
                std::move(run_scroll_complete_callbacks), source_type)) {
          active_smooth_scroll_type_ = scroll_type;
          return true;
        }
        return false;
      } else {
        UserScrollHelper(clamped_offset, behavior, source_type);
        break;
      }
    default:
      NOTREACHED();
  }

  UpdateScrollMarkers();

  std::move(run_scroll_complete_callbacks).Run(ScrollCompletionMode::kFinished);
  return true;
}

bool ScrollableArea::SetScrollOffset(const ScrollOffset& offset,
                                     mojom::blink::ScrollType type,
                                     cc::ScrollSourceType source_type,
                                     mojom::blink::ScrollBehavior behavior) {
  return SetScrollOffset(offset, type, source_type, behavior, ScrollCallback());
}

const LayoutObject* ScrollableArea::GetScrollInitialTarget() const {
  for (const auto& fragment : GetLayoutBox()->PhysicalFragments()) {
    if (auto scroll_start_target = fragment.ScrollInitialTarget()) {
      return scroll_start_target;
    }
  }
  return nullptr;
}

void ScrollableArea::ScrollToScrollInitialTarget(
    const LayoutObject* scroll_initial_target) {
  using Behavior = mojom::ScrollAlignment_Behavior;
  mojom::blink::ScrollAlignment align_x(
      Behavior::kNoScroll, Behavior::kNoScroll, Behavior::kNoScroll);
  mojom::blink::ScrollAlignment align_y(
      Behavior::kNoScroll, Behavior::kNoScroll, Behavior::kNoScroll);
  const LayoutBox* target_box = scroll_initial_target->EnclosingBox();
  if (!target_box) {
    return;
  }
  cc::ScrollSnapAlign snap_alignment =
      scroll_initial_target->Style()->GetScrollSnapAlign();
  switch (snap_alignment.alignment_block) {
    case cc::SnapAlignment::kStart:
      align_y = ScrollAlignment::TopAlways();
      break;
    case cc::SnapAlignment::kCenter:
      align_y = ScrollAlignment::CenterAlways();
      break;
    case cc::SnapAlignment::kEnd:
      align_y = ScrollAlignment::BottomAlways();
      break;
    default:
      align_y = GetLayoutBox()->HasTopOverflow()
                    ? ScrollAlignment::BottomAlways()
                    : ScrollAlignment::TopAlways();
  }
  switch (snap_alignment.alignment_inline) {
    case cc::SnapAlignment::kStart:
      align_x = ScrollAlignment::LeftAlways();
      break;
    case cc::SnapAlignment::kCenter:
      align_x = ScrollAlignment::CenterAlways();
      break;
    case cc::SnapAlignment::kEnd:
      align_x = ScrollAlignment::RightAlways();
      break;
    default:
      align_x = GetLayoutBox()->HasLeftOverflow()
                    ? ScrollAlignment::RightAlways()
                    : ScrollAlignment::LeftAlways();
  }
  mojom::blink::ScrollIntoViewParamsPtr params =
      scroll_into_view_util::CreateScrollIntoViewParams(align_x, align_y);
  params->behavior = mojom::blink::ScrollBehavior::kInstant;
  params->type = mojom::blink::ScrollType::kScrollStart;
  ScrollIntoView(target_box->AbsoluteBoundingBoxRectForScrollIntoView(),
                 PhysicalBoxStrut(), params);
}

void ScrollableArea::ApplyScrollStart() {
  if (RuntimeEnabledFeatures::CSSScrollInitialTargetEnabled()) {
    if (const LayoutObject* scroll_initial_target = GetScrollInitialTarget()) {
      if (auto* box = GetLayoutBox()) {
        UseCounter::Count(box->GetDocument(),
                          WebFeature::kCSSScrollInitialTarget);
      }
      ScrollToScrollInitialTarget(scroll_initial_target);
      // scroll-initial-target takes precedence over scroll-start, so we should
      // return here.
      return;
    }
  }
}

void ScrollableArea::ScrollBy(const ScrollOffset& delta,
                              mojom::blink::ScrollType type,
                              mojom::blink::ScrollBehavior behavior) {
  SetScrollOffset(GetScrollOffset() + delta, type,
                  cc::ScrollSourceType::kRelativeScroll, behavior);
}

bool ScrollableArea::ProgrammaticScrollHelper(
    const ScrollOffset& offset,
    mojom::blink::ScrollBehavior scroll_behavior,
    gfx::Vector2d animation_adjustment,
    ScrollCallback on_finish,
    cc::ScrollSourceType source_type) {
  bool should_use_animation =
      scroll_behavior == mojom::blink::ScrollBehavior::kSmooth &&
      ScrollAnimatorEnabled();
  if (should_use_animation) {
    // If the programmatic scroll will be animated, cancel any user scroll
    // animation already in progress. We don't want two scroll animations
    // running at the same time.
    CancelScrollAnimation();
  }

  if (ScrollOffsetIsNoop(offset)) {
    CancelProgrammaticScrollAnimation();
    if (on_finish)
      std::move(on_finish).Run(ScrollCompletionMode::kZeroDelta);
    return false;
  }

  ScrollCallback callback = std::move(on_finish);
  callback = ScrollCallback(blink::BindOnce(
      [](ScrollCallback original_callback, WeakPersistent<ScrollableArea> area,
         ScrollCompletionMode mode) {
        if (area) {
          area->OnScrollFinished(/*enqueue_scrollend=*/mode ==
                                 ScrollCompletionMode::kFinished);
        }
        if (original_callback)
          std::move(original_callback).Run(mode);
      },
      std::move(callback), WrapWeakPersistent(this)));

  // Enqueue scrollsnapchanging if necessary.
  if (auto* snap_container = GetSnapContainerData()) {
    UpdateScrollSnapChangingTargetsAndEnqueueScrollSnapChanging(
        snap_container->GetTargetSnapAreaElementIds());
  }

  if (should_use_animation) {
    GetProgrammaticScrollAnimator().AnimateToOffset(offset, source_type,
                                                    std::move(callback));
  } else {
    GetProgrammaticScrollAnimator().ScrollToOffsetWithoutAnimation(offset,
                                                                   source_type);

    // If the programmatic scroll was NOT animated, we should adjust (but not
    // cancel) a user scroll animation already in progress (crbug.com/1264266).
    GetScrollAnimator().AdjustAnimation(animation_adjustment);

    if (callback)
      std::move(callback).Run(ScrollCompletionMode::kFinished);
  }
  UpdateScrollMarkers();
  return true;
}

void ScrollableArea::UserScrollHelper(
    const ScrollOffset& offset,
    mojom::blink::ScrollBehavior scroll_behavior,
    cc::ScrollSourceType source_type) {
  CancelProgrammaticScrollAnimation();

  float x = UserInputScrollable(kHorizontalScrollbar)
                ? offset.x()
                : GetScrollAnimator().CurrentOffset().x();
  float y = UserInputScrollable(kVerticalScrollbar)
                ? offset.y()
                : GetScrollAnimator().CurrentOffset().y();

  // Smooth user scrolls (keyboard, wheel clicks) are handled via the userScroll
  // method.
  // TODO(bokan): The userScroll method should probably be modified to call this
  //              method and ScrollAnimatorBase to have a simpler
  //              animateToOffset method like the ProgrammaticScrollAnimator.
  DCHECK_EQ(scroll_behavior, mojom::blink::ScrollBehavior::kInstant);
  GetScrollAnimator().ScrollToOffsetWithoutAnimation(ScrollOffset(x, y),
                                                     source_type);
}

PhysicalRect ScrollableArea::ScrollIntoView(
    const PhysicalRect& rect_in_absolute,
    const PhysicalBoxStrut& scroll_margin,
    const mojom::blink::ScrollIntoViewParamsPtr& params) {
  // TODO(bokan): This should really be implemented here but ScrollAlignment is
  // in Core which is a dependency violation.
  NOTREACHED();
}

void ScrollableArea::ScrollOffsetChanged(const ScrollOffset& offset,
                                         mojom::blink::ScrollType scroll_type,
                                         cc::ScrollSourceType source_type) {
  TRACE_EVENT2("input", "ScrollableArea::scrollOffsetChanged", "x", offset.x(),
               "y", offset.y());
  TRACE_EVENT_INSTANT1("input", "Type", TRACE_EVENT_SCOPE_THREAD, "type",
                       scroll_type);

  ScrollOffset old_offset = GetScrollOffset();
  ScrollOffset truncated_offset =
      ShouldUseIntegerScrollOffset()
          ? ScrollOffset(gfx::ToRoundedVector2d(offset))
          : offset;

  // Tell the derived class to scroll its contents.
  UpdateScrollOffset(truncated_offset, scroll_type, source_type);

  // If the layout object has been detached as a result of updating the scroll
  // this object will be cleaned up shortly.
  if (HasBeenDisposed())
    return;

  // Tell the scrollbars to update their thumb postions.
  // If the scrollbar does not have its own layer, it must always be
  // invalidated to reflect the new thumb offset, even if the theme did not
  // invalidate any individual part.
  if (Scrollbar* horizontal_scrollbar = HorizontalScrollbar())
    horizontal_scrollbar->OffsetDidChange(scroll_type);
  if (Scrollbar* vertical_scrollbar = VerticalScrollbar())
    vertical_scrollbar->OffsetDidChange(scroll_type);

  ScrollOffset delta = GetScrollOffset() - old_offset;
  // TODO(skobes): Should we exit sooner when the offset has not changed?
  bool offset_changed = !delta.IsZero();

  if (GetMacScrollbarAnimator() && offset_changed &&
      IsExplicitScrollType(scroll_type) && ScrollbarsCanBeActive()) {
    GetMacScrollbarAnimator()->DidChangeUserVisibleScrollOffset(delta);
  }

  if (GetLayoutBox()) {
    if (offset_changed && GetLayoutBox()->GetFrameView()) {
      GetLayoutBox()->GetFrameView()->GetPaintTimingDetector().NotifyScroll(
          scroll_type);
    }
  }

  if (offset_changed && GetLayoutBox() && GetLayoutBox()->GetFrameView()) {
    GetLayoutBox()->GetFrameView()->GetLayoutShiftTracker().NotifyScroll(
        scroll_type, delta);
    // FrameSelection caches visual selection information which needs to be
    // invalidated after scrolling.
    GetLayoutBox()->GetFrameView()->GetFrame().Selection().MarkCacheDirty();
  }

  GetScrollAnimator().SetCurrentOffset(offset);
}

mojom::blink::ScrollBehavior ScrollableArea::V8EnumToScrollBehavior(
    V8ScrollBehavior::Enum behavior) {
  switch (behavior) {
    case V8ScrollBehavior::Enum::kAuto:
      return mojom::blink::ScrollBehavior::kAuto;
    case V8ScrollBehavior::Enum::kInstant:
      return mojom::blink::ScrollBehavior::kInstant;
    case V8ScrollBehavior::Enum::kSmooth:
      return mojom::blink::ScrollBehavior::kSmooth;
  }
  NOTREACHED();
}

void ScrollableArea::RegisterScrollCompleteCallback(ScrollCallback callback) {
  DCHECK(!HasBeenDisposed());
  pending_scroll_complete_callbacks_.push_back(std::move(callback));
}

void ScrollableArea::RunScrollCompleteCallbacks(ScrollCompletionMode mode) {
  Vector<ScrollCallback> callbacks(
      std::move(pending_scroll_complete_callbacks_));
  for (auto& callback : callbacks) {
    std::move(callback).Run(mode);
  }
}

void ScrollableArea::MouseEnteredScrollbar(Scrollbar& scrollbar) {
  mouse_over_scrollbar_ = true;

  if (GetMacScrollbarAnimator())
    GetMacScrollbarAnimator()->MouseEnteredScrollbar(scrollbar);
  ShowNonMacOverlayScrollbars();
  if (fade_overlay_scrollbars_timer_)
    fade_overlay_scrollbars_timer_->Value().Stop();
}

void ScrollableArea::MouseExitedScrollbar(Scrollbar& scrollbar) {
  mouse_over_scrollbar_ = false;

  if (GetMacScrollbarAnimator())
    GetMacScrollbarAnimator()->MouseExitedScrollbar(scrollbar);
  if (HasOverlayScrollbars() && !scrollbars_hidden_if_overlay_) {
    // This will kick off the fade out timer.
    ShowNonMacOverlayScrollbars();
  }
}

void ScrollableArea::MouseCapturedScrollbar() {
  scrollbar_captured_ = true;
  ShowNonMacOverlayScrollbars();
  if (fade_overlay_scrollbars_timer_)
    fade_overlay_scrollbars_timer_->Value().Stop();
}

void ScrollableArea::MouseReleasedScrollbar() {
  scrollbar_captured_ = false;
  // This will kick off the fade out timer.
  ShowNonMacOverlayScrollbars();
}

void ScrollableArea::DidAddScrollbar(Scrollbar& scrollbar,
                                     ScrollbarOrientation orientation) {
  if (GetMacScrollbarAnimator()) {
    if (orientation == kVerticalScrollbar)
      GetMacScrollbarAnimator()->DidAddVerticalScrollbar(scrollbar);
    else
      GetMacScrollbarAnimator()->DidAddHorizontalScrollbar(scrollbar);
  }

  // <rdar://problem/9797253> AppKit resets the scrollbar's style when you
  // attach a scrollbar
  SetOverlayScrollbarColorScheme(GetOverlayScrollbarColorScheme());
}

void ScrollableArea::WillRemoveScrollbar(Scrollbar& scrollbar,
                                         ScrollbarOrientation orientation) {
  if (mac_scrollbar_animator_) {
    if (orientation == kVerticalScrollbar)
      mac_scrollbar_animator_->WillRemoveVerticalScrollbar(scrollbar);
    else
      mac_scrollbar_animator_->WillRemoveHorizontalScrollbar(scrollbar);
  }
}

bool ScrollableArea::HasOverlayScrollbars() const {
  Scrollbar* v_scrollbar = VerticalScrollbar();
  if (v_scrollbar && v_scrollbar->IsOverlayScrollbar())
    return true;
  Scrollbar* h_scrollbar = HorizontalScrollbar();
  return h_scrollbar && h_scrollbar->IsOverlayScrollbar();
}

void ScrollableArea::SetOverlayScrollbarColorScheme(
    mojom::blink::ColorScheme overlay_theme) {
  overlay_scrollbar_color_scheme__ = static_cast<unsigned>(overlay_theme);

  if (Scrollbar* scrollbar = HorizontalScrollbar()) {
    scrollbar->SetNeedsPaintInvalidation(kAllParts);
  }

  if (Scrollbar* scrollbar = VerticalScrollbar()) {
    scrollbar->SetNeedsPaintInvalidation(kAllParts);
  }
}

void ScrollableArea::RecalculateOverlayScrollbarColorScheme() {
  mojom::blink::ColorScheme old_overlay_theme =
      GetOverlayScrollbarColorScheme();

  // Start with a scrollbar overlay theme based on the used color scheme.
  mojom::blink::ColorScheme overlay_theme = UsedColorSchemeScrollbars();

  // If there is a background color set on the scroller, use the lightness of
  // the background color for the scrollbar overlay color theme.
  if (GetLayoutBox()) {
    Color background_color = GetLayoutBox()->StyleRef().VisitedDependentColor(
        GetCSSPropertyBackgroundColor());
    if (!background_color.IsFullyTransparent()) {
      const float lightness =
          background_color.GetLightness(Color::ColorSpace::kHSL);
      overlay_theme = lightness <= 0.5f ? mojom::blink::ColorScheme::kDark
                                        : mojom::blink::ColorScheme::kLight;
    }
  }

  if (old_overlay_theme != overlay_theme) {
    SetOverlayScrollbarColorScheme(overlay_theme);
  }
}

void ScrollableArea::SetScrollbarNeedsPaintInvalidation(
    ScrollbarOrientation orientation) {
  if (orientation == kHorizontalScrollbar)
    horizontal_scrollbar_needs_paint_invalidation_ = true;
  else
    vertical_scrollbar_needs_paint_invalidation_ = true;

  // Invalidate the scrollbar directly if it's already composited.
  // GetLayoutBox() may be null in some unit tests.
  if (auto* box = GetLayoutBox()) {
    if (auto* scrollbar = GetScrollbar(orientation)) {
      if (auto* compositor =
              box->GetFrameView()->GetPaintArtifactCompositor()) {
        CompositorElementId element_id = GetScrollbarElementId(orientation);
        if (scrollbar->IsSolidColor()) {
          // This will call SetNeedsDisplay() if the color changes (which is
          // the only reason for a SolidColorScrollbarLayer to update display).
          if (compositor->SetScrollbarSolidColor(
                  element_id, scrollbar->GetTheme().ThumbColor(*scrollbar))) {
            scrollbar->ClearNeedsUpdateDisplay();
          }
        } else if (compositor->SetScrollbarNeedsDisplay(element_id)) {
          scrollbar->ClearNeedsUpdateDisplay();
        }
      }
    }
  }

  // TODO(crbug.com/1505560): we don't need to invalidate paint of scrollbar
  // for changes inside of the scrollbar. We'll invalidate raster if needed
  // after paint. We can remove some of paint invalidation code in this class,
  // and move remaining paint invalidation code into
  // PaintLayerScrollableArea and Scrollbar.
  ScrollControlWasSetNeedsPaintInvalidation();
}

void ScrollableArea::SetScrollCornerNeedsPaintInvalidation() {
  if (cc::Layer* layer = LayerForScrollCorner())
    layer->SetNeedsDisplay();
  scroll_corner_needs_paint_invalidation_ = true;
  ScrollControlWasSetNeedsPaintInvalidation();
}

void ScrollableArea::SetScrollControlsNeedFullPaintInvalidation() {
  if (auto* horizontal_scrollbar = HorizontalScrollbar())
    horizontal_scrollbar->SetNeedsPaintInvalidation(kAllParts);
  if (auto* vertical_scrollbar = VerticalScrollbar())
    vertical_scrollbar->SetNeedsPaintInvalidation(kAllParts);
  SetScrollCornerNeedsPaintInvalidation();
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

void ScrollableArea::ServiceScrollAnimations(double monotonic_time) {
  bool requires_animation_service = false;
  if (ScrollAnimatorBase* scroll_animator = ExistingScrollAnimator()) {
    scroll_animator->TickAnimation(base::Seconds(monotonic_time) +
                                   base::TimeTicks());
    if (scroll_animator->HasAnimationThatRequiresService())
      requires_animation_service = true;
  }
  if (ProgrammaticScrollAnimator* programmatic_scroll_animator =
          ExistingProgrammaticScrollAnimator()) {
    programmatic_scroll_animator->TickAnimation(base::Seconds(monotonic_time) +
                                                base::TimeTicks());
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

void ScrollableArea::SetScrollbarsHiddenForTesting(bool hidden) {
  // If scrollable area has been disposed, we can not get the page scrollbar
  // theme setting. Should early return here.
  if (HasBeenDisposed())
    return;

  SetScrollbarsHiddenIfOverlayInternal(hidden);
}

void ScrollableArea::SetScrollbarsHiddenFromExternalAnimator(bool hidden) {
  // If scrollable area has been disposed, we can not get the page scrollbar
  // theme setting. Should early return here.
  if (HasBeenDisposed())
    return;

  DCHECK(!GetPageScrollbarTheme().BlinkControlsOverlayVisibility());
  SetScrollbarsHiddenIfOverlayInternal(hidden);
}

void ScrollableArea::SetScrollbarsHiddenIfOverlay(bool hidden) {
  // If scrollable area has been disposed, we can not get the page scrollbar
  // theme setting. Should early return here.
  if (HasBeenDisposed())
    return;

  DCHECK(GetPageScrollbarTheme().BlinkControlsOverlayVisibility());
  SetScrollbarsHiddenIfOverlayInternal(hidden);
}

void ScrollableArea::SetScrollbarsHiddenIfOverlayInternal(bool hidden) {
  if (!GetPageScrollbarTheme().UsesOverlayScrollbars())
    return;

  hidden = hidden && !ShouldAvoidHidingOverlayScrollbars();

  if (scrollbars_hidden_if_overlay_ == static_cast<unsigned>(hidden))
    return;

  scrollbars_hidden_if_overlay_ = hidden;
  ScrollbarVisibilityChanged();
}

void ScrollableArea::FadeOverlayScrollbarsTimerFired(TimerBase*) {
  // Scrollbars can become composited in the time it takes the timer set in
  // ShowNonMacOverlayScrollbars to be fired.
  if (RuntimeEnabledFeatures::RasterInducingScrollEnabled() ||
      UsesCompositedScrolling()) {
    return;
  }
  SetScrollbarsHiddenIfOverlay(true);
}

void ScrollableArea::ShowNonMacOverlayScrollbars() {
  if (!GetPageScrollbarTheme().UsesOverlayScrollbars() ||
      !GetPageScrollbarTheme().BlinkControlsOverlayVisibility())
    return;

  // Don't do this for composited scrollbars. These scrollbars are handled
  // by separate code in cc::ScrollbarAnimationController.
  // TODO(crbug.com/1229864): We may want to always composite overlay
  // scrollbars to avoid the bug and the duplicated code for composited and
  // non-composited overlay scrollbars.
  if (RuntimeEnabledFeatures::RasterInducingScrollEnabled() ||
      UsesCompositedScrolling()) {
    return;
  }

  SetScrollbarsHiddenIfOverlay(false);

  const base::TimeDelta time_until_disable =
      GetPageScrollbarTheme().OverlayScrollbarFadeOutDelay() +
      GetPageScrollbarTheme().OverlayScrollbarFadeOutDuration();

  // If the overlay scrollbars never fade out, don't do anything. This is the
  // case for the mock overlays used in tests (and also Mac but its scrollbars
  // are animated by OS APIs and so we've already early-out'ed above).  We also
  // don't fade out overlay scrollbar for popup since we don't create
  // compositor for popup and thus they don't appear on hover so users without
  // a wheel can't scroll if they fade out.
  if (time_until_disable.is_max() || GetChromeClient()->IsPopup()) {
    return;
  }

  if (!fade_overlay_scrollbars_timer_) {
    fade_overlay_scrollbars_timer_ = MakeGarbageCollected<
        DisallowNewWrapper<HeapTaskRunnerTimer<ScrollableArea>>>(
        GetCompositorTaskRunner(), this,
        &ScrollableArea::FadeOverlayScrollbarsTimerFired);
  }

  if (!scrollbar_captured_ && !mouse_over_scrollbar_) {
    fade_overlay_scrollbars_timer_->Value().StartOneShot(time_until_disable,
                                                         FROM_HERE);
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
ScrollableArea::GetCompositorTaskRunner() {
  return compositor_task_runner_;
}

Node* ScrollableArea::EventTargetNode() const {
  const LayoutBox* box = GetLayoutBox();
  Node* node = box->GetNode();
  if (!node && box->Parent() && box->Parent()->IsFieldset()) {
    node = box->Parent()->GetNode();
  }
  if (auto* element = DynamicTo<Element>(node)) {
    const LayoutBox* layout_box_for_scrolling =
        element->GetLayoutBoxForScrolling();
    if (layout_box_for_scrolling)
      DCHECK_EQ(box, layout_box_for_scrolling);
    else
      return nullptr;
  }
  return node;
}

const Document* ScrollableArea::GetDocument() const {
  if (auto* box = GetLayoutBox())
    return &box->GetDocument();
  return nullptr;
}

gfx::Vector2d ScrollableArea::ClampScrollOffset(
    const gfx::Vector2d& scroll_offset) const {
  gfx::Vector2d result = scroll_offset;
  result.SetToMin(MaximumScrollOffsetInt());
  result.SetToMax(MinimumScrollOffsetInt());
  return result;
}

ScrollOffset ScrollableArea::ClampScrollOffset(
    const ScrollOffset& scroll_offset) const {
  ScrollOffset result = scroll_offset;
  result.SetToMin(MaximumScrollOffset());
  result.SetToMax(MinimumScrollOffset());
  return result;
}

int ScrollableArea::LineStep(ScrollbarOrientation) const {
  return PixelsPerLineStep(GetLayoutBox()->GetFrame());
}

gfx::Size ScrollableArea::PageSize() const {
  // Paging scroll operations should take scroll-padding into account [1]. So we
  // use the snapport rect to calculate the page step instead of the visible
  // rect.
  // [1] https://drafts.csswg.org/css-scroll-snap/#scroll-padding
  return VisibleScrollSnapportRect(kExcludeScrollbars).PixelSnappedSize();
}

int ScrollableArea::PageStep(ScrollbarOrientation orientation) const {
  gfx::Size page_size = PageSize();
  return cc::ScrollUtils::CalculatePageStep(orientation == kHorizontalScrollbar
                                                ? page_size.width()
                                                : page_size.height());
}

int ScrollableArea::DocumentStep(ScrollbarOrientation orientation) const {
  return ScrollSize(orientation);
}

float ScrollableArea::PixelStep(ScrollbarOrientation) const {
  return 1;
}

int ScrollableArea::VerticalScrollbarWidth(
    OverlayScrollbarClipBehavior behavior) const {
  DCHECK_EQ(behavior, kIgnoreOverlayScrollbarSize);
  if (Scrollbar* vertical_bar = VerticalScrollbar())
    return !vertical_bar->IsOverlayScrollbar() ? vertical_bar->Width() : 0;
  return 0;
}

int ScrollableArea::HorizontalScrollbarHeight(
    OverlayScrollbarClipBehavior behavior) const {
  DCHECK_EQ(behavior, kIgnoreOverlayScrollbarSize);
  if (Scrollbar* horizontal_bar = HorizontalScrollbar())
    return !horizontal_bar->IsOverlayScrollbar() ? horizontal_bar->Height() : 0;
  return 0;
}

gfx::QuadF ScrollableArea::LocalToVisibleContentQuad(const gfx::QuadF& quad,
                                                     const LayoutObject*,
                                                     unsigned) const {
  return quad - GetScrollOffset();
}

gfx::Size ScrollableArea::ExcludeScrollbars(const gfx::Size& size) const {
  return gfx::Size(std::max(0, size.width() - VerticalScrollbarWidth()),
                   std::max(0, size.height() - HorizontalScrollbarHeight()));
}

void ScrollableArea::DidCompositorScroll(const gfx::PointF& position,
                                         cc::ScrollSourceType source_type) {
  ScrollOffset new_offset(ScrollPositionToOffset(position));
  ScrollMarkerGroupPseudoElement* group = GetScrollMarkerGroup();
  // A non-latched compositor scroll update might be in service of a
  // targeted (i.e. smooth scrollIntoView) or non-targeted scroll (e.g
  // smooth scrollTo or a gesture scroll). If we are still executing a
  // targeted scroll, the associated ScrollMarkerGroupPseudoElement's
  // selected marker will still be pinned and we should not change that.
  bool targeted_scroll = group && group->SelectedMarkerIsPinned();
  // If `source_type=cc::ScrollSourceType::kNone` then compositor scroll was
  // triggered from `ScrollAnimator` or `ProgrammaticScrollAnimator` so we need
  // to use `ScrollSourceType` cached in `ScrollAnimator` or
  // `ProgrammaticScrollAnimator` accordingly.
  if (source_type == cc::ScrollSourceType::kNone && ExistingScrollAnimator()) {
    source_type = ExistingScrollAnimator()->GetScrollSourceType();
  }
  if (source_type == cc::ScrollSourceType::kNone &&
      ExistingProgrammaticScrollAnimator()) {
    source_type = ExistingProgrammaticScrollAnimator()->GetScrollSourceType();
  }

  bool vertical_scrollbar_thumb_pressed =
      VerticalScrollbar() && VerticalScrollbar()->PressedPart() == kThumbPart;
  bool horizontal_scrollbar_thumb_pressed =
      HorizontalScrollbar() &&
      HorizontalScrollbar()->PressedPart() == kThumbPart;
  if (source_type == cc::ScrollSourceType::kRelativeScroll &&
      (vertical_scrollbar_thumb_pressed ||
       horizontal_scrollbar_thumb_pressed)) {
    // Manipulating the scrollbar “thumb” explicitly should be an absolute
    // scroll https://drafts.csswg.org/css-scroll-snap-1/#scroll-types.
    // TODO(crbug.com/414556050): This should be ideally done on the compositor.
    source_type = cc::ScrollSourceType::kAbsoluteScroll;
  }

  SetScrollOffset(new_offset, mojom::blink::ScrollType::kCompositor,
                  source_type, mojom::blink::ScrollBehavior::kInstant,
                  ScrollCallback(), targeted_scroll);
}

Scrollbar* ScrollableArea::GetScrollbar(
    ScrollbarOrientation orientation) const {
  return orientation == kHorizontalScrollbar ? HorizontalScrollbar()
                                             : VerticalScrollbar();
}

CompositorElementId ScrollableArea::GetScrollbarElementId(
    ScrollbarOrientation orientation) {
  CompositorElementId scrollable_element_id = GetScrollElementId();
  DCHECK(scrollable_element_id);
  CompositorElementIdNamespace element_id_namespace =
      orientation == kHorizontalScrollbar
          ? CompositorElementIdNamespace::kHorizontalScrollbar
          : CompositorElementIdNamespace::kVerticalScrollbar;
  return CompositorElementIdWithNamespace(scrollable_element_id,
                                          element_id_namespace);
}

void ScrollableArea::OnScrollFinished(bool scroll_did_end) {
  if (GetLayoutBox()) {
    if (scroll_did_end) {
      active_smooth_scroll_type_.reset();
      UpdateSnappedTargetsAndEnqueueScrollSnapChange();
      if (Node* node = EventTargetNode()) {
        if (auto* viewport_position_tracker =
                AnchorElementViewportPositionTracker::MaybeGetOrCreateFor(
                    node->GetDocument())) {
          viewport_position_tracker->OnScrollEnd();
        }
        // TODO(https://crbug.com/41406914): This is temporary. Remove once we
        // start to migrate to scroll-promises.
        node->GetDocument().Markers().StartGlicMarkerAnimationIfNeeded();
        node->GetDocument().EnqueueScrollEndEventForNode(node);
      }
    }
    GetLayoutBox()
        ->GetFrame()
        ->LocalFrameRoot()
        .GetEventHandler()
        .MarkHoverStateDirty();
  }
}

void ScrollableArea::SnapAfterScrollbarScrolling(
    ScrollbarOrientation orientation,
    cc::ScrollSourceType source_type) {
  SnapAtCurrentPosition(orientation == kHorizontalScrollbar,
                        orientation == kVerticalScrollbar, source_type,
                        base::ScopedClosureRunner());
}

bool ScrollableArea::SnapAtCurrentPosition(
    bool scrolled_x,
    bool scrolled_y,
    cc::ScrollSourceType source_type,
    base::ScopedClosureRunner on_finish) {
  DCHECK(IsRootFrameViewport() || !GetLayoutBox()->IsGlobalRootScroller());
  gfx::PointF current_position = ScrollPosition();
  return SnapForEndPosition(current_position, scrolled_x, scrolled_y,
                            source_type, std::move(on_finish));
}

bool ScrollableArea::SnapForEndPosition(const gfx::PointF& end_position,
                                        bool scrolled_x,
                                        bool scrolled_y,
                                        cc::ScrollSourceType source_type,
                                        base::ScopedClosureRunner on_finish) {
  DCHECK(IsRootFrameViewport() || !GetLayoutBox()->IsGlobalRootScroller());
  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForEndPosition(end_position, scrolled_x,
                                                      scrolled_y);
  return PerformSnapping(
      *strategy, source_type, mojom::blink::ScrollBehavior::kSmooth,
      std::move(on_finish), /*preserve_pinned_marker=*/false);
}

bool ScrollableArea::SnapForDirection(ScrollDirectionPhysical direction) {
  DCHECK(IsRootFrameViewport() || !GetLayoutBox()->IsGlobalRootScroller());
  ScrollOffset delta = ToScrollDelta(direction, 1);
  delta.Scale(LineStep(kHorizontalScrollbar), LineStep(kVerticalScrollbar));

  gfx::PointF current_position = ScrollPosition();
  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForDirection(
          current_position, delta,
          RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled());
  // Only called for arrow key press scrolls, which are relative scrolls.
  // https://drafts.csswg.org/css-scroll-snap-1/#scroll-types
  return PerformSnapping(*strategy, cc::ScrollSourceType::kRelativeScroll,
                         mojom::blink::ScrollBehavior::kSmooth,
                         base::ScopedClosureRunner(),
                         /*preserve_pinned_marker=*/false);
}

bool ScrollableArea::SnapForPageScroll(ScrollDirectionPhysical direction) {
  DCHECK(IsRootFrameViewport() || !GetLayoutBox()->IsGlobalRootScroller());
  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      PageScrollSnapStrategy(direction);
  // Only called for PgUp/PgDn key press scrolls, which are relative scrolls.
  // https://drafts.csswg.org/css-scroll-snap-1/#scroll-types
  return PerformSnapping(*strategy, cc::ScrollSourceType::kRelativeScroll,
                         mojom::blink::ScrollBehavior::kSmooth,
                         base::ScopedClosureRunner(),
                         /*preserve_pinned_marker=*/false);
}

bool ScrollableArea::SnapForDocumentScroll(ScrollDirectionPhysical direction) {
  ScrollOffset delta = ToScrollDelta(direction, 1);
  delta.Scale(DocumentStep(kHorizontalScrollbar),
              DocumentStep(kVerticalScrollbar));
  gfx::PointF end_position = ScrollPosition() + delta;
  bool scrolled_x = direction == kScrollLeft || direction == kScrollRight;
  bool scrolled_y = direction == kScrollUp || direction == kScrollDown;
  // Only called for Home/End key press scrolls, which are absolute scrolls.
  // https://drafts.csswg.org/css-scroll-snap-1/#scroll-types
  return SnapForEndPosition(end_position, scrolled_x, scrolled_y,
                            cc::ScrollSourceType::kAbsoluteScroll,
                            base::ScopedClosureRunner());
}

std::unique_ptr<cc::SnapSelectionStrategy>
ScrollableArea::PageScrollSnapStrategy(
    ScrollDirectionPhysical direction) const {
  return cc::SnapSelectionStrategy::CreateForPageScroll(
      ScrollPosition(), ToScrollDelta(direction, 1), PageSize(),
      RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled());
}

void ScrollableArea::SnapAfterLayout() {
  const cc::SnapContainerData* container_data = GetSnapContainerData();
  if (!container_data || !container_data->size()) {
    UpdateSnappedTargetsAndEnqueueScrollSnapChange();
    return;
  }

  gfx::PointF current_position = ScrollPosition();
  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForTargetElement(current_position);
  PerformSnapping(*strategy, cc::ScrollSourceType::kStationaryScroll,
                  mojom::blink::ScrollBehavior::kInstant,
                  base::ScopedClosureRunner(), /*preserve_pinned_marker=*/true);
}

bool ScrollableArea::PerformSnapping(
    const cc::SnapSelectionStrategy& strategy,
    cc::ScrollSourceType source_type,
    mojom::blink::ScrollBehavior scroll_behavior,
    base::ScopedClosureRunner on_finish,
    bool preserve_pinned_marker) {
  std::optional<gfx::PointF> snap_point = GetSnapPositionAndSetTarget(strategy);
  if (!snap_point) {
    UpdateSnappedTargetsAndEnqueueScrollSnapChange();
    return false;
  }

  // We should set the scrollsnapchanging targets of a snap container the first
  // time it is laid out to avoid a spurious scrollsnapchanging event firing the
  // first time the scroller is scrolled.
  if (!GetScrollsnapchangingTargetIds()) {
    SetScrollsnapchangingTargetIds(
        GetSnapContainerData()->GetTargetSnapAreaElementIds());
  }

  CancelScrollAnimation();
  CancelProgrammaticScrollAnimation();

  bool targeted_scroll = false;
  if (ScrollMarkerGroupPseudoElement* group = GetScrollMarkerGroup()) {
    if (preserve_pinned_marker) {
      targeted_scroll = group->SelectedMarkerIsPinned();
    }
  }
  if (!SetScrollOffset(
          ScrollPositionToOffset(snap_point.value()),
          mojom::blink::ScrollType::kProgrammatic, source_type, scroll_behavior,
          IgnoreArgs<ScrollableArea::ScrollCompletionMode>(on_finish.Release()),
          targeted_scroll)) {
    // If no scroll happens, e.g. we got here because of a layout change, we
    // need to re-compute snapped targets and fire scrollsnapchange if
    // necessary.
    UpdateSnappedTargetsAndEnqueueScrollSnapChange();
  }
  return true;
}

void ScrollableArea::Trace(Visitor* visitor) const {
  visitor->Trace(scroll_animator_);
  visitor->Trace(mac_scrollbar_animator_);
  visitor->Trace(programmatic_scroll_animator_);
  visitor->Trace(fade_overlay_scrollbars_timer_);
}

void ScrollableArea::InjectScrollbarGestureScroll(
    ScrollOffset delta,
    ui::ScrollGranularity granularity,
    WebInputEvent::Type gesture_type) const {
  // All ScrollableArea's have a layout box, except for the VisualViewport.
  // We shouldn't be injecting scrolls for the visual viewport scrollbar, since
  // it is not hit-testable.
  DCHECK(GetLayoutBox());

  // Speculative fix for crash reports (crbug.com/1307510).
  if (!GetLayoutBox() || !GetLayoutBox()->GetFrame())
    return;

  if (granularity == ui::ScrollGranularity::kScrollByPrecisePixel ||
      granularity == ui::ScrollGranularity::kScrollByPixel) {
    // Pixel-based deltas need to be scaled up by the input event scale factor,
    // since the GSUs will be scaled down by that factor when being handled.
    float scale = 1;
    LocalFrameView* root_view =
        GetLayoutBox()->GetFrame()->LocalFrameRoot().View();
    if (root_view)
      scale = root_view->InputEventsScaleFactor();
    delta.Scale(scale);
  }

  GetChromeClient()->InjectScrollbarGestureScroll(
      *GetLayoutBox()->GetFrame(), delta, granularity, GetScrollElementId(),
      gesture_type);
}

ScrollableArea* ScrollableArea::GetForScrolling(const LayoutBox* layout_box) {
  if (!layout_box)
    return nullptr;

  if (!layout_box->IsGlobalRootScroller()) {
    if (const auto* element = DynamicTo<Element>(layout_box->GetNode())) {
      if (auto* scrolling_box = element->GetLayoutBoxForScrolling())
        return scrolling_box->GetScrollableArea();
    }
    return layout_box->GetScrollableArea();
  }

  // The global root scroller should be scrolled by the root frame view's
  // ScrollableArea.
  LocalFrame& root_frame = layout_box->GetFrame()->LocalFrameRoot();
  return root_frame.View()->GetScrollableArea();
}

float ScrollableArea::ScaleFromDIP() const {
  auto* client = GetChromeClient();
  auto* document = GetDocument();
  if (client && document)
    return client->WindowToViewportScalar(document->GetFrame(), 1.0f);
  return 1.0f;
}

bool ScrollableArea::ScrollOffsetIsNoop(const ScrollOffset& offset) const {
  return GetScrollOffset() ==
         (ShouldUseIntegerScrollOffset()
              ? ScrollOffset(gfx::ToRoundedVector2d(offset))
              : offset);
}

void ScrollableArea::EnqueueScrollSnapChangeEvent() const {
  DCHECK(RuntimeEnabledFeatures::CSSScrollSnapChangeEventEnabled());
  Node* target_node = EventTargetNode();
  if (!target_node) {
    return;
  }
  Member<Node> block_target = GetSnapEventTargetAlongAxis(
      event_type_names::kScrollsnapchange, cc::SnapAxis::kBlock);
  Member<Node> inline_target = GetSnapEventTargetAlongAxis(
      event_type_names::kScrollsnapchange, cc::SnapAxis::kInline);
  target_node->GetDocument().EnqueueScrollSnapChangeEvent(
      target_node, block_target, inline_target);
}

void ScrollableArea::EnqueueScrollSnapChangingEvent() const {
  DCHECK(RuntimeEnabledFeatures::CSSScrollSnapChangingEventEnabled());
  Node* target_node = EventTargetNode();
  if (!target_node) {
    return;
  }
  Member<Node> block_target = GetSnapEventTargetAlongAxis(
      event_type_names::kScrollsnapchanging, cc::SnapAxis::kBlock);
  Member<Node> inline_target = GetSnapEventTargetAlongAxis(
      event_type_names::kScrollsnapchanging, cc::SnapAxis::kInline);
  target_node->GetDocument().EnqueueScrollSnapChangingEvent(
      target_node, block_target, inline_target);
}

ScrollOffset ScrollableArea::GetWebExposedScrollOffset() const {
  ScrollOffset scroll_offset =
      SnapScrollOffsetToPhysicalPixels(GetScrollOffset());

  // Ensure that, if fractional scroll offsets are not enabled, the scroll
  // offset is an floored value.
  CHECK_EQ(gfx::ToRoundedVector2d(scroll_offset), scroll_offset);
  return scroll_offset;
}

ScrollOffset ScrollableArea::GetScrollOffsetForScrollMarkerUpdate() {
  ScrollOffset offset_for_scroll_marker_update = GetScrollOffset();
  if (GetScrollAnimator().HasRunningAnimation()) {
    offset_for_scroll_marker_update = GetScrollAnimator().DesiredTargetOffset();
  } else if (GetProgrammaticScrollAnimator().HasRunningAnimation()) {
    offset_for_scroll_marker_update =
        GetProgrammaticScrollAnimator().TargetOffset();
  }
  return offset_for_scroll_marker_update;
}

namespace {

LayoutBox* GetEnclosingLayoutBoxCrossingDocumentBoundary(
    LayoutBox* layout_box) {
  CHECK(layout_box);

  return IsA<LayoutView>(layout_box)
             ? static_cast<LayoutBox*>(
                   layout_box->GetFrame()->OwnerLayoutObject())
             : static_cast<LayoutBox*>(layout_box->ContainingBlock());
}

ScrollableArea* GetNearestScrollableArea(LayoutBox* current_box) {
  CHECK(current_box);
  LayoutBox* next_box = current_box;

  // Scrolling propagates along the containing block chain and ends at the
  // RootScroller node. The RootScroller node will have a custom applyScroll
  // callback that performs scrolling as well as associated "root" actions like
  // browser control movement and overscroll glow.
  do {
    if (next_box->IsGlobalRootScroller() ||
        (next_box->IsScrollContainer() &&
         (next_box->GetScrollableArea()->ScrollsOverflow() ||
          !next_box->GetScrollableArea()->CanPropagateScroll()))) {
      return next_box->GetScrollableArea();
    }

    next_box = GetEnclosingLayoutBoxCrossingDocumentBoundary(next_box);
  } while (next_box);

  return &current_box->GetDocument().GetPage()->GetVisualViewport();
}

}  // namespace

ScrollableAreaTraversal::ScrollableAreaTraversal(Node* target_node) {
  if (!target_node || !target_node->GetLayoutObject()) {
    return;
  }

  auto* layout_box = target_node->GetLayoutObject()->EnclosingBox();
  start_scrollable_area_ =
      layout_box ? GetNearestScrollableArea(layout_box)
                 : &target_node->GetDocument().GetPage()->GetVisualViewport();
}

ScrollableAreaTraversal::Iterator&
ScrollableAreaTraversal::Iterator::operator++() {
  CHECK(current_scrollable_area_);

  if (auto* current_box = current_scrollable_area_->GetLayoutBox()) {
    auto* next_box = GetEnclosingLayoutBoxCrossingDocumentBoundary(current_box);
    current_scrollable_area_ =
        next_box ? GetNearestScrollableArea(next_box)
                 : &current_box->GetDocument().GetPage()->GetVisualViewport();
    return *this;
  }

  current_scrollable_area_ = nullptr;
  return *this;
}

}  // namespace blink
