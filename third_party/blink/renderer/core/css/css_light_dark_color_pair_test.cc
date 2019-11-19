// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_light_dark_color_pair.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"

namespace blink {

namespace {

const CSSValue* CreateLightDarkColorPair(const char* value) {
  auto* ua_context = MakeGarbageCollected<CSSParserContext>(
      kUASheetMode, SecureContextMode::kInsecureContext);
  return CSSParser::ParseSingleValue(CSSPropertyID::kColor, value, ua_context);
}

TEST(CSSLightDarkColorPairTest, Equals) {
  const auto* value1 =
      CreateLightDarkColorPair("-internal-light-dark-color(red, green)");
  const auto* value2 =
      CreateLightDarkColorPair("-internal-light-dark-color(red, green)");
  const auto* value3 =
      CreateLightDarkColorPair("-internal-light-dark-color(#000, #fff)");
  const auto* value4 = CreateLightDarkColorPair(
      "-internal-light-dark-color(rgb(0, 0, 0), rgb(255, 255, 255))");
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

}  // namespace

}  // namespace blink
