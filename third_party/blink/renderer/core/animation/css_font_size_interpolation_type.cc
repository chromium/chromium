// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_font_size_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

namespace {

class IsMonospaceChecker : public CSSInterpolationType::CSSConversionChecker {
 public:
  IsMonospaceChecker(bool is_monospace) : is_monospace_(is_monospace) {}

 private:

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    return is_monospace_ == state.Style()->GetFontDescription().IsMonospace();
  }

  const bool is_monospace_;
};

class InheritedFontSizeChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedFontSizeChecker(const FontDescription::Size& inherited_font_size)
      : inherited_font_size_(inherited_font_size.value) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    return inherited_font_size_ ==
           state.ParentFontDescription().GetSize().value;
  }

  const float inherited_font_size_;
};

InterpolationValue ConvertFontSize(float size) {
  return InterpolationValue(InterpolableLength::CreatePixels(size));
}

InterpolationValue MaybeConvertKeyword(
    CSSValueID value_id,
    const StyleResolverState& state,
    InterpolationType::ConversionCheckers& conversion_checkers) {
  if (FontSizeFunctions::IsValidValueID(value_id)) {
    bool is_monospace = state.Style()->GetFontDescription().IsMonospace();
    conversion_checkers.push_back(
        std::make_unique<IsMonospaceChecker>(is_monospace));
    return ConvertFontSize(state.GetFontBuilder().FontSizeForKeyword(
        FontSizeFunctions::KeywordSize(value_id), is_monospace));
  }

  if (value_id != CSSValueID::kSmaller && value_id != CSSValueID::kLarger)
    return nullptr;

  const FontDescription::Size& inherited_font_size =
      state.ParentFontDescription().GetSize();
  conversion_checkers.push_back(
      std::make_unique<InheritedFontSizeChecker>(inherited_font_size));
  if (value_id == CSSValueID::kSmaller)
    return ConvertFontSize(
        FontDescription::SmallerSize(inherited_font_size).value);
  return ConvertFontSize(
      FontDescription::LargerSize(inherited_font_size).value);
}

}  // namespace

InterpolationValue CSSFontSizeInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(InterpolableLength::CreateNeutral());
}

InterpolationValue CSSFontSizeInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  return MaybeConvertKeyword(FontSizeFunctions::InitialValueID(), state,
                             conversion_checkers);
}

InterpolationValue CSSFontSizeInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  const FontDescription::Size& inherited_font_size =
      state.ParentFontDescription().GetSize();
  conversion_checkers.push_back(
      std::make_unique<InheritedFontSizeChecker>(inherited_font_size));
  return ConvertFontSize(inherited_font_size.value);
}

InterpolationValue CSSFontSizeInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState* state,
    ConversionCheckers& conversion_checkers) const {
  std::unique_ptr<InterpolableValue> result =
      InterpolableLength::MaybeConvertCSSValue(value);
  if (result)
    return InterpolationValue(std::move(result));

  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value)
    return nullptr;

  DCHECK(state);
  return MaybeConvertKeyword(identifier_value->GetValueID(), *state,
                             conversion_checkers);
}

InterpolationValue
CSSFontSizeInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return ConvertFontSize(style.SpecifiedFontSize());
}

void CSSFontSizeInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*,
    StyleResolverState& state) const {
  const FontDescription& parent_font = state.ParentFontDescription();
  Length font_size_length =
      To<InterpolableLength>(interpolable_value)
          .CreateLength(state.FontSizeConversionData(), kValueRangeNonNegative);
  float font_size =
      FloatValueForLength(font_size_length, parent_font.GetSize().value);
  state.GetFontBuilder().SetSize(FontDescription::Size(
      0, font_size,
      !font_size_length.IsPercentOrCalc() || parent_font.IsAbsoluteSize()));
}

}  // namespace blink
