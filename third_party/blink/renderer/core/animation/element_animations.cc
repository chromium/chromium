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

ElementAnimations::ElementAnimations() : animation_style_change_(false) {}

ElementAnimations::~ElementAnimations() = default;

void ElementAnimations::RestartAnimationOnCompositor() {
  for (const auto& entry : animations_)
    entry.key->RestartAnimationOnCompositor();
}

void ElementAnimations::Trace(Visitor* visitor) const {
  visitor->Trace(css_animations_);
  visitor->Trace(css_image_animations_);
  visitor->Trace(effect_stack_);
  visitor->Trace(animations_);
  visitor->Trace(worklet_animations_);
  visitor->Trace(background_color_npw_data_);
  visitor->Trace(clip_path_npw_data_);
  NodeRareDataField::Trace(visitor);
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
  // Usually kInStyleRecalc or kInLayout, but sometimes SMIL can cause updates
  // post-style/layout. See crbug.com/523313381.
  if ((element.GetDocument().Lifecycle().GetState() <
       DocumentLifecycle::kInStyleRecalc) ||
      (element.GetDocument().Lifecycle().GetState() >
       DocumentLifecycle::kLayoutClean)) {
    DCHECK(false) << "RecalcCompositedStatusForKeyframeChange must not be "
                  << "called outside of style/layout.";
    base::debug::DumpWithoutCrashing();
  }
  if (!element.GetLayoutObject()) {
    return;
  }

  if (background_color_npw_data_) {
    background_color_npw_data_->SetNeedsKeyframeSnapshot();
  }
  if (clip_path_npw_data_) {
    clip_path_npw_data_->SetNeedsKeyframeSnapshot();
  }
}

NativePaintWorkletData* ElementAnimations::EnsureBackgroundColorNpwData(
    Element* element) {
  if (!background_color_npw_data_) {
    background_color_npw_data_ = MakeGarbageCollected<NativePaintWorkletData>(
        element, Animation::kBackgroundColorPaintWorklet);
  }
  return background_color_npw_data_;
}

NativePaintWorkletData* ElementAnimations::EnsureClipPathNpwData(
    Element* element) {
  if (!clip_path_npw_data_) {
    clip_path_npw_data_ = MakeGarbageCollected<NativePaintWorkletData>(
        element, Animation::kClipPathPaintWorklet);
    clip_path_npw_data_->SetUpdateTriggersPaintPropertyUpdate(true);
  }
  return clip_path_npw_data_;
}

void ElementAnimations::RecalcCompositedStatus(
    Element* element,
    Animation::CompositorPendingReason pending_reason) {
  Animation::NativePaintWorkletReasons reasons = Animation::kNoPaintWorklet;
  // Multiple animations targeting the same property cannot be composited as
  // the compositor does not support composite-ordering. The overlapping_reasons
  // flag is used to catch this condition.
  Animation::NativePaintWorkletReasons overlapping_reasons =
      Animation::kNoPaintWorklet;
  for (auto& entry : Animations()) {
    if (entry.key->CalculateAnimationPlayState() ==
        V8AnimationPlayState::Enum::kIdle) {
      continue;
    }

    Animation::NativePaintWorkletReasons reasons_to_add =
        entry.key->GetNativePaintWorkletReasons();

    overlapping_reasons |= reasons & reasons_to_add;
    reasons |= reasons_to_add;

    if (reasons_to_add & Animation::kBackgroundColorPaintWorklet) {
      EnsureBackgroundColorNpwData(element)->SetAnimation(entry.key);
    }
    if (reasons_to_add & Animation::kClipPathPaintWorklet) {
      EnsureClipPathNpwData(element)->SetAnimation(entry.key);
    }
  }

  if (background_color_npw_data_) {
    background_color_npw_data_->UpdateCompositedPaintStatus(
        reasons, overlapping_reasons);
    if (pending_reason ==
        Animation::CompositorPendingReason::kPendingEffectChange) {
      // TODO(kevers): We are over invalidating if the effect is invalidated
      // on a different animation from the one animating background color.
      // Recalc compositedStatus could take the animation instead of the
      // element as a parameter to remedy.
      background_color_npw_data_->SetAnimationCurve(nullptr);
    }
  }

  if (clip_path_npw_data_) {
    DocumentLifecycle::LifecycleState state =
        element->GetDocument().Lifecycle().GetState();
    CompositedPaintStatus status = CompositedClipPathStatus();
    bool has_compositing_decision =
        status == CompositedPaintStatus::kComposited ||
        status == CompositedPaintStatus::kNotComposited;
    if (state == DocumentLifecycle::kInPrePaint && has_compositing_decision) {
      DCHECK(false) << "A compositing decision must remain consistent "
                    << "throughout the pre-paint phase of the document "
                    << "lifecycle";
      base::debug::DumpWithoutCrashing();
    }
    clip_path_npw_data_->UpdateCompositedPaintStatus(reasons,
                                                     overlapping_reasons);
  }
}

bool ElementAnimations::SetCompositedPaintStatus(
    Member<NativePaintWorkletData>& data,
    CompositedPaintStatus status) {
  bool result = false;
  // ClipPathClipper::Fallback... and possibly other call sites set the paint
  // status to not-composited without checking for an animation. It is safe to
  // interpret this as "if there is an animation, it is not composited".
  DCHECK(data || status == CompositedPaintStatus::kNotComposited ||
         status == CompositedPaintStatus::kNoAnimation);
  if (data) {
    result = data->SetStatus(status);
  }
  if (status == CompositedPaintStatus::kNoAnimation) {
    data = nullptr;
  }
  return result;
}

bool ElementAnimations::SetCompositedClipPathStatus(
    CompositedPaintStatus status) {
  return SetCompositedPaintStatus(clip_path_npw_data_, status);
}

bool ElementAnimations::SetCompositedBackgroundColorStatus(
    CompositedPaintStatus status) {
  return SetCompositedPaintStatus(background_color_npw_data_, status);
}

void ElementAnimations::CancelCompositedAnimationsAffectingProperties(
    const CSSBitset& property_bitset) {
  for (auto& entry : animations_) {
    if (!entry.key->HasActiveAnimationsOnCompositor()) {
      continue;
    }
    KeyframeEffect* effect = DynamicTo<KeyframeEffect>(entry.key->effect());
    if (!effect) {
      continue;
    }

    for (const auto& property : effect->Model()->DynamicProperties()) {
      if (!property.IsCSSProperty()) {
        continue;
      }
      if (property_bitset.Has(property.GetCSSProperty().PropertyID())) {
        entry.key->SetCompositorPending(
            Animation::CompositorPendingReason::kPendingCancel);
        // No need to check the remaining properties once we have forced the
        // fallback to a main-thread animation.
        break;
      }
    }
  }
}

}  // namespace blink
