// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animatable.h"

#include "third_party/blink/renderer/bindings/core/v8/unrestricted_double_or_keyframe_animation_options.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/effect_input.h"
#include "third_party/blink/renderer/core/animation/effect_model.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/animation/timing_input.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/feature_policy/layout_animations_policy.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {
namespace {

// A helper method which is used to trigger a violation report for cases where
// the |element.animate| API is used to animate a CSS property which is blocked
// by the feature policy 'layout-animations'.
void ReportFeaturePolicyViolationsIfNecessary(
    const Document& document,
    const KeyframeEffectModelBase& effect) {
  for (const auto& property_handle : effect.Properties()) {
    if (!property_handle.IsCSSProperty())
      continue;
    const auto& css_property = property_handle.GetCSSProperty();
    if (LayoutAnimationsPolicy::AffectedCSSProperties().Contains(
            &css_property)) {
      LayoutAnimationsPolicy::ReportViolation(css_property, document);
    }
  }
}

}  // namespace

Animation* Animatable::animate(
    ScriptState* script_state,
    const ScriptValue& keyframes,
    const UnrestrictedDoubleOrKeyframeAnimationOptions& options,
    ExceptionState& exception_state) {
  EffectModel::CompositeOperation composite = EffectModel::kCompositeReplace;
  if (options.IsKeyframeAnimationOptions()) {
    composite = EffectModel::StringToCompositeOperation(
                    options.GetAsKeyframeAnimationOptions()->composite())
                    .value();
  }

  Element* element = GetAnimationTarget();
  KeyframeEffectModelBase* effect = EffectInput::Convert(
      element, keyframes, composite, script_state, exception_state);
  if (exception_state.HadException())
    return nullptr;

  Timing timing =
      TimingInput::Convert(options, &element->GetDocument(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  Animation* animation = animateInternal(*element, effect, timing);
  if (options.IsKeyframeAnimationOptions())
    animation->setId(options.GetAsKeyframeAnimationOptions()->id());
  return animation;
}

Animation* Animatable::animate(ScriptState* script_state,
                               const ScriptValue& keyframes,
                               ExceptionState& exception_state) {
  Element* element = GetAnimationTarget();
  KeyframeEffectModelBase* effect =
      EffectInput::Convert(element, keyframes, EffectModel::kCompositeReplace,
                           script_state, exception_state);
  if (exception_state.HadException())
    return nullptr;
  return animateInternal(*element, effect, Timing());
}

HeapVector<Member<Animation>> Animatable::getAnimations(
    GetAnimationsOptions* options) {
  bool use_subtree = options && options->subtree();
  Element* element = GetAnimationTarget();
  if (use_subtree)
    element->GetDocument().UpdateStyleAndLayoutTreeForSubtree(element);
  else
    element->GetDocument().UpdateStyleAndLayoutTreeForNode(element);

  HeapVector<Member<Animation>> animations;
  if (!use_subtree && !element->HasAnimations())
    return animations;

  for (const auto& animation :
       element->GetDocument().Timeline().getAnimations()) {
    DCHECK(animation->effect());
    Element* target = ToKeyframeEffect(animation->effect())->target();
    if (element == target || (use_subtree && element->contains(target))) {
      // DocumentTimeline::getAnimations should only give us animations that are
      // either current or in effect.
      DCHECK(animation->effect()->IsCurrent() ||
             animation->effect()->IsInEffect());
      animations.push_back(animation);
    }
  }
  return animations;
}

Animation* Animatable::animateInternal(Element& element,
                                       KeyframeEffectModelBase* effect,
                                       const Timing& timing) {
  ReportFeaturePolicyViolationsIfNecessary(element.GetDocument(), *effect);
  auto* keyframe_effect =
      MakeGarbageCollected<KeyframeEffect>(&element, effect, timing);
  return element.GetDocument().Timeline().Play(keyframe_effect);
}

}  // namespace blink
