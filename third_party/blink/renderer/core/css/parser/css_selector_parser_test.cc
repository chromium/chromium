// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
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
#include "third_party/blink/renderer/platform/testing/task_environment.h"
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
  CSSSelectorList* list = css_test_helpers::ParseSelectorList(param.input);
  const char* expected = param.expected ? param.expected : param.input;
  EXPECT_EQ(String(expected), list->SelectorsText());
}

TEST(CSSSelectorParserTest, ValidANPlusB) {
  test::TaskEnvironment task_environment;
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
    CSSParserTokenStream stream(test_case.input);
    bool passed = CSSSelectorParser::ConsumeANPlusB(stream, ab);
    EXPECT_TRUE(passed);
    EXPECT_EQ(test_case.a, ab.first);
    EXPECT_EQ(test_case.b, ab.second);
  }
}

TEST(CSSSelectorParserTest, InvalidANPlusB) {
  test::TaskEnvironment task_environment;
  // Some of these have token range prefixes which are valid <an+b> and could
  // in theory be valid in consumeANPlusB, but this behaviour isn't needed
  // anywhere and not implemented.
  const char* test_cases[] = {
      " odd",     "+ n",     "3m+4",  "12n--34",  "12n- -34",
      "12n- +34", "23n-+43", "10n 5", "10n + +5", "10n + -5",
  };

  for (String test_case : test_cases) {
    SCOPED_TRACE(test_case);

    std::pair<int, int> ab;
    CSSParserTokenStream stream(test_case);
    bool passed = CSSSelectorParser::ConsumeANPlusB(stream, ab);
    EXPECT_FALSE(passed);
  }
}

TEST(CSSSelectorParserTest, PseudoElementsInCompoundLists) {
  test::TaskEnvironment task_environment;
  const char* test_cases[] = {":not(::before)",
                              ":not(::content)",
                              ":host(::before)",
                              ":host(::content)",
                              ":host-context(::before)",
                              ":host-context(::content)",
                              ":-webkit-any(::after, ::before)",
                              ":-webkit-any(::content, span)"};

  HeapVector<CSSSelector> arena;
  for (StringView test_case : test_cases) {
    CSSParserTokenStream stream(test_case);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        stream,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr,
        /*is_within_scope=*/false,
        /*semicolon_aborts_nested_selector=*/false, nullptr, arena);
    EXPECT_EQ(vector.size(), 0u);
  }
}

TEST(CSSSelectorParserTest, ValidSimpleAfterPseudoElementInCompound) {
  test::TaskEnvironment task_environment;
  const char* test_cases[] = {"::-webkit-volume-slider:hover",
                              "::selection:window-inactive",
                              "::search-text:current",
                              "::search-text:not(:current)",
                              "::-webkit-scrollbar:disabled",
                              "::-webkit-volume-slider:not(:hover)",
                              "::-webkit-scrollbar:not(:horizontal)",
                              "::slotted(span)::before",
                              "::slotted(div)::after",
                              "::slotted(div)::view-transition"};

  HeapVector<CSSSelector> arena;
  for (StringView test_case : test_cases) {
    SCOPED_TRACE(test_case);
    CSSParserTokenStream stream(test_case);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        stream,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr,
        /*is_within_scope=*/false,
        /*semicolon_aborts_nested_selector=*/false, nullptr, arena);
    EXPECT_GT(vector.size(), 0u);
  }
}

TEST(CSSSelectorParserTest, InvalidSimpleAfterPseudoElementInCompound) {
  test::TaskEnvironment task_environment;
  const char* test_cases[] = {
      "::before#id",
      "::after:hover",
      ".class::content::before",
      "::shadow.class",
      "::selection:window-inactive::before",
      "::search-text.class",
      "::search-text::before",
      "::search-text:hover",
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
  for (StringView test_case : test_cases) {
    CSSParserTokenStream stream(test_case);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        stream,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr,
        /*is_within_scope=*/false,
        /*semicolon_aborts_nested_selector=*/false, nullptr, arena);
    EXPECT_EQ(vector.size(), 0u);
  }
}

TEST(CSSSelectorParserTest, TransitionPseudoStyles) {
  test::TaskEnvironment task_environment;
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
    CSSParserTokenStream stream(test_case.selector);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        stream,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr,
        /*is_within_scope=*/false,
        /*semicolon_aborts_nested_selector=*/false, nullptr, arena);
    EXPECT_EQ(!vector.empty(), test_case.valid);
    if (!test_case.valid) {
      continue;
    }

    CSSSelectorList* list = CSSSelectorList::AdoptSelectorVector(vector);
    ASSERT_TRUE(list->HasOneSelector());

    auto* selector = list->First();
    while (selector->NextSimpleSelector()) {
      selector = selector->NextSimpleSelector();
    }

    EXPECT_EQ(selector->GetPseudoType(), test_case.type);
    EXPECT_EQ(selector->GetPseudoType() == CSSSelector::kPseudoViewTransition
                  ? selector->Argument()
                  : selector->IdentList()[0],
              test_case.argument);
  }
}

