// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/css/has_argument_match_context.h"
#include "third_party/blink/renderer/core/css/has_matched_cache_scope.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class HasMatchedCacheScopeContextTest : public PageTestBase {
 protected:
  enum CachedState {
    kNotCached = HasSelectorMatchResult::kNotCached,
    kNotMatched = HasSelectorMatchResult::kChecked,
    kMatched =
        HasSelectorMatchResult::kChecked | HasSelectorMatchResult::kMatched,
    kNotMatchedAndAllDescendantsOrNextSiblingsChecked =
        HasSelectorMatchResult::kChecked |
        HasSelectorMatchResult::kAllDescendantsOrNextSiblingsChecked,
    kMatchedAndAllDescendantsOrNextSiblingsChecked =
        HasSelectorMatchResult::kChecked | HasSelectorMatchResult::kMatched |
        HasSelectorMatchResult::kAllDescendantsOrNextSiblingsChecked,
    kNotCheckedAndSomeChildrenChecked =
        HasSelectorMatchResult::kSomeChildrenChecked,
    kNotMatchedAndSomeChildrenChecked =
        HasSelectorMatchResult::kChecked |
        HasSelectorMatchResult::kSomeChildrenChecked,
    kMatchedAndSomeChildrenChecked =
        HasSelectorMatchResult::kChecked | HasSelectorMatchResult::kMatched |
        HasSelectorMatchResult::kSomeChildrenChecked,
    kNotMatchedAndAllDescendantsOrNextSiblingsCheckedAndSomeChildrenChecked =
        HasSelectorMatchResult::kChecked |
        HasSelectorMatchResult::kAllDescendantsOrNextSiblingsChecked |
        HasSelectorMatchResult::kSomeChildrenChecked,
    kMatchedAndAllDescendantsOrNextSiblingsCheckedAndSomeChildrenChecked =
        HasSelectorMatchResult::kChecked | HasSelectorMatchResult::kMatched |
        HasSelectorMatchResult::kAllDescendantsOrNextSiblingsChecked |
        HasSelectorMatchResult::kSomeChildrenChecked,
  };

  enum MatchedResult {
    kCached,
    kNotYetChecked,
    kAlreadyNotMatched,
  };

  struct ExpectedCacheResult {
    const char* element_query;
    CachedState cached_state;
    MatchedResult matched_result;
  };

  bool ElementCached(
      HasMatchedCacheScope::Context& has_matched_cache_scope_context,
      Element* element) const {
    return has_matched_cache_scope_context.GetResult(element) !=
           HasSelectorMatchResult::kNotCached;
  }

  bool ElementChecked(
      HasMatchedCacheScope::Context& has_matched_cache_scope_context,
      Element* element) const {
    return has_matched_cache_scope_context.GetResult(element) &
           HasSelectorMatchResult::kChecked;
  }

  static String MatchedResultToString(uint8_t matched_result) {
    return String::Format(
        "0b%c%c%c%c",
        (matched_result & HasSelectorMatchResult::kSomeChildrenChecked ? '1'
                                                                       : '0'),
        (matched_result &
                 HasSelectorMatchResult::kAllDescendantsOrNextSiblingsChecked
             ? '1'
             : '0'),
        (matched_result & HasSelectorMatchResult::kMatched ? '1' : '0'),
        (matched_result & HasSelectorMatchResult::kChecked ? '1' : '0'));
  }

  template <unsigned length>
  void CheckCacheResults(
      Document* document,
      String query_name,
      const char* selector_text,
      unsigned expected_cache_count,
      HasMatchedCacheScope& has_matched_cache_scope,
      const ExpectedCacheResult (&expected_cache_results)[length]) const {
    CSSSelectorList selector_list = CSSParser::ParseSelector(
        MakeGarbageCollected<CSSParserContext>(
            *document, NullURL(), true /* origin_clean */, Referrer(),
            WTF::TextEncoding(), CSSParserContext::kSnapshotProfile),
        nullptr, selector_text);
    const CSSSelector* selector = nullptr;
    for (selector = selector_list.First();
         selector && selector->GetPseudoType() != CSSSelector::kPseudoHas;
         selector = selector->TagHistory()) {
    }
    if (!selector) {
      ADD_FAILURE() << "Failed : " << query_name << " (Cannot find :has() in "
                    << selector_text << ")";
      return;
    }
    const CSSSelector* argument_selector = selector->SelectorList()->First();

    HasArgumentMatchContext argument_match_context(argument_selector);
    HasMatchedCacheScope::Context has_matched_cache_scope_context(
        document, argument_match_context);

    EXPECT_EQ(expected_cache_count, has_matched_cache_scope_context.map_.size())
        << "Failed : " << query_name;

    for (ExpectedCacheResult expected_result : expected_cache_results) {
      String test_name =
          String::Format("[%s] cache result of %s", query_name.Utf8().c_str(),
                         expected_result.element_query);
      Element* element = document->QuerySelector(expected_result.element_query);
      DCHECK(element) << "Failed to get `" << expected_result.element_query
                      << "'";

      EXPECT_EQ(expected_result.cached_state,
                has_matched_cache_scope_context.GetResult(element))
          << "Failed : " << test_name << " : { expected: "
          << MatchedResultToString(expected_result.cached_state) << ", actual: "
          << MatchedResultToString(
                 has_matched_cache_scope_context.GetResult(element))
          << " }";

      switch (expected_result.matched_result) {
        case kCached:
          EXPECT_TRUE(ElementCached(has_matched_cache_scope_context, element))
              << "Failed : " << test_name;
          break;
        case kNotYetChecked:
        case kAlreadyNotMatched:
          EXPECT_FALSE(ElementChecked(has_matched_cache_scope_context, element))
              << "Failed : " << test_name;
          EXPECT_EQ(expected_result.matched_result == kAlreadyNotMatched,
                    has_matched_cache_scope_context.AlreadyChecked(element))
              << "Failed : " << test_name;
          break;
      }
    }
  }

  template <unsigned cache_size>
  void TestMatches(
      Document* document,
      const char* query_scope_element_id,
      const char* selector_text,
      bool expected_match_result,
      unsigned expected_cache_count,
      const ExpectedCacheResult (&expected_cache_results)[cache_size]) const {
    Element* query_scope_element =
        document->getElementById(query_scope_element_id);
    ASSERT_TRUE(query_scope_element);

    HasMatchedCacheScope has_matched_cache_scope(document);

    String query_name = String::Format("#%s.matches('%s')",
                                       query_scope_element_id, selector_text);

    EXPECT_EQ(expected_match_result,
              query_scope_element->matches(selector_text))
        << "Failed : " << query_name;

    CheckCacheResults(document, query_name, selector_text, expected_cache_count,
                      has_matched_cache_scope, expected_cache_results);
  }

  template <unsigned query_result_size, unsigned cache_size>
  void TestQuerySelectorAll(
      Document* document,
      const char* query_scope_element_id,
      const char* selector_text,
      const String (&expected_results)[query_result_size],
      unsigned expected_cache_count,
      const ExpectedCacheResult (&expected_cache_results)[cache_size]) const {
    Element* query_scope_element =
        document->getElementById(query_scope_element_id);
    ASSERT_TRUE(query_scope_element);

    HasMatchedCacheScope has_matched_cache_scope(document);

    String query_name = String::Format("#%s.querySelectorAll('%s')",
                                       query_scope_element_id, selector_text);

    StaticElementList* result =
        query_scope_element->QuerySelectorAll(selector_text);

    EXPECT_EQ(query_result_size, result->length()) << "Failed : " << query_name;
    unsigned size_max = query_result_size > result->length() ? query_result_size
                                                             : result->length();
    for (unsigned i = 0; i < size_max; ++i) {
      EXPECT_EQ(
          (i < query_result_size ? expected_results[i] : "<null>"),
          (i < result->length() ? result->item(i)->GetIdAttribute() : "<null>"))
          << "Failed :" << query_name << " result at index " << i;
    }

    CheckCacheResults(document, query_name, selector_text, expected_cache_count,
                      has_matched_cache_scope, expected_cache_results);
  }
};

