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
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

TimelineTrigger::TimelineTrigger(TimelineTriggerRangeList* ranges)
    : ranges_(ranges) {
  // TODO(crbug.com/473568234): Support multiple timelines.
  if (AnimationTimeline* timeline = Timeline()) {
    timeline->GetDocument()->GetDocumentAnimations().AddAnimationTrigger(*this);
  }

  UseCounter::Count(GetDocument(), WebFeature::kTimelineTrigger);

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

std::optional<TimelineTrigger::State> TimelineTrigger::ComputeState() {
  return GetRange() ? GetRange()->UpdateState() : std::nullopt;
}

bool TimelineTrigger::Update() {
  if (compositor_trigger_) {
    // If a cc TimelineTrigger exists, it is the source of truth for
    // the trigger's state. The state is synchronized in
    // NotifyActivate/Deactivate.
    return true;
  }

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
    // We are retroactively activating or deactivating the animation. The same
    // restrictions on activation/deactivation should apply.
    base::AutoReset<bool> forbid_updates(&forbid_behavior_map_updates_, true);
    PerformBehavior(*animation, *new_behavior_for_current_state,
                    /*async_event_time=*/std::nullopt, exception_state);
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

  Document* document = GetDocument();
  if (!document || !document->View() ||
      !document->View()->GetCompositorAnimationHost()) {
    return;
  }

  cc::AnimationTimeline* cc_timeline =
      Timeline() ? Timeline()->EnsureCompositorTimeline() : nullptr;
  if (!cc_timeline) {
    return;
  }
  document->AttachCompositorTimeline(cc_timeline);

  std::optional<CcBoundaries> cc_boundaries = ComputeCcBoundaries(cc_timeline);
  if (!cc_boundaries) {
    return;
  }

  cc::AnimationHost* host = cc_timeline->animation_host();
  if (!host) {
    return;
  }

  scoped_refptr<cc::AnimationTimeline> scopedref_cc_timeline =
      host->GetScopedRefTimelineById(cc_timeline->id());

  scoped_refptr<cc::TimelineTrigger> cc_trigger = cc::TimelineTrigger::Create(
      cc::AnimationIdProvider::NextAnimationTriggerId(), state_,
      scopedref_cc_timeline, *cc_boundaries);
  host->AddTrigger(cc_trigger);

  compositor_trigger_ =
      static_cast<scoped_refptr<cc::AnimationTrigger>>(cc_trigger);
}

void TimelineTrigger::NotifyActivated(base::TimeTicks activate_time) {
  pending_compositor_notifications_.push_back(CompositorNotification{
      State::kPrimary, activate_time - base::TimeTicks()});
}

void TimelineTrigger::NotifyDeactivated(base::TimeTicks deactivate_time) {
  pending_compositor_notifications_.push_back(CompositorNotification{
      State::kInverse, deactivate_time - base::TimeTicks()});
}

void TimelineTrigger::ExecuteCompositorNotification(
    const CompositorNotification& notification) {
  state_ = notification.state;
  DCHECK(GetRange());
  // The trigger typically gets its state from the underlying trigger ranges,
  // but when triggering from the compositor, the compositor determines the
  // state of the trigger, so the range is kept in sync with it. Only a single
  // range is supported currently.
  GetRange()->SetState(state_);
  if (notification.state == State::kPrimary) {
    PerformActivate(notification.time);
  } else {
    DCHECK_EQ(notification.state, State::kInverse);
    PerformDeactivate(notification.time);
  }
}

void TimelineTrigger::ExecutePendingCompositorNotifications() {
  // The notifications reach the main thread at the start of a main frame,
  // before "update animations and send events". Syncing them after that step
  // lets the finish event listeners and finished promise callbacks of an
  // animation that finished before the compositor observed the event run
  // before the trigger acts on the animation (e.g. replays it).
  // TODO(crbug.com/451238244): If the compositor acted on an animation shortly
  // before its end, "update animations and send events" can report a finish
  // that the trigger's action preempted.
  Vector<CompositorNotification> notifications;
  notifications.swap(pending_compositor_notifications_);
  for (const auto& notification : notifications) {
    ExecuteCompositorNotification(notification);
  }
}

}  // namespace blink
