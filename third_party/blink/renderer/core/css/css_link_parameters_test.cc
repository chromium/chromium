// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_param_value_pair.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

CSSValue* Parse(StringView input) {
  CSSParserTokenStream stream(input);
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  CSSParserLocalContext local_context =
      CSSParserLocalContext::CreateWithoutPropertyForTest();
  CSSValue* result =
      css_parsing_utils::ConsumeLinkParameters(stream, *context, local_context);
  // The property parser infrastructure verifies the entire value was consumed.
  // Simulate that check here.
  if (result && !stream.AtEnd()) {
    return nullptr;
  }
  return result;
}

// Returns the CSSValueList if the parse succeeds, nullptr otherwise.
const CSSValueList* ParseAsList(StringView input) {
  CSSValue* result = Parse(input);
  return DynamicTo<CSSValueList>(result);
}

// Extracts the name string from a CSSParamValuePair.
String GetName(const CSSParamValuePair& pair) {
  return pair.Name().CustomCSSText();
}

// Extracts the value string from a CSSParamValuePair.
String GetValue(const CSSParamValuePair& pair) {
  return pair.Value().CustomCSSText();
}

struct Param {
  const char* name;
  const char* value;
};

struct ValidParseTestCase {
  std::string test_name;
  const char* input;
  std::vector<Param> expected_params;
};

using CSSLinkParametersValidParseTest =
    ::testing::TestWithParam<ValidParseTestCase>;

TEST_P(CSSLinkParametersValidParseTest, ParsesSuccessfully) {
  test::TaskEnvironment task_environment;
  const ValidParseTestCase& test_case = GetParam();
  const CSSValueList* result = ParseAsList(test_case.input);
  ASSERT_NE(result, nullptr) << "Failed to parse: " << test_case.input;
  ASSERT_EQ(result->length(), test_case.expected_params.size());
  for (wtf_size_t i = 0; i < result->length(); ++i) {
    const auto& pair = To<CSSParamValuePair>(result->Item(i));
    EXPECT_EQ(GetName(pair), test_case.expected_params[i].name)
        << "Param " << i << " name mismatch";
    EXPECT_EQ(GetValue(pair), test_case.expected_params[i].value)
        << "Param " << i << " value mismatch";
  }
}

const auto kValidParseCases = std::to_array<ValidParseTestCase>({
    {"SingleParamWithValue", "param(--color: red)", {{"--color", "red"}}},
    {"SingleParamEmptyValue", "param(--color: )", {{"--color", ""}}},
    {"SingleParamEmptyValueNoSpace", "param(--color:)", {{"--color", ""}}},
    {"MultipleParams",
     "param(--color: red), param(--bg: blue)",
     {{"--color", "red"}, {"--bg", "blue"}}},
    {"ThreeParams",
     "param(--a: 1), param(--b: 2), param(--c: 3)",
     {{"--a", "1"}, {"--b", "2"}, {"--c", "3"}}},
    {"CaseInsensitiveFunction", "PARAM(--color: red)", {{"--color", "red"}}},
    {"MixedCaseFunction", "Param(--color: red)", {{"--color", "red"}}},
    {"WhitespaceAroundName",
     "param(  --color  :  red  )",
     {{"--color", "red"}}},
    {"MinimalDashedIdent", "param(--: value)", {{"--", "value"}}},
    {"LongDashedIdent",
     "param(--my-custom-color: #ff0000)",
     {{"--my-custom-color", "#ff0000"}}},
    {"ValueWithParens",
     "param(--size: calc(100px + 2em))",
     {{"--size", "calc(100px + 2em)"}}},
    {"ValueWithMultipleTokens",
     "param(--border: 1px solid black)",
     {{"--border", "1px solid black"}}},
    {"ValueWithNestedBrackets",
     "param(--grid: [header] auto [main] 1fr)",
     {{"--grid", "[header] auto [main] 1fr"}}},
    {"ValueWithUrl",
     "param(--bg: url(image.png))",
     {{"--bg", "url(image.png)"}}},
    {"ValueWithString",
     "param(--label: \"hello world\")",
     {{"--label", "\"hello world\""}}},
    {"DuplicateParamNames",
     "param(--color: red), param(--color: blue)",
     {{"--color", "red"}, {"--color", "blue"}}},
});

