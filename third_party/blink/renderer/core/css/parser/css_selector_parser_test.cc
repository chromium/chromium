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
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

HeapVector<CSSSelector> ParseSelector(String s) {
  HeapVector<CSSSelector> arena;
  CSSParserTokenStream stream(s);
  base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
      stream,
      MakeGarbageCollected<CSSParserContext>(
          kUASheetMode, SecureContextMode::kInsecureContext),
      CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr,
      /*semicolon_aborts_nested_selector=*/false, nullptr, arena);
  return HeapVector<CSSSelector>(vector);
}

}  // namespace

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
      {"html::view-transition-group-children(foo)", true, "foo",
       CSSSelector::kPseudoViewTransitionGroupChildren},
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
        /*semicolon_aborts_nested_selector=*/false, nullptr, arena);
    EXPECT_EQ(!vector.empty(), test_case.valid);
    if (!test_case.valid) {
      continue;
    }

    CSSSelectorList* list = CSSSelectorList::AdoptSelectorVector(vector);
    ASSERT_TRUE(list->IsSingleComplexSelector());

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
        /*parent_rule_for_nesting=*/nullptr,
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
        /*parent_rule_for_nesting=*/nullptr,
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
        /*parent_rule_for_nesting=*/nullptr,
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
        /*parent_rule_for_nesting=*/nullptr,
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
      {"div::scroll-button(up)", CSSSelector::kPseudoScrollButton},
      {"div::scroll-button(left)", CSSSelector::kPseudoScrollButton},
      {"div::scroll-button(*)", CSSSelector::kPseudoScrollButton},
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

        /*semicolon_aborts_nested_selector=*/false, nullptr, arena);
    EXPECT_TRUE(!vector.empty());

    CSSSelectorList* list = CSSSelectorList::AdoptSelectorVector(vector);
    ASSERT_TRUE(list->IsSingleComplexSelector());

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

        /*semicolon_aborts_nested_selector=*/false, nullptr, arena);

    if (test_case.type == CSSSelector::kPseudoUnknown) {
      EXPECT_TRUE(vector.empty());
      return;
    }

    EXPECT_TRUE(!vector.empty());

    CSSSelectorList* list = CSSSelectorList::AdoptSelectorVector(vector);
    ASSERT_TRUE(list->IsSingleComplexSelector());

    const CSSSelector* selector = list->First();
    while (selector->NextSimpleSelector()) {
      selector = selector->NextSimpleSelector();
    }

    EXPECT_EQ(selector->GetPseudoType(), test_case.type);
  }
}

TEST(CSSSelectorParserTest, PseudoChild_Before_FeatureDisabled) {
  ScopedCSSLogicalCombinationPseudoForTest scoped_feature(false);
  test::TaskEnvironment task_environment;

  HeapVector<CSSSelector> vector = ParseSelector("div::before");
  ASSERT_EQ(2u, vector.size());

  // div
  EXPECT_EQ(CSSSelector::MatchType::kTag, vector[0].Match());
  EXPECT_EQ(CSSSelector::RelationType::kSubSelector, vector[0].Relation());

  // ::before
  EXPECT_EQ(CSSSelector::MatchType::kPseudoElement, vector[1].Match());
  EXPECT_EQ(CSSSelector::RelationType::kSubSelector, vector[1].Relation());
  EXPECT_EQ(CSSSelector::PseudoType::kPseudoBefore, vector[1].GetPseudoType());
}

TEST(CSSSelectorParserTest, PseudoChild_Before) {
  ScopedCSSLogicalCombinationPseudoForTest scoped_feature(true);
  test::TaskEnvironment task_environment;

  HeapVector<CSSSelector> vector = ParseSelector("div::before");
  ASSERT_EQ(2u, vector.size());

  // ::before
  EXPECT_EQ(CSSSelector::MatchType::kPseudoElement, vector[0].Match());
  EXPECT_EQ(CSSSelector::RelationType::kPseudoChild, vector[0].Relation());
  EXPECT_EQ(CSSSelector::PseudoType::kPseudoBefore, vector[0].GetPseudoType());

  // div
  EXPECT_EQ(CSSSelector::MatchType::kTag, vector[1].Match());
  EXPECT_EQ(CSSSelector::RelationType::kSubSelector, vector[1].Relation());
}

