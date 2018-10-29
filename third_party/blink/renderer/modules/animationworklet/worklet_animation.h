// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ANIMATIONWORKLET_WORKLET_ANIMATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ANIMATIONWORKLET_WORKLET_ANIMATION_H_

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/modules/v8/document_timeline_or_scroll_timeline.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/animation_effect_owner.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/worklet_animation_base.h"
#include "third_party/blink/renderer/modules/animationworklet/worklet_animation_options.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_client.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_delegate.h"
#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutators_state.h"

namespace blink {

class AnimationEffectOrAnimationEffectSequence;
class SerializedScriptValue;

// The main-thread controller for a single AnimationWorklet animator instance.
//
// WorkletAnimation instances exist in the document execution context (i.e. in
// the main javascript thread), and are a type of animation that delegates
// actual playback to an 'animator instance'. The animator instance runs in a
// separate worklet execution context (which can either also be on the main
// thread or may be in a separate worklet thread).
//
// All methods in this class should be called in the document execution context.
//
// Spec: https://wicg.github.io/animation-worklet/#worklet-animation-desc
class MODULES_EXPORT WorkletAnimation : public WorkletAnimationBase,
                                        public CompositorAnimationClient,
                                        public CompositorAnimationDelegate,
                                        public AnimationEffectOwner {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(WorkletAnimation);
  USING_PRE_FINALIZER(WorkletAnimation, Dispose);

 public:
  static WorkletAnimation* Create(
      ScriptState*,
      String animator_name,
      const AnimationEffectOrAnimationEffectSequence&,
      ExceptionState&);
  static WorkletAnimation* Create(
      ScriptState*,
      String animator_name,
      const AnimationEffectOrAnimationEffectSequence&,
      DocumentTimelineOrScrollTimeline,
      ExceptionState&);
  static WorkletAnimation* Create(
      ScriptState*,
      String animator_name,
      const AnimationEffectOrAnimationEffectSequence&,
      DocumentTimelineOrScrollTimeline,
      scoped_refptr<SerializedScriptValue>,
      ExceptionState&);

  ~WorkletAnimation() override = default;

  AnimationTimeline* timeline() { return timeline_; }
  String playState();
  void play(ExceptionState& exception_state);
  void cancel();

  // AnimationEffectOwner implementation:
  unsigned SequenceNumber() const override { return sequence_number_; }
  bool Playing() const override;
  // Always allow dispatching events for worklet animations. This is only ever
  // relevant to CSS animations which means it does not have any material effect
  // on worklet animations either way.
  bool IsEventDispatchAllowed() const override { return true; }
  // Effect supression is used by devtool's animation inspection machinery which
  // is not currently supported by worklet animations.
  bool EffectSuppressed() const override { return false; }

  void EffectInvalidated() override;
  void UpdateIfNecessary() override;

  Animation* GetAnimation() override { return nullptr; }

  // WorkletAnimationBase implementation.
  void Update(TimingUpdateReason) override;
  void UpdateCompositingState() override;
  void InvalidateCompositingState() override;

  // CompositorAnimationClient implementation.
  CompositorAnimation* GetCompositorAnimation() const override {
    return compositor_animation_.get();
  }

  // CompositorAnimationDelegate implementation.
  void NotifyAnimationStarted(double monotonic_time, int group) override {}
  void NotifyAnimationFinished(double monotonic_time, int group) override {}
  void NotifyAnimationAborted(double monotonic_time, int group) override {}

  void Dispose();

  Document* GetDocument() const override { return document_.Get(); }
  AnimationTimeline* GetTimeline() const override { return timeline_; }
  const String& Name() { return animator_name_; }

  KeyframeEffect* GetEffect() const override;
  const WorkletAnimationId& GetWorkletAnimationId() const override {
    return id_;
  }
  bool IsActiveAnimation() const override;

  void UpdateInputState(AnimationWorkletDispatcherInput* input_state) override;
  void SetOutputState(
      const AnimationWorkletOutput::AnimationState& state) override;

  void Trace(blink::Visitor*) override;

 private:
  WorkletAnimation(WorkletAnimationId id,
                   const String& animator_name,
                   Document&,
                   const HeapVector<Member<KeyframeEffect>>&,
                   AnimationTimeline*,
                   scoped_refptr<SerializedScriptValue>);
  void DestroyCompositorAnimation();

  // Attempts to start the animation on the compositor side, returning true if
  // it succeeds or false otherwise. If false is returned and the animation
  // cannot be started on main.
  bool StartOnCompositor();
  void StartOnMain();
  bool CheckCanStart(String* failure_message);
  void SetStartTimeToNow();

  // Updates a running animation on the compositor side.
  void UpdateOnCompositor();

  std::unique_ptr<cc::AnimationOptions> CloneOptions() const {
    return options_ ? options_->Clone() : nullptr;
  }

  void SetPlayState(const Animation::AnimationPlayState& state) {
    play_state_ = state;
  }

  unsigned sequence_number_;

  WorkletAnimationId id_;

  const String animator_name_;
  Animation::AnimationPlayState play_state_;
  Animation::AnimationPlayState last_play_state_;
  // Start time in ms.
  base::Optional<base::TimeDelta> start_time_;
  Vector<base::Optional<base::TimeDelta>> local_times_;
  // We use this to skip updating if current time has not changed since last
  // update.
  base::Optional<base::TimeDelta> last_current_time_;

  Member<Document> document_;

  HeapVector<Member<KeyframeEffect>> effects_;
  Member<AnimationTimeline> timeline_;
  std::unique_ptr<WorkletAnimationOptions> options_;

  std::unique_ptr<CompositorAnimation> compositor_animation_;
  bool running_on_main_thread_;

  // Tracks whether any KeyframeEffect associated with this WorkletAnimation has
  // been invalidated and needs to be restarted. Used to avoid unnecessarily
  // restarting the effect on the compositor. When true, a call to
  // |UpdateOnCompositor| will update the effect on the compositor.
  bool effect_needs_restart_;
};

}  // namespace blink

#endif
