// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/svg_integer_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/interpolation_environment.h"
#include "third_party/blink/renderer/core/svg/svg_integer.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

InterpolationValue SVGIntegerInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(std::make_unique<InterpolableNumber>(0));
}

InterpolationValue SVGIntegerInterpolationType::MaybeConvertSVGValue(
    const SVGPropertyBase& svg_value) const {
  if (svg_value.GetType() != kAnimatedInteger)
    return nullptr;
  return InterpolationValue(
      std::make_unique<InterpolableNumber>(ToSVGInteger(svg_value).Value()));
}

SVGPropertyBase* SVGIntegerInterpolationType::AppliedSVGValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*) const {
  double value = ToInterpolableNumber(interpolable_value).Value();
  return MakeGarbageCollected<SVGInteger>(round(value));
}

}  // namespace blink
