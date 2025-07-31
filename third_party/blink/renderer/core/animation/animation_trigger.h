// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TRIGGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TRIGGER_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_animation_trigger_behavior.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_animation_trigger_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_timelinerangeoffset.h"
#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/scroll_snapshot_timeline.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExecutionContext;

// The state of the animation's trigger.
// https://drafts.csswg.org/web-animations-2/#trigger-state
enum class AnimationTriggerState {
  // The initial state of the trigger. The trigger has not yet taken any action
  // on the animation.
  kIdle,
  // The last action taken by the trigger on the animation was due to entering
  // the trigger range.
  kPrimary,
  // The last action taken by the trigger on the animation was due to exiting
  // the exit range.
  kInverse,
};

class CORE_EXPORT AnimationTrigger : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using RangeBoundary = V8UnionStringOrTimelineRangeOffset;
  using Behavior = V8AnimationTriggerBehavior;
  using State = AnimationTriggerState;

  AnimationTrigger(AnimationTimeline* timeline,
                   Behavior behavior,
                   RangeBoundary* range_start,
                   RangeBoundary* range_end,
                   RangeBoundary* exit_range_start,
                   RangeBoundary* exit_range_end);
  static AnimationTrigger* Create(ExecutionContext* execution_context,
                                  AnimationTriggerOptions* options,
                                  ExceptionState& exception_state);

  Behavior behavior() { return behavior_; }

  AnimationTimeline* timeline() {
    return timeline_.Get() ? timeline_.Get()->ExposedTimeline() : nullptr;
  }
  AnimationTimeline* GetTimelineInternal() { return timeline_.Get(); }

  const RangeBoundary* rangeStart(ExecutionContext* execution_context);
  const RangeBoundary* rangeEnd(ExecutionContext* execution_context);
  const RangeBoundary* exitRangeStart(ExecutionContext* execution_context);
  const RangeBoundary* exitRangeEnd(ExecutionContext* execution_context);

  void setRangeBoundariesForTest(RangeBoundary* start,
                                 RangeBoundary* exit,
                                 RangeBoundary* exit_start,
                                 RangeBoundary* exit_end) {
    range_start_ = start;
    range_end_ = exit;
    exit_range_start_ = exit_start;
    exit_range_end_ = exit_end;
  }

  State GetState() { return state_; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(timeline_);
    visitor->Trace(range_start_);
    visitor->Trace(range_end_);
    visitor->Trace(exit_range_start_);
    visitor->Trace(exit_range_end_);
    visitor->Trace(animations_);
    ScriptWrappable::Trace(visitor);
  }

  using TimelineState = ScrollSnapshotTimeline::TimelineState;

  static Behavior ToV8TriggerBehavior(EAnimationTriggerBehavior behavior) {
    switch (behavior) {
      case EAnimationTriggerBehavior::kOnce:
        return Behavior(Behavior::Enum::kOnce);
      case EAnimationTriggerBehavior::kRepeat:
        return Behavior(Behavior::Enum::kRepeat);
      case EAnimationTriggerBehavior::kAlternate:
        return Behavior(Behavior::Enum::kAlternate);
      case EAnimationTriggerBehavior::kState:
        return Behavior(Behavior::Enum::kState);
      default:
        NOTREACHED();
    };
  }

  // Structure representing the scroll offsets (in px) corresponding to the
  // boundaries of the trigger (default) range and the exit range;
  struct TriggerBoundaries {
    // The start offset of the trigger/default range.
    double start;
    // The end offset of the trigger/default range.
    double end;
    // The start offset of the exit range.
    double exit_start;
    // The end offset of the exit range.
    double exit_end;
    double current_offset;
  };

  enum class UpdateType { kNone, kPlay, kPause, kReverse, kUnpause, kReset };

  TriggerBoundaries ComputeTriggerBoundaries(double current_offset,
                                             Element& timeline_source,
                                             const ScrollTimeline& timeline);
  std::optional<AnimationTrigger::TriggerBoundaries>
  CalculateTriggerBoundaries();
  std::optional<AnimationTrigger::State> ComputeState();

  void addAnimation(Animation* animation, ExceptionState& exception_state);
  void removeAnimation(Animation* animation);

  void Update();
  void UpdateInternal(State old_state, State new_state);
  void UpdateAnimations(UpdateType update_type);

  bool CanTrigger();

 private:
  // Handles playing an animation which is added to a trigger which has already
  // tripped.
  void HandlePostTripAdd(Animation* animation, ExceptionState& exception_state);

  Member<AnimationTimeline> timeline_;
  Behavior behavior_;
  // The range boundaries at which the trigger takes action, in CSS pixels.
  Member<const RangeBoundary> range_start_;
  Member<const RangeBoundary> range_end_;
  Member<const RangeBoundary> exit_range_start_;
  Member<const RangeBoundary> exit_range_end_;

  State state_;
  HeapHashSet<WeakMember<Animation>> animations_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TRIGGER_H_
