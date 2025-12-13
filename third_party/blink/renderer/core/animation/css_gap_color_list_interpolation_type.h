// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_GAP_COLOR_LIST_INTERPOLATION_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_GAP_COLOR_LIST_INTERPOLATION_TYPE_H_

#include "third_party/blink/renderer/core/animation/css_interpolation_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"

namespace blink {

// This class handles interpolation for column-rule-color and row-rule-color.
// Internally, these are represented as `GapDataList<StyleColor>` which are
// basically a list of colors, or a list of repeaters of colors, or both. As
// such, for interpolation purposes, we deal with this by interpolating an
// InterpolableList which contains `InterpolableColor` or
// `InterpolableGapColorAutoRepeater` objects.
//
// Since we allow interolating between different types (i.e. repeaters and
// non-repeaters), we expand any integer repeaters at conversion time and do
// `kLowestCommonMultiple` list length matching if the lengths don't match (as
// per the spec). If an auto repeater is present, we segment the list into three
// segments and only interpolate if the lengths match (across `from` and `to`):
// - `leading_values`, the list of values before the auto repeater (after
// expanding any integer repeaters)
// - `auto_repeater`, the auto repeater itself
// - `trailing_values`, the list of values after the auto repeater (after
// expanding any integer repeaters).
class CORE_EXPORT CSSGapColorListInterpolationType
    : public CSSInterpolationType {
 public:
  explicit CSSGapColorListInterpolationType(
      PropertyHandle property,
      const PropertyRegistration* registration = nullptr)
      : CSSInterpolationType(property, registration),
        property_id_(property.GetCSSProperty().PropertyID()) {
    CHECK(property_id_ == CSSPropertyID::kColumnRuleColor ||
          property_id_ == CSSPropertyID::kRowRuleColor);
  }

  InterpolationValue MaybeConvertStandardPropertyUnderlyingValue(
      const ComputedStyle& style) const final;

  void Composite(UnderlyingValueOwner& owner,
                 double underlying_fraction,
                 const InterpolationValue& value,
                 double interpolation_fraction) const final;

  void ApplyStandardPropertyValue(
      const InterpolableValue& interpolable_value,
      const NonInterpolableValue* non_interpolable_value,
      StyleResolverState& state) const final;

  static GapDataList<StyleColor> GetList(const CSSProperty& property,
                                         const ComputedStyle& style);

 private:
  void GetInitialStyleColorList(const CSSProperty& property,
                                const ComputedStyle& style,
                                HeapVector<StyleColor, 1>& result) const;

  InterpolationValue MaybeConvertNeutral(
      const InterpolationValue& underlying,
      ConversionCheckers& conversion_checkers) const final;

  InterpolationValue MaybeConvertInitial(
      const StyleResolverState& state,
      ConversionCheckers& conversion_checkers) const final;

  InterpolationValue MaybeConvertInherit(
      const StyleResolverState& state,
      ConversionCheckers& conversion_checkers) const final;

  InterpolationValue MaybeConvertValue(
      const CSSValue& value,
      const StyleResolverState& state,
      ConversionCheckers& conversion_checkers) const final;

  PairwiseInterpolationValue MaybeMergeSingles(
      InterpolationValue&& start,
      InterpolationValue&& end) const final;

  GapDataList<StyleColor> GetProperty(const ComputedStyle& style) const;

 private:
  CSSPropertyID property_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_GAP_COLOR_LIST_INTERPOLATION_TYPE_H_