TEST(CSSSelectorParserTest, WorkaroundForInvalidCustomPseudoInUAStyle) {
  test::TaskEnvironment task_environment;
  // See crbug.com/578131
  const char* test_cases[] = {
      "video::-webkit-media-text-track-region-container.scrolling",
      "input[type=\"range\" i]::-webkit-media-slider-container > div"};

  HeapVector<CSSSelector> arena;
  for (StringView test_case : test_cases) {
    CSSParserTokenStream stream(test_case);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        stream,
        MakeGarbageCollected<CSSParserContext>(
            kUASheetMode, SecureContextMode::kInsecureContext),
        CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr,
        /*is_within_scope=*/false,
        /*semicolon_aborts_nested_selector=*/false, nullptr, arena);
    EXPECT_GT(vector.size(), 0u);
  }
}

TEST(CSSSelectorParserTest, InvalidPseudoElementInNonRightmostCompound) {
  test::TaskEnvironment task_environment;
  const char* test_cases[] = {"::-webkit-volume-slider *", "::before *",
                              "::-webkit-scrollbar *", "::cue *",
                              "::selection *"};

  HeapVector<CSSSelector> arena;
  for (StringView test_case : test_cases) {
    CSSParserTokenStream stream(test_case);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        stream,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr,
        /*is_within_scope=*/false,
        /*semicolon_aborts_nested_selector=*/false, nullptr, arena);
    EXPECT_EQ(vector.size(), 0u);
  }
}

TEST(CSSSelectorParserTest, UnresolvedNamespacePrefix) {
  test::TaskEnvironment task_environment;
  const char* test_cases[] = {"ns|div", "div ns|div", "div ns|div "};

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);

  HeapVector<CSSSelector> arena;
  for (StringView test_case : test_cases) {
    CSSParserTokenStream stream(test_case);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        stream, context, CSSNestingType::kNone,
        /*parent_rule_for_nesting=*/nullptr, /*is_within_scope=*/false,
        /*semicolon_aborts_nested_selector=*/false, sheet, arena);
    EXPECT_EQ(vector.size(), 0u);
  }
}

TEST(CSSSelectorParserTest, UnexpectedPipe) {
  test::TaskEnvironment task_environment;
  const char* test_cases[] = {"div | .c", "| div", " | div"};

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);

  HeapVector<CSSSelector> arena;
  for (StringView test_case : test_cases) {
    CSSParserTokenStream stream(test_case);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        stream, context, CSSNestingType::kNone,
        /*parent_rule_for_nesting=*/nullptr, /*is_within_scope=*/false,
        /*semicolon_aborts_nested_selector=*/false, sheet, arena);
    EXPECT_EQ(vector.size(), 0u);
  }
}

TEST(CSSSelectorParserTest, SerializedUniversal) {
  test::TaskEnvironment task_environment;
  struct SerializationTestCase {
    const char* source;
    const char* expected;
  };
  const SerializationTestCase test_cases[] = {
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
  sheet->ParserAddNamespace(AtomicString("ns"), AtomicString("http://ns.org"));

  HeapVector<CSSSelector> arena;
  for (const SerializationTestCase& test_case : test_cases) {
    SCOPED_TRACE(test_case.source);
    CSSParserTokenStream stream(test_case.source);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        stream, context, CSSNestingType::kNone,
        /*parent_rule_for_nesting=*/nullptr, /*is_within_scope=*/false,
        /*semicolon_aborts_nested_selector=*/false, sheet, arena);
    CSSSelectorList* list = CSSSelectorList::AdoptSelectorVector(vector);
    EXPECT_TRUE(list->IsValid());
    EXPECT_EQ(test_case.expected, list->SelectorsText());
  }
}

TEST(CSSSelectorParserTest, AttributeSelectorUniversalInvalid) {
  test::TaskEnvironment task_environment;
  const char* test_cases[] = {"[*]", "[*|*]"};

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);

  HeapVector<CSSSelector> arena;
  for (String test_case : test_cases) {
    SCOPED_TRACE(test_case);
    CSSParserTokenStream stream(test_case);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        stream, context, CSSNestingType::kNone,
        /*parent_rule_for_nesting=*/nullptr, /*is_within_scope=*/false,
        /*semicolon_aborts_nested_selector=*/false, sheet, arena);
    EXPECT_EQ(vector.size(), 0u);
  }
}

