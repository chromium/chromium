// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_SHAPE_INTERPOLATION_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_SHAPE_INTERPOLATION_TYPE_H_

#include "third_party/blink/renderer/core/animation/css_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/non_interpolable_value.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

class CSSShapeInterpolationType : public CSSInterpolationType {
 public:
  explicit CSSShapeInterpolationType(PropertyHandle property)
      : CSSInterpolationType(property) {}

  void ApplyStandardPropertyValue(const InterpolableValue&,
                                  const NonInterpolableValue*,
                                  StyleResolverState&) const final;
  void Composite(UnderlyingValueOwner&,
                 double underlying_fraction,
                 const InterpolationValue&,
                 double interpolation_fraction) const final;

  static CORE_EXPORT bool IsShapeNonInterpolableValue(
      const NonInterpolableValue*);

  static CORE_EXPORT BasicShape* CreateShape(const InterpolableValue&,
                                             const NonInterpolableValue*,
                                             const CSSToLengthConversionData&);

  static InterpolationValue MaybeConvertCSSValue(
      const CSSValue& value,
      const CSSProperty& property,
      std::optional<GeometryBox> geometry_box,
      std::optional<CoordBox> coord_box,
      std::optional<ShapeBox> css_box = std::nullopt);
  static InterpolationValue MaybeConvertBasicShape(const BasicShape* shape,
                                                   const CSSProperty& property,
                                                   double zoom,
                                                   GeometryBox geometry_box,
                                                   CoordBox coord_box);
  static bool ShapesAreCompatible(const NonInterpolableValue& a,
                                  const NonInterpolableValue& b);
  static InterpolableValue* CreateNeutralValue(
      const NonInterpolableValue& non_interpolable);
  static NonInterpolableValue::Type ShapeNonInterpolableValueType();
  static std::optional<GeometryBox> GetGeometryBox(
      const NonInterpolableValue& value);
  static std::optional<CoordBox> GetCoordBox(const NonInterpolableValue& value);

 protected:
  InterpolationValue MaybeConvertNeutral(const InterpolationValue& underlying,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertInitial(const StyleResolverState&,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertInherit(const StyleResolverState&,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertValue(const CSSValue&,
                                       const StyleResolverState&,
                                       ConversionCheckers&) const final;
  InterpolationValue MaybeConvertStandardPropertyUnderlyingValue(
      const ComputedStyle&) const final;
  PairwiseInterpolationValue MaybeMergeSingles(
      InterpolationValue&& start,
      InterpolationValue&& end) const final;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_SHAPE_INTERPOLATION_TYPE_H_
