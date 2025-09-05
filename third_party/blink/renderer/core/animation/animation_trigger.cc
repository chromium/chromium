// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animation_trigger.h"

#include "third_party/blink/renderer/core/animation/animation.h"

namespace blink {

void AnimationTrigger::addAnimation(Animation* animation,
                                    ExceptionState& exception_state) {
  if (animations_.Contains(animation)) {
    return;
  }

  WillAddAnimation(animation, exception_state);
  if (exception_state.HadException()) {
    return;
  }

  animations_.insert(animation);
  animation->AddTrigger(this);
  DidAddAnimation(animation, exception_state);
}

void AnimationTrigger::removeAnimation(Animation* animation) {
  animations_.erase(animation);
  animation->RemoveTrigger(this);
  DidRemoveAnimation(animation);
}

void AnimationTrigger::RemoveAnimations() {
  HeapHashSet<WeakMember<Animation>> animations;
  animations_.swap(animations);
  for (Animation* animation : animations) {
    removeAnimation(animation);
  }
}

bool AnimationTrigger::IsTimelineTrigger() const {
  return false;
}

bool AnimationTrigger::IsEventTrigger() const {
  return false;
}

void AnimationTrigger::WillAddAnimation(Animation* animation,
                                        ExceptionState& exception_state) {}
void AnimationTrigger::DidAddAnimation(Animation* animation,
                                       ExceptionState& exception_state) {}
void AnimationTrigger::DidRemoveAnimation(Animation* animation) {}

void AnimationTrigger::Trace(Visitor* visitor) const {
  visitor->Trace(animations_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
