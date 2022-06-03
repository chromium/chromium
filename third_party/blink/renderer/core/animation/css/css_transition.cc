// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_transition.h"

#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

CSSTransition::CSSTransition(ExecutionContext* execution_context,
                             AnimationTimeline* timeline,
                             AnimationEffect* content,
                             uint64_t transition_generation,
                             const PropertyHandle& transition_property)
    : Animation(execution_context, timeline, content),
      transition_property_(transition_property) {
  // The owning_element does not always equal to the target element of an
  // animation.
  owning_element_ = To<KeyframeEffect>(effect())->EffectTarget();
  transition_generation_ = transition_generation;
}

AtomicString CSSTransition::transitionProperty() const {
  return transition_property_.GetCSSPropertyName().ToAtomicString();
}

String CSSTransition::playState() const {
  // TODO(1043778): Flush is likely not required once the CSSTransition is
  // disassociated from its owning element.
  if (GetDocument())
    GetDocument()->UpdateStyleAndLayoutTree();
  return Animation::playState();
}

AnimationEffect::EventDelegate* CSSTransition::CreateEventDelegate(
    Element* target,
    const AnimationEffect::EventDelegate* old_event_delegate) {
  return CSSAnimations::CreateEventDelegate(target, transition_property_,
                                            old_event_delegate);
}

}  // namespace blink
