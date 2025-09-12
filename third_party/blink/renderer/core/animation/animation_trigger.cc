// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animation_trigger.h"

#include "third_party/blink/renderer/core/animation/animation.h"

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

void PerformBehavior(Animation& animation,
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
    default:
      NOTREACHED();
  };
}

}  // namespace

void AnimationTrigger::addAnimation(Animation* animation,
                                    const AtomicString& action,
                                    V8AnimationTriggerBehavior behavior,
                                    ExceptionState& exception_state) {
  if (!WillAddAnimation(animation, action, exception_state)) {
    return;
  }

  std::optional<Behavior> old_behavior =
      UpdateActionMap(animation, action, behavior.AsEnum());
  animation->AddTrigger(this);

  DidAddAnimation(animation, action, old_behavior, behavior.AsEnum(),
                  exception_state);
}

void AnimationTrigger::removeAnimation(Animation* animation) {
  animation_action_map_.erase(animation);
  animation->RemoveTrigger(this);
  DidRemoveAnimation(animation);
}

void AnimationTrigger::RemoveAnimations() {
  HeapHashMap<WeakMember<Animation>, ActionBehaviorMap> animation_action_map;
  animation_action_map_.swap(animation_action_map);
  for (Animation* animation : animation_action_map.Keys()) {
    removeAnimation(animation);
  }
}

bool AnimationTrigger::IsTimelineTrigger() const {
  return false;
}

bool AnimationTrigger::IsEventTrigger() const {
  return false;
}

bool AnimationTrigger::WillAddAnimation(Animation* animation,
                                        const AtomicString& action,
                                        ExceptionState& exception_state) {
  return true;
}
void AnimationTrigger::DidAddAnimation(Animation* animation,
                                       const AtomicString& action,
                                       std::optional<Behavior> old_behavior,
                                       Behavior new_behavior,
                                       ExceptionState& exception_state) {}
void AnimationTrigger::DidRemoveAnimation(Animation* animation) {}

void AnimationTrigger::PerformActionOnAnimation(
    Animation& animation,
    const AtomicString& action,
    ExceptionState& exception_state) {
  ActionBehaviorMap& action_behavior_map =
      animation_action_map_.find(&animation)->value;
  auto behavior_entry = action_behavior_map.find(action);
  if (behavior_entry == action_behavior_map.end()) {
    return;
  }
  PerformBehavior(animation, behavior_entry->value, exception_state);
}

void AnimationTrigger::PerformActionOnAnimations(const AtomicString& action) {
  for (auto& [animation, action_map] : animation_action_map_) {
    PerformActionOnAnimation(*animation, action, ASSERT_NO_EXCEPTION);
  }
}

std::optional<Behavior> AnimationTrigger::UpdateActionMap(
    Animation* animation,
    const AtomicString& action,
    Behavior behavior) {
  std::optional<Behavior> old_behavior;
  auto action_map_it = animation_action_map_.find(animation);
  if (action_map_it != animation_action_map_.end()) {
    ActionBehaviorMap& behavior_map = action_map_it->value;
    auto behavior_map_it = behavior_map.find(action);
    old_behavior = (behavior_map_it == behavior_map.end())
                       ? std::nullopt
                       : std::make_optional<Behavior>(behavior_map_it->value);
    behavior_map.Set(action, behavior);
  } else {
    ActionBehaviorMap behavior_map;
    behavior_map.Set(action, behavior);
    animation_action_map_.Set(animation, std::move(behavior_map));
  }
  return old_behavior;
}

void AnimationTrigger::Trace(Visitor* visitor) const {
  visitor->Trace(animation_action_map_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
