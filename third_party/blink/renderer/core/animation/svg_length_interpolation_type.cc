// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/svg_length_interpolation_type.h"

#include <memory>

#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

InterpolableValue* SVGLengthInterpolationType::NeutralInterpolableValue() {
  return InterpolableLength::CreateNeutral();
}

InterpolationValue SVGLengthInterpolationType::MaybeConvertSVGLength(
    const SVGLength& length) {
  // TODO(crbug.com/991672): This doesn't work on calculated lengths with
  // unitless values, e.g., calc(1 + 1px). Note that unitless values in math
  // expressions remain numbers instead of being converted into |kUserUnit|
  // dimension values. Revisit this later.
  return InterpolationValue(
      InterpolableLength::MaybeConvertCSSValue(length.AsCSSPrimitiveValue()));
}

SVGLength* SVGLengthInterpolationType::ResolveInterpolableSVGLength(
    const InterpolableValue& interpolable_value,
    SVGLengthMode unit_mode,
    bool negative_values_forbidden) {
  const InterpolableLength& length = To<InterpolableLength>(interpolable_value);
  const CSSPrimitiveValue* primitive_value = length.CreateCSSValue(
      negative_values_forbidden ? Length::ValueRange::kNonNegative
                                : Length::ValueRange::kAll);
  return MakeGarbageCollected<SVGLength>(*primitive_value, unit_mode);
}

InterpolationValue SVGLengthInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(NeutralInterpolableValue());
}

InterpolationValue SVGLengthInterpolationType::MaybeConvertSVGValue(
    const SVGPropertyBase& svg_value) const {
  if (svg_value.GetType() != kAnimatedLength)
    return nullptr;

  return MaybeConvertSVGLength(To<SVGLength>(svg_value));
}

SVGPropertyBase* SVGLengthInterpolationType::AppliedSVGValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*) const {
  return ResolveInterpolableSVGLength(interpolable_value, unit_mode_,
                                      negative_values_forbidden_);
}

}  // namespace blink
