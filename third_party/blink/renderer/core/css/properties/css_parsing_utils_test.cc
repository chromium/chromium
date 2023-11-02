// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

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
  auto tokens = CSSTokenizer(target).TokenizeToEOF();
  CSSParserTokenRange range(tokens);
  return ConsumeAngle(range, *MakeContext(), absl::nullopt)->ComputeDegrees();
}

double ConsumeAngleValue(String target, double min, double max) {
  auto tokens = CSSTokenizer(target).TokenizeToEOF();
  CSSParserTokenRange range(tokens);
  return ConsumeAngle(range, *MakeContext(), absl::nullopt, min, max)
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

TEST(CSSParsingUtilsTest, AtIdent_Range) {
  String text = "foo,bar,10px";
  auto tokens = CSSTokenizer(text).TokenizeToEOF();
  CSSParserTokenRange range(tokens);
  EXPECT_FALSE(AtIdent(range.Consume(), "bar"));  // foo
  EXPECT_FALSE(AtIdent(range.Consume(), "bar"));  // ,
  EXPECT_TRUE(AtIdent(range.Consume(), "bar"));   // bar
  EXPECT_FALSE(AtIdent(range.Consume(), "bar"));  // ,
  EXPECT_FALSE(AtIdent(range.Consume(), "bar"));  // 10px
  EXPECT_FALSE(AtIdent(range.Consume(), "bar"));  // EOF
}

TEST(CSSParsingUtilsTest, AtIdent_Stream) {
  String text = "foo,bar,10px";
  CSSTokenizer tokenizer(text);
  CSSParserTokenStream stream(tokenizer);
  EXPECT_FALSE(AtIdent(stream.Consume(), "bar"));  // foo
  EXPECT_FALSE(AtIdent(stream.Consume(), "bar"));  // ,
  EXPECT_TRUE(AtIdent(stream.Consume(), "bar"));   // bar
  EXPECT_FALSE(AtIdent(stream.Consume(), "bar"));  // ,
  EXPECT_FALSE(AtIdent(stream.Consume(), "bar"));  // 10px
  EXPECT_FALSE(AtIdent(stream.Consume(), "bar"));  // EOF
}

TEST(CSSParsingUtilsTest, ConsumeIfIdent_Range) {
  String text = "foo,bar,10px";
  auto tokens = CSSTokenizer(text).TokenizeToEOF();
  CSSParserTokenRange range(tokens);
  EXPECT_TRUE(AtIdent(range.Peek(), "foo"));
  EXPECT_FALSE(ConsumeIfIdent(range, "bar"));
  EXPECT_TRUE(AtIdent(range.Peek(), "foo"));
  EXPECT_TRUE(ConsumeIfIdent(range, "foo"));
  EXPECT_EQ(kCommaToken, range.Peek().GetType());
}

TEST(CSSParsingUtilsTest, ConsumeIfIdent_Stream) {
  String text = "foo,bar,10px";
  CSSTokenizer tokenizer(text);
  CSSParserTokenStream stream(tokenizer);
  EXPECT_TRUE(AtIdent(stream.Peek(), "foo"));
  EXPECT_FALSE(ConsumeIfIdent(stream, "bar"));
  EXPECT_TRUE(AtIdent(stream.Peek(), "foo"));
  EXPECT_TRUE(ConsumeIfIdent(stream, "foo"));
  EXPECT_EQ(kCommaToken, stream.Peek().GetType());
}

TEST(CSSParsingUtilsTest, AtDelimiter_Range) {
  String text = "foo,<,10px";
  auto tokens = CSSTokenizer(text).TokenizeToEOF();
  CSSParserTokenRange range(tokens);
  EXPECT_FALSE(AtDelimiter(range.Consume(), '<'));  // foo
  EXPECT_FALSE(AtDelimiter(range.Consume(), '<'));  // ,
  EXPECT_TRUE(AtDelimiter(range.Consume(), '<'));   // <
  EXPECT_FALSE(AtDelimiter(range.Consume(), '<'));  // ,
  EXPECT_FALSE(AtDelimiter(range.Consume(), '<'));  // 10px
  EXPECT_FALSE(AtDelimiter(range.Consume(), '<'));  // EOF
}

TEST(CSSParsingUtilsTest, AtDelimiter_Stream) {
  String text = "foo,<,10px";
  CSSTokenizer tokenizer(text);
  CSSParserTokenStream stream(tokenizer);
  EXPECT_FALSE(AtDelimiter(stream.Consume(), '<'));  // foo
  EXPECT_FALSE(AtDelimiter(stream.Consume(), '<'));  // ,
  EXPECT_TRUE(AtDelimiter(stream.Consume(), '<'));   // <
  EXPECT_FALSE(AtDelimiter(stream.Consume(), '<'));  // ,
  EXPECT_FALSE(AtDelimiter(stream.Consume(), '<'));  // 10px
  EXPECT_FALSE(AtDelimiter(stream.Consume(), '<'));  // EOF
}

TEST(CSSParsingUtilsTest, ConsumeIfDelimiter_Range) {
  String text = "<,=,10px";
  auto tokens = CSSTokenizer(text).TokenizeToEOF();
  CSSParserTokenRange range(tokens);
  EXPECT_TRUE(AtDelimiter(range.Peek(), '<'));
  EXPECT_FALSE(ConsumeIfDelimiter(range, '='));
  EXPECT_TRUE(AtDelimiter(range.Peek(), '<'));
  EXPECT_TRUE(ConsumeIfDelimiter(range, '<'));
  EXPECT_EQ(kCommaToken, range.Peek().GetType());
}

TEST(CSSParsingUtilsTest, ConsumeIfDelimiter_Stream) {
  String text = "<,=,10px";
  CSSTokenizer tokenizer(text);
  CSSParserTokenStream stream(tokenizer);
  EXPECT_TRUE(AtDelimiter(stream.Peek(), '<'));
  EXPECT_FALSE(ConsumeIfDelimiter(stream, '='));
  EXPECT_TRUE(AtDelimiter(stream.Peek(), '<'));
  EXPECT_TRUE(ConsumeIfDelimiter(stream, '<'));
  EXPECT_EQ(kCommaToken, stream.Peek().GetType());
}

TEST(CSSParsingUtilsTest, ConsumeAnyValue) {
  struct {
    // The input string to parse as <any-value>.
    const char* input;
    // The expected result from ConsumeAnyValue.
    bool expected;
    // The serialization of the tokens remaining in the range.
    const char* remainder;
  } tests[] = {
      {"1", true, ""},
      {"1px", true, ""},
      {"1px ", true, ""},
      {"ident", true, ""},
      {"(([ident]))", true, ""},
      {" ( ( 1 ) ) ", true, ""},
      {"rgb(1, 2, 3)", true, ""},
      {"rgb(1, 2, 3", true, ""},
      {"!!!;;;", true, ""},
      {"asdf)", false, ")"},
      {")asdf", false, ")asdf"},
      {"(ab)cd) e", false, ") e"},
      {"(as]df) e", false, " e"},
      {"(a b [ c { d ) e } f ] g h) i", false, " i"},
      {"a url(() b", false, "url(() b"},
  };

  for (const auto& test : tests) {
    String input(test.input);
    SCOPED_TRACE(input);
    auto tokens = CSSTokenizer(input).TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    EXPECT_EQ(test.expected, css_parsing_utils::ConsumeAnyValue(range));
    EXPECT_EQ(String(test.remainder), range.Serialize());
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
    auto tokens = CSSTokenizer(expectation.css_text).TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    EXPECT_EQ(css_parsing_utils::IsDashedIdent(range.Peek()),
              expectation.is_dashed_indent);
  }
}

TEST(CSSParsingUtilsTest, NoSystemColor) {
  auto ConsumeColorForTest =
      [](String css_text,
         css_parsing_utils::AllowedColorKeywords allowed_keywords) {
        auto tokens = CSSTokenizer(css_text).TokenizeToEOF();
        CSSParserTokenRange range(tokens);
        return ConsumeColor(range, *MakeContext(), false, allowed_keywords);
      };
  using css_parsing_utils::AllowedColorKeywords;

  struct {
    STACK_ALLOCATED();

   public:
    String css_text;
    CSSIdentifierValue* allowed_expectation;
    CSSIdentifierValue* not_allowed_expectation;
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
  };
  for (auto& expectation : expectations) {
    EXPECT_EQ(ConsumeColorForTest(expectation.css_text,
                                  AllowedColorKeywords::kAllowSystemColor),
              expectation.allowed_expectation);
    EXPECT_EQ(ConsumeColorForTest(expectation.css_text,
                                  AllowedColorKeywords::kNoSystemColor),
              expectation.not_allowed_expectation);
  }
}

TEST(CSSParsingUtilsTest, InternalColorsOnlyAllowedInUaMode) {
  auto ConsumeColorForTest = [](String css_text, CSSParserMode mode) {
    auto tokens = CSSTokenizer(css_text).TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    return css_parsing_utils::ConsumeColor(range, *MakeContext(mode));
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

}  // namespace
}  // namespace blink
