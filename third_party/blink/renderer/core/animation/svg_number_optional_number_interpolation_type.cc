// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/svg_number_optional_number_interpolation_type.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/animation/interpolation_environment.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/svg/svg_number_optional_number.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

InterpolationValue
SVGNumberOptionalNumberInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  auto* result = MakeGarbageCollected<InterpolableList>(2);
  result->Set(0, MakeGarbageCollected<InterpolableNumber>(0));
  result->Set(1, MakeGarbageCollected<InterpolableNumber>(0));
  return InterpolationValue(result);
}

InterpolationValue
SVGNumberOptionalNumberInterpolationType::MaybeConvertSVGValue(
    const SVGPropertyBase& svg_value) const {
  if (svg_value.GetType() != kAnimatedNumberOptionalNumber) {
    return nullptr;
  }

  const auto& number_optional_number = To<SVGNumberOptionalNumber>(svg_value);
  auto* result = MakeGarbageCollected<InterpolableList>(2);
  result->Set(0, MakeGarbageCollected<InterpolableNumber>(
                     number_optional_number.FirstNumber()->Value()));
  result->Set(1, MakeGarbageCollected<InterpolableNumber>(
                     number_optional_number.SecondNumber()->Value()));
  return InterpolationValue(result);
}

SVGPropertyBase* SVGNumberOptionalNumberInterpolationType::AppliedSVGValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*) const {
  const auto& list = To<InterpolableList>(interpolable_value);
  // Note: using default CSSToLengthConversionData here as it's
  // guaranteed to be a double.
  // TODO(crbug.com/325821290): Avoid InterpolableNumber here.
  CSSToLengthConversionData length_resolver;
  return MakeGarbageCollected<SVGNumberOptionalNumber>(
      MakeGarbageCollected<SVGNumber>(
          To<InterpolableNumber>(list.Get(0))->Value(length_resolver)),
      MakeGarbageCollected<SVGNumber>(
          To<InterpolableNumber>(list.Get(1))->Value(length_resolver)));
}

}  // namespace blink