TEST_F(HasMatchedCacheScopeContextTest, Case1StartsWithDescendantCombinator) {
  // HasArgumentMatchTraversalScope::kSubtree

  auto* document = HTMLDocument::CreateForTest();
  document->write(R"HTML(
    <!DOCTYPE html>
    <main id=main>
      <div id=div1>
        <div id=div11></div>
      </div>
      <div id=div2>
        <div id=div21>
          <div id=div211></div>
        </div>
        <div id=div22>
          <div id=div221></div>
          <div id=div222 class=a>
            <div id=div2221></div>
          </div>
          <div id=div223>
            <div id=div2231></div>
            <div id=div2232>
              <div id=div22321></div>
              <div id=div22322 class=b>
                <div id=div223221></div>
              </div>
              <div id=div22323></div>
            </div>
          </div>
        </div>
        <div id=div23>
          <div id=div231></div>
        </div>
        <div id=div24>
          <div id=div241></div>
        </div>
      </div>
      <div id=div3>
        <div id=div31></div>
      </div>
      <div id=div4>
        <div id=div41></div>
      </div>
    </main>
  )HTML");

  TestMatches(
      document, "div2", ":has(.a)",
      /* expected_match_result */ true, /* expected_cache_count */ 7,
      {{"main", kMatched, kCached},
       {"#div1", kNotCached, kNotYetChecked},
       {"#div11", kNotCached, kNotYetChecked},
       {"#div2", kMatchedAndSomeChildrenChecked, kCached},
       {"#div21", kNotCached, kNotYetChecked},
       {"#div211", kNotCached, kNotYetChecked},
       {"#div22", kMatchedAndSomeChildrenChecked, kCached},
       {"#div221", kNotCached, kNotYetChecked},
       {"#div222", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div2221", kNotCached, kAlreadyNotMatched},
       {"#div223", kNotCached, kAlreadyNotMatched},
       {"#div2231", kNotCached, kAlreadyNotMatched},
       {"#div2232", kNotCached, kAlreadyNotMatched},
       {"#div22321", kNotCached, kAlreadyNotMatched},
       {"#div22322", kNotCached, kAlreadyNotMatched},
       {"#div223221", kNotCached, kAlreadyNotMatched},
       {"#div22323", kNotCached, kAlreadyNotMatched},
       {"#div23", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div231", kNotCached, kAlreadyNotMatched},
       {"#div24", kNotCached, kAlreadyNotMatched},
       {"#div241", kNotCached, kAlreadyNotMatched},
       {"#div3", kNotCached, kNotYetChecked},
       {"#div31", kNotCached, kNotYetChecked},
       {"#div4", kNotCached, kNotYetChecked},
       {"#div41", kNotCached, kNotYetChecked}});

  TestMatches(
      document, "div2", ":has(.b)",
      /* expected_match_result */ true, /* expected_cache_count */ 9,
      {{"main", kMatched, kCached},
       {"#div1", kNotCached, kNotYetChecked},
       {"#div11", kNotCached, kNotYetChecked},
       {"#div2", kMatchedAndSomeChildrenChecked, kCached},
       {"#div21", kNotCached, kNotYetChecked},
       {"#div211", kNotCached, kNotYetChecked},
       {"#div22", kMatched, kCached},
       {"#div221", kNotCached, kNotYetChecked},
       {"#div222", kNotCached, kNotYetChecked},
       {"#div2221", kNotCached, kNotYetChecked},
       {"#div223", kMatched, kCached},
       {"#div2231", kNotCached, kNotYetChecked},
       {"#div2232", kMatchedAndSomeChildrenChecked, kCached},
       {"#div22321", kNotCached, kNotYetChecked},
       {"#div22322", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kCached},
       {"#div223221", kNotCached, kAlreadyNotMatched},
       {"#div22323", kNotCached, kAlreadyNotMatched},
       {"#div23", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div231", kNotCached, kAlreadyNotMatched},
       {"#div24", kNotCached, kAlreadyNotMatched},
       {"#div241", kNotCached, kAlreadyNotMatched},
       {"#div3", kNotCached, kNotYetChecked},
       {"#div31", kNotCached, kNotYetChecked},
       {"#div4", kNotCached, kNotYetChecked},
       {"#div41", kNotCached, kNotYetChecked}});

  TestMatches(
      document, "div2", ":has(.c)",
      /* expected_match_result */ false, /* expected_cache_count */ 2,
      {{"main", kNotCached, kNotYetChecked},
       {"#div1", kNotCached, kNotYetChecked},
       {"#div11", kNotCached, kNotYetChecked},
       {"#div2", kNotMatchedAndSomeChildrenChecked, kCached},
       {"#div21", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div211", kNotCached, kAlreadyNotMatched},
       {"#div22", kNotCached, kAlreadyNotMatched},
       {"#div221", kNotCached, kAlreadyNotMatched},
       {"#div222", kNotCached, kAlreadyNotMatched},
       {"#div2221", kNotCached, kAlreadyNotMatched},
       {"#div223", kNotCached, kAlreadyNotMatched},
       {"#div2231", kNotCached, kAlreadyNotMatched},
       {"#div2232", kNotCached, kAlreadyNotMatched},
       {"#div22321", kNotCached, kAlreadyNotMatched},
       {"#div22322", kNotCached, kAlreadyNotMatched},
       {"#div223221", kNotCached, kAlreadyNotMatched},
       {"#div22323", kNotCached, kAlreadyNotMatched},
       {"#div23", kNotCached, kAlreadyNotMatched},
       {"#div231", kNotCached, kAlreadyNotMatched},
       {"#div24", kNotCached, kAlreadyNotMatched},
       {"#div241", kNotCached, kAlreadyNotMatched},
       {"#div3", kNotCached, kNotYetChecked},
       {"#div31", kNotCached, kNotYetChecked},
       {"#div4", kNotCached, kNotYetChecked},
       {"#div41", kNotCached, kNotYetChecked}});
}

TEST_F(HasMatchedCacheScopeContextTest, Case1StartsWithChildCombinator) {
  // HasArgumentMatchTraversalScope::kSubtree

  auto* document = HTMLDocument::CreateForTest();
  document->write(R"HTML(
    <!DOCTYPE html>
    <main id=main>
      <div id=div1>
        <div id=div11></div>
      </div>
      <div id=div2>
        <div id=div21>
          <div id=div211></div>
        </div>
        <div id=div22>
          <div id=div221>
            <div id=div2211></div>
          </div>
          <div id=div222 class=a>
            <div id=div2221>
              <div id=div22211></div>
              <div id=div22212 class=b>
                <div id=div222121></div>
              </div>
              <div id=div22213></div>
            </div>
          </div>
          <div id=div223>
            <div id=div2231></div>
          </div>
          <div id=div224>
            <div id=div2241></div>
            <div id=div2242 class=a>
              <div id=div22421></div>
              <div id=div22422>
                <div id=div224221></div>
                <div id=div224222 class=b>
                  <div id=div2242221></div>
                </div>
                <div id=div224223></div>
              </div>
              <div id=div22423>
                <div id=div224231></div>
              </div>
              <div id=div22424></div>
            </div>
            <div id=div2243>
              <div id=div22431></div>
            </div>
            <div id=div2244></div>
          </div>
          <div id=div225>
            <div id=div2251></div>
          </div>
          <div id=div226></div>
        </div>
        <div id=div23>
          <div id=div231></div>
        </div>
        <div id=div24></div>
      </div>
      <div id=div3>
        <div id=div31></div>
      </div>
    </main>
  )HTML");

  TestMatches(
      document, "div22", ":has(> .a .b)",
      /* expected_match_result */ true, /* expected_cache_count */ 5,
      {{"main", kNotCached, kNotYetChecked},
       {"#div1", kNotCached, kNotYetChecked},
       {"#div11", kNotCached, kNotYetChecked},
       {"#div2", kNotCached, kNotYetChecked},
       {"#div21", kNotCached, kNotYetChecked},
       {"#div211", kNotCached, kNotYetChecked},
       {"#div22", kMatchedAndSomeChildrenChecked, kCached},
       {"#div221", kNotCached, kNotYetChecked},
       {"#div2211", kNotCached, kNotYetChecked},
       {"#div222", kNotCached, kNotYetChecked},
       {"#div2221", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div22211", kNotCached, kNotYetChecked},
       {"#div22212", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kCached},
       {"#div222121", kNotCached, kAlreadyNotMatched},
       {"#div22213", kNotCached, kAlreadyNotMatched},
       {"#div223", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div2231", kNotCached, kAlreadyNotMatched},
       {"#div224", kMatched, kCached},
       {"#div2241", kNotCached, kAlreadyNotMatched},
       {"#div2242", kNotCached, kAlreadyNotMatched},
       {"#div22421", kNotCached, kAlreadyNotMatched},
       {"#div22422", kNotCached, kAlreadyNotMatched},
       {"#div224221", kNotCached, kAlreadyNotMatched},
       {"#div224222", kNotCached, kAlreadyNotMatched},
       {"#div2242221", kNotCached, kAlreadyNotMatched},
       {"#div224223", kNotCached, kAlreadyNotMatched},
       {"#div22423", kNotCached, kAlreadyNotMatched},
       {"#div224231", kNotCached, kAlreadyNotMatched},
       {"#div22424", kNotCached, kAlreadyNotMatched},
       {"#div2243", kNotCached, kAlreadyNotMatched},
       {"#div22431", kNotCached, kAlreadyNotMatched},
       {"#div2244", kNotCached, kAlreadyNotMatched},
       {"#div225", kNotCached, kAlreadyNotMatched},
       {"#div2251", kNotCached, kAlreadyNotMatched},
       {"#div226", kNotCached, kAlreadyNotMatched},
       {"#div23", kNotCached, kNotYetChecked},
       {"#div231", kNotCached, kNotYetChecked},
       {"#div24", kNotCached, kNotYetChecked},
       {"#div3", kNotCached, kNotYetChecked},
       {"#div31", kNotCached, kNotYetChecked}});

  TestMatches(
      document, "div2", ":has(> .a .b)",
      /* expected_match_result */ false, /* expected_cache_count */ 4,
      {{"main", kNotCached, kNotYetChecked},
       {"#div1", kNotCached, kNotYetChecked},
       {"#div11", kNotCached, kNotYetChecked},
       {"#div2", kNotMatchedAndSomeChildrenChecked, kCached},
       {"#div21", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div211", kNotCached, kAlreadyNotMatched},
       {"#div22", kMatched, kCached},
       {"#div221", kNotCached, kAlreadyNotMatched},
       {"#div2211", kNotCached, kAlreadyNotMatched},
       {"#div222", kNotCached, kAlreadyNotMatched},
       {"#div2221", kNotCached, kAlreadyNotMatched},
       {"#div22211", kNotCached, kAlreadyNotMatched},
       {"#div22212", kNotCached, kAlreadyNotMatched},
       {"#div222121", kNotCached, kAlreadyNotMatched},
       {"#div22213", kNotCached, kAlreadyNotMatched},
       {"#div223", kNotCached, kAlreadyNotMatched},
       {"#div2231", kNotCached, kAlreadyNotMatched},
       {"#div224", kMatched, kCached},
       {"#div2241", kNotCached, kAlreadyNotMatched},
       {"#div2242", kNotCached, kAlreadyNotMatched},
       {"#div22421", kNotCached, kAlreadyNotMatched},
       {"#div22422", kNotCached, kAlreadyNotMatched},
       {"#div224221", kNotCached, kAlreadyNotMatched},
       {"#div224222", kNotCached, kAlreadyNotMatched},
       {"#div2242221", kNotCached, kAlreadyNotMatched},
       {"#div224223", kNotCached, kAlreadyNotMatched},
       {"#div22423", kNotCached, kAlreadyNotMatched},
       {"#div224231", kNotCached, kAlreadyNotMatched},
       {"#div22424", kNotCached, kAlreadyNotMatched},
       {"#div2243", kNotCached, kAlreadyNotMatched},
       {"#div22431", kNotCached, kAlreadyNotMatched},
       {"#div2244", kNotCached, kAlreadyNotMatched},
       {"#div225", kNotCached, kAlreadyNotMatched},
       {"#div2251", kNotCached, kAlreadyNotMatched},
       {"#div226", kNotCached, kAlreadyNotMatched},
       {"#div23", kNotCached, kAlreadyNotMatched},
       {"#div231", kNotCached, kAlreadyNotMatched},
       {"#div24", kNotCached, kAlreadyNotMatched},
       {"#div3", kNotCached, kNotYetChecked},
       {"#div31", kNotCached, kNotYetChecked}});

  TestMatches(
      document, "div2", ":has(> .a .c)",
      /* expected_match_result */ false, /* expected_cache_count */ 2,
      {{"main", kNotCached, kNotYetChecked},
       {"#div1", kNotCached, kNotYetChecked},
       {"#div11", kNotCached, kNotYetChecked},
       {"#div2", kNotMatchedAndSomeChildrenChecked, kCached},
       {"#div21", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div211", kNotCached, kAlreadyNotMatched},
       {"#div22", kNotCached, kAlreadyNotMatched},
       {"#div221", kNotCached, kAlreadyNotMatched},
       {"#div2211", kNotCached, kAlreadyNotMatched},
       {"#div222", kNotCached, kAlreadyNotMatched},
       {"#div2221", kNotCached, kAlreadyNotMatched},
       {"#div22211", kNotCached, kAlreadyNotMatched},
       {"#div22212", kNotCached, kAlreadyNotMatched},
       {"#div222121", kNotCached, kAlreadyNotMatched},
       {"#div22213", kNotCached, kAlreadyNotMatched},
       {"#div223", kNotCached, kAlreadyNotMatched},
       {"#div2231", kNotCached, kAlreadyNotMatched},
       {"#div224", kNotCached, kAlreadyNotMatched},
       {"#div2241", kNotCached, kAlreadyNotMatched},
       {"#div2242", kNotCached, kAlreadyNotMatched},
       {"#div22421", kNotCached, kAlreadyNotMatched},
       {"#div22422", kNotCached, kAlreadyNotMatched},
       {"#div224221", kNotCached, kAlreadyNotMatched},
       {"#div224222", kNotCached, kAlreadyNotMatched},
       {"#div2242221", kNotCached, kAlreadyNotMatched},
       {"#div224223", kNotCached, kAlreadyNotMatched},
       {"#div22423", kNotCached, kAlreadyNotMatched},
       {"#div224231", kNotCached, kAlreadyNotMatched},
       {"#div22424", kNotCached, kAlreadyNotMatched},
       {"#div2243", kNotCached, kAlreadyNotMatched},
       {"#div22431", kNotCached, kAlreadyNotMatched},
       {"#div2244", kNotCached, kAlreadyNotMatched},
       {"#div225", kNotCached, kAlreadyNotMatched},
       {"#div2251", kNotCached, kAlreadyNotMatched},
       {"#div226", kNotCached, kAlreadyNotMatched},
       {"#div23", kNotCached, kAlreadyNotMatched},
       {"#div231", kNotCached, kAlreadyNotMatched},
       {"#div24", kNotCached, kAlreadyNotMatched},
       {"#div3", kNotCached, kNotYetChecked},
       {"#div31", kNotCached, kNotYetChecked}});
}

TEST_F(HasMatchedCacheScopeContextTest, Case2StartsWithIndirectAdjacent) {
  // HasArgumentMatchTraversalScope::kAllNextSiblings

  auto* document = HTMLDocument::CreateForTest();
  document->write(R"HTML(
    <!DOCTYPE html>
    <main id=main>
      <div id=div1>
        <div id=div11></div>
      </div>
      <div id=div2>
        <div id=div21>
          <div id=div211></div>
          <div id=div212 class=a></div>
        </div>
        <div id=div22>
          <div id=div221></div>
          <div id=div222 class=a></div>
        </div>
        <div id=div23>
          <div id=div231></div>
          <div id=div232 class=a></div>
        </div>
        <div id=div24 class=a>
          <div id=div241></div>
          <div id=div242 class=a></div>
        </div>
        <div id=div25>
          <div id=div251></div>
          <div id=div252 class=a></div>
        </div>
      </div>
      <div id=div3 class=a>
        <div id=div31></div>
      </div>
    </main>
  )HTML");

  TestMatches(
      document, "div22", ":has(~ .a)",
      /* expected_match_result */ true, /* expected_cache_count */ 5,
      {{"main", kNotCached, kNotYetChecked},
       {"#div1", kNotCached, kNotYetChecked},
       {"#div11", kNotCached, kNotYetChecked},
       {"#div2", kNotCheckedAndSomeChildrenChecked},
       {"#div21", kMatched, kCached},
       {"#div211", kNotCached, kNotYetChecked},
       {"#div212", kNotCached, kNotYetChecked},
       {"#div22", kMatched, kCached},
       {"#div221", kNotCached, kNotYetChecked},
       {"#div222", kNotCached, kNotYetChecked},
       {"#div23", kMatched, kCached},
       {"#div231", kNotCached, kNotYetChecked},
       {"#div232", kNotCached, kNotYetChecked},
       {"#div24", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div241", kNotCached, kNotYetChecked},
       {"#div242", kNotCached, kNotYetChecked},
       {"#div25", kNotCached, kAlreadyNotMatched},
       {"#div251", kNotCached, kNotYetChecked},
       {"#div252", kNotCached, kNotYetChecked},
       {"#div3", kNotCached, kNotYetChecked},
       {"#div31", kNotCached, kNotYetChecked}});

  TestMatches(
      document, "div22", ":has(~ .b)",
      /* expected_match_result */ false, /* expected_cache_count */ 3,
      {{"main", kNotCached, kNotYetChecked},
       {"#div1", kNotCached, kNotYetChecked},
       {"#div11", kNotCached, kNotYetChecked},
       {"#div2", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div21", kNotCached, kNotYetChecked},
       {"#div211", kNotCached, kNotYetChecked},
       {"#div212", kNotCached, kNotYetChecked},
       {"#div22", kNotMatched, kCached},
       {"#div221", kNotCached, kNotYetChecked},
       {"#div222", kNotCached, kNotYetChecked},
       {"#div23", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div231", kNotCached, kNotYetChecked},
       {"#div232", kNotCached, kNotYetChecked},
       {"#div24", kNotCached, kAlreadyNotMatched},
       {"#div241", kNotCached, kNotYetChecked},
       {"#div242", kNotCached, kNotYetChecked},
       {"#div25", kNotCached, kAlreadyNotMatched},
       {"#div251", kNotCached, kNotYetChecked},
       {"#div252", kNotCached, kNotYetChecked},
       {"#div3", kNotCached, kNotYetChecked},
       {"#div31", kNotCached, kNotYetChecked}});
}

TEST_F(HasMatchedCacheScopeContextTest, Case2StartsWithDirectAdjacent) {
  // HasArgumentMatchTraversalScope::kAllNextSiblings

  auto* document = HTMLDocument::CreateForTest();
  document->write(R"HTML(
    <!DOCTYPE html>
    <main id=main>
      <div id=div1>
        <div id=div11></div>
      </div>
      <div id=div2>
        <div id=div21>
          <div id=div211></div>
          <div id=div212 class=a></div>
          <div id=div213 class=b></div>
        </div>
        <div id=div22>
          <div id=div221></div>
          <div id=div222 class=a></div>
          <div id=div223 class=b></div>
        </div>
        <div id=div23>
          <div id=div231></div>
          <div id=div232 class=a></div>
          <div id=div233 class=b></div>
        </div>
        <div id=div24 class=a>
          <div id=div241></div>
          <div id=div242 class=a></div>
          <div id=div243 class=b></div>
        </div>
        <div id=div25>
          <div id=div251></div>
          <div id=div252 class=a></div>
          <div id=div253 class=b></div>
        </div>
        <div id=div26 class=b>
          <div id=div261></div>
          <div id=div262 class=a></div>
          <div id=div263 class=b></div>
        </div>
        <div id=div27>
          <div id=div271></div>
          <div id=div272 class=a></div>
          <div id=div273 class=b></div>
        </div>
      </div>
      <div id=div3 class=a>
        <div id=div31></div>
      </div>
      <div id=div4 class=b>
        <div id=div41></div>
      </div>
    </main>
  )HTML");

  TestMatches(
      document, "div23", ":has(+ .a ~ .b)",
      /* expected_match_result */ true, /* expected_cache_count */ 3,
      {{"main", kNotCached, kNotYetChecked},
       {"#div1", kNotCached, kNotYetChecked},
       {"#div11", kNotCached, kNotYetChecked},
       {"#div2", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div21", kNotCached, kNotYetChecked},
       {"#div211", kNotCached, kNotYetChecked},
       {"#div212", kNotCached, kNotYetChecked},
       {"#div213", kNotCached, kNotYetChecked},
       {"#div22", kNotCached, kNotYetChecked},
       {"#div221", kNotCached, kNotYetChecked},
       {"#div222", kNotCached, kNotYetChecked},
       {"#div223", kNotCached, kNotYetChecked},
       {"#div23", kMatched, kCached},
       {"#div231", kNotCached, kNotYetChecked},
       {"#div232", kNotCached, kNotYetChecked},
       {"#div233", kNotCached, kNotYetChecked},
       {"#div24", kNotCached, kNotYetChecked},
       {"#div241", kNotCached, kNotYetChecked},
       {"#div242", kNotCached, kNotYetChecked},
       {"#div243", kNotCached, kNotYetChecked},
       {"#div25", kNotCached, kNotYetChecked},
       {"#div251", kNotCached, kNotYetChecked},
       {"#div252", kNotCached, kNotYetChecked},
       {"#div253", kNotCached, kNotYetChecked},
       {"#div26", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div261", kNotCached, kNotYetChecked},
       {"#div262", kNotCached, kNotYetChecked},
       {"#div263", kNotCached, kNotYetChecked},
       {"#div27", kNotCached, kAlreadyNotMatched},
       {"#div271", kNotCached, kNotYetChecked},
       {"#div272", kNotCached, kNotYetChecked},
       {"#div273", kNotCached, kNotYetChecked},
       {"#div3", kNotCached, kNotYetChecked},
       {"#div31", kNotCached, kNotYetChecked},
       {"#div4", kNotCached, kNotYetChecked},
       {"#div41", kNotCached, kNotYetChecked}});

  TestMatches(
      document, "div22", ":has(+ .a ~ .b)",
      /* expected_match_result */ false, /* expected_cache_count */ 3,
      {{"main", kNotCached, kNotYetChecked},
       {"#div1", kNotCached, kNotYetChecked},
       {"#div11", kNotCached, kNotYetChecked},
       {"#div2", kNotCheckedAndSomeChildrenChecked},
       {"#div21", kNotCached, kNotYetChecked},
       {"#div211", kNotCached, kNotYetChecked},
       {"#div212", kNotCached, kNotYetChecked},
       {"#div213", kNotCached, kNotYetChecked},
       {"#div22", kNotMatched, kCached},
       {"#div221", kNotCached, kNotYetChecked},
       {"#div222", kNotCached, kNotYetChecked},
       {"#div223", kNotCached, kNotYetChecked},
       {"#div23", kMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div231", kNotCached, kNotYetChecked},
       {"#div232", kNotCached, kNotYetChecked},
       {"#div233", kNotCached, kNotYetChecked},
       {"#div24", kNotCached, kAlreadyNotMatched},
       {"#div241", kNotCached, kNotYetChecked},
       {"#div242", kNotCached, kNotYetChecked},
       {"#div243", kNotCached, kNotYetChecked},
       {"#div25", kNotCached, kAlreadyNotMatched},
       {"#div251", kNotCached, kNotYetChecked},
       {"#div252", kNotCached, kNotYetChecked},
       {"#div253", kNotCached, kNotYetChecked},
       {"#div26", kNotCached, kAlreadyNotMatched},
       {"#div261", kNotCached, kNotYetChecked},
       {"#div262", kNotCached, kNotYetChecked},
       {"#div263", kNotCached, kNotYetChecked},
       {"#div27", kNotCached, kAlreadyNotMatched},
       {"#div271", kNotCached, kNotYetChecked},
       {"#div272", kNotCached, kNotYetChecked},
       {"#div273", kNotCached, kNotYetChecked},
       {"#div3", kNotCached, kNotYetChecked},
       {"#div31", kNotCached, kNotYetChecked},
       {"#div4", kNotCached, kNotYetChecked},
       {"#div41", kNotCached, kNotYetChecked}});

  TestMatches(
      document, "div22", ":has(+ .a ~ .c)",
      /* expected_match_result */ false, /* expected_cache_count */ 3,
      {{"main", kNotCached, kNotYetChecked},
       {"#div1", kNotCached, kNotYetChecked},
       {"#div11", kNotCached, kNotYetChecked},
       {"#div2", kNotCheckedAndSomeChildrenChecked},
       {"#div21", kNotCached, kNotYetChecked},
       {"#div211", kNotCached, kNotYetChecked},
       {"#div212", kNotCached, kNotYetChecked},
       {"#div213", kNotCached, kNotYetChecked},
       {"#div22", kNotMatched, kCached},
       {"#div221", kNotCached, kNotYetChecked},
       {"#div222", kNotCached, kNotYetChecked},
       {"#div223", kNotCached, kNotYetChecked},
       {"#div23", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div231", kNotCached, kNotYetChecked},
       {"#div232", kNotCached, kNotYetChecked},
       {"#div233", kNotCached, kNotYetChecked},
       {"#div24", kNotCached, kAlreadyNotMatched},
       {"#div241", kNotCached, kNotYetChecked},
       {"#div242", kNotCached, kNotYetChecked},
       {"#div243", kNotCached, kNotYetChecked},
       {"#div25", kNotCached, kAlreadyNotMatched},
       {"#div251", kNotCached, kNotYetChecked},
       {"#div252", kNotCached, kNotYetChecked},
       {"#div253", kNotCached, kNotYetChecked},
       {"#div26", kNotCached, kAlreadyNotMatched},
       {"#div261", kNotCached, kNotYetChecked},
       {"#div262", kNotCached, kNotYetChecked},
       {"#div263", kNotCached, kNotYetChecked},
       {"#div27", kNotCached, kAlreadyNotMatched},
       {"#div271", kNotCached, kNotYetChecked},
       {"#div272", kNotCached, kNotYetChecked},
       {"#div273", kNotCached, kNotYetChecked},
       {"#div3", kNotCached, kNotYetChecked},
       {"#div31", kNotCached, kNotYetChecked},
       {"#div4", kNotCached, kNotYetChecked},
       {"#div41", kNotCached, kNotYetChecked}});
}

TEST_F(HasMatchedCacheScopeContextTest, Case3) {
  // HasArgumentMatchTraversalScope::kOneNextSiblingSubtree

  auto* document = HTMLDocument::CreateForTest();
  document->write(R"HTML(
    <!DOCTYPE html>
    <main id=main>
      <div id=div1>
        <div id=div11></div>
      </div>
      <div id=div2>
        <div id=div21></div>
        <div id=div22></div>
        <div id=div23 class=a>
          <div id=div231></div>
          <div id=div232>
            <div id=div2321></div>
            <div id=div2322 class=b>
              <div id=div23221></div>
            </div>
            <div id=div2323></div>
          </div>
          <div id=div233></div>
          <div id=div234>
            <div id=div2341></div>
            <div id=div2342></div>
            <div id=div2343 class=a>
              <div id=div23431></div>
              <div id=div23432>
                <div id=div234321></div>
                <div id=div234322 class=b>
                  <div id=div2343221></div>
                </div>
                <div id=div234323></div>
              </div>
              <div id=div23433>
                <div id=div234331></div>
              </div>
              <div id=div23434></div>
            </div>
            <div id=div2344>
              <div id=div23441></div>
            </div>
            <div id=div2345></div>
          </div>
          <div id=div235>
            <div id=div2351></div>
          </div>
          <div id=div236></div>
        </div>
        <div id=div24>
          <div id=div241></div>
        </div>
        <div id=div25></div>
      </div>
      <div id=div3></div>
      <div id=div4></div>
    </main>
  )HTML");

  TestMatches(
      document, "div22", ":has(+ .a .b)",
      /* expected_match_result */ true, /* expected_cache_count */ 10,
      {{"main", kNotCached, kNotYetChecked},
       {"#div1", kNotCached, kNotYetChecked},
       {"#div11", kNotCached, kNotYetChecked},
       {"#div2", kNotCached, kNotYetChecked},
       {"#div21", kNotCached, kNotYetChecked},
       {"#div22", kMatched, kCached},
       {"#div23", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div231", kNotCached, kNotYetChecked},
       {"#div232", kNotCached, kNotYetChecked},
       {"#div2321", kNotCached, kNotYetChecked},
       {"#div2322", kNotCached, kNotYetChecked},
       {"#div23221", kNotCached, kNotYetChecked},
       {"#div2323", kNotCached, kNotYetChecked},
       {"#div233", kNotCached, kNotYetChecked},
       {"#div234", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div2341", kNotCached, kNotYetChecked},
       {"#div2342", kMatched, kCached},
       {"#div2343", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div23431", kNotCached, kNotYetChecked},
       {"#div23432", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div234321", kNotCached, kNotYetChecked},
       {"#div234322", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kCached},
       {"#div2343221", kNotCached, kAlreadyNotMatched},
       {"#div234323", kNotCached, kAlreadyNotMatched},
       {"#div23433", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kCached},
       {"#div234331", kNotCached, kAlreadyNotMatched},
       {"#div23434", kNotCached, kAlreadyNotMatched},
       {"#div2344", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div23441", kNotCached, kAlreadyNotMatched},
       {"#div2345", kNotCached, kAlreadyNotMatched},
       {"#div235", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div2351", kNotCached, kAlreadyNotMatched},
       {"#div236", kNotCached, kAlreadyNotMatched},
       {"#div24", kNotCached, kNotYetChecked},
       {"#div241", kNotCached, kNotYetChecked},
       {"#div25", kNotCached, kNotYetChecked},
       {"#div3", kNotCached, kNotYetChecked},
       {"#div4", kNotCached, kNotYetChecked}});

  TestMatches(
      document, "div1", ":has(+ .a .b)",
      /* expected_match_result */ false, /* expected_cache_count */ 5,
      {{"main", kNotCached, kNotYetChecked},
       {"#div1", kNotMatched, kCached},
       {"#div11", kNotCached, kNotYetChecked},
       {"#div2", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div21", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div22", kMatched, kCached},
       {"#div23", kNotCached, kAlreadyNotMatched},
       {"#div231", kNotCached, kAlreadyNotMatched},
       {"#div232", kNotCached, kAlreadyNotMatched},
       {"#div2321", kNotCached, kAlreadyNotMatched},
       {"#div2322", kNotCached, kAlreadyNotMatched},
       {"#div23221", kNotCached, kAlreadyNotMatched},
       {"#div2323", kNotCached, kAlreadyNotMatched},
       {"#div233", kNotCached, kAlreadyNotMatched},
       {"#div234", kNotCached, kAlreadyNotMatched},
       {"#div2341", kNotCached, kAlreadyNotMatched},
       {"#div2342", kMatched, kCached},
       {"#div2343", kNotCached, kAlreadyNotMatched},
       {"#div23431", kNotCached, kAlreadyNotMatched},
       {"#div23432", kNotCached, kAlreadyNotMatched},
       {"#div234321", kNotCached, kAlreadyNotMatched},
       {"#div234322", kNotCached, kAlreadyNotMatched},
       {"#div2343221", kNotCached, kAlreadyNotMatched},
       {"#div234323", kNotCached, kAlreadyNotMatched},
       {"#div23433", kNotCached, kAlreadyNotMatched},
       {"#div234331", kNotCached, kAlreadyNotMatched},
       {"#div23434", kNotCached, kAlreadyNotMatched},
       {"#div2344", kNotCached, kAlreadyNotMatched},
       {"#div23441", kNotCached, kAlreadyNotMatched},
       {"#div2345", kNotCached, kAlreadyNotMatched},
       {"#div235", kNotCached, kAlreadyNotMatched},
       {"#div2351", kNotCached, kAlreadyNotMatched},
       {"#div236", kNotCached, kAlreadyNotMatched},
       {"#div24", kNotCached, kAlreadyNotMatched},
       {"#div241", kNotCached, kAlreadyNotMatched},
       {"#div25", kNotCached, kAlreadyNotMatched},
       {"#div3", kNotCached, kNotYetChecked},
       {"#div4", kNotCached, kNotYetChecked}});

  TestMatches(
      document, "div22", ":has(+ .a .c)",
      /* expected_match_result */ false, /* expected_cache_count */ 3,
      {{"main", kNotCached, kNotYetChecked},
       {"#div1", kNotCached, kNotYetChecked},
       {"#div11", kNotCached, kNotYetChecked},
       {"#div2", kNotCached, kNotYetChecked},
       {"#div21", kNotCached, kNotYetChecked},
       {"#div22", kNotMatched, kCached},
       {"#div23", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div231", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div232", kNotCached, kAlreadyNotMatched},
       {"#div2321", kNotCached, kAlreadyNotMatched},
       {"#div2322", kNotCached, kAlreadyNotMatched},
       {"#div23221", kNotCached, kAlreadyNotMatched},
       {"#div2323", kNotCached, kAlreadyNotMatched},
       {"#div233", kNotCached, kAlreadyNotMatched},
       {"#div234", kNotCached, kAlreadyNotMatched},
       {"#div2341", kNotCached, kAlreadyNotMatched},
       {"#div2342", kNotCached, kAlreadyNotMatched},
       {"#div2343", kNotCached, kAlreadyNotMatched},
       {"#div23431", kNotCached, kAlreadyNotMatched},
       {"#div23432", kNotCached, kAlreadyNotMatched},
       {"#div234321", kNotCached, kAlreadyNotMatched},
       {"#div234322", kNotCached, kAlreadyNotMatched},
       {"#div2343221", kNotCached, kAlreadyNotMatched},
       {"#div234323", kNotCached, kAlreadyNotMatched},
       {"#div23433", kNotCached, kAlreadyNotMatched},
       {"#div234331", kNotCached, kAlreadyNotMatched},
       {"#div23434", kNotCached, kAlreadyNotMatched},
       {"#div2344", kNotCached, kAlreadyNotMatched},
       {"#div23441", kNotCached, kAlreadyNotMatched},
       {"#div2345", kNotCached, kAlreadyNotMatched},
       {"#div235", kNotCached, kAlreadyNotMatched},
       {"#div2351", kNotCached, kAlreadyNotMatched},
       {"#div236", kNotCached, kAlreadyNotMatched},
       {"#div24", kNotCached, kNotYetChecked},
       {"#div241", kNotCached, kNotYetChecked},
       {"#div25", kNotCached, kNotYetChecked},
       {"#div3", kNotCached, kNotYetChecked},
       {"#div4", kNotCached, kNotYetChecked}});
}

TEST_F(HasMatchedCacheScopeContextTest, Case4) {
  // HasArgumentMatchTraversalScope::kAllNextSiblingSubtrees

  auto* document = HTMLDocument::CreateForTest();
  document->write(R"HTML(
    <!DOCTYPE html>
    <main id=main>
      <div id=div1>
        <div id=div11></div>
      </div>
      <div id=div2>
        <div id=div21></div>
        <div id=div22></div>
        <div id=div23 class=a>
          <div id=div231></div>
          <div id=div232>
            <div id=div2321></div>
            <div id=div2322 class=b>
              <div id=div23221></div>
            </div>
            <div id=div2323></div>
          </div>
          <div id=div233></div>
        </div>
        <div id=div24>
          <div id=div241></div>
          <div id=div242>
            <div id=div2421></div>
            <div id=div2422></div>
            <div id=div2423 class=a>
              <div id=div24231></div>
              <div id=div24232>
                <div id=div242321></div>
                <div id=div242322 class=b>
                  <div id=div2423221></div>
                </div>
                <div id=div242323></div>
              </div>
              <div id=div24233>
                <div id=div242331></div>
              </div>
              <div id=div24234></div>
            </div>
            <div id=div2424>
              <div id=div24241></div>
            </div>
            <div id=div2425></div>
          </div>
          <div id=div243>
            <div id=div2431></div>
          </div>
          <div id=div244></div>
        </div>
        <div id=div25>
          <div id=div251></div>
        </div>
        <div id=div26>
          <div id=div261></div>
        </div>
      </div>
      <div id=div3>
        <div id=div31></div>
      </div>
      <div id=div4>
        <div id=div41></div>
      </div>
    </main>
  )HTML");

  TestMatches(
      document, "div22", ":has(~ .a .b)",
      /* expected_match_result */ true, /* expected_cache_count */ 10,
      {{"main", kNotCached, kNotYetChecked},
       {"#div1", kNotCached, kNotYetChecked},
       {"#div11", kNotCached, kNotYetChecked},
       {"#div2", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div21", kMatched, kCached},
       {"#div22", kMatched, kCached},
       {"#div23", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div231", kNotCached, kNotYetChecked},
       {"#div232", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div2321", kNotCached, kNotYetChecked},
       {"#div2322", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div23221", kNotCached, kAlreadyNotMatched},
       {"#div2323", kNotCached, kAlreadyNotMatched},
       {"#div233", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div24", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div241", kNotCached, kAlreadyNotMatched},
       {"#div242", kNotCached, kAlreadyNotMatched},
       {"#div2421", kMatched, kCached},
       {"#div2422", kMatched, kCached},
       {"#div2423", kNotCached, kAlreadyNotMatched},
       {"#div24231", kNotCached, kAlreadyNotMatched},
       {"#div24232", kNotCached, kAlreadyNotMatched},
       {"#div242321", kNotCached, kAlreadyNotMatched},
       {"#div242322", kNotCached, kAlreadyNotMatched},
       {"#div2423221", kNotCached, kAlreadyNotMatched},
       {"#div242323", kNotCached, kAlreadyNotMatched},
       {"#div24233", kNotCached, kAlreadyNotMatched},
       {"#div242331", kNotCached, kAlreadyNotMatched},
       {"#div24234", kNotCached, kAlreadyNotMatched},
       {"#div2424", kNotCached, kAlreadyNotMatched},
       {"#div24241", kNotCached, kAlreadyNotMatched},
       {"#div2425", kNotCached, kAlreadyNotMatched},
       {"#div243", kNotCached, kAlreadyNotMatched},
       {"#div2431", kNotCached, kAlreadyNotMatched},
       {"#div244", kNotCached, kAlreadyNotMatched},
       {"#div25", kNotCached, kAlreadyNotMatched},
       {"#div251", kNotCached, kAlreadyNotMatched},
       {"#div26", kNotCached, kAlreadyNotMatched},
       {"#div261", kNotCached, kAlreadyNotMatched},
       {"#div3", kNotCached, kNotYetChecked},
       {"#div31", kNotCached, kNotYetChecked},
       {"#div4", kNotCached, kNotYetChecked},
       {"#div41", kNotCached, kNotYetChecked}});

  TestMatches(
      document, "div21", ":has(~ .a .b)",
      /* expected_match_result */ true, /* expected_cache_count */ 10,
      {{"main", kNotCached, kNotYetChecked},
       {"#div1", kNotCached, kNotYetChecked},
       {"#div11", kNotCached, kNotYetChecked},
       {"#div2", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div21", kMatched, kCached},
       {"#div22", kMatched, kCached},
       {"#div23", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div231", kNotCached, kNotYetChecked},
       {"#div232", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div2321", kNotCached, kNotYetChecked},
       {"#div2322", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div23221", kNotCached, kAlreadyNotMatched},
       {"#div2323", kNotCached, kAlreadyNotMatched},
       {"#div233", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div24", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div241", kNotCached, kAlreadyNotMatched},
       {"#div242", kNotCached, kAlreadyNotMatched},
       {"#div2421", kMatched, kCached},
       {"#div2422", kMatched, kCached},
       {"#div2423", kNotCached, kAlreadyNotMatched},
       {"#div24231", kNotCached, kAlreadyNotMatched},
       {"#div24232", kNotCached, kAlreadyNotMatched},
       {"#div242321", kNotCached, kAlreadyNotMatched},
       {"#div242322", kNotCached, kAlreadyNotMatched},
       {"#div2423221", kNotCached, kAlreadyNotMatched},
       {"#div242323", kNotCached, kAlreadyNotMatched},
       {"#div24233", kNotCached, kAlreadyNotMatched},
       {"#div242331", kNotCached, kAlreadyNotMatched},
       {"#div24234", kNotCached, kAlreadyNotMatched},
       {"#div2424", kNotCached, kAlreadyNotMatched},
       {"#div24241", kNotCached, kAlreadyNotMatched},
       {"#div2425", kNotCached, kAlreadyNotMatched},
       {"#div243", kNotCached, kAlreadyNotMatched},
       {"#div2431", kNotCached, kAlreadyNotMatched},
       {"#div244", kNotCached, kAlreadyNotMatched},
       {"#div25", kNotCached, kAlreadyNotMatched},
       {"#div251", kNotCached, kAlreadyNotMatched},
       {"#div26", kNotCached, kAlreadyNotMatched},
       {"#div261", kNotCached, kAlreadyNotMatched},
       {"#div3", kNotCached, kNotYetChecked},
       {"#div31", kNotCached, kNotYetChecked},
       {"#div4", kNotCached, kNotYetChecked},
       {"#div41", kNotCached, kNotYetChecked}});

  TestMatches(
      document, "div1", ":has(~ .a .b)",
      /* expected_match_result */ false, /* expected_cache_count */ 7,
      {{"main", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div1", kNotMatched, kCached},
       {"#div11", kNotCached, kNotYetChecked},
       {"#div2", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div21", kMatched, kCached},
       {"#div22", kMatched, kCached},
       {"#div23", kNotCached, kAlreadyNotMatched},
       {"#div231", kNotCached, kAlreadyNotMatched},
       {"#div232", kNotCached, kAlreadyNotMatched},
       {"#div2321", kNotCached, kAlreadyNotMatched},
       {"#div2322", kNotCached, kAlreadyNotMatched},
       {"#div23221", kNotCached, kAlreadyNotMatched},
       {"#div2323", kNotCached, kAlreadyNotMatched},
       {"#div233", kNotCached, kAlreadyNotMatched},
       {"#div24", kNotCached, kAlreadyNotMatched},
       {"#div241", kNotCached, kAlreadyNotMatched},
       {"#div242", kNotCached, kAlreadyNotMatched},
       {"#div2421", kMatched, kCached},
       {"#div2422", kMatched, kCached},
       {"#div2423", kNotCached, kAlreadyNotMatched},
       {"#div24231", kNotCached, kAlreadyNotMatched},
       {"#div24232", kNotCached, kAlreadyNotMatched},
       {"#div242321", kNotCached, kAlreadyNotMatched},
       {"#div242322", kNotCached, kAlreadyNotMatched},
       {"#div2423221", kNotCached, kAlreadyNotMatched},
       {"#div242323", kNotCached, kAlreadyNotMatched},
       {"#div24233", kNotCached, kAlreadyNotMatched},
       {"#div242331", kNotCached, kAlreadyNotMatched},
       {"#div24234", kNotCached, kAlreadyNotMatched},
       {"#div2424", kNotCached, kAlreadyNotMatched},
       {"#div24241", kNotCached, kAlreadyNotMatched},
       {"#div2425", kNotCached, kAlreadyNotMatched},
       {"#div243", kNotCached, kAlreadyNotMatched},
       {"#div2431", kNotCached, kAlreadyNotMatched},
       {"#div244", kNotCached, kAlreadyNotMatched},
       {"#div25", kNotCached, kAlreadyNotMatched},
       {"#div251", kNotCached, kAlreadyNotMatched},
       {"#div26", kNotCached, kAlreadyNotMatched},
       {"#div261", kNotCached, kAlreadyNotMatched},
       {"#div3", kNotCached, kAlreadyNotMatched},
       {"#div31", kNotCached, kAlreadyNotMatched},
       {"#div4", kNotCached, kAlreadyNotMatched},
       {"#div41", kNotCached, kAlreadyNotMatched}});

  TestMatches(
      document, "div22", ":has(~ .a .c)",
      /* expected_match_result */ false, /* expected_cache_count */ 3,
      {{"main", kNotCached, kNotYetChecked},
       {"#div1", kNotCached, kNotYetChecked},
       {"#div11", kNotCached, kNotYetChecked},
       {"#div2", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div21", kNotCached, kNotYetChecked},
       {"#div22", kNotMatched, kCached},
       {"#div23", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div231", kNotCached, kAlreadyNotMatched},
       {"#div232", kNotCached, kAlreadyNotMatched},
       {"#div2321", kNotCached, kAlreadyNotMatched},
       {"#div2322", kNotCached, kAlreadyNotMatched},
       {"#div23221", kNotCached, kAlreadyNotMatched},
       {"#div2323", kNotCached, kAlreadyNotMatched},
       {"#div233", kNotCached, kAlreadyNotMatched},
       {"#div24", kNotCached, kAlreadyNotMatched},
       {"#div241", kNotCached, kAlreadyNotMatched},
       {"#div242", kNotCached, kAlreadyNotMatched},
       {"#div2421", kNotCached, kAlreadyNotMatched},
       {"#div2422", kNotCached, kAlreadyNotMatched},
       {"#div2423", kNotCached, kAlreadyNotMatched},
       {"#div24231", kNotCached, kAlreadyNotMatched},
       {"#div24232", kNotCached, kAlreadyNotMatched},
       {"#div242321", kNotCached, kAlreadyNotMatched},
       {"#div242322", kNotCached, kAlreadyNotMatched},
       {"#div2423221", kNotCached, kAlreadyNotMatched},
       {"#div242323", kNotCached, kAlreadyNotMatched},
       {"#div24233", kNotCached, kAlreadyNotMatched},
       {"#div242331", kNotCached, kAlreadyNotMatched},
       {"#div24234", kNotCached, kAlreadyNotMatched},
       {"#div2424", kNotCached, kAlreadyNotMatched},
       {"#div24241", kNotCached, kAlreadyNotMatched},
       {"#div2425", kNotCached, kAlreadyNotMatched},
       {"#div243", kNotCached, kAlreadyNotMatched},
       {"#div2431", kNotCached, kAlreadyNotMatched},
       {"#div244", kNotCached, kAlreadyNotMatched},
       {"#div25", kNotCached, kAlreadyNotMatched},
       {"#div251", kNotCached, kAlreadyNotMatched},
       {"#div26", kNotCached, kAlreadyNotMatched},
       {"#div261", kNotCached, kAlreadyNotMatched},
       {"#div3", kNotCached, kNotYetChecked},
       {"#div31", kNotCached, kNotYetChecked},
       {"#div4", kNotCached, kNotYetChecked},
       {"#div41", kNotCached, kNotYetChecked}});
}

TEST_F(HasMatchedCacheScopeContextTest,
       QuerySelectorAllCase1StartsWithDescendantCombinator) {
  // HasArgumentMatchTraversalScope::kSubtree

  auto* document = HTMLDocument::CreateForTest();
  document->write(R"HTML(
    <!DOCTYPE html>
    <main id=main>
      <div id=div1>
        <div id=div11></div>
        <div id=div12 class=a>
          <div id=div121 class=b>
            <div id=div1211 class=a>
              <div id=div12111 class=b></div>
            </div>
          </div>
        </div>
        <div id=div13 class=a>
          <div id=div131 class=b></div>
        </div>
        <div id=div14></div>
      </div>
    </main>
  )HTML");

  TestMatches(
      document, "div1", ":has(.a .b)",
      /* expected_match_result */ true,
      /* expected_cache_count */ 7,
      {{"html", kMatched, kCached},
       {"body", kMatched, kCached},
       {"#main", kMatched, kCached},
       {"#div1", kMatchedAndSomeChildrenChecked, kCached},
       {"#div11", kNotCached, kNotYetChecked},
       {"#div12", kNotCached, kNotYetChecked},
       {"#div121", kNotCached, kNotYetChecked},
       {"#div1211", kNotCached, kNotYetChecked},
       {"#div12111", kNotCached, kNotYetChecked},
       {"#div13", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div131", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div14", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached}});

  TestMatches(document, "div11", ":has(.a .b)",
              /* expected_match_result */ false,
              /* expected_cache_count */ 1, {{"#div11", kNotMatched, kCached}});

  TestMatches(document, "div12", ":has(.a .b)",
              /* expected_match_result */ true,
              /* expected_cache_count */ 8,
              {{"html", kMatched, kCached},
               {"body", kMatched, kCached},
               {"#main", kMatched, kCached},
               {"#div1", kMatched, kCached},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div12", kMatched, kCached},
               {"#div121", kMatched, kCached},
               {"#div1211", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div12111", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kCached},
               {"#div13", kNotCached, kNotYetChecked},
               {"#div131", kNotCached, kNotYetChecked},
               {"#div14", kNotCached, kNotYetChecked}});

  // ':has(.a .b)' does not match #div1211 but this caches possibly matched
  // elements because argument selector matching can cross over the :has()
  // scope element.
  TestMatches(document, "div1211", ":has(.a .b)",
              /* expected_match_result */ false,
              /* expected_cache_count */ 8,
              {{"html", kMatched, kCached},
               {"body", kMatched, kCached},
               {"#main", kMatched, kCached},
               {"#div1", kMatched, kCached},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div12", kMatched, kCached},
               {"#div121", kMatched, kCached},
               {"#div1211", kNotMatchedAndSomeChildrenChecked, kCached},
               {"#div12111", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kCached},
               {"#div13", kNotCached, kNotYetChecked},
               {"#div131", kNotCached, kNotYetChecked},
               {"#div14", kNotCached, kNotYetChecked}});

  // ':has(.a .b)' does not match #div13 but this caches possibly matched
  // elements because argument selector matching can cross over the :has()
  // scope element.
  TestMatches(
      document, "div13", ":has(.a .b)",
      /* expected_match_result */ false,
      /* expected_cache_count */ 6,
      {{"html", kMatched, kCached},
       {"body", kMatched, kCached},
       {"#main", kMatched, kCached},
       {"#div1", kMatched, kCached},
       {"#div11", kNotCached, kNotYetChecked},
       {"#div12", kNotCached, kNotYetChecked},
       {"#div121", kNotCached, kNotYetChecked},
       {"#div1211", kNotCached, kNotYetChecked},
       {"#div12111", kNotCached, kNotYetChecked},
       {"#div13", kNotMatchedAndSomeChildrenChecked, kCached},
       {"#div131", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div14", kNotCached, kNotYetChecked}});

  TestQuerySelectorAll(
      document, "main", ":has(.a .b)", {"div1", "div12", "div121"},
      /* expected_cache_count */ 12,
      {{"html", kMatched, kCached},
       {"body", kMatched, kCached},
       {"#main", kMatched, kCached},
       {"#div1", kMatchedAndSomeChildrenChecked, kCached},
       {"#div11", kNotMatched, kCached},
       {"#div12", kMatched, kCached},
       {"#div121", kMatched, kCached},
       {"#div1211", kNotMatchedAndSomeChildrenChecked, kCached},
       {"#div12111", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kCached},
       {"#div13", kNotMatchedAndSomeChildrenChecked, kCached},
       {"#div131", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div14", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached}});
}

TEST_F(HasMatchedCacheScopeContextTest,
       QuerySelectorAllCase1StartsWithChildCombinator) {
  // HasArgumentMatchTraversalScope::kSubtree

  auto* document = HTMLDocument::CreateForTest();
  document->write(R"HTML(
    <!DOCTYPE html>
    <main id=main>
      <div id=div1>
        <div id=div11 class=a>
          <div id=div111 class=b>
            <div id=div1111 class=a>
              <div id=div11111 class=b></div>
            </div>
          </div>
        </div>
      </div>
    </main>
  )HTML");

  TestMatches(document, "div1", ":has(> .a .b)",
              /* expected_match_result */ true,
              /* expected_cache_count */ 4,
              {{"#div1", kMatched, kCached},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div111", kMatched, kCached},
               {"#div1111", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div11111", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kCached}});

  TestMatches(
      document, "div11", ":has(> .a .b)",
      /* expected_match_result */ false,
      /* expected_cache_count */ 3,
      {{"#div1", kMatched, kCached},
       {"#div11", kNotMatchedAndSomeChildrenChecked, kCached},
       {"#div111", kMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div1111", kNotCached, kAlreadyNotMatched},
       {"#div11111", kNotCached, kAlreadyNotMatched}});

  TestQuerySelectorAll(
      document, "main", ":has(> .a .b)", {"div1", "div111"},
      /* expected_cache_count */ 5,
      {{"#div1", kMatched, kCached},
       {"#div11", kNotMatchedAndSomeChildrenChecked, kCached},
       {"#div111", kMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div1111", kNotMatchedAndSomeChildrenChecked, kCached},
       {"#div11111", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kCached}});
}

TEST_F(HasMatchedCacheScopeContextTest,
       QuerySelectorAllCase1StartsWithChildCombinatorNonSubjectHas) {
  // HasArgumentMatchTraversalScope::kSubtree

  auto* document = HTMLDocument::CreateForTest();
  document->write(R"HTML(
    <!DOCTYPE html>
    <main id=main>
      <div id=div1>
        <div id=div11>
          <div id=div111 class=a>
            <div id=div1111 class=a>
              <div id=div11111 class=b></div>
            </div>
            <div id=div1112></div>
          </div>
          <div id=div112>
            <div id=div1121></div>
          </div>
          <div id=div113 class=c>
            <div id=div1131 class=d></div>
          </div>
        </div>
        <div id=div12 class=c>
          <div id=div121 class=d></div>
        </div>
      </div>
      <div id=div2 class=c>
        <div id=div21 class=d></div>
      </div>
    </main>
  )HTML");

  TestMatches(document, "div112", ":has(> .a .b)",
              /* expected_match_result */ false,
              /* expected_cache_count */ 2,
              {{"#div112", kNotMatchedAndSomeChildrenChecked, kCached},
               {"#div1121", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kCached}});

  TestMatches(document, "div111", ":has(> .a .b)",
              /* expected_match_result */ true,
              /* expected_cache_count */ 4,
              {{"#div111", kMatchedAndSomeChildrenChecked, kCached},
               {"#div1111", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div11111", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kCached},
               {"#div1112", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kCached}});

  TestMatches(
      document, "div11", ":has(> .a .b)",
      /* expected_match_result */ true,
      /* expected_cache_count */ 6,
      {{"#div11", kMatchedAndSomeChildrenChecked, kCached},
       {"#div111", kMatchedAndSomeChildrenChecked, kCached},
       {"#div1111", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div11111", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kCached},
       {"#div1112", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div112", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div1121", kNotCached, kAlreadyNotMatched},
       {"#div113", kNotCached, kAlreadyNotMatched},
       {"#div1131", kNotCached, kAlreadyNotMatched}});

  TestMatches(
      document, "div1", ":has(> .a .b)",
      /* expected_match_result */ false,
      /* expected_cache_count */ 3,
      {{"#div1", kNotMatchedAndSomeChildrenChecked, kCached},
       {"#div11", kMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div111", kMatched, kCached},
       {"#div1111", kNotCached, kAlreadyNotMatched},
       {"#div11111", kNotCached, kAlreadyNotMatched},
       {"#div1112", kNotCached, kAlreadyNotMatched},
       {"#div112", kNotCached, kAlreadyNotMatched},
       {"#div1121", kNotCached, kAlreadyNotMatched},
       {"#div113", kNotCached, kAlreadyNotMatched},
       {"#div1131", kNotCached, kAlreadyNotMatched},
       {"#div12", kNotCached, kAlreadyNotMatched},
       {"#div121", kNotCached, kAlreadyNotMatched}});

  TestQuerySelectorAll(
      document, "main", ":has(> .a .b) ~ .c .d", {"div1131", "div121"},
      /* expected_cache_count */ 8,
      {{"#div1", kNotMatchedAndSomeChildrenChecked, kCached},
       {"#div11",
        kMatchedAndAllDescendantsOrNextSiblingsCheckedAndSomeChildrenChecked,
        kCached},
       {"#div111", kMatchedAndSomeChildrenChecked, kCached},
       {"#div1111", kNotCheckedAndSomeChildrenChecked, kAlreadyNotMatched},
       {"#div11111", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kCached},
       {"#div1112", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div112",
        kNotMatchedAndAllDescendantsOrNextSiblingsCheckedAndSomeChildrenChecked,
        kCached},
       {"#div1121", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div113", kNotCached, kAlreadyNotMatched},
       {"#div1131", kNotCached, kAlreadyNotMatched},
       {"#div12", kNotCached, kAlreadyNotMatched},
       {"#div121", kNotCached, kAlreadyNotMatched},
       {"#div2", kNotCached, kNotYetChecked},
       {"#div21", kNotCached, kNotYetChecked}});
}

TEST_F(HasMatchedCacheScopeContextTest,
       QuerySelectorAllCase4StartsWithDirectAdjacentCombinator) {
  // HasArgumentMatchTraversalScope::kAllNextSiblingSubtrees

  auto* document = HTMLDocument::CreateForTest();
  document->write(R"HTML(
    <!DOCTYPE html>
    <main id=main>
      <div id=div1>
        <div id=div11></div>
        <div id=div12 class=a></div>
        <div id=div13 class=b>
          <div id=div131></div>
          <div id=div132 class=c></div>
        </div>
        <div id=div14>
          <div id=div141></div>
        </div>
        <div id=div15></div>
      </div>
      <div id=div2>
        <div id=div21></div>
      </div>
      <div id=div3>
        <div id=div31></div>
      </div>
      <div id=div4>
        <div id=div41></div>
      </div>
      <div id=div5 class=a>
        <div id=div51></div>
      </div>
      <div id=div6 class=b>
        <div id=div61 class=c></div>
      </div>
    </main>
  )HTML");

  TestMatches(
      document, "div1", ":has(+ .a ~ .b .c)", /* expected_match_result */ false,
      /* expected_cache_count */ 4,
      {{"main", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div1", kNotMatched, kCached},
       {"#div11", kNotCached, kNotYetChecked},
       {"#div12", kNotCached, kNotYetChecked},
       {"#div13", kNotCached, kNotYetChecked},
       {"#div131", kNotCached, kNotYetChecked},
       {"#div132", kNotCached, kNotYetChecked},
       {"#div14", kNotCached, kNotYetChecked},
       {"#div141", kNotCached, kNotYetChecked},
       {"#div15", kNotCached, kNotYetChecked},
       {"#div2", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div21", kNotCached, kAlreadyNotMatched},
       {"#div3", kNotCached, kAlreadyNotMatched},
       {"#div31", kNotCached, kAlreadyNotMatched},
       {"#div4", kMatched, kCached},
       {"#div41", kNotCached, kAlreadyNotMatched},
       {"#div5", kNotCached, kAlreadyNotMatched},
       {"#div51", kNotCached, kAlreadyNotMatched},
       {"#div6", kNotCached, kAlreadyNotMatched},
       {"#div61", kNotCached, kAlreadyNotMatched}});

  TestMatches(
      document, "div11", ":has(+ .a ~ .b .c)",
      /* expected_match_result */ true,
      /* expected_cache_count */ 5,
      {{"#div1", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div11", kMatched, kCached},
       {"#div12", kNotCached, kNotYetChecked},
       {"#div13", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div131", kNotCached, kNotYetChecked},
       {"#div132", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div14", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div141", kNotCached, kAlreadyNotMatched},
       {"#div15", kNotCached, kAlreadyNotMatched}});

  TestMatches(
      document, "div12", ":has(+ .a ~ .b .c)",
      /* expected_match_result */ false,
      /* expected_cache_count */ 4,
      {{"#div1", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div11", kMatched, kCached},
       {"#div12", kNotMatched, kCached},
       {"#div13", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div131", kNotCached, kAlreadyNotMatched},
       {"#div132", kNotCached, kAlreadyNotMatched},
       {"#div14", kNotCached, kAlreadyNotMatched},
       {"#div141", kNotCached, kAlreadyNotMatched},
       {"#div15", kNotCached, kAlreadyNotMatched}});

  TestQuerySelectorAll(
      document, "main", ":has(+ .a ~ .b .c)", {"div11", "div4"},
      /* expected_cache_count */ 9,
      {{"main", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div1", kNotMatchedAndSomeChildrenChecked, kCached},
       {"#div11", kMatched, kCached},
       {"#div12", kNotMatched, kCached},
       {"#div13",
        kNotMatchedAndAllDescendantsOrNextSiblingsCheckedAndSomeChildrenChecked,
        kCached},
       {"#div131", kNotCached, kAlreadyNotMatched},
       {"#div132", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div14", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div141", kNotCached, kAlreadyNotMatched},
       {"#div15", kNotCached, kAlreadyNotMatched},
       {"#div2", kNotMatchedAndAllDescendantsOrNextSiblingsChecked, kCached},
       {"#div21", kNotCached, kAlreadyNotMatched},
       {"#div3", kNotCached, kAlreadyNotMatched},
       {"#div31", kNotCached, kAlreadyNotMatched},
       {"#div4", kMatched, kCached},
       {"#div41", kNotCached, kAlreadyNotMatched},
       {"#div5", kNotCached, kAlreadyNotMatched},
       {"#div51", kNotCached, kAlreadyNotMatched},
       {"#div6", kNotCached, kAlreadyNotMatched},
       {"#div61", kNotCached, kAlreadyNotMatched}});
}

}  // namespace blink
