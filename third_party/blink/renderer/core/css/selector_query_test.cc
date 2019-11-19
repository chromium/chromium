// Copyright 2016 The Chromium Authors. All rights reserved.
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
#include "third_party/blink/renderer/platform/heap/heap.h"

// Uncomment to run the SelectorQueryTests for stats in a release build.
// #define RELEASE_QUERY_STATS

namespace blink {

namespace {
struct QueryTest {
  const char* selector;
  bool query_all;
  unsigned matches;
  // {totalCount, fastId, fastClass, fastTagName, fastScan, slowScan,
  //  slowTraversingShadowTreeScan}
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
      StaticElementList* match_all = scope.QuerySelectorAll(selector);
      EXPECT_EQ(test_case.matches, match_all->length());
    } else {
      Element* match = scope.QuerySelector(selector);
      EXPECT_EQ(test_case.matches, match ? 1u : 0u);
    }
#if DCHECK_IS_ON() || defined(RELEASE_QUERY_STATS)
    SelectorQuery::QueryStats stats = SelectorQuery::LastQueryStats();
    EXPECT_EQ(test_case.stats.total_count, stats.total_count);
    EXPECT_EQ(test_case.stats.fast_id, stats.fast_id);
    EXPECT_EQ(test_case.stats.fast_class, stats.fast_class);
    EXPECT_EQ(test_case.stats.fast_tag_name, stats.fast_tag_name);
    EXPECT_EQ(test_case.stats.fast_scan, stats.fast_scan);
    EXPECT_EQ(test_case.stats.slow_scan, stats.slow_scan);
    EXPECT_EQ(test_case.stats.slow_traversing_shadow_tree_scan,
              stats.slow_traversing_shadow_tree_scan);
#endif
  }
}
}  // namespace

TEST(SelectorQueryTest, NotMatchingPseudoElement) {
  auto* document = MakeGarbageCollected<Document>();
  auto* html = MakeGarbageCollected<HTMLHtmlElement>(*document);
  document->AppendChild(html);
  document->documentElement()->SetInnerHTMLFromString(
      "<body><style>span::before { content: 'X' }</style><span></span></body>");

  CSSSelectorList selector_list = CSSParser::ParseSelector(
      MakeGarbageCollected<CSSParserContext>(
          *document, NullURL(), true /* origin_clean */,
          network::mojom::ReferrerPolicy::kDefault, WTF::TextEncoding(),
          CSSParserContext::kSnapshotProfile),
      nullptr, "span::before");
  std::unique_ptr<SelectorQuery> query =
      SelectorQuery::Adopt(std::move(selector_list));
  Element* elm = query->QueryFirst(*document);
  EXPECT_EQ(nullptr, elm);

  selector_list = CSSParser::ParseSelector(
      MakeGarbageCollected<CSSParserContext>(
          *document, NullURL(), true /* origin_clean */,
          network::mojom::ReferrerPolicy::kDefault, WTF::TextEncoding(),
          CSSParserContext::kSnapshotProfile),
      nullptr, "span");
  query = SelectorQuery::Adopt(std::move(selector_list));
  elm = query->QueryFirst(*document);
  EXPECT_NE(nullptr, elm);
}

TEST(SelectorQueryTest, LastOfTypeNotFinishedParsing) {
  auto* document = MakeGarbageCollected<HTMLDocument>();
  auto* html = MakeGarbageCollected<HTMLHtmlElement>(*document);
  document->AppendChild(html);
  document->documentElement()->SetInnerHTMLFromString(
      "<body><p></p><p id=last></p></body>", ASSERT_NO_EXCEPTION);

  document->body()->BeginParsingChildren();

  CSSSelectorList selector_list = CSSParser::ParseSelector(
      MakeGarbageCollected<CSSParserContext>(
          *document, NullURL(), true /* origin_clean */,
          network::mojom::ReferrerPolicy::kDefault, WTF::TextEncoding(),
          CSSParserContext::kSnapshotProfile),
      nullptr, "p:last-of-type");
  std::unique_ptr<SelectorQuery> query =
      SelectorQuery::Adopt(std::move(selector_list));
  Element* elm = query->QueryFirst(*document);
  ASSERT_TRUE(elm);
  EXPECT_EQ("last", elm->IdForStyleResolution());
}

