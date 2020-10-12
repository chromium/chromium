// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

typedef struct {
  const char* input;
  const int a;
  const int b;
} ANPlusBTestCase;

struct SelectorTestCase {
  // The input string to parse as a selector list.
  const char* input;

  // The expected serialization of the parsed selector list. If nullptr, then
  // the expected serialization is the same as the input value.
  //
  // For selector list that are expected to fail parsing, use the empty
  // string "".
  const char* expected = nullptr;
};

class SelectorParseTest : public ::testing::TestWithParam<SelectorTestCase> {};

TEST_P(SelectorParseTest, Parse) {
  auto param = GetParam();
  SCOPED_TRACE(param.input);
  CSSSelectorList list = css_test_helpers::ParseSelectorList(param.input);
  const char* expected = param.expected ? param.expected : param.input;
  EXPECT_EQ(String(expected), list.SelectorsText());
}

TEST(CSSSelectorParserTest, ValidANPlusB) {
  ANPlusBTestCase test_cases[] = {
      {"odd", 2, 1},
      {"OdD", 2, 1},
      {"even", 2, 0},
      {"EveN", 2, 0},
      {"0", 0, 0},
      {"8", 0, 8},
      {"+12", 0, 12},
      {"-14", 0, -14},

      {"0n", 0, 0},
      {"16N", 16, 0},
      {"-19n", -19, 0},
      {"+23n", 23, 0},
      {"n", 1, 0},
      {"N", 1, 0},
      {"+n", 1, 0},
      {"-n", -1, 0},
      {"-N", -1, 0},

      {"6n-3", 6, -3},
      {"-26N-33", -26, -33},
      {"n-18", 1, -18},
      {"+N-5", 1, -5},
      {"-n-7", -1, -7},

      {"0n+0", 0, 0},
      {"10n+5", 10, 5},
      {"10N +5", 10, 5},
      {"10n -5", 10, -5},
      {"N+6", 1, 6},
      {"n +6", 1, 6},
      {"+n -7", 1, -7},
      {"-N -8", -1, -8},
      {"-n+9", -1, 9},

      {"33N- 22", 33, -22},
      {"+n- 25", 1, -25},
      {"N- 46", 1, -46},
      {"n- 0", 1, 0},
      {"-N- 951", -1, -951},
      {"-n- 951", -1, -951},

      {"29N + 77", 29, 77},
      {"29n - 77", 29, -77},
      {"+n + 61", 1, 61},
      {"+N - 63", 1, -63},
      {"+n/**/- 48", 1, -48},
      {"-n + 81", -1, 81},
      {"-N - 88", -1, -88},

      {"3091970736n + 1", std::numeric_limits<int>::max(), 1},
      {"-3091970736n + 1", std::numeric_limits<int>::min(), 1},
      // B is calculated as +ve first, then negated.
      {"N- 3091970736", 1, -std::numeric_limits<int>::max()},
      {"N+ 3091970736", 1, std::numeric_limits<int>::max()},
  };

  for (auto test_case : test_cases) {
    SCOPED_TRACE(test_case.input);

    std::pair<int, int> ab;
    CSSTokenizer tokenizer(test_case.input);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    bool passed = CSSSelectorParser::ConsumeANPlusB(range, ab);
    EXPECT_TRUE(passed);
    EXPECT_EQ(test_case.a, ab.first);
    EXPECT_EQ(test_case.b, ab.second);
  }
}

TEST(CSSSelectorParserTest, InvalidANPlusB) {
  // Some of these have token range prefixes which are valid <an+b> and could
  // in theory be valid in consumeANPlusB, but this behaviour isn't needed
  // anywhere and not implemented.
  const char* test_cases[] = {
      " odd",     "+ n",     "3m+4",  "12n--34",  "12n- -34",
      "12n- +34", "23n-+43", "10n 5", "10n + +5", "10n + -5",
  };

  for (auto* test_case : test_cases) {
    SCOPED_TRACE(test_case);

    std::pair<int, int> ab;
    CSSTokenizer tokenizer(test_case);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    bool passed = CSSSelectorParser::ConsumeANPlusB(range, ab);
    EXPECT_FALSE(passed);
  }
}

