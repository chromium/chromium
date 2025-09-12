// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_trigger_attachment.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_animation_trigger_behavior.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/animation_trigger.h"

namespace blink {

void StyleTriggerAttachment::Attach(AnimationTrigger& trigger,
                                    Animation& animation) const {
  for (const auto& [action, behavior] : ActionBehaviorPairs()) {
    std::optional<V8AnimationTriggerBehavior> v8_behavior =
        V8AnimationTriggerBehavior::Create(behavior.GetString());
    if (!v8_behavior) {
      continue;
    }

    trigger.addAnimation(&animation, action, *v8_behavior, ASSERT_NO_EXCEPTION);
  }
}

}  // namespace blink
