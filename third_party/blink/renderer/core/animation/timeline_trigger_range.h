// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_TRIGGER_RANGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_TRIGGER_RANGE_H_

#include "cc/animation/timeline_trigger.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_trigger_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_timelinerangeoffset.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class AnimationTimeline;
class Element;
class ExecutionContext;
class Node;
class ScrollTimeline;

// https://drafts.csswg.org/web-animations-2/#trigger-state
enum class TimelineTriggerState {
  // The initial state of the trigger. The trigger has not yet taken any action.
  kIdle,
  // The last action taken by the trigger was due to entering the trigger range.
  kPrimary,
  // The last action taken by the trigger was due to exiting the exit range.
  kInverse,
};

// This class encapsulates a single instance of the configuration that a
// TimelineTrigger needs to function, i.e. an AnimationTimeline, the boundaries
// of an activation range and the boundaries of an exit range.
// Currently, we only support one such configuration per TimelineTrigger.
// However, TimelineTrigger will support having multiple such configurations in
// the future: crbug.com/473568234.
class CORE_EXPORT TimelineTriggerRange : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using Boundary = V8UnionStringOrTimelineRangeOffset;
  using State = TimelineTriggerState;
  using CcBoundaries = cc::TimelineTrigger::Boundaries;

  TimelineTriggerRange(AnimationTimeline* timeline,
                       Boundary* activation_range_start,
                       Boundary* activation_range_end,
                       Boundary* active_range_start,
                       Boundary* active_range_end);

  static TimelineTriggerRange* Create(ExecutionContext* execution_context,
                                      const TimelineTriggerOptions* options,
                                      ExceptionState& exception_state);

  // IDL interface
  AnimationTimeline* timeline();
  const Boundary* activationRangeStart(ExecutionContext* execution_context);
  const Boundary* activationRangeEnd(ExecutionContext* execution_context);
  const Boundary* activeRangeStart(ExecutionContext* execution_context);
  const Boundary* activeRangeEnd(ExecutionContext* execution_context);

  AnimationTimeline* GetTimelineInternal() { return timeline_.Get(); }

  // Structure representing the scroll offsets (in px) corresponding to the
  // boundaries of the trigger (default) range and the exit range;
  struct TriggerBoundaries {
    // The start offset of the trigger/default range.
    double activation_start = 0.;
    // The end offset of the trigger/default range.
    double activation_end = 0.;
    // The start offset of the exit range.
    double active_start = 0.;
    // The end offset of the exit range.
    double active_end = 0.;
    double current_offset = 0.;
  };

  static Node* ComputeBoundariesSource(const ScrollTimeline& timeline);
  std::optional<TriggerBoundaries> CalculateTriggerBoundaries();
  TriggerBoundaries ComputeTriggerBoundaries(double current_offset,
                                             Element& timeline_source,
                                             const ScrollTimeline& timeline);
  TriggerBoundaries ComputeTriggerBoundariesForTest(
      double current_offset,
      Element& timeline_source,
      const ScrollTimeline& timeline) {
    return ComputeTriggerBoundaries(current_offset, timeline_source, timeline);
  }

  std::optional<CcBoundaries> ComputeCcBoundaries(
      cc::AnimationTimeline* cc_timeline);

  std::optional<State> UpdateState();
  std::optional<State> ComputeState();
  void SetRangeBoundariesForTest(Boundary* activation_start,
                                 Boundary* activation_end,
                                 Boundary* active_start,
                                 Boundary* active_end) {
    activation_range_start_ = activation_start;
    activation_range_end_ = activation_end;
    active_range_start_ = active_start;
    active_range_end_ = active_end;
  }

  void Trace(Visitor* visitor) const override;

 private:
  Member<AnimationTimeline> timeline_;
  // The range boundaries at which the trigger takes action, in CSS pixels.
  Member<const Boundary> activation_range_start_;
  Member<const Boundary> activation_range_end_;
  Member<const Boundary> active_range_start_;
  Member<const Boundary> active_range_end_;

  // TODO(crbug.com/473568234): While we only support a single
  // TimelineTriggerRange per TimelineTrigger we can simply have the
  // TimelineTriggerRange track its state and have TimelineTrigger read its
  // state from this range. When we support multiple TimelineTriggerRanges, we
  // might want to have only the TimelineTrigger tracking state.
  State last_snapshot_state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_TRIGGER_RANGE_H_