TEST(CSSSelectorParserTest, ShadowDomPseudoInCompound) {
  const char* test_cases[][2] = {{"::content", "::content"},
                                 {".a::content", ".a::content"},
                                 {"::content.a", "::content.a"},
                                 {"::content.a.b", "::content.a.b"},
                                 {".a::content.b", ".a::content.b"},
                                 {"::content:not(#id)", "::content:not(#id)"}};

  for (auto** test_case : test_cases) {
    SCOPED_TRACE(test_case[0]);
    CSSTokenizer tokenizer(test_case[0]);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    CSSSelectorList list = CSSSelectorParser::ParseSelector(
        range,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        nullptr);
    EXPECT_EQ(test_case[1], list.SelectorsText());
  }
}

TEST(CSSSelectorParserTest, PseudoElementsInCompoundLists) {
  const char* test_cases[] = {":not(::before)",
                              ":not(::content)",
                              ":host(::before)",
                              ":host(::content)",
                              ":host-context(::before)",
                              ":host-context(::content)",
                              ":-webkit-any(::after, ::before)",
                              ":-webkit-any(::content, span)"};

  for (auto* test_case : test_cases) {
    CSSTokenizer tokenizer(test_case);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    CSSSelectorList list = CSSSelectorParser::ParseSelector(
        range,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        nullptr);
    EXPECT_FALSE(list.IsValid());
  }
}

TEST(CSSSelectorParserTest, ValidSimpleAfterPseudoElementInCompound) {
  const char* test_cases[] = {"::-webkit-volume-slider:hover",
                              "::selection:window-inactive",
                              "::-webkit-scrollbar:disabled",
                              "::-webkit-volume-slider:not(:hover)",
                              "::-webkit-scrollbar:not(:horizontal)",
                              "::slotted(span)::before",
                              "::slotted(div)::after"};

  for (auto* test_case : test_cases) {
    CSSTokenizer tokenizer(test_case);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    CSSSelectorList list = CSSSelectorParser::ParseSelector(
        range,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        nullptr);
    EXPECT_TRUE(list.IsValid());
  }
}

TEST(CSSSelectorParserTest, InvalidSimpleAfterPseudoElementInCompound) {
  const char* test_cases[] = {
      "::before#id",
      "::after:hover",
      ".class::content::before",
      "::shadow.class",
      "::selection:window-inactive::before",
      "::-webkit-volume-slider.class",
      "::before:not(.a)",
      "::shadow:not(::after)",
      "::-webkit-scrollbar:vertical:not(:first-child)",
      "video::-webkit-media-text-track-region-container.scrolling",
      "div ::before.a",
      "::slotted(div):hover",
      "::slotted(div)::slotted(span)",
      "::slotted(div)::before:hover",
      "::slotted(div)::before::slotted(span)",
      "::slotted(*)::first-letter",
      "::slotted(.class)::first-line",
      "::slotted([attr])::-webkit-scrollbar"};

  for (auto* test_case : test_cases) {
    CSSTokenizer tokenizer(test_case);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    CSSSelectorList list = CSSSelectorParser::ParseSelector(
        range,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        nullptr);
    EXPECT_FALSE(list.IsValid());
  }
}

TEST(CSSSelectorParserTest, WorkaroundForInvalidCustomPseudoInUAStyle) {
  // See crbug.com/578131
  const char* test_cases[] = {
      "video::-webkit-media-text-track-region-container.scrolling",
      "input[type=\"range\" i]::-webkit-media-slider-container > div"};

  for (auto* test_case : test_cases) {
    CSSTokenizer tokenizer(test_case);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    CSSSelectorList list = CSSSelectorParser::ParseSelector(
        range,
        MakeGarbageCollected<CSSParserContext>(
            kUASheetMode, SecureContextMode::kInsecureContext),
        nullptr);
    EXPECT_TRUE(list.IsValid());
  }
}

TEST(CSSSelectorParserTest, ValidPseudoElementInNonRightmostCompound) {
  const char* test_cases[] = {"::content *", "::content div::before"};

  for (auto* test_case : test_cases) {
    CSSTokenizer tokenizer(test_case);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    CSSSelectorList list = CSSSelectorParser::ParseSelector(
        range,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        nullptr);
    EXPECT_TRUE(list.IsValid());
  }
}

TEST(CSSSelectorParserTest, InvalidPseudoElementInNonRightmostCompound) {
  const char* test_cases[] = {"::-webkit-volume-slider *", "::before *",
                              "::-webkit-scrollbar *", "::cue *",
                              "::selection *"};

  for (auto* test_case : test_cases) {
    CSSTokenizer tokenizer(test_case);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    CSSSelectorList list = CSSSelectorParser::ParseSelector(
        range,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        nullptr);
    EXPECT_FALSE(list.IsValid());
  }
}

