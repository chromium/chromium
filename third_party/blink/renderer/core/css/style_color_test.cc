// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_color.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_alpha_color_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

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

class StyleColorTest : public PageTestBase {
 private:
  void SetUp() override { PageTestBase::SetUp(gfx::Size()); }
};

TEST_F(StyleColorTest, ConstructionAndIsCurrentColor) {
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

TEST_F(StyleColorTest, Equality) {
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
  UpdateAllLifecyclePhasesForTest();
  Element* element = GetDocument().documentElement();
  CSSToLengthConversionData::Flags ignored_flags = 0;
  CSSToLengthConversionData length_resolver(
      element->ComputedStyleRef(), element->GetComputedStyle(),
      element->GetComputedStyle(),
      CSSToLengthConversionData::ViewportSize(GetDocument().GetLayoutView()),
      CSSToLengthConversionData::ContainerSizes(element),
      CSSToLengthConversionData::AnchorData(), 1., ignored_flags, element);
  StyleColor unresolved_relative_1(
      MakeGarbageCollected<StyleColor::UnresolvedRelativeColor>(
          currentcolor_1, Color::ColorSpace::kSRGB, *r, *r, *r, nullptr,
          length_resolver));
  StyleColor unresolved_relative_2(
      MakeGarbageCollected<StyleColor::UnresolvedRelativeColor>(
          currentcolor_1, Color::ColorSpace::kSRGB, *b, *b, *b, nullptr,
          length_resolver));
  EXPECT_NE(unresolved_mix_1, unresolved_mix_2);
  EXPECT_NE(unresolved_mix_1, unresolved_relative_1);
  EXPECT_NE(unresolved_relative_1, unresolved_relative_2);
  EXPECT_NE(unresolved_mix_1, red_keyword);
  EXPECT_NE(unresolved_mix_1, blue_rgb);
  EXPECT_NE(unresolved_mix_1, rgba_transparent);
  EXPECT_NE(rgba_transparent, unresolved_mix_1);
}

TEST_F(StyleColorTest, UnresolvedColorMix_Equality) {
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

TEST_F(StyleColorTest, UnresolvedRelativeColor_Equality) {
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

  UpdateAllLifecyclePhasesForTest();
  Element* element = GetDocument().documentElement();
  CSSToLengthConversionData::Flags ignored_flags = 0;
  CSSToLengthConversionData length_resolver(
      element->ComputedStyleRef(), element->GetComputedStyle(),
      element->GetComputedStyle(),
      CSSToLengthConversionData::ViewportSize(GetDocument().GetLayoutView()),
      CSSToLengthConversionData::ContainerSizes(element),
      CSSToLengthConversionData::AnchorData(), 1., ignored_flags, element);
  using UnresolvedRelativeColor = StyleColor::UnresolvedRelativeColor;
  UnresolvedRelativeColor* relative_1 =
      MakeGarbageCollected<UnresolvedRelativeColor>(
          currentcolor, Color::ColorSpace::kSRGB, *r, *number, *calc_1, nullptr,
          length_resolver);
  UnresolvedRelativeColor* relative_2 =
      MakeGarbageCollected<UnresolvedRelativeColor>(
          currentcolor, Color::ColorSpace::kSRGB, *r, *number, *calc_1, nullptr,
          length_resolver);
  EXPECT_EQ(*relative_1, *relative_2);

  UnresolvedRelativeColor* relative_3 =
      MakeGarbageCollected<UnresolvedRelativeColor>(
          mix, Color::ColorSpace::kSRGB, *r, *number, *calc_1, nullptr,
          length_resolver);
  EXPECT_NE(*relative_1, *relative_3);

  UnresolvedRelativeColor* relative_4 =
      MakeGarbageCollected<UnresolvedRelativeColor>(
          currentcolor, Color::ColorSpace::kHSL, *r, *number, *calc_1, nullptr,
          length_resolver);
  EXPECT_NE(*relative_1, *relative_4);

  UnresolvedRelativeColor* relative_5 =
      MakeGarbageCollected<UnresolvedRelativeColor>(
          currentcolor, Color::ColorSpace::kSRGB, *none, *number, *calc_1,
          nullptr, length_resolver);
  EXPECT_NE(*relative_1, *relative_5);

  UnresolvedRelativeColor* relative_6 =
      MakeGarbageCollected<UnresolvedRelativeColor>(
          currentcolor, Color::ColorSpace::kSRGB, *r, *percent, *calc_1,
          nullptr, length_resolver);
  EXPECT_NE(*relative_1, *relative_6);

  UnresolvedRelativeColor* relative_7 =
      MakeGarbageCollected<UnresolvedRelativeColor>(
          currentcolor, Color::ColorSpace::kSRGB, *r, *number, *calc_2, nullptr,
          length_resolver);
  EXPECT_NE(*relative_1, *relative_7);

  UnresolvedRelativeColor* relative_8 =
      MakeGarbageCollected<UnresolvedRelativeColor>(
          currentcolor, Color::ColorSpace::kSRGB, *r, *number, *calc_1, percent,
          length_resolver);
  EXPECT_NE(*relative_1, *relative_8);

  UnresolvedRelativeColor* relative_9 =
      MakeGarbageCollected<UnresolvedRelativeColor>(
          currentcolor, Color::ColorSpace::kSRGB, *r, *number, *calc_1, percent,
          length_resolver);
  EXPECT_EQ(*relative_8, *relative_9);
}

TEST_F(StyleColorTest, UnresolvedColorMix_ToCSSValue) {
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

TEST_F(StyleColorTest, UnresolvedRelativeColor_ToCSSValue) {
  StyleColor currentcolor;

  CSSValue* r = CSSIdentifierValue::Create(CSSValueID::kR);
  CSSValue* none = CSSIdentifierValue::Create(CSSValueID::kNone);
  CSSValue* number =
      CSSNumericLiteralValue::Create(75, CSSPrimitiveValue::UnitType::kNumber);
  CSSValue* percent = CSSNumericLiteralValue::Create(
      25, CSSPrimitiveValue::UnitType::kPercentage);
  CSSValue* calc = CreateCalcAddValue(CSSValueID::kR, CSSValueID::kG);

  UpdateAllLifecyclePhasesForTest();
  Element* element = GetDocument().documentElement();
  CSSToLengthConversionData::Flags ignored_flags = 0;
  CSSToLengthConversionData length_resolver(
      element->ComputedStyleRef(), element->GetComputedStyle(),
      element->GetComputedStyle(),
      CSSToLengthConversionData::ViewportSize(GetDocument().GetLayoutView()),
      CSSToLengthConversionData::ContainerSizes(element),
      CSSToLengthConversionData::AnchorData(), 1., ignored_flags, element);
  using UnresolvedRelativeColor = StyleColor::UnresolvedRelativeColor;
  UnresolvedRelativeColor* relative_1 =
      MakeGarbageCollected<UnresolvedRelativeColor>(
          currentcolor, Color::ColorSpace::kSRGB, *r, *number, *calc, nullptr,
          length_resolver);
  CSSValue* value_1 = relative_1->ToCSSValue();
  EXPECT_TRUE(value_1->IsRelativeColorValue());
  EXPECT_EQ(value_1->CssText(),
            "color(from currentcolor srgb r 75 calc(r + g))");

  using UnresolvedRelativeColor = StyleColor::UnresolvedRelativeColor;
  UnresolvedRelativeColor* relative_2 =
      MakeGarbageCollected<UnresolvedRelativeColor>(
          currentcolor, Color::ColorSpace::kSRGB, *r, *percent, *none, nullptr,
          length_resolver);
  CSSValue* value_2 = relative_2->ToCSSValue();
  EXPECT_TRUE(value_2->IsRelativeColorValue());
  EXPECT_EQ(value_2->CssText(), "color(from currentcolor srgb r 25% none)");
}

TEST_F(StyleColorTest, UnresolvedRelativeColor_Resolve) {
  StyleColor currentcolor;
  Color rebeccapurple(102, 51, 153);

  // Note: This test compares serializations to allow tolerance for
  // floating-point rounding error.

  UpdateAllLifecyclePhasesForTest();
  Element* element = GetDocument().documentElement();
  CSSToLengthConversionData::Flags ignored_flags = 0;
  CSSToLengthConversionData length_resolver(
      element->ComputedStyleRef(), element->GetComputedStyle(),
      element->GetComputedStyle(),
      CSSToLengthConversionData::ViewportSize(GetDocument().GetLayoutView()),
      CSSToLengthConversionData::ContainerSizes(element),
      CSSToLengthConversionData::AnchorData(), 1., ignored_flags, element);
  using UnresolvedRelativeColor = StyleColor::UnresolvedRelativeColor;
  UnresolvedRelativeColor* rgb = MakeGarbageCollected<UnresolvedRelativeColor>(
      currentcolor, Color::ColorSpace::kSRGB,
      *CreateCalcAddValue(CSSValueID::kR, CSSValueID::kG),
      *CSSNumericLiteralValue::Create(0, CSSPrimitiveValue::UnitType::kNumber),
      *CSSIdentifierValue::Create(CSSValueID::kNone), nullptr, length_resolver);
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
      CSSIdentifierValue::Create(CSSValueID::kAlpha), length_resolver);
  EXPECT_EQ(hsl->Resolve(rebeccapurple).SerializeAsCSSColor(),
            Color::FromRGB(102, 82, 122).SerializeAsCSSColor());
  EXPECT_EQ(
      StyleColor(hsl)
          .Resolve(rebeccapurple, mojom::blink::ColorScheme::kLight, nullptr)
          .SerializeAsCSSColor(),
      Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.4, 0.32, 0.48, 1.0)
          .SerializeAsCSSColor());

  UnresolvedRelativeColor* lch = MakeGarbageCollected<UnresolvedRelativeColor>(
      currentcolor, Color::ColorSpace::kLch,
      *CSSIdentifierValue::Create(CSSValueID::kL),
      *CSSIdentifierValue::Create(CSSValueID::kC),
      *CSSIdentifierValue::Create(CSSValueID::kH),
      CSSIdentifierValue::Create(CSSValueID::kAlpha), length_resolver);
  EXPECT_EQ(lch->Resolve(Color::FromColorSpace(Color::ColorSpace::kLch, 200,
                                               300, 400, 5))
                .SerializeAsCSSColor(),
            Color::FromColorSpace(Color::ColorSpace::kLch, 100, 300, 40, 1)
                .SerializeAsCSSColor());
}

TEST_F(StyleColorTest, UnresolvedContrastColor_ToCSSValue) {
  StyleColor currentcolor;
  StyleColor green(Color(0, 128, 0));

  StyleColor::UnresolvedContrastColor* contrast_current =
      MakeGarbageCollected<StyleColor::UnresolvedContrastColor>(currentcolor);

  StyleColor::UnresolvedContrastColor* contrast_green =
      MakeGarbageCollected<StyleColor::UnresolvedContrastColor>(green);

  CSSValue* value = contrast_current->ToCSSValue();
  EXPECT_TRUE(value->IsContrastColorValue());
  EXPECT_EQ(value->CssText(), "contrast-color(currentcolor)");

  value = contrast_green->ToCSSValue();
  EXPECT_TRUE(value->IsContrastColorValue());
  EXPECT_EQ(value->CssText(), "contrast-color(rgb(0, 128, 0))");
}

TEST_F(StyleColorTest, UnresolvedContrastColor_Resolve) {
  StyleColor light(Color(220, 220, 220));
  StyleColor dark(Color(20, 20, 20));

  StyleColor::UnresolvedContrastColor* contrast_light =
      MakeGarbageCollected<StyleColor::UnresolvedContrastColor>(light);
  StyleColor::UnresolvedContrastColor* contrast_dark =
      MakeGarbageCollected<StyleColor::UnresolvedContrastColor>(dark);

  EXPECT_EQ(contrast_light->Resolve(Color::kBlack), Color::kBlack);
  EXPECT_EQ(contrast_dark->Resolve(Color::kBlack), Color::kWhite);
}

TEST_F(StyleColorTest, UnresolvedAlphaColor_Equality) {
  ScopedCSSAlphaColorFunctionForTest feature(true);

  StyleColor currentcolor;
  StyleColor red_rgb(Color(255, 0, 0));

  CSSValue* number_half =
      CSSNumericLiteralValue::Create(0.5, CSSPrimitiveValue::UnitType::kNumber);
  CSSValue* number_one =
      CSSNumericLiteralValue::Create(1.0, CSSPrimitiveValue::UnitType::kNumber);
  CSSValue* percent_50 = CSSNumericLiteralValue::Create(
      50, CSSPrimitiveValue::UnitType::kPercentage);

  UpdateAllLifecyclePhasesForTest();
  Element* element = GetDocument().documentElement();
  CSSToLengthConversionData::Flags ignored_flags = 0;
  CSSToLengthConversionData length_resolver(
      element->ComputedStyleRef(), element->GetComputedStyle(),
      element->GetComputedStyle(),
      CSSToLengthConversionData::ViewportSize(GetDocument().GetLayoutView()),
      CSSToLengthConversionData::ContainerSizes(element),
      CSSToLengthConversionData::AnchorData(), 1., ignored_flags, element);

  using UnresolvedAlphaColor = StyleColor::UnresolvedAlphaColor;

  // Same origin, same alpha => equal.
  UnresolvedAlphaColor* alpha_1 = MakeGarbageCollected<UnresolvedAlphaColor>(
      currentcolor, number_half, length_resolver);
  UnresolvedAlphaColor* alpha_2 = MakeGarbageCollected<UnresolvedAlphaColor>(
      currentcolor, number_half, length_resolver);
  EXPECT_EQ(*alpha_1, *alpha_2);

  // Different origin color => not equal.
  UnresolvedAlphaColor* alpha_3 = MakeGarbageCollected<UnresolvedAlphaColor>(
      red_rgb, number_half, length_resolver);
  EXPECT_NE(*alpha_1, *alpha_3);

  // Different alpha value => not equal.
  UnresolvedAlphaColor* alpha_4 = MakeGarbageCollected<UnresolvedAlphaColor>(
      currentcolor, number_one, length_resolver);
  EXPECT_NE(*alpha_1, *alpha_4);

  // Percentage alpha vs number alpha => not equal.
  UnresolvedAlphaColor* alpha_5 = MakeGarbageCollected<UnresolvedAlphaColor>(
      currentcolor, percent_50, length_resolver);
  EXPECT_NE(*alpha_1, *alpha_5);

  // Alpha specified vs not specified => not equal.
  UnresolvedAlphaColor* alpha_6 = MakeGarbageCollected<UnresolvedAlphaColor>(
      currentcolor, nullptr, length_resolver);
  EXPECT_NE(*alpha_1, *alpha_6);

  // Both with no alpha specified => equal.
  UnresolvedAlphaColor* alpha_7 = MakeGarbageCollected<UnresolvedAlphaColor>(
      currentcolor, nullptr, length_resolver);
  EXPECT_EQ(*alpha_6, *alpha_7);
}

TEST_F(StyleColorTest, UnresolvedAlphaColor_ToCSSValue) {
  ScopedCSSAlphaColorFunctionForTest feature(true);

  StyleColor currentcolor;

  CSSValue* number_half =
      CSSNumericLiteralValue::Create(0.5, CSSPrimitiveValue::UnitType::kNumber);

  UpdateAllLifecyclePhasesForTest();
  Element* element = GetDocument().documentElement();
  CSSToLengthConversionData::Flags ignored_flags = 0;
  CSSToLengthConversionData length_resolver(
      element->ComputedStyleRef(), element->GetComputedStyle(),
      element->GetComputedStyle(),
      CSSToLengthConversionData::ViewportSize(GetDocument().GetLayoutView()),
      CSSToLengthConversionData::ContainerSizes(element),
      CSSToLengthConversionData::AnchorData(), 1., ignored_flags, element);

  using UnresolvedAlphaColor = StyleColor::UnresolvedAlphaColor;
  UnresolvedAlphaColor* alpha = MakeGarbageCollected<UnresolvedAlphaColor>(
      currentcolor, number_half, length_resolver);
  CSSValue* value = alpha->ToCSSValue();
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->IsAlphaColorValue());
  EXPECT_EQ(value->CssText(), "alpha(from currentcolor / 0.5)");
}

