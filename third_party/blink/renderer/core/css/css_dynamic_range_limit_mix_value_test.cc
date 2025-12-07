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

TEST(CSSDynamicRangeLimitMixValueTest, ParsingSimple) {
  String value =
      "dynamic-range-limit-mix(standard 10%, constrained 80%, no-limit 10%)";
  const auto* parsed =
      DynamicTo<CSSDynamicRangeLimitMixValue>(CSSParser::ParseSingleValue(
          CSSPropertyID::kDynamicRangeLimit, value,
          StrictCSSParserContext(SecureContextMode::kInsecureContext)));
  ASSERT_NE(parsed, nullptr);

  HeapVector<Member<const CSSValue>> limits = {
      CSSIdentifierValue::Create(CSSValueID::kStandard),
      CSSIdentifierValue::Create(CSSValueID::kConstrained),
      CSSIdentifierValue::Create(CSSValueID::kNoLimit),
  };
  HeapVector<Member<const CSSPrimitiveValue>> percentages = {
      CSSNumericLiteralValue::Create(10,
                                     CSSPrimitiveValue::UnitType::kPercentage),
      CSSNumericLiteralValue::Create(80,
                                     CSSPrimitiveValue::UnitType::kPercentage),
      CSSNumericLiteralValue::Create(10,
                                     CSSPrimitiveValue::UnitType::kPercentage),
  };
  auto* expected = MakeGarbageCollected<CSSDynamicRangeLimitMixValue>(
      std::move(limits), std::move(percentages));

  EXPECT_TRUE(parsed->Equals(*expected));
}

TEST(CSSDynamicRangeLimitMixValueTest, ParsingNested) {
  String value =
      "dynamic-range-limit-mix(dynamic-range-limit-mix(standard 80%, no-limit "
      "20%) "
      "10%, "
      "constrained 90%)";

  const auto* parsed =
      DynamicTo<CSSDynamicRangeLimitMixValue>(CSSParser::ParseSingleValue(
          CSSPropertyID::kDynamicRangeLimit, value,
          StrictCSSParserContext(SecureContextMode::kInsecureContext)));
  ASSERT_NE(parsed, nullptr);

  HeapVector<Member<const CSSValue>> nested_limits = {
      CSSIdentifierValue::Create(CSSValueID::kStandard),
      CSSIdentifierValue::Create(CSSValueID::kNoLimit),
  };
  HeapVector<Member<const CSSPrimitiveValue>> nested_percentages = {
      CSSNumericLiteralValue::Create(80,
                                     CSSPrimitiveValue::UnitType::kPercentage),
      CSSNumericLiteralValue::Create(20,
                                     CSSPrimitiveValue::UnitType::kPercentage),
  };
  HeapVector<Member<const CSSValue>> limits = {
      MakeGarbageCollected<CSSDynamicRangeLimitMixValue>(
          std::move(nested_limits), std::move(nested_percentages)),
      CSSIdentifierValue::Create(CSSValueID::kConstrained),
  };
  HeapVector<Member<const CSSPrimitiveValue>> percentages = {
      CSSNumericLiteralValue::Create(10,
                                     CSSPrimitiveValue::UnitType::kPercentage),
      CSSNumericLiteralValue::Create(90,
                                     CSSPrimitiveValue::UnitType::kPercentage),
  };
  auto* expected = MakeGarbageCollected<CSSDynamicRangeLimitMixValue>(
      std::move(limits), std::move(percentages));

  EXPECT_TRUE(parsed->Equals(*expected));
}

TEST(CSSDynamicRangeLimitMixValueTest, ParsingInvalid) {
  // If all percentages are zero then fail.
  {
    String value = "dynamic-range-limit-mix(standard 0%, constrained 0%)";
    const auto* parsed =
        DynamicTo<CSSDynamicRangeLimitMixValue>(CSSParser::ParseSingleValue(
            CSSPropertyID::kDynamicRangeLimit, value,
            StrictCSSParserContext(SecureContextMode::kInsecureContext)));
    EXPECT_EQ(parsed, nullptr);
  }

  // Negative percentages not allowed.
  {
    String value = "dynamic-range-limit-mix(standard -1%, constrained 10%)";
    const auto* parsed =
        DynamicTo<CSSDynamicRangeLimitMixValue>(CSSParser::ParseSingleValue(
            CSSPropertyID::kDynamicRangeLimit, value,
            StrictCSSParserContext(SecureContextMode::kInsecureContext)));
    EXPECT_EQ(parsed, nullptr);
  }

  // Percentages above 100 not allowed.
  {
    String value = "dynamic-range-limit-mix(standard 110%, constrained 10%)";
    const auto* parsed =
        DynamicTo<CSSDynamicRangeLimitMixValue>(CSSParser::ParseSingleValue(
            CSSPropertyID::kDynamicRangeLimit, value,
            StrictCSSParserContext(SecureContextMode::kInsecureContext)));
    EXPECT_EQ(parsed, nullptr);
  }

  // Percentages are not optional.
  {
    String value = "dynamic-range-limit-mix(no-limit, constrained 10%)";
    const auto* parsed =
        DynamicTo<CSSDynamicRangeLimitMixValue>(CSSParser::ParseSingleValue(
            CSSPropertyID::kDynamicRangeLimit, value,
            StrictCSSParserContext(SecureContextMode::kInsecureContext)));
    EXPECT_EQ(parsed, nullptr);
  }

  // Disallow junk after the percent.
  {
    String value =
        "dynamic-range-limit-mix(standard 10% parasaurolophus, "
        "constrained 10%)";
    const auto* parsed =
        DynamicTo<CSSDynamicRangeLimitMixValue>(CSSParser::ParseSingleValue(
            CSSPropertyID::kDynamicRangeLimit, value,
            StrictCSSParserContext(SecureContextMode::kInsecureContext)));
    EXPECT_EQ(parsed, nullptr);
  }

  // Disallow junk at the end
  {
    String value =
        "dynamic-range-limit-mix(standard 10%, constrained 10%, "
        "pachycephalosaurus)";
    const auto* parsed =
        DynamicTo<CSSDynamicRangeLimitMixValue>(CSSParser::ParseSingleValue(
            CSSPropertyID::kDynamicRangeLimit, value,
            StrictCSSParserContext(SecureContextMode::kInsecureContext)));
    EXPECT_EQ(parsed, nullptr);
  }
}

}  // namespace
}  // namespace cssvalue
}  // namespace blink
