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

#include "third_party/blink/renderer/core/animation/element_animations.h"

#include "third_party/blink/renderer/core/css/css_property_equality.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

void UpdateAnimationFlagsForEffect(const KeyframeEffect& effect,
                                   ComputedStyle& style) {
  if (effect.Affects(PropertyHandle(GetCSSPropertyOpacity())))
    style.SetHasCurrentOpacityAnimation(true);
  if (effect.Affects(PropertyHandle(GetCSSPropertyTransform())) ||
      effect.Affects(PropertyHandle(GetCSSPropertyRotate())) ||
      effect.Affects(PropertyHandle(GetCSSPropertyScale())) ||
      effect.Affects(PropertyHandle(GetCSSPropertyTranslate())))
    style.SetHasCurrentTransformAnimation(true);
  if (effect.Affects(PropertyHandle(GetCSSPropertyFilter())))
    style.SetHasCurrentFilterAnimation(true);
  if (effect.Affects(PropertyHandle(GetCSSPropertyBackdropFilter())))
    style.SetHasCurrentBackdropFilterAnimation(true);
}

}  // namespace

ElementAnimations::ElementAnimations() : animation_style_change_(false) {}

ElementAnimations::~ElementAnimations() = default;

void ElementAnimations::UpdateAnimationFlags(ComputedStyle& style) {
  for (const auto& entry : animations_) {
    const Animation& animation = *entry.key;
    DCHECK(animation.effect());
    // FIXME: Needs to consider AnimationGroup once added.
    DCHECK(IsA<KeyframeEffect>(animation.effect()));
    const auto& effect = *To<KeyframeEffect>(animation.effect());
    if (!effect.IsCurrent())
      continue;
    UpdateAnimationFlagsForEffect(effect, style);
  }

  for (const auto& entry : worklet_animations_) {
    const KeyframeEffect& effect = *entry->GetEffect();
    // TODO(majidvp): we should check the effect's phase before updating the
    // style once the timing of effect is ready to use.
    // https://crbug.com/814851.
    UpdateAnimationFlagsForEffect(effect, style);
  }

  if (style.HasCurrentOpacityAnimation()) {
    style.SetIsRunningOpacityAnimationOnCompositor(
        effect_stack_.HasActiveAnimationsOnCompositor(
            PropertyHandle(GetCSSPropertyOpacity())));
  }
  if (style.HasCurrentTransformAnimation()) {
    style.SetIsRunningTransformAnimationOnCompositor(
        effect_stack_.HasActiveAnimationsOnCompositor(
            PropertyHandle(GetCSSPropertyTransform())));
  }
  if (style.HasCurrentFilterAnimation()) {
    style.SetIsRunningFilterAnimationOnCompositor(
        effect_stack_.HasActiveAnimationsOnCompositor(
            PropertyHandle(GetCSSPropertyFilter())));
  }
  if (style.HasCurrentBackdropFilterAnimation()) {
    style.SetIsRunningBackdropFilterAnimationOnCompositor(
        effect_stack_.HasActiveAnimationsOnCompositor(
            PropertyHandle(GetCSSPropertyBackdropFilter())));
  }
}

void ElementAnimations::RestartAnimationOnCompositor() {
  for (const auto& entry : animations_)
    entry.key->RestartAnimationOnCompositor();
}

void ElementAnimations::Trace(Visitor* visitor) const {
  visitor->Trace(css_animations_);
  visitor->Trace(effect_stack_);
  visitor->Trace(animations_);
  visitor->Trace(worklet_animations_);
}

const ComputedStyle* ElementAnimations::BaseComputedStyle() const {
  return base_computed_style_.get();
}

const CSSBitset* ElementAnimations::BaseImportantSet() const {
  if (IsAnimationStyleChange())
    return base_important_set_.get();
  return nullptr;
}

void ElementAnimations::UpdateBaseComputedStyle(
    const ComputedStyle* computed_style,
    std::unique_ptr<CSSBitset> base_important_set) {
  DCHECK(computed_style);
  base_computed_style_ = ComputedStyle::Clone(*computed_style);
  base_important_set_ = std::move(base_important_set);
}

void ElementAnimations::ClearBaseComputedStyle() {
  base_computed_style_ = nullptr;
  base_important_set_ = nullptr;
}

bool ElementAnimations::UpdateBoxSizeAndCheckTransformAxisAlignment(
    const FloatSize& box_size) {
  bool preserves_axis_alignment = true;
  for (auto& entry : animations_) {
    Animation& animation = *entry.key;
    if (auto* effect = DynamicTo<KeyframeEffect>(animation.effect())) {
      if (!effect->UpdateBoxSizeAndCheckTransformAxisAlignment(box_size))
        preserves_axis_alignment = false;
    }
  }
  return preserves_axis_alignment;
}

}  // namespace blink
