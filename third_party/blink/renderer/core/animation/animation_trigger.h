// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TRIGGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TRIGGER_H_

#include "cc/animation/animation_trigger.h"
#include "cc/animation/animation_trigger_delegate.h"
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

class CORE_EXPORT AnimationTrigger : public ScriptWrappable,
                                     public cc::AnimationTriggerDelegate {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(AnimationTrigger, Dispose);

 public:
  using Behavior = V8AnimationTriggerBehavior::Enum;
  // Maps an Animation to its associated "activate" (first) and "deactivate"
  // (second) behaviors.
  using AnimationBehaviorMap =
      HeapHashMap<WeakMember<Animation>, std::pair<Behavior, Behavior>>;
  using CcBehavior = cc::AnimationTrigger::Behavior;

  // To avoid expensive compositing checks, we maintain a whitelist of
  // compositing failure reasons for which a re-check is warranted from an
  // animation trigger's point of view. In other words, for reasons not in this
  // whitelist, we should not even bother running the check function.
  static const CompositorAnimations::FailureReasons kRecheckCompositingReasons =
      CompositorAnimations::kInvalidAnimationOrEffect;

  void addAnimation(Animation* animation,
                    V8AnimationTriggerBehavior activate_behavior,
                    V8AnimationTriggerBehavior deactivate_behavior,
                    ExceptionState& exception_state);
  void removeAnimation(Animation* animation);
  HeapVector<Member<Animation>> getAnimations();

  virtual bool CanTrigger() const = 0;
  virtual bool IsTimelineTrigger() const;
  virtual bool IsEventTrigger() const;

  AnimationBehaviorMap& BehaviorMap() { return animation_behavior_map_; }
  void UpdateBehaviorMap(Animation& animation,
                         Behavior activate_behavior,
                         Behavior deactivate_behavior);
  const AnimationBehaviorMap& BehaviorMapForTest() const {
    return animation_behavior_map_;
  }

  static bool HasPausedCSSPlayState(Animation* animation);
  static CcBehavior ToCcAnimationTriggerBehavior(Behavior behavior);

  void UpdateCompositorTrigger(
      const PaintArtifactCompositor* paint_artifact_compositor);
  virtual void CreateCompositorTrigger() {}
  virtual void DestroyCompositorTrigger();
  cc::AnimationTrigger* CompositorTrigger() { return nullptr; }

  void Dispose();

  void Trace(Visitor* visitor) const override;

  Element* OwningElement() { return owning_element_.Get(); }

 protected:
  void PerformActivate();
  void PerformDeactivate();
  static void PerformBehavior(Animation& animation,
                              Behavior behavior,
                              ExceptionState& exception_state);

  // Gets the document associated with this AnimationTrigger. For a timeline
  // trigger, it corresponds to the document of the trigger's underlying
  // timeline. For an event trigger, it corresponds to the document to which the
  // event source (the element) is connected.
  virtual Document* GetDocument() { return nullptr; }

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

  bool IsTriggeredOnCompositor(Animation* animation);
  void UpdateCompositorTriggerAnimations(
      const PaintArtifactCompositor* paint_artifact_compositor);

  AnimationBehaviorMap animation_behavior_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TRIGGER_H_
