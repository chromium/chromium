// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_fast_paths.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"

namespace blink {

TEST(CSSParserFastPathsTest, ParseKeyword) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  CSSValue* value = CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kFloat,
                                                        "left", context);
  ASSERT_NE(nullptr, value);
  CSSIdentifierValue* identifier_value = To<CSSIdentifierValue>(value);
  EXPECT_EQ(CSSValueID::kLeft, identifier_value->GetValueID());
  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kFloat, "foo",
                                              context);
  ASSERT_EQ(nullptr, value);
}

TEST(CSSParserFastPathsTest, ParseCSSWideKeywords) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  CSSValue* value = CSSParserFastPaths::MaybeParseValue(
      CSSPropertyID::kMarginTop, "inherit", context);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsInheritedValue());
  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kMarginRight,
                                              "InHeriT", context);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsInheritedValue());
  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kMarginBottom,
                                              "initial", context);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsInitialValue());
  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kMarginLeft,
                                              "IniTiaL", context);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsInitialValue());
  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kMarginTop,
                                              "unset", context);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsUnsetValue());
  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kMarginLeft,
                                              "unsEt", context);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsUnsetValue());
  // Fast path doesn't handle short hands.
  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kMargin, "initial",
                                              context);
  ASSERT_EQ(nullptr, value);
}

TEST(CSSParserFastPathsTest, ParseRevert) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  // Revert enabled, IsHandledByKeywordFastPath=false
  {
    DCHECK(!CSSParserFastPaths::IsHandledByKeywordFastPath(
        CSSPropertyID::kMarginTop));
    CSSValue* value = CSSParserFastPaths::MaybeParseValue(
        CSSPropertyID::kMarginTop, "revert", context);
    ASSERT_TRUE(value);
    EXPECT_TRUE(value->IsRevertValue());
  }

  // Revert enabled, IsHandledByKeywordFastPath=true
  {
    DCHECK(CSSParserFastPaths::IsHandledByKeywordFastPath(
        CSSPropertyID::kDirection));
    CSSValue* value = CSSParserFastPaths::MaybeParseValue(
        CSSPropertyID::kDirection, "revert", context);
    ASSERT_TRUE(value);
    EXPECT_TRUE(value->IsRevertValue());
  }
}

TEST(CSSParserFastPathsTest, ParseRevertLayer) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  // 'revert-layer' enabled, IsHandledByKeywordFastPath=false
  {
    DCHECK(!CSSParserFastPaths::IsHandledByKeywordFastPath(
        CSSPropertyID::kMarginTop));
    CSSValue* value = CSSParserFastPaths::MaybeParseValue(
        CSSPropertyID::kMarginTop, "revert-layer", context);
    ASSERT_TRUE(value);
    EXPECT_TRUE(value->IsRevertLayerValue());
  }

  // 'revert-layer' enabled, IsHandledByKeywordFastPath=true
  {
    DCHECK(CSSParserFastPaths::IsHandledByKeywordFastPath(
        CSSPropertyID::kDirection));
    CSSValue* value = CSSParserFastPaths::MaybeParseValue(
        CSSPropertyID::kDirection, "revert-layer", context);
    ASSERT_TRUE(value);
    EXPECT_TRUE(value->IsRevertLayerValue());
  }
}

TEST(CSSParserFastPathsTest, ParseSimpleLength) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  CSSValue* value = CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kWidth,
                                                        "234px", context);
  ASSERT_NE(nullptr, value);
  EXPECT_FALSE(value->IsValueList());
  EXPECT_EQ("234px", value->CssText());

  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kWidth,
                                              "234.567px", context);
  ASSERT_NE(nullptr, value);
  EXPECT_FALSE(value->IsValueList());
  EXPECT_EQ("234.567px", value->CssText());

  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kWidth, ".567px",
                                              context);
  ASSERT_NE(nullptr, value);
  EXPECT_FALSE(value->IsValueList());
  EXPECT_EQ("0.567px", value->CssText());

  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kWidth, "234.px",
                                              context);
  EXPECT_EQ(nullptr, value);

  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kWidth, "234.e2px",
                                              context);
  EXPECT_EQ(nullptr, value);

  value =
      CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kWidth, ".", context);
  EXPECT_EQ(nullptr, value);

  // This is legal, but we don't support it in the fast path.
  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kWidth, "234e2px",
                                              context);
  EXPECT_EQ(nullptr, value);
}