TEST(CSSSelectorParserTest, PseudoChild_After) {
  ScopedCSSLogicalCombinationPseudoForTest scoped_feature(true);
  test::TaskEnvironment task_environment;

  HeapVector<CSSSelector> vector = ParseSelector("div::after");
  ASSERT_EQ(2u, vector.size());

  // ::after
  EXPECT_EQ(CSSSelector::MatchType::kPseudoElement, vector[0].Match());
  EXPECT_EQ(CSSSelector::RelationType::kPseudoChild, vector[0].Relation());
  EXPECT_EQ(CSSSelector::PseudoType::kPseudoAfter, vector[0].GetPseudoType());

  // div
  EXPECT_EQ(CSSSelector::MatchType::kTag, vector[1].Match());
  EXPECT_EQ(CSSSelector::RelationType::kSubSelector, vector[1].Relation());
}

TEST(CSSSelectorParserTest, PseudoChild_BeforeMarker) {
  ScopedCSSLogicalCombinationPseudoForTest scoped_feature(true);
  test::TaskEnvironment task_environment;

  HeapVector<CSSSelector> vector = ParseSelector("div::before::marker");
  ASSERT_EQ(3u, vector.size());

  // ::marker
  EXPECT_EQ(CSSSelector::MatchType::kPseudoElement, vector[0].Match());
  EXPECT_EQ(CSSSelector::RelationType::kPseudoChild, vector[0].Relation());
  EXPECT_EQ(CSSSelector::PseudoType::kPseudoMarker, vector[0].GetPseudoType());

  // ::before
  EXPECT_EQ(CSSSelector::MatchType::kPseudoElement, vector[1].Match());
  EXPECT_EQ(CSSSelector::RelationType::kPseudoChild, vector[1].Relation());
  EXPECT_EQ(CSSSelector::PseudoType::kPseudoBefore, vector[1].GetPseudoType());

  // div
  EXPECT_EQ(CSSSelector::MatchType::kTag, vector[2].Match());
  EXPECT_EQ(CSSSelector::RelationType::kSubSelector, vector[2].Relation());
}

TEST(CSSSelectorParserTest, PseudoChild_UniversalOriginating) {
  ScopedCSSLogicalCombinationPseudoForTest scoped_feature(true);
  test::TaskEnvironment task_environment;

  HeapVector<CSSSelector> vector = ParseSelector("*::after");
  ASSERT_EQ(2u, vector.size());

  // ::after
  EXPECT_EQ(CSSSelector::MatchType::kPseudoElement, vector[0].Match());
  EXPECT_EQ(CSSSelector::RelationType::kPseudoChild, vector[0].Relation());
  EXPECT_EQ(CSSSelector::PseudoType::kPseudoAfter, vector[0].GetPseudoType());

  // *
  EXPECT_EQ(CSSSelector::MatchType::kUniversalTag, vector[1].Match());
  EXPECT_EQ(CSSSelector::RelationType::kSubSelector, vector[1].Relation());
}

TEST(CSSSelectorParserTest, PseudoChild_NoOriginating) {
  ScopedCSSLogicalCombinationPseudoForTest scoped_feature(true);
  test::TaskEnvironment task_environment;

  HeapVector<CSSSelector> vector = ParseSelector("::after");
  ASSERT_EQ(2u, vector.size());

  // ::after
  EXPECT_EQ(CSSSelector::MatchType::kPseudoElement, vector[0].Match());
  EXPECT_EQ(CSSSelector::RelationType::kPseudoChild, vector[0].Relation());
  EXPECT_EQ(CSSSelector::PseudoType::kPseudoAfter, vector[0].GetPseudoType());

  // * (implicitly inserted)
  EXPECT_EQ(CSSSelector::MatchType::kUniversalTag, vector[1].Match());
  EXPECT_EQ(CSSSelector::RelationType::kSubSelector, vector[1].Relation());
}

