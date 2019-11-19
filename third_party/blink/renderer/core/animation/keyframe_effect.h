/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_KEYFRAME_EFFECT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_KEYFRAME_EFFECT_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/animation/animation_effect.h"
#include "third_party/blink/renderer/core/animation/compositor_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class Element;
class ExceptionState;
class KeyframeEffectModelBase;
class PaintArtifactCompositor;
class SampledEffect;
class UnrestrictedDoubleOrKeyframeEffectOptions;

// Represents the effect of an Animation on an Element's properties.
// https://drafts.csswg.org/web-animations/#keyframe-effect
class CORE_EXPORT KeyframeEffect final : public AnimationEffect {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum Priority { kDefaultPriority, kTransitionPriority };

  // Web Animations API Bindings constructors.
  static KeyframeEffect* Create(
      ScriptState*,
      Element*,
      const ScriptValue&,
      const UnrestrictedDoubleOrKeyframeEffectOptions&,
      ExceptionState&);
  static KeyframeEffect* Create(ScriptState*,
                                Element*,
                                const ScriptValue&,
                                ExceptionState&);
  static KeyframeEffect* Create(ScriptState*, KeyframeEffect*, ExceptionState&);

  KeyframeEffect(Element*,
                 KeyframeEffectModelBase*,
                 const Timing&,
                 Priority = kDefaultPriority,
                 EventDelegate* = nullptr);
  ~KeyframeEffect() override;

  bool IsKeyframeEffect() const override { return true; }

  // IDL implementation.
  Element* target() const { return target_; }
  void setTarget(Element*);
  String composite() const;
  void setComposite(String);
  HeapVector<ScriptValue> getKeyframes(ScriptState*);
  void setKeyframes(ScriptState*,
                    const ScriptValue& keyframes,
                    ExceptionState&);

  void SetKeyframes(StringKeyframeVector keyframes);

  bool Affects(const PropertyHandle&) const;
  const KeyframeEffectModelBase* Model() const { return model_.Get(); }
  KeyframeEffectModelBase* Model() { return model_.Get(); }
  void SetModel(KeyframeEffectModelBase* model) {
    DCHECK(model);
    model_ = model;
  }
  Priority GetPriority() const { return priority_; }

  void NotifySampledEffectRemovedFromEffectStack();

  CompositorAnimations::FailureReasons CheckCanStartAnimationOnCompositor(
      const PaintArtifactCompositor*,
      double animation_playback_rate) const;
  // Must only be called once.
  void StartAnimationOnCompositor(int group,
                                  base::Optional<double> start_time,
                                  double time_offset,
                                  double animation_playback_rate,
                                  CompositorAnimation* = nullptr);
  bool HasActiveAnimationsOnCompositor() const;
  bool HasActiveAnimationsOnCompositor(const PropertyHandle&) const;
  bool CancelAnimationOnCompositor(CompositorAnimation*);
  void CancelIncompatibleAnimationsOnCompositor();
  void PauseAnimationForTestingOnCompositor(double pause_time);

  void AttachCompositedLayers();

  void DowngradeToNormal() { priority_ = kDefaultPriority; }

  bool HasAnimation() const;
  bool HasPlayingAnimation() const;

  void Trace(blink::Visitor*) override;

  bool AnimationsPreserveAxisAlignment() const;

 private:
  EffectModel::CompositeOperation CompositeInternal() const;

  void ApplyEffects();
  void ClearEffects();
  void UpdateChildrenAndEffects() const override;
  void Attach(AnimationEffectOwner*) override;
  void Detach() override;
  void AttachTarget(Animation*);
  void DetachTarget(Animation*);
  AnimationTimeDelta CalculateTimeToEffectChange(
      bool forwards,
      double inherited_time,
      double time_to_next_iteration) const override;
  bool HasIncompatibleStyle() const;
  bool HasMultipleTransformProperties() const;

  bool AnimationsPreserveAxisAlignment(const PropertyHandle&) const;

  Member<Element> target_;
  Member<KeyframeEffectModelBase> model_;
  Member<SampledEffect> sampled_effect_;

  Priority priority_;

  Vector<int> compositor_keyframe_model_ids_;
};

DEFINE_TYPE_CASTS(KeyframeEffect,
                  AnimationEffect,
                  animationNode,
                  animationNode->IsKeyframeEffect(),
                  animationNode.IsKeyframeEffect());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_KEYFRAME_EFFECT_H_
