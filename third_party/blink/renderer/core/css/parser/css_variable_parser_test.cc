// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenized_value.h"

#include "testing/gtest/include/gtest/gtest.h"

#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {

Vector<CSSParserToken, 32> Parse(const char* input) {
  String string(input);
  CSSTokenizer tokenizer(string);
  return tokenizer.TokenizeToEOF();
}

}  // namespace

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

class ValidVariableReferenceTest
    : public testing::Test,
      public testing::WithParamInterface<const char*>,
      private ScopedCSSNestingIdentForTest {
 public:
  ValidVariableReferenceTest() : ScopedCSSNestingIdentForTest(true) {}
};

INSTANTIATE_TEST_SUITE_P(All,
                         ValidVariableReferenceTest,
                         testing::ValuesIn(valid_variable_reference_value));

TEST_P(ValidVariableReferenceTest, ContainsValidVariableReferences) {
  SCOPED_TRACE(GetParam());
  Vector<CSSParserToken, 32> tokens = Parse(GetParam());
  CSSParserTokenRange range(tokens);
  EXPECT_TRUE(CSSVariableParser::ContainsValidVariableReferences(range));
}

TEST_P(ValidVariableReferenceTest, ParseVariableReferenceValue) {
  SCOPED_TRACE(GetParam());
  Vector<CSSParserToken, 32> tokens = Parse(GetParam());
  CSSParserTokenRange range(tokens);
  CSSTokenizedValue tokenized_value = {range, /* text */ ""};
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  EXPECT_NE(nullptr,
            CSSVariableParser::ParseVariableReferenceValue(
                tokenized_value, *context, /* is_animation_tainted */ false));
}

class InvalidVariableReferenceTest
    : public testing::Test,
      public testing::WithParamInterface<const char*>,
      private ScopedCSSNestingIdentForTest {
 public:
  InvalidVariableReferenceTest() : ScopedCSSNestingIdentForTest(true) {}
};

INSTANTIATE_TEST_SUITE_P(All,
                         InvalidVariableReferenceTest,
                         testing::ValuesIn(invalid_variable_reference_value));

TEST_P(InvalidVariableReferenceTest, ContainsValidVariableReferences) {
  SCOPED_TRACE(GetParam());
  Vector<CSSParserToken, 32> tokens = Parse(GetParam());
  CSSParserTokenRange range(tokens);
  EXPECT_FALSE(CSSVariableParser::ContainsValidVariableReferences(range));
}

TEST_P(InvalidVariableReferenceTest, ParseVariableReferenceValue) {
  SCOPED_TRACE(GetParam());
  Vector<CSSParserToken, 32> tokens = Parse(GetParam());
  CSSParserTokenRange range(tokens);
  CSSTokenizedValue tokenized_value = {range, /* text */ ""};
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  EXPECT_EQ(nullptr,
            CSSVariableParser::ParseVariableReferenceValue(
                tokenized_value, *context, /* is_animation_tainted */ false));
}

class CustomPropertyDeclarationTest
    : public testing::Test,
      public testing::WithParamInterface<const char*>,
      private ScopedCSSNestingIdentForTest {
 public:
  CustomPropertyDeclarationTest() : ScopedCSSNestingIdentForTest(true) {}
};

// Although these are invalid as var()-containing <declaration-value>s
// in a standard property, they are valid in custom property declarations.
INSTANTIATE_TEST_SUITE_P(All,
                         CustomPropertyDeclarationTest,
                         testing::ValuesIn(invalid_variable_reference_value));

TEST_P(CustomPropertyDeclarationTest, ParseDeclarationValue) {
  SCOPED_TRACE(GetParam());
  Vector<CSSParserToken, 32> tokens = Parse(GetParam());
  CSSParserTokenRange range(tokens);
  CSSTokenizedValue tokenized_value = {range, /* text */ ""};
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  EXPECT_NE(nullptr,
            CSSVariableParser::ParseDeclarationValue(
                tokenized_value, /* is_animation_tainted */ false, *context));
}

}  // namespace blink