TEST(CSSSelectorParserTest, UnresolvedNamespacePrefix) {
  const char* test_cases[] = {"ns|div", "div ns|div", "div ns|div "};

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);

  for (auto* test_case : test_cases) {
    CSSTokenizer tokenizer(test_case);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    CSSSelectorList list =
        CSSSelectorParser::ParseSelector(range, context, sheet);
    EXPECT_FALSE(list.IsValid());
  }
}

TEST(CSSSelectorParserTest, UnexpectedPipe) {
  const char* test_cases[] = {"div | .c", "| div", " | div"};

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);

  for (auto* test_case : test_cases) {
    CSSTokenizer tokenizer(test_case);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    CSSSelectorList list =
        CSSSelectorParser::ParseSelector(range, context, sheet);
    EXPECT_FALSE(list.IsValid());
  }
}

TEST(CSSSelectorParserTest, SerializedUniversal) {
  const char* test_cases[][2] = {
      {"*::-webkit-volume-slider", "::-webkit-volume-slider"},
      {"*::cue(i)", "::cue(i)"},
      {"*:host-context(.x)", "*:host-context(.x)"},
      {"*:host", "*:host"},
      {"|*::-webkit-volume-slider", "|*::-webkit-volume-slider"},
      {"|*::cue(i)", "|*::cue(i)"},
      {"*|*::-webkit-volume-slider", "::-webkit-volume-slider"},
      {"*|*::cue(i)", "::cue(i)"},
      {"ns|*::-webkit-volume-slider", "ns|*::-webkit-volume-slider"},
      {"ns|*::cue(i)", "ns|*::cue(i)"}};

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
  sheet->ParserAddNamespace("ns", "http://ns.org");

  for (auto** test_case : test_cases) {
    SCOPED_TRACE(test_case[0]);
    CSSTokenizer tokenizer(test_case[0]);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    CSSSelectorList list =
        CSSSelectorParser::ParseSelector(range, context, sheet);
    EXPECT_TRUE(list.IsValid());
    EXPECT_EQ(test_case[1], list.SelectorsText());
  }
}

TEST(CSSSelectorParserTest, AttributeSelectorUniversalInvalid) {
  const char* test_cases[] = {"[*]", "[*|*]"};

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);

  for (auto* test_case : test_cases) {
    SCOPED_TRACE(test_case);
    CSSTokenizer tokenizer(test_case);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    CSSSelectorList list =
        CSSSelectorParser::ParseSelector(range, context, sheet);
    EXPECT_FALSE(list.IsValid());
  }
}

TEST(CSSSelectorParserTest, InternalPseudo) {
  const char* test_cases[] = {"::-internal-whatever",
                              "::-internal-media-controls-text-track-list",
                              ":-internal-is-html",
                              ":-internal-list-box",
                              ":-internal-multi-select-focus",
                              ":-internal-shadow-host-has-appearance",
                              ":-internal-spatial-navigation-focus",
                              ":-internal-spatial-navigation-interest",
                              ":-internal-video-persistent",
                              ":-internal-video-persistent-ancestor"};
  for (auto* test_case : test_cases) {
    SCOPED_TRACE(test_case);
    CSSTokenizer tokenizer(test_case);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);

    CSSSelectorList author_list = CSSSelectorParser::ParseSelector(
        range,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        nullptr);
    EXPECT_FALSE(author_list.IsValid());

    CSSSelectorList ua_list = CSSSelectorParser::ParseSelector(
        range,
        MakeGarbageCollected<CSSParserContext>(
            kUASheetMode, SecureContextMode::kInsecureContext),
        nullptr);
    EXPECT_TRUE(ua_list.IsValid());
  }
}

// Pseudo-elements are not valid within :is() as per the spec:
// https://drafts.csswg.org/selectors-4/#matches
static const SelectorTestCase invalid_pseudo_is_argments_data[] = {
    // clang-format off
    {":is(::-webkit-progress-bar)", ":is()"},
    {":is(::-webkit-progress-value)", ":is()"},
    {":is(::-webkit-slider-runnable-track)", ":is()"},
    {":is(::-webkit-slider-thumb)", ":is()"},
    {":is(::after)", ":is()"},
    {":is(::backdrop)", ":is()"},
    {":is(::before)", ":is()"},
    {":is(::cue)", ":is()"},
    {":is(::first-letter)", ":is()"},
    {":is(::first-line)", ":is()"},
    {":is(::grammar-error)", ":is()"},
    {":is(::marker)", ":is()"},
    {":is(::placeholder)", ":is()"},
    {":is(::selection)", ":is()"},
    {":is(::slotted)", ":is()"},
    {":is(::spelling-error)", ":is()"},
    {":is(:after)", ":is()"},
    {":is(:before)", ":is()"},
    {":is(:cue)", ":is()"},
    {":is(:first-letter)", ":is()"},
    {":is(:first-line)", ":is()"},
    // clang-format on
};

