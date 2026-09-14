// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_TEXT_DECORATION_INSET_INTERPOLATION_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_TEXT_DECORATION_INSET_INTERPOLATION_TYPE_H_

#include "third_party/blink/renderer/core/animation/css_interpolation_type.h"

namespace blink {

class CSSTextDecorationInsetInterpolationType final
    : public CSSInterpolationType {
 public:
  explicit CSSTextDecorationInsetInterpolationType(PropertyHandle property)
      : CSSInterpolationType(property) {
    DCHECK_EQ(CSSPropertyID::kTextDecorationInset,
              property.GetCSSProperty().PropertyID());
  }

  InterpolationValue MaybeConvertStandardPropertyUnderlyingValue(
      const ComputedStyle&) const final;
  PairwiseInterpolationValue MaybeMergeSingles(
      InterpolationValue&& start,
      InterpolationValue&& end) const final;
  void ApplyStandardPropertyValue(const InterpolableValue&,
                                  const NonInterpolableValue*,
                                  StyleResolverState&) const final;
  void Composite(UnderlyingValueOwner&,
                 double underlying_fraction,
                 const InterpolationValue&,
                 double interpolation_fraction) const final;

 private:
  InterpolationValue MaybeConvertNeutral(const InterpolationValue& underlying,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertInitial(const StyleResolverState&,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertInherit(const StyleResolverState&,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertValue(const CSSValue&,
                                       const StyleResolverState&,
                                       ConversionCheckers&) const final;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_TEXT_DECORATION_INSET_INTERPOLATION_TYPE_H_