TEST(SelectorQueryTest, StandardsModeFastPaths) {
  auto* document = MakeGarbageCollected<HTMLDocument>();
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
            <span id=multiple class=two></span>
          </span>
        </div>
      </body>
    </html>
  )HTML");
  static const struct QueryTest kTestCases[] = {
      // Id in right most selector fast path.
      {"#A", false, 1, {1, 1, 0, 0, 0, 0, 0}},
      {"#multiple", false, 1, {1, 1, 0, 0, 0, 0, 0}},
      {"#multiple.two", false, 1, {1, 1, 0, 0, 0, 0, 0}},
      {"#multiple", true, 2, {2, 2, 0, 0, 0, 0, 0}},
      {"span#multiple", true, 1, {2, 2, 0, 0, 0, 0, 0}},
      {"#multiple.two", true, 2, {2, 2, 0, 0, 0, 0, 0}},
      {"body #multiple", false, 1, {1, 1, 0, 0, 0, 0, 0}},
      {"body span#multiple", false, 1, {2, 2, 0, 0, 0, 0, 0}},
      {"body #multiple", true, 2, {2, 2, 0, 0, 0, 0, 0}},
      {"[id=multiple]", true, 2, {2, 2, 0, 0, 0, 0, 0}},
      {"body [id=multiple]", true, 2, {2, 2, 0, 0, 0, 0, 0}},

      // Single selector tag fast path.
      {"span", false, 1, {4, 0, 0, 4, 0, 0, 0}},
      {"span", true, 9, {14, 0, 0, 14, 0, 0, 0}},

      // Single selector class fast path.
      {".two", false, 1, {6, 0, 6, 0, 0, 0, 0}},
      {".two", true, 4, {14, 0, 14, 0, 0, 0, 0}},

      // Class in the right most selector fast path.
      {"body .two", false, 1, {6, 0, 6, 0, 0, 0, 0}},
      {"div .two", false, 1, {12, 0, 12, 0, 0, 0, 0}},

      // Classes in the right most selector for querySelectorAll use a fast
      // path.
      {"body .two", true, 4, {14, 0, 14, 0, 0, 0, 0}},
      {"div .two", true, 2, {14, 0, 14, 0, 0, 0, 0}},

      // TODO: We could use the fast class path to find the elements inside
      // the id scope instead of the fast scan.
      {"#second .two", false, 1, {3, 1, 0, 0, 2, 0, 0}},
      {"#second .two", true, 2, {5, 1, 0, 0, 4, 0, 0}},

      // We combine the class fast path with the fast scan mode when possible.
      {".B span", false, 1, {11, 0, 10, 0, 1, 0, 0}},
      {".B span", true, 4, {14, 0, 10, 0, 4, 0, 0}},

      // We expand the scope of id selectors when affected by an adjectent
      // combinator.
      {"#c + :last-child", false, 1, {5, 1, 0, 0, 4, 0, 0}},
      {"#a ~ :last-child", false, 1, {5, 1, 0, 0, 4, 0, 0}},
      {"#c + div", true, 1, {5, 1, 0, 0, 4, 0, 0}},
      {"#a ~ span", true, 2, {5, 1, 0, 0, 4, 0, 0}},

      // We only expand the scope for id selectors if they're directly affected
      // the adjacent combinator.
      {"#first span + span", false, 1, {3, 1, 0, 0, 2, 0, 0}},
      {"#first span ~ span", false, 1, {3, 1, 0, 0, 2, 0, 0}},
      {"#second span + span", true, 3, {5, 1, 0, 0, 4, 0, 0}},
      {"#second span ~ span", true, 3, {5, 1, 0, 0, 4, 0, 0}},

      // We disable the fast path for class selectors when affected by adjacent
      // combinator.
      {".one + :last-child", false, 1, {8, 0, 0, 0, 8, 0, 0}},
      {".A ~ :last-child", false, 1, {9, 0, 0, 0, 9, 0, 0}},
      {".A + div", true, 1, {14, 0, 0, 0, 14, 0, 0}},
      {".one ~ span", true, 5, {14, 0, 0, 0, 14, 0, 0}},

      // We re-enable the fast path for classes once past the selector directly
      // affected by the adjacent combinator.
      {".B span + span", true, 3, {14, 0, 10, 0, 4, 0, 0}},
      {".B span ~ span", true, 3, {14, 0, 10, 0, 4, 0, 0}},

      // Selectors with no classes or ids use the fast scan.
      {":scope", false, 1, {1, 0, 0, 0, 1, 0, 0}},
      {":scope", true, 1, {14, 0, 0, 0, 14, 0, 0}},
      {"foo bar", false, 0, {14, 0, 0, 0, 14, 0, 0}},

      // Multiple selectors always uses the slow path.
      // TODO(esprehn): We could make this fast if we sorted the output, not
      // sure it's worth it unless we're dealing with ids.
      {"#a, #b", false, 1, {5, 0, 0, 0, 0, 5, 0}},
      {"#a, #b", true, 2, {14, 0, 0, 0, 0, 14, 0}},

      // Anything that crosses shadow boundaries is slow path.
      {"#foo /deep/ .a", false, 0, {14, 0, 0, 0, 0, 0, 14}},
      {"#foo::shadow .a", false, 0, {14, 0, 0, 0, 0, 0, 14}},
      {"::content .a", false, 0, {14, 0, 0, 0, 0, 14, 0}},
      {"#foo /deep/ .a", true, 0, {14, 0, 0, 0, 0, 0, 14}},
      {"#foo::shadow .a", true, 0, {14, 0, 0, 0, 0, 0, 14}},
      {"::content .a", true, 0, {14, 0, 0, 0, 0, 14, 0}},
  };
  RunTests(*document, kTestCases);
}

