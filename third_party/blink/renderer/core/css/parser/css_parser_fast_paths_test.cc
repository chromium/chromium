// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_fast_paths.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

TEST(CSSParserFastPathsTest, ParseKeyword) {
  CSSValue* value = CSSParserFastPaths::MaybeParseValue(
      CSSPropertyID::kFloat, "left", kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  CSSIdentifierValue* identifier_value = To<CSSIdentifierValue>(value);
  EXPECT_EQ(CSSValueID::kLeft, identifier_value->GetValueID());
  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kFloat, "foo",
                                              kHTMLStandardMode);
  ASSERT_EQ(nullptr, value);
}

TEST(CSSParserFastPathsTest, ParseCSSWideKeywords) {
  CSSValue* value = CSSParserFastPaths::MaybeParseValue(
      CSSPropertyID::kMarginTop, "inherit", kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsInheritedValue());
  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kMarginRight,
                                              "InHeriT", kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsInheritedValue());
  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kMarginBottom,
                                              "initial", kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsInitialValue());
  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kMarginLeft,
                                              "IniTiaL", kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsInitialValue());
  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kMarginTop,
                                              "unset", kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsUnsetValue());
  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kMarginLeft,
                                              "unsEt", kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsUnsetValue());
  // Fast path doesn't handle short hands.
  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kMargin, "initial",
                                              kHTMLStandardMode);
  ASSERT_EQ(nullptr, value);
}

TEST(CSSParserFastPathsTest, ParseRevert) {
  // Revert enabled, IsHandledByKeywordFastPath=false
  {
    DCHECK(!CSSParserFastPaths::IsHandledByKeywordFastPath(
        CSSPropertyID::kMarginTop));
    CSSValue* value = CSSParserFastPaths::MaybeParseValue(
        CSSPropertyID::kMarginTop, "revert", kHTMLStandardMode);
    ASSERT_TRUE(value);
    EXPECT_TRUE(value->IsRevertValue());
  }

  // Revert enabled, IsHandledByKeywordFastPath=true
  {
    DCHECK(CSSParserFastPaths::IsHandledByKeywordFastPath(
        CSSPropertyID::kDirection));
    CSSValue* value = CSSParserFastPaths::MaybeParseValue(
        CSSPropertyID::kDirection, "revert", kHTMLStandardMode);
    ASSERT_TRUE(value);
    EXPECT_TRUE(value->IsRevertValue());
  }
}

TEST(CSSParserFastPathsTest, ParseRevertLayer) {
  // 'revert-layer' enabled, IsHandledByKeywordFastPath=false
  {
    DCHECK(!CSSParserFastPaths::IsHandledByKeywordFastPath(
        CSSPropertyID::kMarginTop));
    CSSValue* value = CSSParserFastPaths::MaybeParseValue(
        CSSPropertyID::kMarginTop, "revert-layer", kHTMLStandardMode);
    ASSERT_TRUE(value);
    EXPECT_TRUE(value->IsRevertLayerValue());
  }

  // 'revert-layer' enabled, IsHandledByKeywordFastPath=true
  {
    DCHECK(CSSParserFastPaths::IsHandledByKeywordFastPath(
        CSSPropertyID::kDirection));
    CSSValue* value = CSSParserFastPaths::MaybeParseValue(
        CSSPropertyID::kDirection, "revert-layer", kHTMLStandardMode);
    ASSERT_TRUE(value);
    EXPECT_TRUE(value->IsRevertLayerValue());
  }
}

TEST(CSSParserFastPathsTest, ParseTransform) {
  CSSValue* value = CSSParserFastPaths::MaybeParseValue(
      CSSPropertyID::kTransform, "translate(5.5px, 5px)", kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  ASSERT_TRUE(value->IsValueList());
  ASSERT_EQ("translate(5.5px, 5px)", value->CssText());

  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kTransform,
                                              "translate3d(5px, 5px, 10.1px)",
                                              kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  ASSERT_TRUE(value->IsValueList());
  ASSERT_EQ("translate3d(5px, 5px, 10.1px)", value->CssText());
}

TEST(CSSParserFastPathsTest, ParseComplexTransform) {
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
      CSSPropertyID::kTransform, kComplexTransform, kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  ASSERT_TRUE(value->IsValueList());
  ASSERT_EQ(kComplexTransformNormalized, value->CssText());
}

TEST(CSSParserFastPathsTest, ParseTransformNotFastPath) {
  CSSValue* value = CSSParserFastPaths::MaybeParseValue(
      CSSPropertyID::kTransform, "rotateX(1deg)", kHTMLStandardMode);
  ASSERT_EQ(nullptr, value);
  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyID::kTransform,
                                              "translateZ(1px) rotateX(1deg)",
                                              kHTMLStandardMode);
  ASSERT_EQ(nullptr, value);
}

