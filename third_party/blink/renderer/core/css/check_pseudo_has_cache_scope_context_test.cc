// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/check_pseudo_has_argument_context.h"
#include "third_party/blink/renderer/core/css/check_pseudo_has_cache_scope.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_selector.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class CheckPseudoHasCacheScopeContextTest : public PageTestBase {
 protected:
  enum CachedCheckPseudoHasResult {
    kNotCached = CheckPseudoHasResult::kNotCached,
    kNotMatched = CheckPseudoHasResult::kChecked,
    kMatched = CheckPseudoHasResult::kChecked | CheckPseudoHasResult::kMatched,
    kNotMatchedAndAllDescendantsOrNextSiblingsChecked =
        CheckPseudoHasResult::kChecked |
        CheckPseudoHasResult::kAllDescendantsOrNextSiblingsChecked,
    kMatchedAndAllDescendantsOrNextSiblingsChecked =
        CheckPseudoHasResult::kChecked | CheckPseudoHasResult::kMatched |
        CheckPseudoHasResult::kAllDescendantsOrNextSiblingsChecked,
    kNotCheckedAndSomeChildrenChecked =
        CheckPseudoHasResult::kSomeChildrenChecked,
    kNotMatchedAndSomeChildrenChecked =
        CheckPseudoHasResult::kChecked |
        CheckPseudoHasResult::kSomeChildrenChecked,
    kMatchedAndSomeChildrenChecked = CheckPseudoHasResult::kChecked |
                                     CheckPseudoHasResult::kMatched |
                                     CheckPseudoHasResult::kSomeChildrenChecked,
    kNotMatchedAndAllDescendantsOrNextSiblingsCheckedAndSomeChildrenChecked =
        CheckPseudoHasResult::kChecked |
        CheckPseudoHasResult::kAllDescendantsOrNextSiblingsChecked |
        CheckPseudoHasResult::kSomeChildrenChecked,
    kMatchedAndAllDescendantsOrNextSiblingsCheckedAndSomeChildrenChecked =
        CheckPseudoHasResult::kChecked | CheckPseudoHasResult::kMatched |
        CheckPseudoHasResult::kAllDescendantsOrNextSiblingsChecked |
        CheckPseudoHasResult::kSomeChildrenChecked,
  };

  enum ExpectedCheckPseudoHasResult {
    kSameAsCached,
    kNotYetChecked,
    kAlreadyNotMatched,
  };

  struct ExpectedResultCacheEntry {
    const char* element_query;
    CachedCheckPseudoHasResult cached_result;
    ExpectedCheckPseudoHasResult expected_result;
  };

  static uint8_t GetResult(
      CheckPseudoHasCacheScope::Context& cache_scope_context,
      Element* element) {
    return cache_scope_context.CacheAllowed()
               ? cache_scope_context.GetResult(element)
               : CheckPseudoHasResult::kNotCached;
  }

  static bool ElementCached(
      CheckPseudoHasCacheScope::Context& cache_scope_context,
      Element* element) {
    return GetResult(cache_scope_context, element) !=
           CheckPseudoHasResult::kNotCached;
  }

  static bool ElementChecked(
      CheckPseudoHasCacheScope::Context& cache_scope_context,
      Element* element) {
    return GetResult(cache_scope_context, element) &
           CheckPseudoHasResult::kChecked;
  }

  static String TestResultToString(uint8_t test_result) {
    return String::Format(
        "0b%c%c%c%c",
        (test_result & CheckPseudoHasResult::kSomeChildrenChecked ? '1' : '0'),
        (test_result &
                 CheckPseudoHasResult::kAllDescendantsOrNextSiblingsChecked
             ? '1'
             : '0'),
        (test_result & CheckPseudoHasResult::kMatched ? '1' : '0'),
        (test_result & CheckPseudoHasResult::kChecked ? '1' : '0'));
  }

  template <unsigned length>
  void CheckCacheResults(
      Document* document,
      String query_name,
      const char* selector_text,
      unsigned expected_result_cache_count,
      const ExpectedResultCacheEntry (&expected_result_cache_entries)[length],
      unsigned expected_fast_reject_filter_cache_count,
      unsigned expected_bloom_filter_allocation_count) const {
    CSSSelectorVector selector_vector = CSSParser::ParseSelector(
        MakeGarbageCollected<CSSParserContext>(
            *document, NullURL(), true /* origin_clean */, Referrer(),
            WTF::TextEncoding(), CSSParserContext::kSnapshotProfile),
        nullptr, selector_text);
    CSSSelectorList selector_list =
        CSSSelectorList::AdoptSelectorVector(selector_vector);
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

    CheckPseudoHasArgumentContext argument_context(argument_selector);
    CheckPseudoHasCacheScope::Context cache_scope_context(document,
                                                          argument_context);

    EXPECT_EQ(expected_result_cache_count,
              cache_scope_context.GetResultCacheCountForTesting())
        << "Failed : " << query_name;

    for (ExpectedResultCacheEntry expected_result_cache_entry :
         expected_result_cache_entries) {
      String test_name =
          String::Format("[%s] cache result of %s", query_name.Utf8().c_str(),
                         expected_result_cache_entry.element_query);
      Element* element =
          document->QuerySelector(expected_result_cache_entry.element_query);
      DCHECK(element) << "Failed to get `"
                      << expected_result_cache_entry.element_query << "'";

      EXPECT_EQ(expected_result_cache_entry.cached_result,
                GetResult(cache_scope_context, element))
          << "Failed : " << test_name << " : { expected: "
          << TestResultToString(expected_result_cache_entry.cached_result)
          << ", actual: "
          << TestResultToString(GetResult(cache_scope_context, element))
          << " }";

      switch (expected_result_cache_entry.expected_result) {
        case kSameAsCached:
          EXPECT_TRUE(ElementCached(cache_scope_context, element))
              << "Failed : " << test_name;
          break;
        case kNotYetChecked:
        case kAlreadyNotMatched:
          EXPECT_FALSE(ElementChecked(cache_scope_context, element))
              << "Failed : " << test_name;
          EXPECT_EQ(
              expected_result_cache_entry.expected_result == kAlreadyNotMatched,
              cache_scope_context.AlreadyChecked(element))
              << "Failed : " << test_name;
          break;
      }
    }

    EXPECT_EQ(expected_fast_reject_filter_cache_count,
              cache_scope_context.GetFastRejectFilterCacheCountForTesting())
        << "Failed : " << query_name;

    EXPECT_EQ(expected_bloom_filter_allocation_count,
              cache_scope_context.GetBloomFilterAllocationCountForTesting())
        << "Failed : " << query_name;
  }

  template <unsigned cache_size>
  void TestMatches(Document* document,
                   const char* query_scope_element_id,
                   const char* selector_text,
                   bool expected_match_result,
                   unsigned expected_result_cache_count,
                   const ExpectedResultCacheEntry (
                       &expected_result_cache_entries)[cache_size],
                   unsigned expected_fast_reject_filter_cache_count,
                   unsigned expected_bloom_filter_allocation_count) const {
    Element* query_scope_element =
        document->getElementById(query_scope_element_id);
    ASSERT_TRUE(query_scope_element);

    CheckPseudoHasCacheScope cache_scope(document);

    String query_name = String::Format("#%s.matches('%s')",
                                       query_scope_element_id, selector_text);

    EXPECT_EQ(expected_match_result,
              query_scope_element->matches(selector_text))
        << "Failed : " << query_name;

    CheckCacheResults(
        document, query_name, selector_text, expected_result_cache_count,
        expected_result_cache_entries, expected_fast_reject_filter_cache_count,
        expected_bloom_filter_allocation_count);
  }

  template <unsigned query_result_size, unsigned cache_size>
  void TestQuerySelectorAll(
      Document* document,
      const char* query_scope_element_id,
      const char* selector_text,
      const String (&expected_results)[query_result_size],
      unsigned expected_result_cache_count,
      const ExpectedResultCacheEntry (
          &expected_result_cache_entries)[cache_size],
      unsigned expected_fast_reject_filter_cache_count,
      unsigned expected_bloom_filter_allocation_count) const {
    Element* query_scope_element =
        document->getElementById(query_scope_element_id);
    ASSERT_TRUE(query_scope_element);

    CheckPseudoHasCacheScope cache_scope(document);

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

    CheckCacheResults(
        document, query_name, selector_text, expected_result_cache_count,
        expected_result_cache_entries, expected_fast_reject_filter_cache_count,
        expected_bloom_filter_allocation_count);
  }
};