TEST(CSSSelectorParserTest, InternalPseudo) {
  test::TaskEnvironment task_environment;
  const char* test_cases[] = {"::-internal-whatever",
                              "::-internal-media-controls-text-track-list",
                              ":-internal-is-html",
                              ":-internal-list-box",
                              ":-internal-multi-select-focus",
                              ":-internal-shadow-host-has-non-auto-appearance",
                              ":-internal-spatial-navigation-focus",
                              ":-internal-video-persistent",
                              ":-internal-video-persistent-ancestor"};

  HeapVector<CSSSelector> arena;
  for (String test_case : test_cases) {
    SCOPED_TRACE(test_case);
    {
      CSSParserTokenStream stream(test_case);
      base::span<CSSSelector> author_vector = CSSSelectorParser::ParseSelector(
          stream,
          MakeGarbageCollected<CSSParserContext>(
              kHTMLStandardMode, SecureContextMode::kInsecureContext),
          CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr,
          /*is_within_scope=*/false,
          /*semicolon_aborts_nested_selector=*/false, nullptr, arena);
      EXPECT_EQ(author_vector.size(), 0u);
    }

    {
      CSSParserTokenStream stream(test_case);
      base::span<CSSSelector> ua_vector = CSSSelectorParser::ParseSelector(
          stream,
          MakeGarbageCollected<CSSParserContext>(
              kUASheetMode, SecureContextMode::kInsecureContext),
          CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr,
          /*is_within_scope=*/false,
          /*semicolon_aborts_nested_selector=*/false, nullptr, arena);
      EXPECT_GT(ua_vector.size(), 0u);
    }
  }
}

TEST(CSSSelectorParserTest, ScrollControlPseudos) {
  test::TaskEnvironment task_environment;
  struct TestCase {
    const char* selector;
    CSSSelector::PseudoType type;
  };

  TestCase test_cases[] = {
      {"ul::scroll-marker-group", CSSSelector::kPseudoScrollMarkerGroup},
      {"li::scroll-marker", CSSSelector::kPseudoScrollMarker},
      {"div::scroll-next-button", CSSSelector::kPseudoScrollNextButton},
      {"div::scroll-prev-button", CSSSelector::kPseudoScrollPrevButton},
  };

  HeapVector<CSSSelector> arena;
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.selector);
    CSSParserTokenStream stream(test_case.selector);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        stream,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr,
        /*is_within_scope=*/false,
        /*semicolon_aborts_nested_selector=*/false, nullptr, arena);
    EXPECT_TRUE(!vector.empty());

    CSSSelectorList* list = CSSSelectorList::AdoptSelectorVector(vector);
    ASSERT_TRUE(list->HasOneSelector());

    const CSSSelector* selector = list->First();
    while (selector->NextSimpleSelector()) {
      selector = selector->NextSimpleSelector();
    }

    EXPECT_EQ(selector->GetPseudoType(), test_case.type);
  }
}

TEST(CSSSelectorParserTest, ColumnPseudo) {
  test::TaskEnvironment task_environment;
  struct TestCase {
    const char* selector;
    CSSSelector::PseudoType type;
  };

  TestCase test_cases[] = {
      {".scroller::column", CSSSelector::kPseudoColumn},
      {"#scroller::column", CSSSelector::kPseudoColumn},
      {"div::column", CSSSelector::kPseudoColumn},
      {"div::before::column", CSSSelector::kPseudoUnknown},
      {"div::after::column", CSSSelector::kPseudoUnknown},
  };

  HeapVector<CSSSelector> arena;
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.selector);
    CSSParserTokenStream stream(StringView(test_case.selector));
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        stream,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr,
        /*is_within_scope=*/false,
        /*semicolon_aborts_nested_selector=*/false, nullptr, arena);

    if (test_case.type == CSSSelector::kPseudoUnknown) {
      EXPECT_TRUE(vector.empty());
      return;
    }

    EXPECT_TRUE(!vector.empty());

    CSSSelectorList* list = CSSSelectorList::AdoptSelectorVector(vector);
    ASSERT_TRUE(list->HasOneSelector());

    const CSSSelector* selector = list->First();
    while (selector->NextSimpleSelector()) {
      selector = selector->NextSimpleSelector();
    }

    EXPECT_EQ(selector->GetPseudoType(), test_case.type);
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
    // If the selector is nest-containing, it serializes as-is:
    // https://drafts.csswg.org/css-nesting-1/#syntax
    {":is(:unknown(&))"},
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
    // Structural pseudos are not allowed after ::part().
    {"::part(foo):is(.a)", "::part(foo):is()"},
    {"::part(foo):is(.a:hover)", "::part(foo):is()"},
    {"::part(foo):is(:hover.a)", "::part(foo):is()"},
    {"::part(foo):is(:hover + .a)", "::part(foo):is()"},
    {"::part(foo):is(.a + :hover)", "::part(foo):is()"},
    {"::part(foo):is(:hover:first-child)", "::part(foo):is()"},
    {"::part(foo):is(:first-child:hover)", "::part(foo):is()"},
    {"::part(foo):is(:hover, :where(.a))",
     "::part(foo):is(:hover, :where())"},
    {"::part(foo):is(:hover, .a)", "::part(foo):is(:hover)"},
    {"::part(foo):is(:state(bar), .a)", "::part(foo):is(:state(bar))"},
    {"::part(foo):is(:first-child)", "::part(foo):is()"},
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
  test::TaskEnvironment task_environment;
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
    CSSParserTokenStream stream(test_case.input);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        stream, context, CSSNestingType::kNone,
        /*parent_rule_for_nesting=*/nullptr, /*is_within_scope=*/false,
        /*semicolon_aborts_nested_selector=*/false, sheet, arena);
    EXPECT_GT(vector.size(), 0u);
    CSSSelectorList* list = CSSSelectorList::AdoptSelectorVector(vector);
    EXPECT_TRUE(list->IsValid());
    const CSSSelector* selector = list->First();
    ASSERT_TRUE(selector);
    EXPECT_EQ(AtomicString(test_case.expected), test_case.getter(selector));
  }
}