INSTANTIATE_TEST_SUITE_P(InvalidPseudoIsArguments,
                         SelectorParseTest,
                         testing::ValuesIn(invalid_pseudo_is_argments_data));

// To reduce complexity, ShadowDOM v0 features are not supported in
// combination with :is/:where.
static const SelectorTestCase shadow_v0_with_is_where_data[] = {
    // clang-format off
    {":is(.a) ::content", ""},
    {":is(.a /deep/ .b)", ":is()"},
    {":is(::content)", ":is()"},
    {":is(::shadow)", ":is()"},
    {":is(::content .a)", ":is()"},
    {":is(::shadow .b)", ":is()"},
    {":is(.a)::shadow", ""},
    {":is(.a) ::content", ""},
    {":is(.a) ::shadow", ""},
    {"::content :is(.a)", ""},
    {"::shadow :is(.a)", ""},
    {":is(.a) /deep/ .b", ""},
    {":.a /deep/ :is(.b)", ""},
    {":where(.a /deep/ .b)", ":where()"},
    {":where(.a) ::shadow", ""},
    // clang-format on
};

INSTANTIATE_TEST_SUITE_P(ShadowDomV0WithIsAndWhere,
                         SelectorParseTest,
                         testing::ValuesIn(shadow_v0_with_is_where_data));

static const SelectorTestCase is_where_nesting_data[] = {
    // clang-format off
    // These pseudos only accept compound selectors:
    {"::slotted(:is(.a .b))", "::slotted(:is())"},
    {"::slotted(:is(.a + .b))", "::slotted(:is())"},
    {"::slotted(:is(.a, .b + .c))", "::slotted(:is(.a))"},
    {":host(:is(.a .b))", ":host(:is())"},
    {":host(:is(.a + .b))", ":host(:is())"},
    {":host(:is(.a, .b + .c))", ":host(:is(.a))"},
    {":host-context(:is(.a .b))", ":host-context(:is())"},
    {":host-context(:is(.a + .b))", ":host-context(:is())"},
    {":host-context(:is(.a, .b + .c))", ":host-context(:is(.a))"},
    {"::cue(:is(.a .b))", "::cue(:is())"},
    {"::cue(:is(.a + .b))", "::cue(:is())"},
    {"::cue(:is(.a, .b + .c))", "::cue(:is(.a))"},
    // Only user-action pseudos + :state() are allowed after kPseudoPart:
    {"::part(foo):is(.a)", "::part(foo):is()"},
    {"::part(foo):is(.a:hover)", "::part(foo):is()"},
    {"::part(foo):is(:hover.a)", "::part(foo):is()"},
    {"::part(foo):is(:hover + .a)", "::part(foo):is()"},
    {"::part(foo):is(.a + :hover)", "::part(foo):is()"},
    {"::part(foo):is(:hover:enabled)", "::part(foo):is()"},
    {"::part(foo):is(:enabled:hover)", "::part(foo):is()"},
    {"::part(foo):is(:hover, :where(.a))",
     "::part(foo):is(:hover, :where())"},
    {"::part(foo):is(:hover, .a)", "::part(foo):is(:hover)"},
    {"::part(foo):is(:state(bar), .a)", "::part(foo):is(:state(bar))"},
    {"::part(foo):is(:enabled)", "::part(foo):is()"},
    // Only scrollbar pseudos after kPseudoScrollbar:
    {"::-webkit-scrollbar:is(:focus)", "::-webkit-scrollbar:is()"},
    // Only :window-inactive after kPseudoSelection:
    {"::selection:is(:focus)", "::selection:is()"},
    // Only user-action pseudos after webkit pseudos:
    {"::-webkit-input-placeholder:is(:enabled)",
     "::-webkit-input-placeholder:is()"},
    {"::-webkit-input-placeholder:is(:not(:enabled))",
     "::-webkit-input-placeholder:is()"},

    // Valid selectors:
    {":is(.a, .b)"},
    {":is(.a .b, .c)"},
    {":is(.a :is(.b .c), .d)"},
    {":is(.a :where(.b .c), .d)"},
    {":where(.a :is(.b .c), .d)"},
    {":not(:is(.a))"},
    {":not(:is(.a, .b))"},
    {":not(:is(.a + .b, .c .d))"},
    {":not(:where(:not(.a)))"},
    {"::slotted(:is(.a))"},
    {"::slotted(:is(div.a))"},
    {"::slotted(:is(.a, .b))"},
    {":host(:is(.a))"},
    {":host(:is(div.a))"},
    {":host(:is(.a, .b))"},
    {":host-context(:is(.a))"},
    {":host-context(:is(div.a))"},
    {":host-context(:is(.a, .b))"},
    {"::cue(:is(.a))"},
    {"::cue(:is(div.a))"},
    {"::cue(:is(.a, .b))"},
    {"::part(foo):is(:hover)"},
    {"::part(foo):is(:hover:focus)"},
    {"::part(foo):is(:is(:hover))"},
    {"::part(foo):is(:focus, :hover)"},
    {"::part(foo):is(:focus, :is(:hover))"},
    {"::part(foo):is(:focus, :state(bar))"},
    {"::-webkit-scrollbar:is(:enabled)"},
    {"::selection:is(:window-inactive)"},
    {"::-webkit-input-placeholder:is(:hover)"},
    {"::-webkit-input-placeholder:is(:not(:hover))"},
    {"::-webkit-input-placeholder:where(:hover)"},
    {"::-webkit-input-placeholder:is()"},
    {"::-webkit-input-placeholder:is(:where(:hover))"},
    // clang-format on
};

