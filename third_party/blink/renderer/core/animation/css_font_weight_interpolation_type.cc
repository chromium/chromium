// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_font_weight_interpolation_type.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

class InheritedFontWeightChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit InheritedFontWeightChecker(FontSelectionValue font_weight)
      : font_weight_(font_weight) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    return font_weight_ == state.ParentStyle()->GetFontWeight();
  }

  const double font_weight_;
};

InterpolationValue CSSFontWeightInterpolationType::CreateFontWeightValue(
    FontSelectionValue font_weight) const {
  return InterpolationValue(
      MakeGarbageCollected<InterpolableNumber>(font_weight));
}

InterpolationValue CSSFontWeightInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(MakeGarbageCollected<InterpolableNumber>(0));
}

InterpolationValue CSSFontWeightInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers& conversion_checkers) const {
  return CreateFontWeightValue(kNormalWeightValue);
}

InterpolationValue CSSFontWeightInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle())
    return nullptr;
  FontSelectionValue inherited_font_weight =
      state.ParentStyle()->GetFontWeight();
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedFontWeightChecker>(inherited_font_weight));
  return CreateFontWeightValue(inherited_font_weight);
}

InterpolationValue CSSFontWeightInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState* state,
    ConversionCheckers& conversion_checkers) const {
  DCHECK(state);
  FontSelectionValue inherited_font_weight =
      state->ParentStyle()->GetFontWeight();
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    CSSValueID keyword = identifier_value->GetValueID();
    if (keyword == CSSValueID::kBolder || keyword == CSSValueID::kLighter) {
      conversion_checkers.push_back(
          MakeGarbageCollected<InheritedFontWeightChecker>(
              inherited_font_weight));
    }
  }
  return CreateFontWeightValue(StyleBuilderConverterBase::ConvertFontWeight(
      value, inherited_font_weight));
}

InterpolationValue
CSSFontWeightInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return CreateFontWeightValue(style.GetFontWeight());
}

void CSSFontWeightInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*,
    StyleResolverState& state) const {
  state.GetFontBuilder().SetWeight(FontSelectionValue(
      ClampTo(To<InterpolableNumber>(interpolable_value).Value(),
              kMinWeightValue, kMaxWeightValue)));
}

}  // namespace blink
