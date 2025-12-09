// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animation_trigger.h"

#include "cc/animation/animation_id_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/css/css_animation.h"

namespace blink {

using Behavior = AnimationTrigger::Behavior;

namespace {

// Behavior implementations per
// https://github.com/w3c/csswg-drafts/issues/12611#issue-3326243729

void PerformPlay(Animation& animation,
                 V8AnimationPlayState::Enum play_state,
                 ExceptionState& exception_state) {
  animation.PlayInternal(Animation::AutoRewind::kEnabled, exception_state);
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

void PerformPlayAlternate(Animation& animation,
                          V8AnimationPlayState::Enum play_state,
                          ExceptionState& exception_state) {
  if (play_state == V8AnimationPlayState::Enum::kIdle) {
    animation.PlayInternal(Animation::AutoRewind::kEnabled, exception_state);
  } else {
    animation.ReverseInternal(exception_state);
  }
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

void PerformPlayPause(Animation& animation,
                      V8AnimationPlayState::Enum play_state,
                      ExceptionState& exception_state) {
  if (play_state != V8AnimationPlayState::Enum::kRunning) {
    animation.PlayInternal(Animation::AutoRewind::kEnabled, exception_state);
  } else {
    animation.PauseInternal(ASSERT_NO_EXCEPTION);
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
void AnimationTrigger::PerformBehavior(Animation& animation,
                                       Behavior behavior,
                                       ExceptionState& exception_state) {
  V8AnimationPlayState::Enum play_state =
      animation.CalculateAnimationPlayState();
  switch (behavior) {
    case Behavior::kPlay:
      PerformPlay(animation, play_state, exception_state);
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
    case Behavior::kPlayAlternate:
      PerformPlayAlternate(animation, play_state, exception_state);
      break;
    case Behavior::kPlayOnce:
      PerformPlayOnce(animation, play_state, exception_state);
      break;
    case Behavior::kPlayPause:
      PerformPlayPause(animation, play_state, exception_state);
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

void AnimationTrigger::Dispose() {
  DestroyCompositorTrigger();
}

void AnimationTrigger::addAnimation(
    Animation* animation,
    V8AnimationTriggerBehavior activate_behavior,
    V8AnimationTriggerBehavior deactivate_behavior,
    ExceptionState& exception_state) {
  if (!animation) {
    return;
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

void AnimationTrigger::RemoveAnimations() {
  AnimationBehaviorMap animation_behavior_map;
  animation_behavior_map_.swap(animation_behavior_map);
  for (Animation* animation : animation_behavior_map.Keys()) {
    removeAnimation(animation);
  }
}

void AnimationTrigger::UpdateBehaviorMap(Animation& animation,
                                         Behavior activate_behavior,
                                         Behavior deactivate_behavior) {
  animation_behavior_map_.Set(
      &animation, std::make_pair<>(activate_behavior, deactivate_behavior));
}

void AnimationTrigger::PerformActivate() {
  for (auto [animation, behaviors] : animation_behavior_map_) {
    if (HasPausedCSSPlayState(animation)) {
      continue;
    }
    PerformBehavior(*animation, behaviors.first, ASSERT_NO_EXCEPTION);
  }
}

void AnimationTrigger::PerformDeactivate() {
  for (auto [animation, behaviors] : animation_behavior_map_) {
    if (HasPausedCSSPlayState(animation)) {
      continue;
    }
    PerformBehavior(*animation, behaviors.second, ASSERT_NO_EXCEPTION);
  }
}

void AnimationTrigger::UpdateCompositorTrigger() {
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

  // TODO(crbug.com/451238244): if (compositor_trigger_) { Update cc animations.
  // }
}

void AnimationTrigger::Trace(Visitor* visitor) const {
  visitor->Trace(animation_behavior_map_);
  visitor->Trace(owning_element_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
