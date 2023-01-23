// Copyright 2015 The Chromium Authors
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
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
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

class SelectorParseTestForHasForgivingParsing
    : public ::testing::TestWithParam<SelectorTestCase> {};

TEST_P(SelectorParseTest, Parse) {
  auto param = GetParam();
  SCOPED_TRACE(param.input);
  CSSSelectorList* list = css_test_helpers::ParseSelectorList(param.input);
  const char* expected = param.expected ? param.expected : param.input;
  EXPECT_EQ(String(expected), list->SelectorsText());
}

TEST_P(SelectorParseTestForHasForgivingParsing, Parse) {
  ScopedCSSPseudoHasNonForgivingParsingForTest scoped_feature(false);

  auto param = GetParam();
  SCOPED_TRACE(param.input);
  CSSSelectorList* list = css_test_helpers::ParseSelectorList(param.input);
  const char* expected = param.expected ? param.expected : param.input;
  EXPECT_EQ(String(expected), list->SelectorsText());
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

TEST(CSSSelectorParserTest, PseudoElementsInCompoundLists) {
  const char* test_cases[] = {":not(::before)",
                              ":not(::content)",
                              ":host(::before)",
                              ":host(::content)",
                              ":host-context(::before)",
                              ":host-context(::content)",
                              ":-webkit-any(::after, ::before)",
                              ":-webkit-any(::content, span)"};

  HeapVector<CSSSelector> arena;
  for (auto* test_case : test_cases) {
    CSSTokenizer tokenizer(test_case);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        range,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        /*parent_rule_for_nesting=*/nullptr, nullptr, arena);
    EXPECT_EQ(vector.size(), 0u);
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

  HeapVector<CSSSelector> arena;
  for (auto* test_case : test_cases) {
    CSSTokenizer tokenizer(test_case);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        range,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        /*parent_rule_for_nesting=*/nullptr, nullptr, arena);
    EXPECT_GT(vector.size(), 0u);
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

  HeapVector<CSSSelector> arena;
  for (auto* test_case : test_cases) {
    CSSTokenizer tokenizer(test_case);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        range,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        /*parent_rule_for_nesting=*/nullptr, nullptr, arena);
    EXPECT_EQ(vector.size(), 0u);
  }
}

TEST(CSSSelectorParserTest, TransitionPseudoStyles) {
  ScopedViewTransitionForTest view_transition_enabled(true);

  struct TestCase {
    const char* selector;
    bool valid;
    const char* argument;
    CSSSelector::PseudoType type;
  };

  TestCase test_cases[] = {
      {"html::view-transition-group(*)", true, nullptr,
       CSSSelector::kPseudoViewTransitionGroup},
      {"html::view-transition-group(foo)", true, "foo",
       CSSSelector::kPseudoViewTransitionGroup},
      {"html::view-transition-image-pair(foo)", true, "foo",
       CSSSelector::kPseudoViewTransitionImagePair},
      {"html::view-transition-old(foo)", true, "foo",
       CSSSelector::kPseudoViewTransitionOld},
      {"html::view-transition-new(foo)", true, "foo",
       CSSSelector::kPseudoViewTransitionNew},
      {"::view-transition-group(foo)", true, "foo",
       CSSSelector::kPseudoViewTransitionGroup},
      {"div::view-transition-group(*)", true, nullptr,
       CSSSelector::kPseudoViewTransitionGroup},
      {"::view-transition-group(*)::before", false, nullptr,
       CSSSelector::kPseudoUnknown},
      {"::view-transition-group(*):hover", false, nullptr,
       CSSSelector::kPseudoUnknown},
  };

  HeapVector<CSSSelector> arena;
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.selector);
    CSSTokenizer tokenizer(test_case.selector);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        range,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        /*parent_rule_for_nesting=*/nullptr, nullptr, arena);
    EXPECT_EQ(!vector.empty(), test_case.valid);
    if (!test_case.valid) {
      continue;
    }

    CSSSelectorList* list = CSSSelectorList::AdoptSelectorVector(vector);
    ASSERT_TRUE(list->HasOneSelector());

    auto* selector = list->First();
    while (selector->TagHistory()) {
      selector = selector->TagHistory();
    }

    EXPECT_EQ(selector->GetPseudoType(), test_case.type);
    EXPECT_EQ(selector->Argument(), test_case.argument);
  }
}

