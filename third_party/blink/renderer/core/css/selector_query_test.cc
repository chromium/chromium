// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/selector_query.h"

#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

// Uncomment to run the SelectorQueryTests for stats in a release build.
// #define RELEASE_QUERY_STATS

namespace blink {

namespace {
struct QueryTest {
  const char* selector;
  bool query_all;
  unsigned matches;
  SelectorQuery::QueryStats stats;
};

template <unsigned length>
void RunTests(ContainerNode& scope, const QueryTest (&test_cases)[length]) {
  for (const auto& test_case : test_cases) {
    const char* selector = test_case.selector;
    SCOPED_TRACE(testing::Message()
                 << (test_case.query_all ? "querySelectorAll('"
                                         : "querySelector('")
                 << selector << "')");
    if (test_case.query_all) {
      StaticElementList* match_all =
          scope.QuerySelectorAll(AtomicString(selector));
      EXPECT_EQ(test_case.matches, match_all->length());
    } else {
      Element* match = scope.QuerySelector(AtomicString(selector));
      EXPECT_EQ(test_case.matches, match ? 1u : 0u);
    }
#if DCHECK_IS_ON() || defined(RELEASE_QUERY_STATS)
    EXPECT_EQ(test_case.stats, SelectorQuery::LastQueryStats());
#endif
  }
}
}  // namespace

TEST(SelectorQueryTest, NotMatchingPseudoElement) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  auto* html = MakeGarbageCollected<HTMLHtmlElement>(*document);
  document->AppendChild(html);
  document->documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><style>span::before { content: 'X' }</style><span></span></body>");

  HeapVector<CSSSelector> arena;
  base::span<CSSSelector> selector_vector = CSSParser::ParseSelector(
      MakeGarbageCollected<CSSParserContext>(
          *document, NullUrl(), true /* origin_clean */, Referrer()),
      CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr, nullptr,
      "span::before", arena);
  CSSSelectorList* selector_list =
      CSSSelectorList::AdoptSelectorVector(selector_vector);
  SelectorQuery* query = MakeGarbageCollected<SelectorQuery>(selector_list);
  Element* elm = query->QueryFirst(*document);
  EXPECT_EQ(nullptr, elm);

  selector_vector = CSSParser::ParseSelector(
      MakeGarbageCollected<CSSParserContext>(
          *document, NullUrl(), true /* origin_clean */, Referrer()),
      CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr, nullptr,
      "span", arena);
  selector_list = CSSSelectorList::AdoptSelectorVector(selector_vector);
  query = MakeGarbageCollected<SelectorQuery>(selector_list);
  elm = query->QueryFirst(*document);
  EXPECT_NE(nullptr, elm);
}

TEST(SelectorQueryTest, LastOfTypeNotFinishedParsing) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  auto* html = MakeGarbageCollected<HTMLHtmlElement>(*document);
  document->AppendChild(html);
  document->documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><p></p><p id=last></p></body>");

  document->body()->BeginParsingChildren();

  HeapVector<CSSSelector> arena;
  base::span<CSSSelector> selector_vector = CSSParser::ParseSelector(
      MakeGarbageCollected<CSSParserContext>(
          *document, NullUrl(), true /* origin_clean */, Referrer()),
      CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr, nullptr,
      "p:last-of-type", arena);
  CSSSelectorList* selector_list =
      CSSSelectorList::AdoptSelectorVector(selector_vector);
  SelectorQuery* query = MakeGarbageCollected<SelectorQuery>(selector_list);
  Element* elm = query->QueryFirst(*document);
  ASSERT_TRUE(elm);
  EXPECT_EQ("last", elm->IdForStyleResolution());
}