INSTANTIATE_TEST_SUITE_P(NestedSelectorValidity,
                         SelectorParseTest,
                         testing::ValuesIn(is_where_nesting_data));

static const SelectorTestCase is_where_forgiving_data[] = {
    // clang-format off
    {":is():where()"},
    {":is(.a, .b):where(.c)"},
    {":is(.a, :unknown, .b)", ":is(.a, .b)"},
    {":where(.a, :unknown, .b)", ":where(.a, .b)"},
    {":is(.a, :unknown)", ":is(.a)"},
    {":is(:unknown, .a)", ":is(.a)"},
    {":is(:unknown)", ":is()"},
    {":is(:unknown, :where(.a))", ":is(:where(.a))"},
    {":is(:unknown, :where(:unknown))", ":is(:where())"},
    {":is(.a, :is(.b, :unknown), .c)", ":is(.a, :is(.b), .c)"},
    {":host(:is(.a, .b + .c, .d))", ":host(:is(.a, .d))"},
    {":is(,,  ,, )", ":is()"},
    {":is(.a,,,,)", ":is(.a)"},
    {":is(,,.a,,)", ":is(.a)"},
    {":is(,,,,.a)", ":is(.a)"},
    {":is(@x {,.b,}, .a)", ":is(.a)"},
    {":is({,.b,} @x, .a)", ":is(.a)"},
    {":is((@x), .a)", ":is(.a)"},
    {":is((.b), .a)", ":is(.a)"},
    // clang-format on
};

INSTANTIATE_TEST_SUITE_P(IsWhereForgiving,
                         SelectorParseTest,
                         testing::ValuesIn(is_where_forgiving_data));
namespace {

const auto TagLocalName = [](const CSSSelector* selector) {
  return selector->TagQName().LocalName();
};

const auto AttributeLocalName = [](const CSSSelector* selector) {
  return selector->Attribute().LocalName();
};

const auto SelectorValue = [](const CSSSelector* selector) {
  return selector->Value();
};

struct ASCIILowerTestCase {
  const char* input;
  const char16_t* expected;
  std::function<AtomicString(const CSSSelector*)> getter;
};

}  // namespace

TEST(CSSSelectorParserTest, ASCIILowerHTMLStrict) {
  const ASCIILowerTestCase test_cases[] = {
      {"\\212a bd", u"\u212abd", TagLocalName},
      {"[\\212alass]", u"\u212alass", AttributeLocalName},
      {".\\212alass", u"\u212alass", SelectorValue},
      {"#\\212alass", u"\u212alass", SelectorValue}};

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);

  for (auto test_case : test_cases) {
    SCOPED_TRACE(test_case.input);
    CSSTokenizer tokenizer(test_case.input);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    CSSSelectorList list =
        CSSSelectorParser::ParseSelector(range, context, sheet);
    EXPECT_TRUE(list.IsValid());
    const CSSSelector* selector = list.First();
    ASSERT_TRUE(selector);
    EXPECT_EQ(AtomicString(test_case.expected), test_case.getter(selector));
  }
}