TEST(CSSSelectorParserTest, ASCIILowerHTMLQuirks) {
  test::TaskEnvironment task_environment;
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
    CSSParserTokenStream stream(test_case.input);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        stream, context, CSSNestingType::kNone,
        /*parent_rule_for_nesting=*/nullptr, /*is_within_scope=*/false,
        /*semicolon_aborts_nested_selector=*/false, sheet, arena);
    EXPECT_GT(vector.size(), 0u);
    CSSSelectorList* list = CSSSelectorList::AdoptSelectorVector(vector);
    EXPECT_TRUE(list->IsValid());
    const CSSSelector* selector = list->First();
    ASSERT_TRUE(selector);
    EXPECT_EQ(AtomicString(test_case.expected), test_case.getter(selector));
  }
}

TEST(CSSSelectorParserTest, ShadowPartPseudoElementValid) {
  test::TaskEnvironment task_environment;
  const char* test_cases[] = {"::part(ident)", "host::part(ident)",
                              "host::part(ident):hover"};

  HeapVector<CSSSelector> arena;
  for (String test_case : test_cases) {
    SCOPED_TRACE(test_case);
    CSSParserTokenStream stream(test_case);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        stream,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr,
        /*is_within_scope=*/false,
        /*semicolon_aborts_nested_selector=*/false, nullptr, arena);
    CSSSelectorList* list = CSSSelectorList::AdoptSelectorVector(vector);
    EXPECT_EQ(test_case, list->SelectorsText());
  }
}

TEST(CSSSelectorParserTest, ShadowPartAndBeforeAfterPseudoElementValid) {
  test::TaskEnvironment task_environment;
  const char* test_cases[] = {
      "::part(ident)::before",       "::part(ident)::after",
      "::part(ident)::placeholder",  "::part(ident)::first-line",
      "::part(ident)::first-letter", "::part(ident)::selection"};

  HeapVector<CSSSelector> arena;
  for (String test_case : test_cases) {
    SCOPED_TRACE(test_case);
    CSSParserTokenStream stream(test_case);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        stream,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr,
        /*is_within_scope=*/false,
        /*semicolon_aborts_nested_selector=*/false, nullptr, arena);
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
      mode, SecureContextMode::kSecureContext, doc);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);

  DCHECK(!doc->IsUseCounted(feature));

  CSSParserTokenStream stream(selector);
  HeapVector<CSSSelector> arena;
  CSSSelectorParser::ParseSelector(stream, context, CSSNestingType::kNone,
                                   /*parent_rule_for_nesting=*/nullptr,
                                   /*is_within_scope=*/false,
                                   /*semicolon_aborts_nested_selector=*/false,
                                   sheet, arena);

  return doc->IsUseCounted(feature);
}

