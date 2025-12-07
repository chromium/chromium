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

#include "base/debug/dump_without_crashing.h"
#include "third_party/blink/renderer/core/css/css_property_equality.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

ElementAnimations::CompositedPaintStatus CalculateStatusFromNativePaintReasons(
    Animation::NativePaintWorkletReasons animation_type,
    Animation::NativePaintWorkletReasons aggregated_reasons,
    Animation::NativePaintWorkletReasons overlapping_reasons) {
  if (animation_type & aggregated_reasons) {
    return animation_type & overlapping_reasons
               ? ElementAnimations::CompositedPaintStatus::kNotComposited
               : ElementAnimations::CompositedPaintStatus::kNeedsRepaint;
  }
  return ElementAnimations::CompositedPaintStatus::kNoAnimation;
}

}  // namespace

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
  visitor->Trace(clip_path_paint_worklet_candidate_);
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
  if ((element.GetDocument().Lifecycle().GetState() !=
       DocumentLifecycle::kInStyleRecalc) &&
      (element.GetDocument().Lifecycle().GetState() !=
       DocumentLifecycle::kInPerformLayout)) {
    DCHECK(false) << "RecalcCompositedStatusForKeyframeChange must not be "
                  << "called outside of style/layout.";
    base::debug::DumpWithoutCrashing();
  }
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
  clip_path_paint_worklet_candidate_ = nullptr;
  Animation::NativePaintWorkletReasons reasons = Animation::kNoPaintWorklet;
  // Multiple animations targeting the same property cannot be compsoited as
  // the compositor does not support composite-ordering.
  Animation::NativePaintWorkletReasons overlapping_reasons =
      Animation::kNoPaintWorklet;
  for (auto& entry : Animations()) {
    if (entry.key->CalculateAnimationPlayState() ==
        V8AnimationPlayState::Enum::kIdle) {
      continue;
    }

    overlapping_reasons |= reasons & entry.key->GetNativePaintWorkletReasons();
    reasons |= entry.key->GetNativePaintWorkletReasons();

    if (entry.key->GetNativePaintWorkletReasons() &
        Animation::kClipPathPaintWorklet) {
      clip_path_paint_worklet_candidate_ = entry.key;
    }
  }

  if (RuntimeEnabledFeatures::CompositeBGColorAnimationEnabled()) {
    ElementAnimations::CompositedPaintStatus status =
        CalculateStatusFromNativePaintReasons(
            Animation::kBackgroundColorPaintWorklet, reasons,
            overlapping_reasons);
    if (SetCompositedBackgroundColorStatus(status) &&
        element->GetLayoutObject()) {
      element->GetLayoutObject()->SetShouldDoFullPaintInvalidation();
    }
  }
  if (RuntimeEnabledFeatures::CompositeClipPathAnimationEnabled()) {
    ElementAnimations::CompositedPaintStatus status =
        CalculateStatusFromNativePaintReasons(Animation::kClipPathPaintWorklet,
                                              reasons, overlapping_reasons);
    // Must not run during paint or pre-paint. Can be run post-paint via JS,
    // during stop due to detach, and post-layout from the post style animation
    // update.
    if ((element->GetDocument().Lifecycle().GetState() ==
         DocumentLifecycle::kInPaint) ||
        (((composited_clip_path_status_ ==
           static_cast<unsigned>(
               ElementAnimations::CompositedPaintStatus::kComposited)) ||
          (composited_clip_path_status_ ==
           static_cast<unsigned>(
               ElementAnimations::CompositedPaintStatus::kNotComposited))) &&
         (element->GetDocument().Lifecycle().GetState() ==
          DocumentLifecycle::kInPrePaint))) {
      DCHECK(false) << "Composited clip path status must not be reset "
                    << "once it has been resolved in pre-paint.";
      base::debug::DumpWithoutCrashing();
    }
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
  if (status == ElementAnimations::CompositedPaintStatus::kNotComposited ||
      status == ElementAnimations::CompositedPaintStatus::kNoAnimation) {
    clip_path_paint_worklet_candidate_ = nullptr;
  }

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