TEST(CSSSelectorParserTest, WorkaroundForInvalidCustomPseudoInUAStyle) {
  // See crbug.com/578131
  const char* test_cases[] = {
      "video::-webkit-media-text-track-region-container.scrolling",
      "input[type=\"range\" i]::-webkit-media-slider-container > div"};

  HeapVector<CSSSelector> arena;
  for (auto* test_case : test_cases) {
    CSSTokenizer tokenizer(test_case);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        range,
        MakeGarbageCollected<CSSParserContext>(
            kUASheetMode, SecureContextMode::kInsecureContext),
        /*parent_rule_for_nesting=*/nullptr, nullptr, arena);
    EXPECT_GT(vector.size(), 0u);
  }
}

TEST(CSSSelectorParserTest, InvalidPseudoElementInNonRightmostCompound) {
  const char* test_cases[] = {"::-webkit-volume-slider *", "::before *",
                              "::-webkit-scrollbar *", "::cue *",
                              "::selection *"};

  HeapVector<CSSSelector> arena;
  for (auto* test_case : test_cases) {
    CSSTokenizer tokenizer(test_case);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        range,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        /*parent_rule_for_nesting=*/nullptr, nullptr, arena);
    EXPECT_EQ(vector.size(), 0u);
  }
}

TEST(CSSSelectorParserTest, UnresolvedNamespacePrefix) {
  const char* test_cases[] = {"ns|div", "div ns|div", "div ns|div "};

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);

  HeapVector<CSSSelector> arena;
  for (auto* test_case : test_cases) {
    CSSTokenizer tokenizer(test_case);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        range, context, /*parent_rule_for_nesting=*/nullptr, sheet, arena);
    EXPECT_EQ(vector.size(), 0u);
  }
}

TEST(CSSSelectorParserTest, UnexpectedPipe) {
  const char* test_cases[] = {"div | .c", "| div", " | div"};

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);

  HeapVector<CSSSelector> arena;
  for (auto* test_case : test_cases) {
    CSSTokenizer tokenizer(test_case);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        range, context, /*parent_rule_for_nesting=*/nullptr, sheet, arena);
    EXPECT_EQ(vector.size(), 0u);
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

  HeapVector<CSSSelector> arena;
  for (auto** test_case : test_cases) {
    SCOPED_TRACE(test_case[0]);
    CSSTokenizer tokenizer(test_case[0]);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        range, context, /*parent_rule_for_nesting=*/nullptr, sheet, arena);
    CSSSelectorList* list = CSSSelectorList::AdoptSelectorVector(vector);
    EXPECT_TRUE(list->IsValid());
    EXPECT_EQ(test_case[1], list->SelectorsText());
  }
}

