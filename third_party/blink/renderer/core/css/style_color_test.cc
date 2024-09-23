// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_color.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"

namespace blink {

namespace {

CSSValue* CreateCalcAddValue(CSSValueID value_a, CSSValueID value_b) {
  return CSSMathFunctionValue::Create(
      CSSMathExpressionOperation::CreateArithmeticOperation(
          CSSMathExpressionKeywordLiteral::Create(
              value_a, CSSMathExpressionKeywordLiteral::Context::kColorChannel),
          CSSMathExpressionKeywordLiteral::Create(
              value_b, CSSMathExpressionKeywordLiteral::Context::kColorChannel),
          CSSMathOperator::kAdd));
}

}  // namespace

TEST(StyleColorTest, ConstructionAndIsCurrentColor) {
  StyleColor default_value;
  EXPECT_TRUE(default_value.IsCurrentColor());

  StyleColor currentcolor(CSSValueID::kCurrentcolor);
  EXPECT_TRUE(currentcolor.IsCurrentColor());

  StyleColor red_rgb(Color(255, 0, 0));
  EXPECT_FALSE(red_rgb.IsCurrentColor());

  StyleColor unresolved_mix(
      MakeGarbageCollected<StyleColor::UnresolvedColorMix>(
          Color::ColorSpace::kSRGB, Color::HueInterpolationMethod::kShorter,
          currentcolor, red_rgb, 0.5, 1.0));
  EXPECT_FALSE(unresolved_mix.IsCurrentColor());
}

TEST(StyleColorTest, Equality) {
  StyleColor currentcolor_1;
  StyleColor currentcolor_2(CSSValueID::kCurrentcolor);
  EXPECT_EQ(currentcolor_1, currentcolor_2);
  StyleColor red_keyword(CSSValueID::kRed);
  EXPECT_NE(currentcolor_1, red_keyword);
  StyleColor rgba_transparent(Color(0, 0, 0, 0));
  EXPECT_NE(currentcolor_1, rgba_transparent);

  StyleColor red_rgb_1(Color(255, 0, 0));
  StyleColor red_rgb_2(Color(255, 0, 0));
  StyleColor blue_rgb(Color(0, 0, 255));
  EXPECT_EQ(red_rgb_1, red_rgb_2);
  EXPECT_NE(red_rgb_1, red_keyword);
  EXPECT_NE(red_rgb_1, blue_rgb);

  StyleColor red_rgb_system_color(Color(255, 0, 0), CSSValueID::kCanvastext);
  EXPECT_NE(red_rgb_system_color, red_rgb_1);

  StyleColor unresolved_mix_1(
      MakeGarbageCollected<StyleColor::UnresolvedColorMix>(
          Color::ColorSpace::kSRGB, Color::HueInterpolationMethod::kShorter,
          currentcolor_1, red_keyword, 0.5, 1.0));
  StyleColor unresolved_mix_2(
      MakeGarbageCollected<StyleColor::UnresolvedColorMix>(
          Color::ColorSpace::kSRGB, Color::HueInterpolationMethod::kShorter,
          currentcolor_1, red_rgb_1, 0.5, 1.0));
  CSSIdentifierValue* r = CSSIdentifierValue::Create(CSSValueID::kR);
  CSSIdentifierValue* b = CSSIdentifierValue::Create(CSSValueID::kB);
  StyleColor unresolved_relative_1(
      MakeGarbageCollected<StyleColor::UnresolvedRelativeColor>(
          currentcolor_1, Color::ColorSpace::kSRGB, *r, *r, *r, nullptr));
  StyleColor unresolved_relative_2(
      MakeGarbageCollected<StyleColor::UnresolvedRelativeColor>(
          currentcolor_1, Color::ColorSpace::kSRGB, *b, *b, *b, nullptr));
  EXPECT_NE(unresolved_mix_1, unresolved_mix_2);
  EXPECT_NE(unresolved_mix_1, unresolved_relative_1);
  EXPECT_NE(unresolved_relative_1, unresolved_relative_2);
  EXPECT_NE(unresolved_mix_1, red_keyword);
  EXPECT_NE(unresolved_mix_1, blue_rgb);
  EXPECT_NE(unresolved_mix_1, rgba_transparent);
  EXPECT_NE(rgba_transparent, unresolved_mix_1);
}

TEST(StyleColorTest, UnresolvedColorMix_Equality) {
  StyleColor currentcolor;
  StyleColor red_rgb(Color(255, 0, 0));
  StyleColor blue_rgb(Color(0, 0, 255));

  using UnresolvedColorMix = StyleColor::UnresolvedColorMix;
  UnresolvedColorMix* mix_1 = MakeGarbageCollected<UnresolvedColorMix>(
      Color::ColorSpace::kSRGB, Color::HueInterpolationMethod::kShorter,
      currentcolor, red_rgb, 0.25, 1.0);

  UnresolvedColorMix* mix_2 = MakeGarbageCollected<UnresolvedColorMix>(
      Color::ColorSpace::kSRGB, Color::HueInterpolationMethod::kShorter,
      currentcolor, red_rgb, 0.25, 1.0);
  EXPECT_EQ(*mix_1, *mix_2);

  UnresolvedColorMix* mix_3 = MakeGarbageCollected<UnresolvedColorMix>(
      Color::ColorSpace::kHSL, Color::HueInterpolationMethod::kShorter,
      currentcolor, red_rgb, 0.25, 1.0);
  EXPECT_NE(*mix_1, *mix_3);

  UnresolvedColorMix* mix_4 = MakeGarbageCollected<UnresolvedColorMix>(
      Color::ColorSpace::kSRGB, Color::HueInterpolationMethod::kLonger,
      currentcolor, red_rgb, 0.25, 1.0);
  EXPECT_NE(*mix_1, *mix_4);

  UnresolvedColorMix* mix_5 = MakeGarbageCollected<UnresolvedColorMix>(
      Color::ColorSpace::kSRGB, Color::HueInterpolationMethod::kShorter,
      red_rgb, currentcolor, 0.25, 1.0);
  EXPECT_NE(*mix_1, *mix_5);

  UnresolvedColorMix* mix_6 = MakeGarbageCollected<UnresolvedColorMix>(
      Color::ColorSpace::kSRGB, Color::HueInterpolationMethod::kShorter,
      currentcolor, blue_rgb, 0.25, 1.0);
  EXPECT_NE(*mix_1, *mix_6);

  UnresolvedColorMix* mix_7 = MakeGarbageCollected<UnresolvedColorMix>(
      Color::ColorSpace::kSRGB, Color::HueInterpolationMethod::kShorter,
      currentcolor, red_rgb, 0.75, 1.0);
  EXPECT_NE(*mix_1, *mix_7);

  UnresolvedColorMix* mix_8 = MakeGarbageCollected<UnresolvedColorMix>(
      Color::ColorSpace::kSRGB, Color::HueInterpolationMethod::kShorter,
      currentcolor, red_rgb, 0.25, 0.5);
  EXPECT_NE(*mix_1, *mix_8);
}

TEST(StyleColorTest, UnresolvedRelativeColor_Equality) {
  StyleColor currentcolor;
  StyleColor mix(MakeGarbageCollected<StyleColor::UnresolvedColorMix>(
      Color::ColorSpace::kSRGB, Color::HueInterpolationMethod::kShorter,
      currentcolor, StyleColor(Color(255, 0, 0)), 0.25, 1.0));

  CSSValue* r = CSSIdentifierValue::Create(CSSValueID::kR);
  CSSValue* none = CSSIdentifierValue::Create(CSSValueID::kNone);
  CSSValue* number =
      CSSNumericLiteralValue::Create(75, CSSPrimitiveValue::UnitType::kNumber);
  CSSValue* percent = CSSNumericLiteralValue::Create(
      25, CSSPrimitiveValue::UnitType::kPercentage);
  CSSValue* calc_1 = CreateCalcAddValue(CSSValueID::kR, CSSValueID::kG);
  CSSValue* calc_2 = CreateCalcAddValue(CSSValueID::kG, CSSValueID::kB);

  using UnresolvedRelativeColor = StyleColor::UnresolvedRelativeColor;
  UnresolvedRelativeColor* relative_1 =
      MakeGarbageCollected<UnresolvedRelativeColor>(
          currentcolor, Color::ColorSpace::kSRGB, *r, *number, *calc_1,
          nullptr);
  UnresolvedRelativeColor* relative_2 =
      MakeGarbageCollected<UnresolvedRelativeColor>(
          currentcolor, Color::ColorSpace::kSRGB, *r, *number, *calc_1,
          nullptr);
  EXPECT_EQ(*relative_1, *relative_2);

  UnresolvedRelativeColor* relative_3 =
      MakeGarbageCollected<UnresolvedRelativeColor>(
          mix, Color::ColorSpace::kSRGB, *r, *number, *calc_1, nullptr);
  EXPECT_NE(*relative_1, *relative_3);

  UnresolvedRelativeColor* relative_4 =
      MakeGarbageCollected<UnresolvedRelativeColor>(
          currentcolor, Color::ColorSpace::kHSL, *r, *number, *calc_1, nullptr);
  EXPECT_NE(*relative_1, *relative_4);

  UnresolvedRelativeColor* relative_5 =
      MakeGarbageCollected<UnresolvedRelativeColor>(
          currentcolor, Color::ColorSpace::kSRGB, *none, *number, *calc_1,
          nullptr);
  EXPECT_NE(*relative_1, *relative_5);

  UnresolvedRelativeColor* relative_6 =
      MakeGarbageCollected<UnresolvedRelativeColor>(
          currentcolor, Color::ColorSpace::kSRGB, *r, *percent, *calc_1,
          nullptr);
  EXPECT_NE(*relative_1, *relative_6);

  UnresolvedRelativeColor* relative_7 =
      MakeGarbageCollected<UnresolvedRelativeColor>(
          currentcolor, Color::ColorSpace::kSRGB, *r, *number, *calc_2,
          nullptr);
  EXPECT_NE(*relative_1, *relative_7);

  UnresolvedRelativeColor* relative_8 =
      MakeGarbageCollected<UnresolvedRelativeColor>(
          currentcolor, Color::ColorSpace::kSRGB, *r, *number, *calc_1,
          percent);
  EXPECT_NE(*relative_1, *relative_8);

  UnresolvedRelativeColor* relative_9 =
      MakeGarbageCollected<UnresolvedRelativeColor>(
          currentcolor, Color::ColorSpace::kSRGB, *r, *number, *calc_1,
          percent);
  EXPECT_EQ(*relative_8, *relative_9);
}

TEST(StyleColorTest, UnresolvedColorMix_ToCSSValue) {
  StyleColor currentcolor;
  StyleColor::UnresolvedColorMix* mix =
      MakeGarbageCollected<StyleColor::UnresolvedColorMix>(
          Color::ColorSpace::kSRGB, Color::HueInterpolationMethod::kShorter,
          currentcolor, StyleColor(Color(255, 0, 0)), 0.25, 1.0);

  CSSValue* value = mix->ToCSSValue();
  EXPECT_TRUE(value->IsColorMixValue());
  EXPECT_EQ(value->CssText(),
            "color-mix(in srgb, currentcolor 75%, rgb(255, 0, 0))");
}

TEST(StyleColorTest, UnresolvedRelativeColor_ToCSSValue) {
  StyleColor currentcolor;

  CSSValue* r = CSSIdentifierValue::Create(CSSValueID::kR);
  CSSValue* none = CSSIdentifierValue::Create(CSSValueID::kNone);
  CSSValue* number =
      CSSNumericLiteralValue::Create(75, CSSPrimitiveValue::UnitType::kNumber);
  CSSValue* percent = CSSNumericLiteralValue::Create(
      25, CSSPrimitiveValue::UnitType::kPercentage);
  CSSValue* calc = CreateCalcAddValue(CSSValueID::kR, CSSValueID::kG);

  using UnresolvedRelativeColor = StyleColor::UnresolvedRelativeColor;
  UnresolvedRelativeColor* relative_1 =
      MakeGarbageCollected<UnresolvedRelativeColor>(
          currentcolor, Color::ColorSpace::kSRGB, *r, *number, *calc, nullptr);
  CSSValue* value_1 = relative_1->ToCSSValue();
  EXPECT_TRUE(value_1->IsRelativeColorValue());
  EXPECT_EQ(value_1->CssText(),
            "color(from currentcolor srgb r 75 calc(r + g))");

  using UnresolvedRelativeColor = StyleColor::UnresolvedRelativeColor;
  UnresolvedRelativeColor* relative_2 =
      MakeGarbageCollected<UnresolvedRelativeColor>(
          currentcolor, Color::ColorSpace::kSRGB, *r, *percent, *none, nullptr);
  CSSValue* value_2 = relative_2->ToCSSValue();
  EXPECT_TRUE(value_2->IsRelativeColorValue());
  EXPECT_EQ(value_2->CssText(), "color(from currentcolor srgb r 25% none)");
}

TEST(StyleColorTest, UnresolvedRelativeColor_Resolve) {
  StyleColor currentcolor;
  Color rebeccapurple(102, 51, 153);

  // Note: This test compares serializations to allow tolerance for
  // floating-point rounding error.

  using UnresolvedRelativeColor = StyleColor::UnresolvedRelativeColor;
  UnresolvedRelativeColor* rgb = MakeGarbageCollected<UnresolvedRelativeColor>(
      currentcolor, Color::ColorSpace::kSRGB,
      *CreateCalcAddValue(CSSValueID::kR, CSSValueID::kG),
      *CSSNumericLiteralValue::Create(0, CSSPrimitiveValue::UnitType::kNumber),
      *CSSIdentifierValue::Create(CSSValueID::kNone), nullptr);
  EXPECT_EQ(
      rgb->Resolve(rebeccapurple).SerializeAsCSSColor(),
      Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.6, 0, std::nullopt, 1.0)
          .SerializeAsCSSColor());

