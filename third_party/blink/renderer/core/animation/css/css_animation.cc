// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_animation.h"

namespace blink {

CSSAnimation* CSSAnimation::Create(AnimationEffect* effect,
                                   AnimationTimeline* timeline,
                                   const String& animation_name) {
  DCHECK(timeline && timeline->IsDocumentTimeline());

  return MakeGarbageCollected<CSSAnimation>(
      timeline->GetDocument()->ContextDocument(), timeline, effect,
      animation_name);
}

CSSAnimation::CSSAnimation(ExecutionContext* execution_context,
                           AnimationTimeline* timeline,
                           AnimationEffect* content,
                           const String& animation_name)
    : Animation(execution_context, timeline, content),
      animation_name_(animation_name) {}

}  // namespace blink
