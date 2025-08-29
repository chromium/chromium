// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TRIGGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TRIGGER_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_animation_trigger_behavior.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_timelinerangeoffset.h"
#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/scroll_snapshot_timeline.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class CORE_EXPORT AnimationTrigger : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using Behavior = V8AnimationTriggerBehavior;

  AnimationTrigger(Behavior behavior) : behavior_(behavior) {}

  Behavior behavior() const { return behavior_; }
  void addAnimation(Animation* animation, ExceptionState& exception_state);
  void removeAnimation(Animation* animation);

  static Behavior ToV8TriggerBehavior(EAnimationTriggerBehavior behavior) {
    switch (behavior) {
      case EAnimationTriggerBehavior::kOnce:
        return Behavior(Behavior::Enum::kOnce);
      case EAnimationTriggerBehavior::kRepeat:
        return Behavior(Behavior::Enum::kRepeat);
      case EAnimationTriggerBehavior::kAlternate:
        return Behavior(Behavior::Enum::kAlternate);
      case EAnimationTriggerBehavior::kState:
        return Behavior(Behavior::Enum::kState);
      default:
        NOTREACHED();
    };
  }

  virtual bool CanTrigger() const = 0;
  virtual bool IsTimelineTrigger() const;
  virtual bool IsEventTrigger() const;

  void RemoveAnimations();

  void Trace(Visitor* visitor) const override;

 protected:
  const HeapHashSet<WeakMember<Animation>>& animations() const {
    return animations_;
  }

 private:
  virtual void DidAddAnimation(Animation* animation,
                               ExceptionState& exception_state);
  virtual void DidRemoveAnimation(Animation* animation);

  Behavior behavior_;
  HeapHashSet<WeakMember<Animation>> animations_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TRIGGER_H_
