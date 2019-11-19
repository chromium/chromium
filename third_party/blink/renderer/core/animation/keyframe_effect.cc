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

#include "third_party/blink/renderer/core/animation/keyframe_effect.h"

#include "third_party/blink/renderer/bindings/core/v8/unrestricted_double_or_keyframe_effect_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_transform.h"
#include "third_party/blink/renderer/core/animation/effect_input.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/sampled_effect.h"
#include "third_party/blink/renderer/core/animation/timing_input.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

KeyframeEffect* KeyframeEffect::Create(
    ScriptState* script_state,
    Element* element,
    const ScriptValue& keyframes,
    const UnrestrictedDoubleOrKeyframeEffectOptions& options,
    ExceptionState& exception_state) {
  if (element) {
    UseCounter::Count(
        element->GetDocument(),
        WebFeature::kAnimationConstructorKeyframeListEffectObjectTiming);
  }
  Document* document = element ? &element->GetDocument() : nullptr;
  Timing timing = TimingInput::Convert(options, document, exception_state);
  if (exception_state.HadException())
    return nullptr;

  EffectModel::CompositeOperation composite = EffectModel::kCompositeReplace;
  if (options.IsKeyframeEffectOptions()) {
    composite = EffectModel::StringToCompositeOperation(
                    options.GetAsKeyframeEffectOptions()->composite())
                    .value();
  }

  KeyframeEffectModelBase* model = EffectInput::Convert(
      element, keyframes, composite, script_state, exception_state);
  if (exception_state.HadException())
    return nullptr;
  return MakeGarbageCollected<KeyframeEffect>(element, model, timing);
}

KeyframeEffect* KeyframeEffect::Create(ScriptState* script_state,
                                       Element* element,
                                       const ScriptValue& keyframes,
                                       ExceptionState& exception_state) {
  if (element) {
    UseCounter::Count(
        element->GetDocument(),
        WebFeature::kAnimationConstructorKeyframeListEffectNoTiming);
  }
  KeyframeEffectModelBase* model =
      EffectInput::Convert(element, keyframes, EffectModel::kCompositeReplace,
                           script_state, exception_state);
  if (exception_state.HadException())
    return nullptr;
  return MakeGarbageCollected<KeyframeEffect>(element, model, Timing());
}

KeyframeEffect* KeyframeEffect::Create(ScriptState* script_state,
                                       KeyframeEffect* source,
                                       ExceptionState& exception_state) {
  Timing new_timing = source->SpecifiedTiming();
  KeyframeEffectModelBase* model = source->Model()->Clone();
  return MakeGarbageCollected<KeyframeEffect>(source->target(), model,
                                              new_timing, source->GetPriority(),
                                              source->GetEventDelegate());
}

KeyframeEffect::KeyframeEffect(Element* target,
                               KeyframeEffectModelBase* model,
                               const Timing& timing,
                               Priority priority,
                               EventDelegate* event_delegate)
    : AnimationEffect(timing, event_delegate),
      target_(target),
      model_(model),
      sampled_effect_(nullptr),
      priority_(priority) {
  DCHECK(model_);
}

KeyframeEffect::~KeyframeEffect() = default;

void KeyframeEffect::setTarget(Element* target) {
  if (target_ == target)
    return;

  DetachTarget(GetAnimation());
  target_ = target;
  AttachTarget(GetAnimation());

  InvalidateAndNotifyOwner();
}

String KeyframeEffect::composite() const {
  return EffectModel::CompositeOperationToString(CompositeInternal());
}

void KeyframeEffect::setComposite(String composite_string) {
  Model()->SetComposite(
      EffectModel::StringToCompositeOperation(composite_string).value());

  ClearEffects();
  InvalidateAndNotifyOwner();
}

HeapVector<ScriptValue> KeyframeEffect::getKeyframes(
    ScriptState* script_state) {
  HeapVector<ScriptValue> computed_keyframes;
  if (!model_->HasFrames())
    return computed_keyframes;

  // getKeyframes() returns a list of 'ComputedKeyframes'. A ComputedKeyframe
  // consists of the normal keyframe data combined with the computed offset for
  // the given keyframe.
  //
  // https://w3c.github.io/web-animations/#dom-keyframeeffectreadonly-getkeyframes
  const KeyframeVector& keyframes = model_->GetFrames();
  Vector<double> computed_offsets =
      KeyframeEffectModelBase::GetComputedOffsets(keyframes);
  computed_keyframes.ReserveInitialCapacity(keyframes.size());
  ScriptState::Scope scope(script_state);
  for (wtf_size_t i = 0; i < keyframes.size(); i++) {
    V8ObjectBuilder object_builder(script_state);
    keyframes[i]->AddKeyframePropertiesToV8Object(object_builder);
    object_builder.Add("computedOffset", computed_offsets[i]);
    computed_keyframes.push_back(object_builder.GetScriptValue());
  }

  return computed_keyframes;
}

