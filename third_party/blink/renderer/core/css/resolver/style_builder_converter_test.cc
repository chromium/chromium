// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_color_mix_value.h"
#include "third_party/blink/renderer/core/css/css_relative_color_value.h"

namespace blink {

TEST(StyleBuilderConverterTest,
     ResolveColorValue_SimplifyColorMixSubexpression) {
  const CSSIdentifierValue* red = CSSIdentifierValue::Create(CSSValueID::kRed);
  const CSSIdentifierValue* blue =
      CSSIdentifierValue::Create(CSSValueID::kBlue);
  const CSSIdentifierValue* currentcolor =
      CSSIdentifierValue::Create(CSSValueID::kCurrentcolor);
  const CSSNumericLiteralValue* percent = CSSNumericLiteralValue::Create(
      50, CSSPrimitiveValue::UnitType::kPercentage);

  const cssvalue::CSSColorMixValue* color_mix_sub_value =
      MakeGarbageCollected<cssvalue::CSSColorMixValue>(
          red, blue, percent, percent, Color::ColorSpace::kSRGB,
          Color::HueInterpolationMethod::kShorter);

  const cssvalue::CSSColorMixValue* color_mix_value =
      MakeGarbageCollected<cssvalue::CSSColorMixValue>(
          color_mix_sub_value, currentcolor, percent, percent,
          Color::ColorSpace::kSRGB, Color::HueInterpolationMethod::kShorter);

  const StyleColor expected(
      MakeGarbageCollected<StyleColor::UnresolvedColorMix>(
          Color::ColorSpace::kSRGB, Color::HueInterpolationMethod::kShorter,
          StyleColor(Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.5f, 0.0f,
                                           0.5f)),
          StyleColor(), 0.5f, 1.));

  const ResolveColorValueContext context{
      .length_resolver = CSSToLengthConversionData(),
      .text_link_colors = TextLinkColors()};
  EXPECT_EQ(ResolveColorValue(*color_mix_value, context), expected);
}

TEST(StyleBuilderConverterTest,
     ResolveColorValue_SimplifyRelativeColorSubexpression) {
  const CSSIdentifierValue* red = CSSIdentifierValue::Create(CSSValueID::kRed);
  const CSSIdentifierValue* r = CSSIdentifierValue::Create(CSSValueID::kR);
  const CSSIdentifierValue* currentcolor =
      CSSIdentifierValue::Create(CSSValueID::kCurrentcolor);
  const CSSNumericLiteralValue* percent = CSSNumericLiteralValue::Create(
      50, CSSPrimitiveValue::UnitType::kPercentage);

  const cssvalue::CSSRelativeColorValue* relative_color_value =
      MakeGarbageCollected<cssvalue::CSSRelativeColorValue>(
          *red, Color::ColorSpace::kSRGB, *r, *r, *r, nullptr);

  const cssvalue::CSSColorMixValue* color_mix_value =
      MakeGarbageCollected<cssvalue::CSSColorMixValue>(
          relative_color_value, currentcolor, percent, percent,
          Color::ColorSpace::kSRGB, Color::HueInterpolationMethod::kShorter);

  const StyleColor expected(
      MakeGarbageCollected<StyleColor::UnresolvedColorMix>(
          Color::ColorSpace::kSRGB, Color::HueInterpolationMethod::kShorter,
          StyleColor(Color::FromColorSpace(Color::ColorSpace::kSRGB, 1.0f, 1.0f,
                                           1.0f)),
          StyleColor(), 0.5f, 1.));

  const ResolveColorValueContext context{
      .length_resolver = CSSToLengthConversionData(),
      .text_link_colors = TextLinkColors()};
  EXPECT_EQ(ResolveColorValue(*color_mix_value, context), expected);
}

}  // namespace blink
