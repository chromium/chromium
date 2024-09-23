// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/svg_integer_optional_integer_interpolation_type.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/animation/interpolation_environment.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/svg/svg_integer_optional_integer.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

InterpolationValue
SVGIntegerOptionalIntegerInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  auto* result = MakeGarbageCollected<InterpolableList>(2);
  result->Set(0, MakeGarbageCollected<InterpolableNumber>(0));
  result->Set(1, MakeGarbageCollected<InterpolableNumber>(0));
  return InterpolationValue(result);
}

InterpolationValue
SVGIntegerOptionalIntegerInterpolationType::MaybeConvertSVGValue(
    const SVGPropertyBase& svg_value) const {
  if (svg_value.GetType() != kAnimatedIntegerOptionalInteger) {
    return nullptr;
  }

  const auto& integer_optional_integer =
      To<SVGIntegerOptionalInteger>(svg_value);
  auto* result = MakeGarbageCollected<InterpolableList>(2);
  result->Set(0, MakeGarbageCollected<InterpolableNumber>(
                     integer_optional_integer.FirstInteger()->Value()));
  result->Set(1, MakeGarbageCollected<InterpolableNumber>(
                     integer_optional_integer.SecondInteger()->Value()));
  return InterpolationValue(result);
}

static SVGInteger* ToPositiveInteger(const InterpolableValue* number) {
  // Note: using default CSSToLengthConversionData here as it's
  // guaranteed to be a double.
  // TODO(crbug.com/325821290): Avoid InterpolableNumber here.
  return MakeGarbageCollected<SVGInteger>(ClampTo<int>(
      round(To<InterpolableNumber>(number)->Value(CSSToLengthConversionData())),
      1));
}

SVGPropertyBase* SVGIntegerOptionalIntegerInterpolationType::AppliedSVGValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*) const {
  const auto& list = To<InterpolableList>(interpolable_value);
  return MakeGarbageCollected<SVGIntegerOptionalInteger>(
      ToPositiveInteger(list.Get(0)), ToPositiveInteger(list.Get(1)));
}

}  // namespace blink
