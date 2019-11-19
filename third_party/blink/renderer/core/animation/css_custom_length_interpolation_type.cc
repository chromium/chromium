// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_custom_length_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"

namespace blink {

InterpolationValue CSSCustomLengthInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(InterpolableLength::CreateNeutral());
}

InterpolationValue CSSCustomLengthInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  std::unique_ptr<InterpolableLength> maybe_length =
      InterpolableLength::MaybeConvertCSSValue(value);
  if (!maybe_length || maybe_length->HasPercentage())
    return nullptr;
  return InterpolationValue(std::move(maybe_length));
}

const CSSValue* CSSCustomLengthInterpolationType::CreateCSSValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    const StyleResolverState&) const {
  const auto& interpolable_length = To<InterpolableLength>(interpolable_value);
  DCHECK(!interpolable_length.HasPercentage());
  return interpolable_length.CreateCSSValue(kValueRangeAll);
}

}  // namespace blink