TEST(CSSSelectorParserTest, AttributeSelectorUniversalInvalid) {
  const char* test_cases[] = {"[*]", "[*|*]"};

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);

  HeapVector<CSSSelector> arena;
  for (auto* test_case : test_cases) {
    SCOPED_TRACE(test_case);
    CSSTokenizer tokenizer(test_case);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        range, context, /*parent_rule_for_nesting=*/nullptr, sheet, arena);
    EXPECT_EQ(vector.size(), 0u);
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

  HeapVector<CSSSelector> arena;
  for (auto* test_case : test_cases) {
    SCOPED_TRACE(test_case);
    CSSTokenizer tokenizer(test_case);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);

    base::span<CSSSelector> author_vector = CSSSelectorParser::ParseSelector(
        range,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        /*parent_rule_for_nesting=*/nullptr, nullptr, arena);
    EXPECT_EQ(author_vector.size(), 0u);

    base::span<CSSSelector> ua_vector = CSSSelectorParser::ParseSelector(
        range,
        MakeGarbageCollected<CSSParserContext>(
            kUASheetMode, SecureContextMode::kInsecureContext),
        /*parent_rule_for_nesting=*/nullptr, nullptr, arena);
    EXPECT_GT(ua_vector.size(), 0u);
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
    // Only user-action pseudos + :--state are allowed after kPseudoPart:
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
    {"::part(foo):is(:--bar, .a)", "::part(foo):is(:--bar)"},
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
    {":is(.a\n)", ":is(.a)"},
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
    {":host(:is(.a\n))", ":host(:is(.a))"},
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
    {"::part(foo):is(:focus, :--bar)"},
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

AtomicString TagLocalName(const CSSSelector* selector) {
  return selector->TagQName().LocalName();
}

AtomicString AttributeLocalName(const CSSSelector* selector) {
  return selector->Attribute().LocalName();
}

AtomicString SelectorValue(const CSSSelector* selector) {
  return selector->Value();
}

struct ASCIILowerTestCase {
  const char* input;
  const char16_t* expected;
  using GetterFn = AtomicString(const CSSSelector*);
  GetterFn* getter;
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

  HeapVector<CSSSelector> arena;
  for (auto test_case : test_cases) {
    SCOPED_TRACE(test_case.input);
    CSSTokenizer tokenizer(test_case.input);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        range, context, /*parent_rule_for_nesting=*/nullptr, sheet, arena);
    EXPECT_GT(vector.size(), 0u);
    CSSSelectorList* list = CSSSelectorList::AdoptSelectorVector(vector);
    EXPECT_TRUE(list->IsValid());
    const CSSSelector* selector = list->First();
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

  HeapVector<CSSSelector> arena;
  for (auto test_case : test_cases) {
    SCOPED_TRACE(test_case.input);
    CSSTokenizer tokenizer(test_case.input);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        range, context, /*parent_rule_for_nesting=*/nullptr, sheet, arena);
    EXPECT_GT(vector.size(), 0u);
    CSSSelectorList* list = CSSSelectorList::AdoptSelectorVector(vector);
    EXPECT_TRUE(list->IsValid());
    const CSSSelector* selector = list->First();
    ASSERT_TRUE(selector);
    EXPECT_EQ(AtomicString(test_case.expected), test_case.getter(selector));
  }
}

TEST(CSSSelectorParserTest, ShadowPartPseudoElementValid) {
  const char* test_cases[] = {"::part(ident)", "host::part(ident)",
                              "host::part(ident):hover"};

  HeapVector<CSSSelector> arena;
  for (auto* test_case : test_cases) {
    SCOPED_TRACE(test_case);
    CSSTokenizer tokenizer(test_case);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        range,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        /*parent_rule_for_nesting=*/nullptr, nullptr, arena);
    CSSSelectorList* list = CSSSelectorList::AdoptSelectorVector(vector);
    EXPECT_EQ(test_case, list->SelectorsText().Utf8());
  }
}

TEST(CSSSelectorParserTest, ShadowPartAndBeforeAfterPseudoElementValid) {
  const char* test_cases[] = {
      "::part(ident)::before",       "::part(ident)::after",
      "::part(ident)::placeholder",  "::part(ident)::first-line",
      "::part(ident)::first-letter", "::part(ident)::selection"};

  HeapVector<CSSSelector> arena;
  for (auto* test_case : test_cases) {
    SCOPED_TRACE(test_case);
    CSSTokenizer tokenizer(test_case);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        range,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        /*parent_rule_for_nesting=*/nullptr, nullptr, arena);
    EXPECT_GT(vector.size(), 0u);
    CSSSelectorList* list = CSSSelectorList::AdoptSelectorVector(vector);
    EXPECT_TRUE(list->IsValid());
    EXPECT_EQ(test_case, list->SelectorsText());
  }
}