TEST(CSSParserFastPathsTest, ParseInvalidTransform) {
  CSSValue* value = CSSParserFastPaths::MaybeParseValue(
      CSSPropertyID::kTransform, "rotateX(1deg", kHTMLStandardMode);
  ASSERT_EQ(nullptr, value);
  value = CSSParserFastPaths::MaybeParseValue(
      CSSPropertyID::kTransform, "translateZ(1px) (1px, 1px) rotateX(1deg",
      kHTMLStandardMode);
  ASSERT_EQ(nullptr, value);
}

TEST(CSSParserFastPathsTest, ParseColorWithLargeAlpha) {
  CSSValue* value = CSSParserFastPaths::ParseColor("rgba(0,0,0,1893205797.13)",
                                                   kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsColorValue());
  EXPECT_EQ(Color::kBlack, To<cssvalue::CSSColor>(*value).Value());
}

TEST(CSSParserFastPathsTest, ParseColorWithNewSyntax) {
  CSSValue* value =
      CSSParserFastPaths::ParseColor("rgba(0 0 0)", kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsColorValue());
  EXPECT_EQ(Color::kBlack, To<cssvalue::CSSColor>(*value).Value());

  value = CSSParserFastPaths::ParseColor("rgba(0 0 0 / 1)", kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsColorValue());
  EXPECT_EQ(Color::kBlack, To<cssvalue::CSSColor>(*value).Value());

  value = CSSParserFastPaths::ParseColor("rgba(0, 0, 0, 1)", kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsColorValue());
  EXPECT_EQ(Color::kBlack, To<cssvalue::CSSColor>(*value).Value());

  value = CSSParserFastPaths::ParseColor("rgba(0 0 0 0)", kHTMLStandardMode);
  EXPECT_EQ(nullptr, value);

  value = CSSParserFastPaths::ParseColor("rgba(0, 0 0 1)", kHTMLStandardMode);
  EXPECT_EQ(nullptr, value);

  value =
      CSSParserFastPaths::ParseColor("rgba(0, 0, 0 / 1)", kHTMLStandardMode);
  EXPECT_EQ(nullptr, value);

  value = CSSParserFastPaths::ParseColor("rgba(0 0 0, 1)", kHTMLStandardMode);
  EXPECT_EQ(nullptr, value);
}

TEST(CSSParserFastPathsTest, ParseColorWithDecimal) {
  CSSValue* value = CSSParserFastPaths::ParseColor("rgba(0.0, 0.0, 0.0, 1.0)",
                                                   kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsColorValue());
  EXPECT_EQ(Color::kBlack, To<cssvalue::CSSColor>(*value).Value());

  value =
      CSSParserFastPaths::ParseColor("rgb(0.0, 0.0, 0.0)", kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsColorValue());
  EXPECT_EQ(Color::kBlack, To<cssvalue::CSSColor>(*value).Value());

  value =
      CSSParserFastPaths::ParseColor("rgb(0.0 , 0.0,0.0)", kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsColorValue());
  EXPECT_EQ(Color::kBlack, To<cssvalue::CSSColor>(*value).Value());

  value = CSSParserFastPaths::ParseColor("rgb(254.5, 254.5, 254.5)",
                                         kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsColorValue());
  EXPECT_EQ(Color::kWhite, To<cssvalue::CSSColor>(*value).Value());
}

TEST(CSSParserFastPathsTest, ParseHSL) {
  CSSValue* value =
      CSSParserFastPaths::ParseColor("hsl(90deg, 50%, 25%)", kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsColorValue());
  EXPECT_EQ("rgb(64, 96, 32)", value->CssText());

  // Implicit “deg” angle.
  value =
      CSSParserFastPaths::ParseColor("hsl(180, 50%, 50%)", kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsColorValue());
  EXPECT_EQ("rgb(64, 191, 191)", value->CssText());

  // turn.
  value = CSSParserFastPaths::ParseColor("hsl(0.25turn, 25%, 50%)",
                                         kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsColorValue());
  EXPECT_EQ("rgb(128, 159, 96)", value->CssText());

  // rad.
  value = CSSParserFastPaths::ParseColor("hsl(1.0rad, 50%, 50%)",
                                         kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsColorValue());
  EXPECT_EQ("rgb(191, 186, 64)", value->CssText());

  // Wraparound.
  value = CSSParserFastPaths::ParseColor("hsl(450deg, 50%, 50%)",
                                         kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsColorValue());
  EXPECT_EQ("rgb(128, 191, 64)", value->CssText());

  // Lots of wraparound.
  value = CSSParserFastPaths::ParseColor("hsl(4050deg, 50%, 50%)",
                                         kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsColorValue());
  EXPECT_EQ("rgb(128, 191, 64)", value->CssText());

  // Negative wraparound.
  value = CSSParserFastPaths::ParseColor("hsl(-270deg, 50%, 50%)",
                                         kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsColorValue());
  EXPECT_EQ("rgb(128, 191, 64)", value->CssText());

  // Saturation clamping.
  value = CSSParserFastPaths::ParseColor("hsl(45deg, 150%, 50%)",
                                         kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsColorValue());
  EXPECT_EQ("rgb(255, 191, 0)", value->CssText());

  // Lightness clamping to negative.
  value = CSSParserFastPaths::ParseColor("hsl(45deg, 150%, -1000%)",
                                         kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsColorValue());
  EXPECT_EQ("rgb(0, 0, 0)", value->CssText());

  // Writing hsla() without alpha.
  value = CSSParserFastPaths::ParseColor("hsla(45deg, 150%, 50%)",
                                         kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsColorValue());
  EXPECT_EQ("rgb(255, 191, 0)", value->CssText());
}

