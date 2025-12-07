// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_font_style_interpolation_type.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/length_units_checker.h"
#include "third_party/blink/renderer/core/animation/tree_counting_checker.h"
#include "third_party/blink/renderer/core/css/css_font_style_range_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

class InheritedFontStyleChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit InheritedFontStyleChecker(FontSelectionValue font_style)
      : font_style_(font_style) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    return font_style_ == state.ParentStyle()->GetFontStyle();
  }

  const double font_style_;
};

InterpolationValue CSSFontStyleInterpolationType::CreateFontStyleValue(
    FontSelectionValue font_style) const {
  return InterpolationValue(
      MakeGarbageCollected<InterpolableNumber>(font_style));
}

InterpolationValue CSSFontStyleInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(MakeGarbageCollected<InterpolableNumber>(0));
}

InterpolationValue CSSFontStyleInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers& conversion_checkers) const {
  return CreateFontStyleValue(kNormalSlopeValue);
}

InterpolationValue CSSFontStyleInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  DCHECK(state.ParentStyle());
  FontSelectionValue inherited_font_style = state.ParentStyle()->GetFontStyle();
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedFontStyleChecker>(inherited_font_style));
  return CreateFontStyleValue(inherited_font_style);
}

InterpolationValue CSSFontStyleInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value &&
      identifier_value->GetValueID() == CSSValueID::kItalic) {
    return nullptr;
  }
  if (const auto* style_range_value =
          DynamicTo<cssvalue::CSSFontStyleRangeValue>(value)) {
    const CSSValueList* values = style_range_value->GetObliqueValues();
    if (values->length()) {
      const auto& primitive_value = To<CSSPrimitiveValue>(values->Item(0));
      if (primitive_value.IsElementDependent()) {
        conversion_checkers.push_back(
            TreeCountingChecker::Create(state.CssToLengthConversionData()));
      }
      CSSPrimitiveValue::LengthTypeFlags types;
      primitive_value.AccumulateLengthUnitTypes(types);
      if (CSSInterpolationType::ConversionChecker* length_units_checker =
              LengthUnitsChecker::MaybeCreate(types, state)) {
        conversion_checkers.push_back(length_units_checker);
      }
    }
  }
  return CreateFontStyleValue(StyleBuilderConverterBase::ConvertFontStyle(
      state.CssToLengthConversionData(), value));
}

InterpolationValue
CSSFontStyleInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return CreateFontStyleValue(style.GetFontStyle());
}

void CSSFontStyleInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*,
    StyleResolverState& state) const {
  state.GetFontBuilder().SetStyle(
      FontSelectionValue(ClampTo(To<InterpolableNumber>(interpolable_value)
                                     .Value(state.CssToLengthConversionData()),
                                 kMinObliqueValue, kMaxObliqueValue)));
}

}  // namespace blink
