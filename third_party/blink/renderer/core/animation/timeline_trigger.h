// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_TRIGGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_TRIGGER_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_trigger_options.h"
#include "third_party/blink/renderer/core/animation/animation_trigger.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"

namespace blink {

// https://drafts.csswg.org/web-animations-2/#trigger-state
enum class TimelineTriggerState {
  // The initial state of the trigger. The trigger has not yet taken any action.
  kIdle,
  // The last action taken by the trigger was due to entering the trigger range.
  kPrimary,
  // The last action taken by the trigger was due to exiting the exit range.
  kInverse,
};

struct TimelineTriggerAction {
  // The action to take when entering the trigger range.
  static constexpr const char kEnter[] = "enter";
  // The action to take when exiting the exit range.
  static constexpr const char kExit[] = "exit";
};

class CORE_EXPORT TimelineTrigger : public AnimationTrigger {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using RangeBoundary = V8UnionStringOrTimelineRangeOffset;
  using State = TimelineTriggerState;

  TimelineTrigger(AnimationTimeline* timeline,
                  RangeBoundary* range_start,
                  RangeBoundary* range_end,
                  RangeBoundary* exit_range_start,
                  RangeBoundary* exit_range_end,
                  Element* owning_element = nullptr);
  static TimelineTrigger* Create(ExecutionContext* execution_context,
                                 TimelineTriggerOptions* options,
                                 ExceptionState& exception_state);

  // IDL interface
  AnimationTimeline* timeline();
  const RangeBoundary* rangeStart(ExecutionContext* execution_context);
  const RangeBoundary* rangeEnd(ExecutionContext* execution_context);
  const RangeBoundary* exitRangeStart(ExecutionContext* execution_context);
  const RangeBoundary* exitRangeEnd(ExecutionContext* execution_context);

  bool CanTrigger() const override;
  bool IsTimelineTrigger() const override;

  AnimationTimeline* GetTimelineInternal() { return timeline_.Get(); }
  State GetState() { return state_; }
  void Update();

  void Trace(Visitor* visitor) const override;

  // Structure representing the scroll offsets (in px) corresponding to the
  // boundaries of the trigger (default) range and the exit range;
  struct TriggerBoundaries {
    // The start offset of the trigger/default range.
    double start = 0.;
    // The end offset of the trigger/default range.
    double end = 0.;
    // The start offset of the exit range.
    double exit_start = 0.;
    // The end offset of the exit range.
    double exit_end = 0.;
    double current_offset = 0.;
  };

  void SetRangeBoundariesForTest(RangeBoundary* start,
                                 RangeBoundary* exit,
                                 RangeBoundary* exit_start,
                                 RangeBoundary* exit_end) {
    range_start_ = start;
    range_end_ = exit;
    exit_range_start_ = exit_start;
    exit_range_end_ = exit_end;
  }

  TriggerBoundaries ComputeTriggerBoundariesForTest(
      double current_offset,
      Element& timeline_source,
      const ScrollTimeline& timeline) {
    return ComputeTriggerBoundaries(current_offset, timeline_source, timeline);
  }

  void CreateCompositorTrigger() override;
  void DestroyCompositorTrigger() override;

  using TimelineState = ScrollSnapshotTimeline::TimelineState;

 private:
  void WillAddAnimation(Animation* animation,
                        Behavior activate_behavior,
                        Behavior deactivate_behavior,
                        ExceptionState& exception_state) override;
  void DidAddAnimation() override;
  void DidRemoveAnimation(Animation* animation) override;

  void HandlePostTripAdd(Animation* animation,
                         Behavior activate_behavior,
                         Behavior deactivate_behavior,
                         ExceptionState& exception_state);

  std::optional<TimelineTrigger::TriggerBoundaries>
  CalculateTriggerBoundaries();

  TriggerBoundaries ComputeTriggerBoundaries(double current_offset,
                                             Element& timeline_source,
                                             const ScrollTimeline& timeline);
  std::optional<TimelineTrigger::State> ComputeState();

  Member<AnimationTimeline> timeline_;
  // The range boundaries at which the trigger takes action, in CSS pixels.
  Member<const RangeBoundary> range_start_;
  Member<const RangeBoundary> range_end_;
  Member<const RangeBoundary> exit_range_start_;
  Member<const RangeBoundary> exit_range_end_;

  State state_;
};

template <>
struct DowncastTraits<TimelineTrigger> {
  static bool AllowFrom(const AnimationTrigger& trigger) {
    return trigger.IsTimelineTrigger();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_TRIGGER_H_