TEST(SelectorQueryTest, StandardsModeFastPaths) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write(R"HTML(
    <!DOCTYPE html>
    <html>
      <head></head>
      <body>
        <span id=first class=A>
          <span id=a class=one></span>
          <span id=b class=two></span>
          <span id=c class=one></span>
          <div id=multiple class=two></div>
        </span>
        <div>
          <span id=second class=B>
            <span id=A class=one></span>
            <span id=B class=two></span>
            <span id=C class=one></span>
            <span id=D class=three></span>
            <span id=multiple class=two></span>
          </span>
        </div>
      </body>
    </html>
  )HTML");

  // NOTE: If StringHasher changes and we're unlucky,
  // the values for class and attribute selectors may need changing.
  // clang-format off
  static const struct QueryTest kTestCases[] = {
      // ID in rightmost selector.
      // For these, we could in theory optimize away the .recheck_selector for
      // all that have them, since there is only one level and it's not a child combinator.
      {"#A",                 false,  1, {.elements_seen = 1, .fast_id_roots = 1, .check_id = 1}},
      {"#multiple",          false,  1, {.elements_seen = 1, .fast_id_roots = 1, .check_id = 1}},
      {"#multiple.two",      false,  1, {.elements_seen = 1, .fast_id_roots = 1, .check_id = 1, .check_class = 1}},
      {"#multiple",           true,  2, {.elements_seen = 2, .fast_id_roots = 2, .check_id = 2}},
      {"span#multiple",       true,  1, {.elements_seen = 2, .fast_id_roots = 2, .check_id = 2, .check_tag = 2}},
      {"#multiple.two",       true,  2, {.elements_seen = 2, .fast_id_roots = 2, .check_id = 2, .check_class = 2}},
      {"body #multiple",     false,  1, {.elements_seen = 1, .fast_id_roots = 1, .check_id = 1, .recheck_selector = 1}},
      {"body span#multiple", false,  1, {.elements_seen = 2, .fast_id_roots = 2, .check_id = 2, .check_tag = 2, .recheck_selector = 1}},
      {"body #multiple",      true,  2, {.elements_seen = 2, .fast_id_roots = 2, .check_id = 2, .recheck_selector = 2}},
      {"[id=multiple]",       true,  2, {.elements_seen = 2, .fast_id_roots = 2, .check_attr = 2}},
      {"body [id=multiple]",  true,  2, {.elements_seen = 2, .fast_id_roots = 2, .check_attr = 2, .recheck_selector = 2}},

      // Single selector tag.
      {"span",               false,  1, {.elements_seen = 4, .check_tag = 4}},
      {"span",                true, 10, {.elements_seen = 15, .check_tag = 15}},

      // Single selector class. .two manages only to skip <head></head>, really.
      {".two",               false, 1, {.elements_seen = 5, .check_class = 5, .skipped_subtree = 1}},
      {".two",                true, 4, {.elements_seen = 13, .check_class = 13, .skipped_subtree = 2}},
      {".three",             false, 1, {.elements_seen = 5, .check_class = 5, .skipped_subtree = 5}},
      {".does-not-exist",    false, 0, {.skipped_subtree = 1}},

      // Class in the rightmost selector.
      // Note that body satisfies its tag quickly, and thus has
      // fewer check_tag but more check_class.
      {"body .two",          false, 1, {.elements_seen = 5, .check_tag = 2, .check_class = 3, .skipped_subtree = 1}},
      {"div .two",           false, 1, {.elements_seen = 11, .check_tag = 8, .check_class = 3, .skipped_subtree = 1}},
      {"body .two",           true, 4, {.elements_seen = 13, .check_tag = 2, .check_class = 11,
                                        .skipped_subtree = 2}},
      {"div .two",            true, 2, {.elements_seen = 13, .check_tag = 8, .check_class = 5,
                                        .skipped_subtree = 2}},

      // We need to recheck if there is a child combinator, but traversal should otherwise be the same.
      {"body > .two",         true, 0, {.elements_seen = 13, .check_tag = 2, .check_class = 11, .recheck_selector = 4,
                                        .skipped_subtree = 2}},
      {"div > .two",          true, 0, {.elements_seen = 13, .check_tag = 8, .check_class = 5, .recheck_selector = 2,
                                        .skipped_subtree = 2}},

      // ID in an ancestor.
      {"#second .two",       false, 1, {.elements_seen = 3, .fast_id_roots = 1, .check_id = 1, .check_class = 2}},
      {"#second .two",        true, 2, {.elements_seen = 5, .fast_id_roots = 1, .check_id = 1, .check_class = 4, .skipped_subtree = 1}},

      // Class in ancestor.
      {".B span",            false, 1, {.elements_seen = 5, .check_tag = 1, .check_class = 4, .skipped_subtree = 2}},
      {".B span",             true, 5, {.elements_seen = 9, .check_tag = 5, .check_class = 4, .skipped_subtree = 2}},

      // ID selector among siblings. :last-child and the + combinator require full selector checking.
      {"#c + :last-child",   false, 1, {.elements_seen = 2, .fast_id_roots = 1, .check_id = 1, .recheck_selector = 1}},
      {"#a ~ :last-child",   false, 1, {.elements_seen = 4, .fast_id_roots = 1, .check_id = 1, .recheck_selector = 3}},
      {"#c + div",            true, 1, {.elements_seen = 2, .fast_id_roots = 1, .check_id = 1, .check_tag = 1, .recheck_selector = 1}},
      {"#a ~ span",           true, 2, {.elements_seen = 4, .fast_id_roots = 1, .check_id = 1, .check_tag = 3}},

      // With multiple ID selectors, we pick the latest.
      {"#second #C ~ #D",     true, 1, {.elements_seen = 1, .fast_id_roots = 1, .check_id = 1, .recheck_selector = 1}},
      {"#second #C ~ span",   true, 2, {.elements_seen = 3, .fast_id_roots = 1, .check_id = 1, .check_tag = 2, .recheck_selector = 2}},

      // ID selector above siblings are just treated like a normal ancestor.
      {"#first span + span", false, 1, {.elements_seen = 3, .fast_id_roots = 1, .check_id = 1, .check_tag = 2, .recheck_selector = 1}},
      {"#first span ~ span", false, 1, {.elements_seen = 3, .fast_id_roots = 1, .check_id = 1, .check_tag = 2}},
      {"#second span + span", true, 4, {.elements_seen = 6, .fast_id_roots = 1, .check_id = 1, .check_tag = 5, .recheck_selector = 4}},
      {"#second span ~ span", true, 4, {.elements_seen = 6, .fast_id_roots = 1, .check_id = 1, .check_tag = 5}},

      // ID selector with an ancestor rechecks, but otherwise behaves the same
      // (body is discarded).
      {"body #second span ~ span", true, 4,
                                       {.elements_seen = 6, .fast_id_roots = 1, .check_id = 1, .check_tag = 5, .recheck_selector = 4}},

      // TODO(sesse): This is a case we could probably do better, and discard
      // immediately since there is no <doesnotexist> over #second.
      {"doesnotexist #second span ~ span", true, 0,
                                       {.elements_seen = 6, .fast_id_roots = 1, .check_id = 1, .check_tag = 5, .recheck_selector = 4}},

      // Multiple selectors always uses the slow path.
      // TODO(esprehn): We could make this fast if we sorted the output, not
      // sure it's worth it unless we're dealing with ids.
      {"#a, #b",            false, 1, {.recheck_selector = 9, .slow_scan = 5}},
      {"#a, #b",             true, 2, {.recheck_selector = 29, .slow_scan = 15}},
  };
  // clang-format on
  RunTests(*document, kTestCases);
}

