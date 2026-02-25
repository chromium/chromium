// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_TRIGGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_TRIGGER_H_

#include "cc/animation/timeline_trigger.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_trigger_options.h"
#include "third_party/blink/renderer/core/animation/animation_trigger.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/animation/timeline_trigger_range_list.h"

namespace blink {

struct TimelineTriggerAction {
  // The action to take when entering the trigger range.
  static constexpr const char kEnter[] = "enter";
  // The action to take when exiting the exit range.
  static constexpr const char kExit[] = "exit";
};

class CORE_EXPORT TimelineTrigger : public AnimationTrigger {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using State = TimelineTriggerRange::State;
  using RangeBoundary = TimelineTriggerRange::Boundary;
  using TriggerBoundaries = TimelineTriggerRange::TriggerBoundaries;
  using CcBoundaries = cc::TimelineTrigger::Boundaries;

  TimelineTrigger(TimelineTriggerRangeList* ranges,
                  Element* owning_element = nullptr);
  static TimelineTrigger* Create(
      ExecutionContext* execution_context,
      const HeapVector<Member<TimelineTriggerOptions>>& options,
      ExceptionState& exception_state);

  // IDL interface
  TimelineTriggerRangeList* ranges() { return ranges_; }

  // TODO(crbug.com/473568234): The TimelineTrigger interface supports multiple
  // timelines, but at the moment we only support a single timeline.
  // These methods simplify the TimelineTrigger implementation given that we
  // only support a single timeline. When we support multiple timelines these
  // methods should be deleted.
  AnimationTimeline* Timeline() {
    return GetRange() ? GetRange()->timeline() : nullptr;
  }
  const AnimationTimeline* Timeline() const {
    return GetRange() ? GetRange()->timeline() : nullptr;
  }
  const RangeBoundary* ActivationRangeStart() {
    return GetRange() ? GetRange()->activationRangeStart(nullptr) : nullptr;
  }
  const RangeBoundary* ActivationRangeEnd() {
    return GetRange() ? GetRange()->activationRangeEnd(nullptr) : nullptr;
  }
  const RangeBoundary* ActiveRangeStart() {
    return GetRange() ? GetRange()->activeRangeStart(nullptr) : nullptr;
  }
  const RangeBoundary* ActiveRangeEnd() {
    return GetRange() ? GetRange()->activeRangeEnd(nullptr) : nullptr;
  }
  AnimationTimeline* GetTimelineInternal() {
    return GetRange() ? GetRange()->GetTimelineInternal() : nullptr;
  }
  void SetRangeBoundariesForTest(RangeBoundary* activation_start,
                                 RangeBoundary* activation_end,
                                 RangeBoundary* active_start,
                                 RangeBoundary* active_end) {
    // TODO(crbug.com/473568234): Support multiple timelines.
    if (TimelineTriggerRange* range = GetRange()) {
      range->SetRangeBoundariesForTest(activation_start, activation_end,
                                       active_start, active_end);
    }
  }
  TriggerBoundaries ComputeTriggerBoundariesForTest(
      double current_offset,
      Element& timeline_source,
      const ScrollTimeline& timeline) {
    // TODO(crbug.com/473568234): Support multiple timelines.
    return GetRange() ? GetRange()->ComputeTriggerBoundariesForTest(
                            current_offset, timeline_source, timeline)
                      : TriggerBoundaries();
  }

  bool CanTrigger() const override;
  bool IsTimelineTrigger() const override;

  State GetState() { return state_; }
  bool Update();

  void Trace(Visitor* visitor) const override;

  void CreateCompositorTrigger() override;

  // TODO(crbug.com/473568234): Support multiple timelines.
  std::optional<CcBoundaries> ComputeCcBoundaries(
      cc::AnimationTimeline* cc_timeline) {
    return GetRange() ? GetRange()->ComputeCcBoundaries(cc_timeline)
                      : std::nullopt;
  }

  Document* GetDocument() override {
    return Timeline() ? Timeline()->GetDocument() : nullptr;
  }

  using TimelineState = ScrollSnapshotTimeline::TimelineState;

 private:
  void WillAddAnimation(Animation* animation,
                        Behavior activate_behavior,
                        Behavior deactivate_behavior,
                        ExceptionState& exception_state) override;
  void DidAddAnimation() override;
  void DidRemoveAnimation(Animation* animation) override;

  // These functions are called when the cc::AnimationTrigger associated with
  // this trigger observes the related conditions on the compositor thread.
  void NotifyActivated() override;
  void NotifyDeactivated() override;

  void HandlePostTripAdd(Animation* animation,
                         Behavior activate_behavior,
                         Behavior deactivate_behavior,
                         ExceptionState& exception_state);
  // TODO(crbug.com/473568234): Support multiple timelines/ranges.
  TimelineTriggerRange* GetRange() const {
    return ranges_ && ranges_->Ranges().size() ? ranges_->Ranges()[0] : nullptr;
  }

  std::optional<TimelineTrigger::State> ComputeState();

  Member<TimelineTriggerRangeList> ranges_;

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