TEST(CSSSelectorParserTest, PseudoChild_InPseudoIs) {
  ScopedCSSLogicalCombinationPseudoForTest scoped_feature(true);
  test::TaskEnvironment task_environment;

  HeapVector<CSSSelector> vector = ParseSelector(":is(div::after)");
  ASSERT_EQ(1u, vector.size());

  // :is()
  EXPECT_EQ(CSSSelector::MatchType::kPseudoClass, vector[0].Match());
  EXPECT_EQ(CSSSelector::RelationType::kSubSelector, vector[0].Relation());
  EXPECT_EQ(CSSSelector::PseudoType::kPseudoIs, vector[0].GetPseudoType());
  ASSERT_TRUE(vector[0].SelectorList());

  // Inside :is():

  // ::after
  const CSSSelector* first = vector[0].SelectorList()->First();
  ASSERT_TRUE(first);
  EXPECT_EQ(CSSSelector::MatchType::kPseudoElement, first->Match());
  EXPECT_EQ(CSSSelector::RelationType::kPseudoChild, first->Relation());
  EXPECT_EQ(CSSSelector::PseudoType::kPseudoAfter, first->GetPseudoType());

  // div
  const CSSSelector* second = first->NextSimpleSelector();
  ASSERT_TRUE(second);
  EXPECT_EQ(CSSSelector::MatchType::kTag, second->Match());
  EXPECT_EQ(CSSSelector::RelationType::kSubSelector, second->Relation());
}

TEST(CSSSelectorParserTest, PseudoChild_InPseudoWhere) {
  ScopedCSSLogicalCombinationPseudoForTest scoped_feature(true);
  test::TaskEnvironment task_environment;

  HeapVector<CSSSelector> vector = ParseSelector(":where(div::after)");
  ASSERT_EQ(1u, vector.size());

  // :where()
  EXPECT_EQ(CSSSelector::MatchType::kPseudoClass, vector[0].Match());
  EXPECT_EQ(CSSSelector::RelationType::kSubSelector, vector[0].Relation());
  EXPECT_EQ(CSSSelector::PseudoType::kPseudoWhere, vector[0].GetPseudoType());
  ASSERT_TRUE(vector[0].SelectorList());

  // Inside :where():

  // ::after
  const CSSSelector* first = vector[0].SelectorList()->First();
  ASSERT_TRUE(first);
  EXPECT_EQ(CSSSelector::MatchType::kPseudoElement, first->Match());
  EXPECT_EQ(CSSSelector::RelationType::kPseudoChild, first->Relation());
  EXPECT_EQ(CSSSelector::PseudoType::kPseudoAfter, first->GetPseudoType());

  // div
  const CSSSelector* second = first->NextSimpleSelector();
  ASSERT_TRUE(second);
  EXPECT_EQ(CSSSelector::MatchType::kTag, second->Match());
  EXPECT_EQ(CSSSelector::RelationType::kSubSelector, second->Relation());
}

TEST(CSSSelectorParserTest, PseudoChild_InPseudoNot) {
  ScopedCSSLogicalCombinationPseudoForTest scoped_feature(true);
  test::TaskEnvironment task_environment;

  HeapVector<CSSSelector> vector = ParseSelector(":not(div::after)");
  ASSERT_EQ(1u, vector.size());

  // :not()
  EXPECT_EQ(CSSSelector::MatchType::kPseudoClass, vector[0].Match());
  EXPECT_EQ(CSSSelector::RelationType::kSubSelector, vector[0].Relation());
  EXPECT_EQ(CSSSelector::PseudoType::kPseudoNot, vector[0].GetPseudoType());
  ASSERT_TRUE(vector[0].SelectorList());

  // Inside :not():

  // ::after
  const CSSSelector* first = vector[0].SelectorList()->First();
  ASSERT_TRUE(first);
  EXPECT_EQ(CSSSelector::MatchType::kPseudoElement, first->Match());
  EXPECT_EQ(CSSSelector::RelationType::kPseudoChild, first->Relation());
  EXPECT_EQ(CSSSelector::PseudoType::kPseudoAfter, first->GetPseudoType());

  // div
  const CSSSelector* second = first->NextSimpleSelector();
  ASSERT_TRUE(second);
  EXPECT_EQ(CSSSelector::MatchType::kTag, second->Match());
  EXPECT_EQ(CSSSelector::RelationType::kSubSelector, second->Relation());
}

