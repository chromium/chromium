// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_TRIGGER_ATTACHMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_TRIGGER_ATTACHMENT_H_

#include "third_party/blink/renderer/core/css/css_trigger_attachment_value.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/scoped_css_name.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class CSSAnimation;
class AnimationTrigger;

// This class represents a configuration for attaching a CSS animation to an
// animation trigger. It maps 1:1 with CSSTriggerAttachmentValue.
class StyleTriggerAttachment : public GarbageCollected<StyleTriggerAttachment> {
 public:
  StyleTriggerAttachment(const ScopedCSSName* trigger_name,
                         EAnimationTriggerBehavior enter_behavior,
                         std::optional<EAnimationTriggerBehavior> exit_behavior)
      : trigger_name_(trigger_name),
        enter_behavior_(enter_behavior),
        exit_behavior_(exit_behavior) {}

  const ScopedCSSName* TriggerName() const { return trigger_name_.Get(); }
  EAnimationTriggerBehavior EnterBehavior() const { return enter_behavior_; }
  std::optional<EAnimationTriggerBehavior> ExitBehavior() const {
    return exit_behavior_;
  }

  void Attach(AnimationTrigger& trigger, CSSAnimation& animation) const;

  void Trace(Visitor* visitor) const {
    visitor->Trace(trigger_name_);
  }

 private:
  // Represents the name of the trigger to which the animation will be attached.
  Member<const ScopedCSSName> trigger_name_;

  EAnimationTriggerBehavior enter_behavior_;
  std::optional<EAnimationTriggerBehavior> exit_behavior_;
};

typedef GCedHeapVector<Member<const StyleTriggerAttachment>>
    StyleTriggerAttachmentVector;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_TRIGGER_ATTACHMENT_H_