// Mostly to stress-test the SIMD paths.
TEST(CSSParserFastPathsTest, VariousNumberOfDecimalsInLength) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  const std::pair<std::string, double> kTestCases[] = {
      {"0.1px", 0.1},
      {"0.12px", 0.12},
      {"0.123px", 0.123},
      {"0.1234px", 0.1234},
      {"0.12345px", 0.12345},
      {"0.123456px", 0.123456},
      {"0.1234567px", 0.1234567},
      {"0.12345678px", 0.1234567},   // NOTE: Max. seven digits.
      {"0.123456789px", 0.1234567},  // NOTE: Max. seven digits.
  };
  for (const auto& [str, expected_val] : kTestCases) {
    SCOPED_TRACE(str);
    CSSValue* value = CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kWidth,
                                                          str.c_str(), context);
    ASSERT_NE(nullptr, value);
    EXPECT_FALSE(value->IsValueList());
    EXPECT_DOUBLE_EQ(expected_val,
                     To<CSSNumericLiteralValue>(value)->DoubleValue());
  }
}

TEST(CSSParserFastPathsTest, ParseTransform) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  CSSValue* value = CSSParserFastPaths::MaybeParseValue(
      CSSPropertyID::kTransform, "translate(5.5px, 5px)", context);
  ASSERT_NE(nullptr, value);
  ASSERT_TRUE(value->IsValueList());
  ASSERT_EQ("translate(5.5px, 5px)", value->CssText());

  value = CSSParserFastPaths::MaybeParseValue(
      CSSPropertyID::kTransform, "translate3d(5px, 5px, 10.1px)", context);
  ASSERT_NE(nullptr, value);
  ASSERT_TRUE(value->IsValueList());
  ASSERT_EQ("translate3d(5px, 5px, 10.1px)", value->CssText());
}

TEST(CSSParserFastPathsTest, ParseComplexTransform) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  // Random whitespace is on purpose.
  static const char* kComplexTransform =
      "translateX(5px) "
      "translateZ(20.5px)   "
      "translateY(10px) "
      "scale3d(0.5, 1, 0.7)   "
      "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)   ";
  static const char* kComplexTransformNormalized =
      "translateX(5px) "
      "translateZ(20.5px) "
      "translateY(10px) "
      "scale3d(0.5, 1, 0.7) "
      "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)";
  CSSValue* value = CSSParserFastPaths::MaybeParseValue(
      CSSPropertyID::kTransform, kComplexTransform, context);
  ASSERT_NE(nullptr, value);
  ASSERT_TRUE(value->IsValueList());
  ASSERT_EQ(kComplexTransformNormalized, value->CssText());
}

TEST(CSSParserFastPathsTest, ParseTransformNotFastPath) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  CSSValue* value = CSSParserFastPaths::MaybeParseValue(
      CSSPropertyID::kTransform, "rotateX(1deg)", context);
  ASSERT_EQ(nullptr, value);
  value = CSSParserFastPaths::MaybeParseValue(
      CSSPropertyID::kTransform, "translateZ(1px) rotateX(1deg)", context);
  ASSERT_EQ(nullptr, value);
}

TEST(CSSParserFastPathsTest, ParseInvalidTransform) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  CSSValue* value = CSSParserFastPaths::MaybeParseValue(
      CSSPropertyID::kTransform, "rotateX(1deg", context);
  ASSERT_EQ(nullptr, value);
  value = CSSParserFastPaths::MaybeParseValue(
      CSSPropertyID::kTransform, "translateZ(1px) (1px, 1px) rotateX(1deg",
      context);
  ASSERT_EQ(nullptr, value);
}

TEST(CSSParserFastPathsTest, ParseColorWithLargeAlpha) {
  Color color;
  EXPECT_EQ(ParseColorResult::kColor,
            CSSParserFastPaths::ParseColor("rgba(0,0,0,1893205797.13)",
                                           kHTMLStandardMode, color));
  EXPECT_EQ(Color::kBlack, color);
}

