// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/scroll_snapshot_timeline.h"

#include <optional>

#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_double.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline_util.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_values.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/layout/forms/layout_fieldset.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"

namespace blink {

ScrollSnapshotTimeline::ScrollSnapshotTimeline(Document* document)
    : AnimationTimeline(document), ScrollSnapshotClient(document->GetFrame()) {}

bool ScrollSnapshotTimeline::IsResolved() const {
  return ScrollContainer();
}

bool ScrollSnapshotTimeline::IsActive() const {
  return timeline_state_snapshotted_.phase != TimelinePhase::kInactive;
}

std::optional<ScrollOffsets> ScrollSnapshotTimeline::GetResolvedScrollOffsets()
    const {
  return timeline_state_snapshotted_.scroll_offsets;
}

std::optional<ScrollSnapshotTimeline::ViewOffsets>
ScrollSnapshotTimeline::GetResolvedViewOffsets() const {
  return timeline_state_snapshotted_.view_offsets;
}

// TODO(crbug.com/1336260): Since phase can only be kActive or kInactive and
// currentTime  can only be null if phase is inactive or before the first
// snapshot we can probably drop phase.
AnimationTimeline::PhaseAndTime ScrollSnapshotTimeline::CurrentPhaseAndTime() {
  return {timeline_state_snapshotted_.phase,
          timeline_state_snapshotted_.current_time};
}

V8CSSNumberish* ScrollSnapshotTimeline::ConvertTimeToProgress(
    AnimationTimeDelta time) const {
  return MakeGarbageCollected<V8CSSNumberish>(
      CSSUnitValues::percent((time / GetDuration().value()) * 100));
}

V8CSSNumberish* ScrollSnapshotTimeline::currentTime() {
  // Compute time as a percentage based on the relative scroll position, where
  // the start offset corresponds to 0% and the end to 100%.
  auto current_time = timeline_state_snapshotted_.current_time;

  if (current_time) {
    return ConvertTimeToProgress(AnimationTimeDelta(current_time.value()));
  }
  return nullptr;
}

V8CSSNumberish* ScrollSnapshotTimeline::duration() {
  return MakeGarbageCollected<V8CSSNumberish>(CSSUnitValues::percent(100));
}

void ScrollSnapshotTimeline::ResolveTimelineOffsets() const {
  TimelineRange timeline_range = GetTimelineRange();
  for (Animation* animation : GetAnimations()) {
    animation->ResolveTimelineOffsets(timeline_range);
  }
}

// Scroll-linked animations are initialized with the start time of zero.
std::optional<base::TimeDelta>
ScrollSnapshotTimeline::InitialStartTimeForAnimations() {
  return base::TimeDelta();
}

AnimationTimeDelta ScrollSnapshotTimeline::CalculateIntrinsicIterationDuration(
    const TimelineRange& timeline_range,
    const std::optional<TimelineOffset>& range_start,
    const std::optional<TimelineOffset>& range_end,
    const Timing& timing) {
  std::optional<AnimationTimeDelta> duration = GetDuration();

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

TimelineRange ScrollSnapshotTimeline::GetTimelineRange() const {
  std::optional<ScrollOffsets> scroll_offsets = GetResolvedScrollOffsets();

  if (!scroll_offsets.has_value()) {
    return TimelineRange();
  }

  std::optional<ViewOffsets> view_offsets = GetResolvedViewOffsets();

  return TimelineRange(scroll_offsets.value(), view_offsets.has_value()
                                                   ? view_offsets.value()
                                                   : ViewOffsets());
}

void ScrollSnapshotTimeline::ServiceAnimations(TimingUpdateReason reason) {
  // When scroll timeline goes from inactive to active the animations may need
  // to be started and possibly composited.
  bool was_active =
      last_current_phase_and_time_ &&
      last_current_phase_and_time_.value().phase == TimelinePhase::kActive;
  if (!was_active && IsActive()) {
    MarkAnimationsCompositorPending();
  }

  AnimationTimeline::ServiceAnimations(reason);
}

bool ScrollSnapshotTimeline::ShouldScheduleNextService() {
  if (AnimationsNeedingUpdateCount() == 0) {
    return false;
  }

  auto state = ComputeTimelineState();
  PhaseAndTime current_phase_and_time{state.phase, state.current_time};
  return current_phase_and_time != last_current_phase_and_time_;
}

void ScrollSnapshotTimeline::ScheduleNextService() {
  // See DocumentAnimations::UpdateAnimations() for why we shouldn't reach here.
  NOTREACHED_IN_MIGRATION();
}

void ScrollSnapshotTimeline::UpdateSnapshot() {
  auto state = ComputeTimelineState();
  bool layout_changed = !state.HasConsistentLayout(timeline_state_snapshotted_);
  timeline_state_snapshotted_ = state;

  if (layout_changed) {
    // Force recalculation of an auto-aligned start time, and invalidate
    // normalized timing.
    for (Animation* animation : GetAnimations()) {
      // Avoid setting a deferred start time during the update snapshot phase.
      // Instead wait for the validation phase post layout.
      if (!animation->CurrentTimeInternal()) {
        continue;
      }
      animation->OnValidateSnapshot(layout_changed);
    }
  }
  ResolveTimelineOffsets();
}

LayoutBox* ScrollSnapshotTimeline::ComputeScrollContainer(
    Node* resolved_source) {
  if (!resolved_source) {
    return nullptr;
  }

  LayoutBox* layout_box = resolved_source->GetLayoutBox();
  if (!layout_box) {
    return nullptr;
  }

  if (auto* field_set = DynamicTo<LayoutFieldset>(layout_box)) {
    layout_box = field_set->FindAnonymousFieldsetContentBox();
  }

  return layout_box->IsScrollContainer() ? layout_box : nullptr;
}

void ScrollSnapshotTimeline::Trace(Visitor* visitor) const {
  visitor->Trace(timeline_state_snapshotted_);
  AnimationTimeline::Trace(visitor);
  ScrollSnapshotClient::Trace(visitor);
}

void ScrollSnapshotTimeline::InvalidateEffectTargetStyle() const {
  for (Animation* animation : GetAnimations()) {
    animation->InvalidateEffectTargetStyle();
  }
}

bool ScrollSnapshotTimeline::ValidateSnapshot() {
  TimelineState new_state = ComputeTimelineState();
  bool is_valid = timeline_state_snapshotted_ == new_state;
  bool state_changed =
      !timeline_state_snapshotted_.HasConsistentLayout(new_state);
  // Note that `timeline_state_snapshotted_` must be updated before
  // ResolveTimelineOffsets is called.
  timeline_state_snapshotted_ = new_state;
  if (state_changed) {
    ResolveTimelineOffsets();
  }

  for (Animation* animation : GetAnimations()) {
    // Compute deferred start times and update animation timing if required.
    is_valid &= animation->OnValidateSnapshot(state_changed);
  }

  return is_valid;
}

cc::AnimationTimeline* ScrollSnapshotTimeline::EnsureCompositorTimeline() {
  if (compositor_timeline_) {
    return compositor_timeline_.get();
  }

  compositor_timeline_ = scroll_timeline_util::ToCompositorScrollTimeline(this);
  return compositor_timeline_.get();
}

void ScrollSnapshotTimeline::UpdateCompositorTimeline() {
  if (!compositor_timeline_) {
    return;
  }

  ToScrollTimeline(compositor_timeline_.get())
      ->UpdateScrollerIdAndScrollOffsets(
          scroll_timeline_util::GetCompositorScrollElementId(ResolvedSource()),
          GetResolvedScrollOffsets());
}

}  // namespace blink
