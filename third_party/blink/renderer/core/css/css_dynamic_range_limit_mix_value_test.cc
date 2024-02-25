// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_dynamic_range_limit_mix_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {
namespace cssvalue {
namespace {

TEST(CSSDynamicRangeLimitMixValueTest, Parsing) {
  String value =
      "dynamic-range-limit-mix(dynamic-range-limit-mix(standard, high, 20%), "
      "constrained-high, 60%)";

  const auto* parsed =
      DynamicTo<CSSDynamicRangeLimitMixValue>(CSSParser::ParseSingleValue(
          CSSPropertyID::kDynamicRangeLimit, value,
          StrictCSSParserContext(SecureContextMode::kInsecureContext)));
  ASSERT_NE(parsed, nullptr);
  EXPECT_TRUE(parsed->Equals(CSSDynamicRangeLimitMixValue(
      MakeGarbageCollected<CSSDynamicRangeLimitMixValue>(
          CSSIdentifierValue::Create(CSSValueID::kStandard),
          CSSIdentifierValue::Create(CSSValueID::kHigh),
          CSSNumericLiteralValue::Create(
              20, CSSPrimitiveValue::UnitType::kPercentage)),
      CSSIdentifierValue::Create(CSSValueID::kConstrainedHigh),
      CSSNumericLiteralValue::Create(
          60, CSSPrimitiveValue::UnitType::kPercentage))));
}

}  // namespace
}  // namespace cssvalue
}  // namespace blink
