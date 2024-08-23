// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_font_stretch_interpolation_type.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

class InheritedFontStretchChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit InheritedFontStretchChecker(FontSelectionValue font_stretch)
      : font_stretch_(font_stretch) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    return font_stretch_ == state.ParentStyle()->GetFontStretch();
  }

  const double font_stretch_;
};

InterpolationValue CSSFontStretchInterpolationType::CreateFontStretchValue(
    FontSelectionValue font_stretch) const {
  return InterpolationValue(MakeGarbageCollected<InterpolableNumber>(
      font_stretch, CSSPrimitiveValue::UnitType::kPercentage));
}

InterpolationValue CSSFontStretchInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(MakeGarbageCollected<InterpolableNumber>(
      0, CSSPrimitiveValue::UnitType::kPercentage));
}

InterpolationValue CSSFontStretchInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers& conversion_checkers) const {
  return CreateFontStretchValue(kNormalWidthValue);
}

InterpolationValue CSSFontStretchInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle())
    return nullptr;
  FontSelectionValue inherited_font_stretch =
      state.ParentStyle()->GetFontStretch();
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedFontStretchChecker>(
          inherited_font_stretch));
  return CreateFontStretchValue(inherited_font_stretch);
}

InterpolationValue CSSFontStretchInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState* state,
    ConversionCheckers& conversion_checkers) const {
  if (const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value)) {
    if (primitive_value->IsPercentage()) {
      if (auto* numeric_value =
              DynamicTo<CSSNumericLiteralValue>(primitive_value)) {
        return CreateFontStretchValue(
            ClampTo<FontSelectionValue>(numeric_value->ComputePercentage()));
      }
      CHECK(primitive_value->IsMathFunctionValue());
      return InterpolationValue(MakeGarbageCollected<InterpolableNumber>(
          *To<CSSMathFunctionValue>(primitive_value)->ExpressionNode()));
    }
  }

  if (std::optional<FontSelectionValue> keyword =
          StyleBuilderConverter::ConvertFontStretchKeyword(value);
      keyword.has_value()) {
    return CreateFontStretchValue(keyword.value());
  }

  return CreateFontStretchValue(kNormalWidthValue);
}

InterpolationValue
CSSFontStretchInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return CreateFontStretchValue(style.GetFontStretch());
}

void CSSFontStretchInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*,
    StyleResolverState& state) const {
  state.GetFontBuilder().SetStretch(
      FontSelectionValue(ClampTo(To<InterpolableNumber>(interpolable_value)
                                     .Value(state.CssToLengthConversionData()),
                                 0.0)));
}

}  // namespace blink
