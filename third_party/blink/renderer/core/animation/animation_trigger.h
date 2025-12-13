// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TRIGGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TRIGGER_H_

#include "cc/animation/animation_host.h"
#include "cc/animation/animation_trigger.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_animation_play_state.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_animation_trigger_behavior.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class Animation;
class Element;
class ExceptionState;

class CORE_EXPORT AnimationTrigger : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(AnimationTrigger, Dispose);

 public:
  using Behavior = V8AnimationTriggerBehavior::Enum;
  // Maps an Animation to its associated "activate" (first) and "deactivate"
  // (second) behaviors.
  using AnimationBehaviorMap =
      HeapHashMap<WeakMember<Animation>, std::pair<Behavior, Behavior>>;

  void addAnimation(Animation* animation,
                    V8AnimationTriggerBehavior activate_behavior,
                    V8AnimationTriggerBehavior deactivate_behavior,
                    ExceptionState& exception_state);
  void removeAnimation(Animation* animation);
  HeapVector<Member<Animation>> getAnimations();

  virtual bool CanTrigger() const = 0;
  virtual bool IsTimelineTrigger() const;
  virtual bool IsEventTrigger() const;

  void RemoveAnimations();

  void UpdateBehaviorMap(Animation& animation,
                         Behavior activate_behavior,
                         Behavior deactivate_behavior);

  static bool HasPausedCSSPlayState(Animation* animation);

  void UpdateCompositorTrigger();
  virtual void CreateCompositorTrigger() {}
  virtual void DestroyCompositorTrigger() {}
  cc::AnimationTrigger* CompositorTrigger() { return nullptr; }

  void Dispose();

  void Trace(Visitor* visitor) const override;

  Element* OwningElement() { return owning_element_.Get(); }

 protected:
  AnimationBehaviorMap& BehaviorMap() { return animation_behavior_map_; }
  void PerformActivate();
  void PerformDeactivate();
  static void PerformBehavior(Animation& animation,
                              Behavior behavior,
                              ExceptionState& exception_state);

  // The (main thread) cc::AnimationTrigger corresponding to |this|. The impl
  // thread version is cloned from this.
  scoped_refptr<cc::AnimationTrigger> compositor_trigger_;

  WeakMember<Element> owning_element_;

 private:
  virtual void WillAddAnimation(Animation* animation,
                                Behavior activate_behavior,
                                Behavior deactivate_behavior,
                                ExceptionState& exception_state);
  virtual void DidAddAnimation();
  virtual void DidRemoveAnimation(Animation* animation);

  AnimationBehaviorMap animation_behavior_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TRIGGER_H_
