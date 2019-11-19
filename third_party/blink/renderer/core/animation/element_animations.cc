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

#if DCHECK_IS_ON()
// Under certain conditions ComputedStyle::operator==() may return false for
// differences that are permitted during an animation.
bool ShouldCheckComputedStyles(const ComputedStyle& base_computed_style,
                               const ComputedStyle& computed_style) {
  // The FontFaceCache version number may be increased without forcing a style
  // recalc (see crbug.com/471079).
  if (!base_computed_style.GetFont().IsFallbackValid())
    return false;
  // Images use instance equality rather than value equality (see
  // crbug.com/781461).
  for (CSSPropertyID id :
       {CSSPropertyID::kBackgroundImage, CSSPropertyID::kWebkitMaskImage}) {
    if (!CSSPropertyEquality::PropertiesEqual(
            PropertyHandle(CSSProperty::Get(id)), base_computed_style,
            computed_style)) {
      return false;
    }
  }
  return true;
}
#endif  // DCHECK_IS_ON()

}  // namespace

ElementAnimations::ElementAnimations() : animation_style_change_(false) {}

ElementAnimations::~ElementAnimations() = default;

void ElementAnimations::UpdateAnimationFlags(ComputedStyle& style) {
  for (const auto& entry : animations_) {
    const Animation& animation = *entry.key;
    DCHECK(animation.effect());
    // FIXME: Needs to consider AnimationGroup once added.
    DCHECK(animation.effect()->IsKeyframeEffect());
    const KeyframeEffect& effect = *ToKeyframeEffect(animation.effect());
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

void ElementAnimations::Trace(blink::Visitor* visitor) {
  visitor->Trace(css_animations_);
  visitor->Trace(effect_stack_);
  visitor->Trace(animations_);
  visitor->Trace(worklet_animations_);
}

const ComputedStyle* ElementAnimations::BaseComputedStyle() const {
// When DCHECK is on we lie and claim to never have a base computed style
// stored. This allows us to check that an invariant holds; see the comments in
// |UpdateBaseComputedStyle|.
#if !DCHECK_IS_ON()
  if (IsAnimationStyleChange())
    return base_computed_style_.get();
#endif
  return nullptr;
}

void ElementAnimations::UpdateBaseComputedStyle(
    const ComputedStyle* computed_style) {
  DCHECK(computed_style);
  if (!IsAnimationStyleChange()) {
    base_computed_style_ = nullptr;
    return;
  }
#if DCHECK_IS_ON()
  // The invariant in the base computed style optimization is that as long as
  // |IsAnimationStyleChange| is true, the computed style that would be
  // generated by the style resolver is equivalent to the one we hold
  // internally. To ensure this we disable the optimization when DCHECKs are
  // enabled, but keep the internal base computed style and make sure the
  // equivalency holds here.
  if (base_computed_style_ && computed_style &&
      ShouldCheckComputedStyles(*base_computed_style_, *computed_style)) {
    DCHECK(*base_computed_style_ == *computed_style);
  }
#endif
  base_computed_style_ = ComputedStyle::Clone(*computed_style);
}

void ElementAnimations::ClearBaseComputedStyle() {
  base_computed_style_ = nullptr;
}

bool ElementAnimations::AnimationsPreserveAxisAlignment() const {
  for (const auto& entry : animations_) {
    const Animation& animation = *entry.key;
    DCHECK(animation.effect());
    DCHECK(animation.effect()->IsKeyframeEffect());
    const KeyframeEffect& effect = *ToKeyframeEffect(animation.effect());
    if (!effect.AnimationsPreserveAxisAlignment())
      return false;
  }
  return true;
}

}  // namespace blink
