// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_trigger_attachment.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_animation_trigger_behavior.h"
#include "third_party/blink/renderer/core/animation/animation_trigger.h"
#include "third_party/blink/renderer/core/animation/css/css_animation.h"

namespace blink {

void StyleTriggerAttachment::Attach(AnimationTrigger& trigger,
                                    CSSAnimation& animation) const {
  DCHECK(animation.OwningElement());
  std::optional<V8AnimationTriggerBehavior> enter_behavior(enter_behavior_);

  std::optional<V8AnimationTriggerBehavior> exit_behavior(
      exit_behavior_ ? exit_behavior_.value()
                     : V8AnimationTriggerBehavior::Enum::kNone);

  // This method attaches animations to triggers based on CSS declarations which
  // should only parse valid behaviors.
  DCHECK(enter_behavior && exit_behavior);

  trigger.addAnimation(&animation, *enter_behavior, *exit_behavior,
                       ASSERT_NO_EXCEPTION);
  animation.SetNamedTriggerAttachment(trigger_name_, &trigger);
}

}  // namespace blink
