// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_basic_shape_values.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {

const CSSValue* ParsePropertyValue(CSSPropertyID id, const char* value) {
  auto* ua_context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  return CSSParser::ParseSingleValue(id, value, ua_context);
}

TEST(CSSBasicShapeValuesTest, PolygonEquals) {
  const auto* value_default_windrule = ParsePropertyValue(
      CSSPropertyID::kClipPath, "polygon(0% 0%, 100% 0%, 50% 50%)");
  const auto* value_evenodd_windrule = ParsePropertyValue(
      CSSPropertyID::kClipPath, "polygon(evenodd, 0% 0%, 100% 0%, 50% 50%)");
  const auto* value_nonzero_windrule = ParsePropertyValue(
      CSSPropertyID::kClipPath, "polygon(nonzero, 0% 0%, 100% 0%, 50% 50%)");
  ASSERT_TRUE(value_default_windrule);
  ASSERT_TRUE(value_evenodd_windrule);
  ASSERT_TRUE(value_nonzero_windrule);
  EXPECT_TRUE(*value_default_windrule == *value_nonzero_windrule);
  EXPECT_FALSE(*value_default_windrule == *value_evenodd_windrule);
  EXPECT_FALSE(*value_nonzero_windrule == *value_evenodd_windrule);
}

TEST(CSSBasicShapeValuesTest, PolygonRoundParsingIsFlagged) {
  {
    ScopedCSSPolygonRoundingForTest scoped_polygon_rounding(false);
    EXPECT_FALSE(
        ParsePropertyValue(CSSPropertyID::kClipPath,
                           "polygon(round 10px, 0% 0%, 100% 0%, 50% 50%)"));
  }

  ScopedCSSPolygonRoundingForTest scoped_polygon_rounding(true);
  const auto* value = ParsePropertyValue(
      CSSPropertyID::kClipPath,
      "polygon(evenodd round 10px, 0% 0%, 100% 0%, 50% 50%)");
  ASSERT_TRUE(value);
  EXPECT_EQ("polygon(evenodd round 10px, 0% 0%, 100% 0%, 50% 50%)",
            value->CssText());
}

TEST(CSSBasicShapeValuesTest, PolygonRoundAffectsEquality) {
  ScopedCSSPolygonRoundingForTest scoped_polygon_rounding(true);
  const auto* rounded = ParsePropertyValue(
      CSSPropertyID::kClipPath, "polygon(round 10px, 0% 0%, 100% 0%, 50% 50%)");
  const auto* sharp = ParsePropertyValue(CSSPropertyID::kClipPath,
                                         "polygon(0% 0%, 100% 0%, 50% 50%)");
  ASSERT_TRUE(rounded);
  ASSERT_TRUE(sharp);
  EXPECT_NE(*rounded, *sharp);
}

}  // namespace

}  // namespace blink