void KeyframeEffect::setKeyframes(ScriptState* script_state,
                                  const ScriptValue& keyframes,
                                  ExceptionState& exception_state) {
  // TODO(crbug.com/799061): Support TransitionKeyframeEffectModel. This will
  // require a lot of work as the setKeyframes API can mutate a transition
  // Animation into a 'normal' one with multiple properties.
  if (!Model()->IsStringKeyframeEffectModel()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Calling setKeyframes on CSS Transitions is not yet supported");
    return;
  }

  StringKeyframeVector new_keyframes = EffectInput::ParseKeyframesArgument(
      target(), keyframes, script_state, exception_state);
  if (exception_state.HadException())
    return;

  SetKeyframes(new_keyframes);
}

void KeyframeEffect::SetKeyframes(StringKeyframeVector keyframes) {
  Model()->SetComposite(
      EffectInput::ResolveCompositeOperation(Model()->Composite(), keyframes));

  ToStringKeyframeEffectModel(Model())->SetFrames(keyframes);

  // Changing the keyframes will invalidate any sampled effect, as well as
  // potentially affect the effect owner.
  ClearEffects();
  InvalidateAndNotifyOwner();
}

bool KeyframeEffect::Affects(const PropertyHandle& property) const {
  return model_->Affects(property);
}

void KeyframeEffect::NotifySampledEffectRemovedFromEffectStack() {
  sampled_effect_ = nullptr;
}

CompositorAnimations::FailureReasons
KeyframeEffect::CheckCanStartAnimationOnCompositor(
    const PaintArtifactCompositor* paint_artifact_compositor,
    double animation_playback_rate) const {
  CompositorAnimations::FailureReasons reasons =
      CompositorAnimations::kNoFailure;

  // There would be no reason to composite an effect that has no keyframes; it
  // has no visual result.
  if (model_->Properties().IsEmpty())
    reasons |= CompositorAnimations::kInvalidAnimationOrEffect;

  // There would be no reason to composite an effect that has no target; it has
  // no visual result.
  if (!target_) {
    reasons |= CompositorAnimations::kInvalidAnimationOrEffect;
  } else {
    if (target_->GetComputedStyle() && target_->GetComputedStyle()->HasOffset())
      reasons |= CompositorAnimations::kTargetHasCSSOffset;

    // Do not put transforms on compositor if more than one of them are defined
    // in computed style because they need to be explicitly ordered
    if (HasMultipleTransformProperties())
      reasons |= CompositorAnimations::kTargetHasMultipleTransformProperties;

    reasons |= CompositorAnimations::CheckCanStartAnimationOnCompositor(
        SpecifiedTiming(), *target_, GetAnimation(), *Model(),
        paint_artifact_compositor, animation_playback_rate);
  }

  return reasons;
}

void KeyframeEffect::StartAnimationOnCompositor(
    int group,
    base::Optional<double> start_time,
    double current_time,
    double animation_playback_rate,
    CompositorAnimation* compositor_animation) {
  DCHECK(!HasActiveAnimationsOnCompositor());
  // TODO(petermayo): Maybe we should recheck that we can start on the
  // compositor if we have the compositable IDs somewhere.

  if (!compositor_animation)
    compositor_animation = GetAnimation()->GetCompositorAnimation();

  DCHECK(compositor_animation);
  DCHECK(target_);
  DCHECK(Model());

  CompositorAnimations::StartAnimationOnCompositor(
      *target_, group, start_time, current_time, SpecifiedTiming(),
      GetAnimation(), *compositor_animation, *Model(),
      compositor_keyframe_model_ids_, animation_playback_rate);
  DCHECK(!compositor_keyframe_model_ids_.IsEmpty());
}

bool KeyframeEffect::HasActiveAnimationsOnCompositor() const {
  return !compositor_keyframe_model_ids_.IsEmpty();
}

bool KeyframeEffect::HasActiveAnimationsOnCompositor(
    const PropertyHandle& property) const {
  return HasActiveAnimationsOnCompositor() && Affects(property);
}

bool KeyframeEffect::CancelAnimationOnCompositor(
    CompositorAnimation* compositor_animation) {
  if (!HasActiveAnimationsOnCompositor())
    return false;
  if (!target_ || !target_->GetLayoutObject())
    return false;
  for (const auto& compositor_keyframe_model_id :
       compositor_keyframe_model_ids_) {
    CompositorAnimations::CancelAnimationOnCompositor(
        *target_, compositor_animation, compositor_keyframe_model_id);
  }
  compositor_keyframe_model_ids_.clear();
  return true;
}