static bool IsCounted(const char* selector,
                      CSSParserMode mode,
                      WebFeature feature) {
  auto dummy_holder = std::make_unique<DummyPageHolder>(gfx::Size(500, 500));
  Document* doc = &dummy_holder->GetDocument();
  Page::InsertOrdinaryPageForTesting(&dummy_holder->GetPage());
  auto* context = MakeGarbageCollected<CSSParserContext>(
      mode, SecureContextMode::kSecureContext, CSSParserContext::kLiveProfile,
      doc);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);

  DCHECK(!doc->IsUseCounted(feature));

  CSSTokenizer tokenizer(selector);
  const auto tokens = tokenizer.TokenizeToEOF();
  CSSParserTokenRange range(tokens);
  HeapVector<CSSSelector> arena;
  CSSSelectorParser::ParseSelector(
      range, context, /*parent_rule_for_nesting=*/nullptr, sheet, arena);

  return doc->IsUseCounted(feature);
}

TEST(CSSSelectorParserTest, UseCountShadowPseudo) {
  auto ExpectCount = [](const char* selector, WebFeature feature) {
    SCOPED_TRACE(selector);
    EXPECT_TRUE(IsCounted(selector, kHTMLStandardMode, feature));
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

TEST(CSSSelectorParserTest, IsWhereUseCount) {
  const auto is_feature = WebFeature::kCSSSelectorPseudoIs;
  EXPECT_FALSE(IsCounted(".a", kHTMLStandardMode, is_feature));
  EXPECT_FALSE(IsCounted(":not(.a)", kHTMLStandardMode, is_feature));
  EXPECT_FALSE(IsCounted(":where(.a)", kHTMLStandardMode, is_feature));
  EXPECT_TRUE(IsCounted(":is()", kHTMLStandardMode, is_feature));
  EXPECT_TRUE(IsCounted(":is(.a)", kHTMLStandardMode, is_feature));
  EXPECT_TRUE(IsCounted(":not(:is(.a))", kHTMLStandardMode, is_feature));
  EXPECT_TRUE(IsCounted(".a:is(.b)", kHTMLStandardMode, is_feature));
  EXPECT_TRUE(IsCounted(":is(.a).b", kHTMLStandardMode, is_feature));
  EXPECT_FALSE(IsCounted(":is(.a)", kUASheetMode, is_feature));

  const auto where_feature = WebFeature::kCSSSelectorPseudoWhere;
  EXPECT_FALSE(IsCounted(".a", kHTMLStandardMode, where_feature));
  EXPECT_FALSE(IsCounted(":not(.a)", kHTMLStandardMode, where_feature));
  EXPECT_FALSE(IsCounted(":is(.a)", kHTMLStandardMode, where_feature));
  EXPECT_TRUE(IsCounted(":where()", kHTMLStandardMode, where_feature));
  EXPECT_TRUE(IsCounted(":where(.a)", kHTMLStandardMode, where_feature));
  EXPECT_TRUE(IsCounted(":not(:where(.a))", kHTMLStandardMode, where_feature));
  EXPECT_TRUE(IsCounted(".a:where(.b)", kHTMLStandardMode, where_feature));
  EXPECT_TRUE(IsCounted(":where(.a).b", kHTMLStandardMode, where_feature));
  EXPECT_FALSE(IsCounted(":where(.a)", kUASheetMode, where_feature));
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
              {"placeholder", CSSSelector::kUAShadow},
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
              {"placeholder", CSSSelector::kUAShadow},
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
              {"-webkit-media-controls", CSSSelector::kUAShadow},
              {"video", CSSSelector::kSubSelector},
          },
      },
  };

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);

  HeapVector<CSSSelector> arena;
  for (auto test_case : test_cases) {
    SCOPED_TRACE(test_case.input);
    CSSTokenizer tokenizer(test_case.input);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        range, context, /*parent_rule_for_nesting=*/nullptr, sheet, arena);
    CSSSelectorList* list = CSSSelectorList::AdoptSelectorVector(vector);
    EXPECT_TRUE(list->IsValid());
    const CSSSelector* selector = list->First();
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

