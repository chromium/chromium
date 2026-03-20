// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_alpha_color_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class CSSAlphaColorValueTest : public testing::Test {
 protected:
  CSSAlphaColorValueTest()
      : context_(MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode,
            SecureContextMode::kInsecureContext)) {}

  const CSSValue* Parse(const String& input) {
    return CSSParser::ParseSingleValue(CSSPropertyID::kColor, input, context_);
  }

  ScopedCSSAlphaColorFunctionForTest feature_{true};
  Persistent<const CSSParserContext> context_;
};

TEST_F(CSSAlphaColorValueTest, ParseBasic) {
  const CSSValue* value = Parse("alpha(from red / 0.5)");
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->IsAlphaColorValue());
}

TEST_F(CSSAlphaColorValueTest, ParseWithCurrentColor) {
  const CSSValue* value = Parse("alpha(from currentcolor / 0.5)");
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->IsAlphaColorValue());
}

TEST_F(CSSAlphaColorValueTest, ParseNoAlpha) {
  const CSSValue* value = Parse("alpha(from currentcolor)");
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->IsAlphaColorValue());
}

TEST_F(CSSAlphaColorValueTest, ParseAlphaKeyword) {
  const CSSValue* value = Parse("alpha(from currentcolor / alpha)");
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->IsAlphaColorValue());
}

TEST_F(CSSAlphaColorValueTest, ParseAlphaCalc) {
  const CSSValue* value = Parse("alpha(from currentcolor / calc(alpha * 0.5))");
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->IsAlphaColorValue());
}

TEST_F(CSSAlphaColorValueTest, ParseAlphaNone) {
  const CSSValue* value = Parse("alpha(from currentcolor / none)");
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->IsAlphaColorValue());
}

TEST_F(CSSAlphaColorValueTest, ParseAlphaPercentage) {
  const CSSValue* value = Parse("alpha(from currentcolor / 50%)");
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->IsAlphaColorValue());
}

TEST_F(CSSAlphaColorValueTest, ParseInvalid_NoFrom) {
  EXPECT_EQ(nullptr, Parse("alpha(red / 0.5)"));
}

TEST_F(CSSAlphaColorValueTest, ParseInvalid_NoOriginColor) {
  EXPECT_EQ(nullptr, Parse("alpha(from / 0.5)"));
}

TEST_F(CSSAlphaColorValueTest, ParseInvalid_ExtraTokens) {
  EXPECT_EQ(nullptr, Parse("alpha(from red / 0.5 0.5)"));
}

TEST_F(CSSAlphaColorValueTest, CustomCSSText) {
  const CSSValue* value1 = Parse("alpha(from currentcolor / 0.5)");
  ASSERT_TRUE(value1);
  EXPECT_EQ(value1->CssText(), "alpha(from currentcolor / 0.5)");

  const CSSValue* value2 = Parse("alpha(from currentcolor)");
  ASSERT_TRUE(value2);
  EXPECT_EQ(value2->CssText(), "alpha(from currentcolor)");

  const CSSValue* value3 = Parse("alpha(from currentcolor / alpha)");
  ASSERT_TRUE(value3);
  EXPECT_EQ(value3->CssText(), "alpha(from currentcolor / alpha)");
}

TEST_F(CSSAlphaColorValueTest, Equals) {
  const CSSValue* value1 = Parse("alpha(from currentcolor / 0.5)");
  const CSSValue* value2 = Parse("alpha(from currentcolor / 0.5)");
  const CSSValue* value3 = Parse("alpha(from currentcolor / 0.8)");

  ASSERT_TRUE(value1);
  ASSERT_TRUE(value2);
  ASSERT_TRUE(value3);
  EXPECT_EQ(*value1, *value2);
  EXPECT_NE(*value1, *value3);
}

TEST_F(CSSAlphaColorValueTest, HasRandomFunctions) {
  // Basic alpha color values should not have random functions.
  const CSSValue* value = Parse("alpha(from red / 0.5)");
  ASSERT_TRUE(value);
  EXPECT_FALSE(value->HasRandomFunctions());
}

TEST_F(CSSAlphaColorValueTest, OriginColorAccessor) {
  const CSSValue* value = Parse("alpha(from red / 0.5)");
  ASSERT_TRUE(value);
  const auto* alpha_value = To<cssvalue::CSSAlphaColorValue>(value);
  // "red" is parsed as a color keyword, check the origin color is present.
  EXPECT_EQ(alpha_value->OriginColor().CssText(), "red");
}

TEST_F(CSSAlphaColorValueTest, AlphaAccessor) {
  const CSSValue* with_alpha = Parse("alpha(from red / 0.5)");
  ASSERT_TRUE(with_alpha);
  const auto* alpha_value = To<cssvalue::CSSAlphaColorValue>(with_alpha);
  EXPECT_NE(nullptr, alpha_value->Alpha());

  const CSSValue* without_alpha = Parse("alpha(from red)");
  ASSERT_TRUE(without_alpha);
  const auto* no_alpha_value = To<cssvalue::CSSAlphaColorValue>(without_alpha);
  EXPECT_EQ(nullptr, no_alpha_value->Alpha());
}

TEST_F(CSSAlphaColorValueTest, DisabledByDefault) {
  ScopedCSSAlphaColorFunctionForTest disable(false);
  // When the feature is disabled, alpha() should not parse as a color.
  const CSSValue* value = Parse("alpha(from red / 0.5)");
  EXPECT_EQ(nullptr, value);
}

}  // namespace blink