TEST(CSSSelectorParserTest, PseudoChild_InPseudoList_FeatureDisabled) {
  ScopedCSSLogicalCombinationPseudoForTest scoped_feature(false);
  test::TaskEnvironment task_environment;

  // Note: :is()/:where() parses a *forgiving* selector list,
  // which means an invalid argument doesn't make the outer selector
  // invalid.

  // :is()
  HeapVector<CSSSelector> is = ParseSelector(":is(div::after)");
  ASSERT_EQ(1u, is.size());
  ASSERT_TRUE(is[0].SelectorList());
  EXPECT_FALSE(is[0].SelectorList()->IsValid());

  // :where()
  HeapVector<CSSSelector> where = ParseSelector(":where(div::after)");
  ASSERT_EQ(1u, where.size());
  ASSERT_TRUE(where[0].SelectorList());
  EXPECT_FALSE(where[0].SelectorList()->IsValid());

  // :not() (unforgiving)
  EXPECT_TRUE(ParseSelector(":not(div::after)").empty());
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
        /*parent_rule_for_nesting=*/nullptr,
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
        /*parent_rule_for_nesting=*/nullptr,
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
        /*parent_rule_for_nesting=*/nullptr,
        /*semicolon_aborts_nested_selector=*/false, sheet, arena);
    CSSSelectorList* list = CSSSelectorList::AdoptSelectorVector(vector);
    EXPECT_TRUE(list->IsValid());
    const CSSSelector* selector = list->First();
    for (auto sub_expectation : test_case.expectation) {
      ASSERT_TRUE(selector);
      AtomicString selector_value =
          (selector->Match() == CSSSelector::kTag ||
           selector->Match() == CSSSelector::kUniversalTag)
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

    // restrict use of pseudo-element inside :has()
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
    // :has() is not allowed after pseudo-elements:
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
  CSSSelectorList* list = css_test_helpers::ParseSelectorList(
      inner_rule, nesting_type, parent_rule_for_nesting);
  if (!list || !list->First()) {
    return nullptr;
  }
  return list;
}

static std::optional<CSSSelector> GetImplicitlyAddedSelector(
    String inner_rule,
    CSSNestingType nesting_type) {
  CSSSelectorList* list = ParseNested(inner_rule, nesting_type);
  if (!list) {
    return std::nullopt;
  }

  const CSSSelector* leftmost_simple = nullptr;
  for (const CSSSelector* selector = list->First(); selector;
       selector = selector->NextSimpleSelector()) {
    leftmost_simple = selector;
  }

  if (!leftmost_simple ||
      leftmost_simple->Match() != CSSSelector::kPseudoClass ||
      !leftmost_simple->IsImplicit()) {
    return std::nullopt;
  }
  return *leftmost_simple;
}

