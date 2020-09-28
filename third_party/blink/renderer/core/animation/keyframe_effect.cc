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
#include "third_party/blink/renderer/core/animation/animation_input_helpers.h"
#include "third_party/blink/renderer/core/animation/animation_utils.h"
#include "third_party/blink/renderer/core/animation/compositor_animations.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_transform.h"
#include "third_party/blink/renderer/core/animation/effect_input.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/sampled_effect.h"
#include "third_party/blink/renderer/core/animation/timing_input.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

// Verifies that a pseudo-element selector lexes and canonicalizes legacy forms
bool ValidateAndCanonicalizePseudo(String& selector) {
  if (selector.IsNull()) {
    return true;
  } else if (selector.StartsWith("::")) {
    return true;
  } else if (selector == ":before") {
    selector = "::before";
    return true;
  } else if (selector == ":after") {
    selector = "::after";
    return true;
  } else if (selector == ":first-letter") {
    selector = "::first-letter";
    return true;
  } else if (selector == ":first-line") {
    selector = "::first-line";
    return true;
  }
  return false;
}

}  // namespace

KeyframeEffect* KeyframeEffect::Create(
    ScriptState* script_state,
    Element* element,
    const ScriptValue& keyframes,
    const UnrestrictedDoubleOrKeyframeEffectOptions& options,
    ExceptionState& exception_state) {
  Document* document = element ? &element->GetDocument() : nullptr;
  Timing timing = TimingInput::Convert(options, document, exception_state);
  if (exception_state.HadException())
    return nullptr;

  EffectModel::CompositeOperation composite = EffectModel::kCompositeReplace;
  String pseudo = String();
  if (options.IsKeyframeEffectOptions()) {
    auto* effect_options = options.GetAsKeyframeEffectOptions();
    composite =
        EffectModel::StringToCompositeOperation(effect_options->composite())
            .value();
    if (RuntimeEnabledFeatures::WebAnimationsAPIEnabled() &&
        !effect_options->pseudoElement().IsEmpty()) {
      pseudo = effect_options->pseudoElement();
      if (!ValidateAndCanonicalizePseudo(pseudo)) {
        // TODO(gtsteel): update when
        // https://github.com/w3c/csswg-drafts/issues/4586 resolves
        exception_state.ThrowDOMException(
            DOMExceptionCode::kSyntaxError,
            "A valid pseudo-selector must be null or start with ::.");
      }
    }
  }

  KeyframeEffectModelBase* model = EffectInput::Convert(
      element, keyframes, composite, script_state, exception_state);
  if (exception_state.HadException())
    return nullptr;
  KeyframeEffect* effect =
      MakeGarbageCollected<KeyframeEffect>(element, model, timing);

  if (!pseudo.IsEmpty()) {
    effect->target_pseudo_ = pseudo;
    if (element) {
      element->GetDocument().UpdateStyleAndLayoutTreeForNode(element);
      effect->effect_target_ = element->GetPseudoElement(
          CSSSelector::ParsePseudoId(pseudo, element));
    }
  }
  return effect;
}

KeyframeEffect* KeyframeEffect::Create(ScriptState* script_state,
                                       Element* element,
                                       const ScriptValue& keyframes,
                                       ExceptionState& exception_state) {
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
  return MakeGarbageCollected<KeyframeEffect>(source->EffectTarget(), model,
                                              new_timing, source->GetPriority(),
                                              source->GetEventDelegate());
}

KeyframeEffect::KeyframeEffect(Element* target,
                               KeyframeEffectModelBase* model,
                               const Timing& timing,
                               Priority priority,
                               EventDelegate* event_delegate)
    : AnimationEffect(timing, event_delegate),
      effect_target_(target),
      target_element_(target),
      target_pseudo_(),
      model_(model),
      sampled_effect_(nullptr),
      priority_(priority),
      ignore_css_keyframes_(false) {
  DCHECK(model_);

  // fix target for css animations and transitions
  if (target && target->IsPseudoElement()) {
    target_element_ = target->parentElement();
    DCHECK(!target_element_->IsPseudoElement());
    target_pseudo_ = target->tagName();
  }

  CountAnimatedProperties();
}

KeyframeEffect::~KeyframeEffect() = default;

void KeyframeEffect::setTarget(Element* new_target) {
  DCHECK(!new_target || !new_target->IsPseudoElement());
  target_element_ = new_target;
  RefreshTarget();
}

const String& KeyframeEffect::pseudoElement() const {
  return target_pseudo_;
}

void KeyframeEffect::setPseudoElement(String pseudo,
                                      ExceptionState& exception_state) {
  if (ValidateAndCanonicalizePseudo(pseudo)) {
    target_pseudo_ = pseudo;
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "A valid pseudo-selector must be null or start with ::.");
  }

  RefreshTarget();
}

