// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_transition.h"

namespace blink {

CSSTransition* CSSTransition::Create(AnimationEffect* effect,
                                     AnimationTimeline* timeline,
                                     const PropertyHandle& property) {
  DCHECK(timeline && timeline->IsDocumentTimeline());
  return MakeGarbageCollected<CSSTransition>(
      timeline->GetDocument()->ContextDocument(), timeline, effect, property);
}

CSSTransition::CSSTransition(ExecutionContext* execution_context,
                             AnimationTimeline* timeline,
                             AnimationEffect* content,
                             const PropertyHandle& transition_property)
    : Animation(execution_context, timeline, content),
      transition_property_(transition_property) {}

AtomicString CSSTransition::transitionProperty() const {
  return transition_property_.GetCSSPropertyName().ToAtomicString();
}

}  // namespace blink