TEST(SelectorQueryTest, FastPathScoped) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write(R"HTML(
    <!DOCTYPE html>
    <html id=root-id class=root-class>
      <head></head>
      <body>
        <span id=first>
          <span id=A class='a child'></span>
          <span id=B class='a child'>
              <a class=first></a>
              <a class=second></a>
              <a class=third></a>
          </span>
          <span id=multiple class='b child'></span>
          <span id=multiple class='c child'></span>
        </span>
      </body>
    </html>
  )HTML");
  Element* scope = document->getElementById(AtomicString("first"));
  ASSERT_NE(nullptr, scope);
  ShadowRoot& shadowRoot =
      scope->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  // Make the inside the shadow root be identical to that of the outer document.
  shadowRoot.appendChild(document->documentElement()->cloneNode(/*deep*/ true));

  // NOTE: If StringHasher changes and we're unlucky,
  // the values for .c and .child may need updating.
  // clang-format off
  static const struct QueryTest kTestCases[] = {
      // ID in the rightmost selector.
      {"#first",           false, 0, {.elements_seen = 1, .fast_id_roots = 1}},

      {"#B",               false, 1, {.elements_seen = 1, .fast_id_roots = 1, .check_id = 1}},
      {"#multiple",        false, 1, {.elements_seen = 1, .fast_id_roots = 1, .check_id = 1}},
      {"#multiple.c",      false, 1, {.elements_seen = 1, .fast_id_roots = 2, .check_id = 1, .check_class = 1, .skipped_subtree = 1}},

      // Class in the rightmost selector.
      {".child",           false, 1, {.elements_seen = 1, .check_class = 1}},
      {".child",            true, 4, {.elements_seen = 4, .check_class = 4, .skipped_subtree = 3}},

      // If an ancestor has the class name, we scan all the descendants of the scope
      // (we wouldn't need the recheck here; it could be optimized away in theory).
      // The two .check_class come from scanning ancestors to find .root-class above #first.
      {".root-class span",  true, 4, {.elements_seen = 7, .check_tag = 7, .check_class = 2, .recheck_selector = 4}},

      // If the id is an ancestor, we scan all the descendants.
      {"#root-id span",     true, 4, {.elements_seen = 7, .check_id = 2, .check_tag = 7, .recheck_selector = 4}},
  };
  // clang-format on

  {
    SCOPED_TRACE("Inside document");
    RunTests(*scope, kTestCases);
  }

  {
    // Run all the tests a second time but with a scope inside a shadow root,
    // all the fast paths should behave the same.
    SCOPED_TRACE("Inside shadow root");
    scope = shadowRoot.getElementById(AtomicString("first"));
    ASSERT_NE(nullptr, scope);
    RunTests(*scope, kTestCases);
  }
}

