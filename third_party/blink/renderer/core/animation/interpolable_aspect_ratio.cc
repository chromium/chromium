// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_aspect_ratio.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/style_aspect_ratio.h"

namespace blink {

// static
InterpolableAspectRatio* InterpolableAspectRatio::MaybeCreate(
    const StyleAspectRatio& aspect_ratio) {
  // Auto aspect ratio cannot be interpolated to / from.
  if (aspect_ratio.IsAuto()) {
    return nullptr;
  }
  return MakeGarbageCollected<InterpolableAspectRatio>(aspect_ratio.GetRatio());
}

InterpolableAspectRatio::InterpolableAspectRatio(
    const gfx::SizeF& aspect_ratio) {
  // The StyleAspectRatio::IsAuto check in MaybeCreate should return true if we
  // have a degenerate aspect ratio.
  DCHECK(aspect_ratio.height() > 0 && aspect_ratio.width() > 0);

  value_ = MakeGarbageCollected<InterpolableNumber>(
      log(aspect_ratio.width() / aspect_ratio.height()));
}

gfx::SizeF InterpolableAspectRatio::GetRatio() const {
  return gfx::SizeF(exp(To<InterpolableNumber>(*value_).Value()), 1);
}

void InterpolableAspectRatio::Scale(double scale) {
  value_->Scale(scale);
}

void InterpolableAspectRatio::Add(const InterpolableValue& other) {
  value_->Add(*To<InterpolableAspectRatio>(other).value_);
}

void InterpolableAspectRatio::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  const InterpolableAspectRatio& other_aspect_ratio =
      To<InterpolableAspectRatio>(other);
  value_->AssertCanInterpolateWith(*other_aspect_ratio.value_);
}

void InterpolableAspectRatio::Interpolate(const InterpolableValue& to,
                                          const double progress,
                                          InterpolableValue& result) const {
  const InterpolableAspectRatio& aspect_ratio_to =
      To<InterpolableAspectRatio>(to);
  InterpolableAspectRatio& aspect_ratio_result =
      To<InterpolableAspectRatio>(result);
  value_->Interpolate(*aspect_ratio_to.value_, progress,
                      *aspect_ratio_result.value_);
}

}  // namespace blink
