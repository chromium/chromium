// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

using css_parsing_utils::AtDelimiter;
using css_parsing_utils::AtIdent;
using css_parsing_utils::ConsumeAngle;
using css_parsing_utils::ConsumeIfDelimiter;
using css_parsing_utils::ConsumeIfIdent;

CSSParserContext* MakeContext(CSSParserMode mode = kHTMLStandardMode) {
  return MakeGarbageCollected<CSSParserContext>(
      mode, SecureContextMode::kInsecureContext);
}

TEST(CSSParsingUtilsTest, BasicShapeUseCount) {
  test::TaskEnvironment task_environment;
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSBasicShape;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->setInnerHTML(
      "<style>span { shape-outside: circle(); }</style>");
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST(CSSParsingUtilsTest, Revert) {
  EXPECT_TRUE(css_parsing_utils::IsCSSWideKeyword(CSSValueID::kRevert));
  EXPECT_TRUE(css_parsing_utils::IsCSSWideKeyword("revert"));
}

double ConsumeAngleValue(String target) {
  CSSParserTokenStream stream(target);
  return ConsumeAngle(stream, *MakeContext(), std::nullopt)->ComputeDegrees();
}

double ConsumeAngleValue(String target, double min, double max) {
  CSSParserTokenStream stream(target);
  return ConsumeAngle(stream, *MakeContext(), std::nullopt, min, max)
      ->ComputeDegrees();
}

TEST(CSSParsingUtilsTest, ConsumeAngles) {
  const double kMaxDegreeValue = 2867080569122160;

  EXPECT_EQ(10.0, ConsumeAngleValue("10deg"));
  EXPECT_EQ(-kMaxDegreeValue, ConsumeAngleValue("-3.40282e+38deg"));
  EXPECT_EQ(kMaxDegreeValue, ConsumeAngleValue("3.40282e+38deg"));

  EXPECT_EQ(kMaxDegreeValue, ConsumeAngleValue("calc(infinity * 1deg)"));
  EXPECT_EQ(-kMaxDegreeValue, ConsumeAngleValue("calc(-infinity * 1deg)"));
  EXPECT_EQ(kMaxDegreeValue, ConsumeAngleValue("calc(NaN * 1deg)"));

  // Math function with min and max ranges

  EXPECT_EQ(-100, ConsumeAngleValue("calc(-3.40282e+38deg)", -100, 100));
  EXPECT_EQ(100, ConsumeAngleValue("calc(3.40282e+38deg)", -100, 100));
}

TEST(CSSParsingUtilsTest, AtIdent) {
  String text = "foo,bar,10px";
  CSSParserTokenStream stream(text);
  EXPECT_FALSE(AtIdent(stream.Peek(), "bar"));  // foo
  stream.Consume();
  EXPECT_FALSE(AtIdent(stream.Peek(), "bar"));  // ,
  stream.Consume();
  EXPECT_TRUE(AtIdent(stream.Peek(), "bar"));  // bar
  stream.Consume();
  EXPECT_FALSE(AtIdent(stream.Peek(), "bar"));  // ,
  stream.Consume();
  EXPECT_FALSE(AtIdent(stream.Peek(), "bar"));  // 10px
  stream.Consume();
  EXPECT_FALSE(AtIdent(stream.Peek(), "bar"));  // EOF
  stream.Consume();
}

TEST(CSSParsingUtilsTest, ConsumeIfIdent) {
  String text = "foo,bar,10px";
  CSSParserTokenStream stream(text);
  EXPECT_TRUE(AtIdent(stream.Peek(), "foo"));
  EXPECT_FALSE(ConsumeIfIdent(stream, "bar"));
  EXPECT_TRUE(AtIdent(stream.Peek(), "foo"));
  EXPECT_TRUE(ConsumeIfIdent(stream, "foo"));
  EXPECT_EQ(kCommaToken, stream.Peek().GetType());
}

TEST(CSSParsingUtilsTest, AtDelimiter) {
  String text = "foo,<,10px";
  CSSParserTokenStream stream(text);
  EXPECT_FALSE(AtDelimiter(stream.Peek(), '<'));  // foo
  stream.Consume();
  EXPECT_FALSE(AtDelimiter(stream.Peek(), '<'));  // ,
  stream.Consume();
  EXPECT_TRUE(AtDelimiter(stream.Peek(), '<'));  // <
  stream.Consume();
  EXPECT_FALSE(AtDelimiter(stream.Peek(), '<'));  // ,
  stream.Consume();
  EXPECT_FALSE(AtDelimiter(stream.Peek(), '<'));  // 10px
  stream.Consume();
  EXPECT_FALSE(AtDelimiter(stream.Peek(), '<'));  // EOF
  stream.Consume();
}

TEST(CSSParsingUtilsTest, ConsumeIfDelimiter) {
  String text = "<,=,10px";
  CSSParserTokenStream stream(text);
  EXPECT_TRUE(AtDelimiter(stream.Peek(), '<'));
  EXPECT_FALSE(ConsumeIfDelimiter(stream, '='));
  EXPECT_TRUE(AtDelimiter(stream.Peek(), '<'));
  EXPECT_TRUE(ConsumeIfDelimiter(stream, '<'));
  EXPECT_EQ(kCommaToken, stream.Peek().GetType());
}

TEST(CSSParsingUtilsTest, ConsumeAnyValue_Stream) {
  struct {
    // The input string to parse as <any-value>.
    const char* input;
    // The serialization of the tokens remaining in the stream.
    const char* remainder;
  } tests[] = {
      {"1", ""},
      {"1px", ""},
      {"1px ", ""},
      {"ident", ""},
      {"(([ident]))", ""},
      {" ( ( 1 ) ) ", ""},
      {"rgb(1, 2, 3)", ""},
      {"rgb(1, 2, 3", ""},
      {"!!!;;;", ""},
      {"asdf)", ")"},
      {")asdf", ")asdf"},
      {"(ab)cd) e", ") e"},
      {"(as]df) e", "(as]df) e"},
      {"(a b [ c { d ) e } f ] g h) i", "(a b [ c { d ) e } f ] g h) i"},
      {"a url(() b", "url(() b"},
  };

  for (const auto& test : tests) {
    String input(test.input);
    SCOPED_TRACE(input);
    CSSParserTokenStream stream(input);
    css_parsing_utils::ConsumeAnyValue(stream);
    EXPECT_EQ(String(test.remainder), stream.RemainingText().ToString());
  }
}

TEST(CSSParsingUtilsTest, DashedIdent) {
  struct Expectations {
    String css_text;
    bool is_dashed_indent;
  } expectations[] = {
      {"--grogu", true}, {"--1234", true}, {"--\U0001F37A", true},
      {"--", true},      {"-", false},     {"blue", false},
      {"body", false},   {"0", false},     {"#FFAA00", false},
  };
  for (auto& expectation : expectations) {
    CSSParserTokenStream stream(expectation.css_text);
    EXPECT_EQ(css_parsing_utils::IsDashedIdent(stream.Peek()),
              expectation.is_dashed_indent);
  }
}

TEST(CSSParsingUtilsTest, ConsumeAbsoluteColor) {
  auto ConsumeColorForTest = [](String css_text, auto func) {
    CSSParserTokenStream stream(css_text);
    CSSParserContext* context = MakeContext();
    return func(stream, *context);
  };

  struct {
    STACK_ALLOCATED();

   public:
    String css_text;
    CSSIdentifierValue* consume_color_expectation;
    CSSIdentifierValue* consume_absolute_color_expectation;
  } expectations[]{
      {"Canvas", CSSIdentifierValue::Create(CSSValueID::kCanvas), nullptr},
      {"HighlightText", CSSIdentifierValue::Create(CSSValueID::kHighlighttext),
       nullptr},
      {"GrayText", CSSIdentifierValue::Create(CSSValueID::kGraytext), nullptr},
      {"blue", CSSIdentifierValue::Create(CSSValueID::kBlue),
       CSSIdentifierValue::Create(CSSValueID::kBlue)},
      // Deprecated system colors are not allowed either.
      {"ActiveBorder", CSSIdentifierValue::Create(CSSValueID::kActiveborder),
       nullptr},
      {"WindowText", CSSIdentifierValue::Create(CSSValueID::kWindowtext),
       nullptr},
      {"currentcolor", CSSIdentifierValue::Create(CSSValueID::kCurrentcolor),
       nullptr},
  };
  for (auto& expectation : expectations) {
    EXPECT_EQ(ConsumeColorForTest(expectation.css_text,
                                  css_parsing_utils::ConsumeColor),
              expectation.consume_color_expectation);
    EXPECT_EQ(ConsumeColorForTest(expectation.css_text,
                                  css_parsing_utils::ConsumeAbsoluteColor),
              expectation.consume_absolute_color_expectation);
  }
}

TEST(CSSParsingUtilsTest, InternalColorsOnlyAllowedInUaMode) {
  auto ConsumeColorForTest = [](String css_text, CSSParserMode mode) {
    CSSParserTokenStream stream(css_text);
    return css_parsing_utils::ConsumeColor(stream, *MakeContext(mode));
  };

  struct {
    STACK_ALLOCATED();

   public:
    String css_text;
    CSSIdentifierValue* ua_expectation;
    CSSIdentifierValue* other_expectation;
  } expectations[]{
      {"blue", CSSIdentifierValue::Create(CSSValueID::kBlue),
       CSSIdentifierValue::Create(CSSValueID::kBlue)},
      {"-internal-spelling-error-color",
       CSSIdentifierValue::Create(CSSValueID::kInternalSpellingErrorColor),
       nullptr},
      {"-internal-grammar-error-color",
       CSSIdentifierValue::Create(CSSValueID::kInternalGrammarErrorColor),
       nullptr},
      {"-internal-search-color",
       CSSIdentifierValue::Create(CSSValueID::kInternalSearchColor), nullptr},
      {"-internal-search-text-color",
       CSSIdentifierValue::Create(CSSValueID::kInternalSearchTextColor),
       nullptr},
      {"-internal-current-search-color",
       CSSIdentifierValue::Create(CSSValueID::kInternalCurrentSearchColor),
       nullptr},
      {"-internal-current-search-text-color",
       CSSIdentifierValue::Create(CSSValueID::kInternalCurrentSearchTextColor),
       nullptr},
  };
  for (auto& expectation : expectations) {
    EXPECT_EQ(ConsumeColorForTest(expectation.css_text, kHTMLStandardMode),
              expectation.other_expectation);
    EXPECT_EQ(ConsumeColorForTest(expectation.css_text, kHTMLQuirksMode),
              expectation.other_expectation);
    EXPECT_EQ(ConsumeColorForTest(expectation.css_text, kUASheetMode),
              expectation.ua_expectation);
  }
}

// Verify that the state of CSSParserTokenStream is preserved
// for failing <color> values.
TEST(CSSParsingUtilsTest, ConsumeColorRangePreservation) {
  const char* tests[] = {
      "color-mix(42deg)",
      "color-contrast(42deg)",
  };
  for (const char*& test : tests) {
    String input(test);
    SCOPED_TRACE(input);
    CSSParserTokenStream stream(input);
    EXPECT_EQ(nullptr, css_parsing_utils::ConsumeColor(stream, *MakeContext()));
    EXPECT_EQ(test, stream.RemainingText());
  }
}

TEST(CSSParsingUtilsTest, InternalPositionTryFallbacksInUAMode) {
  auto ConsumePositionTryFallbackForTest = [](String css_text,
                                              CSSParserMode mode) {
    CSSParserTokenStream stream(css_text);
    return css_parsing_utils::ConsumeSinglePositionTryFallback(
        stream, *MakeContext(mode));
  };

  struct {
    STACK_ALLOCATED();

   public:
    String css_text;
    bool allow_ua;
    bool allow_other;
  } expectations[]{
      {.css_text = "--foo", .allow_ua = true, .allow_other = true},
      {.css_text = "-foo", .allow_ua = false, .allow_other = false},
      {.css_text = "-internal-foo", .allow_ua = true, .allow_other = false},
  };
  for (auto& expectation : expectations) {
    EXPECT_EQ(ConsumePositionTryFallbackForTest(expectation.css_text,
                                                kHTMLStandardMode) != nullptr,
              expectation.allow_other);
    EXPECT_EQ(ConsumePositionTryFallbackForTest(expectation.css_text,
                                                kHTMLQuirksMode) != nullptr,
              expectation.allow_other);
    EXPECT_EQ(ConsumePositionTryFallbackForTest(expectation.css_text,
                                                kUASheetMode) != nullptr,
              expectation.allow_ua);
  }
}

// crbug.com/364340016
TEST(CSSParsingUtilsTest, ConsumePositionTryFallbacksInUAMode) {
  String css_text = "block-start span-inline-end";
  CSSParserTokenStream stream(css_text);
  CSSValue* value = css_parsing_utils::ConsumePositionTryFallbacks(
      stream, *MakeContext(kUASheetMode));
  ASSERT_TRUE(value);
  EXPECT_EQ("block-start span-inline-end", value->CssText());
}

}  // namespace
}  // namespace blink
