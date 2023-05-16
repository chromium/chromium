// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/scroll_timeline.h"

#include <tuple>

#include "base/memory/values_equivalent.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_timeline_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_double.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline_util.h"
#include "third_party/blink/renderer/core/animation/worklet_animation_base.h"
#include "third_party/blink/renderer/core/animation/worklet_animation_controller.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_values.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

namespace {

ScrollOrientation ToPhysicalScrollOrientation(ScrollAxis axis,
                                              const LayoutBox& source_box) {
  bool is_horizontal = source_box.IsHorizontalWritingMode();
  switch (axis) {
    case ScrollAxis::kBlock:
      return is_horizontal ? kVerticalScroll : kHorizontalScroll;
    case ScrollAxis::kInline:
      return is_horizontal ? kHorizontalScroll : kVerticalScroll;
    case ScrollAxis::kHorizontal:
      return kHorizontalScroll;
    case ScrollAxis::kVertical:
      return kVerticalScroll;
  }
}

Node* ResolveSource(Element* source) {
  if (source && source == source->GetDocument().ScrollingElementNoLayout()) {
    return &source->GetDocument();
  }
  return source;
}

}  // namespace

ScrollTimeline* ScrollTimeline::Create(Document& document,
                                       ScrollTimelineOptions* options,
                                       ExceptionState& exception_state) {
  absl::optional<Element*> source = options->hasSource()
                                        ? absl::make_optional(options->source())
                                        : absl::nullopt;

  ScrollAxis axis =
      options->hasAxis() ? options->axis().AsEnum() : ScrollAxis::kBlock;

  TimelineAttachment attachment = TimelineAttachment::kLocal;

  // The scrollingElement depends on style/layout-tree in quirks mode. Update
  // such that subsequent calls to ScrollingElementNoLayout returns up-to-date
  // information.
  if (document.InQuirksMode())
    document.UpdateStyleAndLayoutTree();

  return Create(&document, source.value_or(document.ScrollingElementNoLayout()),
                axis, attachment);
}

ScrollTimeline* ScrollTimeline::Create(Document* document,
                                       Element* source,
                                       ScrollAxis axis,
                                       TimelineAttachment attachment) {
  ScrollTimeline* scroll_timeline = MakeGarbageCollected<ScrollTimeline>(
      document, attachment, ReferenceType::kSource, source, axis);
  scroll_timeline->UpdateSnapshot();

  return scroll_timeline;
}

ScrollTimeline::ScrollTimeline(Document* document,
                               TimelineAttachment attachment,
                               ReferenceType reference_type,
                               Element* reference,
                               ScrollAxis axis)
    : ScrollTimeline(
          document,
          attachment,
          attachment == TimelineAttachment::kDefer
              ? nullptr
              : MakeGarbageCollected<ScrollTimelineAttachment>(reference_type,
                                                               reference,
                                                               axis)) {}

ScrollTimeline::ScrollTimeline(Document* document,
                               TimelineAttachment attachment_type,
                               ScrollTimelineAttachment* attachment)
    : AnimationTimeline(document),
      ScrollSnapshotClient(document->GetFrame()),
      attachment_type_(attachment_type) {
  if (attachment) {
    attachments_.push_back(attachment);
  }
}

bool ScrollTimeline::IsResolved() const {
  return ComputeIsResolved(ResolvedSource());
}

bool ScrollTimeline::IsActive() const {
  return timeline_state_snapshotted_.phase != TimelinePhase::kInactive;
}

absl::optional<ScrollOffsets> ScrollTimeline::GetResolvedScrollOffsets() const {
  return timeline_state_snapshotted_.scroll_offsets;
}

absl::optional<ScrollOffsets> ScrollTimeline::GetResolvedViewOffsets() const {
  return timeline_state_snapshotted_.view_offsets;
}

// TODO(crbug.com/1336260): Since phase can only be kActive or kInactive and
// currentTime  can only be null if phase is inactive or before the first
// snapshot we can probably drop phase.
AnimationTimeline::PhaseAndTime ScrollTimeline::CurrentPhaseAndTime() {
  return {timeline_state_snapshotted_.phase,
          timeline_state_snapshotted_.current_time};
}

V8CSSNumberish* ScrollTimeline::ConvertTimeToProgress(
    AnimationTimeDelta time) const {
  return MakeGarbageCollected<V8CSSNumberish>(
      CSSUnitValues::percent((time / GetDuration().value()) * 100));
}

