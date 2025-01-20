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
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

ElementAnimations::ElementAnimations()
    : animation_style_change_(false),
      composited_background_color_status_(static_cast<unsigned>(
          CompositedPaintStatus::kNoAnimation)),
      composited_clip_path_status_(static_cast<unsigned>(
          CompositedPaintStatus::kNoAnimation)) {}

ElementAnimations::~ElementAnimations() = default;

void ElementAnimations::RestartAnimationOnCompositor() {
  for (const auto& entry : animations_)
    entry.key->RestartAnimationOnCompositor();
}

void ElementAnimations::Trace(Visitor* visitor) const {
  visitor->Trace(css_animations_);
  visitor->Trace(effect_stack_);
  visitor->Trace(animations_);
  visitor->Trace(worklet_animations_);
  ElementRareDataField::Trace(visitor);
}

bool ElementAnimations::UpdateBoxSizeAndCheckTransformAxisAlignment(
    const gfx::SizeF& box_size) {
  bool preserves_axis_alignment = true;
  for (auto& entry : animations_) {
    Animation& animation = *entry.key;
    if (auto* effect = DynamicTo<KeyframeEffect>(animation.effect())) {
      if (!effect->IsCurrent() && !effect->IsInEffect())
        continue;
      if (!effect->UpdateBoxSizeAndCheckTransformAxisAlignment(box_size))
        preserves_axis_alignment = false;
    }
  }
  return preserves_axis_alignment;
}

bool ElementAnimations::IsIdentityOrTranslation() const {
  for (auto& entry : animations_) {
    if (auto* effect = DynamicTo<KeyframeEffect>(entry.key->effect())) {
      if (!effect->IsCurrent() && !effect->IsInEffect())
        continue;
      if (!effect->IsIdentityOrTranslation())
        return false;
    }
  }
  return true;
}

bool ElementAnimations::HasCompositedPaintWorkletAnimation() {
  return CompositedBackgroundColorStatus() ==
             ElementAnimations::CompositedPaintStatus::kComposited ||
         CompositedClipPathStatus() ==
             ElementAnimations::CompositedPaintStatus::kComposited;
}

void ElementAnimations::RecalcCompositedStatusForKeyframeChange(
    Element& element,
    Animation::NativePaintWorkletReasons properties) {
  if (!element.GetLayoutObject()) {
    return;
  }
  if ((CompositedBackgroundColorStatus() ==
       ElementAnimations::CompositedPaintStatus::kComposited) &&
      (properties &
       Animation::NativePaintWorkletProperties::kBackgroundColorPaintWorklet)) {
    SetCompositedBackgroundColorStatus(
        ElementAnimations::CompositedPaintStatus::kNeedsRepaint);
    element.GetLayoutObject()->SetShouldDoFullPaintInvalidation();
  }
  if ((CompositedClipPathStatus() ==
       ElementAnimations::CompositedPaintStatus::kComposited) &&
      (properties &
       Animation::NativePaintWorkletProperties::kClipPathPaintWorklet)) {
    SetCompositedClipPathStatus(
        ElementAnimations::CompositedPaintStatus::kNeedsRepaint);
    element.GetLayoutObject()->SetShouldDoFullPaintInvalidation();
    // For clip paths, we also need to update the paint properties to switch
    // from path based to mask based clip.
    element.GetLayoutObject()->SetNeedsPaintPropertyUpdate();
  }
}

void ElementAnimations::RecalcCompositedStatus(Element* element) {
  Animation::NativePaintWorkletReasons reasons = Animation::kNoPaintWorklet;
  for (auto& entry : Animations()) {
    if (entry.key->CalculateAnimationPlayState() ==
        V8AnimationPlayState::Enum::kIdle) {
      continue;
    }
    reasons |= entry.key->GetNativePaintWorkletReasons();
  }

  if (RuntimeEnabledFeatures::CompositeBGColorAnimationEnabled()) {
    ElementAnimations::CompositedPaintStatus status =
        reasons & Animation::kBackgroundColorPaintWorklet
            ? ElementAnimations::CompositedPaintStatus::kNeedsRepaint
            : ElementAnimations::CompositedPaintStatus::kNoAnimation;
    if (SetCompositedBackgroundColorStatus(status) &&
        element->GetLayoutObject()) {
      element->GetLayoutObject()->SetShouldDoFullPaintInvalidation();
    }
  }
  if (RuntimeEnabledFeatures::CompositeClipPathAnimationEnabled()) {
    ElementAnimations::CompositedPaintStatus status =
        reasons & Animation::kClipPathPaintWorklet
            ? ElementAnimations::CompositedPaintStatus::kNeedsRepaint
            : ElementAnimations::CompositedPaintStatus::kNoAnimation;
    if (SetCompositedClipPathStatus(status) && element->GetLayoutObject()) {
      element->GetLayoutObject()->SetShouldDoFullPaintInvalidation();
      // For clip paths, we also need to update the paint properties to switch
      // from path based to mask based clip.
      element->GetLayoutObject()->SetNeedsPaintPropertyUpdate();
    }
  }
}

bool ElementAnimations::SetCompositedClipPathStatus(
    CompositedPaintStatus status) {
  if (static_cast<unsigned>(status) != composited_clip_path_status_) {
    composited_clip_path_status_ = static_cast<unsigned>(status);
    return true;
  }
  return false;
}

bool ElementAnimations::SetCompositedBackgroundColorStatus(
    CompositedPaintStatus status) {
  if (static_cast<unsigned>(status) != composited_background_color_status_) {
    composited_background_color_status_ = static_cast<unsigned>(status);
    return true;
  }
  return false;
}

}  // namespace blink