TEST(CSSSelectorParserTest, UseCountShadowPseudo) {
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
  struct ShadowCombinatorTest {
    const char* input;
    Vector<std::pair<AtomicString, CSSSelector::RelationType>> expectation;
  };

  const ShadowCombinatorTest test_cases[] = {
      {
          "*::placeholder",
          {
              {AtomicString("placeholder"), CSSSelector::kUAShadow},
              {g_null_atom, CSSSelector::kSubSelector},
          },
      },
      {
          "div::slotted(*)",
          {
              {AtomicString("slotted"), CSSSelector::kShadowSlot},
              {AtomicString("div"), CSSSelector::kSubSelector},
          },
      },
      {
          "::slotted(*)::placeholder",
          {
              {AtomicString("placeholder"), CSSSelector::kUAShadow},
              {AtomicString("slotted"), CSSSelector::kShadowSlot},
              {g_null_atom, CSSSelector::kSubSelector},
          },
      },
      {
          "span::part(my-part)",
          {
              {AtomicString("part"), CSSSelector::kShadowPart},
              {AtomicString("span"), CSSSelector::kSubSelector},
          },
      },
      {
          "video::-webkit-media-controls",
          {
              {AtomicString("-webkit-media-controls"), CSSSelector::kUAShadow},
              {AtomicString("video"), CSSSelector::kSubSelector},
          },
      },
  };

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);

  HeapVector<CSSSelector> arena;
  for (auto test_case : test_cases) {
    SCOPED_TRACE(test_case.input);
    CSSParserTokenStream stream(test_case.input);
    base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
        stream, context, CSSNestingType::kNone,
        /*parent_rule_for_nesting=*/nullptr, /*is_within_scope=*/false,
        /*semicolon_aborts_nested_selector=*/false, sheet, arena);
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
      selector = selector->NextSimpleSelector();
    }
    EXPECT_FALSE(selector);
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
    {"::part(foo):has(:focus, :state(bar))", ""},
    {"::part(foo):has(.a)", ""},
    {"::part(foo):has(.a:hover)", ""},
    {"::part(foo):has(:hover.a)", ""},
    {"::part(foo):has(:hover + .a)", ""},
    {"::part(foo):has(.a + :hover)", ""},
    {"::part(foo):has(:hover:enabled)", ""},
    {"::part(foo):has(:enabled:hover)", ""},
    {"::part(foo):has(:hover, :where(.a))", ""},
    {"::part(foo):has(:hover, .a)", ""},
    {"::part(foo):has(:state(bar), .a)", ""},
    {"::part(foo):has(:enabled)", ""},
    {"::-webkit-scrollbar:has(:enabled)", ""},
    {"::selection:has(:window-inactive)", ""},
    {"::-webkit-input-placeholder:has(:hover)", ""},
    // clang-format on
};

INSTANTIATE_TEST_SUITE_P(NestedHasSelectorValidity,
                         SelectorParseTest,
                         testing::ValuesIn(has_nesting_data));

static CSSSelectorList* ParseNested(String inner_rule,
                                    CSSNestingType nesting_type) {
  auto dummy_holder = std::make_unique<DummyPageHolder>(gfx::Size(500, 500));
  Document& document = dummy_holder->GetDocument();

  auto* parent_rule_for_nesting =
      nesting_type == CSSNestingType::kNone
          ? nullptr
          : DynamicTo<StyleRule>(
                css_test_helpers::ParseRule(document, "div {}"));
  bool is_within_scope = nesting_type == CSSNestingType::kScope;
  CSSSelectorList* list = css_test_helpers::ParseSelectorList(
      inner_rule, nesting_type, parent_rule_for_nesting, is_within_scope);
  if (!list || !list->First()) {
    return nullptr;
  }
  return list;
}

static std::optional<CSSSelector::PseudoType> GetImplicitlyAddedPseudo(
    String inner_rule,
    CSSNestingType nesting_type) {
  CSSSelectorList* list = ParseNested(inner_rule, nesting_type);
  if (!list) {
    return std::nullopt;
  }

  Vector<const CSSSelector*> selectors;
  for (const CSSSelector* selector = list->First(); selector;
       selector = selector->NextSimpleSelector()) {
    selectors.push_back(selector);
  }
  // The back of `selectors` now contains the leftmost simple CSSSelector.

  // Ignore leading :true.
  if (!selectors.empty() &&
      selectors.back()->GetPseudoType() == CSSSelector::kPseudoTrue) {
    selectors.pop_back();
  }

  const CSSSelector* back = !selectors.empty() ? selectors.back() : nullptr;
  if (!back || back->Match() != CSSSelector::kPseudoClass ||
      !back->IsImplicit()) {
    return std::nullopt;
  }
  return back->GetPseudoType();
}

