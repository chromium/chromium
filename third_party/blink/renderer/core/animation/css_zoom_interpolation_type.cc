// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_zoom_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/animation/tree_counting_checker.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"

namespace blink {

InterpolationValue CSSZoomInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    if (identifier_value->GetValueID() == CSSValueID::kNormal) {
      return InterpolationValue(MakeGarbageCollected<InterpolableNumber>(1.0));
    }
    return nullptr;
  }
  const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value);
  if (!primitive_value || !primitive_value->IsPercentage()) {
    return CSSNumberInterpolationType::MaybeConvertValue(value, state,
                                                         conversion_checkers);
  }
  const CSSLengthResolver& length_resolver = state.CssToLengthConversionData();
  if (primitive_value->IsElementDependent()) {
    conversion_checkers.push_back(TreeCountingChecker::Create(length_resolver));
  }
  return InterpolationValue(MakeGarbageCollected<InterpolableNumber>(
      primitive_value->ComputePercentage(length_resolver) / 100.0));
}

}  // namespace blink