TEST(SelectorQueryTest, FastPathScoped) {
  auto* document = MakeGarbageCollected<HTMLDocument>();
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
  Element* scope = document->getElementById("first");
  ASSERT_NE(nullptr, scope);
  ShadowRoot& shadowRoot =
      scope->AttachShadowRootInternal(ShadowRootType::kOpen);
  // Make the inside the shadow root be identical to that of the outer document.
  shadowRoot.appendChild(&document->documentElement()->CloneWithChildren());
  static const struct QueryTest kTestCases[] = {
      // Id in the right most selector.
      {"#first", false, 0, {0, 0, 0, 0, 0, 0, 0}},

      {"#B", false, 1, {1, 1, 0, 0, 0, 0, 0}},
      {"#multiple", false, 1, {1, 1, 0, 0, 0, 0, 0}},
      {"#multiple.c", false, 1, {2, 2, 0, 0, 0, 0, 0}},

      // Class in the right most selector.
      {".child", false, 1, {1, 0, 1, 0, 0, 0, 0}},
      {".child", true, 4, {7, 0, 7, 0, 0, 0, 0}},

      // If an ancestor has the class name we fast scan all the descendants of
      // the scope.
      {".root-class span", true, 4, {7, 0, 0, 0, 7, 0, 0}},

      // If an ancestor has the class name in the middle of the selector we fast
      // scan all the descendants of the scope.
      {".root-class span:nth-child(2)", false, 1, {2, 0, 0, 0, 2, 0, 0}},
      {".root-class span:nth-child(2)", true, 1, {7, 0, 0, 0, 7, 0, 0}},

      // If the id is an ancestor we scan all the descendants.
      {"#root-id span", true, 4, {8, 1, 0, 0, 7, 0, 0}},
  };

  {
    SCOPED_TRACE("Inside document");
    RunTests(*scope, kTestCases);
  }

  {
    // Run all the tests a second time but with a scope inside a shadow root,
    // all the fast paths should behave the same.
    SCOPED_TRACE("Inside shadow root");
    scope = shadowRoot.getElementById("first");
    ASSERT_NE(nullptr, scope);
    RunTests(*scope, kTestCases);
  }
}