void KeyframeEffect::CancelIncompatibleAnimationsOnCompositor() {
  if (target_ && GetAnimation() && model_->HasFrames()) {
    CompositorAnimations::CancelIncompatibleAnimationsOnCompositor(
        *target_, *GetAnimation(), *Model());
  }
}

void KeyframeEffect::PauseAnimationForTestingOnCompositor(double pause_time) {
  DCHECK(HasActiveAnimationsOnCompositor());
  if (!target_ || !target_->GetLayoutObject())
    return;
  DCHECK(GetAnimation());
  for (const auto& compositor_keyframe_model_id :
       compositor_keyframe_model_ids_) {
    CompositorAnimations::PauseAnimationForTestingOnCompositor(
        *target_, *GetAnimation(), compositor_keyframe_model_id, pause_time);
  }
}

void KeyframeEffect::AttachCompositedLayers() {
  DCHECK(target_);
  DCHECK(GetAnimation());
  CompositorAnimations::AttachCompositedLayers(
      *target_, GetAnimation()->GetCompositorAnimation());
}

bool KeyframeEffect::HasAnimation() const {
  return !!owner_;
}

bool KeyframeEffect::HasPlayingAnimation() const {
  return owner_ && owner_->Playing();
}

void KeyframeEffect::Trace(blink::Visitor* visitor) {
  visitor->Trace(target_);
  visitor->Trace(model_);
  visitor->Trace(sampled_effect_);
  AnimationEffect::Trace(visitor);
}

bool KeyframeEffect::AnimationsPreserveAxisAlignment(
    const PropertyHandle& property) const {
  const auto* keyframes = Model()->GetPropertySpecificKeyframes(property);
  if (!keyframes)
    return true;
  for (const auto& keyframe : *keyframes) {
    const auto* value = keyframe->GetCompositorKeyframeValue();
    if (!value)
      continue;
    DCHECK(value->IsTransform());
    const auto& transform_operations =
        ToCompositorKeyframeTransform(value)->GetTransformOperations();
    if (!transform_operations.PreservesAxisAlignment())
      return false;
  }
  return true;
}

namespace {

static const size_t num_transform_properties = 4;

const CSSProperty** TransformProperties() {
  static const CSSProperty* kTransformProperties[num_transform_properties] = {
      &GetCSSPropertyTransform(), &GetCSSPropertyScale(),
      &GetCSSPropertyRotate(), &GetCSSPropertyTranslate()};
  return kTransformProperties;
}

}  // namespace

bool KeyframeEffect::AnimationsPreserveAxisAlignment() const {
  static const auto** properties = TransformProperties();
  for (size_t i = 0; i < num_transform_properties; i++) {
    if (!AnimationsPreserveAxisAlignment(PropertyHandle(*properties[i])))
      return false;
  }

  return true;
}

EffectModel::CompositeOperation KeyframeEffect::CompositeInternal() const {
  return model_->Composite();
}

void KeyframeEffect::ApplyEffects() {
  DCHECK(IsInEffect());
  if (!target_ || !model_->HasFrames())
    return;

  if (GetAnimation() && HasIncompatibleStyle()) {
    GetAnimation()->CancelAnimationOnCompositor();
  }

  base::Optional<double> iteration = CurrentIteration();
  DCHECK(iteration);
  DCHECK_GE(iteration.value(), 0);
  bool changed = false;
  if (sampled_effect_) {
    changed =
        model_->Sample(clampTo<int>(iteration.value(), 0), Progress().value(),
                       SpecifiedTiming().IterationDuration(),
                       sampled_effect_->MutableInterpolations());
  } else {
    HeapVector<Member<Interpolation>> interpolations;
    model_->Sample(clampTo<int>(iteration.value(), 0), Progress().value(),
                   SpecifiedTiming().IterationDuration(), interpolations);
    if (!interpolations.IsEmpty()) {
      auto* sampled_effect =
          MakeGarbageCollected<SampledEffect>(this, owner_->SequenceNumber());
      sampled_effect->MutableInterpolations().swap(interpolations);
      sampled_effect_ = sampled_effect;
      target_->EnsureElementAnimations().GetEffectStack().Add(sampled_effect);
      changed = true;
    } else {
      return;
    }
  }

  if (changed) {
    target_->SetNeedsAnimationStyleRecalc();
    auto* svg_element = DynamicTo<SVGElement>(target_.Get());
    if (RuntimeEnabledFeatures::WebAnimationsSVGEnabled() && svg_element)
      svg_element->SetWebAnimationsPending();
  }
}

