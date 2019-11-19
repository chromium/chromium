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

#include "third_party/blink/renderer/core/animation/effect_stack.h"

#include <algorithm>
#include "third_party/blink/renderer/core/animation/compositor_animations.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/invalidatable_interpolation.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

void CopyToActiveInterpolationsMap(
    const HeapVector<Member<Interpolation>>& source,
    EffectStack::PropertyHandleFilter property_handle_filter,
    ActiveInterpolationsMap& target) {
  for (const auto& interpolation : source) {
    PropertyHandle property = interpolation->GetProperty();
    if (property_handle_filter && !property_handle_filter(property))
      continue;

    ActiveInterpolationsMap::AddResult entry =
        target.insert(property, ActiveInterpolations());
    ActiveInterpolations& active_interpolations = entry.stored_value->value;

    // Assuming stacked effects are enabled, interpolations that depend on
    // underlying values (e.g. have a non-replace composite mode) should be
    // added onto the 'stack' of active interpolations. However any 'replace'
    // effect erases everything that came before it, so we must clear the stack
    // when that happens.
    const bool allow_stacked_effects =
        RuntimeEnabledFeatures::StackedCSSPropertyAnimationsEnabled() ||
        !property.IsCSSProperty() || property.IsPresentationAttribute();
    const bool effect_depends_on_underlying_value =
        interpolation->IsInvalidatableInterpolation() &&
        ToInvalidatableInterpolation(*interpolation).DependsOnUnderlyingValue();
    if (!allow_stacked_effects || !effect_depends_on_underlying_value)
      active_interpolations.clear();
    active_interpolations.push_back(interpolation);
  }
}

bool CompareSampledEffects(const Member<SampledEffect>& sampled_effect1,
                           const Member<SampledEffect>& sampled_effect2) {
  DCHECK(sampled_effect1 && sampled_effect2);
  return sampled_effect1->SequenceNumber() < sampled_effect2->SequenceNumber();
}

void CopyNewAnimationsToActiveInterpolationsMap(
    const HeapVector<Member<const InertEffect>>& new_animations,
    EffectStack::PropertyHandleFilter property_handle_filter,
    ActiveInterpolationsMap& result) {
  for (const auto& new_animation : new_animations) {
    HeapVector<Member<Interpolation>> sample;
    new_animation->Sample(sample);
    if (!sample.IsEmpty())
      CopyToActiveInterpolationsMap(sample, property_handle_filter, result);
  }
}

}  // namespace

EffectStack::EffectStack() = default;

bool EffectStack::HasActiveAnimationsOnCompositor(
    const PropertyHandle& property) const {
  for (const auto& sampled_effect : sampled_effects_) {
    if (sampled_effect->Effect() &&
        sampled_effect->Effect()->HasPlayingAnimation() &&
        sampled_effect->Effect()->HasActiveAnimationsOnCompositor(property))
      return true;
  }
  return false;
}

bool EffectStack::AffectsProperties(PropertyHandleFilter filter) const {
  for (const auto& sampled_effect : sampled_effects_) {
    for (const auto& interpolation : sampled_effect->Interpolations()) {
      if (filter(interpolation->GetProperty()))
        return true;
    }
  }
  return false;
}

ActiveInterpolationsMap EffectStack::ActiveInterpolations(
    EffectStack* effect_stack,
    const HeapVector<Member<const InertEffect>>* new_animations,
    const HeapHashSet<Member<const Animation>>* suppressed_animations,
    KeyframeEffect::Priority priority,
    PropertyHandleFilter property_handle_filter) {
  ActiveInterpolationsMap result;

  if (effect_stack) {
    HeapVector<Member<SampledEffect>>& sampled_effects =
        effect_stack->sampled_effects_;
    std::sort(sampled_effects.begin(), sampled_effects.end(),
              CompareSampledEffects);
    effect_stack->RemoveRedundantSampledEffects();
    for (const auto& sampled_effect : sampled_effects) {
      if (sampled_effect->GetPriority() != priority ||
          // TODO(majidvp): Instead of accessing the effect's animation move the
          // check inside KeyframeEffect. http://crbug.com/812410
          (suppressed_animations && sampled_effect->Effect() &&
           suppressed_animations->Contains(
               sampled_effect->Effect()->GetAnimation())))
        continue;
      CopyToActiveInterpolationsMap(sampled_effect->Interpolations(),
                                    property_handle_filter, result);
    }
  }

  if (new_animations) {
    CopyNewAnimationsToActiveInterpolationsMap(*new_animations,
                                               property_handle_filter, result);
  }
  return result;
}

void EffectStack::RemoveRedundantSampledEffects() {
  HashSet<PropertyHandle> replaced_properties;
  for (wtf_size_t i = sampled_effects_.size(); i--;) {
    SampledEffect& sampled_effect = *sampled_effects_[i];
    if (sampled_effect.WillNeverChange()) {
      sampled_effect.RemoveReplacedInterpolations(replaced_properties);
      sampled_effect.UpdateReplacedProperties(replaced_properties);
    }
  }

  wtf_size_t new_size = 0;
  for (auto& sampled_effect : sampled_effects_) {
    if (!sampled_effect->Interpolations().IsEmpty())
      sampled_effects_[new_size++].Swap(sampled_effect);
    else if (sampled_effect->Effect())
      sampled_effect->Effect()->NotifySampledEffectRemovedFromEffectStack();
  }
  sampled_effects_.Shrink(new_size);
}

void EffectStack::Trace(blink::Visitor* visitor) {
  visitor->Trace(sampled_effects_);
}

}  // namespace blink
