// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/svg_integer_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/interpolation_environment.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/svg/svg_integer.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

InterpolationValue SVGIntegerInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(MakeGarbageCollected<InterpolableNumber>(0));
}

InterpolationValue SVGIntegerInterpolationType::MaybeConvertSVGValue(
    const SVGPropertyBase& svg_value) const {
  if (svg_value.GetType() != kAnimatedInteger) {
    return nullptr;
  }
  return InterpolationValue(MakeGarbageCollected<InterpolableNumber>(
      To<SVGInteger>(svg_value).Value()));
}

SVGPropertyBase* SVGIntegerInterpolationType::AppliedSVGValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*) const {
  // Note: using default CSSToLengthConversionData here as it's
  // guaranteed to be a double.
  // TODO(crbug.com/325821290): Avoid InterpolableNumber here.
  double value = To<InterpolableNumber>(interpolable_value)
                     .Value(CSSToLengthConversionData());
  return MakeGarbageCollected<SVGInteger>(round(value));
}

}  // namespace blink
