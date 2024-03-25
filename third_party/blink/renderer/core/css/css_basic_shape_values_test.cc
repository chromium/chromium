// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_basic_shape_values.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"

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

}  // namespace

}  // namespace blink