V8CSSNumberish* ScrollTimeline::currentTime() {
  // Compute time as a percentage based on the relative scroll position, where
  // the start offset corresponds to 0% and the end to 100%.
  auto current_time = timeline_state_snapshotted_.current_time;

  if (current_time) {
    return ConvertTimeToProgress(AnimationTimeDelta(current_time.value()));
  }
  return nullptr;
}

V8CSSNumberish* ScrollTimeline::duration() {
  return MakeGarbageCollected<V8CSSNumberish>(CSSUnitValues::percent(100));
}

void ScrollTimeline::ResolveTimelineOffsets() const {
  TimelineRange timeline_range = GetTimelineRange();
  for (Animation* animation : GetAnimations()) {
    animation->ResolveTimelineOffsets(timeline_range);
  }
}

Element* ScrollTimeline::RetainingElement() const {
  if (attachment_type_ == TimelineAttachment::kLocal) {
    return CurrentAttachment()->GetReferenceElement();
  }
  // TODO(crbug.com/1425939): Remove this branch.
  //
  // The attachment concept is going away [1], at which point only local
  // timelines will be reachable from JS, so we don't care about non-local
  // timelines.
  //
  // A new concept similar to non-local timelines will be introduced, but such
  // timelines will not be exposed to JS, and therefore the strong reference in
  // blink::CSSAnimations is enough to keep the timeline alive.
  //
  // [1] https://github.com/w3c/csswg-drafts/issues/7759
  return nullptr;
}

// TODO(crbug.com/1060384): This section is missing from the spec rewrite.
// Resolved to remove the before and after phases in
// https://github.com/w3c/csswg-drafts/issues/7240.
// https://drafts.csswg.org/scroll-animations-1/#current-time-algorithm
ScrollTimeline::TimelineState ScrollTimeline::ComputeTimelineState() const {
  TimelineState state;
  state.resolved_source = ComputeResolvedSource();

  // 1. If scroll timeline is inactive, return an unresolved time value.
  // https://github.com/WICG/scroll-animations/issues/31
  // https://wicg.github.io/scroll-animations/#current-time-algorithm
  if (!ComputeIsResolved(state.resolved_source)) {
    return state;
  }
  DCHECK(state.resolved_source);
  LayoutBox* layout_box = state.resolved_source->GetLayoutBox();

  // Layout box and scrollable area must exist since the timeline is active.
  DCHECK(layout_box);
  DCHECK(layout_box->GetScrollableArea());

  // Depending on the writing-mode and direction, the scroll origin shifts and
  // the scroll offset may be negative. The easiest way to deal with this is to
  // use only the magnitude of the scroll offset, and compare it to (max_offset
  // - min_offset).
  PaintLayerScrollableArea* scrollable_area = layout_box->GetScrollableArea();

  // Using the absolute value of the scroll offset only makes sense if either
  // the max or min scroll offset for a given axis is 0. This should be
  // guaranteed by the scroll origin code, but these DCHECKs ensure that.
  DCHECK(scrollable_area->MaximumScrollOffset().y() == 0 ||
         scrollable_area->MinimumScrollOffset().y() == 0);
  DCHECK(scrollable_area->MaximumScrollOffset().x() == 0 ||
         scrollable_area->MinimumScrollOffset().x() == 0);

  ScrollOffset scroll_offset = scrollable_area->GetScrollOffset();
  auto physical_orientation =
      ToPhysicalScrollOrientation(GetAxis(), *layout_box);
  double current_offset = (physical_orientation == kHorizontalScroll)
                              ? scroll_offset.x()
                              : scroll_offset.y();
  // When using a rtl direction, current_offset grows correctly from 0 to
  // max_offset, but is negative. Since our offsets are all just deltas along
  // the orientation direction, we can just take the absolute current_offset and
  // use that everywhere.
  current_offset = std::abs(current_offset);

  CalculateOffsets(scrollable_area, physical_orientation, &state);
  DCHECK(state.scroll_offsets);

  state.zoom = layout_box->StyleRef().EffectiveZoom();
  // Timeline is inactive unless the scroll offset range is positive.
  // github.com/w3c/csswg-drafts/issues/7401
  if (std::abs(state.scroll_offsets->end - state.scroll_offsets->start) > 0) {
    state.phase = TimelinePhase::kActive;
    double progress = (current_offset - state.scroll_offsets->start) /
                      (state.scroll_offsets->end - state.scroll_offsets->start);

    base::TimeDelta duration = base::Seconds(GetDuration()->InSecondsF());
    state.current_time =
        base::Milliseconds(progress * duration.InMillisecondsF());
  }
  return state;
}