TEST_F(StyleColorTest, UnresolvedAlphaColor_Resolve) {
  ScopedCSSAlphaColorFunctionForTest feature(true);

  StyleColor currentcolor;
  Color red = Color::FromRGB(255, 0, 0);
  Color semi_transparent_blue =
      Color::FromColorSpace(Color::ColorSpace::kSRGB, 0, 0, 1.0, 0.5);

  UpdateAllLifecyclePhasesForTest();
  Element* element = GetDocument().documentElement();
  CSSToLengthConversionData::Flags ignored_flags = 0;
  CSSToLengthConversionData length_resolver(
      element->ComputedStyleRef(), element->GetComputedStyle(),
      element->GetComputedStyle(),
      CSSToLengthConversionData::ViewportSize(GetDocument().GetLayoutView()),
      CSSToLengthConversionData::ContainerSizes(element),
      CSSToLengthConversionData::AnchorData(), 1., ignored_flags, element);

  using UnresolvedAlphaColor = StyleColor::UnresolvedAlphaColor;

  // alpha(from red / 0.5) => red with alpha 0.5
  CSSValue* half =
      CSSNumericLiteralValue::Create(0.5, CSSPrimitiveValue::UnitType::kNumber);
  UnresolvedAlphaColor* alpha_half = MakeGarbageCollected<UnresolvedAlphaColor>(
      StyleColor(red), half, length_resolver);
  Color resolved_half = alpha_half->Resolve(Color::kBlack);
  EXPECT_FLOAT_EQ(resolved_half.Alpha(), 0.5f);
  // Color components should be preserved (legacy sRGB uses 0-255 range).
  EXPECT_EQ(resolved_half.Param0(), red.Param0());
  EXPECT_EQ(resolved_half.Param1(), red.Param1());
  EXPECT_EQ(resolved_half.Param2(), red.Param2());

  // alpha(from red / 50%) => red with alpha 0.5
  CSSValue* percent_50 = CSSNumericLiteralValue::Create(
      50, CSSPrimitiveValue::UnitType::kPercentage);
  UnresolvedAlphaColor* alpha_percent =
      MakeGarbageCollected<UnresolvedAlphaColor>(StyleColor(red), percent_50,
                                                 length_resolver);
  Color resolved_percent = alpha_percent->Resolve(Color::kBlack);
  EXPECT_FLOAT_EQ(resolved_percent.Alpha(), 0.5f);

  // alpha(from red / none) => red with no alpha (nullopt)
  CSSValue* none = CSSIdentifierValue::Create(CSSValueID::kNone);
  UnresolvedAlphaColor* alpha_none = MakeGarbageCollected<UnresolvedAlphaColor>(
      StyleColor(red), none, length_resolver);
  Color resolved_none = alpha_none->Resolve(Color::kBlack);
  // "none" alpha results in a missing alpha component, which serializes
  // differently.
  EXPECT_EQ(resolved_none.GetColorSpace(), red.GetColorSpace());

  // alpha(from semiTransparentBlue / alpha) => preserves alpha from origin
  CSSValue* alpha_keyword = CSSIdentifierValue::Create(CSSValueID::kAlpha);
  UnresolvedAlphaColor* alpha_kw = MakeGarbageCollected<UnresolvedAlphaColor>(
      StyleColor(semi_transparent_blue), alpha_keyword, length_resolver);
  Color resolved_kw = alpha_kw->Resolve(Color::kBlack);
  EXPECT_FLOAT_EQ(resolved_kw.Alpha(), 0.5f);

  // alpha(from semiTransparentBlue / calc(alpha * 0.5)) => alpha becomes 0.25
  CSSValue* calc_alpha = CSSMathFunctionValue::Create(
      CSSMathExpressionOperation::CreateArithmeticOperation(
          CSSMathExpressionKeywordLiteral::Create(
              CSSValueID::kAlpha,
              CSSMathExpressionKeywordLiteral::Context::kColorChannel),
          CSSMathExpressionNumericLiteral::Create(
              CSSNumericLiteralValue::Create(
                  0.5, CSSPrimitiveValue::UnitType::kNumber)),
          CSSMathOperator::kMultiply));
  UnresolvedAlphaColor* alpha_calc = MakeGarbageCollected<UnresolvedAlphaColor>(
      StyleColor(semi_transparent_blue), calc_alpha, length_resolver);
  Color resolved_calc = alpha_calc->Resolve(Color::kBlack);
  EXPECT_FLOAT_EQ(resolved_calc.Alpha(), 0.25f);

  // alpha(from red) with no alpha specified => defaults to origin alpha (1.0)
  UnresolvedAlphaColor* alpha_default =
      MakeGarbageCollected<UnresolvedAlphaColor>(StyleColor(red), nullptr,
                                                 length_resolver);
  Color resolved_default = alpha_default->Resolve(Color::kBlack);
  EXPECT_FLOAT_EQ(resolved_default.Alpha(), 1.0f);

  // Test alpha clamping: values > 1.0 clamp to 1.0
  CSSValue* two =
      CSSNumericLiteralValue::Create(2.0, CSSPrimitiveValue::UnitType::kNumber);
  UnresolvedAlphaColor* alpha_clamp_high =
      MakeGarbageCollected<UnresolvedAlphaColor>(StyleColor(red), two,
                                                 length_resolver);
  Color resolved_clamp_high = alpha_clamp_high->Resolve(Color::kBlack);
  EXPECT_FLOAT_EQ(resolved_clamp_high.Alpha(), 1.0f);

  // Test alpha clamping: values < 0 clamp to 0
  CSSValue* negative = CSSNumericLiteralValue::Create(
      -0.5, CSSPrimitiveValue::UnitType::kNumber);
  UnresolvedAlphaColor* alpha_clamp_low =
      MakeGarbageCollected<UnresolvedAlphaColor>(StyleColor(red), negative,
                                                 length_resolver);
  Color resolved_clamp_low = alpha_clamp_low->Resolve(Color::kBlack);
  EXPECT_FLOAT_EQ(resolved_clamp_low.Alpha(), 0.0f);

  // alpha(from currentcolor / 0.5) resolves currentcolor to provided color
  UnresolvedAlphaColor* alpha_current =
      MakeGarbageCollected<UnresolvedAlphaColor>(currentcolor, half,
                                                 length_resolver);
  Color resolved_current = alpha_current->Resolve(red);
  EXPECT_FLOAT_EQ(resolved_current.Alpha(), 0.5f);
  EXPECT_EQ(resolved_current.Param0(), red.Param0());
}