TEST(CSSSelectorParserTest, WebKitScrollbarPseudoParsing) {
  const char* test_cases[] = {"::-webkit-resizer",
                              "::-webkit-scrollbar",
                              "::-webkit-scrollbar-button",
                              "::-webkit-scrollbar-corner",
                              "::-webkit-scrollbar-thumb",
                              "::-webkit-scrollbar-track",
                              "::-webkit-scrollbar-track-piece"};

  HeapVector<CSSSelector> arena;
  bool enabled_states[] = {false, true};
  for (auto state : enabled_states) {
    ScopedWebKitScrollbarStylingForTest scoped_feature(state);
    for (auto* test_case : test_cases) {
      CSSTokenizer tokenizer(test_case);
      const auto tokens = tokenizer.TokenizeToEOF();
      CSSParserTokenRange range(tokens);
      base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
          range,
          MakeGarbageCollected<CSSParserContext>(
              kHTMLStandardMode, SecureContextMode::kInsecureContext),
          /*parent_rule_for_nesting=*/nullptr, nullptr, arena);
      EXPECT_EQ(vector.size(), state ? 1u : 0u);
    }
  }
}

static const SelectorTestCase invalid_pseudo_has_arguments_data[] = {
    // clang-format off
    // restrict use of nested :has()
    {":has(:has(.a))", ""},
    {":has(.a, :has(.b), .c)", ""},
    {":has(.a, :has(.b))", ""},
    {":has(:has(.a), .b)", ""},
    {":has(:is(:has(.a)))", ":has(:is())"},

    // restrict use of pseudo element inside :has()
    {":has(::-webkit-progress-bar)", ""},
    {":has(::-webkit-progress-value)", ""},
    {":has(::-webkit-slider-runnable-track)", ""},
    {":has(::-webkit-slider-thumb)", ""},
    {":has(::after)", ""},
    {":has(::backdrop)", ""},
    {":has(::before)", ""},
    {":has(::cue)", ""},
    {":has(::first-letter)", ""},
    {":has(::first-line)", ""},
    {":has(::grammar-error)", ""},
    {":has(::marker)", ""},
    {":has(::placeholder)", ""},
    {":has(::selection)", ""},
    {":has(::slotted(*))", ""},
    {":has(::part(foo))", ""},
    {":has(::spelling-error)", ""},
    {":has(:after)", ""},
    {":has(:before)", ""},
    {":has(:cue)", ""},
    {":has(:first-letter)", ""},
    {":has(:first-line)", ""},

    // drops empty :has()
    {":has()", ""},
    {":has(,,  ,, )", ""},

    // drops :has() when it contains invalid argument
    {":has(.a,,,,)", ""},
    {":has(,,.a,,)", ""},
    {":has(,,,,.a)", ""},
    {":has(@x {,.b,}, .a)", ""},
    {":has({,.b,} @x, .a)", ""},
    {":has((@x), .a)", ""},
    {":has((.b), .a)", ""},

    // clang-format on
};

INSTANTIATE_TEST_SUITE_P(InvalidPseudoHasArguments,
                         SelectorParseTest,
                         testing::ValuesIn(invalid_pseudo_has_arguments_data));

