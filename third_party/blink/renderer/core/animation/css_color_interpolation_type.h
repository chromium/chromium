// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_COLOR_INTERPOLATION_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_COLOR_INTERPOLATION_TYPE_H_

#include <memory>
#include "third_party/blink/renderer/core/animation/css_interpolation_type.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/graphics/color.h"

namespace blink {

class StyleColor;
struct OptionalStyleColor;

class CSSColorInterpolationType : public CSSInterpolationType {
 public:
  CSSColorInterpolationType(PropertyHandle property,
                            const PropertyRegistration* registration = nullptr)
      : CSSInterpolationType(property, registration) {}

  InterpolationValue MaybeConvertStandardPropertyUnderlyingValue(
      const ComputedStyle&) const final;
  void ApplyStandardPropertyValue(const InterpolableValue&,
                                  const NonInterpolableValue*,
                                  StyleResolverState&) const final;
  void Composite(UnderlyingValueOwner& underlying_value_owner,
                 double underlying_fraction,
                 const InterpolationValue& value,
                 double interpolation_fraction) const final;

  static std::unique_ptr<InterpolableValue> CreateInterpolableColor(
      const Color&);
  static std::unique_ptr<InterpolableValue> CreateInterpolableColor(CSSValueID);
  static std::unique_ptr<InterpolableValue> CreateInterpolableColor(
      const StyleColor&);
  static std::unique_ptr<InterpolableValue> MaybeCreateInterpolableColor(
      const CSSValue&);
  static Color ResolveInterpolableColor(
      const InterpolableValue& interpolable_color,
      const StyleResolverState&,
      bool is_visited = false,
      bool is_text_decoration = false);

 private:
  InterpolationValue MaybeConvertNeutral(const InterpolationValue& underlying,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertInitial(const StyleResolverState&,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertInherit(const StyleResolverState&,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertValue(const CSSValue&,
                                       const StyleResolverState*,
                                       ConversionCheckers&) const final;
  InterpolationValue ConvertStyleColorPair(const OptionalStyleColor&,
                                           const OptionalStyleColor&) const;

  const CSSValue* CreateCSSValue(const InterpolableValue&,
                                 const NonInterpolableValue*,
                                 const StyleResolverState&) const final;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_COLOR_INTERPOLATION_TYPE_H_
