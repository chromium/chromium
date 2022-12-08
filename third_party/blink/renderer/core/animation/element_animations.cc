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
          CompositedPaintStatus::kNeedsRepaintOrNoAnimation)),
      composited_clip_path_status_(static_cast<unsigned>(
          CompositedPaintStatus::kNeedsRepaintOrNoAnimation)) {}

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

void ElementAnimations::SetCompositedBackgroundColorStatus(
    CompositedPaintStatus status) {
  if (composited_background_color_status_ == static_cast<unsigned>(status))
    return;

  if (status == CompositedPaintStatus::kNotComposited) {
    // Ensure that animation is cancelled on the compositor. We do this ahead
    // of updating the status since the act of cancelling a background color
    // animation forces it back into the kNeedsRepaintOrNoAnimation state,
    // which we then need to stomp with a kNotComposited decision.
    PropertyHandle background_color_property =
        PropertyHandle(GetCSSPropertyBackgroundColor());
    for (auto& entry : Animations()) {
      KeyframeEffect* effect = DynamicTo<KeyframeEffect>(entry.key->effect());
      if (effect && effect->Affects(background_color_property)) {
        entry.key->CancelAnimationOnCompositor();
      }
    }
  }
  composited_background_color_status_ = static_cast<unsigned>(status);
}

}  // namespace blink