void ScrollTimeline::CalculateOffsets(PaintLayerScrollableArea* scrollable_area,
                                      ScrollOrientation physical_orientation,
                                      TimelineState* state) const {
  ScrollOffset scroll_dimensions = scrollable_area->MaximumScrollOffset() -
                                   scrollable_area->MinimumScrollOffset();
  double end_offset = physical_orientation == kHorizontalScroll
                          ? scroll_dimensions.x()
                          : scroll_dimensions.y();
  state->scroll_offsets = absl::make_optional<ScrollOffsets>(0, end_offset);
}

// Scroll-linked animations are initialized with the start time of zero.
absl::optional<base::TimeDelta>
ScrollTimeline::InitialStartTimeForAnimations() {
  return base::TimeDelta();
}

AnimationTimeDelta ScrollTimeline::CalculateIntrinsicIterationDuration(
    const TimelineRange& timeline_range,
    const absl::optional<TimelineOffset>& range_start,
    const absl::optional<TimelineOffset>& range_end,
    const Timing& timing) {
  absl::optional<AnimationTimeDelta> duration = GetDuration();

  // Only run calculation for progress based scroll timelines
  if (duration && timing.iteration_count > 0) {
    double active_interval = 1;

    double start = range_start
                       ? timeline_range.ToFractionalOffset(range_start.value())
                       : 0;
    double end =
        range_end ? timeline_range.ToFractionalOffset(range_end.value()) : 1;

    active_interval -= start;
    active_interval -= (1 - end);
    active_interval = std::max(0., active_interval);

    // Start and end delays are proportional to the active interval.
    double start_delay = timing.start_delay.relative_delay.value_or(0);
    double end_delay = timing.end_delay.relative_delay.value_or(0);
    double delay = start_delay + end_delay;

    if (delay >= 1) {
      return AnimationTimeDelta();
    }

    active_interval *= (1 - delay);
    return duration.value() * active_interval / timing.iteration_count;
  }
  return AnimationTimeDelta();
}

TimelineRange ScrollTimeline::GetTimelineRange() const {
  absl::optional<ScrollOffsets> scroll_offsets = GetResolvedScrollOffsets();
  return scroll_offsets.has_value() ? TimelineRange(scroll_offsets.value())
                                    : TimelineRange();
}

void ScrollTimeline::ServiceAnimations(TimingUpdateReason reason) {
  // When scroll timeline goes from inactive to active the animations may need
  // to be started and possibly composited.
  bool was_active =
      last_current_phase_and_time_ &&
      last_current_phase_and_time_.value().phase == TimelinePhase::kActive;
  if (!was_active && IsActive())
    MarkAnimationsCompositorPending();

  AnimationTimeline::ServiceAnimations(reason);
}

bool ScrollTimeline::ShouldScheduleNextService() {
  if (AnimationsNeedingUpdateCount() == 0)
    return false;

  auto state = ComputeTimelineState();
  PhaseAndTime current_phase_and_time{state.phase, state.current_time};
  return current_phase_and_time != last_current_phase_and_time_;
}

void ScrollTimeline::ScheduleNextService() {
  // See DocumentAnimations::UpdateAnimations() for why we shouldn't reach here.
  NOTREACHED();
}

void ScrollTimeline::UpdateSnapshot() {
  auto state = ComputeTimelineState();
  bool invalidate_timing =
      !state.HasConsistentLayout(timeline_state_snapshotted_);
  timeline_state_snapshotted_ = state;
  if (invalidate_timing) {
    for (Animation* animation : GetAnimations()) {
      animation->InvalidateNormalizedTiming();
    }
  }
  ResolveTimelineOffsets();
}

Element* ScrollTimeline::source() const {
  return CurrentAttachment() ? CurrentAttachment()->ComputeSource() : nullptr;
}

void ScrollTimeline::AnimationAttached(Animation* animation) {
  if (RetainingElement() && !HasAnimations()) {
    RetainingElement()->RegisterScrollTimeline(this);
  }

  AnimationTimeline::AnimationAttached(animation);
}

void ScrollTimeline::AnimationDetached(Animation* animation) {
  AnimationTimeline::AnimationDetached(animation);

  if (RetainingElement() && !HasAnimations()) {
    RetainingElement()->UnregisterScrollTimeline(this);
  }
}