TEST(SelectorQueryTest, QuirksModeSlowPath) {
  auto* document = MakeGarbageCollected<HTMLDocument>();
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
  static const struct QueryTest kTestCases[] = {
      // Quirks mode can't use the id fast path due to being case-insensitive.
      {"#one", false, 1, {5, 0, 0, 0, 5, 0, 0}},
      {"#One", false, 1, {5, 0, 0, 0, 5, 0, 0}},
      {"#ONE", false, 1, {5, 0, 0, 0, 5, 0, 0}},
      {"#ONE", true, 2, {6, 0, 0, 0, 6, 0, 0}},
      {"[id=One]", false, 1, {5, 0, 0, 0, 5, 0, 0}},
      {"[id=One]", true, 1, {6, 0, 0, 0, 6, 0, 0}},
      {"body #first", false, 1, {4, 0, 0, 0, 4, 0, 0}},
      {"body #one", true, 2, {6, 0, 0, 0, 6, 0, 0}},
      // Quirks can use the class and tag name fast paths though.
      {"span", false, 1, {4, 0, 0, 4, 0, 0, 0}},
      {"span", true, 3, {6, 0, 0, 6, 0, 0, 0}},
      {".two", false, 1, {5, 0, 5, 0, 0, 0, 0}},
      {".two", true, 2, {6, 0, 6, 0, 0, 0, 0}},
      {"body span", false, 1, {4, 0, 0, 0, 4, 0, 0}},
      {"body span", true, 3, {6, 0, 0, 0, 6, 0, 0}},
      {"body .two", false, 1, {5, 0, 5, 0, 0, 0, 0}},
      {"body .two", true, 2, {6, 0, 6, 0, 0, 0, 0}},
  };
  RunTests(*document, kTestCases);
}

TEST(SelectorQueryTest, DisconnectedSubtree) {
  auto* document = MakeGarbageCollected<HTMLDocument>();
  Element* scope = document->CreateRawElement(html_names::kDivTag);
  scope->SetInnerHTMLFromString(R"HTML(
    <section>
      <span id=first>
        <span id=A class=A></span>
        <span id=B class=child></span>
        <span id=multiple class=child></span>
        <span id=multiple class=B></span>
      </span>
    </section>
  )HTML");
  static const struct QueryTest kTestCases[] = {
      {"#A", false, 1, {3, 0, 0, 0, 3, 0, 0}},
      {"#B", false, 1, {4, 0, 0, 0, 4, 0, 0}},
      {"#B", true, 1, {6, 0, 0, 0, 6, 0, 0}},
      {"#multiple", true, 2, {6, 0, 0, 0, 6, 0, 0}},
      {".child", false, 1, {4, 0, 4, 0, 0, 0, 0}},
      {".child", true, 2, {6, 0, 6, 0, 0, 0, 0}},
      {"#first span", false, 1, {3, 0, 0, 0, 3, 0, 0}},
      {"#first span", true, 4, {6, 0, 0, 0, 6, 0, 0}},
  };

  RunTests(*scope, kTestCases);
}

TEST(SelectorQueryTest, DisconnectedTreeScope) {
  auto* document = MakeGarbageCollected<HTMLDocument>();
  Element* host = document->CreateRawElement(html_names::kDivTag);
  ShadowRoot& shadowRoot =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadowRoot.SetInnerHTMLFromString(R"HTML(
    <section>
      <span id=first>
        <span id=A class=A></span>
        <span id=B class=child></span>
        <span id=multiple class=child></span>
        <span id=multiple class=B></span>
      </span>
    </section>
  )HTML");
  static const struct QueryTest kTestCases[] = {
      {"#A", false, 1, {1, 1, 0, 0, 0, 0, 0}},
      {"#B", false, 1, {1, 1, 0, 0, 0, 0, 0}},
      {"#B", true, 1, {1, 1, 0, 0, 0, 0, 0}},
      {"#multiple", true, 2, {2, 2, 0, 0, 0, 0, 0}},
      {".child", false, 1, {4, 0, 4, 0, 0, 0, 0}},
      {".child", true, 2, {6, 0, 6, 0, 0, 0, 0}},
      {"#first span", false, 1, {2, 1, 0, 0, 1, 0, 0}},
      {"#first span", true, 4, {5, 1, 0, 0, 4, 0, 0}},
  };

  RunTests(shadowRoot, kTestCases);
}

}  // namespace blink