TEST(CSSParserFastPathsTest, ParseColorWithNewSyntax) {
  Color color;
  EXPECT_EQ(
      ParseColorResult::kColor,
      CSSParserFastPaths::ParseColor("rgba(0 0 0)", kHTMLStandardMode, color));
  EXPECT_EQ(Color::kBlack, color);

  EXPECT_EQ(ParseColorResult::kColor,
            CSSParserFastPaths::ParseColor("rgba(0 0 0 / 1)", kHTMLStandardMode,
                                           color));
  EXPECT_EQ(Color::kBlack, color);

  EXPECT_EQ(ParseColorResult::kColor,
            CSSParserFastPaths::ParseColor("rgba(0, 0, 0, 1)",
                                           kHTMLStandardMode, color));
  EXPECT_EQ(Color::kBlack, color);

  EXPECT_EQ(ParseColorResult::kFailure,
            CSSParserFastPaths::ParseColor("rgba(0 0 0 0)", kHTMLStandardMode,
                                           color));

  EXPECT_EQ(ParseColorResult::kFailure,
            CSSParserFastPaths::ParseColor("rgba(0, 0 0 1)", kHTMLStandardMode,
                                           color));

  EXPECT_EQ(ParseColorResult::kFailure,
            CSSParserFastPaths::ParseColor("rgba(0, 0, 0 / 1)",
                                           kHTMLStandardMode, color));

  EXPECT_EQ(ParseColorResult::kFailure,
            CSSParserFastPaths::ParseColor("rgba(0 0 0, 1)", kHTMLStandardMode,
                                           color));
}

TEST(CSSParserFastPathsTest, ParseColorWithDecimal) {
  Color color;
  EXPECT_EQ(ParseColorResult::kColor,
            CSSParserFastPaths::ParseColor("rgba(0.0, 0.0, 0.0, 1.0)",
                                           kHTMLStandardMode, color));
  EXPECT_EQ(Color::kBlack, color);

  EXPECT_EQ(ParseColorResult::kColor,
            CSSParserFastPaths::ParseColor("rgb(0.0, 0.0, 0.0)",
                                           kHTMLStandardMode, color));
  EXPECT_EQ(Color::kBlack, color);

  EXPECT_EQ(ParseColorResult::kColor,
            CSSParserFastPaths::ParseColor("rgb(0.0 , 0.0,0.0)",
                                           kHTMLStandardMode, color));
  EXPECT_EQ(Color::kBlack, color);

  EXPECT_EQ(ParseColorResult::kColor,
            CSSParserFastPaths::ParseColor("rgb(254.5, 254.5, 254.5)",
                                           kHTMLStandardMode, color));
  EXPECT_EQ(Color::kWhite, color);
}

TEST(CSSParserFastPathsTest, ParseHSL) {
  Color color;
  EXPECT_EQ(ParseColorResult::kColor,
            CSSParserFastPaths::ParseColor("hsl(90deg, 50%, 25%)",
                                           kHTMLStandardMode, color));
  EXPECT_EQ("rgb(64, 96, 32)", color.SerializeAsCSSColor());

  // Implicit “deg” angle.
  EXPECT_EQ(ParseColorResult::kColor,
            CSSParserFastPaths::ParseColor("hsl(180, 50%, 50%)",
                                           kHTMLStandardMode, color));
  EXPECT_EQ("rgb(64, 191, 191)", color.SerializeAsCSSColor());

  // turn.
  EXPECT_EQ(ParseColorResult::kColor,
            CSSParserFastPaths::ParseColor("hsl(0.25turn, 25%, 50%)",
                                           kHTMLStandardMode, color));
  EXPECT_EQ("rgb(128, 159, 96)", color.SerializeAsCSSColor());

  // rad.
  EXPECT_EQ(ParseColorResult::kColor,
            CSSParserFastPaths::ParseColor("hsl(1.0rad, 50%, 50%)",
                                           kHTMLStandardMode, color));
  EXPECT_EQ("rgb(191, 186, 64)", color.SerializeAsCSSColor());

  // Wraparound.
  EXPECT_EQ(ParseColorResult::kColor,
            CSSParserFastPaths::ParseColor("hsl(450deg, 50%, 50%)",
                                           kHTMLStandardMode, color));
  EXPECT_EQ("rgb(128, 191, 64)", color.SerializeAsCSSColor());

  // Lots of wraparound.
  EXPECT_EQ(ParseColorResult::kColor,
            CSSParserFastPaths::ParseColor("hsl(4050deg, 50%, 50%)",
                                           kHTMLStandardMode, color));
  EXPECT_EQ("rgb(128, 191, 64)", color.SerializeAsCSSColor());

  // Negative wraparound.
  EXPECT_EQ(ParseColorResult::kColor,
            CSSParserFastPaths::ParseColor("hsl(-270deg, 50%, 50%)",
                                           kHTMLStandardMode, color));
  EXPECT_EQ("rgb(128, 191, 64)", color.SerializeAsCSSColor());

  // Saturation clamping.
  EXPECT_EQ(ParseColorResult::kColor,
            CSSParserFastPaths::ParseColor("hsl(45deg, 150%, 50%)",
                                           kHTMLStandardMode, color));
  EXPECT_EQ("rgb(255, 191, 0)", color.SerializeAsCSSColor());

  // Lightness clamping to negative.
  EXPECT_EQ(ParseColorResult::kColor,
            CSSParserFastPaths::ParseColor("hsl(45deg, 150%, -1000%)",
                                           kHTMLStandardMode, color));
  EXPECT_EQ("rgb(0, 0, 0)", color.SerializeAsCSSColor());

  // Writing hsla() without alpha.
  EXPECT_EQ(ParseColorResult::kColor,
            CSSParserFastPaths::ParseColor("hsla(45deg, 150%, 50%)",
                                           kHTMLStandardMode, color));
  EXPECT_EQ("rgb(255, 191, 0)", color.SerializeAsCSSColor());

  // Stray period at the end
  EXPECT_NE(ParseColorResult::kColor,
            CSSParserFastPaths::ParseColor("hsl(0.turn, 25%, 50%)",
                                           kHTMLStandardMode, color));
}