void ScrollTimeline::WorkletAnimationAttached(WorkletAnimationBase* worklet) {
  if (!ResolvedSource()) {
    return;
  }
  attached_worklet_animations_.insert(worklet);
}

Node* ScrollTimeline::ComputeResolvedSource() const {
  if (!CurrentAttachment()) {
    return nullptr;
  }
  return ResolveSource(CurrentAttachment()->ComputeSourceNoLayout());
}

bool ScrollTimeline::ComputeIsResolved(Node* resolved_source) {
  LayoutBox* layout_box =
      resolved_source ? resolved_source->GetLayoutBox() : nullptr;
  return layout_box && layout_box->IsScrollContainer();
}

void ScrollTimeline::Trace(Visitor* visitor) const {
  visitor->Trace(timeline_state_snapshotted_);
  visitor->Trace(attached_worklet_animations_);
  visitor->Trace(attachments_);
  AnimationTimeline::Trace(visitor);
  ScrollSnapshotClient::Trace(visitor);
}

bool ScrollTimeline::Matches(TimelineAttachment attachment_type,
                             ReferenceType reference_type,
                             Element* reference_element,
                             ScrollAxis axis) const {
  if (attachment_type_ == TimelineAttachment::kDefer) {
    return attachment_type == TimelineAttachment::kDefer;
  }
  const ScrollTimelineAttachment* attachment = CurrentAttachment();
  DCHECK(attachment);
  return (attachment_type_ == attachment_type) &&
         (attachment->GetReferenceType() == reference_type) &&
         (attachment->GetReferenceElement() == reference_element) &&
         (attachment->GetAxis() == axis);
}

ScrollAxis ScrollTimeline::GetAxis() const {
  if (const ScrollTimelineAttachment* attachment = CurrentAttachment()) {
    return attachment->GetAxis();
  }
  return ScrollAxis::kBlock;
}

void ScrollTimeline::InvalidateEffectTargetStyle() const {
  for (Animation* animation : GetAnimations()) {
    animation->InvalidateEffectTargetStyle();
  }
}

bool ScrollTimeline::ValidateSnapshot() {
  TimelineState new_state = ComputeTimelineState();

  if (timeline_state_snapshotted_.HasConsistentLayout(new_state)) {
    return true;
  }

  // Note that `timeline_state_snapshotted_` must be updated before
  // ResolveTimelineOffsets is called.
  timeline_state_snapshotted_ = new_state;
  ResolveTimelineOffsets();

  // Mark an attached animation's target as dirty if the play state is running
  // or finished. Idle animations are not in effect and the effect of a paused
  // animation is not impacted by timeline staleness.
  for (Animation* animation : GetAnimations()) {
    Animation::AnimationPlayState play_state =
        animation->CalculateAnimationPlayState();
    if (play_state != Animation::kRunning &&
        play_state != Animation::kFinished) {
      continue;
    }

    // The animation's effect target requires a style update to ensure that we
    // pickup the new effect value.
    animation->InvalidateEffectTargetStyle();
    // Normalized timing needs to be reevaluated since the intrinsic iteration
    // duration may be affected.
    animation->InvalidateNormalizedTiming();
    // The animation range may be affected, which in turn can affect animation
    // start time as well as intrinsic iteration duration.
    animation->OnRangeUpdate();
  }

  return false;
}

void ScrollTimeline::AddAttachment(ScrollTimelineAttachment* attachment) {
  DCHECK_EQ(attachment_type_, TimelineAttachment::kDefer);
  attachments_.push_back(attachment);
}

void ScrollTimeline::RemoveAttachment(ScrollTimelineAttachment* attachment) {
  DCHECK_EQ(attachment_type_, TimelineAttachment::kDefer);
  wtf_size_t i = attachments_.Find(attachment);
  if (i != kNotFound) {
    attachments_.EraseAt(i);
  }
}

cc::AnimationTimeline* ScrollTimeline::EnsureCompositorTimeline() {
  if (compositor_timeline_)
    return compositor_timeline_.get();

  compositor_timeline_ = scroll_timeline_util::ToCompositorScrollTimeline(this);
  return compositor_timeline_.get();
}

void ScrollTimeline::UpdateCompositorTimeline() {
  if (!compositor_timeline_)
    return;

  ToScrollTimeline(compositor_timeline_.get())
      ->UpdateScrollerIdAndScrollOffsets(
          scroll_timeline_util::GetCompositorScrollElementId(ResolvedSource()),
          GetResolvedScrollOffsets());
}

}  // namespace blink