TEST(SelectorQueryTest, QuirksModeSlowPath) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write(R"HTML(
    <html>
      <head></head>
      <body>
        <span id=first>
          <span id=One class=Two></span>
          <span id=one class=tWo></span>
        </span>
      </body>
    </html>
  )HTML");

  // NOTE: If StringHasher changes and we're unlucky,
  // the values for .two may need updating.
  // clang-format off
  static const struct QueryTest kTestCases[] = {
      // Quirks mode can't use the ID fast path due to being case-insensitive.
      {"#one",        false, 1, {.elements_seen = 5, .check_id = 5}},
      {"#One",        false, 1, {.elements_seen = 5, .check_id = 5}},
      {"#ONE",        false, 1, {.elements_seen = 5, .check_id = 5}},
      {"#ONE",         true, 2, {.elements_seen = 6, .check_id = 6}},
      {"[id=One]",    false, 1, {.elements_seen = 5, .check_attr = 5}},
      {"[id=One]",     true, 1, {.elements_seen = 6, .check_attr = 6}},
      {"body #first", false, 1, {.elements_seen = 4, .check_id = 1, .check_tag = 3}},
      {"body #one",    true, 2, {.elements_seen = 6, .check_id = 3, .check_tag = 3}},
      // Quirks can use the class and tag name checks, though.
      {"span",        false, 1, {.elements_seen = 4, .check_tag = 4}},
      {"span",         true, 3, {.elements_seen = 6, .check_tag = 6}},
      {".two",        false, 1, {.elements_seen = 4, .check_class = 4, .skipped_subtree = 1}},
      {".two",         true, 2, {.elements_seen = 5, .check_class = 5, .skipped_subtree = 1}},
      {"body span",   false, 1, {.elements_seen = 4, .check_tag = 4}},
      {"body span",    true, 3, {.elements_seen = 6, .check_tag = 6}},
      {"body .two",   false, 1, {.elements_seen = 4, .check_tag = 2, .check_class = 2, .skipped_subtree = 1}},
      {"body .two",    true, 2, {.elements_seen = 5, .check_tag = 2, .check_class = 3, .skipped_subtree = 1}},
  };
  // clang-format on
  RunTests(*document, kTestCases);
}