  UnresolvedRelativeColor* hsl = MakeGarbageCollected<UnresolvedRelativeColor>(
      currentcolor, Color::ColorSpace::kHSL,
      *CSSIdentifierValue::Create(CSSValueID::kH),
      *CSSNumericLiteralValue::Create(20,
                                      CSSPrimitiveValue::UnitType::kPercentage),
      *CSSIdentifierValue::Create(CSSValueID::kL),
      CSSIdentifierValue::Create(CSSValueID::kAlpha));
  EXPECT_EQ(
      hsl->Resolve(rebeccapurple).SerializeAsCSSColor(),
      Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.4, 0.32, 0.48, 1.0)
          .SerializeAsCSSColor());

  UnresolvedRelativeColor* lch = MakeGarbageCollected<UnresolvedRelativeColor>(
      currentcolor, Color::ColorSpace::kLch,
      *CSSIdentifierValue::Create(CSSValueID::kL),
      *CSSIdentifierValue::Create(CSSValueID::kC),
      *CSSIdentifierValue::Create(CSSValueID::kH),
      CSSIdentifierValue::Create(CSSValueID::kAlpha));
  EXPECT_EQ(lch->Resolve(Color::FromColorSpace(Color::ColorSpace::kLch, 200,
                                               300, 400, 5))
                .SerializeAsCSSColor(),
            Color::FromColorSpace(Color::ColorSpace::kLch, 100, 300, 40, 1)
                .SerializeAsCSSColor());
}

}  // namespace blink
