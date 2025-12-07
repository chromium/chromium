// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_color_function_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_color_mix_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_relative_color_value.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"

namespace blink {

TEST(ColorFunctionParserTest, RelativeColorWithKeywordBase) {
  const String test_case = "rgb(from red r g b)";
  CSSParserTokenStream stream(test_case);

  const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);

  ColorFunctionParser parser;
  const CSSValue* result = parser.ConsumeFunctionalSyntaxColor(
      stream, *context, css_parsing_utils::ColorParserContext());
  EXPECT_TRUE(result->IsRelativeColorValue());
  const cssvalue::CSSRelativeColorValue* color =
      To<cssvalue::CSSRelativeColorValue>(result);

  const CSSValue& origin = color->OriginColor();
  EXPECT_TRUE(origin.IsIdentifierValue());
  EXPECT_EQ(To<CSSIdentifierValue>(origin).GetValueID(), CSSValueID::kRed);

  EXPECT_EQ(color->ColorInterpolationSpace(), Color::ColorSpace::kSRGBLegacy);

  const CSSValue& channel0 = color->Channel0();
  EXPECT_TRUE(channel0.IsIdentifierValue());
  EXPECT_EQ(To<CSSIdentifierValue>(channel0).GetValueID(), CSSValueID::kR);

  const CSSValue& channel1 = color->Channel1();
  EXPECT_TRUE(channel1.IsIdentifierValue());
  EXPECT_EQ(To<CSSIdentifierValue>(channel1).GetValueID(), CSSValueID::kG);

  const CSSValue& channel2 = color->Channel2();
  EXPECT_TRUE(channel2.IsIdentifierValue());
  EXPECT_EQ(To<CSSIdentifierValue>(channel2).GetValueID(), CSSValueID::kB);

  EXPECT_EQ(color->Alpha(), nullptr);
}

TEST(ColorFunctionParserTest, RelativeColorWithInvalidChannelReference) {
  const String test_case = "rgb(from red h s l)";
  CSSParserTokenStream stream(test_case);

  const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);

  ColorFunctionParser parser;
  const CSSValue* result = parser.ConsumeFunctionalSyntaxColor(
      stream, *context, css_parsing_utils::ColorParserContext());
  EXPECT_EQ(result, nullptr);
}

TEST(ColorFunctionParserTest, RelativeColorWithCurrentcolorBase_NoAlpha) {
  const String test_case = "rgb(from currentcolor 1 calc(g) b)";
  CSSParserTokenStream stream(test_case);

  const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);

  ColorFunctionParser parser;
  const CSSValue* result = parser.ConsumeFunctionalSyntaxColor(
      stream, *context, css_parsing_utils::ColorParserContext());
  EXPECT_TRUE(result->IsRelativeColorValue());
  const cssvalue::CSSRelativeColorValue* color =
      To<cssvalue::CSSRelativeColorValue>(result);

  const CSSValue& origin = color->OriginColor();
  EXPECT_TRUE(origin.IsIdentifierValue());
  EXPECT_EQ(To<CSSIdentifierValue>(origin).GetValueID(),
            CSSValueID::kCurrentcolor);

  EXPECT_EQ(color->ColorInterpolationSpace(), Color::ColorSpace::kSRGBLegacy);

  const CSSValue& channel0 = color->Channel0();
  EXPECT_TRUE(channel0.IsNumericLiteralValue());
  EXPECT_EQ(To<CSSNumericLiteralValue>(channel0).DoubleValue(), 1.0f);

  const CSSValue& channel1 = color->Channel1();
  EXPECT_TRUE(channel1.IsMathFunctionValue());
  EXPECT_EQ(channel1.CssText(), "calc(g)");

  const CSSValue& channel2 = color->Channel2();
  EXPECT_TRUE(channel2.IsIdentifierValue());
  EXPECT_EQ(To<CSSIdentifierValue>(channel2).GetValueID(), CSSValueID::kB);

  EXPECT_EQ(color->Alpha(), nullptr);
}

TEST(ColorFunctionParserTest, RelativeColorWithCurrentcolorBase_CalcAlpha) {
  const String test_case =
      "rgb(from currentcolor 1 calc(g) b / calc(alpha / 2))";
  CSSParserTokenStream stream(test_case);

  const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);

  ColorFunctionParser parser;
  const CSSValue* result = parser.ConsumeFunctionalSyntaxColor(
      stream, *context, css_parsing_utils::ColorParserContext());
  EXPECT_TRUE(result->IsRelativeColorValue());
  const cssvalue::CSSRelativeColorValue* color =
      To<cssvalue::CSSRelativeColorValue>(result);

  const CSSValue& origin = color->OriginColor();
  EXPECT_TRUE(origin.IsIdentifierValue());
  EXPECT_EQ(To<CSSIdentifierValue>(origin).GetValueID(),
            CSSValueID::kCurrentcolor);

  EXPECT_EQ(color->ColorInterpolationSpace(), Color::ColorSpace::kSRGBLegacy);

  const CSSValue& channel0 = color->Channel0();
  EXPECT_TRUE(channel0.IsNumericLiteralValue());
  EXPECT_EQ(To<CSSNumericLiteralValue>(channel0).DoubleValue(), 1.0f);

  const CSSValue& channel1 = color->Channel1();
  EXPECT_TRUE(channel1.IsMathFunctionValue());
  EXPECT_EQ(channel1.CssText(), "calc(g)");

  const CSSValue& channel2 = color->Channel2();
  EXPECT_TRUE(channel2.IsIdentifierValue());
  EXPECT_EQ(To<CSSIdentifierValue>(channel2).GetValueID(), CSSValueID::kB);

  const CSSValue* alpha = color->Alpha();
  EXPECT_TRUE(alpha->IsMathFunctionValue());
  EXPECT_EQ(alpha->CssText(), "calc(0.5 * alpha)");
}