static const SelectorTestCase has_nesting_data[] = {
    // clang-format off
    // :has() is not allowed in the pseudos accepting only compound selectors:
    {"::slotted(:has(.a))", ""},
    {":host(:has(.a))", ""},
    {":host-context(:has(.a))", ""},
    {"::cue(:has(.a))", ""},
    // :has() is not allowed after pseudo elements:
    {"::part(foo):has(:hover)", ""},
    {"::part(foo):has(:hover:focus)", ""},
    {"::part(foo):has(:focus, :hover)", ""},
    {"::part(foo):has(:focus)", ""},
    {"::part(foo):has(:focus, :--bar)", ""},
    {"::part(foo):has(.a)", ""},
    {"::part(foo):has(.a:hover)", ""},
    {"::part(foo):has(:hover.a)", ""},
    {"::part(foo):has(:hover + .a)", ""},
    {"::part(foo):has(.a + :hover)", ""},
    {"::part(foo):has(:hover:enabled)", ""},
    {"::part(foo):has(:enabled:hover)", ""},
    {"::part(foo):has(:hover, :where(.a))", ""},
    {"::part(foo):has(:hover, .a)", ""},
    {"::part(foo):has(:--bar, .a)", ""},
    {"::part(foo):has(:enabled)", ""},
    {"::-webkit-scrollbar:has(:enabled)", ""},
    {"::selection:has(:window-inactive)", ""},
    {"::-webkit-input-placeholder:has(:hover)", ""},
    // clang-format on
};

INSTANTIATE_TEST_SUITE_P(NestedHasSelectorValidity,
                         SelectorParseTest,
                         testing::ValuesIn(has_nesting_data));

// TODO(blee@igalia.com) Workaround to make :has() unforgiving to avoid
// JQuery :has() issue: https://github.com/w3c/csswg-drafts/issues/7676
// :has() should be valid after all arguments are dropped.
static const SelectorTestCase invalid_forgiving_pseudo_has_arguments_data[] = {
    // clang-format off
    // restrict use of nested :has()
    {":has(:has(.a))", "" /* should be ":has()" */},

    // restrict use of pseudo element inside :has()
    {":has(::-webkit-progress-bar)", "" /* should be ":has()" */},
    {":has(::-webkit-progress-value)", "" /* should be ":has()" */},
    {":has(::-webkit-slider-runnable-track)", "" /* should be ":has()" */},
    {":has(::-webkit-slider-thumb)", "" /* should be ":has()" */},
    {":has(::after)", "" /* should be ":has()" */},
    {":has(::backdrop)", "" /* should be ":has()" */},
    {":has(::before)", "" /* should be ":has()" */},
    {":has(::cue)", "" /* should be ":has()" */},
    {":has(::first-letter)", "" /* should be ":has()" */},
    {":has(::first-line)", "" /* should be ":has()" */},
    {":has(::grammar-error)", "" /* should be ":has()" */},
    {":has(::marker)", "" /* should be ":has()" */},
    {":has(::placeholder)", "" /* should be ":has()" */},
    {":has(::selection)", "" /* should be ":has()" */},
    {":has(::slotted(*))", "" /* should be ":has()" */},
    {":has(::part(foo))", "" /* should be ":has()" */},
    {":has(::spelling-error)", "" /* should be ":has()" */},
    {":has(:after)", "" /* should be ":has()" */},
    {":has(:before)", "" /* should be ":has()" */},
    {":has(:cue)", "" /* should be ":has()" */},
    {":has(:first-letter)", "" /* should be ":has()" */},
    {":has(:first-line)", "" /* should be ":has()" */},
    // clang-format on
};

INSTANTIATE_TEST_SUITE_P(
    InvalidPseudoHasArguments,
    SelectorParseTestForHasForgivingParsing,
    testing::ValuesIn(invalid_forgiving_pseudo_has_arguments_data));