TEST(SelectorQueryTest, DisconnectedSubtree) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  Element* scope = document->CreateRawElement(html_names::kDivTag);
  scope->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <section>
      <span id=first>
        <span id=A class=A></span>
        <span id=B class=child></span>
        <span id=multiple class=child></span>
        <span id=multiple class=B></span>
      </span>
    </section>
  )HTML");

  // NOTE: If StringHasher changes and we're unlucky,
  // the values for .child may need updating.
  // clang-format off
  static const struct QueryTest kTestCases[] = {
      // None of these use the fast ID path, since we're disconnected.
      {"#A",          false, 1, {.elements_seen = 3, .check_id = 3}},
      {"#B",          false, 1, {.elements_seen = 4, .check_id = 4}},
      {"#B",           true, 1, {.elements_seen = 6, .check_id = 6}},
      {"#multiple",    true, 2, {.elements_seen = 6, .check_id = 6}},
      {".child",      false, 1, {.elements_seen = 3, .check_class = 3, .skipped_subtree = 1}},
      {".child",       true, 2, {.elements_seen = 4, .check_class = 4, .skipped_subtree = 2}},
      {"#first span", false, 1, {.elements_seen = 4, .check_id = 3, .check_tag = 1}},
      {"#first span",  true, 4, {.elements_seen = 7, .check_id = 3, .check_tag = 4}},
  };
  // clang-format on

  RunTests(*scope, kTestCases);
}

TEST(SelectorQueryTest, DisconnectedTreeScope) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  Element* host = document->CreateRawElement(html_names::kDivTag);
  ShadowRoot& shadowRoot =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadowRoot.SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <section>
      <span id=first>
        <span id=A class=A></span>
        <span id=B class=child></span>
        <span id=multiple class=child></span>
        <span id=multiple class=B></span>
      </span>
    </section>
  )HTML");

  // NOTE: If StringHasher changes and we're unlucky,
  // the values for .child may need updating.
  // clang-format off
  static const struct QueryTest kTestCases[] = {
      // These can use the fast ID path, since we're in within a tree scope.
      {"#A",          false, 1, {.elements_seen = 1, .fast_id_roots = 1, .check_id = 1}},
      {"#B",          false, 1, {.elements_seen = 1, .fast_id_roots = 1, .check_id = 1}},
      {"#B",           true, 1, {.elements_seen = 1, .fast_id_roots = 1, .check_id = 1}},
      {"#multiple",    true, 2, {.elements_seen = 2, .fast_id_roots = 2, .check_id = 2}},
      {".child",      false, 1, {.elements_seen = 3, .check_class = 3, .skipped_subtree = 1}},
      {".child",       true, 2, {.elements_seen = 4, .check_class = 4, .skipped_subtree = 2}},
      {"#first span", false, 1, {.elements_seen = 2, .fast_id_roots = 1, .check_id = 1, .check_tag = 1}},
      {"#first span",  true, 4, {.elements_seen = 5, .fast_id_roots = 1, .check_id = 1, .check_tag = 4}},
  };
  // clang-format on

  RunTests(shadowRoot, kTestCases);
}

