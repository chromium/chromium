// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_attr_type.h"

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

constexpr const char* kDimensionUnits[] = {
    "em",  "ex",   "cap", "ch", "ic",   "rem",  "lh",  "rlh",
    "vw",  "vh",   "vi",  "vb", "vmin", "vmax", "deg", "grad",
    "rad", "turn", "ms",  "ms", "hz",   "khz"};
constexpr const char* kValidAttrSyntax[] = {
    "type(<color>)",   "type(<length> | <percentage>)",
    "type(<angle>#)",  "type(<color>+ | <image>#)",
    "type(<color> )",  "type( <color>)",
    "type( <color> )", "type(<length>)   "};
constexpr const char* kInvalidAttrSyntax[] = {
    "type(<number >)", "type(< angle>)", "type(<length> +)", "type(<color> !)",
    "type(!<color>)"};

class CSSAttrTypeTest : public PageTestBase {};

TEST_F(CSSAttrTypeTest, ConsumeRawStringType) {
  CSSParserTokenStream valid_stream("raw-string");
  std::optional<CSSAttrType> valid_type = CSSAttrType::Consume(valid_stream);
  ASSERT_TRUE(valid_type.has_value());
  EXPECT_TRUE(valid_type->IsString());
  EXPECT_TRUE(valid_stream.AtEnd());
}

TEST_F(CSSAttrTypeTest, ConsumeNumberType) {
  CSSParserTokenStream valid_stream("number");
  std::optional<CSSAttrType> valid_type = CSSAttrType::Consume(valid_stream);
  ASSERT_TRUE(valid_type.has_value());
  EXPECT_TRUE(valid_type->IsNumber());
  EXPECT_TRUE(valid_stream.AtEnd());
}

TEST_F(CSSAttrTypeTest, ConsumeInvalidType) {
  CSSParserTokenStream stream("invalid");
  std::optional<CSSAttrType> type = CSSAttrType::Consume(stream);
  ASSERT_FALSE(type.has_value());
  EXPECT_EQ(stream.Offset(), 0u);
}

class ValidSyntaxTest : public CSSAttrTypeTest,
                        public testing::WithParamInterface<const char*> {};

INSTANTIATE_TEST_SUITE_P(CSSAttrTypeTest,
                         ValidSyntaxTest,
                         testing::ValuesIn(kValidAttrSyntax));

TEST_P(ValidSyntaxTest, ConsumeValidSyntaxType) {
  CSSParserTokenStream stream(GetParam());
  std::optional<CSSAttrType> type = CSSAttrType::Consume(stream);
  ASSERT_TRUE(type.has_value());
  EXPECT_TRUE(type->IsSyntax());
  EXPECT_TRUE(stream.AtEnd());
}

class InvalidSyntaxTest : public CSSAttrTypeTest,
                          public testing::WithParamInterface<const char*> {};

INSTANTIATE_TEST_SUITE_P(CSSAttrTypeTest,
                         InvalidSyntaxTest,
                         testing::ValuesIn(kInvalidAttrSyntax));

TEST_P(InvalidSyntaxTest, ConsumeInvalidSyntaxType) {
  CSSParserTokenStream stream(GetParam());
  std::optional<CSSAttrType> type = CSSAttrType::Consume(stream);
  ASSERT_FALSE(type.has_value());
  EXPECT_EQ(stream.Offset(), 0u);
}

class DimensionUnitTypeTest : public CSSAttrTypeTest,
                              public testing::WithParamInterface<const char*> {
};

INSTANTIATE_TEST_SUITE_P(CSSAttrTypeTest,
                         DimensionUnitTypeTest,
                         testing::ValuesIn(kDimensionUnits));

TEST_P(DimensionUnitTypeTest, ConsumeDimensionUnitType) {
  CSSParserTokenStream stream(GetParam());
  std::optional<CSSAttrType> type = CSSAttrType::Consume(stream);
  ASSERT_TRUE(type.has_value());
  EXPECT_TRUE(type->IsDimensionUnit());
  EXPECT_TRUE(stream.AtEnd());
}

TEST_P(DimensionUnitTypeTest, ParseDimensionUnitTypeValid) {
  CSSParserTokenStream stream(GetParam());
  std::optional<CSSAttrType> type = CSSAttrType::Consume(stream);
  ASSERT_TRUE(type.has_value());
  String valid_value("3");
  String expected_value = valid_value + String(GetParam());
  const auto* context = MakeGarbageCollected<CSSParserContext>(GetDocument());
  const CSSValue* parsed_value = type->Parse(valid_value, *context);
  EXPECT_EQ(parsed_value->CssText(), expected_value);
}

TEST_P(DimensionUnitTypeTest, ParseDimensionUnitTypeInvalid) {
  CSSParserTokenStream stream(GetParam());
  std::optional<CSSAttrType> type = CSSAttrType::Consume(stream);
  ASSERT_TRUE(type.has_value());
  String valid_value("3px");
  const auto* context = MakeGarbageCollected<CSSParserContext>(GetDocument());
  const CSSValue* parsed_value = type->Parse(valid_value, *context);
  EXPECT_FALSE(parsed_value);
}

}  // namespace blink
