// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_SNAPSHOT_TIMELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_SNAPSHOT_TIMELINE_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_axis.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/scroll/scroll_snapshot_client.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"

namespace blink {

// See ScrollTimeline.
class CORE_EXPORT ScrollSnapshotTimeline : public AnimationTimeline,
                                           public ScrollSnapshotClient {
 public:
  using ScrollOffsets = cc::ScrollTimeline::ScrollOffsets;
  using ScrollAxis = V8ScrollAxis::Enum;
  using ViewOffsets = TimelineRange::ViewOffsets;

  explicit ScrollSnapshotTimeline(Document*);

  bool IsScrollSnapshotTimeline() const override { return true; }

  // ScrollTimeline is not resolved if source is null, does not currently
  // have a CSS layout box, or if its layout box is not a scroll container.
  bool IsResolved() const override;

  // ScrollTimeline is not active if not resolved or if the current time is
  // unresolved (e.g. before the timeline ticks).
  // https://github.com/WICG/scroll-animations/issues/31
  bool IsActive() const override;

  absl::optional<base::TimeDelta> InitialStartTimeForAnimations() override;

  TimelineRange GetTimelineRange() const override;

  AnimationTimeDelta ZeroTime() override { return AnimationTimeDelta(); }

  void ServiceAnimations(TimingUpdateReason) override;
  void ScheduleNextService() override;

  V8CSSNumberish* currentTime() override;
  V8CSSNumberish* duration() override;
  V8CSSNumberish* ConvertTimeToProgress(AnimationTimeDelta time) const;

  // Returns the Node that should actually have the ScrollableArea (if one
  // exists). This can differ from |source| when defaulting to the
  // Document's scrollingElement, and it may be null if the document was
  // removed before the ScrollTimeline was created.
  Node* ResolvedSource() const {
    return timeline_state_snapshotted_.resolved_source;
  }

  // Return the latest resolved scroll/view offsets. This will be empty when
  // timeline is inactive.
  absl::optional<ScrollOffsets> GetResolvedScrollOffsets() const;
  absl::optional<ViewOffsets> GetResolvedViewOffsets() const;

  float GetResolvedZoom() const { return timeline_state_snapshotted_.zoom; }

  // Mark every effect target of every Animation attached to this timeline
  // for style recalc.
  void InvalidateEffectTargetStyle() const;

  void Trace(Visitor*) const override;

  // Duration is the maximum value a timeline may generate for current time.
  // Used to convert time values to proportional values.
  absl::optional<AnimationTimeDelta> GetDuration() const override {
    // Any arbitrary value should be able to be used here.
    return absl::make_optional(ANIMATION_TIME_DELTA_FROM_SECONDS(100));
  }

  void ResolveTimelineOffsets() const;

  cc::AnimationTimeline* EnsureCompositorTimeline() override;
  void UpdateCompositorTimeline() override;

  virtual ScrollAxis GetAxis() const = 0;

 protected:
  PhaseAndTime CurrentPhaseAndTime() override;

  AnimationTimeDelta CalculateIntrinsicIterationDuration(
      const TimelineRange&,
      const absl::optional<TimelineOffset>& range_start,
      const absl::optional<TimelineOffset>& range_end,
      const Timing&) override;

  static bool ComputeIsResolved(Node* resolved_source);

  struct TimelineState {
    DISALLOW_NEW();

   public:
    // TODO(crbug.com/1338167): Remove phase as it can be inferred from
    // current_time.
    TimelinePhase phase = TimelinePhase::kInactive;
    absl::optional<base::TimeDelta> current_time;
    absl::optional<ScrollOffsets> scroll_offsets;
    // The view offsets will be null unless using a view timeline.
    absl::optional<ViewOffsets> view_offsets;
    // Zoom factor applied to the scroll offsets.
    float zoom = 1.0f;
    // The scroller driving the timeline.
    Member<Node> resolved_source;

    bool HasConsistentLayout(const TimelineState& other) const {
      return scroll_offsets == other.scroll_offsets && zoom == other.zoom &&
             view_offsets == other.view_offsets;
    }

    bool operator==(const TimelineState& other) const {
      return phase == other.phase && current_time == other.current_time &&
             scroll_offsets == other.scroll_offsets && zoom == other.zoom &&
             view_offsets == other.view_offsets &&
             resolved_source == other.resolved_source;
    }

    void Trace(blink::Visitor* visitor) const {
      visitor->Trace(resolved_source);
    }
  };

  // ScrollSnapshotClient:
  // https://wicg.github.io/scroll-animations/#avoiding-cycles
  // Snapshots scroll timeline current time and phase.
  // Called once per animation frame.
  void UpdateSnapshot() override;
  bool ValidateSnapshot() override;
  bool ShouldScheduleNextService() override;

 public:
  // Public for DeferredTimeline::ComputeTimelineState.
  virtual TimelineState ComputeTimelineState() const = 0;

 private:
  // Snapshotted value produced by the last SnapshotState call.
  TimelineState timeline_state_snapshotted_;
};

template <>
struct DowncastTraits<ScrollSnapshotTimeline> {
  static bool AllowFrom(const AnimationTimeline& value) {
    return value.IsScrollSnapshotTimeline();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_SNAPSHOT_TIMELINE_H_
