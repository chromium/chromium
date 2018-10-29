// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_LENGTH_INTERPOLATION_FUNCTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_LENGTH_INTERPOLATION_FUNCTIONS_H_

#include <memory>
#include "third_party/blink/renderer/core/animation/interpolation_value.h"
#include "third_party/blink/renderer/core/animation/pairwise_interpolation_value.h"
#include "third_party/blink/renderer/platform/geometry/length.h"

namespace blink {

class CSSToLengthConversionData;

class LengthInterpolationFunctions {
  STATIC_ONLY(LengthInterpolationFunctions);

 public:
  static std::unique_ptr<InterpolableValue> CreateInterpolablePixels(
      double pixels);
  static InterpolationValue CreateInterpolablePercent(double percent);
  static std::unique_ptr<InterpolableList> CreateNeutralInterpolableValue();

  static InterpolationValue MaybeConvertCSSValue(const CSSValue&);
  static InterpolationValue MaybeConvertLength(const Length&, float zoom);
  static PairwiseInterpolationValue MergeSingles(InterpolationValue&& start,
                                                 InterpolationValue&& end);
  static bool NonInterpolableValuesAreCompatible(const NonInterpolableValue*,
                                                 const NonInterpolableValue*);
  static void Composite(std::unique_ptr<InterpolableValue>&,
                        scoped_refptr<NonInterpolableValue>&,
                        double underlying_fraction,
                        const InterpolableValue&,
                        const NonInterpolableValue*);
  static Length CreateLength(const InterpolableValue&,
                             const NonInterpolableValue*,
                             const CSSToLengthConversionData&,
                             ValueRange);

  // Unlike createLength() this preserves all specificed unit types via calc()
  // expressions.
  static const CSSValue* CreateCSSValue(const InterpolableValue&,
                                        const NonInterpolableValue*,
                                        ValueRange);

  static void SubtractFromOneHundredPercent(InterpolationValue& result);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_LENGTH_INTERPOLATION_FUNCTIONS_H_