TEST(CSSParserFastPathsTest, ParseHSLWithAlpha) {
  // With alpha, using hsl().
  Color color;
  EXPECT_EQ(ParseColorResult::kColor,
            CSSParserFastPaths::ParseColor("hsl(30 , 1%,75%, 0.5)",
                                           kHTMLStandardMode, color));
  EXPECT_EQ("rgba(192, 191, 191, 0.5)", color.SerializeAsCSSColor());

  // With alpha, using hsla().
  EXPECT_EQ(ParseColorResult::kColor,
            CSSParserFastPaths::ParseColor("hsla(30 , 1%,75%, 0.5)",
                                           kHTMLStandardMode, color));
  EXPECT_EQ("rgba(192, 191, 191, 0.5)", color.SerializeAsCSSColor());

  // With alpha, using space-separated syntax.
  EXPECT_EQ(ParseColorResult::kColor,
            CSSParserFastPaths::ParseColor("hsla(30 1% 75% / 0.1)",
                                           kHTMLStandardMode, color));
  EXPECT_EQ("rgba(192, 191, 191, 0.1)", color.SerializeAsCSSColor());

  // Clamp alpha.
  EXPECT_EQ(ParseColorResult::kColor,
            CSSParserFastPaths::ParseColor("hsla(30 1% 75% / 1.2)",
                                           kHTMLStandardMode, color));
  EXPECT_EQ("rgb(192, 191, 191)", color.SerializeAsCSSColor());
}

TEST(CSSParserFastPathsTest, ParseHSLInvalid) {
  // Invalid unit.
  Color color;
  EXPECT_EQ(ParseColorResult::kFailure,
            CSSParserFastPaths::ParseColor("hsl(20dag, 50%, 20%)",
                                           kHTMLStandardMode, color));

  // Mix of new and old space syntax.
  EXPECT_EQ(ParseColorResult::kFailure,
            CSSParserFastPaths::ParseColor("hsl(0.2, 50%, 20% 0.3)",
                                           kHTMLStandardMode, color));
  EXPECT_EQ(ParseColorResult::kFailure,
            CSSParserFastPaths::ParseColor("hsl(0.2, 50%, 20% / 0.3)",
                                           kHTMLStandardMode, color));
  EXPECT_EQ(ParseColorResult::kFailure,
            CSSParserFastPaths::ParseColor("hsl(0.2 50% 20%, 0.3)",
                                           kHTMLStandardMode, color));

  // Junk after percentage.
  EXPECT_EQ(ParseColorResult::kFailure,
            CSSParserFastPaths::ParseColor("hsl(0.2, 50% foo, 20% 0.3)",
                                           kHTMLStandardMode, color));

  // Stopping right before an expected %.
  EXPECT_EQ(
      ParseColorResult::kFailure,
      CSSParserFastPaths::ParseColor("hsl(9,0.6", kHTMLStandardMode, color));
}

