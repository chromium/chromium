// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animation_trigger.h"

#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/css/css_animation.h"
#include "third_party/blink/renderer/core/animation/deferred_timeline.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline_util.h"
#include "third_party/blink/renderer/core/animation/timeline_range.h"
#include "third_party/blink/renderer/core/animation/view_timeline.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

void AnimationTrigger::addAnimation(Animation* animation,
                                    ExceptionState& exception_state) {
  if (animations_.Contains(animation)) {
    return;
  }

  animation->PauseInternal(exception_state);
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

void AnimationTrigger::DidAddAnimation(Animation* animation,
                                       ExceptionState& exception_state) {}
void AnimationTrigger::DidRemoveAnimation(Animation* animation) {}

void AnimationTrigger::Trace(Visitor* visitor) const {
  visitor->Trace(animations_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
