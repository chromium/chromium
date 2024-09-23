// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

const char* valid_variable_reference_value[] = {
    // clang-format off
    "var(--x)",
    "A var(--x)",
    "var(--x) A",

    // {} as the whole value:
    "{ var(--x) }",
    "{ A var(--x) }",
    "{ var(--x) A }",
    "{ var(--x) A",
    "{ var(--x)",
    "{ var(--x) []",

    // {} inside another block:
    "var(--x) [{}]",
    "[{}] var(--x)",
    "foo({}) var(--x)",
    "var(--x) foo({})",
    // clang-format on
};

const char* invalid_variable_reference_value[] = {
    // clang-format off
    "var(--x) {}",
    "{} var(--x)",
    "A { var(--x) }",
    "{ var(--x) } A",
    "[] { var(--x) }",
    "{ var(--x) } []",
    "{}{ var(--x) }",
    "{ var(--x) }{}",
    // clang-format on
};

const char* valid_attr_values[] = {
    // clang-format off
    "attr(p)",
    "attr(p,)",
    "attr(p string)",
    "attr(p color)",
    "attr(p, color)",
    "attr(p color,)",
    "attr(p color, color)",
    "attr(p number)",
    "attr(p color, red)",
    // clang-format on
};

const char* invalid_attr_values[] = {
    // clang-format off
    "attr(p url)",
    "attr(p !)",
    "attr(p color red)",
    // clang-format on
};

class ValidVariableReferenceTest
    : public testing::Test,
      public testing::WithParamInterface<const char*> {
 public:
  ValidVariableReferenceTest() = default;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ValidVariableReferenceTest,
                         testing::ValuesIn(valid_variable_reference_value));

TEST_P(ValidVariableReferenceTest, ConsumeUnparsedDeclaration) {
  SCOPED_TRACE(GetParam());
  CSSParserTokenStream stream{GetParam()};
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  bool important;
  EXPECT_TRUE(CSSVariableParser::ConsumeUnparsedDeclaration(
      stream, /*allow_important_annotation=*/false,
      /*is_animation_tainted=*/false, /*must_contain_variable_reference=*/true,
      /*restricted_value=*/true, /*comma_ends_declaration=*/false, important,
      context->GetExecutionContext()));
}

TEST_P(ValidVariableReferenceTest, ParseUniversalSyntaxValue) {
  SCOPED_TRACE(GetParam());
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  EXPECT_NE(nullptr,
            CSSVariableParser::ParseUniversalSyntaxValue(
                GetParam(), *context, /* is_animation_tainted */ false));
}

class InvalidVariableReferenceTest
    : public testing::Test,
      public testing::WithParamInterface<const char*> {
 public:
  InvalidVariableReferenceTest() = default;
};

INSTANTIATE_TEST_SUITE_P(All,
                         InvalidVariableReferenceTest,
                         testing::ValuesIn(invalid_variable_reference_value));

TEST_P(InvalidVariableReferenceTest, ConsumeUnparsedDeclaration) {
  SCOPED_TRACE(GetParam());
  CSSParserTokenStream stream{GetParam()};
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  bool important;
  EXPECT_FALSE(CSSVariableParser::ConsumeUnparsedDeclaration(
      stream, /*allow_important_annotation=*/false,
      /*is_animation_tainted=*/false, /*must_contain_variable_reference=*/true,
      /*restricted_value=*/true, /*comma_ends_declaration=*/false, important,
      context->GetExecutionContext()));
}

TEST_P(InvalidVariableReferenceTest, ParseUniversalSyntaxValue) {
  SCOPED_TRACE(GetParam());
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  EXPECT_NE(nullptr,
            CSSVariableParser::ParseUniversalSyntaxValue(
                GetParam(), *context, /* is_animation_tainted */ false));
}

class CustomPropertyDeclarationTest
    : public testing::Test,
      public testing::WithParamInterface<const char*> {
 public:
  CustomPropertyDeclarationTest() = default;
};

// Although these are invalid as var()-containing <declaration-value>s
// in a standard property, they are valid in custom property declarations.
INSTANTIATE_TEST_SUITE_P(All,
                         CustomPropertyDeclarationTest,
                         testing::ValuesIn(invalid_variable_reference_value));

TEST_P(CustomPropertyDeclarationTest, ParseDeclarationValue) {
  SCOPED_TRACE(GetParam());
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  EXPECT_NE(nullptr,
            CSSVariableParser::ParseDeclarationValue(
                GetParam(), /* is_animation_tainted */ false, *context));
}

class ValidAttrTest : public testing::Test,
                      public testing::WithParamInterface<const char*> {};

INSTANTIATE_TEST_SUITE_P(All,
                         ValidAttrTest,
                         testing::ValuesIn(valid_attr_values));

TEST_P(ValidAttrTest, ContainsValidAttr) {
  ScopedCSSAdvancedAttrFunctionForTest scoped_feature(true);
  SCOPED_TRACE(GetParam());
  CSSParserTokenStream stream{GetParam()};
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  bool important;
  EXPECT_TRUE(CSSVariableParser::ConsumeUnparsedDeclaration(
      stream, /*allow_important_annotation=*/false,
      /*is_animation_tainted=*/false, /*must_contain_variable_reference=*/true,
      /*restricted_value=*/true, /*comma_ends_declaration=*/false, important,
      context->GetExecutionContext()));
}

class InvalidAttrTest : public testing::Test,
                        public testing::WithParamInterface<const char*> {};

INSTANTIATE_TEST_SUITE_P(All,
                         InvalidAttrTest,
                         testing::ValuesIn(invalid_attr_values));

TEST_P(InvalidAttrTest, ContainsValidAttr) {
  ScopedCSSAdvancedAttrFunctionForTest scoped_feature(true);

  SCOPED_TRACE(GetParam());
  CSSParserTokenStream stream{GetParam()};
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  bool important;
  EXPECT_FALSE(CSSVariableParser::ConsumeUnparsedDeclaration(
      stream, /*allow_important_annotation=*/false,
      /*is_animation_tainted=*/false, /*must_contain_variable_reference=*/true,
      /*restricted_value=*/true, /*comma_ends_declaration=*/false, important,
      context->GetExecutionContext()));
}

}  // namespace blink