TEST(ColorFunctionParserTest, RelativeColorWithCurrentcolorBase_NoneKeyword) {
  const String test_case = "rgb(from currentcolor none none none / none)";
  CSSParserTokenStream stream(test_case);

  const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);

  ColorFunctionParser parser;
  const CSSValue* result = parser.ConsumeFunctionalSyntaxColor(
      stream, *context, css_parsing_utils::ColorParserContext());
  EXPECT_TRUE(result->IsRelativeColorValue());
  const cssvalue::CSSRelativeColorValue* color =
      To<cssvalue::CSSRelativeColorValue>(result);

  const CSSValue& origin = color->OriginColor();
  EXPECT_TRUE(origin.IsIdentifierValue());
  EXPECT_EQ(To<CSSIdentifierValue>(origin).GetValueID(),
            CSSValueID::kCurrentcolor);

  EXPECT_EQ(color->ColorInterpolationSpace(), Color::ColorSpace::kSRGBLegacy);

  const CSSValue& channel0 = color->Channel0();
  EXPECT_TRUE(channel0.IsIdentifierValue());
  EXPECT_EQ(To<CSSIdentifierValue>(channel0).GetValueID(), CSSValueID::kNone);

  const CSSValue& channel1 = color->Channel1();
  EXPECT_TRUE(channel1.IsIdentifierValue());
  EXPECT_EQ(To<CSSIdentifierValue>(channel1).GetValueID(), CSSValueID::kNone);

  const CSSValue& channel2 = color->Channel2();
  EXPECT_TRUE(channel2.IsIdentifierValue());
  EXPECT_EQ(To<CSSIdentifierValue>(channel2).GetValueID(), CSSValueID::kNone);

  const CSSValue* alpha = color->Alpha();
  EXPECT_TRUE(alpha->IsIdentifierValue());
  EXPECT_EQ(To<CSSIdentifierValue>(alpha)->GetValueID(), CSSValueID::kNone);
}

TEST(ColorFunctionParserTest, RelativeColorWithColorMixWithCurrentColorBase) {
  const String test_case =
      "rgb(from color-mix(in srgb, currentColor 50%, green) r g b)";
  CSSParserTokenStream stream(test_case);

  const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);

  ColorFunctionParser parser;
  const CSSValue* result = parser.ConsumeFunctionalSyntaxColor(
      stream, *context, css_parsing_utils::ColorParserContext());
  EXPECT_TRUE(result->IsRelativeColorValue());
  const cssvalue::CSSRelativeColorValue* color =
      To<cssvalue::CSSRelativeColorValue>(result);

  const CSSValue& origin = color->OriginColor();
  EXPECT_TRUE(origin.IsColorMixValue());
  const cssvalue::CSSColorMixValue& origin_color =
      To<cssvalue::CSSColorMixValue>(origin);
  EXPECT_TRUE(origin_color.Color1().IsIdentifierValue());
  EXPECT_EQ(To<CSSIdentifierValue>(origin_color.Color1()).GetValueID(),
            CSSValueID::kCurrentcolor);
  EXPECT_TRUE(origin_color.Color2().IsIdentifierValue());
  EXPECT_EQ(To<CSSIdentifierValue>(origin_color.Color2()).GetValueID(),
            CSSValueID::kGreen);
}

namespace {

void TestColorParsing(const char* input, const char* expected) {
  static const CSSParserContext* context =
      MakeGarbageCollected<CSSParserContext>(
          kHTMLStandardMode, SecureContextMode::kInsecureContext);

  CSSParserTokenStream stream(input);
  ColorFunctionParser parser;
  const CSSValue* result = parser.ConsumeFunctionalSyntaxColor(
      stream, *context, css_parsing_utils::ColorParserContext());
  EXPECT_EQ(result->CssText(), expected);
}

}  // namespace

// Tests HSL negative saturation/lightness clamping and RGB serialization
TEST(ColorFunctionParserTest, HSLNegativeValueClamping) {
  TestColorParsing("hsl(120 -50% 50%)", "rgb(128, 128, 128)");
  TestColorParsing("hsl(120 -50% 50% / 0.5)", "rgba(128, 128, 128, 0.5)");
  TestColorParsing("hsl(120 50% -20%)", "rgb(0, 0, 0)");
}

