// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_gradient_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"

namespace blink {

namespace {

bool CompareGradients(const char* gradient1, const char* gradient2) {
  const CSSValue* value1 = CSSParser::ParseSingleValue(
      CSSPropertyID::kBackgroundImage, gradient1,
      StrictCSSParserContext(SecureContextMode::kInsecureContext));
  const CSSValue* value2 = CSSParser::ParseSingleValue(
      CSSPropertyID::kBackgroundImage, gradient2,
      StrictCSSParserContext(SecureContextMode::kInsecureContext));
  return *value1 == *value2;
}

TEST(CSSGradientValueTest, RadialGradient_Equals) {
  // Trivially identical.
  EXPECT_TRUE(CompareGradients(
      "radial-gradient(circle closest-corner at 100px 60px, blue, red)",
      "radial-gradient(circle closest-corner at 100px 60px, blue, red)"));
  EXPECT_TRUE(CompareGradients(
      "radial-gradient(100px 150px at 100px 60px, blue, red)",
      "radial-gradient(100px 150px at 100px 60px, blue, red)"));

  // Identical with differing parameterization.
  EXPECT_TRUE(CompareGradients(
      "radial-gradient(100px 150px at 100px 60px, blue, red)",
      "radial-gradient(ellipse 100px 150px at 100px 60px, blue, red)"));
  EXPECT_TRUE(CompareGradients(
      "radial-gradient(100px at 100px 60px, blue, red)",
      "radial-gradient(circle 100px at 100px 60px, blue, red)"));
  EXPECT_TRUE(CompareGradients(
      "radial-gradient(closest-corner at 100px 60px, blue, red)",
      "radial-gradient(ellipse closest-corner at 100px 60px, blue, red)"));
  EXPECT_TRUE(CompareGradients(
      "radial-gradient(ellipse at 100px 60px, blue, red)",
      "radial-gradient(ellipse farthest-corner at 100px 60px, blue, red)"));

  // Different.
  EXPECT_FALSE(CompareGradients(
      "radial-gradient(circle closest-corner at 100px 60px, blue, red)",
      "radial-gradient(circle farthest-side  at 100px 60px, blue, red)"));
  EXPECT_FALSE(CompareGradients(
      "radial-gradient(circle at 100px 60px, blue, red)",
      "radial-gradient(circle farthest-side  at 100px 60px, blue, red)"));
  EXPECT_FALSE(CompareGradients(
      "radial-gradient(100px 150px at 100px 60px, blue, red)",
      "radial-gradient(circle farthest-side  at 100px 60px, blue, red)"));
  EXPECT_FALSE(
      CompareGradients("radial-gradient(100px 150px at 100px 60px, blue, red)",
                       "radial-gradient(100px at 100px 60px, blue, red)"));
}

}  // namespace

}  // namespace blink