TEST(CSSSelectorParserTest, NestingTypeImpliedDescendant) {
  test::TaskEnvironment task_environment;
  // Nesting selector (&)
  EXPECT_EQ(CSSSelector::kPseudoParent,
            GetImplicitlyAddedPseudo(".foo", CSSNestingType::kNesting));
  EXPECT_EQ(
      CSSSelector::kPseudoParent,
      GetImplicitlyAddedPseudo(".foo:is(.bar)", CSSNestingType::kNesting));
  EXPECT_EQ(CSSSelector::kPseudoParent,
            GetImplicitlyAddedPseudo("> .foo", CSSNestingType::kNesting));
  EXPECT_EQ(std::nullopt,
            GetImplicitlyAddedPseudo(".foo > &", CSSNestingType::kNesting));
  EXPECT_EQ(std::nullopt, GetImplicitlyAddedPseudo(".foo > :is(.b, &)",
                                                   CSSNestingType::kNesting));
  EXPECT_EQ(std::nullopt,
            GetImplicitlyAddedPseudo("& .foo", CSSNestingType::kNesting));

  // :scope
  EXPECT_EQ(CSSSelector::kPseudoScope,
            GetImplicitlyAddedPseudo(".foo", CSSNestingType::kScope));
  EXPECT_EQ(CSSSelector::kPseudoScope,
            GetImplicitlyAddedPseudo(".foo:is(.bar)", CSSNestingType::kScope));
  EXPECT_EQ(CSSSelector::kPseudoScope,
            GetImplicitlyAddedPseudo("> .foo", CSSNestingType::kScope));
  // :scope makes a selector :scope-containing:
  EXPECT_EQ(std::nullopt,
            GetImplicitlyAddedPseudo(".foo > :scope", CSSNestingType::kScope));
  EXPECT_EQ(std::nullopt, GetImplicitlyAddedPseudo(".foo > :is(.b, :scope)",
                                                   CSSNestingType::kScope));
  EXPECT_EQ(std::nullopt,
            GetImplicitlyAddedPseudo(":scope .foo", CSSNestingType::kScope));
  // '&' also makes a selector :scope-containing:
  EXPECT_EQ(std::nullopt,
            GetImplicitlyAddedPseudo(".foo > &", CSSNestingType::kScope));
  EXPECT_EQ(std::nullopt, GetImplicitlyAddedPseudo(".foo > :is(.b, &)",
                                                   CSSNestingType::kScope));
  EXPECT_EQ(std::nullopt, GetImplicitlyAddedPseudo(".foo > :is(.b, !&)",
                                                   CSSNestingType::kScope));
  EXPECT_EQ(std::nullopt, GetImplicitlyAddedPseudo(".foo > :is(.b, :scope)",
                                                   CSSNestingType::kScope));
  EXPECT_EQ(std::nullopt, GetImplicitlyAddedPseudo(".foo > :is(.b, :SCOPE)",
                                                   CSSNestingType::kScope));
  EXPECT_EQ(std::nullopt, GetImplicitlyAddedPseudo(".foo > :is(.b, !:scope)",
                                                   CSSNestingType::kScope));
  EXPECT_EQ(std::nullopt,
            GetImplicitlyAddedPseudo("& .foo", CSSNestingType::kScope));

  // kNone
  EXPECT_EQ(std::nullopt,
            GetImplicitlyAddedPseudo(".foo", CSSNestingType::kNone));
  EXPECT_EQ(std::nullopt,
            GetImplicitlyAddedPseudo(".foo:is(.bar)", CSSNestingType::kNone));
  EXPECT_EQ(std::nullopt,
            GetImplicitlyAddedPseudo("> .foo", CSSNestingType::kNone));
  EXPECT_EQ(std::nullopt,
            GetImplicitlyAddedPseudo(".foo > &", CSSNestingType::kNone));
  EXPECT_EQ(std::nullopt, GetImplicitlyAddedPseudo(".foo > :is(.b, &)",
                                                   CSSNestingType::kNone));
  EXPECT_EQ(std::nullopt,
            GetImplicitlyAddedPseudo("& .foo", CSSNestingType::kNone));
  EXPECT_EQ(std::nullopt,
            GetImplicitlyAddedPseudo(".foo > :scope", CSSNestingType::kNone));
  EXPECT_EQ(std::nullopt, GetImplicitlyAddedPseudo(".foo > :is(.b, :scope)",
                                                   CSSNestingType::kNone));
  EXPECT_EQ(std::nullopt,
            GetImplicitlyAddedPseudo(":scope .foo", CSSNestingType::kNone));
}

static const CSSSelector* NthSimpleSelector(const CSSSelector& selector,
                                            wtf_size_t index) {
  wtf_size_t i = 0;
  for (const CSSSelector* s = &selector; s; s = s->NextSimpleSelector()) {
    if (i == index) {
      return s;
    }
    ++i;
  }
  return nullptr;
}

struct ScopeActivationData {
  // The selector text, e.g. ".a .b > .c".
  const char* inner_rule;
  // The simple CSSSelector to "focus" the test on, specified by the Nth
  // CSSSelector in the list of simple selectors.
  wtf_size_t index;
};

