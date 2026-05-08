// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animation_trigger.h"

#include "base/time/time.h"
#include "cc/animation/animation_id_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/css/css_animation.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation.h"
#include "ui/gfx/animation/keyframe/keyframe_model.h"

namespace blink {

using Behavior = AnimationTrigger::Behavior;

namespace {

// Behavior implementations per
// https://github.com/w3c/csswg-drafts/issues/12611#issue-3326243729

void PerformPlay(Animation& animation,
                 std::optional<base::TimeDelta> event_time,
                 ExceptionState& exception_state) {
  animation.PlayInternal(Animation::AutoRewind::kEnabled, exception_state);

  bool notify = event_time && animation.PendingInternal();

  // If the animation is already running, then this is a no-op and we don't
  // need to update its start time. If it isn't already running, then it should
  // be pending play due to the above call to Play().
  DCHECK(animation.PendingInternal() ||
         animation.CalculateAnimationPlayState() ==
             V8AnimationPlayState::Enum::kRunning);

  if (notify) {
    animation.NotifyAnimationStartedAsync(event_time.value());
  }
}

void PerformPause(Animation& animation,
                  V8AnimationPlayState::Enum play_state,
                  ExceptionState& exception_state) {
  if (play_state == V8AnimationPlayState::Enum::kRunning) {
    animation.PauseInternal(ASSERT_NO_EXCEPTION);
  }
}

void PerformPlayForwards(Animation& animation,
                         V8AnimationPlayState::Enum play_state,
                         ExceptionState& exception_state) {
  double playback_rate = std::abs(animation.EffectivePlaybackRate());

  animation.updatePlaybackRate(playback_rate, exception_state);

  animation.PlayInternal(Animation::AutoRewind::kEnabled, exception_state);
}

void PerformPlayBackwards(Animation& animation,
                          V8AnimationPlayState::Enum play_state,
                          ExceptionState& exception_state) {
  double playback_rate = -std::abs(animation.EffectivePlaybackRate());

  animation.updatePlaybackRate(playback_rate, exception_state);

  animation.PlayInternal(Animation::AutoRewind::kEnabled, exception_state);
}

void PerformPlayOnce(Animation& animation,
                     V8AnimationPlayState::Enum play_state,
                     ExceptionState& exception_state) {
  if (play_state != V8AnimationPlayState::Enum::kFinished) {
    animation.PlayInternal(Animation::AutoRewind::kEnabled, exception_state);
  }
}

void PerformReset(Animation& animation,
                  V8AnimationPlayState::Enum play_state,
                  ExceptionState& exception_state) {
  animation.ResetPlayback();
}

void PerformReplay(Animation& animation,
                   V8AnimationPlayState::Enum play_state,
                   ExceptionState& exception_state) {
  animation.ResetPlayback();

  animation.PlayInternal(Animation::AutoRewind::kEnabled, exception_state);
}

}  // namespace

// static
void AnimationTrigger::PerformBehavior(
    Animation& animation,
    Behavior behavior,
    std::optional<base::TimeDelta> async_event_time,
    ExceptionState& exception_state) {
  ScriptForbiddenScope forbid_script;
  // TODO(crbug.com/451238244): Plumb the impl thread animation's start time,
  // |async_event_time| through to the individual behaviors.
  V8AnimationPlayState::Enum play_state =
      animation.CalculateAnimationPlayState();
  switch (behavior) {
    case Behavior::kPlay:
      PerformPlay(animation, async_event_time, exception_state);
      break;
    case Behavior::kPause:
      PerformPause(animation, play_state, exception_state);
      break;
    case Behavior::kPlayForwards:
      PerformPlayForwards(animation, play_state, exception_state);
      break;
    case Behavior::kPlayBackwards:
      PerformPlayBackwards(animation, play_state, exception_state);
      break;
    case Behavior::kPlayOnce:
      PerformPlayOnce(animation, play_state, exception_state);
      break;
    case Behavior::kReset:
      PerformReset(animation, play_state, exception_state);
      break;
    case Behavior::kReplay:
      PerformReplay(animation, play_state, exception_state);
      break;
    case Behavior::kNone:
      break;
    default:
      NOTREACHED();
  };
}

// static
bool AnimationTrigger::HasPausedCSSPlayState(Animation* animation) {
  if (!animation->IsCSSAnimation()) {
    return false;
  }

  CSSAnimation* css_animation = To<CSSAnimation>(animation);

  if (css_animation->GetIgnoreCSSPlayState()) {
    return false;
  }

  return animation->GetTriggerActionPlayState() == EAnimPlayState::kPaused;
}

// static
cc::AnimationTrigger::Behavior AnimationTrigger::ToCcAnimationTriggerBehavior(
    Behavior behavior) {
  switch (behavior) {
    case Behavior::kPlay:
      return cc::AnimationTrigger::Behavior::kPlay;
    case Behavior::kPause:
      return cc::AnimationTrigger::Behavior::kPause;
    case Behavior::kPlayForwards:
      return cc::AnimationTrigger::Behavior::kPlayForwards;
    case Behavior::kPlayBackwards:
      return cc::AnimationTrigger::Behavior::kPlayBackwards;
    case Behavior::kPlayOnce:
      return cc::AnimationTrigger::Behavior::kPlayOnce;
    case Behavior::kReset:
      return cc::AnimationTrigger::Behavior::kReset;
    case Behavior::kReplay:
      return cc::AnimationTrigger::Behavior::kReplay;
    case Behavior::kNone:
      return cc::AnimationTrigger::Behavior::kNone;
  };
  NOTREACHED();
}

bool AnimationTrigger::CanCompositeBehavior(Behavior behavior) {
  switch (behavior) {
    case Behavior::kPlay:
    case Behavior::kNone:
      return true;
    case Behavior::kPlayOnce:
    case Behavior::kPlayForwards:
    case Behavior::kPlayBackwards:
    case Behavior::kReplay:
    case Behavior::kPause:
    case Behavior::kReset:
      return false;
  }
  NOTREACHED();
}

void AnimationTrigger::DestroyCompositorTrigger() {
  if (!compositor_trigger_) {
    return;
  }

  compositor_trigger_->SetAnimationTriggerDelegate(nullptr);

  if (cc::AnimationHost* host = compositor_trigger_->GetAnimationHost()) {
    host->RemoveTrigger(compositor_trigger_);
  }

  compositor_trigger_ = nullptr;
}

void AnimationTrigger::Dispose() {
  DestroyCompositorTrigger();
}

void AnimationTrigger::addAnimation(
    Animation* animation,
    V8AnimationTriggerBehavior activate_behavior,
    V8AnimationTriggerBehavior deactivate_behavior,
    ExceptionState& exception_state) {
  CHECK(!is_activating_or_deactivating_);

  if (!animation) {
    return;
  }

  const HeapHashSet<WeakMember<AnimationTrigger>>& animation_triggers =
      animation->GetTriggers();
  if (!animation_triggers.empty() && !animation_triggers.Contains(this)) {
    // TODO(crbug.com/474398437): Support multiple triggers per animation when
    // the working group resolevs to do so:
    // https://github.com/w3c/csswg-drafts/issues/12399#issuecomment-3089703026
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Attaching multiple triggers to an animation is not allowed.");
  }

  WillAddAnimation(animation, activate_behavior.AsEnum(),
                   deactivate_behavior.AsEnum(), exception_state);
  if (exception_state.HadException()) {
    return;
  }

  UpdateBehaviorMap(*animation, activate_behavior.AsEnum(),
                    deactivate_behavior.AsEnum());
  animation->AddTrigger(this);

  DidAddAnimation();
}

void AnimationTrigger::removeAnimation(Animation* animation) {
  CHECK(!is_activating_or_deactivating_);

  if (!animation) {
    return;
  }

  animation->RemoveTrigger(this);
  animation_behavior_map_.erase(animation);
  DidRemoveAnimation(animation);
}

HeapVector<Member<Animation>> AnimationTrigger::getAnimations() {
  HeapVector<Member<Animation>> animations;

  for (auto& [animation, behaviors] : animation_behavior_map_) {
    animations.push_back(animation);
  }

  std::sort(animations.begin(), animations.end(), Animation::CompareAnimations);
  return animations;
}

bool AnimationTrigger::IsTimelineTrigger() const {
  return false;
}

bool AnimationTrigger::IsEventTrigger() const {
  return false;
}

void AnimationTrigger::WillAddAnimation(Animation* animation,
                                        Behavior enter_behavior,
                                        Behavior exit_behavior,
                                        ExceptionState& exception_state) {}

void AnimationTrigger::DidAddAnimation() {}

void AnimationTrigger::DidRemoveAnimation(Animation* animation) {}

void AnimationTrigger::UpdateBehaviorMap(Animation& animation,
                                         Behavior activate_behavior,
                                         Behavior deactivate_behavior) {
  animation_behavior_map_.Set(
      &animation, std::make_pair<>(activate_behavior, deactivate_behavior));
}

void AnimationTrigger::PerformActivate(
    std::optional<base::TimeDelta> async_activate_time) {
  base::AutoReset<bool> is_activating(&is_activating_or_deactivating_, true);

  for (auto [animation, behaviors] : animation_behavior_map_) {
    if (HasPausedCSSPlayState(animation)) {
      continue;
    }
    std::optional<base::TimeDelta> time =
        IsTriggeredOnCompositor(animation, behaviors) ? async_activate_time
                                                      : std::nullopt;
    DCHECK(!IsTriggeredOnCompositor(animation, behaviors) || time.has_value());
    PerformBehavior(*animation, behaviors.first, time, ASSERT_NO_EXCEPTION);
  }
}

void AnimationTrigger::PerformDeactivate(
    std::optional<base::TimeDelta> async_deactivate_time) {
  base::AutoReset<bool> is_deactivating(&is_activating_or_deactivating_, true);

  for (auto [animation, behaviors] : animation_behavior_map_) {
    if (HasPausedCSSPlayState(animation)) {
      continue;
    }

    std::optional<base::TimeDelta> time =
        IsTriggeredOnCompositor(animation, behaviors) ? async_deactivate_time
                                                      : std::nullopt;
    DCHECK(!IsTriggeredOnCompositor(animation, behaviors) || time.has_value());
    PerformBehavior(*animation, behaviors.second, time, ASSERT_NO_EXCEPTION);
  }
}

void AnimationTrigger::UpdateCompositorTriggerAnimations(
    const PaintArtifactCompositor* paint_artifact_compositor) {
  CHECK(compositor_trigger_);

  std::vector<cc::AnimationTrigger::AnimationData> animation_data;
  for (auto& [animation, behaviors] : animation_behavior_map_) {
    if (!CanCompositeBehavior(behaviors.first) ||
        !CanCompositeBehavior(behaviors.second)) {
      continue;
    }

    if (!animation->StartTriggeredAnimationOnCompositor(
            paint_artifact_compositor)) {
      // Check that the animation is compositable. If it is, create a
      // cc::Animation for it if necessary.
      continue;
    }

    CompositorAnimation* compositor_anim = animation->GetCompositorAnimation();
    compositor_anim = animation->GetCompositorAnimation();
    DCHECK(compositor_anim);
    cc::Animation* cc_animation = compositor_anim->CcAnimation();
    DCHECK(cc_animation);

    AnimationTimeline* timeline = animation->TimelineInternal();
    cc::AnimationTimeline* cc_timeline =
        timeline ? timeline->CompositorTimeline() : nullptr;
    if (!cc_timeline) {
      continue;
    }

    CcBehavior activate = ToCcAnimationTriggerBehavior(behaviors.first);
    CcBehavior deactivate = ToCcAnimationTriggerBehavior(behaviors.second);

    cc::AnimationTrigger::AnimationData data(
        cc_animation->id(), cc_timeline->id(), activate, deactivate);
    animation_data.push_back(data);
  }

  compositor_trigger_->SetAnimationData(animation_data);
}

void AnimationTrigger::UpdateCompositorTrigger(
    const PaintArtifactCompositor* paint_artifact_compositor) {
  DCHECK(Platform::Current()->IsThreadedAnimationEnabled());

  bool compositing_supported =
      (IsEventTrigger() &&
       RuntimeEnabledFeatures::CompositorEventTriggerEnabled()) ||
      (IsTimelineTrigger() &&
       RuntimeEnabledFeatures::CompositorTimelineTriggerEnabled());

  if (!compositing_supported) {
    return;
  }

  if (BehaviorMap().empty()) {
    DestroyCompositorTrigger();
  } else if (!compositor_trigger_) {
    // TODO(crbug.com/451238244): Currently, the code always creates cc
    // triggers. So, we might be creating cc triggers that do not do anything.
    // We should tie creating a cc trigger to whether there is at least one
    // compositable animation attached, perhaps in addAnimation.
    CreateCompositorTrigger();
  }

  if (compositor_trigger_) {
    compositor_trigger_->SetAnimationTriggerDelegate(
        static_cast<cc::AnimationTriggerDelegate*>(this));
    UpdateCompositorTriggerAnimations(paint_artifact_compositor);
  }
}

bool AnimationTrigger::IsTriggeredOnCompositor(
    Animation* animation,
    const std::pair<Behavior, Behavior>& behaviors) {
  if (!compositor_trigger_) {
    return false;
  }

  CompositorAnimation* compositor_anim = animation->GetCompositorAnimation();
  cc::Animation* cc_animation =
      compositor_anim ? compositor_anim->CcAnimation() : nullptr;
  if (!cc_animation) {
    return false;
  }

  return CanCompositeBehavior(behaviors.first) &&
         CanCompositeBehavior(behaviors.second);
}

void AnimationTrigger::Trace(Visitor* visitor) const {
  visitor->Trace(animation_behavior_map_);
  visitor->Trace(owning_element_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
