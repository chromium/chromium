// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TRIGGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TRIGGER_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_animation_play_state.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_animation_trigger_behavior.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class Animation;
class ExceptionState;

class CORE_EXPORT AnimationTrigger : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using Behavior = V8AnimationTriggerBehavior::Enum;

  void addAnimation(Animation* animation,
                    const AtomicString& action,
                    V8AnimationTriggerBehavior behavior,
                    ExceptionState& exception_state);
  void removeAnimation(Animation* animation);

  virtual bool CanTrigger() const = 0;
  virtual bool IsTimelineTrigger() const;
  virtual bool IsEventTrigger() const;

  void RemoveAnimations();

  // Finds the behavior associated with a particular action for a particular
  // animation. It expects to be called only with an animation which is known to
  // be in the |animation_action_map_|.
  void PerformActionOnAnimation(Animation& animation,
                                const AtomicString& action,
                                ExceptionState& exception_state);
  // Executes PerformActionOnAnimation for all animations in
  // |animation_action_map_| when the given |action| occurs.
  void PerformActionOnAnimations(const AtomicString& action);

  void Trace(Visitor* visitor) const override;

 protected:
  using ActionBehaviorMap = HashMap<AtomicString, Behavior>;

  const HeapHashMap<WeakMember<Animation>, ActionBehaviorMap>& ActionsMap()
      const {
    return animation_action_map_;
  }

 private:
  virtual bool WillAddAnimation(Animation* animation,
                                const AtomicString& action,
                                ExceptionState& exception_state);
  virtual void DidAddAnimation(Animation* animation,
                               const AtomicString& action,
                               std::optional<Behavior> old_behavior,
                               Behavior new_behavior,
                               ExceptionState& exception_state);
  virtual void DidRemoveAnimation(Animation* animation);

  // Updates the behavior for a particular action for a particular animation.
  std::optional<Behavior> UpdateActionMap(Animation* animation,
                                          const AtomicString& action,
                                          Behavior behavior);

  HeapHashMap<WeakMember<Animation>, ActionBehaviorMap> animation_action_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TRIGGER_H_