TEST(CSSSelectorParserTest, ASCIILowerHTMLQuirks) {
  const ASCIILowerTestCase test_cases[] = {
      {"\\212a bd", u"\u212abd", TagLocalName},
      {"[\\212alass]", u"\u212alass", AttributeLocalName},
      {".\\212aLASS", u"\u212alass", SelectorValue},
      {"#\\212aLASS", u"\u212alass", SelectorValue}};

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLQuirksMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);

  for (auto test_case : test_cases) {
    SCOPED_TRACE(test_case.input);
    CSSTokenizer tokenizer(test_case.input);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    CSSSelectorList list =
        CSSSelectorParser::ParseSelector(range, context, sheet);
    EXPECT_TRUE(list.IsValid());
    const CSSSelector* selector = list.First();
    ASSERT_TRUE(selector);
    EXPECT_EQ(AtomicString(test_case.expected), test_case.getter(selector));
  }
}

TEST(CSSSelectorParserTest, ShadowPartPseudoElementValid) {
  const char* test_cases[] = {"::part(ident)",
                              "host::part(ident)",
                              "host::part(ident):hover"};

  for (auto* test_case : test_cases) {
    SCOPED_TRACE(test_case);
    CSSTokenizer tokenizer(test_case);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    CSSSelectorList list = CSSSelectorParser::ParseSelector(
        range,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        nullptr);
    EXPECT_EQ(test_case, list.SelectorsText());
  }
}

TEST(CSSSelectorParserTest, ShadowPartAndBeforeAfterPseudoElementValid) {
  const char* test_cases[] = {
      "::part(ident)::before",       "::part(ident)::after",
      "::part(ident)::placeholder",  "::part(ident)::first-line",
      "::part(ident)::first-letter", "::part(ident)::selection"};

  for (auto* test_case : test_cases) {
    SCOPED_TRACE(test_case);
    CSSTokenizer tokenizer(test_case);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    CSSSelectorList list = CSSSelectorParser::ParseSelector(
        range,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        nullptr);
    EXPECT_EQ(test_case, list.SelectorsText());
  }
}