// Tests HWB negative whiteness/blackness clamping and RGB serialization
TEST(ColorFunctionParserTest, HWBNegativeValueClamping) {
  TestColorParsing("hwb(120 -30% 50%)", "rgb(0, 128, 0)");
  TestColorParsing("hwb(120 30% -50%)", "rgb(77, 255, 77)");
  TestColorParsing("hwb(120 -30% -50% / 0.5)", "rgba(0, 255, 0, 0.5)");
}

// Tests Lab percentage conversions (L=100, a/b=125) and lightness clamping
TEST(ColorFunctionParserTest, LabPercentageConversion) {
  TestColorParsing("lab(50% 100% -100%)", "lab(50 125 -125)");
  TestColorParsing("lab(50% 50% -50%)", "lab(50 62.5 -62.5)");
  TestColorParsing("lab(150 50 50)", "lab(100 50 50)");
  TestColorParsing("lab(-50 50 50)", "lab(0 50 50)");
  TestColorParsing("lab(150% 0 0)", "lab(100 0 0)");
  TestColorParsing("lab(50% 100% -100% / 0.5)", "lab(50 125 -125 / 0.5)");
}

// Tests OkLab percentage conversions (L=1, a/b=0.4) and lightness clamping
TEST(ColorFunctionParserTest, OklabPercentageConversion) {
  TestColorParsing("oklab(100% 100% -100%)", "oklab(1 0.4 -0.4)");
  TestColorParsing("oklab(50% 50% -50%)", "oklab(0.5 0.2 -0.2)");
  TestColorParsing("oklab(1.5 0.2 0.2)", "oklab(1 0.2 0.2)");
  TestColorParsing("oklab(-0.5 0.2 0.2)", "oklab(0 0.2 0.2)");
  TestColorParsing("oklab(150% 0 0)", "oklab(1 0 0)");
  TestColorParsing("oklab(100% 100% -100% / 0.5)", "oklab(1 0.4 -0.4 / 0.5)");
}

// Tests Lch percentage conversions (L=100, C=150) and clamping
TEST(ColorFunctionParserTest, LchPercentageConversion) {
  TestColorParsing("lch(50% 100% 180)", "lch(50 150 180)");
  TestColorParsing("lch(50% 50% 90)", "lch(50 75 90)");
  TestColorParsing("lch(150 100 90)", "lch(100 100 90)");
  TestColorParsing("lch(-50 100 90)", "lch(0 100 90)");
  TestColorParsing("lch(50 -100 90)", "lch(50 0 90)");
  TestColorParsing("lch(150% 0 0)", "lch(100 0 0)");
  TestColorParsing("lch(50% 100% 180 / 0.5)", "lch(50 150 180 / 0.5)");
}

// Tests OkLch percentage conversions (L=1, C=0.4) and clamping
TEST(ColorFunctionParserTest, OklchPercentageConversion) {
  TestColorParsing("oklch(100% 100% 180)", "oklch(1 0.4 180)");
  TestColorParsing("oklch(50% 50% 90)", "oklch(0.5 0.2 90)");
  TestColorParsing("oklch(1.5 0.3 90)", "oklch(1 0.3 90)");
  TestColorParsing("oklch(-0.5 0.3 90)", "oklch(0 0.3 90)");
  TestColorParsing("oklch(0.5 -0.3 90)", "oklch(0.5 0 90)");
  TestColorParsing("oklch(150% 0 0)", "oklch(1 0 0)");
  TestColorParsing("oklch(100% 100% 180 / 0.5)", "oklch(1 0.4 180 / 0.5)");
}

// Tests RGB values clamped to [0, 255] range
TEST(ColorFunctionParserTest, RGBClamping) {
  TestColorParsing("rgb(300 128 -50)", "rgb(255, 128, 0)");
  TestColorParsing("rgb(300 128 -50 / 0.5)", "rgba(255, 128, 0, 0.5)");
  TestColorParsing("rgb(150% 50% -10%)", "rgb(255, 128, 0)");
  TestColorParsing("rgb(120% 50% -10% / 0.5)", "rgba(255, 128, 0, 0.5)");
}

// Tests calc() expressions preserved in Lab/OkLab/Lch/OkLch color spaces
TEST(ColorFunctionParserTest, CalcPreservationInLabColorSpaces) {
  TestColorParsing("lab(calc(50) calc(25) calc(-25))",
                   "lab(calc(50) calc(25) calc(-25))");
  TestColorParsing("oklab(calc(0.5) calc(0.2) calc(-0.2))",
                   "oklab(calc(0.5) calc(0.2) calc(-0.2))");
  TestColorParsing("lch(calc(50) calc(75) calc(180))",
                   "lch(calc(50) calc(75) calc(180))");
  TestColorParsing("oklch(calc(0.5) calc(0.2) calc(180))",
                   "oklch(calc(0.5) calc(0.2) calc(180))");
  TestColorParsing("lab(calc(50) 25 -25 / calc(0.5))",
                   "lab(calc(50) 25 -25 / calc(0.5))");
  TestColorParsing("lab(50 calc(25) -25)", "lab(50 calc(25) -25)");
}

}  // namespace blink
