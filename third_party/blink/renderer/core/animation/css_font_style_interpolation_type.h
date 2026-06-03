// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_FONT_STYLE_INTERPOLATION_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_FONT_STYLE_INTERPOLATION_TYPE_H_

#include "base/check_op.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_type.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"

namespace blink {

// Animates font-style by computed value: slope in degrees for oblique/normal,
// discrete across the italic/oblique boundary since italic is not on the slope
// axis. `italic`, `oblique`, and `oblique 14deg` all alias at
// FontSelectionValue(14) but must serialize distinctly, so the authored
// `FontDescription::StyleSyntax` is carried alongside the slope as a
// non-interpolable value. Interpolating between `oblique` and
// `oblique <angle>` stays continuous on the slope axis; the result keeps an
// explicit angle whenever either input had one.
// https://www.w3.org/TR/css-fonts-4/#font-style-prop
class CSSFontStyleInterpolationType : public CSSInterpolationType {
 public:
  explicit CSSFontStyleInterpolationType(PropertyHandle property)
      : CSSInterpolationType(property) {
    DCHECK_EQ(CssProperty().PropertyID(), CSSPropertyID::kFontStyle);
  }

  InterpolationValue MaybeConvertStandardPropertyUnderlyingValue(
      const ComputedStyle&) const final;
  PairwiseInterpolationValue MaybeMergeSingles(
      InterpolationValue&& start,
      InterpolationValue&& end) const final;
  void Composite(UnderlyingValueOwner&,
                 double underlying_fraction,
                 const InterpolationValue&,
                 double interpolation_fraction) const final;
  void ApplyStandardPropertyValue(const InterpolableValue&,
                                  const NonInterpolableValue*,
                                  StyleResolverState&) const final;

 private:
  InterpolationValue CreateFontStyleValue(FontSelectionValue,
                                          FontDescription::StyleSyntax) const;
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

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_FONT_STYLE_INTERPOLATION_TYPE_H_
