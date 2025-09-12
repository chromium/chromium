// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_TRIGGER_ATTACHMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_TRIGGER_ATTACHMENT_H_

#include "third_party/blink/renderer/core/css/css_trigger_attachment_value.h"
#include "third_party/blink/renderer/core/style/scoped_css_name.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class Animation;
class AnimationTrigger;

// This class represents a configuration for attaching a CSS animation to an
// animation trigger. It maps 1:1 with CSSTriggerAttachmentValue.
class StyleTriggerAttachment : public GarbageCollected<StyleTriggerAttachment> {
 public:
  StyleTriggerAttachment(
      const ScopedCSSName* trigger_name,
      const HeapVector<std::pair<AtomicString, AtomicString>>&
          action_behavior_pairs)
      : trigger_name_(trigger_name),
        action_behavior_pairs_(std::move(action_behavior_pairs)) {}

  const ScopedCSSName* TriggerName() const { return trigger_name_.Get(); }
  const HeapVector<std::pair<AtomicString, AtomicString>>& ActionBehaviorPairs()
      const {
    return action_behavior_pairs_;
  }

  void Attach(AnimationTrigger& trigger, Animation& animation) const;

  void Trace(Visitor* visitor) const {
    visitor->Trace(trigger_name_);
    visitor->Trace(action_behavior_pairs_);
  }

 private:
  // Represents the name of the trigger to which the animation will be attached.
  Member<const ScopedCSSName> trigger_name_;

  // Specifies a list of action-behavior pairs.
  HeapVector<std::pair<AtomicString, AtomicString>> action_behavior_pairs_;
};

typedef GCedHeapVector<Member<const StyleTriggerAttachment>>
    StyleTriggerAttachmentVector;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_TRIGGER_ATTACHMENT_H_