TEST_F(StyleColorTest, UnresolvedAlphaColor_EqualityInStyleColor) {
  ScopedCSSAlphaColorFunctionForTest feature(true);

  StyleColor currentcolor;
  StyleColor red_rgb(Color(255, 0, 0));

  CSSValue* number_half =
      CSSNumericLiteralValue::Create(0.5, CSSPrimitiveValue::UnitType::kNumber);

  UpdateAllLifecyclePhasesForTest();
  Element* element = GetDocument().documentElement();
  CSSToLengthConversionData::Flags ignored_flags = 0;
  CSSToLengthConversionData length_resolver(
      element->ComputedStyleRef(), element->GetComputedStyle(),
      element->GetComputedStyle(),
      CSSToLengthConversionData::ViewportSize(GetDocument().GetLayoutView()),
      CSSToLengthConversionData::ContainerSizes(element),
      CSSToLengthConversionData::AnchorData(), 1., ignored_flags, element);

  using UnresolvedAlphaColor = StyleColor::UnresolvedAlphaColor;

  // Test that UnresolvedAlphaColor works correctly inside StyleColor equality.
  StyleColor alpha_style_1(MakeGarbageCollected<UnresolvedAlphaColor>(
      currentcolor, number_half, length_resolver));
  StyleColor alpha_style_2(MakeGarbageCollected<UnresolvedAlphaColor>(
      currentcolor, number_half, length_resolver));
  EXPECT_EQ(alpha_style_1, alpha_style_2);

  // Different from a non-alpha StyleColor.
  EXPECT_NE(alpha_style_1, red_rgb);
  EXPECT_NE(alpha_style_1, currentcolor);

  // Different from a color mix.
  StyleColor mix(MakeGarbageCollected<StyleColor::UnresolvedColorMix>(
      Color::ColorSpace::kSRGB, Color::HueInterpolationMethod::kShorter,
      currentcolor, red_rgb, 0.5, 1.0));
  EXPECT_NE(alpha_style_1, mix);
}

}  // namespace blink