INSTANTIATE_TEST_SUITE_P(
    ValidCases,
    CSSLinkParametersValidParseTest,
    ::testing::ValuesIn(kValidParseCases),
    [](const testing::TestParamInfo<CSSLinkParametersValidParseTest::ParamType>&
           info) { return info.param.test_name; });

struct InvalidParseTestCase {
  std::string test_name;
  const char* input;
};

using CSSLinkParametersInvalidParseTest =
    ::testing::TestWithParam<InvalidParseTestCase>;

TEST_P(CSSLinkParametersInvalidParseTest, ReturnsNullptr) {
  test::TaskEnvironment task_environment;
  const InvalidParseTestCase& test_case = GetParam();
  CSSValue* result = Parse(test_case.input);
  EXPECT_EQ(result, nullptr) << "Should not parse: " << test_case.input;
}

const auto kInvalidParseCases = std::to_array<InvalidParseTestCase>({
    {"Empty", ""},
    {"NotDashedIdent", "param(color: red)"},
    {"SingleDash", "param(-color: red)"},
    {"NotFunction", "param --color: red"},
    {"WrongFunction", "env(--color: red)"},
    {"MissingCommaBetweenParams", "param(--a: x) param(--b: y)"},
    {"NoParenthesis", "param --color"},
    {"NumberInsteadOfIdent", "param(123: red)"},
    {"EmptyParamFunction", "param()"},
    {"ValueOmitted", "param(--color)"},
    // A parameter name and explicit value are separated by a colon per
    // https://github.com/w3c/csswg-drafts/issues/14275.
    {"CommaInsteadOfColon", "param(--color, red)"},
    {"TrailingGarbage", "param(--color: red) garbage"},
    {"TrailingComma", "param(--a: x), "},
    {"DoubleComma", "param(--a: x), , param(--b: y)"},
    {"NoneKeyword", "none"},
});

INSTANTIATE_TEST_SUITE_P(
    InvalidCases,
    CSSLinkParametersInvalidParseTest,
    ::testing::ValuesIn(kInvalidParseCases),
    [](const testing::TestParamInfo<
        CSSLinkParametersInvalidParseTest::ParamType>& info) {
      return info.param.test_name;
    });

TEST(CSSLinkParametersTest, Serialization) {
  test::TaskEnvironment task_environment;
  const CSSValueList* result =
      ParseAsList("param(--color: red), param(--bg: )");
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->CssText(), "param(--color: red), param(--bg: )");
}

TEST(CSSLinkParametersTest, SerializationRoundTrip) {
  test::TaskEnvironment task_environment;
  const CSSValueList* result =
      ParseAsList("param(--a: hello), param(--b: world)");
  ASSERT_NE(result, nullptr);
  String serialized = result->CssText();

  // Parse the serialized output again.
  const CSSValueList* reparsed = ParseAsList(serialized);
  ASSERT_NE(reparsed, nullptr);
  EXPECT_EQ(*result, *reparsed);
}

TEST(CSSLinkParametersTest, EqualsDifferentValues) {
  test::TaskEnvironment task_environment;
  const CSSValueList* a = ParseAsList("param(--color: red)");
  const CSSValueList* b = ParseAsList("param(--color: blue)");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  EXPECT_NE(*a, *b);
}

TEST(CSSLinkParametersTest, EqualsDifferentNames) {
  test::TaskEnvironment task_environment;
  const CSSValueList* a = ParseAsList("param(--color: red)");
  const CSSValueList* b = ParseAsList("param(--bg: red)");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  EXPECT_NE(*a, *b);
}

TEST(CSSLinkParametersTest, EqualsDifferentCount) {
  test::TaskEnvironment task_environment;
  const CSSValueList* a = ParseAsList("param(--color: red)");
  const CSSValueList* b = ParseAsList("param(--color: red), param(--bg: blue)");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  EXPECT_NE(*a, *b);
}

}  // namespace

}  // namespace blink
