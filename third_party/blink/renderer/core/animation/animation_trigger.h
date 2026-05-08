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
  static bool CanCompositeBehavior(Behavior behavior);

  void UpdateCompositorTrigger(
      const PaintArtifactCompositor* paint_artifact_compositor);
  virtual void CreateCompositorTrigger() {}
  virtual void DestroyCompositorTrigger();
  cc::AnimationTrigger* CompositorTrigger() {
    return compositor_trigger_.get();
  }

  void Dispose();

  void Trace(Visitor* visitor) const override;

  Element* OwningElement() { return owning_element_.Get(); }

 protected:
  FRIEND_TEST_ALL_PREFIXES(ScriptedTimelineTriggerTest,
                           ForbidScriptDuringActivation);
  // |async_activate_time| and |async_deactivate_time| are the timestamps
  // at which the impl thread observed activation and deactivation respectively.
  // If the event (activation/deactivation) was observed on the main thread
  // (i.e. compositor_trigger_ is null) and not the impl thread, these times are
  // not set.
  //
  // When these times are set, if the animation is composited, it has already
  // been acted on by the impl thread and these functions work to synchronize
  // the animation to that time. If the animation is not composited, these
  // functions trigger the animation newly from the main thread, similar to
  // observing activation/deactivation on the main thread.
  void PerformActivate(
      std::optional<base::TimeDelta> async_activate_time = std::nullopt);
  void PerformDeactivate(
      std::optional<base::TimeDelta> async_deactivate_time = std::nullopt);
  static void PerformBehavior(Animation& animation,
                              Behavior behavior,
                              std::optional<base::TimeDelta> async_event_time,
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

  // Set to true during PerformActivate and PerformDeactivate to prevent
  // mutations of the behavior map.
  bool is_activating_or_deactivating_ = false;

 private:
  virtual void WillAddAnimation(Animation* animation,
                                Behavior activate_behavior,
                                Behavior deactivate_behavior,
                                ExceptionState& exception_state);
  virtual void DidAddAnimation();
  virtual void DidRemoveAnimation(Animation* animation);

  bool IsTriggeredOnCompositor(Animation* animation,
                               const std::pair<Behavior, Behavior>&);
  void UpdateCompositorTriggerAnimations(
      const PaintArtifactCompositor* paint_artifact_compositor);

  AnimationBehaviorMap animation_behavior_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TRIGGER_H_