TEST(CSSParserFastPathsTest, ParseHSLWithAlpha) {
  // With alpha, using hsl().
  CSSValue* value = CSSParserFastPaths::ParseColor("hsl(30 , 1%,75%, 0.5)",
                                                   kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsColorValue());
  EXPECT_EQ("rgba(192, 191, 191, 0.5)", value->CssText());

  // With alpha, using hsla().
  value = CSSParserFastPaths::ParseColor("hsla(30 , 1%,75%, 0.5)",
                                         kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsColorValue());
  EXPECT_EQ("rgba(192, 191, 191, 0.5)", value->CssText());

  // With alpha, using space-separated syntax.
  value = CSSParserFastPaths::ParseColor("hsla(30 1% 75% / 0.1)",
                                         kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsColorValue());
  EXPECT_EQ("rgba(192, 191, 191, 0.1)", value->CssText());

  // Clamp alpha.
  value = CSSParserFastPaths::ParseColor("hsla(30 1% 75% / 1.2)",
                                         kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsColorValue());
  EXPECT_EQ("rgb(192, 191, 191)", value->CssText());
}

TEST(CSSParserFastPathsTest, ParseHSLInvalid) {
  // Invalid unit.
  EXPECT_EQ(nullptr, CSSParserFastPaths::ParseColor("hsl(20dag, 50%, 20%)",
                                                    kHTMLStandardMode));

  // Mix of new and old space syntax.
  EXPECT_EQ(nullptr, CSSParserFastPaths::ParseColor("hsl(0.2, 50%, 20% 0.3)",
                                                    kHTMLStandardMode));
  EXPECT_EQ(nullptr, CSSParserFastPaths::ParseColor("hsl(0.2, 50%, 20% / 0.3)",
                                                    kHTMLStandardMode));
  EXPECT_EQ(nullptr, CSSParserFastPaths::ParseColor("hsl(0.2 50% 20%, 0.3)",
                                                    kHTMLStandardMode));

  // Junk after percentage.
  EXPECT_EQ(nullptr, CSSParserFastPaths::ParseColor(
                         "hsl(0.2, 50% foo, 20% 0.3)", kHTMLStandardMode));

  // Stopping right before an expected %.
  EXPECT_EQ(nullptr,
            CSSParserFastPaths::ParseColor("hsl(9,0.6", kHTMLStandardMode));
}

TEST(CSSParserFastPathsTest, IsValidKeywordPropertyAndValueOverflowClip) {
  EXPECT_TRUE(CSSParserFastPaths::IsValidKeywordPropertyAndValue(
      CSSPropertyID::kOverflowX, CSSValueID::kClip,
      CSSParserMode::kHTMLStandardMode));
}

TEST(CSSParserFastPathsTest, InternalColorsOnlyAllowedInUaMode) {
  EXPECT_EQ(CSSParserFastPaths::ParseColor("blue", kHTMLStandardMode),
            CSSIdentifierValue::Create(CSSValueID::kBlue));
  EXPECT_EQ(CSSParserFastPaths::ParseColor("blue", kHTMLQuirksMode),
            CSSIdentifierValue::Create(CSSValueID::kBlue));
  EXPECT_EQ(CSSParserFastPaths::ParseColor("blue", kUASheetMode),
            CSSIdentifierValue::Create(CSSValueID::kBlue));

  EXPECT_EQ(CSSParserFastPaths::ParseColor("-internal-spelling-error-color",
                                           kHTMLStandardMode),
            nullptr);
  EXPECT_EQ(CSSParserFastPaths::ParseColor("-internal-spelling-error-color",
                                           kHTMLQuirksMode),
            nullptr);
  EXPECT_EQ(
      CSSParserFastPaths::ParseColor("-internal-spelling-error-color",
                                     kUASheetMode),
      CSSIdentifierValue::Create(CSSValueID::kInternalSpellingErrorColor));

  EXPECT_EQ(CSSParserFastPaths::ParseColor("-internal-grammar-error-color",
                                           kHTMLStandardMode),
            nullptr);
  EXPECT_EQ(CSSParserFastPaths::ParseColor("-internal-grammar-error-color",
                                           kHTMLQuirksMode),
            nullptr);
  EXPECT_EQ(CSSParserFastPaths::ParseColor("-internal-grammar-error-color",
                                           kUASheetMode),
            CSSIdentifierValue::Create(CSSValueID::kInternalGrammarErrorColor));
}

}  // namespace blink