static const SelectorTestCase has_forgiving_data[] = {
    // clang-format off
    {":has(.a, :has(.b), .c)", ":has(.a, .c)"},
    {":has(.a, :has(.b))", ":has(.a)"},
    {":has(:has(.a), .b)", ":has(.b)"},

    // TODO(blee@igalia.com) Workaround to make :has() unforgiving to avoid
    // JQuery :has() issue: https://github.com/w3c/csswg-drafts/issues/7676
    // :has() should be valid after all arguments are dropped.
    {":has(:has(.a))", "" /* should be ":has()" */},
    {":has(,,  ,, )", "" /* should be ":has()" */},

    {":has(.a,,,,)", ":has(.a)"},
    {":has(,,.a,,)", ":has(.a)"},
    {":has(,,,,.a)", ":has(.a)"},
    {":has(@x {,.b,}, .a)", ":has(.a)"},
    {":has({,.b,} @x, .a)", ":has(.a)"},
    {":has((@x), .a)", ":has(.a)"},
    {":has((.b), .a)", ":has(.a)"},
    {":has(:is(:foo))", ":has(:is())"},
    {":has(:is(:has(.a)))", ":has(:is())"},
    // clang-format on
};

INSTANTIATE_TEST_SUITE_P(HasForgiving,
                         SelectorParseTestForHasForgivingParsing,
                         testing::ValuesIn(has_forgiving_data));

// TODO(blee@igalia.com) Workaround to make :has() unforgiving to avoid
// JQuery :has() issue: https://github.com/w3c/csswg-drafts/issues/7676
// :has() should be valid after all arguments are dropped.
static const SelectorTestCase forgiving_has_nesting_data[] = {
    // clang-format off
    // :has() is not allowed in the pseudos accepting only compound selectors:
    {"::slotted(:has(.a))", "" /* should be "::slotted(:has())" */},
    {":host(:has(.a))", "" /* should be ":host(:has())" */},
    {":host-context(:has(.a))", "" /* should be ":host-context(:has())" */},
    {"::cue(:has(.a))", "" /* should be "::cue(:has())" */},
    // :has() is not allowed after pseudo elements:
    {"::part(foo):has(:hover)", "" /* should be "::part(foo):has()" */},
    {"::part(foo):has(:hover:focus)", "" /* should be "::part(foo):has()" */},
    {"::part(foo):has(:focus, :hover)", "" /* should be "::part(foo):has()" */},
    {"::part(foo):has(:focus)", "" /* should be "::part(foo):has()" */},
    {"::part(foo):has(:focus, :--bar)", "" /* should be "::part(foo):has()" */},
    {"::part(foo):has(.a)", "" /* should be "::part(foo):has()" */},
    {"::part(foo):has(.a:hover)", "" /* should be "::part(foo):has()" */},
    {"::part(foo):has(:hover.a)", "" /* should be "::part(foo):has()" */},
    {"::part(foo):has(:hover + .a)", "" /* should be "::part(foo):has()" */},
    {"::part(foo):has(.a + :hover)", "" /* should be "::part(foo):has()" */},
    {"::part(foo):has(:hover:enabled)", "" /* should be "::part(foo):has()" */},
    {"::part(foo):has(:enabled:hover)", "" /* should be "::part(foo):has()" */},
    {"::part(foo):has(:hover, :where(.a))",
     "" /* should be "::part(foo):has()" */},
    {"::part(foo):has(:hover, .a)", "" /* should be "::part(foo):has()" */},
    {"::part(foo):has(:--bar, .a)", "" /* should be "::part(foo):has()" */},
    {"::part(foo):has(:enabled)", "" /* should be "::part(foo):has()" */},
    {"::-webkit-scrollbar:has(:enabled)",
     "" /* should be "::-webkit-scrollbar:has()" */},
    {"::selection:has(:window-inactive)",
     "" /* should be "::selection:has()" */},
    {"::-webkit-input-placeholder:has(:hover)",
     "" /* should be "::-webkit-input-placeholder:has()" */},
    // clang-format on
};

INSTANTIATE_TEST_SUITE_P(NestedHasSelectorValidity,
                         SelectorParseTestForHasForgivingParsing,
                         testing::ValuesIn(forgiving_has_nesting_data));

}  // namespace blink
