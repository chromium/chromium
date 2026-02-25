// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animation_trigger.h"

#include "cc/animation/animation_id_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/css/css_animation.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation.h"

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

void AnimationTrigger::PerformActivate() {
  for (auto [animation, behaviors] : animation_behavior_map_) {
    if (HasPausedCSSPlayState(animation) ||
        (compositor_trigger_ && IsTriggeredOnCompositor(animation))) {
      continue;
    }
    PerformBehavior(*animation, behaviors.first, ASSERT_NO_EXCEPTION);
  }
}

void AnimationTrigger::PerformDeactivate() {
  for (auto [animation, behaviors] : animation_behavior_map_) {
    if (HasPausedCSSPlayState(animation) ||
        (compositor_trigger_ && IsTriggeredOnCompositor(animation))) {
      continue;
    }
    PerformBehavior(*animation, behaviors.second, ASSERT_NO_EXCEPTION);
  }
}

void AnimationTrigger::UpdateCompositorTriggerAnimations(
    const PaintArtifactCompositor* paint_artifact_compositor) {
  CHECK(compositor_trigger_);

  std::vector<cc::AnimationTrigger::AnimationData> animation_data;
  for (auto& [animation, behaviors] : animation_behavior_map_) {
    bool pause_keyframe_models = false;

    if (!animation->StartTriggeredAnimationOnCompositor(
            paint_artifact_compositor, pause_keyframe_models)) {
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

    // Ensure that cc animations created because of the trigger remain
    // paused until triggered. We only do this if we instantiated the cc
    // animation so as not to interfere with an animation that was already
    // playing.
    if (pause_keyframe_models) {
      cc_animation->PauseKeyframeModels(base::TimeDelta());
    }
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
    compositor_trigger_->SetAnimationTriggerDelegate(
        static_cast<cc::AnimationTriggerDelegate*>(this));
  }

  if (compositor_trigger_) {
    UpdateCompositorTriggerAnimations(paint_artifact_compositor);
  }
}

bool AnimationTrigger::IsTriggeredOnCompositor(Animation* animation) {
  DCHECK(compositor_trigger_);

  CompositorAnimation* compositor_anim = animation->GetCompositorAnimation();
  cc::Animation* cc_animation =
      compositor_anim ? compositor_anim->CcAnimation() : nullptr;
  if (!cc_animation) {
    return false;
  }

  return true;
}

void AnimationTrigger::Trace(Visitor* visitor) const {
  visitor->Trace(animation_behavior_map_);
  visitor->Trace(owning_element_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
