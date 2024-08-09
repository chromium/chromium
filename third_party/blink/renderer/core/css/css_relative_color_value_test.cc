// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_relative_color_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

TEST(CSSRelativeColorValueTest, Equals) {
  ScopedCSSRelativeColorSupportsCurrentcolorForTest scoped_feature_for_test(
      true);

  const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);

  const CSSValue* value1 = CSSParser::ParseSingleValue(
      CSSPropertyID::kColor, "rgb(from currentcolor 1 calc(g) b)", context);
  EXPECT_TRUE(value1->IsRelativeColorValue());
  const CSSValue* value2 = CSSParser::ParseSingleValue(
      CSSPropertyID::kColor, "rgb(from currentcolor 1 calc(g) b)", context);
  EXPECT_TRUE(value2->IsRelativeColorValue());
  const CSSValue* value3 = CSSParser::ParseSingleValue(
      CSSPropertyID::kColor, "rgb(from currentcolor 1 g b)", context);
  EXPECT_TRUE(value3->IsRelativeColorValue());

  EXPECT_EQ(*value1, *value2);
  EXPECT_NE(*value1, *value3);
}

}  // namespace blink