void KeyframeEffect::ClearEffects() {
  if (!sampled_effect_)
    return;
  sampled_effect_->Clear();
  sampled_effect_ = nullptr;
  if (GetAnimation())
    GetAnimation()->RestartAnimationOnCompositor();
  target_->SetNeedsAnimationStyleRecalc();
  auto* svg_element = DynamicTo<SVGElement>(target_.Get());
  if (RuntimeEnabledFeatures::WebAnimationsSVGEnabled() && svg_element)
    svg_element->ClearWebAnimatedAttributes();
  Invalidate();
}

void KeyframeEffect::UpdateChildrenAndEffects() const {
  if (!model_->HasFrames())
    return;
  DCHECK(owner_);
  if (IsInEffect() && !owner_->EffectSuppressed())
    const_cast<KeyframeEffect*>(this)->ApplyEffects();
  else
    const_cast<KeyframeEffect*>(this)->ClearEffects();
}

void KeyframeEffect::Attach(AnimationEffectOwner* owner) {
  AttachTarget(owner->GetAnimation());
  AnimationEffect::Attach(owner);
}

void KeyframeEffect::Detach() {
  DetachTarget(GetAnimation());
  AnimationEffect::Detach();
}

void KeyframeEffect::AttachTarget(Animation* animation) {
  if (!target_ || !animation)
    return;
  target_->EnsureElementAnimations().Animations().insert(animation);
  target_->SetNeedsAnimationStyleRecalc();
  auto* svg_element = DynamicTo<SVGElement>(target_.Get());
  if (RuntimeEnabledFeatures::WebAnimationsSVGEnabled() && svg_element)
    svg_element->SetWebAnimationsPending();
}

void KeyframeEffect::DetachTarget(Animation* animation) {
  if (target_ && animation)
    target_->GetElementAnimations()->Animations().erase(animation);
  // If we have sampled this effect previously, we need to purge that state.
  // ClearEffects takes care of clearing the cached sampled effect, informing
  // the target that it needs to refresh its style, and doing any necessary
  // update on the compositor.
  ClearEffects();
}

AnimationTimeDelta KeyframeEffect::CalculateTimeToEffectChange(
    bool forwards,
    double local_time,
    double time_to_next_iteration) const {
  const double start_time = SpecifiedTiming().start_delay;
  const double end_time_minus_end_delay =
      start_time + SpecifiedTiming().ActiveDuration();
  const double end_time =
      end_time_minus_end_delay + SpecifiedTiming().end_delay;
  const double after_time = std::min(end_time_minus_end_delay, end_time);

  switch (GetPhase()) {
    case Timing::kPhaseNone:
      return AnimationTimeDelta::Max();
    case Timing::kPhaseBefore:
      DCHECK_GE(start_time, local_time);
      return forwards
                 ? AnimationTimeDelta::FromSecondsD(start_time - local_time)
                 : AnimationTimeDelta::Max();
    case Timing::kPhaseActive:
      if (forwards) {
        // Need service to apply fill / fire events.
        const double time_to_end = after_time - local_time;
        if (RequiresIterationEvents()) {
          return AnimationTimeDelta::FromSecondsD(
              std::min(time_to_end, time_to_next_iteration));
        }
        return AnimationTimeDelta::FromSecondsD(time_to_end);
      }
      return {};
    case Timing::kPhaseAfter:
      DCHECK_GE(local_time, after_time);
      if (forwards) {
        // If an animation has a positive-valued end delay, we need an
        // additional tick at the end time to ensure that the finished event is
        // delivered.
        return end_time > local_time
                   ? AnimationTimeDelta::FromSecondsD(end_time - local_time)
                   : AnimationTimeDelta::Max();
      }
      return AnimationTimeDelta::FromSecondsD(local_time - after_time);
    default:
      NOTREACHED();
      return AnimationTimeDelta::Max();
  }
}

// Returns true if transform, translate, rotate or scale is composited
// and a motion path or other transform properties
// has been introduced on the element
bool KeyframeEffect::HasIncompatibleStyle() const {
  if (!target_->GetComputedStyle())
    return false;

  if (HasActiveAnimationsOnCompositor()) {
    if (target_->GetComputedStyle()->HasOffset()) {
      static const auto** properties = TransformProperties();
      for (size_t i = 0; i < num_transform_properties; i++) {
        if (Affects(PropertyHandle(*properties[i])))
          return true;
      }
    }
    return HasMultipleTransformProperties();
  }

  return false;
}

bool KeyframeEffect::HasMultipleTransformProperties() const {
  if (!target_->GetComputedStyle())
    return false;

  unsigned transform_property_count = 0;
  if (target_->GetComputedStyle()->HasTransformOperations())
    transform_property_count++;
  if (target_->GetComputedStyle()->Rotate())
    transform_property_count++;
  if (target_->GetComputedStyle()->Scale())
    transform_property_count++;
  if (target_->GetComputedStyle()->Translate())
    transform_property_count++;
  return transform_property_count > 1;
}

}  // namespace blink