static std::optional<CSSSelector::PseudoType> GetImplicitlyAddedPseudo(
    String inner_rule,
    CSSNestingType nesting_type) {
  std::optional<CSSSelector> implicit_selector =
      GetImplicitlyAddedSelector(inner_rule, nesting_type);
  if (!implicit_selector.has_value()) {
    return std::nullopt;
  }
  return implicit_selector->GetPseudoType();
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

// See IsScopeContainingData.
//
// Creates a selector equivalent to to `selector_text`, except inserting
// an empty :where() at each point indicated by `arrows`. The empty :where()
// selectors are used by IsScopeContainingComparison as signals for when
// IsScopeContaining==true is expected.
static String CreateReferenceSelectorForScopeContaining(String selector_text,
                                                        String arrows) {
  CHECK_EQ(selector_text.length(), arrows.length());
  StringBuilder builder;
  for (wtf_size_t i = 0; i < selector_text.length(); ++i) {
    if (arrows[i] == '^') {
      builder.Append(":where()");
    }
    builder.Append(selector_text[i]);
  }
  return builder.ToString();
}

static HeapVector<CSSSelector> FlattenSelector(const CSSSelector* selector) {
  HeapVector<CSSSelector> result;
  while (selector) {
    result.push_back(*selector);
    if (const CSSSelectorList* list = selector->SelectorList()) {
      for (const CSSSelector* s = list->First(); s;
           s = CSSSelectorList::Next(*s)) {
        result.AppendVector(FlattenSelector(s));
      }
    }
    selector = selector->NextSimpleSelector();
  }
  return result;
}

static bool IsScopeContainingComparison(HeapVector<CSSSelector> actual,
                                        HeapVector<CSSSelector> ref) {
  actual.Reverse();
  ref.Reverse();
  // [actual,ref].back() now holds the first CSSSelector produced
  // by FlattenSelector.

  while (!actual.empty()) {
    bool at_arrow = (ref.back().GetPseudoType() == CSSSelector::kPseudoWhere) &&
                    !ref.back().SelectorList()->IsValid();
    if (at_arrow) {
      ref.pop_back();
      CHECK(!ref.empty());
    }
    if (actual.back().IsScopeContaining() != at_arrow) {
      DLOG(ERROR) << "Unexpected value for IsScopeContaining:" << " expected="
                  << at_arrow << " actual=" << actual.back().IsScopeContaining()
                  << " selector=" << actual.back().SimpleSelectorTextForDebug();
      return false;
    }
    actual.pop_back();
    ref.pop_back();
  }

  return ref.empty();
}

struct IsScopeContainingData {
  // The selector text, e.g. ".a .b > .c".
  const char* selector_text;
  // A string of the same length as `selector_text`, where each '^' indicates
  // a simple selector which has the IsScopeContaining flag set.
  const char* arrows;
};

IsScopeContainingData scope_containing_data[] = {
    // No IsScopeContaining flags set:
    {
        ".a",
        "  ",
    },
    {
        "div > .a",
        "        ",
    },
    {
        "div > :is(.b, main) ~ .a",
        "                        ",
    },

    // Explicit :scope top-level:
    {
        ":scope",
        "^     ",
    },
    {
        ".a :scope",
        "   ^     ",
    },
    {
        ".a > :scope > .b",
        "     ^          ",
    },
    {
        ":scope > :scope",
        "^        ^     ",
    },
    {
        ":scope > .a > :scope",
        "^             ^     ",
    },

    // :scope in inner selector lists:
    {
        ".a > :is(.b, :scope, .c) .d",
        "     ^       ^             ",
    },
    {
        ".a > :not(.b, :scope, .c) .d",
        "     ^        ^             ",
    },
    {
        ".a > :is(.b, :scope, .c):scope .d",
        "     ^       ^          ^        ",
    },
    {
        ".a > :is(.b, :scope, .c):scope .d:scope",
        "     ^       ^          ^        ^     ",
    },
    {
        ".a > :is(.b, :scope, :scope, .c):scope .d:scope",
        "     ^       ^       ^          ^        ^     ",
    },
    {
        ".a > :has(> :scope):scope > .b",
        "     ^      ^      ^          ",
    },

    // As the previous section, but using '&' instead of :scope.
    {
        ".a > :is(.b, &, .c) .d",
        "     ^       ^        ",
    },
    {
        ".a > :not(.b, &, .c) .d",
        "     ^        ^        ",
    },
    {
        ".a > :is(.b, &, .c)& .d",
        "     ^       ^     ^   ",
    },
    {
        ".a > :is(.b, &, .c)& .d&",
        "     ^       ^     ^   ^",
    },
    {
        ".a > :is(.b, &, &, .c)& .d&",
        "     ^       ^  ^     ^   ^",
    },
    {
        ".a > :has(> &)& > .b",
        "     ^      ^ ^     ",
    },
};

class IsScopeContainingTest
    : public ::testing::TestWithParam<IsScopeContainingData> {
 private:
  test::TaskEnvironment task_environment_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         IsScopeContainingTest,
                         testing::ValuesIn(scope_containing_data));

TEST_P(IsScopeContainingTest, RefTest) {
  IsScopeContainingData param = GetParam();
  SCOPED_TRACE(param.arrows);
  SCOPED_TRACE(param.selector_text);
  SCOPED_TRACE("'^' indicates where IsScopeContaining==true was expected");
  ASSERT_EQ(strlen(param.selector_text), strlen(param.arrows));

  String ref = CreateReferenceSelectorForScopeContaining(param.selector_text,
                                                         param.arrows);
  CSSSelectorList* actual_list =
      ParseNested(param.selector_text, CSSNestingType::kNone);
  CSSSelectorList* ref_list = ParseNested(ref, CSSNestingType::kNone);
  ASSERT_TRUE(actual_list);
  ASSERT_TRUE(actual_list->First());
  ASSERT_TRUE(ref_list);
  ASSERT_TRUE(ref_list->First());
  EXPECT_TRUE(IsScopeContainingComparison(FlattenSelector(actual_list->First()),
                                          FlattenSelector(ref_list->First())));
}

TEST(CSSSelectorParserTest, ImplicitSelectorIsScopeContaining) {
  test::TaskEnvironment task_environment;
  EXPECT_TRUE(GetImplicitlyAddedSelector("> .a", CSSNestingType::kNesting)
                  .value_or(CSSSelector())
                  .IsScopeContaining());
  EXPECT_TRUE(GetImplicitlyAddedSelector(".a", CSSNestingType::kNesting)
                  .value_or(CSSSelector())
                  .IsScopeContaining());
  EXPECT_TRUE(GetImplicitlyAddedSelector("> .a", CSSNestingType::kScope)
                  .value_or(CSSSelector())
                  .IsScopeContaining());
  EXPECT_TRUE(GetImplicitlyAddedSelector(".a", CSSNestingType::kScope)
                  .value_or(CSSSelector())
                  .IsScopeContaining());
}

// Helper function for :lang() parsing validation tests
bool ValidateLang(const String& selector_text) {
  CSSSelectorList* selector_list =
      css_test_helpers::ParseSelectorList(selector_text);
  return selector_list && selector_list->First();
}

// This class is used to validate :lang() parsing against the RFC 4647 basic
// language range grammar, regardless of the value of CSSLangExtendedRanges.
// language-range = (1*8ALPHA *("-" 1*8alphanum)) / "*"
class LangParsingInvariantTest : public testing::TestWithParam<bool>,
                                 public ScopedCSSLangExtendedRangesForTest {
 public:
  LangParsingInvariantTest() : ScopedCSSLangExtendedRangesForTest(GetParam()) {}

 private:
  test::TaskEnvironment task_environment_;
};

INSTANTIATE_TEST_SUITE_P(CSSSelectorParser,
                         LangParsingInvariantTest,
                         testing::Bool());

// Test class for values that only parse when the runtime flag is enabled.
class LangParsingFlagDependentTest : public testing::TestWithParam<bool>,
                                     public ScopedCSSLangExtendedRangesForTest {
 public:
  LangParsingFlagDependentTest()
      : ScopedCSSLangExtendedRangesForTest(GetParam()) {}

 private:
  test::TaskEnvironment task_environment_;
};

INSTANTIATE_TEST_SUITE_P(CSSSelectorParser,
                         LangParsingFlagDependentTest,
                         testing::Bool());

TEST_P(LangParsingInvariantTest, EmptyTest) {
  EXPECT_FALSE(ValidateLang(":lang()"));
}

// A CSS ident containing a valid language range.
TEST_P(LangParsingInvariantTest, LanguageRangeIdentTest) {
  EXPECT_TRUE(ValidateLang(":lang(en)"));
  EXPECT_TRUE(ValidateLang(":lang(pt-BR)"));
  EXPECT_TRUE(ValidateLang(":lang(zh-Hant)"));
  EXPECT_TRUE(ValidateLang(":lang(zh-cmn-Hans-CN)"));

  // Whitespace around the ident is ignored.
  EXPECT_TRUE(ValidateLang(":lang( en)"));
  EXPECT_TRUE(ValidateLang(":lang(en )"));
  EXPECT_TRUE(ValidateLang(":lang( en )"));
}

// A CSS ident can contain wildcards as long as they are escaped.
TEST_P(LangParsingInvariantTest, EscapedWildcardsLanguageRangeIdentTest) {
  EXPECT_TRUE(ValidateLang(":lang(\\*)"));
  EXPECT_TRUE(ValidateLang(":lang(\\*-US)"));
  EXPECT_TRUE(ValidateLang(":lang(en-\\*)"));
  EXPECT_TRUE(ValidateLang(":lang(\\*-\\*)"));
}

// A CSS ident containing a malformed range should be accepted by the parser.
TEST_P(LangParsingInvariantTest, MalformedLanguageRangeIdentTest) {
  // Hyphens in unexpected positions.
  EXPECT_TRUE(ValidateLang(":lang(--)"));
  EXPECT_TRUE(ValidateLang(":lang(-en)"));
  EXPECT_TRUE(ValidateLang(":lang(en-)"));
  EXPECT_TRUE(ValidateLang(":lang(en--US)"));
  EXPECT_TRUE(ValidateLang(":lang(en--23)"));
  EXPECT_TRUE(ValidateLang(":lang(--2)"));

  // Numbers in first tag.
  EXPECT_TRUE(ValidateLang(":lang(en123)"));
  EXPECT_TRUE(ValidateLang(":lang(e123n)"));

  // Tag too long.
  EXPECT_TRUE(ValidateLang(":lang(ninechars)"));
  EXPECT_TRUE(ValidateLang(":lang(en-123456789)"));
  EXPECT_TRUE(ValidateLang(":lang(en-ninechars)"));
  EXPECT_TRUE(ValidateLang(":lang(en-US-ninechars)"));
}

// A CSS ident containing invalid characters should be accepted by the parser.
TEST_P(LangParsingInvariantTest, InvalidCharsLanguageRangeIdentTest) {
  // Non-ASCII characters.
  EXPECT_TRUE(ValidateLang(":lang(café)"));
  EXPECT_TRUE(ValidateLang(":lang(es-España)"));
  EXPECT_TRUE(ValidateLang(":lang(日本語)"));

  // Underscore and escaped special characters.
  EXPECT_TRUE(ValidateLang(":lang(en_US)"));
  EXPECT_TRUE(ValidateLang(":lang( my\\.thing )"));
  EXPECT_TRUE(ValidateLang(":lang( you\\&me )"));
  EXPECT_TRUE(ValidateLang(":lang( j\\ a )"));
  EXPECT_TRUE(ValidateLang(":lang(me\\ \\&\\ you)"));
}

// Content is not a valid CSS ident.
TEST_P(LangParsingInvariantTest, NotIdentTest) {
  // Single hyphen is not a valid ident.
  EXPECT_FALSE(ValidateLang(":lang(-)"));

  // Hyphen followed by digit or wildcard.
  EXPECT_FALSE(ValidateLang(":lang(-1)"));
  EXPECT_FALSE(ValidateLang(":lang(-*)"));

  // Digit at start.
  EXPECT_FALSE(ValidateLang(":lang(3en)"));
  EXPECT_FALSE(ValidateLang(":lang(1-en)"));
  EXPECT_FALSE(ValidateLang(":lang(3.14)"));

  // Unescaped wildcards and other special characters.
  EXPECT_FALSE(ValidateLang(":lang(*)"));
  EXPECT_FALSE(ValidateLang(":lang(en-*)"));
  EXPECT_FALSE(ValidateLang(":lang(en*US)"));
  EXPECT_FALSE(ValidateLang(":lang(+)"));
  EXPECT_FALSE(ValidateLang(":lang(.)"));
  EXPECT_FALSE(ValidateLang(":lang(!)"));
  EXPECT_FALSE(ValidateLang(":lang(@)"));

  // Space in the middle.
  EXPECT_FALSE(ValidateLang(":lang( - en )"));
  EXPECT_FALSE(ValidateLang(":lang( en - )"));
  EXPECT_FALSE(ValidateLang(":lang( en -US )"));
  EXPECT_FALSE(ValidateLang(":lang( en- US )"));
  EXPECT_FALSE(ValidateLang(":lang( en - US )"));

  // Invalid comma usage.
  EXPECT_FALSE(ValidateLang(":lang(,)"));
  EXPECT_FALSE(ValidateLang(":lang(en,)"));
  EXPECT_FALSE(ValidateLang(":lang(,en)"));
  EXPECT_FALSE(ValidateLang(":lang(en,,fr)"));
  EXPECT_FALSE(ValidateLang(":lang(en, fr,)"));
  EXPECT_FALSE(ValidateLang(":lang(en fr, de)"));

  // Multiple values without comma separator.
  EXPECT_FALSE(ValidateLang(":lang(en fr)"));
  EXPECT_FALSE(ValidateLang(":lang(\"en\" fr)"));
  EXPECT_FALSE(ValidateLang(":lang(en \"fr\")"));
  EXPECT_FALSE(ValidateLang(":lang(\"en\" \"fr\")"));

  // String combined with idents and hyphens.
  EXPECT_FALSE(ValidateLang(":lang(en')"));
  EXPECT_FALSE(ValidateLang(":lang(en\")"));
  EXPECT_FALSE(ValidateLang(":lang(\"en\"- )"));
  EXPECT_FALSE(ValidateLang(":lang(\"en\"-US)"));
  EXPECT_FALSE(ValidateLang(":lang(en-\"US\")"));
  EXPECT_FALSE(ValidateLang(":lang(\"en\"-\"US\")"));

  // Numbers and dimensions.
  EXPECT_FALSE(ValidateLang(":lang(123)"));
  EXPECT_FALSE(ValidateLang(":lang(1e2)"));
  EXPECT_FALSE(ValidateLang(":lang(50% )"));
  EXPECT_FALSE(ValidateLang(":lang(2em )"));
  EXPECT_FALSE(ValidateLang(":lang(#FFF )"));
}

// Values that are not parsed regardless of the runtime flag.
TEST_P(LangParsingInvariantTest, InvalidListValues) {
  EXPECT_FALSE(ValidateLang(":lang(en,  *  )"));
  EXPECT_FALSE(ValidateLang(":lang(en,  -  )"));
  EXPECT_FALSE(ValidateLang(":lang(en, en-*)"));
  EXPECT_FALSE(ValidateLang(":lang(en, 123 )"));
  EXPECT_FALSE(ValidateLang(":lang(en, 1e2 )"));
  EXPECT_FALSE(ValidateLang(":lang(en, 50% )"));
  EXPECT_FALSE(ValidateLang(":lang(en, 2em )"));
  EXPECT_FALSE(ValidateLang(":lang(en, #FFF)"));
}

// Values that only parse when extended lang ranges are enabled.
TEST_P(LangParsingFlagDependentTest, ExtendedLangRangesParsing) {
  // Comma-separated lists.
  EXPECT_EQ(ValidateLang(":lang(en, fr)"), GetParam());
  EXPECT_EQ(ValidateLang(":lang(en-US, fr-FR, ja-JP)"), GetParam());
  EXPECT_EQ(ValidateLang(":lang(en, fr, de)"), GetParam());

  // Lists mixing valid and malformed ranges, as long as all parse as idents.
  EXPECT_EQ(ValidateLang(":lang(my\\.thing, en)"), GetParam());
  EXPECT_EQ(ValidateLang(":lang(fr, en_US, ---)"), GetParam());
  EXPECT_EQ(ValidateLang(":lang( café, en_US, j\\ a )"), GetParam());

  // Strings.
  EXPECT_EQ(ValidateLang(":lang(\"en\")"), GetParam());
  EXPECT_EQ(ValidateLang(":lang(\"\")"), GetParam());
  EXPECT_EQ(ValidateLang(":lang(\"  \")"), GetParam());
  EXPECT_EQ(ValidateLang(":lang(\"*\")"), GetParam());
  EXPECT_EQ(ValidateLang(":lang(\"*-US\")"), GetParam());
  EXPECT_EQ(ValidateLang(":lang(\"en-*\")"), GetParam());
  EXPECT_EQ(ValidateLang(":lang(\"*-*-*\")"), GetParam());

  // Single-quote strings.
  EXPECT_EQ(ValidateLang(":lang('en')"), GetParam());
  EXPECT_EQ(ValidateLang(":lang('*-US')"), GetParam());

  // Strings containing characters that are not allowed unescaped in idents.
  EXPECT_EQ(ValidateLang(":lang(\"en US\")"), GetParam());
  EXPECT_EQ(ValidateLang(":lang(\"en.US\")"), GetParam());

  // List with idents and strings.
  EXPECT_EQ(ValidateLang(":lang(en, \"*-US\")"), GetParam());
  EXPECT_EQ(ValidateLang(":lang(\"*\", en)"), GetParam());
  EXPECT_EQ(ValidateLang(":lang(en, \"fr-*\", ja)"), GetParam());
  EXPECT_EQ(ValidateLang(":lang(\"en\", fr)"), GetParam());

  // List with whitespace.
  EXPECT_EQ(ValidateLang(":lang(  en  ,  fr  ,  de  )"), GetParam());
  EXPECT_EQ(ValidateLang(":lang( \"*\" , en )"), GetParam());
}

}  // namespace blink