void KeyframeEffect::RefreshTarget() {
  Element* new_target;
  if (!target_element_) {
    new_target = nullptr;
  } else if (target_pseudo_.IsEmpty()) {
    new_target = target_element_;
  } else {
    target_element_->GetDocument().UpdateStyleAndLayoutTreeForNode(
        target_element_);
    PseudoId pseudoId =
        CSSSelector::ParsePseudoId(target_pseudo_, target_element_);
    new_target = target_element_->GetPseudoElement(pseudoId);
  }

  if (new_target != effect_target_) {
    DetachTarget(GetAnimation());
    effect_target_ = new_target;
    AttachTarget(GetAnimation());
    InvalidateAndNotifyOwner();
  }
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
  if (Animation* animation = GetAnimation())
    animation->FlushPendingUpdates();

  HeapVector<ScriptValue> computed_keyframes;
  if (!model_->HasFrames())
    return computed_keyframes;

  // getKeyframes() returns a list of 'ComputedKeyframes'. A ComputedKeyframe
  // consists of the normal keyframe data combined with the computed offset for
  // the given keyframe.
  //
  // https://w3c.github.io/web-animations/#dom-keyframeeffectreadonly-getkeyframes
  KeyframeVector keyframes = ignore_css_keyframes_
                                 ? model_->GetFrames()
                                 : model_->GetComputedKeyframes(EffectTarget());

  Vector<double> computed_offsets =
      KeyframeEffectModelBase::GetComputedOffsets(keyframes);
  computed_keyframes.ReserveInitialCapacity(keyframes.size());
  ScriptState::Scope scope(script_state);
  for (wtf_size_t i = 0; i < keyframes.size(); i++) {
    V8ObjectBuilder object_builder(script_state);
    keyframes[i]->AddKeyframePropertiesToV8Object(object_builder, target());
    object_builder.Add("computedOffset", computed_offsets[i]);
    computed_keyframes.push_back(object_builder.GetScriptValue());
  }

  return computed_keyframes;
}

void KeyframeEffect::setKeyframes(ScriptState* script_state,
                                  const ScriptValue& keyframes,
                                  ExceptionState& exception_state) {
  StringKeyframeVector new_keyframes = EffectInput::ParseKeyframesArgument(
      target(), keyframes, script_state, exception_state);
  if (exception_state.HadException())
    return;

  ignore_css_keyframes_ = true;

  if (auto* model = DynamicTo<TransitionKeyframeEffectModel>(Model()))
    SetModel(model->CloneAsEmptyStringKeyframeModel());

  DCHECK(Model()->IsStringKeyframeEffectModel());
  SetKeyframes(new_keyframes);
}

void KeyframeEffect::SetKeyframes(StringKeyframeVector keyframes) {
  Model()->SetComposite(
      EffectInput::ResolveCompositeOperation(Model()->Composite(), keyframes));

  To<StringKeyframeEffectModel>(Model())->SetFrames(keyframes);

  // Changing the keyframes will invalidate any sampled effect, as well as
  // potentially affect the effect owner.
  ClearEffects();
  InvalidateAndNotifyOwner();
  CountAnimatedProperties();
}

bool KeyframeEffect::Affects(const PropertyHandle& property) const {
  return model_->Affects(property);
}

bool KeyframeEffect::HasRevert() const {
  return model_->HasRevert();
}

void KeyframeEffect::NotifySampledEffectRemovedFromEffectStack() {
  sampled_effect_ = nullptr;
}

CompositorAnimations::FailureReasons
KeyframeEffect::CheckCanStartAnimationOnCompositor(
    const PaintArtifactCompositor* paint_artifact_compositor,
    double animation_playback_rate,
    PropertyHandleSet* unsupported_properties) const {
  CompositorAnimations::FailureReasons reasons =
      CompositorAnimations::kNoFailure;

  // There would be no reason to composite an effect that has no keyframes; it
  // has no visual result.
  if (model_->Properties().IsEmpty())
    reasons |= CompositorAnimations::kInvalidAnimationOrEffect;

  // There would be no reason to composite an effect that has no target; it has
  // no visual result.
  if (!effect_target_) {
    reasons |= CompositorAnimations::kInvalidAnimationOrEffect;
  } else {
    if (effect_target_->GetComputedStyle() &&
        effect_target_->GetComputedStyle()->HasOffset())
      reasons |= CompositorAnimations::kTargetHasCSSOffset;

    // Do not put transforms on compositor if more than one of them are defined
    // in computed style because they need to be explicitly ordered
    if (HasMultipleTransformProperties())
      reasons |= CompositorAnimations::kTargetHasMultipleTransformProperties;

    reasons |= CompositorAnimations::CheckCanStartAnimationOnCompositor(
        SpecifiedTiming(), *effect_target_, GetAnimation(), *Model(),
        paint_artifact_compositor, animation_playback_rate,
        unsupported_properties);
  }

  return reasons;
}