// Each test verifies that the simple selector at the specified selector
// index is ':true' and that it has relation=kPseudoActivation.
ScopeActivationData scope_activation_data[] = {
    // Comments indicate the expected order of simple selectors
    // in the list of simple selectors.

    // [:true, :scope]
    {":scope", 0},

    // [:true, :scope, :true, :scope]
    {":scope :scope", 0},
    {":scope :scope", 2},

    // [.bar, .foo, :true, :scope]
    {".foo > .bar", 2},

    // [.bar, .foo, :true, :scope]
    {"> .foo > .bar", 2},

    // [:true, :scope, .foo]
    {".foo > :scope", 0},

    // [.bar, :true, :scope, .foo]
    {".foo > :scope > .bar", 1},

    // [.bar, :true, :scope, .foo]
    {".foo :scope .bar", 1},

    // [.bar, :true, .a, .b, .c, :scope, .foo]
    {".foo > .a.b.c:scope > .bar", 1},

    // [.bar, :true, .a, :where(...), .foo]
    {".foo > .a:where(.b, :scope) > .bar", 1},

    // [:true, :scope, :true, :scope, .foo]
    {".foo > :scope > :scope", 0},
    {".foo > :scope > :scope", 2},

    // [:true, &, :true, :scope]
    {".a :scope > &", 0},
    {".a :scope > &", 2},

    // [:true, &]
    {"&", 0},

    // [:true, &, :true, &, :true, &]
    {"& & &", 0},
    {"& & &", 2},
    {"& & &", 4},
};

class ScopeActivationTest
    : public ::testing::TestWithParam<ScopeActivationData> {};

INSTANTIATE_TEST_SUITE_P(CSSSelectorParserTest,
                         ScopeActivationTest,
                         testing::ValuesIn(scope_activation_data));

TEST_P(ScopeActivationTest, All) {
  test::TaskEnvironment task_environment;
  ScopeActivationData param = GetParam();
  SCOPED_TRACE(param.inner_rule);

  CSSSelectorList* list = ParseNested(param.inner_rule, CSSNestingType::kScope);
  ASSERT_TRUE(list);
  ASSERT_TRUE(list->First());
  const CSSSelector* selector = NthSimpleSelector(*list->First(), param.index);
  ASSERT_TRUE(selector);
  SCOPED_TRACE(selector->SimpleSelectorTextForDebug().Utf8());
  EXPECT_EQ(CSSSelector::kPseudoTrue, selector->GetPseudoType());
  EXPECT_EQ(CSSSelector::kScopeActivation, selector->Relation());
}

// Returns the number of simple selectors that match `predicate`, including
// selectors within nested selector lists (e.g. :is()).
template <typename PredicateFunc>
static wtf_size_t CountSimpleSelectors(const CSSSelectorList& list,
                                       PredicateFunc predicate) {
  wtf_size_t count = 0;
  for (const CSSSelector* selector = list.First(); selector;
       selector = CSSSelectorList::Next(*selector)) {
    for (const CSSSelector* s = selector; s; s = s->NextSimpleSelector()) {
      if (s->SelectorList()) {
        count += CountSimpleSelectors(*s->SelectorList(), predicate);
      }
      if (predicate(*s)) {
        ++count;
      }
    }
  }
  return count;
}

template <typename PredicateFunc>
static std::optional<wtf_size_t> CountSimpleSelectors(
    String selector_text,
    CSSNestingType nesting_type,
    PredicateFunc predicate) {
  CSSSelectorList* list = ParseNested(selector_text, nesting_type);
  if (!list || !list->First()) {
    return std::nullopt;
  }
  return CountSimpleSelectors<PredicateFunc>(*list, predicate);
}

static std::optional<wtf_size_t> CountPseudoTrue(String selector_text,
                                                 CSSNestingType nesting_type) {
  return CountSimpleSelectors(
      selector_text, nesting_type, [](const CSSSelector& selector) {
        return selector.GetPseudoType() == CSSSelector::kPseudoTrue;
      });
}

static std::optional<wtf_size_t> CountScopeActivations(
    String selector_text,
    CSSNestingType nesting_type) {
  return CountSimpleSelectors(
      selector_text, nesting_type, [](const CSSSelector& selector) {
        return selector.Relation() == CSSSelector::kScopeActivation;
      });
}

static std::optional<wtf_size_t> CountPseudoTrueWithScopeActivation(
    String selector_text,
    CSSNestingType nesting_type) {
  return CountSimpleSelectors(
      selector_text, nesting_type, [](const CSSSelector& selector) {
        return selector.GetPseudoType() == CSSSelector::kPseudoTrue &&
               selector.Relation() == CSSSelector::kScopeActivation;
      });
}

