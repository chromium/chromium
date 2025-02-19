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
    "attr(p type(<string>))",
    "attr(p type(<color>))",
    "attr(p, type(color))",
    "attr(p type(<color>),)",
    "attr(p type(<color> | ident), color)",
    "attr(p type(<number>+))",
    "attr(p type(<color>#), red)",
    "attr(p px)",
    "attr(p string)",
    "attr(p type(<color>))",
    "attr(p type(<color> ))",
    "attr(p type( <color>))",
    "attr(p type(  <color> ))",
    "attr(p type(<color>) )",
    // clang-format on
};

const char* invalid_attr_values[] = {
    // clang-format off
    "attr(p type(< length>))",
    "attr(p type(<angle> !))",
    "attr(p type(<number >))",
    "attr(p type(<number> +))",
    "attr(p type(<transform-list>+))",
    "attr(p type(!))",
    "attr(p !)",
    "attr(p <px>)",
    "attr(p <string>)",
    "attr(p type(<color>) red)",
    "attr(p type(<url>))",
    // clang-format on
};

const char* valid_auto_base_values[] = {
    // clang-format off
    "-internal-auto-base(foo, bar)",
    "-internal-auto-base(inherit, auto)",
    "-internal-auto-base( 100px ,  200px)",
    "-internal-auto-base(100px,)",
    "-internal-auto-base(,100px)",
    // clang-format on
};

const char* invalid_auto_base_values[] = {
    // clang-format off
    "-internal-auto-base()",
    "-internal-auto-base(100px)",
    "-internal-auto-base(100px;200px)",
    "-internal-auto-base(foo, bar,)",
    "-internal-auto-base(foo, bar, baz)",
    // clang-format on
};

const char* valid_if_values[] = {
    // clang-format off
    "if(style(--prop: abc): true_val;)",
    "if(  style(--prop: abc): true_val;)",
    "if(style(--prop: abc): true_val;   )",
    "if(   style(--prop: abc): true_val   )",
    "if(   style(--prop: abc):   true_val   )",
    "if(style(--prop: abc):)",
    "if(   style(   --prop:   abc   ):   true_val   )",
    "if(style(--prop: abc): true_val; else: false_val)",
    "if(style(--prop: abc): true_val; else: false_val;)",
    "if(style((--prop: abc) and (--prop: def)): true_val; else: false_val)",
    "if(style(--prop1: abc): val1; else: val2)",
    "if(style(((--prop1: abc) and (--prop2: def)) or (not (--prop3: ghi))): val1; else: val2)",
    "if(style(not (--prop: abc)): true_val; else: false_val; style(not (--prop: def)): true_val)",
    "if(style(not (--prop: abc)): true_val; style(not (--prop: def)): true_val)",
    "if(style(--prop: abc): abc; else: if (style(--prop: def): def))",
    "if(style(--prop: abc): abc; else: def; else: ghi)",
    "if(style(--prop: abc): if(style(--prop1: def): x); else: if(style(--prop2: ghi): y))",
    "if(style(--x): a; style(--y): b; style(--z): c;)",
    "if(style(--x): a; style(--y): b; else: c;)",
    "if(style(--prop: abc): )",
    "if(style(--prop: abc): ;)",
    "if(style(--prop: abc): abc; else:)",
    "if(style(--prop: abc): ; else: )",
    "if(style(--prop: abc) : true_val;)",
    "if(style(--prop: abc): true_val; else : false_val;)",
    "if(style(--prop: abc) : true_val; else : false_val)",
    "if(media(screen): true_val; else: false_val;)",
    "if(media((min-width : 500px)): true_val; else: false_val;)",
    "if(media(media(screen and (color))): true_val; else: false_val;)",
    // clang-format on
};

