// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/svg_integer_optional_integer_interpolation_type.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/animation/interpolation_environment.h"
#include "third_party/blink/renderer/core/svg/svg_integer_optional_integer.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

InterpolationValue
SVGIntegerOptionalIntegerInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  auto result = std::make_unique<InterpolableList>(2);
  result->Set(0, std::make_unique<InterpolableNumber>(0));
  result->Set(1, std::make_unique<InterpolableNumber>(0));
  return InterpolationValue(std::move(result));
}

InterpolationValue
SVGIntegerOptionalIntegerInterpolationType::MaybeConvertSVGValue(
    const SVGPropertyBase& svg_value) const {
  if (svg_value.GetType() != kAnimatedIntegerOptionalInteger)
    return nullptr;

  const SVGIntegerOptionalInteger& integer_optional_integer =
      ToSVGIntegerOptionalInteger(svg_value);
  auto result = std::make_unique<InterpolableList>(2);
  result->Set(0, std::make_unique<InterpolableNumber>(
                     integer_optional_integer.FirstInteger()->Value()));
  result->Set(1, std::make_unique<InterpolableNumber>(
                     integer_optional_integer.SecondInteger()->Value()));
  return InterpolationValue(std::move(result));
}

static SVGInteger* ToPositiveInteger(const InterpolableValue* number) {
  return MakeGarbageCollected<SVGInteger>(
      clampTo<int>(round(ToInterpolableNumber(number)->Value()), 1));
}

SVGPropertyBase* SVGIntegerOptionalIntegerInterpolationType::AppliedSVGValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*) const {
  const InterpolableList& list = ToInterpolableList(interpolable_value);
  return MakeGarbageCollected<SVGIntegerOptionalInteger>(
      ToPositiveInteger(list.Get(0)), ToPositiveInteger(list.Get(1)));
}

}  // namespace blink