void KeyframeEffect::StartAnimationOnCompositor(
    int group,
    base::Optional<double> start_time,
    base::TimeDelta time_offset,
    double animation_playback_rate,
    CompositorAnimation* compositor_animation) {
  DCHECK(!HasActiveAnimationsOnCompositor());
  // TODO(petermayo): Maybe we should recheck that we can start on the
  // compositor if we have the compositable IDs somewhere.

  if (!compositor_animation)
    compositor_animation = GetAnimation()->GetCompositorAnimation();

  DCHECK(compositor_animation);
  DCHECK(effect_target_);
  DCHECK(Model());

  CompositorAnimations::StartAnimationOnCompositor(
      *effect_target_, group, start_time, time_offset, SpecifiedTiming(),
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
  if (!effect_target_ || !effect_target_->GetLayoutObject())
    return false;
  for (const auto& compositor_keyframe_model_id :
       compositor_keyframe_model_ids_) {
    CompositorAnimations::CancelAnimationOnCompositor(
        *effect_target_, compositor_animation, compositor_keyframe_model_id);
  }
  compositor_keyframe_model_ids_.clear();
  return true;
}

void KeyframeEffect::CancelIncompatibleAnimationsOnCompositor() {
  if (effect_target_ && GetAnimation() && model_->HasFrames()) {
    CompositorAnimations::CancelIncompatibleAnimationsOnCompositor(
        *effect_target_, *GetAnimation(), *Model());
  }
}

void KeyframeEffect::PauseAnimationForTestingOnCompositor(
    base::TimeDelta pause_time) {
  DCHECK(HasActiveAnimationsOnCompositor());
  if (!effect_target_ || !effect_target_->GetLayoutObject())
    return;
  DCHECK(GetAnimation());
  for (const auto& compositor_keyframe_model_id :
       compositor_keyframe_model_ids_) {
    CompositorAnimations::PauseAnimationForTestingOnCompositor(
        *effect_target_, *GetAnimation(), compositor_keyframe_model_id,
        pause_time);
  }
}

void KeyframeEffect::AttachCompositedLayers() {
  DCHECK(effect_target_);
  DCHECK(GetAnimation());
  CompositorAnimations::AttachCompositedLayers(
      *effect_target_, GetAnimation()->GetCompositorAnimation());
}

bool KeyframeEffect::HasAnimation() const {
  return !!owner_;
}

bool KeyframeEffect::HasPlayingAnimation() const {
  return owner_ && owner_->Playing();
}

void KeyframeEffect::Trace(Visitor* visitor) const {
  visitor->Trace(effect_target_);
  visitor->Trace(target_element_);
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
        To<CompositorKeyframeTransform>(value)->GetTransformOperations();
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
  if (!effect_target_ || !model_->HasFrames())
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
      effect_target_->EnsureElementAnimations().GetEffectStack().Add(
          sampled_effect);
      changed = true;
    } else {
      return;
    }
  }

  if (changed) {
    effect_target_->SetNeedsAnimationStyleRecalc();
    auto* svg_element = DynamicTo<SVGElement>(effect_target_.Get());
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
  if (!effect_target_->GetDocument().Lifecycle().InDetach())
    effect_target_->SetNeedsAnimationStyleRecalc();
  auto* svg_element = DynamicTo<SVGElement>(effect_target_.Get());
  if (RuntimeEnabledFeatures::WebAnimationsSVGEnabled() && svg_element)
    svg_element->ClearWebAnimatedAttributes();
  Invalidate();
}

void KeyframeEffect::UpdateChildrenAndEffects() const {
  if (!model_->HasFrames())
    return;
  DCHECK(owner_);
  if (IsInEffect() && !owner_->EffectSuppressed() &&
      !owner_->ReplaceStateRemoved())
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
  if (!effect_target_ || !animation)
    return;
  effect_target_->EnsureElementAnimations().Animations().insert(animation);
  effect_target_->SetNeedsAnimationStyleRecalc();
  auto* svg_element = DynamicTo<SVGElement>(effect_target_.Get());
  if (RuntimeEnabledFeatures::WebAnimationsSVGEnabled() && svg_element)
    svg_element->SetWebAnimationsPending();
}