const char* invalid_if_values[] = {
    // clang-format off
    "if()",
    "if(style(--prop: abc))",
    "if(style(--prop: abc): true_val!)",
    "if(!style(--prop: abc): true_val)",
    "if(style(--prop abc): true_val)",
    "if(style(--x and --y): true_val)",
    "if(style(--x) true_val)",
    "if(style(--x!): true_val)",
    "if(style(--prop)): abc",
    "if(style(--prop: abc) abc; else: cba)",
    "if(style(prop: abc): abc; else: cba)",
    "if(style(--prop: abc): abc; else cba)",
    "if(style(--prop1: abc): abc; style(--prop2: def) cba)",
    "if(not style(--prop: abc): true_val; else: false_val; (not style(--prop: def)): true_val)",
    "if(style(--prop: abc): if(style(--prop1: def): x); else: if(style(--prop2: ghi) y))",
    "if(style(--prop: abc) and style(--prop: def): true_val; else: false_val)",
    "if((style(--prop1: abc)): val1; else: val2)","if((style(--prop1: abc)): val1; else: val2)",
    "if(not style(--prop: abc): true_val; else: false_val; (not style(--prop: def)): true_val)",
    "if(not style(--prop: abc): true_val; (not style(--prop: def)): true_val)",
    "if(media(min-width : 500px): true_val; else: false_val;)",
    "if(media(only (min-width : 500px)): true_val; else: false_val;)",
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
      *context));
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
      *context));
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
      *context));
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
      *context));
}

class ValidAutoBaseTest
    : public testing::Test,
      public testing::WithParamInterface<const char*> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    ValidAutoBaseTest,
    testing::ValuesIn(valid_auto_base_values));

TEST_P(ValidAutoBaseTest, ContainsValidFunction) {
  SCOPED_TRACE(GetParam());
  CSSParserTokenStream stream{GetParam()};
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kUASheetMode, SecureContextMode::kInsecureContext);
  bool important;
  EXPECT_TRUE(CSSVariableParser::ConsumeUnparsedDeclaration(
      stream, /*allow_important_annotation=*/false,
      /*is_animation_tainted=*/false, /*must_contain_variable_reference=*/true,
      /*restricted_value=*/true, /*comma_ends_declaration=*/false, important,
      *context));
}

class InvalidAutoBaseTest
    : public testing::Test,
      public testing::WithParamInterface<const char*> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    InvalidAutoBaseTest,
    testing::ValuesIn(invalid_auto_base_values));

TEST_P(InvalidAutoBaseTest, ContainsInvalidFunction) {
  ScopedCSSAdvancedAttrFunctionForTest scoped_feature(true);

  SCOPED_TRACE(GetParam());
  CSSParserTokenStream stream{GetParam()};
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kUASheetMode, SecureContextMode::kInsecureContext);
  bool important;
  EXPECT_FALSE(CSSVariableParser::ConsumeUnparsedDeclaration(
      stream, /*allow_important_annotation=*/false,
      /*is_animation_tainted=*/false, /*must_contain_variable_reference=*/true,
      /*restricted_value=*/true, /*comma_ends_declaration=*/false, important,
      *context));
}

class ValidIfTest : public testing::Test,
                    public testing::WithParamInterface<const char*> {};

INSTANTIATE_TEST_SUITE_P(All, ValidIfTest, testing::ValuesIn(valid_if_values));

TEST_P(ValidIfTest, ContainsValidIf) {
  ScopedCSSInlineIfForStyleQueriesForTest scoped_style_feature(true);
  ScopedCSSInlineIfForMediaQueriesForTest scoped_media_feature(true);

  SCOPED_TRACE(GetParam());
  CSSParserTokenStream stream{GetParam()};
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  bool important;
  EXPECT_TRUE(CSSVariableParser::ConsumeUnparsedDeclaration(
      stream, /*allow_important_annotation=*/false,
      /*is_animation_tainted=*/false, /*must_contain_variable_reference=*/true,
      /*restricted_value=*/true, /*comma_ends_declaration=*/false, important,
      *context));
}

class InvalidIfTest : public testing::Test,
                      public testing::WithParamInterface<const char*> {};

INSTANTIATE_TEST_SUITE_P(All,
                         InvalidIfTest,
                         testing::ValuesIn(invalid_if_values));

TEST_P(InvalidIfTest, ContainsInvalidIf) {
  ScopedCSSInlineIfForStyleQueriesForTest scoped_style_feature(true);
  ScopedCSSInlineIfForMediaQueriesForTest scoped_media_feature(true);

  SCOPED_TRACE(GetParam());
  CSSParserTokenStream stream{GetParam()};
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  bool important;
  EXPECT_FALSE(CSSVariableParser::ConsumeUnparsedDeclaration(
      stream, /*allow_important_annotation=*/false,
      /*is_animation_tainted=*/false, /*must_contain_variable_reference=*/true,
      /*restricted_value=*/true, /*comma_ends_declaration=*/false, important,
      *context));
}

}  // namespace blink
