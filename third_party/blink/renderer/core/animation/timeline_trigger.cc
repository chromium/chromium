// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/timeline_trigger.h"

#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/timeline_trigger.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_animation_trigger_behavior.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_trigger_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_timelinerangeoffset.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/css/css_animation.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/animation/timeline_trigger_range_list.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

TimelineTrigger::TimelineTrigger(TimelineTriggerRangeList* ranges,
                                 Element* owning_element)
    : ranges_(ranges) {
  owning_element_ = owning_element;

  // TODO(crbug.com/473568234): Support multiple timelines.
  if (AnimationTimeline* timeline = Timeline()) {
    timeline->GetDocument()->GetDocumentAnimations().AddAnimationTrigger(*this);
  }

  // A default trigger will need to trip immediately.
  Update();
}

/* static */
TimelineTrigger* TimelineTrigger::Create(
    ExecutionContext* execution_context,
    const HeapVector<Member<TimelineTriggerOptions>>& options_list,
    ExceptionState& exception_state) {
  if (options_list.size() > 1) {
    exception_state.ThrowTypeError(
        "Configuring multiple timelines is not supported.");
    return nullptr;
  }

  return MakeGarbageCollected<TimelineTrigger>(TimelineTriggerRangeList::Create(
      execution_context, options_list, exception_state));
}

std::optional<TimelineTriggerState> TimelineTrigger::ComputeState() {
  return GetRange() ? GetRange()->UpdateState() : std::nullopt;
}

bool TimelineTrigger::Update() {
  std::optional<State> new_state = ComputeState();
  if (!new_state) {
    return false;
  }

  State old_state = state_;
  if (new_state.value() == old_state) {
    return true;
  }

  state_ = *new_state;

  switch (state_) {
    case State::kPrimary:
      PerformActivate();
      break;
    case State::kInverse:
      PerformDeactivate();
      break;
    default:
      NOTREACHED();
  }

  return false;
}

void TimelineTrigger::Trace(Visitor* visitor) const {
  visitor->Trace(ranges_);
  AnimationTrigger::Trace(visitor);
}

void TimelineTrigger::HandlePostTripAdd(Animation* animation,
                                        Behavior activate_behavior,
                                        Behavior deactivate_behavior,
                                        ExceptionState& exception_state) {
  if (state_ == State::kIdle || HasPausedCSSPlayState(animation)) {
    return;
  }

  // If the trigger has already tripped, we want the animation to affected as if
  // it had been added when the tripping event occurred.
  std::optional<Behavior> old_behavior_for_current_state;
  std::optional<Behavior> new_behavior_for_current_state;
  auto old_data_it = BehaviorMap().find(animation);

  switch (state_) {
    case State::kPrimary:
      // We last tripped into "activate"; we might need to act.
      new_behavior_for_current_state = activate_behavior;
      if (old_data_it != BehaviorMap().end()) {
        old_behavior_for_current_state = old_data_it->value.first;
      }
      break;
    case State::kInverse:
      // We last tripped into "deactivate"; we might need to act.
      new_behavior_for_current_state = deactivate_behavior;
      if (old_data_it != BehaviorMap().end()) {
        old_behavior_for_current_state = old_data_it->value.second;
      }
      break;
    default:
      NOTREACHED();
  };

  if (old_behavior_for_current_state != new_behavior_for_current_state) {
    PerformBehavior(*animation, *new_behavior_for_current_state,
                    exception_state);
    animation->UpdateIfNecessary();
  }
}

void TimelineTrigger::WillAddAnimation(Animation* animation,
                                       Behavior activate_behavior,
                                       Behavior deactivate_behavior,
                                       ExceptionState& exception_state) {
  bool was_paused_for_trigger = animation->PausedForTrigger();
  if (animation->CalculateAnimationPlayState() ==
      V8AnimationPlayState::Enum::kIdle) {
    animation->PauseInternal(exception_state);
    if (exception_state.HadException()) {
      return;
    }
    animation->SetPausedForTrigger(true);
    animation->UpdateIfNecessary();
  }

  HandlePostTripAdd(animation, activate_behavior, deactivate_behavior,
                    exception_state);
  if (exception_state.HadException()) {
    animation->SetPausedForTrigger(was_paused_for_trigger);
  }
}

void TimelineTrigger::DidAddAnimation() {
  // TODO(crbug.com/473568234): Support multiple timelines.
  if (Timeline() && BehaviorMap().size() == 1) {
    Timeline()->AddTrigger(this);
  }
}

void TimelineTrigger::DidRemoveAnimation(Animation* animation) {
  // TODO(crbug.com/473568234): Support multiple timelines.
  if (Timeline() && BehaviorMap().empty()) {
    Timeline()->RemoveTrigger(this);
  }
}

bool TimelineTrigger::CanTrigger() const {
  // TODO(crbug.com/473568234): Support multiple timelines.
  return Timeline() && Timeline()->IsActive();
}

bool TimelineTrigger::IsTimelineTrigger() const {
  return true;
}

void TimelineTrigger::CreateCompositorTrigger() {
  if (compositor_trigger_) {
    return;
  }

  cc::AnimationTimeline* cc_timeline =
      Timeline() ? Timeline()->EnsureCompositorTimeline() : nullptr;
  if (!cc_timeline) {
    return;
  }
  GetDocument()->AttachCompositorTimeline(cc_timeline);

  std::optional<CcBoundaries> cc_boundaries = ComputeCcBoundaries(cc_timeline);
  if (!cc_boundaries) {
    return;
  }

  cc::AnimationHost* host = cc_timeline->animation_host();
  CHECK(host);

  scoped_refptr<cc::AnimationTimeline> scopedref_cc_timeline =
      host->GetScopedRefTimelineById(cc_timeline->id());

  scoped_refptr<cc::TimelineTrigger> cc_trigger = cc::TimelineTrigger::Create(
      cc::AnimationIdProvider::NextAnimationTriggerId(), scopedref_cc_timeline,
      *cc_boundaries);
  host->AddTrigger(cc_trigger);

  compositor_trigger_ =
      static_cast<scoped_refptr<cc::AnimationTrigger>>(cc_trigger);
}

void TimelineTrigger::NotifyActivated() {
  state_ = State::kPrimary;
  PerformActivate();
}

void TimelineTrigger::NotifyDeactivated() {
  state_ = State::kInverse;
  PerformDeactivate();
}

}  // namespace blink