void KeyframeEffect::DetachTarget(Animation* animation) {
  if (effect_target_ && animation)
    effect_target_->GetElementAnimations()->Animations().erase(animation);
  // If we have sampled this effect previously, we need to purge that state.
  // ClearEffects takes care of clearing the cached sampled effect, informing
  // the target that it needs to refresh its style, and doing any necessary
  // update on the compositor.
  ClearEffects();
}

AnimationTimeDelta KeyframeEffect::CalculateTimeToEffectChange(
    bool forwards,
    base::Optional<double> local_time,
    AnimationTimeDelta time_to_next_iteration) const {
  const double start_time = SpecifiedTiming().start_delay;
  const double end_time_minus_end_delay =
      start_time + SpecifiedTiming().ActiveDuration();
  const double end_time =
      end_time_minus_end_delay + SpecifiedTiming().end_delay;
  const double after_time = std::min(end_time_minus_end_delay, end_time);

  Timing::Phase phase = GetPhase();
  DCHECK(local_time || phase == Timing::kPhaseNone);
  switch (phase) {
    case Timing::kPhaseNone:
      return AnimationTimeDelta::Max();
    case Timing::kPhaseBefore:
      // Return value is clamped at 0 to prevent unexpected results that could
      // be caused by returning negative values.
      return forwards ? AnimationTimeDelta::FromSecondsD(std::max<double>(
                            start_time - local_time.value(), 0))
                      : AnimationTimeDelta::Max();
    case Timing::kPhaseActive:
      if (forwards) {
        // Need service to apply fill / fire events.
        const double time_to_end = after_time - local_time.value();
        if (RequiresIterationEvents()) {
          return std::min(AnimationTimeDelta::FromSecondsD(time_to_end),
                          time_to_next_iteration);
        }
        return AnimationTimeDelta::FromSecondsD(time_to_end);
      }
      return {};
    case Timing::kPhaseAfter:
      DCHECK_GE(local_time.value(), after_time);
      if (forwards) {
        // If an animation has a positive-valued end delay, we need an
        // additional tick at the end time to ensure that the finished event is
        // delivered.
        return end_time > local_time ? AnimationTimeDelta::FromSecondsD(
                                           end_time - local_time.value())
                                     : AnimationTimeDelta::Max();
      }
      return AnimationTimeDelta::FromSecondsD(local_time.value() - after_time);
    default:
      NOTREACHED();
      return AnimationTimeDelta::Max();
  }
}

// Returns true if transform, translate, rotate or scale is composited
// and a motion path or other transform properties
// has been introduced on the element
bool KeyframeEffect::HasIncompatibleStyle() const {
  if (!effect_target_->GetComputedStyle())
    return false;

  if (HasActiveAnimationsOnCompositor()) {
    if (effect_target_->GetComputedStyle()->HasOffset()) {
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
  if (!effect_target_->GetComputedStyle())
    return false;

  unsigned transform_property_count = 0;
  if (effect_target_->GetComputedStyle()->HasTransformOperations())
    transform_property_count++;
  if (effect_target_->GetComputedStyle()->Rotate())
    transform_property_count++;
  if (effect_target_->GetComputedStyle()->Scale())
    transform_property_count++;
  if (effect_target_->GetComputedStyle()->Translate())
    transform_property_count++;
  return transform_property_count > 1;
}

ActiveInterpolationsMap KeyframeEffect::InterpolationsForCommitStyles() {
  // If the associated animation has been removed, it needs to be temporarily
  // reintroduced to the effect stack in order to be including in the
  // interpolations map.
  bool removed = owner_->ReplaceStateRemoved();
  if (removed)
    ApplyEffects();

  ActiveInterpolationsMap results = EffectStack::ActiveInterpolations(
      &target()->GetElementAnimations()->GetEffectStack(),
      /*new_animations=*/nullptr,
      /*suppressed_animations=*/nullptr, kDefaultPriority,
      /*property_pass_filter=*/nullptr, this);

  if (removed)
    ClearEffects();

  return results;
}

void KeyframeEffect::SetLogicalPropertyResolutionContext(
    TextDirection text_direction,
    WritingMode writing_mode) {
  if (auto* model = DynamicTo<StringKeyframeEffectModel>(Model())) {
    if (model->SetLogicalPropertyResolutionContext(text_direction,
                                                   writing_mode)) {
      ClearEffects();
      InvalidateAndNotifyOwner();
    }
  }
}

void KeyframeEffect::CountAnimatedProperties() const {
  if (target_element_) {
    Document& document = target_element_->GetDocument();
    for (const auto& property : model_->Properties()) {
      if (property.IsCSSProperty()) {
        DCHECK(isValidCSSPropertyID(property.GetCSSProperty().PropertyID()));
        document.CountAnimatedProperty(property.GetCSSProperty().PropertyID());
      }
    }
  }
}

}  // namespace blink