TEST(CSSSelectorParserTest, UseCountShadowPseudo) {
  auto dummy_holder = std::make_unique<DummyPageHolder>(IntSize(500, 500));
  Document* doc = &dummy_holder->GetDocument();
  Page::InsertOrdinaryPageForTesting(&dummy_holder->GetPage());
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kSecureContext,
      CSSParserContext::kLiveProfile, doc);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);

  auto ExpectCount = [doc, context, sheet](const char* selector,
                                           WebFeature feature) {
    EXPECT_FALSE(doc->IsUseCounted(feature));

    CSSTokenizer tokenizer(selector);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    CSSSelectorParser::ParseSelector(range, context, sheet);

    EXPECT_TRUE(doc->IsUseCounted(feature));
  };

  ExpectCount("::cue", WebFeature::kCSSSelectorCue);
  ExpectCount("::-internal-media-controls-overlay-cast-button",
              WebFeature::kCSSSelectorInternalMediaControlsOverlayCastButton);
  ExpectCount("::-webkit-calendar-picker-indicator",
              WebFeature::kCSSSelectorWebkitCalendarPickerIndicator);
  ExpectCount("::-webkit-clear-button",
              WebFeature::kCSSSelectorWebkitClearButton);
  ExpectCount("::-webkit-color-swatch",
              WebFeature::kCSSSelectorWebkitColorSwatch);
  ExpectCount("::-webkit-color-swatch-wrapper",
              WebFeature::kCSSSelectorWebkitColorSwatchWrapper);
  ExpectCount("::-webkit-date-and-time-value",
              WebFeature::kCSSSelectorWebkitDateAndTimeValue);
  ExpectCount("::-webkit-datetime-edit",
              WebFeature::kCSSSelectorWebkitDatetimeEdit);
  ExpectCount("::-webkit-datetime-edit-ampm-field",
              WebFeature::kCSSSelectorWebkitDatetimeEditAmpmField);
  ExpectCount("::-webkit-datetime-edit-day-field",
              WebFeature::kCSSSelectorWebkitDatetimeEditDayField);
  ExpectCount("::-webkit-datetime-edit-fields-wrapper",
              WebFeature::kCSSSelectorWebkitDatetimeEditFieldsWrapper);
  ExpectCount("::-webkit-datetime-edit-hour-field",
              WebFeature::kCSSSelectorWebkitDatetimeEditHourField);
  ExpectCount("::-webkit-datetime-edit-millisecond-field",
              WebFeature::kCSSSelectorWebkitDatetimeEditMillisecondField);
  ExpectCount("::-webkit-datetime-edit-minute-field",
              WebFeature::kCSSSelectorWebkitDatetimeEditMinuteField);
  ExpectCount("::-webkit-datetime-edit-month-field",
              WebFeature::kCSSSelectorWebkitDatetimeEditMonthField);
  ExpectCount("::-webkit-datetime-edit-second-field",
              WebFeature::kCSSSelectorWebkitDatetimeEditSecondField);
  ExpectCount("::-webkit-datetime-edit-text",
              WebFeature::kCSSSelectorWebkitDatetimeEditText);
  ExpectCount("::-webkit-datetime-edit-week-field",
              WebFeature::kCSSSelectorWebkitDatetimeEditWeekField);
  ExpectCount("::-webkit-datetime-edit-year-field",
              WebFeature::kCSSSelectorWebkitDatetimeEditYearField);
  ExpectCount("::-webkit-details-marker",
              WebFeature::kCSSSelectorWebkitDetailsMarker);
  ExpectCount("::-webkit-file-upload-button",
              WebFeature::kCSSSelectorWebkitFileUploadButton);
  ExpectCount("::-webkit-inner-spin-button",
              WebFeature::kCSSSelectorWebkitInnerSpinButton);
  ExpectCount("::-webkit-input-placeholder",
              WebFeature::kCSSSelectorWebkitInputPlaceholder);
  ExpectCount("::-webkit-media-controls",
              WebFeature::kCSSSelectorWebkitMediaControls);
  ExpectCount("::-webkit-media-controls-current-time-display",
              WebFeature::kCSSSelectorWebkitMediaControlsCurrentTimeDisplay);
  ExpectCount("::-webkit-media-controls-enclosure",
              WebFeature::kCSSSelectorWebkitMediaControlsEnclosure);
  ExpectCount("::-webkit-media-controls-fullscreen-button",
              WebFeature::kCSSSelectorWebkitMediaControlsFullscreenButton);
  ExpectCount("::-webkit-media-controls-mute-button",
              WebFeature::kCSSSelectorWebkitMediaControlsMuteButton);
  ExpectCount("::-webkit-media-controls-overlay-enclosure",
              WebFeature::kCSSSelectorWebkitMediaControlsOverlayEnclosure);
  ExpectCount("::-webkit-media-controls-overlay-play-button",
              WebFeature::kCSSSelectorWebkitMediaControlsOverlayPlayButton);
  ExpectCount("::-webkit-media-controls-panel",
              WebFeature::kCSSSelectorWebkitMediaControlsPanel);
  ExpectCount("::-webkit-media-controls-play-button",
              WebFeature::kCSSSelectorWebkitMediaControlsPlayButton);
  ExpectCount("::-webkit-media-controls-timeline",
              WebFeature::kCSSSelectorWebkitMediaControlsTimeline);
  ExpectCount("::-webkit-media-controls-timeline-container",
              WebFeature::kCSSSelectorWebkitMediaControlsTimelineContainer);
  ExpectCount("::-webkit-media-controls-time-remaining-display",
              WebFeature::kCSSSelectorWebkitMediaControlsTimeRemainingDisplay);
  ExpectCount(
      "::-webkit-media-controls-toggle-closed-captions-button",
      WebFeature::kCSSSelectorWebkitMediaControlsToggleClosedCaptionsButton);
  ExpectCount("::-webkit-media-controls-volume-slider",
              WebFeature::kCSSSelectorWebkitMediaControlsVolumeSlider);
  ExpectCount("::-webkit-media-slider-container",
              WebFeature::kCSSSelectorWebkitMediaSliderContainer);
  ExpectCount("::-webkit-media-slider-thumb",
              WebFeature::kCSSSelectorWebkitMediaSliderThumb);
  ExpectCount("::-webkit-media-text-track-container",
              WebFeature::kCSSSelectorWebkitMediaTextTrackContainer);
  ExpectCount("::-webkit-media-text-track-display",
              WebFeature::kCSSSelectorWebkitMediaTextTrackDisplay);
  ExpectCount("::-webkit-media-text-track-region",
              WebFeature::kCSSSelectorWebkitMediaTextTrackRegion);
  ExpectCount("::-webkit-media-text-track-region-container",
              WebFeature::kCSSSelectorWebkitMediaTextTrackRegionContainer);
  ExpectCount("::-webkit-meter-bar", WebFeature::kCSSSelectorWebkitMeterBar);
  ExpectCount("::-webkit-meter-even-less-good-value",
              WebFeature::kCSSSelectorWebkitMeterEvenLessGoodValue);
  ExpectCount("::-webkit-meter-inner-element",
              WebFeature::kCSSSelectorWebkitMeterInnerElement);
  ExpectCount("::-webkit-meter-optimum-value",
              WebFeature::kCSSSelectorWebkitMeterOptimumValue);
  ExpectCount("::-webkit-meter-suboptimum-value",
              WebFeature::kCSSSelectorWebkitMeterSuboptimumValue);
  ExpectCount("::-webkit-progress-bar",
              WebFeature::kCSSSelectorWebkitProgressBar);
  ExpectCount("::-webkit-progress-inner-element",
              WebFeature::kCSSSelectorWebkitProgressInnerElement);
  ExpectCount("::-webkit-progress-value",
              WebFeature::kCSSSelectorWebkitProgressValue);
  ExpectCount("::-webkit-search-cancel-button",
              WebFeature::kCSSSelectorWebkitSearchCancelButton);
  ExpectCount("::-webkit-slider-container",
              WebFeature::kCSSSelectorWebkitSliderContainer);
  ExpectCount("::-webkit-slider-runnable-track",
              WebFeature::kCSSSelectorWebkitSliderRunnableTrack);
  ExpectCount("::-webkit-slider-thumb",
              WebFeature::kCSSSelectorWebkitSliderThumb);
  ExpectCount("::-webkit-textfield-decoration-container",
              WebFeature::kCSSSelectorWebkitTextfieldDecorationContainer);
  ExpectCount("::-webkit-unrecognized",
              WebFeature::kCSSSelectorWebkitUnknownPseudo);
}