TEST_F(CheckPseudoHasCacheScopeContextTest,
       Case1StartsWithDescendantCombinator) {
  // CheckPseudoHasArgumentTraversalScope::kSubtree

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

  TestMatches(document, "div2", ":has(.a)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 7,
              {{"main", kMatched, kSameAsCached},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div2", kMatchedAndSomeChildrenChecked, kSameAsCached},
               {"#div21", kNotCached, kNotYetChecked},
               {"#div211", kNotCached, kNotYetChecked},
               {"#div22", kMatchedAndSomeChildrenChecked, kSameAsCached},
               {"#div221", kNotCached, kNotYetChecked},
               {"#div222", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div2221", kNotCached, kAlreadyNotMatched},
               {"#div223", kNotCached, kAlreadyNotMatched},
               {"#div2231", kNotCached, kAlreadyNotMatched},
               {"#div2232", kNotCached, kAlreadyNotMatched},
               {"#div22321", kNotCached, kAlreadyNotMatched},
               {"#div22322", kNotCached, kAlreadyNotMatched},
               {"#div223221", kNotCached, kAlreadyNotMatched},
               {"#div22323", kNotCached, kAlreadyNotMatched},
               {"#div23", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div231", kNotCached, kAlreadyNotMatched},
               {"#div24", kNotCached, kAlreadyNotMatched},
               {"#div241", kNotCached, kAlreadyNotMatched},
               {"#div3", kNotCached, kNotYetChecked},
               {"#div31", kNotCached, kNotYetChecked},
               {"#div4", kNotCached, kNotYetChecked},
               {"#div41", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div2", ":has(.b)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 9,
              {{"main", kMatched, kSameAsCached},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div2", kMatchedAndSomeChildrenChecked, kSameAsCached},
               {"#div21", kNotCached, kNotYetChecked},
               {"#div211", kNotCached, kNotYetChecked},
               {"#div22", kMatched, kSameAsCached},
               {"#div221", kNotCached, kNotYetChecked},
               {"#div222", kNotCached, kNotYetChecked},
               {"#div2221", kNotCached, kNotYetChecked},
               {"#div223", kMatched, kSameAsCached},
               {"#div2231", kNotCached, kNotYetChecked},
               {"#div2232", kMatchedAndSomeChildrenChecked, kSameAsCached},
               {"#div22321", kNotCached, kNotYetChecked},
               {"#div22322", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div223221", kNotCached, kAlreadyNotMatched},
               {"#div22323", kNotCached, kAlreadyNotMatched},
               {"#div23", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div231", kNotCached, kAlreadyNotMatched},
               {"#div24", kNotCached, kAlreadyNotMatched},
               {"#div241", kNotCached, kAlreadyNotMatched},
               {"#div3", kNotCached, kNotYetChecked},
               {"#div31", kNotCached, kNotYetChecked},
               {"#div4", kNotCached, kNotYetChecked},
               {"#div41", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div2", ":has(.c)",
              /* expected_match_result */ false,
              /* expected_result_cache_count */ 2,
              {{"main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div2", kNotMatchedAndSomeChildrenChecked, kSameAsCached},
               {"#div21", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
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
               {"#div41", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest, Case1StartsWithChildCombinator) {
  // CheckPseudoHasArgumentTraversalScope::kSubtree

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

  TestMatches(document, "div22", ":has(> .a .b)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 5,
              {{"main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div2", kNotCached, kNotYetChecked},
               {"#div21", kNotCached, kNotYetChecked},
               {"#div211", kNotCached, kNotYetChecked},
               {"#div22", kMatchedAndSomeChildrenChecked, kSameAsCached},
               {"#div221", kNotCached, kNotYetChecked},
               {"#div2211", kNotCached, kNotYetChecked},
               {"#div222", kNotCached, kNotYetChecked},
               {"#div2221", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div22211", kNotCached, kNotYetChecked},
               {"#div22212", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div222121", kNotCached, kAlreadyNotMatched},
               {"#div22213", kNotCached, kAlreadyNotMatched},
               {"#div223", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div2231", kNotCached, kAlreadyNotMatched},
               {"#div224", kMatched, kSameAsCached},
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
               {"#div31", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div2", ":has(> .a .b)",
              /* expected_match_result */ false,
              /* expected_result_cache_count */ 4,
              {{"main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div2", kNotMatchedAndSomeChildrenChecked, kSameAsCached},
               {"#div21", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div211", kNotCached, kAlreadyNotMatched},
               {"#div22", kMatched, kSameAsCached},
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
               {"#div224", kMatched, kSameAsCached},
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
               {"#div31", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div2", ":has(> .a .c)",
              /* expected_match_result */ false,
              /* expected_result_cache_count */ 2,
              {{"main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div2", kNotMatchedAndSomeChildrenChecked, kSameAsCached},
               {"#div21", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
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
               {"#div31", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest, Case2StartsWithIndirectAdjacent) {
  // CheckPseudoHasArgumentTraversalScope::kAllNextSiblings

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

  TestMatches(document, "div22", ":has(~ .a)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 5,
              {{"main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div2", kNotCheckedAndSomeChildrenChecked},
               {"#div21", kMatched, kSameAsCached},
               {"#div211", kNotCached, kNotYetChecked},
               {"#div212", kNotCached, kNotYetChecked},
               {"#div22", kMatched, kSameAsCached},
               {"#div221", kNotCached, kNotYetChecked},
               {"#div222", kNotCached, kNotYetChecked},
               {"#div23", kMatched, kSameAsCached},
               {"#div231", kNotCached, kNotYetChecked},
               {"#div232", kNotCached, kNotYetChecked},
               {"#div24", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div241", kNotCached, kNotYetChecked},
               {"#div242", kNotCached, kNotYetChecked},
               {"#div25", kNotCached, kAlreadyNotMatched},
               {"#div251", kNotCached, kNotYetChecked},
               {"#div252", kNotCached, kNotYetChecked},
               {"#div3", kNotCached, kNotYetChecked},
               {"#div31", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div22", ":has(~ .b)",
              /* expected_match_result */ false,
              /* expected_result_cache_count */ 3,
              {{"main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div2", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div21", kNotCached, kNotYetChecked},
               {"#div211", kNotCached, kNotYetChecked},
               {"#div212", kNotCached, kNotYetChecked},
               {"#div22", kNotMatched, kSameAsCached},
               {"#div221", kNotCached, kNotYetChecked},
               {"#div222", kNotCached, kNotYetChecked},
               {"#div23", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div231", kNotCached, kNotYetChecked},
               {"#div232", kNotCached, kNotYetChecked},
               {"#div24", kNotCached, kAlreadyNotMatched},
               {"#div241", kNotCached, kNotYetChecked},
               {"#div242", kNotCached, kNotYetChecked},
               {"#div25", kNotCached, kAlreadyNotMatched},
               {"#div251", kNotCached, kNotYetChecked},
               {"#div252", kNotCached, kNotYetChecked},
               {"#div3", kNotCached, kNotYetChecked},
               {"#div31", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest, Case2StartsWithDirectAdjacent) {
  // CheckPseudoHasArgumentTraversalScope::kAllNextSiblings

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

  TestMatches(document, "div23", ":has(+ .a ~ .b)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 3,
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
               {"#div23", kMatched, kSameAsCached},
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
               {"#div26", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
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
               {"#div41", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div22", ":has(+ .a ~ .b)",
              /* expected_match_result */ false,
              /* expected_result_cache_count */ 3,
              {{"main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div2", kNotCheckedAndSomeChildrenChecked},
               {"#div21", kNotCached, kNotYetChecked},
               {"#div211", kNotCached, kNotYetChecked},
               {"#div212", kNotCached, kNotYetChecked},
               {"#div213", kNotCached, kNotYetChecked},
               {"#div22", kNotMatched, kSameAsCached},
               {"#div221", kNotCached, kNotYetChecked},
               {"#div222", kNotCached, kNotYetChecked},
               {"#div223", kNotCached, kNotYetChecked},
               {"#div23", kMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
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
               {"#div41", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div22", ":has(+ .a ~ .c)",
              /* expected_match_result */ false,
              /* expected_result_cache_count */ 3,
              {{"main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div2", kNotCheckedAndSomeChildrenChecked},
               {"#div21", kNotCached, kNotYetChecked},
               {"#div211", kNotCached, kNotYetChecked},
               {"#div212", kNotCached, kNotYetChecked},
               {"#div213", kNotCached, kNotYetChecked},
               {"#div22", kNotMatched, kSameAsCached},
               {"#div221", kNotCached, kNotYetChecked},
               {"#div222", kNotCached, kNotYetChecked},
               {"#div223", kNotCached, kNotYetChecked},
               {"#div23", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
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
               {"#div41", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest, Case3) {
  // CheckPseudoHasArgumentTraversalScope::kOneNextSiblingSubtree

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

  TestMatches(document, "div22", ":has(+ .a .b)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 10,
              {{"main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div2", kNotCached, kNotYetChecked},
               {"#div21", kNotCached, kNotYetChecked},
               {"#div22", kMatched, kSameAsCached},
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
               {"#div2342", kMatched, kSameAsCached},
               {"#div2343", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div23431", kNotCached, kNotYetChecked},
               {"#div23432", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div234321", kNotCached, kNotYetChecked},
               {"#div234322", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div2343221", kNotCached, kAlreadyNotMatched},
               {"#div234323", kNotCached, kAlreadyNotMatched},
               {"#div23433", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div234331", kNotCached, kAlreadyNotMatched},
               {"#div23434", kNotCached, kAlreadyNotMatched},
               {"#div2344", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div23441", kNotCached, kAlreadyNotMatched},
               {"#div2345", kNotCached, kAlreadyNotMatched},
               {"#div235", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div2351", kNotCached, kAlreadyNotMatched},
               {"#div236", kNotCached, kAlreadyNotMatched},
               {"#div24", kNotCached, kNotYetChecked},
               {"#div241", kNotCached, kNotYetChecked},
               {"#div25", kNotCached, kNotYetChecked},
               {"#div3", kNotCached, kNotYetChecked},
               {"#div4", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div1", ":has(+ .a .b)",
              /* expected_match_result */ false,
              /* expected_result_cache_count */ 5,
              {{"main", kNotCached, kNotYetChecked},
               {"#div1", kNotMatched, kSameAsCached},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div2", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div21", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div22", kMatched, kSameAsCached},
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
               {"#div2342", kMatched, kSameAsCached},
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
               {"#div4", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div22", ":has(+ .a .c)",
              /* expected_match_result */ false,
              /* expected_result_cache_count */ 3,
              {{"main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div2", kNotCached, kNotYetChecked},
               {"#div21", kNotCached, kNotYetChecked},
               {"#div22", kNotMatched, kSameAsCached},
               {"#div23", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div231", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
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
               {"#div4", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest, Case4) {
  // CheckPseudoHasArgumentTraversalScope::kAllNextSiblingSubtrees

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

  TestMatches(document, "div22", ":has(~ .a .b)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 10,
              {{"main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div2", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div21", kMatched, kSameAsCached},
               {"#div22", kMatched, kSameAsCached},
               {"#div23", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div231", kNotCached, kNotYetChecked},
               {"#div232", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div2321", kNotCached, kNotYetChecked},
               {"#div2322", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div23221", kNotCached, kAlreadyNotMatched},
               {"#div2323", kNotCached, kAlreadyNotMatched},
               {"#div233", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div24", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div241", kNotCached, kAlreadyNotMatched},
               {"#div242", kNotCached, kAlreadyNotMatched},
               {"#div2421", kMatched, kSameAsCached},
               {"#div2422", kMatched, kSameAsCached},
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
               {"#div41", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div21", ":has(~ .a .b)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 10,
              {{"main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div2", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div21", kMatched, kSameAsCached},
               {"#div22", kMatched, kSameAsCached},
               {"#div23", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div231", kNotCached, kNotYetChecked},
               {"#div232", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div2321", kNotCached, kNotYetChecked},
               {"#div2322", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div23221", kNotCached, kAlreadyNotMatched},
               {"#div2323", kNotCached, kAlreadyNotMatched},
               {"#div233", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div24", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div241", kNotCached, kAlreadyNotMatched},
               {"#div242", kNotCached, kAlreadyNotMatched},
               {"#div2421", kMatched, kSameAsCached},
               {"#div2422", kMatched, kSameAsCached},
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
               {"#div41", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div1", ":has(~ .a .b)",
              /* expected_match_result */ false,
              /* expected_result_cache_count */ 7,
              {{"main", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div1", kNotMatched, kSameAsCached},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div2", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div21", kMatched, kSameAsCached},
               {"#div22", kMatched, kSameAsCached},
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
               {"#div2421", kMatched, kSameAsCached},
               {"#div2422", kMatched, kSameAsCached},
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
               {"#div41", kNotCached, kAlreadyNotMatched}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div22", ":has(~ .a .c)",
              /* expected_match_result */ false,
              /* expected_result_cache_count */ 3,
              {{"main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div2", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div21", kNotCached, kNotYetChecked},
               {"#div22", kNotMatched, kSameAsCached},
               {"#div23", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
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
               {"#div41", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest,
       QuerySelectorAllCase1StartsWithDescendantCombinator) {
  // CheckPseudoHasArgumentTraversalScope::kSubtree

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

  TestMatches(document, "div1", ":has(.a .b)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 7,
              {{"html", kMatched, kSameAsCached},
               {"body", kMatched, kSameAsCached},
               {"#main", kMatched, kSameAsCached},
               {"#div1", kMatchedAndSomeChildrenChecked, kSameAsCached},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div12", kNotCached, kNotYetChecked},
               {"#div121", kNotCached, kNotYetChecked},
               {"#div1211", kNotCached, kNotYetChecked},
               {"#div12111", kNotCached, kNotYetChecked},
               {"#div13", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div131", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div14", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div11", ":has(.a .b)",
              /* expected_match_result */ false,
              /* expected_result_cache_count */ 1,
              {{"#div11", kNotMatched, kSameAsCached}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div12", ":has(.a .b)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 8,
              {{"html", kMatched, kSameAsCached},
               {"body", kMatched, kSameAsCached},
               {"#main", kMatched, kSameAsCached},
               {"#div1", kMatched, kSameAsCached},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div12", kMatched, kSameAsCached},
               {"#div121", kMatched, kSameAsCached},
               {"#div1211", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div12111", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div13", kNotCached, kNotYetChecked},
               {"#div131", kNotCached, kNotYetChecked},
               {"#div14", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  // ':has(.a .b)' does not match #div1211 but this caches possibly matched
  // elements because argument selector checking can cross over the :has()
  // anchor element.
  TestMatches(document, "div1211", ":has(.a .b)",
              /* expected_match_result */ false,
              /* expected_result_cache_count */ 8,
              {{"html", kMatched, kSameAsCached},
               {"body", kMatched, kSameAsCached},
               {"#main", kMatched, kSameAsCached},
               {"#div1", kMatched, kSameAsCached},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div12", kMatched, kSameAsCached},
               {"#div121", kMatched, kSameAsCached},
               {"#div1211", kNotMatchedAndSomeChildrenChecked, kSameAsCached},
               {"#div12111", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div13", kNotCached, kNotYetChecked},
               {"#div131", kNotCached, kNotYetChecked},
               {"#div14", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  // ':has(.a .b)' does not match #div13 but this caches possibly matched
  // elements because argument selector checking can cross over the :has()
  // anchor element.
  TestMatches(document, "div13", ":has(.a .b)",
              /* expected_match_result */ false,
              /* expected_result_cache_count */ 6,
              {{"html", kMatched, kSameAsCached},
               {"body", kMatched, kSameAsCached},
               {"#main", kMatched, kSameAsCached},
               {"#div1", kMatched, kSameAsCached},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div12", kNotCached, kNotYetChecked},
               {"#div121", kNotCached, kNotYetChecked},
               {"#div1211", kNotCached, kNotYetChecked},
               {"#div12111", kNotCached, kNotYetChecked},
               {"#div13", kNotMatchedAndSomeChildrenChecked, kSameAsCached},
               {"#div131", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div14", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(
      document, "main", ":has(.a .b)", {"div1", "div12", "div121"},
      /* expected_result_cache_count */ 12,
      {{"html", kMatched, kSameAsCached},
       {"body", kMatched, kSameAsCached},
       {"#main", kMatched, kSameAsCached},
       {"#div1", kMatchedAndSomeChildrenChecked, kSameAsCached},
       {"#div11", kNotMatched, kSameAsCached},
       {"#div12", kMatched, kSameAsCached},
       {"#div121", kMatched, kSameAsCached},
       {"#div1211", kNotMatchedAndSomeChildrenChecked, kSameAsCached},
       {"#div12111", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div13", kNotMatchedAndSomeChildrenChecked, kSameAsCached},
       {"#div131", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div14", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached}},
      /* expected_fast_reject_filter_cache_count */ 5,
      /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(
      document, "main", ":has(.a .b), :has(.c .d)", {"div1", "div12", "div121"},
      /* expected_result_cache_count */ 12,
      {{"html", kMatched, kSameAsCached},
       {"body", kMatched, kSameAsCached},
       {"#main", kMatched, kSameAsCached},
       {"#div1", kMatchedAndSomeChildrenChecked, kSameAsCached},
       {"#div11", kNotMatched, kSameAsCached},
       {"#div12", kMatched, kSameAsCached},
       {"#div121", kMatched, kSameAsCached},
       {"#div1211", kNotMatchedAndSomeChildrenChecked, kSameAsCached},
       {"#div12111", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div13", kNotMatchedAndSomeChildrenChecked, kSameAsCached},
       {"#div131", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div14", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached}},
      /* expected_fast_reject_filter_cache_count */ 6,
      /* expected_bloom_filter_allocation_count */ 3);
}

TEST_F(CheckPseudoHasCacheScopeContextTest,
       QuerySelectorAllCase1StartsWithChildCombinator) {
  // CheckPseudoHasArgumentTraversalScope::kSubtree

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
              /* expected_result_cache_count */ 4,
              {{"#div1", kMatched, kSameAsCached},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div111", kMatched, kSameAsCached},
               {"#div1111", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div11111", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div11", ":has(> .a .b)",
              /* expected_match_result */ false,
              /* expected_result_cache_count */ 3,
              {{"#div1", kMatched, kSameAsCached},
               {"#div11", kNotMatchedAndSomeChildrenChecked, kSameAsCached},
               {"#div111", kMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div1111", kNotCached, kAlreadyNotMatched},
               {"#div11111", kNotCached, kAlreadyNotMatched}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(
      document, "main", ":has(> .a .b)", {"div1", "div111"},
      /* expected_result_cache_count */ 5,
      {{"#div1", kMatched, kSameAsCached},
       {"#div11", kNotMatchedAndSomeChildrenChecked, kSameAsCached},
       {"#div111", kMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div1111", kNotMatchedAndSomeChildrenChecked, kSameAsCached},
       {"#div11111", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached}},
      /* expected_fast_reject_filter_cache_count */ 2,
      /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(
      document, "main", ":has(> .a .b), :has(> .c .d)", {"div1", "div111"},
      /* expected_result_cache_count */ 5,
      {{"#div1", kMatched, kSameAsCached},
       {"#div11", kNotMatchedAndSomeChildrenChecked, kSameAsCached},
       {"#div111", kMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div1111", kNotMatchedAndSomeChildrenChecked, kSameAsCached},
       {"#div11111", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached}},
      /* expected_fast_reject_filter_cache_count */ 2,
      /* expected_bloom_filter_allocation_count */ 1);
}

TEST_F(CheckPseudoHasCacheScopeContextTest,
       QuerySelectorAllCase1StartsWithChildCombinatorNonSubjectHas) {
  // CheckPseudoHasArgumentTraversalScope::kSubtree

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
              /* expected_result_cache_count */ 2,
              {{"#div112", kNotMatchedAndSomeChildrenChecked, kSameAsCached},
               {"#div1121", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div111", ":has(> .a .b)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 4,
              {{"#div111", kMatchedAndSomeChildrenChecked, kSameAsCached},
               {"#div1111", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div11111", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div1112", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div11", ":has(> .a .b)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 6,
              {{"#div11", kMatchedAndSomeChildrenChecked, kSameAsCached},
               {"#div111", kMatchedAndSomeChildrenChecked, kSameAsCached},
               {"#div1111", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div11111", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div1112", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div112", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div1121", kNotCached, kAlreadyNotMatched},
               {"#div113", kNotCached, kAlreadyNotMatched},
               {"#div1131", kNotCached, kAlreadyNotMatched}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div1", ":has(> .a .b)",
              /* expected_match_result */ false,
              /* expected_result_cache_count */ 3,
              {{"#div1", kNotMatchedAndSomeChildrenChecked, kSameAsCached},
               {"#div11", kMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div111", kMatched, kSameAsCached},
               {"#div1111", kNotCached, kAlreadyNotMatched},
               {"#div11111", kNotCached, kAlreadyNotMatched},
               {"#div1112", kNotCached, kAlreadyNotMatched},
               {"#div112", kNotCached, kAlreadyNotMatched},
               {"#div1121", kNotCached, kAlreadyNotMatched},
               {"#div113", kNotCached, kAlreadyNotMatched},
               {"#div1131", kNotCached, kAlreadyNotMatched},
               {"#div12", kNotCached, kAlreadyNotMatched},
               {"#div121", kNotCached, kAlreadyNotMatched}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(
      document, "main", ":has(> .a .b) ~ .c .d", {"div1131", "div121"},
      /* expected_result_cache_count */ 8,
      {{"#div1", kNotMatchedAndSomeChildrenChecked, kSameAsCached},
       {"#div11",
        kMatchedAndAllDescendantsOrNextSiblingsCheckedAndSomeChildrenChecked,
        kSameAsCached},
       {"#div111", kMatchedAndSomeChildrenChecked, kSameAsCached},
       {"#div1111", kNotCheckedAndSomeChildrenChecked, kAlreadyNotMatched},
       {"#div11111", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div1112", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div112",
        kNotMatchedAndAllDescendantsOrNextSiblingsCheckedAndSomeChildrenChecked,
        kSameAsCached},
       {"#div1121", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div113", kNotCached, kAlreadyNotMatched},
       {"#div1131", kNotCached, kAlreadyNotMatched},
       {"#div12", kNotCached, kAlreadyNotMatched},
       {"#div121", kNotCached, kAlreadyNotMatched},
       {"#div2", kNotCached, kNotYetChecked},
       {"#div21", kNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 4,
      /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest,
       QuerySelectorAllCase2NonSubjectHas) {
  // CheckPseudoHasArgumentTraversalScope::kAllNextSiblings

  auto* document = HTMLDocument::CreateForTest();
  document->write(R"HTML(
    <!DOCTYPE html>
    <main id=main>
      <div id=div1>
        <div id=div11 class=a>
          <div id=div111>
            <div id=div1111 class=b></div>
          </div>
          <div id=div112 class=a></div>
        </div>
        <div id=div12>
          <div id=div121>
            <div id=div1211 class=b></div>
          </div>
          <div id=div122></div>
        </div>
        <div id=div13></div>
      </div>
      <div id=div2 class=a></div>
    </main>
  )HTML");

  TestMatches(document, "div1111", ":has(~ .a) .b",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 3,
              {{"main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div111", kMatched, kSameAsCached},
               {"#div1111", kNotCached, kNotYetChecked},
               {"#div112", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div12", kNotCached, kNotYetChecked},
               {"#div121", kNotCached, kNotYetChecked},
               {"#div1211", kNotCached, kNotYetChecked},
               {"#div122", kNotCached, kNotYetChecked},
               {"#div13", kNotCached, kNotYetChecked},
               {"#div2", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div1211", ":has(~ .a) .b",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 7,
              {{"main", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div1", kMatchedAndSomeChildrenChecked, kSameAsCached},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div111", kNotCached, kNotYetChecked},
               {"#div1111", kNotCached, kNotYetChecked},
               {"#div112", kNotCached, kNotYetChecked},
               {"#div12", kNotMatchedAndSomeChildrenChecked, kSameAsCached},
               {"#div121", kNotMatched, kSameAsCached},
               {"#div1211", kNotCached, kNotYetChecked},
               {"#div122", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div13", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div2", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached}},
              /* expected_fast_reject_filter_cache_count */ 3,
              /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(
      document, "main", ":has(~ .a) .b", {"div1111", "div1211"},
      /* expected_result_cache_count */ 10,
      {{"main", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div1", kMatchedAndSomeChildrenChecked, kSameAsCached},
       {"#div11", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div111", kMatched, kSameAsCached},
       {"#div1111", kNotCached, kNotYetChecked},
       {"#div112", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div12", kNotMatchedAndSomeChildrenChecked, kSameAsCached},
       {"#div121", kNotMatched, kSameAsCached},
       {"#div1211", kNotCached, kNotYetChecked},
       {"#div122", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div13", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div2", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached}},
      /* expected_fast_reject_filter_cache_count */ 4,
      /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest,
       QuerySelectorAllCase3NonSubjectHas) {
  // CheckPseudoHasArgumentTraversalScope::kOneNextSiblingSubtree

  auto* document = HTMLDocument::CreateForTest();
  document->write(R"HTML(
    <!DOCTYPE html>
    <main id=main>
      <div id=div1>
        <div id=div11 class=c></div>
      </div>
      <div id=div2 class=a>
        <div id=div21>
          <div id=div211 class=c></div>
        </div>
        <div id=div22 class=a>
          <div id=div221 class=b></div>
        </div>
        <div id=div23>
          <div id=div231 class=b></div>
        </div>
      </div>
    </main>
  )HTML");

  TestMatches(document, "div11", ":has(+ .a .b) .c",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 3,
              {{"main", kNotCached, kNotYetChecked},
               {"#div1", kMatched, kSameAsCached},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div2", kNotCached, kNotYetChecked},
               {"#div21", kNotCached, kNotYetChecked},
               {"#div211", kNotCached, kNotYetChecked},
               {"#div22", kNotCached, kNotYetChecked},
               {"#div221", kNotCached, kNotYetChecked},
               {"#div23", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div231", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div211", ":has(+ .a .b) .c",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 3,
              {{"main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div2", kNotCached, kNotYetChecked},
               {"#div21", kMatched, kSameAsCached},
               {"#div211", kNotCached, kNotYetChecked},
               {"#div22", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div221", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div23", kNotCached, kNotYetChecked},
               {"#div231", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(
      document, "main", ":has(+ .a .b) .c", {"div11", "div211"},
      /* expected_result_cache_count */ 6,
      {{"main", kNotCached, kNotYetChecked},
       {"#div1", kMatched, kSameAsCached},
       {"#div11", kNotCached, kNotYetChecked},
       {"#div2", kNotCached, kNotYetChecked},
       {"#div21", kMatched, kSameAsCached},
       {"#div211", kNotCached, kNotYetChecked},
       {"#div22", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div221", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div23", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div231", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached}},
      /* expected_fast_reject_filter_cache_count */ 2,
      /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest,
       QuerySelectorAllCase4NonSubjectHas) {
  // CheckPseudoHasArgumentTraversalScope::kAllNextSiblingSubtrees

  auto* document = HTMLDocument::CreateForTest();
  document->write(R"HTML(
    <!DOCTYPE html>
    <main id=main>
      <div id=div1>
        <div id=div11 class=c></div>
      </div>
      <div id=div2 class=a>
        <div id=div21>
          <div id=div211>
            <div id=div2111 class=c></div>
          </div>
          <div id=div212 class=a>
            <div id=div2121 class=b></div>
          </div>
        </div>
        <div id=div22>
          <div id=div221 class=b></div>
        </div>
      </div>
    </main>
  )HTML");

  TestMatches(document, "div11", ":has(~ .a .b) .c",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 3,
              {{"main", kNotCached, kNotYetChecked},
               {"#div1", kMatched, kSameAsCached},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div2", kNotCached, kNotYetChecked},
               {"#div21", kNotCached, kNotYetChecked},
               {"#div211", kNotCached, kNotYetChecked},
               {"#div2111", kNotCached, kNotYetChecked},
               {"#div212", kNotCached, kNotYetChecked},
               {"#div2121", kNotCached, kNotYetChecked},
               {"#div22", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div221", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div2111", ":has(~ .a .b) .c",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 3,
              {{"main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div2", kNotCached, kNotYetChecked},
               {"#div21", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div211", kMatched, kSameAsCached},
               {"#div2111", kNotCached, kNotYetChecked},
               {"#div212", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div2121", kNotCached, kAlreadyNotMatched},
               {"#div22", kNotCached, kNotYetChecked},
               {"#div221", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(
      document, "main", ":has(~ .a .b) .c", {"div11", "div2111"},
      /* expected_result_cache_count */ 6,
      {{"main", kNotCached, kNotYetChecked},
       {"#div1", kMatched, kSameAsCached},
       {"#div11", kNotCached, kNotYetChecked},
       {"#div2", kNotCached, kNotYetChecked},
       {"#div21", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div211", kMatched, kSameAsCached},
       {"#div2111", kNotCached, kNotYetChecked},
       {"#div212", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div2121", kNotCached, kAlreadyNotMatched},
       {"#div22", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div221", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached}},
      /* expected_fast_reject_filter_cache_count */ 2,
      /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest,
       QuerySelectorAllCase4StartsWithDirectAdjacentCombinator) {
  // CheckPseudoHasArgumentTraversalScope::kAllNextSiblingSubtrees

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

  TestMatches(document, "div1", ":has(+ .a ~ .b .c)",
              /* expected_match_result */ false,
              /* expected_result_cache_count */ 4,
              {{"main", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div1", kNotMatched, kSameAsCached},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div12", kNotCached, kNotYetChecked},
               {"#div13", kNotCached, kNotYetChecked},
               {"#div131", kNotCached, kNotYetChecked},
               {"#div132", kNotCached, kNotYetChecked},
               {"#div14", kNotCached, kNotYetChecked},
               {"#div141", kNotCached, kNotYetChecked},
               {"#div15", kNotCached, kNotYetChecked},
               {"#div2", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div21", kNotCached, kAlreadyNotMatched},
               {"#div3", kNotCached, kAlreadyNotMatched},
               {"#div31", kNotCached, kAlreadyNotMatched},
               {"#div4", kMatched, kSameAsCached},
               {"#div41", kNotCached, kAlreadyNotMatched},
               {"#div5", kNotCached, kAlreadyNotMatched},
               {"#div51", kNotCached, kAlreadyNotMatched},
               {"#div6", kNotCached, kAlreadyNotMatched},
               {"#div61", kNotCached, kAlreadyNotMatched}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div11", ":has(+ .a ~ .b .c)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 5,
              {{"#div1", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div11", kMatched, kSameAsCached},
               {"#div12", kNotCached, kNotYetChecked},
               {"#div13", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div131", kNotCached, kNotYetChecked},
               {"#div132", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div14", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div141", kNotCached, kAlreadyNotMatched},
               {"#div15", kNotCached, kAlreadyNotMatched}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div12", ":has(+ .a ~ .b .c)",
              /* expected_match_result */ false,
              /* expected_result_cache_count */ 4,
              {{"#div1", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
               {"#div11", kMatched, kSameAsCached},
               {"#div12", kNotMatched, kSameAsCached},
               {"#div13", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div131", kNotCached, kAlreadyNotMatched},
               {"#div132", kNotCached, kAlreadyNotMatched},
               {"#div14", kNotCached, kAlreadyNotMatched},
               {"#div141", kNotCached, kAlreadyNotMatched},
               {"#div15", kNotCached, kAlreadyNotMatched}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(
      document, "main", ":has(+ .a ~ .b .c)", {"div11", "div4"},
      /* expected_result_cache_count */ 9,
      {{"main", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div1", kNotMatchedAndSomeChildrenChecked, kSameAsCached},
       {"#div11", kMatched, kSameAsCached},
       {"#div12", kNotMatched, kSameAsCached},
       {"#div13",
        kNotMatchedAndAllDescendantsOrNextSiblingsCheckedAndSomeChildrenChecked,
        kSameAsCached},
       {"#div131", kNotCached, kAlreadyNotMatched},
       {"#div132", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div14", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div141", kNotCached, kAlreadyNotMatched},
       {"#div15", kNotCached, kAlreadyNotMatched},
       {"#div2", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div21", kNotCached, kAlreadyNotMatched},
       {"#div3", kNotCached, kAlreadyNotMatched},
       {"#div31", kNotCached, kAlreadyNotMatched},
       {"#div4", kMatched, kSameAsCached},
       {"#div41", kNotCached, kAlreadyNotMatched},
       {"#div5", kNotCached, kAlreadyNotMatched},
       {"#div51", kNotCached, kAlreadyNotMatched},
       {"#div6", kNotCached, kAlreadyNotMatched},
       {"#div61", kNotCached, kAlreadyNotMatched}},
      /* expected_fast_reject_filter_cache_count */ 3,
      /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(
      document, "main", ":has(+ .a ~ .b .c), :has(+ .d ~ .e .f)",
      {"div11", "div4"}, /* expected_result_cache_count */ 9,
      {{"main", kNotCheckedAndSomeChildrenChecked, kNotYetChecked},
       {"#div1", kNotMatchedAndSomeChildrenChecked, kSameAsCached},
       {"#div11", kMatched, kSameAsCached},
       {"#div12", kNotMatched, kSameAsCached},
       {"#div13",
        kNotMatchedAndAllDescendantsOrNextSiblingsCheckedAndSomeChildrenChecked,
        kSameAsCached},
       {"#div131", kNotCached, kAlreadyNotMatched},
       {"#div132", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div14", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div141", kNotCached, kAlreadyNotMatched},
       {"#div15", kNotCached, kAlreadyNotMatched},
       {"#div2", kNotMatchedAndAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div21", kNotCached, kAlreadyNotMatched},
       {"#div3", kNotCached, kAlreadyNotMatched},
       {"#div31", kNotCached, kAlreadyNotMatched},
       {"#div4", kMatched, kSameAsCached},
       {"#div41", kNotCached, kAlreadyNotMatched},
       {"#div5", kNotCached, kAlreadyNotMatched},
       {"#div51", kNotCached, kAlreadyNotMatched},
       {"#div6", kNotCached, kAlreadyNotMatched},
       {"#div61", kNotCached, kAlreadyNotMatched}},
      /* expected_fast_reject_filter_cache_count */ 3,
      /* expected_bloom_filter_allocation_count */ 2);
}

TEST_F(CheckPseudoHasCacheScopeContextTest, QuerySelectorAllCase5) {
  // CheckPseudoHasArgumentTraversalScope::kOneNextSibling

  auto* document = HTMLDocument::CreateForTest();
  document->write(R"HTML(
    <!DOCTYPE html>
    <main id=main>
      <div id=div1>
        <div id=div11></div>
        <div id=div12></div>
        <div id=div13></div>
      </div>
      <div id=div2>
        <div id=div21></div>
        <div id=div22 class=a></div>
        <div id=div23></div>
      </div>
      <div id=div3 class=a>
        <div id=div31></div>
        <div id=div32></div>
        <div id=div33></div>
      </div>
    </main>
  )HTML");

  TestMatches(document, "div2", ":has(+ .a)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 0,
              {{"#main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div12", kNotCached, kNotYetChecked},
               {"#div13", kNotCached, kNotYetChecked},
               {"#div2", kNotCached, kNotYetChecked},
               {"#div21", kNotCached, kNotYetChecked},
               {"#div22", kNotCached, kNotYetChecked},
               {"#div23", kNotCached, kNotYetChecked},
               {"#div3", kNotCached, kNotYetChecked},
               {"#div31", kNotCached, kNotYetChecked},
               {"#div32", kNotCached, kNotYetChecked},
               {"#div33", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 0,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div21", ":has(+ .a)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 0,
              {{"#main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div12", kNotCached, kNotYetChecked},
               {"#div13", kNotCached, kNotYetChecked},
               {"#div2", kNotCached, kNotYetChecked},
               {"#div21", kNotCached, kNotYetChecked},
               {"#div22", kNotCached, kNotYetChecked},
               {"#div23", kNotCached, kNotYetChecked},
               {"#div3", kNotCached, kNotYetChecked},
               {"#div31", kNotCached, kNotYetChecked},
               {"#div32", kNotCached, kNotYetChecked},
               {"#div33", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 0,
              /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(document, "main", ":has(+ .a)", {"div2", "div21"},
                       /* expected_result_cache_count */ 0,
                       {{"#main", kNotCached, kNotYetChecked},
                        {"#div1", kNotCached, kNotYetChecked},
                        {"#div11", kNotCached, kNotYetChecked},
                        {"#div12", kNotCached, kNotYetChecked},
                        {"#div13", kNotCached, kNotYetChecked},
                        {"#div2", kNotCached, kNotYetChecked},
                        {"#div21", kNotCached, kNotYetChecked},
                        {"#div22", kNotCached, kNotYetChecked},
                        {"#div23", kNotCached, kNotYetChecked},
                        {"#div3", kNotCached, kNotYetChecked},
                        {"#div31", kNotCached, kNotYetChecked},
                        {"#div32", kNotCached, kNotYetChecked},
                        {"#div33", kNotCached, kNotYetChecked}},
                       /* expected_fast_reject_filter_cache_count */ 0,
                       /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest, QuerySelectorAllCase6) {
  // CheckPseudoHasArgumentTraversalScope::kFixedDepthDescendants

  auto* document = HTMLDocument::CreateForTest();
  document->write(R"HTML(
    <!DOCTYPE html>
    <main id=main>
      <div id=div1>
        <div id=div11 class=a>
          <div id=div111></div>
          <div id=div112>
            <div id=div1121></div>
            <div id=div1122 class=a></div>
            <div id=div1123></div>
          </div>
          <div id=div113></div>
        </div>
        <div id=div12>
          <div id=div121></div>
          <div id=div122 class=a></div>
          <div id=div123></div>
        </div>
      </div>
    </main>
  )HTML");

  TestMatches(document, "div1", ":has(> .a)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 0,
              {{"#main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div111", kNotCached, kNotYetChecked},
               {"#div112", kNotCached, kNotYetChecked},
               {"#div1121", kNotCached, kNotYetChecked},
               {"#div1122", kNotCached, kNotYetChecked},
               {"#div1123", kNotCached, kNotYetChecked},
               {"#div113", kNotCached, kNotYetChecked},
               {"#div12", kNotCached, kNotYetChecked},
               {"#div121", kNotCached, kNotYetChecked},
               {"#div122", kNotCached, kNotYetChecked},
               {"#div123", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 0,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div112", ":has(> .a)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 0,
              {{"#main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div111", kNotCached, kNotYetChecked},
               {"#div112", kNotCached, kNotYetChecked},
               {"#div1121", kNotCached, kNotYetChecked},
               {"#div1122", kNotCached, kNotYetChecked},
               {"#div1123", kNotCached, kNotYetChecked},
               {"#div113", kNotCached, kNotYetChecked},
               {"#div12", kNotCached, kNotYetChecked},
               {"#div121", kNotCached, kNotYetChecked},
               {"#div122", kNotCached, kNotYetChecked},
               {"#div123", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 0,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div12", ":has(> .a)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 0,
              {{"#main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div111", kNotCached, kNotYetChecked},
               {"#div112", kNotCached, kNotYetChecked},
               {"#div1121", kNotCached, kNotYetChecked},
               {"#div1122", kNotCached, kNotYetChecked},
               {"#div1123", kNotCached, kNotYetChecked},
               {"#div113", kNotCached, kNotYetChecked},
               {"#div12", kNotCached, kNotYetChecked},
               {"#div121", kNotCached, kNotYetChecked},
               {"#div122", kNotCached, kNotYetChecked},
               {"#div123", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 0,
              /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(document, "main", ":has(> .a)",
                       {"div1", "div112", "div12"},
                       /* expected_result_cache_count */ 0,
                       {{"#main", kNotCached, kNotYetChecked},
                        {"#div1", kNotCached, kNotYetChecked},
                        {"#div11", kNotCached, kNotYetChecked},
                        {"#div111", kNotCached, kNotYetChecked},
                        {"#div112", kNotCached, kNotYetChecked},
                        {"#div1121", kNotCached, kNotYetChecked},
                        {"#div1122", kNotCached, kNotYetChecked},
                        {"#div1123", kNotCached, kNotYetChecked},
                        {"#div113", kNotCached, kNotYetChecked},
                        {"#div12", kNotCached, kNotYetChecked},
                        {"#div121", kNotCached, kNotYetChecked},
                        {"#div122", kNotCached, kNotYetChecked},
                        {"#div123", kNotCached, kNotYetChecked}},
                       /* expected_fast_reject_filter_cache_count */ 0,
                       /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest, QuerySelectorAllCase7) {
  // CheckPseudoHasArgumentTraversalScope::kOneNextSiblingFixedDepthDescendants

  auto* document = HTMLDocument::CreateForTest();
  document->write(R"HTML(
    <!DOCTYPE html>
    <main id=main>
      <div id=div1>
        <div id=div11></div>
        <div id=div12></div>
        <div id=div13></div>
      </div>
      <div id=div2 class=a>
        <div id=div21></div>
        <div id=div22 class=b></div>
        <div id=div23 class=a>
          <div id=div231></div>
          <div id=div232 class=b></div>
          <div id=div233></div>
        </div>
      </div>
    </main>
  )HTML");

  TestMatches(document, "div1", ":has(+ .a > .b)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 0,
              {{"#main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div12", kNotCached, kNotYetChecked},
               {"#div13", kNotCached, kNotYetChecked},
               {"#div2", kNotCached, kNotYetChecked},
               {"#div21", kNotCached, kNotYetChecked},
               {"#div22", kNotCached, kNotYetChecked},
               {"#div23", kNotCached, kNotYetChecked},
               {"#div231", kNotCached, kNotYetChecked},
               {"#div232", kNotCached, kNotYetChecked},
               {"#div233", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 0,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div22", ":has(+ .a > .b)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 0,
              {{"#main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div12", kNotCached, kNotYetChecked},
               {"#div13", kNotCached, kNotYetChecked},
               {"#div2", kNotCached, kNotYetChecked},
               {"#div21", kNotCached, kNotYetChecked},
               {"#div22", kNotCached, kNotYetChecked},
               {"#div23", kNotCached, kNotYetChecked},
               {"#div231", kNotCached, kNotYetChecked},
               {"#div232", kNotCached, kNotYetChecked},
               {"#div233", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 0,
              /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(document, "main", ":has(+ .a > .b)", {"div1", "div22"},
                       /* expected_result_cache_count */ 0,
                       {{"#main", kNotCached, kNotYetChecked},
                        {"#div1", kNotCached, kNotYetChecked},
                        {"#div11", kNotCached, kNotYetChecked},
                        {"#div12", kNotCached, kNotYetChecked},
                        {"#div13", kNotCached, kNotYetChecked},
                        {"#div2", kNotCached, kNotYetChecked},
                        {"#div21", kNotCached, kNotYetChecked},
                        {"#div22", kNotCached, kNotYetChecked},
                        {"#div23", kNotCached, kNotYetChecked},
                        {"#div231", kNotCached, kNotYetChecked},
                        {"#div232", kNotCached, kNotYetChecked},
                        {"#div233", kNotCached, kNotYetChecked}},
                       /* expected_fast_reject_filter_cache_count */ 0,
                       /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest, QuerySelectorAllCase8) {
  // CheckPseudoHasArgumentTraversalScope::kAllNextSiblingsFixedDepthDescendants

  auto* document = HTMLDocument::CreateForTest();
  document->write(R"HTML(
    <!DOCTYPE html>
    <main id=main>
      <div id=div1>
        <div id=div11></div>
        <div id=div12></div>
        <div id=div13></div>
      </div>
      <div id=div2>
        <div id=div21></div>
        <div id=div22 class=a>
          <div id=div221 class=b></div>
          <div id=div222></div>
          <div id=div223></div>
        </div>
        <div id=div23></div>
      </div>
      <div id=div3 class=a>
        <div id=div31 class=b></div>
        <div id=div32></div>
        <div id=div33></div>
      </div>
    </main>
  )HTML");

  TestMatches(document, "div1", ":has(~ .a > .b)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 0,
              {{"#main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div12", kNotCached, kNotYetChecked},
               {"#div13", kNotCached, kNotYetChecked},
               {"#div2", kNotCached, kNotYetChecked},
               {"#div21", kNotCached, kNotYetChecked},
               {"#div22", kNotCached, kNotYetChecked},
               {"#div221", kNotCached, kNotYetChecked},
               {"#div222", kNotCached, kNotYetChecked},
               {"#div223", kNotCached, kNotYetChecked},
               {"#div23", kNotCached, kNotYetChecked},
               {"#div3", kNotCached, kNotYetChecked},
               {"#div31", kNotCached, kNotYetChecked},
               {"#div32", kNotCached, kNotYetChecked},
               {"#div33", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 0,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div2", ":has(~ .a > .b)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 0,
              {{"#main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div12", kNotCached, kNotYetChecked},
               {"#div13", kNotCached, kNotYetChecked},
               {"#div2", kNotCached, kNotYetChecked},
               {"#div21", kNotCached, kNotYetChecked},
               {"#div22", kNotCached, kNotYetChecked},
               {"#div221", kNotCached, kNotYetChecked},
               {"#div222", kNotCached, kNotYetChecked},
               {"#div223", kNotCached, kNotYetChecked},
               {"#div23", kNotCached, kNotYetChecked},
               {"#div3", kNotCached, kNotYetChecked},
               {"#div31", kNotCached, kNotYetChecked},
               {"#div32", kNotCached, kNotYetChecked},
               {"#div33", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 0,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div21", ":has(~ .a > .b)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 0,
              {{"#main", kNotCached, kNotYetChecked},
               {"#div1", kNotCached, kNotYetChecked},
               {"#div11", kNotCached, kNotYetChecked},
               {"#div12", kNotCached, kNotYetChecked},
               {"#div13", kNotCached, kNotYetChecked},
               {"#div2", kNotCached, kNotYetChecked},
               {"#div21", kNotCached, kNotYetChecked},
               {"#div22", kNotCached, kNotYetChecked},
               {"#div221", kNotCached, kNotYetChecked},
               {"#div222", kNotCached, kNotYetChecked},
               {"#div223", kNotCached, kNotYetChecked},
               {"#div23", kNotCached, kNotYetChecked},
               {"#div3", kNotCached, kNotYetChecked},
               {"#div31", kNotCached, kNotYetChecked},
               {"#div32", kNotCached, kNotYetChecked},
               {"#div33", kNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 0,
              /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(document, "main", ":has(~ .a > .b)",
                       {"div1", "div2", "div21"},
                       /* expected_result_cache_count */ 0,
                       {{"#main", kNotCached, kNotYetChecked},
                        {"#div1", kNotCached, kNotYetChecked},
                        {"#div11", kNotCached, kNotYetChecked},
                        {"#div12", kNotCached, kNotYetChecked},
                        {"#div13", kNotCached, kNotYetChecked},
                        {"#div2", kNotCached, kNotYetChecked},
                        {"#div21", kNotCached, kNotYetChecked},
                        {"#div22", kNotCached, kNotYetChecked},
                        {"#div221", kNotCached, kNotYetChecked},
                        {"#div222", kNotCached, kNotYetChecked},
                        {"#div223", kNotCached, kNotYetChecked},
                        {"#div23", kNotCached, kNotYetChecked},
                        {"#div3", kNotCached, kNotYetChecked},
                        {"#div31", kNotCached, kNotYetChecked},
                        {"#div32", kNotCached, kNotYetChecked},
                        {"#div33", kNotCached, kNotYetChecked}},
                       /* expected_fast_reject_filter_cache_count */ 0,
                       /* expected_bloom_filter_allocation_count */ 0);
}

}  // namespace blink
