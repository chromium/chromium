// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_TRIGGER_RANGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_TRIGGER_RANGE_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_trigger_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_timelinerangeoffset.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class AnimationTimeline;
class Element;
class ExecutionContext;
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
// of an entry range and the boundaries of an exit range.
// Currently, we only support one such configuration per TimelineTrigger.
// However, TimelineTrigger will support having multiple such configurations in
// the future: crbug.com/473568234.
class CORE_EXPORT TimelineTriggerRange : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using Boundary = V8UnionStringOrTimelineRangeOffset;
  using State = TimelineTriggerState;

  TimelineTriggerRange(AnimationTimeline* timeline,
                       Boundary* range_start,
                       Boundary* range_end,
                       Boundary* exit_range_start,
                       Boundary* exit_range_end);

  static TimelineTriggerRange* Create(ExecutionContext* execution_context,
                                      const TimelineTriggerOptions* options,
                                      ExceptionState& exception_state);

  // IDL interface
  AnimationTimeline* timeline();
  const Boundary* rangeStart(ExecutionContext* execution_context);
  const Boundary* rangeEnd(ExecutionContext* execution_context);
  const Boundary* exitRangeStart(ExecutionContext* execution_context);
  const Boundary* exitRangeEnd(ExecutionContext* execution_context);

  AnimationTimeline* GetTimelineInternal() { return timeline_.Get(); }

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

  std::optional<State> UpdateState();
  std::optional<State> ComputeState();
  void SetRangeBoundariesForTest(Boundary* start,
                                 Boundary* exit,
                                 Boundary* exit_start,
                                 Boundary* exit_end) {
    range_start_ = start;
    range_end_ = exit;
    exit_range_start_ = exit_start;
    exit_range_end_ = exit_end;
  }

  void Trace(Visitor* visitor) const override;

 private:
  Member<AnimationTimeline> timeline_;
  // The range boundaries at which the trigger takes action, in CSS pixels.
  Member<const Boundary> range_start_;
  Member<const Boundary> range_end_;
  Member<const Boundary> exit_range_start_;
  Member<const Boundary> exit_range_end_;

  // TODO(crbug.com/473568234): While we only support a single
  // TimelineTriggerRange per TimelineTrigger we can simply have the
  // TimelineTriggerRange track its state and have TimelineTrigger read its
  // state from this range. When we support multiple TimelineTriggerRanges, we
  // might want to have only the TimelineTrigger tracking state.
  State last_snapshot_state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_TRIGGER_RANGE_H_