TEST(CSSSelectorParserTest, ImplicitShadowCrossingCombinators) {
  struct ShadowCombinatorTest {
    const char* input;
    Vector<std::pair<AtomicString, CSSSelector::RelationType>> expectation;
  };

  const ShadowCombinatorTest test_cases[] = {
      {
          "*::placeholder",
          {
              {"placeholder", CSSSelector::kShadowPseudo},
              {g_null_atom, CSSSelector::kSubSelector},
          },
      },
      {
          "div::slotted(*)",
          {
              {"slotted", CSSSelector::kShadowSlot},
              {"div", CSSSelector::kSubSelector},
          },
      },
      {
          "::slotted(*)::placeholder",
          {
              {"placeholder", CSSSelector::kShadowPseudo},
              {"slotted", CSSSelector::kShadowSlot},
              {g_null_atom, CSSSelector::kSubSelector},
          },
      },
      {
          "span::part(my-part)",
          {
              {"part", CSSSelector::kShadowPart},
              {"span", CSSSelector::kSubSelector},
          },
      },
      {
          "video::-webkit-media-controls",
          {
              {"-webkit-media-controls", CSSSelector::kShadowPseudo},
              {"video", CSSSelector::kSubSelector},
          },
      },
  };

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);

  for (auto test_case : test_cases) {
    SCOPED_TRACE(test_case.input);
    CSSTokenizer tokenizer(test_case.input);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    CSSSelectorList list =
        CSSSelectorParser::ParseSelector(range, context, sheet);
    EXPECT_TRUE(list.IsValid());
    const CSSSelector* selector = list.First();
    for (auto sub_expectation : test_case.expectation) {
      ASSERT_TRUE(selector);
      AtomicString selector_value = selector->Match() == CSSSelector::kTag
                                        ? selector->TagQName().LocalName()
                                        : selector->Value();
      EXPECT_EQ(sub_expectation.first, selector_value);
      EXPECT_EQ(sub_expectation.second, selector->Relation());
      selector = selector->TagHistory();
    }
    EXPECT_FALSE(selector);
  }
}

}  // namespace blink