TEST(CSSSelectorParserTest, CountMatchesSelfTest) {
  test::TaskEnvironment task_environment;
  auto is_focus = [](const CSSSelector& selector) {
    return selector.GetPseudoType() == CSSSelector::kPseudoFocus;
  };
  auto is_hover = [](const CSSSelector& selector) {
    return selector.GetPseudoType() == CSSSelector::kPseudoHover;
  };
  EXPECT_EQ(2u, CountSimpleSelectors(":focus > .a > :focus",
                                     CSSNestingType::kNone, is_focus));
  EXPECT_EQ(3u, CountSimpleSelectors(":focus > .a > :focus, .b, :focus",
                                     CSSNestingType::kNone, is_focus));
  EXPECT_EQ(0u,
            CountSimpleSelectors(".a > .b", CSSNestingType::kNone, is_focus));
  EXPECT_EQ(4u,
            CountSimpleSelectors(":hover > :is(:hover, .a, :hover) > :hover",
                                 CSSNestingType::kNone, is_hover));
}

struct ScopeActivationCountData {
  // The selector text, e.g. ".a .b > .c".
  const char* selector_text;
  // The expected number of :true pseudo-classes with relation=kScopeActivation
  // if the selector is parsed with CSSNestingType::kScope.
  wtf_size_t pseudo_count;
};

ScopeActivationCountData scope_activation_count_data[] = {
    // Implicit :scope with descendant combinator:
    {".a", 1},
    {".a .b", 1},
    {".a .b > .c", 1},

    // Implicit :scope for relative selectors:
    {"> .a", 1},
    {"> .a .b", 1},
    {"> .a .b > .c", 1},

    // Explicit :scope top-level:
    {":scope", 1},
    {".a :scope", 1},
    {".a > :scope > .b", 1},
    {":scope > :scope", 2},
    {":scope > .a > :scope", 2},

    // :scope in inner selector lists:
    {".a > :is(.b, :scope, .c) .d", 1},
    {".a > :not(.b, :scope, .c) .d", 1},
    {".a > :is(.b, :scope, .c):scope .d", 1},
    {".a > :is(.b, :scope, .c):scope .d:scope", 2},
    {".a > :is(.b, :scope, :scope, .c):scope .d:scope", 2},
    {".a > :has(> :scope):scope > .b", 1},

    // As the previous section, but using '&' instead of :scope.
    {".a > :is(.b, &, .c) .d", 1},
    {".a > :not(.b, &, .c) .d", 1},
    {".a > :is(.b, &, .c)& .d", 1},
    {".a > :is(.b, &, .c)& .d&", 2},
    {".a > :is(.b, &, &, .c)& .d&", 2},
    {".a > :has(> &)& > .b", 1},
};

class ScopeActivationCountTest
    : public ::testing::TestWithParam<ScopeActivationCountData> {
 private:
  test::TaskEnvironment task_environment_;
};

INSTANTIATE_TEST_SUITE_P(CSSSelectorParserTest,
                         ScopeActivationCountTest,
                         testing::ValuesIn(scope_activation_count_data));

TEST_P(ScopeActivationCountTest, Scope) {
  ScopeActivationCountData param = GetParam();
  SCOPED_TRACE(param.selector_text);

  // We expect :true and kScopeActivation to only occur ever occur together.
  EXPECT_EQ(param.pseudo_count,
            CountPseudoTrue(param.selector_text, CSSNestingType::kScope));
  EXPECT_EQ(param.pseudo_count,
            CountScopeActivations(param.selector_text, CSSNestingType::kScope));
  EXPECT_EQ(param.pseudo_count,
            CountPseudoTrueWithScopeActivation(param.selector_text,
                                               CSSNestingType::kScope));
}

TEST_P(ScopeActivationCountTest, Nesting) {
  ScopeActivationCountData param = GetParam();
  SCOPED_TRACE(param.selector_text);

  // We do not expect any inserted :true/kScopeActivation for kNesting.
  EXPECT_EQ(0u, CountPseudoTrue(param.selector_text, CSSNestingType::kNesting));
  EXPECT_EQ(
      0u, CountScopeActivations(param.selector_text, CSSNestingType::kNesting));
  EXPECT_EQ(0u, CountPseudoTrueWithScopeActivation(param.selector_text,
                                                   CSSNestingType::kNesting));
}

TEST_P(ScopeActivationCountTest, None) {
  ScopeActivationCountData param = GetParam();
  SCOPED_TRACE(param.selector_text);

  // We do not expect any inserted :true/kScopeActivation for kNone. Note that
  // relative selectors do not parse for kNone.
  EXPECT_EQ(
      0u,
      CountPseudoTrue(param.selector_text, CSSNestingType::kNone).value_or(0));
  EXPECT_EQ(0u,
            CountScopeActivations(param.selector_text, CSSNestingType::kNone)
                .value_or(0));
  EXPECT_EQ(0u, CountPseudoTrueWithScopeActivation(param.selector_text,
                                                   CSSNestingType::kNone)
                    .value_or(0));
}

}  // namespace blink