TEST(SelectorQueryTest, QueryHasPseudoClass) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write(R"HTML(
    <!DOCTYPE html>
    <main id=main>
      <div id=div1 class=subject3>
        <div id=div2 class=a>
          <div id=div3 class=b></div>
        </div>
        <div id=div4 class='subject1 subject3 subject4'>
          <div id=div5 class='subject2 subject5 subject6'></div>
          <div id=div6 class=a>
            <div id=div7 class='subject1 subject4'>
              <div id=div8></div>
              <div id=div9 class=a></div>
              <div id=div10 class=b>
                <div id=div11 class=c></div>
              </div>
            </div>
            <div id=div12 class=b>
              <div id=div13 class=c></div>
            </div>
          </div>
          <div id=div14 class=b>
            <div id=div15 class='c d'></div>
          </div>
        </div>
        <div id=div16 class='subject1 subject3'>
          <div id=div17 class='subject2 subject5'></div>
          <div id=div18 class=a>
            <div id=div19 class='subject1 subject4'>
              <div id=div20 class='subject5 subject6'></div>
              <div id=div21 class=a></div>
              <div id=div22 class=b>
                <div id=div23 class='c d'></div>
              </div>
            </div>
            <div id=div24 class=b>
              <div id=div25 class=c></div>
            </div>
          </div>
          <div id=div26></div>
          <div id=div27 class=b>
            <div id=div28 class='c d'></div>
          </div>
          <div id=div29></div>
          <div id=div30>
            <div id=div31></div>
          </div>
        </div>
      </div>
    </main>
  )HTML");
  Element* scope = document->getElementById(AtomicString("main"));
  {
    StaticElementList* result =
        scope->QuerySelectorAll(AtomicString(":has(> .a ~ .b)"));
    ASSERT_EQ(4U, result->length());
    EXPECT_EQ(result->item(0)->GetIdAttribute(), "div4");
    EXPECT_TRUE(
        result->item(0)->ClassNames().Contains(AtomicString("subject1")));
    EXPECT_EQ(result->item(1)->GetIdAttribute(), "div7");
    EXPECT_TRUE(
        result->item(1)->ClassNames().Contains(AtomicString("subject1")));
    EXPECT_EQ(result->item(2)->GetIdAttribute(), "div16");
    EXPECT_TRUE(
        result->item(2)->ClassNames().Contains(AtomicString("subject1")));
    EXPECT_EQ(result->item(3)->GetIdAttribute(), "div19");
    EXPECT_TRUE(
        result->item(3)->ClassNames().Contains(AtomicString("subject1")));
  }

  {
    StaticElementList* result =
        scope->QuerySelectorAll(AtomicString(":has(+ .a > .b .c)"));
    ASSERT_EQ(2U, result->length());
    EXPECT_EQ(result->item(0)->GetIdAttribute(), "div5");
    EXPECT_TRUE(
        result->item(0)->ClassNames().Contains(AtomicString("subject2")));
    EXPECT_EQ(result->item(1)->GetIdAttribute(), "div17");
    EXPECT_TRUE(
        result->item(1)->ClassNames().Contains(AtomicString("subject2")));
  }

  {
    StaticElementList* result =
        scope->QuerySelectorAll(AtomicString(":has(> .a .b)"));
    ASSERT_EQ(3U, result->length());
    EXPECT_EQ(result->item(0)->GetIdAttribute(), "div1");
    EXPECT_TRUE(
        result->item(0)->ClassNames().Contains(AtomicString("subject3")));
    EXPECT_EQ(result->item(1)->GetIdAttribute(), "div4");
    EXPECT_TRUE(
        result->item(1)->ClassNames().Contains(AtomicString("subject3")));
    EXPECT_EQ(result->item(2)->GetIdAttribute(), "div16");
    EXPECT_TRUE(
        result->item(2)->ClassNames().Contains(AtomicString("subject3")));
  }

  {
    StaticElementList* result =
        scope->QuerySelectorAll(AtomicString(":has(> .a + .b .c)"));
    ASSERT_EQ(3U, result->length());
    EXPECT_EQ(result->item(0)->GetIdAttribute(), "div4");
    EXPECT_TRUE(
        result->item(0)->ClassNames().Contains(AtomicString("subject4")));
    EXPECT_EQ(result->item(1)->GetIdAttribute(), "div7");
    EXPECT_TRUE(
        result->item(1)->ClassNames().Contains(AtomicString("subject4")));
    EXPECT_EQ(result->item(2)->GetIdAttribute(), "div19");
    EXPECT_TRUE(
        result->item(2)->ClassNames().Contains(AtomicString("subject4")));
  }

  {
    StaticElementList* result =
        scope->QuerySelectorAll(AtomicString(":has(~ .a ~ .b .d)"));
    ASSERT_EQ(3U, result->length());
    EXPECT_EQ(result->item(0)->GetIdAttribute(), "div5");
    EXPECT_TRUE(
        result->item(0)->ClassNames().Contains(AtomicString("subject5")));
    EXPECT_EQ(result->item(1)->GetIdAttribute(), "div17");
    EXPECT_TRUE(
        result->item(1)->ClassNames().Contains(AtomicString("subject5")));
    EXPECT_EQ(result->item(2)->GetIdAttribute(), "div20");
    EXPECT_TRUE(
        result->item(2)->ClassNames().Contains(AtomicString("subject5")));
  }

  {
    StaticElementList* result =
        scope->QuerySelectorAll(AtomicString(":has(+ .a + .b .d)"));
    ASSERT_EQ(2U, result->length());
    EXPECT_EQ(result->item(0)->GetIdAttribute(), "div5");
    EXPECT_TRUE(
        result->item(0)->ClassNames().Contains(AtomicString("subject6")));
    EXPECT_EQ(result->item(1)->GetIdAttribute(), "div20");
    EXPECT_TRUE(
        result->item(1)->ClassNames().Contains(AtomicString("subject6")));
  }
}

}  // namespace blink