TEST(CSSParserFastPathsTest, IsValidKeywordPropertyAndValueOverflowClip) {
  EXPECT_TRUE(CSSParserFastPaths::IsValidKeywordPropertyAndValue(
      CSSPropertyID::kOverflowX, CSSValueID::kClip,
      CSSParserMode::kHTMLStandardMode));
}

TEST(CSSParserFastPathsTest, InternalColorsOnlyAllowedInUaMode) {
  Color color;

  EXPECT_EQ(ParseColorResult::kKeyword,
            CSSParserFastPaths::ParseColor("blue", kHTMLStandardMode, color));
  EXPECT_EQ(ParseColorResult::kKeyword,
            CSSParserFastPaths::ParseColor("blue", kHTMLQuirksMode, color));
  EXPECT_EQ(ParseColorResult::kKeyword,
            CSSParserFastPaths::ParseColor("blue", kUASheetMode, color));

  EXPECT_EQ(ParseColorResult::kFailure,
            CSSParserFastPaths::ParseColor("-internal-spelling-error-color",
                                           kHTMLStandardMode, color));
  EXPECT_EQ(ParseColorResult::kFailure,
            CSSParserFastPaths::ParseColor("-internal-spelling-error-color",
                                           kHTMLQuirksMode, color));
  EXPECT_EQ(ParseColorResult::kKeyword,
            CSSParserFastPaths::ParseColor("-internal-spelling-error-color",
                                           kUASheetMode, color));

  EXPECT_EQ(ParseColorResult::kFailure,
            CSSParserFastPaths::ParseColor("-internal-grammar-error-color",
                                           kHTMLStandardMode, color));
  EXPECT_EQ(ParseColorResult::kFailure,
            CSSParserFastPaths::ParseColor("-internal-grammar-error-color",
                                           kHTMLQuirksMode, color));
  EXPECT_EQ(ParseColorResult::kKeyword,
            CSSParserFastPaths::ParseColor("-internal-grammar-error-color",
                                           kUASheetMode, color));

  EXPECT_EQ(ParseColorResult::kFailure,
            CSSParserFastPaths::ParseColor("-internal-search-color",
                                           kHTMLStandardMode, color));
  EXPECT_EQ(ParseColorResult::kFailure,
            CSSParserFastPaths::ParseColor("-internal-search-color",
                                           kHTMLQuirksMode, color));
  EXPECT_EQ(ParseColorResult::kKeyword,
            CSSParserFastPaths::ParseColor("-internal-search-color",
                                           kUASheetMode, color));

  EXPECT_EQ(ParseColorResult::kFailure,
            CSSParserFastPaths::ParseColor("-internal-search-text-color",
                                           kHTMLStandardMode, color));
  EXPECT_EQ(ParseColorResult::kFailure,
            CSSParserFastPaths::ParseColor("-internal-search-text-color",
                                           kHTMLQuirksMode, color));
  EXPECT_EQ(ParseColorResult::kKeyword,
            CSSParserFastPaths::ParseColor("-internal-search-text-color",
                                           kUASheetMode, color));

  EXPECT_EQ(ParseColorResult::kFailure,
            CSSParserFastPaths::ParseColor("-internal-current-search-color",
                                           kHTMLStandardMode, color));
  EXPECT_EQ(ParseColorResult::kFailure,
            CSSParserFastPaths::ParseColor("-internal-current-search-color",
                                           kHTMLQuirksMode, color));
  EXPECT_EQ(ParseColorResult::kKeyword,
            CSSParserFastPaths::ParseColor("-internal-current-search-color",
                                           kUASheetMode, color));

  EXPECT_EQ(
      ParseColorResult::kFailure,
      CSSParserFastPaths::ParseColor("-internal-current-search-text-color",
                                     kHTMLStandardMode, color));
  EXPECT_EQ(ParseColorResult::kFailure,
            CSSParserFastPaths::ParseColor(
                "-internal-current-search-text-color", kHTMLQuirksMode, color));
  EXPECT_EQ(ParseColorResult::kKeyword,
            CSSParserFastPaths::ParseColor(
                "-internal-current-search-text-color", kUASheetMode, color));
}

}  // namespace blink
