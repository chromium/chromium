// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/svg_length_interpolation_type.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/animation/svg_interpolation_environment.h"
#include "third_party/blink/renderer/core/css/css_resolution_units.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

std::unique_ptr<InterpolableValue>
SVGLengthInterpolationType::NeutralInterpolableValue() {
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
    const SVGLengthContext& length_context,
    SVGLengthMode unit_mode,
    bool negative_values_forbidden) {
  const InterpolableLength& length = To<InterpolableLength>(interpolable_value);
  const CSSPrimitiveValue* primitive_value = length.CreateCSSValue(
      negative_values_forbidden ? kValueRangeNonNegative : kValueRangeAll);

  // We optimise for the common case where only one unit type is involved.
  if (primitive_value->IsNumericLiteralValue())
    return MakeGarbageCollected<SVGLength>(*primitive_value, unit_mode);

  // SVGLength does not support calc expressions, so we convert to canonical
  // units.
  // TODO(crbug.com/991672): This code path uses |primitive_value| as a
  // temporary object to calculate the pixel values. Try to avoid that.
  const auto unit_type = CSSPrimitiveValue::UnitType::kUserUnits;
  const double value = length_context.ResolveValue(*primitive_value, unit_mode);

  auto* result =
      MakeGarbageCollected<SVGLength>(unit_mode);  // defaults to the length 0
  result->NewValueSpecifiedUnits(unit_type, value);
  return result;
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

  return MaybeConvertSVGLength(ToSVGLength(svg_value));
}

SVGPropertyBase* SVGLengthInterpolationType::AppliedSVGValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*) const {
  NOTREACHED();
  // This function is no longer called, because apply has been overridden.
  return nullptr;
}

void SVGLengthInterpolationType::Apply(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    InterpolationEnvironment& environment) const {
  SVGElement& element = ToSVGInterpolationEnvironment(environment).SvgElement();
  SVGLengthContext length_context(&element);
  element.SetWebAnimatedAttribute(
      Attribute(),
      ResolveInterpolableSVGLength(interpolable_value, length_context,
                                   unit_mode_, negative_values_forbidden_));
}

}  // namespace blink
