// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_light_dark_value_pair.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"

namespace blink {

namespace {

const CSSValue* ParseUAPropertyValue(CSSPropertyID id, const char* value) {
  auto* ua_context = MakeGarbageCollected<CSSParserContext>(
      kUASheetMode, SecureContextMode::kInsecureContext);
  return CSSParser::ParseSingleValue(id, value, ua_context);
}

TEST(CSSLightDarkValuePairTest, ColorEquals) {
  const auto* value1 =
      ParseUAPropertyValue(CSSPropertyID::kColor, "light-dark(red, green)");
  const auto* value2 =
      ParseUAPropertyValue(CSSPropertyID::kColor, "light-dark(red, green)");
  const auto* value3 =
      ParseUAPropertyValue(CSSPropertyID::kColor, "light-dark(#000, #fff)");
  const auto* value4 = ParseUAPropertyValue(
      CSSPropertyID::kColor, "light-dark(rgb(0, 0, 0), rgb(255, 255, 255))");
  ASSERT_TRUE(value1);
  ASSERT_TRUE(value2);
  ASSERT_TRUE(value3);
  ASSERT_TRUE(value4);
  EXPECT_TRUE(*value1 == *value1);
  EXPECT_TRUE(*value1 == *value2);
  EXPECT_TRUE(*value3 == *value3);
  EXPECT_TRUE(*value4 == *value4);
  EXPECT_TRUE(*value3 == *value4);
}

TEST(CSSLightDarkValuePairTest, BackgroundImageEquals) {
  const auto* value1 = ParseUAPropertyValue(CSSPropertyID::kBackgroundImage,
                                            "light-dark(none, url(dark.png))");
  const auto* value2 = ParseUAPropertyValue(CSSPropertyID::kBackgroundImage,
                                            "light-dark(none, url(dark.png))");
  const auto* value3 = ParseUAPropertyValue(CSSPropertyID::kBackgroundImage,
                                            "light-dark(none, none)");
  const auto* value4 =
      ParseUAPropertyValue(CSSPropertyID::kBackgroundImage, "none");
  ASSERT_TRUE(value1);
  ASSERT_TRUE(value2);
  ASSERT_TRUE(value3);
  ASSERT_TRUE(value4);
  EXPECT_TRUE(*value1 == *value1);
  EXPECT_TRUE(*value1 == *value2);
  EXPECT_TRUE(*value3 == *value3);
  EXPECT_TRUE(*value4 == *value4);
  EXPECT_FALSE(*value3 == *value4);
}

}  // namespace

}  // namespace blink
