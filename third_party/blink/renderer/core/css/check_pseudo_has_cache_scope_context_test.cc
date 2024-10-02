// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/check_pseudo_has_argument_context.h"
#include "third_party/blink/renderer/core/css/check_pseudo_has_cache_scope.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class CheckPseudoHasCacheScopeContextTest : public PageTestBase {
 protected:
  enum ExpectedCheckPseudoHasResult {
    kSameAsCached,
    kNotYetChecked,
    kAlreadyNotMatched,
  };

  struct ExpectedResultCacheEntry {
    const char* element_query;
    CheckPseudoHasResult cached_result;
    ExpectedCheckPseudoHasResult expected_result;
    const char* shadow_host_id = nullptr;
  };

  static CheckPseudoHasResult GetResult(
      CheckPseudoHasCacheScope::Context& cache_scope_context,
      Element* element) {
    return cache_scope_context.CacheAllowed()
               ? cache_scope_context.GetResult(element)
               : kCheckPseudoHasResultNotCached;
  }

  static bool ElementCached(
      CheckPseudoHasCacheScope::Context& cache_scope_context,
      Element* element) {
    return GetResult(cache_scope_context, element) !=
           kCheckPseudoHasResultNotCached;
  }

  static bool ElementChecked(
      CheckPseudoHasCacheScope::Context& cache_scope_context,
      Element* element) {
    return GetResult(cache_scope_context, element) &
           kCheckPseudoHasResultChecked;
  }

  static ContainerNode* GetQueryRoot(Document* document,
                                     const char* shadow_host_id) {
    if (shadow_host_id) {
      return document->getElementById(AtomicString(shadow_host_id))
          ->GetShadowRoot();
    }
    return document;
  }

  static String TestResultToString(CheckPseudoHasResult test_result) {
    return String::Format(
        "0b%c%c%c%c",
        (test_result & kCheckPseudoHasResultSomeChildrenChecked ? '1' : '0'),
        (test_result & kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked
             ? '1'
             : '0'),
        (test_result & kCheckPseudoHasResultMatched ? '1' : '0'),
        (test_result & kCheckPseudoHasResultChecked ? '1' : '0'));
  }

  template <unsigned length>
  void CheckCacheResults(
      Document* document,
      String query_name,
      const char* selector_text,
      unsigned expected_result_cache_count,
      const ExpectedResultCacheEntry (&expected_result_cache_entries)[length],
      unsigned expected_fast_reject_filter_cache_count,
      unsigned expected_bloom_filter_allocation_count,
      bool match_in_shadow_tree) const {
    HeapVector<CSSSelector> arena;
    base::span<CSSSelector> selector_vector = CSSParser::ParseSelector(
        MakeGarbageCollected<CSSParserContext>(
            *document, NullURL(), true /* origin_clean */, Referrer()),
        CSSNestingType::kNone,
        /*parent_rule_for_nesting=*/nullptr, /*is_within_scope=*/false, nullptr,
        selector_text, arena);
    CSSSelectorList* selector_list =
        CSSSelectorList::AdoptSelectorVector(selector_vector);
    const CSSSelector* selector = nullptr;
    for (selector = selector_list->First();
         selector && selector->GetPseudoType() != CSSSelector::kPseudoHas;
         selector = selector->NextSimpleSelector()) {
    }
    if (!selector) {
      ADD_FAILURE() << "Failed : " << query_name << " (Cannot find :has() in "
                    << selector_text << ")";
      return;
    }
    const CSSSelector* argument_selector = selector->SelectorList()->First();

    CheckPseudoHasArgumentContext argument_context(argument_selector,
                                                   match_in_shadow_tree);
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
          GetQueryRoot(document, expected_result_cache_entry.shadow_host_id)
              ->QuerySelector(
                  AtomicString(expected_result_cache_entry.element_query));
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
                   unsigned expected_bloom_filter_allocation_count,
                   const char* shadow_host_id = nullptr) const {
    Element* query_scope_element =
        GetQueryRoot(document, shadow_host_id)
            ->getElementById(AtomicString(query_scope_element_id));
    ASSERT_TRUE(query_scope_element);

    CheckPseudoHasCacheScope cache_scope(document,
                                         /*within_selector_checking=*/false);

    String query_name = String::Format("#%s.matches('%s')",
                                       query_scope_element_id, selector_text);

    EXPECT_EQ(expected_match_result,
              query_scope_element->matches(AtomicString(selector_text)))
        << "Failed : " << query_name;

    CheckCacheResults(
        document, query_name, selector_text, expected_result_cache_count,
        expected_result_cache_entries, expected_fast_reject_filter_cache_count,
        expected_bloom_filter_allocation_count, !!shadow_host_id);
  }

  template <unsigned query_result_size, unsigned cache_size>
  void TestQuerySelectorAll(Document* document,
                            const char* query_scope_element_id,
                            const char* selector_text,
                            const String (&expected_results)[query_result_size],
                            unsigned expected_result_cache_count,
                            const ExpectedResultCacheEntry (
                                &expected_result_cache_entries)[cache_size],
                            unsigned expected_fast_reject_filter_cache_count,
                            unsigned expected_bloom_filter_allocation_count,
                            const char* shadow_host_id = nullptr) const {
    ContainerNode* query_scope_node = GetQueryRoot(document, shadow_host_id);
    if (query_scope_element_id) {
      query_scope_node = query_scope_node->getElementById(
          AtomicString(query_scope_element_id));
    }
    ASSERT_TRUE(query_scope_node);

    CheckPseudoHasCacheScope cache_scope(document,
                                         /*within_selector_checking=*/false);

    String query_name = String::Format("#%s.querySelectorAll('%s')",
                                       query_scope_element_id, selector_text);

    StaticElementList* result =
        query_scope_node->QuerySelectorAll(AtomicString(selector_text));

    EXPECT_EQ(query_result_size, result->length()) << "Failed : " << query_name;
    unsigned size_max = query_result_size > result->length() ? query_result_size
                                                             : result->length();
    for (unsigned i = 0; i < size_max; ++i) {
      EXPECT_EQ((i < query_result_size ? expected_results[i] : "<null>"),
                (i < result->length() ? result->item(i)->GetIdAttribute()
                                      : AtomicString()))
          << "Failed :" << query_name << " result at index " << i;
    }

    CheckCacheResults(
        document, query_name, selector_text, expected_result_cache_count,
        expected_result_cache_entries, expected_fast_reject_filter_cache_count,
        expected_bloom_filter_allocation_count, !!shadow_host_id);
  }
};

TEST_F(CheckPseudoHasCacheScopeContextTest,
       Case1StartsWithDescendantCombinator) {
  // CheckPseudoHasArgumentTraversalScope::kSubtree

  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
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
      /* expected_match_result */ true,
      /* expected_result_cache_count */ 7,
      {{"main", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched |
            kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div211", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div22",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched |
            kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div221", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div222",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div2221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div223", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2231", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2232", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22321", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22322", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div223221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22323", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div23",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div231", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div241", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div4", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div41", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(
      document, "div2", ":has(.b)",
      /* expected_match_result */ true,
      /* expected_result_cache_count */ 9,
      {{"main", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched |
            kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div211", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div22", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div221", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div222", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2221", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div223", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div2231", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2232",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched |
            kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div22321", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div22322",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div223221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22323", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div23",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div231", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div241", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div4", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div41", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(
      document, "div2", ":has(.c)",
      /* expected_match_result */ false,
      /* expected_result_cache_count */ 2,
      {{"main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div21",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div211", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div222", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div223", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2231", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2232", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22321", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22322", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div223221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22323", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div23", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div231", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div241", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div4", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div41", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest, Case1StartsWithChildCombinator) {
  // CheckPseudoHasArgumentTraversalScope::kSubtree

  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
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
      /* expected_match_result */ true,
      /* expected_result_cache_count */ 5,
      {{"main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div211", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div22",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched |
            kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div221", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2211", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div222", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2221", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div22211", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div22212",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div222121", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22213", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div223",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div2231", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div224", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div2241", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2242", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22421", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22422", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div224221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div224222", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2242221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div224223", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22423", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div224231", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22424", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2243", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22431", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2244", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div225", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2251", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div226", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div23", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div231", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div24", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(
      document, "div2", ":has(> .a .b)",
      /* expected_match_result */ false,
      /* expected_result_cache_count */ 4,
      {{"main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div21",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div211", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2211", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div222", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22211", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22212", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div222121", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22213", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div223", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2231", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div224", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div2241", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2242", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22421", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22422", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div224221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div224222", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2242221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div224223", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22423", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div224231", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22424", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2243", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22431", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2244", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div225", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2251", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div226", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div23", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div231", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(
      document, "div2", ":has(> .a .c)",
      /* expected_match_result */ false,
      /* expected_result_cache_count */ 2,
      {{"main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div21",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div211", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2211", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div222", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22211", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22212", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div222121", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22213", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div223", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2231", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div224", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2241", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2242", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22421", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22422", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div224221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div224222", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2242221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div224223", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22423", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div224231", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22424", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2243", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22431", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2244", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div225", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2251", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div226", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div23", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div231", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest, Case2StartsWithIndirectAdjacent) {
  // CheckPseudoHasArgumentTraversalScope::kAllNextSiblings

  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
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
      /* expected_match_result */ true,
      /* expected_result_cache_count */ 5,
      {{"main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2", kCheckPseudoHasResultSomeChildrenChecked},
       {"#div21", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div211", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div212", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div22", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div221", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div222", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div23", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div231", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div232", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div24",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div241", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div242", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div25", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div251", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div252", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(
      document, "div22", ":has(~ .b)",
      /* expected_match_result */ false,
      /* expected_result_cache_count */ 3,
      {{"main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div211", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div212", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div22", kCheckPseudoHasResultChecked, kSameAsCached},
       {"#div221", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div222", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div23",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div231", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div232", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div24", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div241", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div242", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div25", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div251", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div252", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest, Case2StartsWithDirectAdjacent) {
  // CheckPseudoHasArgumentTraversalScope::kAllNextSiblings

  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
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
      /* expected_match_result */ true,
      /* expected_result_cache_count */ 3,
      {{"main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div211", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div212", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div213", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div22", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div221", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div222", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div223", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div23", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div231", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div232", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div233", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div24", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div241", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div242", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div243", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div25", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div251", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div252", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div253", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div26",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div261", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div262", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div263", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div27", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div271", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div272", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div273", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div4", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div41", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div22", ":has(+ .a ~ .b)",
              /* expected_match_result */ false,
              /* expected_result_cache_count */ 3,
              {{"main", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div2", kCheckPseudoHasResultSomeChildrenChecked},
               {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div211", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div212", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div213", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div22", kCheckPseudoHasResultChecked, kSameAsCached},
               {"#div221", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div222", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div223", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div23",
                kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched |
                    kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div231", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div232", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div233", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div24", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
               {"#div241", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div242", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div243", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div25", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
               {"#div251", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div252", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div253", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div26", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
               {"#div261", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div262", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div263", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div27", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
               {"#div271", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div272", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div273", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div4", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div41", kCheckPseudoHasResultNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div22", ":has(+ .a ~ .c)",
              /* expected_match_result */ false,
              /* expected_result_cache_count */ 3,
              {{"main", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div2", kCheckPseudoHasResultSomeChildrenChecked},
               {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div211", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div212", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div213", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div22", kCheckPseudoHasResultChecked, kSameAsCached},
               {"#div221", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div222", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div223", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div23",
                kCheckPseudoHasResultChecked |
                    kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
                kSameAsCached},
               {"#div231", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div232", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div233", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div24", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
               {"#div241", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div242", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div243", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div25", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
               {"#div251", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div252", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div253", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div26", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
               {"#div261", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div262", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div263", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div27", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
               {"#div271", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div272", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div273", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div4", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div41", kCheckPseudoHasResultNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest, Case3) {
  // CheckPseudoHasArgumentTraversalScope::kOneNextSiblingSubtree

  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
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
      /* expected_match_result */ true,
      /* expected_result_cache_count */ 10,
      {{"main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div22", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div23", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div231", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div232", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2321", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2322", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div23221", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2323", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div233", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div234", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div2341", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2342", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div2343", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div23431", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div23432", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div234321", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div234322",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div2343221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div234323", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div23433",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div234331", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div23434", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2344",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div23441", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2345", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div235",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div2351", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div236", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div241", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div25", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div4", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(
      document, "div1", ":has(+ .a .b)",
      /* expected_match_result */ false,
      /* expected_result_cache_count */ 5,
      {{"main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultChecked, kSameAsCached},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div21",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div22", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div23", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div231", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div232", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2321", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2322", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div23221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2323", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div233", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div234", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2341", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2342", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div2343", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div23431", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div23432", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div234321", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div234322", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2343221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div234323", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div23433", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div234331", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div23434", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2344", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div23441", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2345", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div235", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2351", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div236", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div241", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div25", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div4", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(
      document, "div22", ":has(+ .a .c)",
      /* expected_match_result */ false,
      /* expected_result_cache_count */ 3,
      {{"main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div22", kCheckPseudoHasResultChecked, kSameAsCached},
       {"#div23", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div231",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div232", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2321", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2322", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div23221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2323", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div233", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div234", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2341", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2342", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2343", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div23431", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div23432", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div234321", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div234322", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2343221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div234323", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div23433", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div234331", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div23434", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2344", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div23441", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2345", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div235", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2351", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div236", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div241", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div25", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div4", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest, Case4) {
  // CheckPseudoHasArgumentTraversalScope::kAllNextSiblingSubtrees

  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
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
      /* expected_match_result */ true,
      /* expected_result_cache_count */ 10,
      {{"main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div21", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div22", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div23", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div231", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div232", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div2321", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2322",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div23221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2323", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div233",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div24",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div241", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div242", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2421", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div2422", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div2423", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24231", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24232", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div242321", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div242322", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2423221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div242323", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24233", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div242331", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24234", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2424", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24241", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2425", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div243", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2431", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div244", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div25", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div251", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div26", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div261", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div4", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div41", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(
      document, "div21", ":has(~ .a .b)",
      /* expected_match_result */ true,
      /* expected_result_cache_count */ 10,
      {{"main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div21", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div22", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div23", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div231", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div232", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div2321", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2322",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div23221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2323", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div233",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div24",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div241", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div242", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2421", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div2422", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div2423", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24231", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24232", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div242321", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div242322", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2423221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div242323", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24233", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div242331", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24234", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2424", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24241", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2425", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div243", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2431", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div244", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div25", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div251", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div26", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div261", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div4", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div41", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(
      document, "div1", ":has(~ .a .b)",
      /* expected_match_result */ false,
      /* expected_result_cache_count */ 7,
      {{"main", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultChecked, kSameAsCached},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div21", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div22", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div23", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div231", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div232", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2321", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2322", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div23221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2323", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div233", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div241", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div242", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2421", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div2422", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div2423", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24231", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24232", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div242321", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div242322", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2423221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div242323", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24233", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div242331", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24234", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2424", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24241", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2425", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div243", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2431", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div244", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div25", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div251", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div26", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div261", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div3", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div31", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div4", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div41", kCheckPseudoHasResultNotCached, kAlreadyNotMatched}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(
      document, "div22", ":has(~ .a .c)",
      /* expected_match_result */ false,
      /* expected_result_cache_count */ 3,
      {{"main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div22", kCheckPseudoHasResultChecked, kSameAsCached},
       {"#div23",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div231", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div232", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2321", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2322", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div23221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2323", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div233", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div241", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div242", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2421", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2422", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2423", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24231", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24232", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div242321", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div242322", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2423221", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div242323", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24233", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div242331", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24234", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2424", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div24241", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2425", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div243", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2431", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div244", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div25", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div251", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div26", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div261", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div4", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div41", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest,
       QuerySelectorAllCase1StartsWithDescendantCombinator) {
  // CheckPseudoHasArgumentTraversalScope::kSubtree

  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
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
      /* expected_result_cache_count */ 7,
      {{"html", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"body", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#main", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div1",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched |
            kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div12", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div121", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1211", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div12111", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div13", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div131",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div14",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div11", ":has(.a .b)",
              /* expected_match_result */ false,
              /* expected_result_cache_count */ 1,
              {{"#div11", kCheckPseudoHasResultChecked, kSameAsCached}},
              /* expected_fast_reject_filter_cache_count */ 1,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(
      document, "div12", ":has(.a .b)",
      /* expected_match_result */ true,
      /* expected_result_cache_count */ 8,
      {{"html", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"body", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#main", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div1", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div12", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div121", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div1211", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div12111",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div13", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div131", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div14", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  // ':has(.a .b)' does not match #div1211 but this caches possibly matched
  // elements because argument selector checking can cross over the :has()
  // anchor element.
  TestMatches(
      document, "div1211", ":has(.a .b)",
      /* expected_match_result */ false,
      /* expected_result_cache_count */ 8,
      {{"html", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"body", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#main", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div1", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div12", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div121", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div1211",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div12111",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div13", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div131", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div14", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  // ':has(.a .b)' does not match #div13 but this caches possibly matched
  // elements because argument selector checking can cross over the :has()
  // anchor element.
  TestMatches(
      document, "div13", ":has(.a .b)",
      /* expected_match_result */ false,
      /* expected_result_cache_count */ 6,
      {{"html", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"body", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#main", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div1", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div12", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div121", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1211", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div12111", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div13",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div131",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div14", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(
      document, "main", ":has(.a .b)", {"div1", "div12", "div121"},
      /* expected_result_cache_count */ 12,
      {{"html", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"body", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#main", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div1",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched |
            kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div11", kCheckPseudoHasResultChecked, kSameAsCached},
       {"#div12", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div121", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div1211",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div12111",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div13",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div131",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div14",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached}},
      /* expected_fast_reject_filter_cache_count */ 5,
      /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(
      document, "main", ":has(.a .b), :has(.c .d)", {"div1", "div12", "div121"},
      /* expected_result_cache_count */ 12,
      {{"html", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"body", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#main", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div1",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched |
            kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div11", kCheckPseudoHasResultChecked, kSameAsCached},
       {"#div12", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div121", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div1211",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div12111",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div13",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div131",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div14",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached}},
      /* expected_fast_reject_filter_cache_count */ 6,
      /* expected_bloom_filter_allocation_count */ 3);
}

TEST_F(CheckPseudoHasCacheScopeContextTest,
       QuerySelectorAllCase1StartsWithChildCombinator) {
  // CheckPseudoHasArgumentTraversalScope::kSubtree

  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
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

  TestMatches(
      document, "div1", ":has(> .a .b)",
      /* expected_match_result */ true,
      /* expected_result_cache_count */ 4,
      {{"#div1", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div111", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div1111", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div11111",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(
      document, "div11", ":has(> .a .b)",
      /* expected_match_result */ false,
      /* expected_result_cache_count */ 3,
      {{"#div1", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div11",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div111",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div1111", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div11111", kCheckPseudoHasResultNotCached, kAlreadyNotMatched}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(
      document, "main", ":has(> .a .b)", {"div1", "div111"},
      /* expected_result_cache_count */ 5,
      {{"#div1", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div11",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div111",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div1111",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div11111",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached}},
      /* expected_fast_reject_filter_cache_count */ 2,
      /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(
      document, "main", ":has(> .a .b), :has(> .c .d)", {"div1", "div111"},
      /* expected_result_cache_count */ 5,
      {{"#div1", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div11",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div111",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div1111",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div11111",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached}},
      /* expected_fast_reject_filter_cache_count */ 2,
      /* expected_bloom_filter_allocation_count */ 1);
}

TEST_F(CheckPseudoHasCacheScopeContextTest,
       QuerySelectorAllCase1StartsWithChildCombinatorNonSubjectHas) {
  // CheckPseudoHasArgumentTraversalScope::kSubtree

  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
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

  TestMatches(
      document, "div112", ":has(> .a .b)",
      /* expected_match_result */ false,
      /* expected_result_cache_count */ 2,
      {{"#div112",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div1121",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(
      document, "div111", ":has(> .a .b)",
      /* expected_match_result */ true,
      /* expected_result_cache_count */ 4,
      {{"#div111",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched |
            kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div1111", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div11111",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div1112",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(
      document, "div11", ":has(> .a .b)",
      /* expected_match_result */ true,
      /* expected_result_cache_count */ 6,
      {{"#div11",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched |
            kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div111",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched |
            kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div1111", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div11111",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div1112",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div112",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div1121", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div113", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div1131", kCheckPseudoHasResultNotCached, kAlreadyNotMatched}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(
      document, "div1", ":has(> .a .b)",
      /* expected_match_result */ false,
      /* expected_result_cache_count */ 3,
      {{"#div1",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div11",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div111", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div1111", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div11111", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div1112", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div112", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div1121", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div113", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div1131", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div12", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div121", kCheckPseudoHasResultNotCached, kAlreadyNotMatched}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(
      document, "main", ":has(> .a .b) ~ .c .d", {"div1131", "div121"},
      /* expected_result_cache_count */ 8,
      {{"#div1",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div11",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked |
            kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div111",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched |
            kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div1111", kCheckPseudoHasResultSomeChildrenChecked,
        kAlreadyNotMatched},
       {"#div11111",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div1112",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div112",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked |
            kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div1121",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div113", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div1131", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div12", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div121", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 4,
      /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest,
       QuerySelectorAllCase2NonSubjectHas) {
  // CheckPseudoHasArgumentTraversalScope::kAllNextSiblings

  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
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

  TestMatches(
      document, "div1111", ":has(~ .a) .b",
      /* expected_match_result */ true,
      /* expected_result_cache_count */ 3,
      {{"main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div11", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div111", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div1111", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div112",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div12", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div121", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1211", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div122", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div13", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(
      document, "div1211", ":has(~ .a) .b",
      /* expected_match_result */ true,
      /* expected_result_cache_count */ 7,
      {{"main", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div1",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched |
            kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div111", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1111", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div112", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div12",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div121", kCheckPseudoHasResultChecked, kSameAsCached},
       {"#div1211", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div122",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div13",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div2",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached}},
      /* expected_fast_reject_filter_cache_count */ 3,
      /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(
      document, "main", ":has(~ .a) .b", {"div1111", "div1211"},
      /* expected_result_cache_count */ 10,
      {{"main", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div1",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched |
            kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div11", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div111", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div1111", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div112",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div12",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div121", kCheckPseudoHasResultChecked, kSameAsCached},
       {"#div1211", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div122",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div13",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div2",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached}},
      /* expected_fast_reject_filter_cache_count */ 4,
      /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest,
       QuerySelectorAllCase3NonSubjectHas) {
  // CheckPseudoHasArgumentTraversalScope::kOneNextSiblingSubtree

  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
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

  TestMatches(
      document, "div11", ":has(+ .a .b) .c",
      /* expected_match_result */ true,
      /* expected_result_cache_count */ 3,
      {{"main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div211", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div22", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div221", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div23", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div231",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(
      document, "div211", ":has(+ .a .b) .c",
      /* expected_match_result */ true,
      /* expected_result_cache_count */ 3,
      {{"main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div21", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div211", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div22", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div221",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div23", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div231", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(
      document, "main", ":has(+ .a .b) .c", {"div11", "div211"},
      /* expected_result_cache_count */ 6,
      {{"main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div21", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div211", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div22", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div221",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div23", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div231",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached}},
      /* expected_fast_reject_filter_cache_count */ 2,
      /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest,
       QuerySelectorAllCase4NonSubjectHas) {
  // CheckPseudoHasArgumentTraversalScope::kAllNextSiblingSubtrees

  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
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

  TestMatches(
      document, "div11", ":has(~ .a .b) .c",
      /* expected_match_result */ true,
      /* expected_result_cache_count */ 3,
      {{"main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div211", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2111", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div212", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2121", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div22", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div221",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(
      document, "div2111", ":has(~ .a .b) .c",
      /* expected_match_result */ true,
      /* expected_result_cache_count */ 3,
      {{"main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div21", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div211", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div2111", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div212",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div2121", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div221", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(
      document, "main", ":has(~ .a .b) .c", {"div11", "div2111"},
      /* expected_result_cache_count */ 6,
      {{"main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div21", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div211", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div2111", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div212",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div2121", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div22", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div221",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached}},
      /* expected_fast_reject_filter_cache_count */ 2,
      /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest,
       QuerySelectorAllCase4StartsWithDirectAdjacentCombinator) {
  // CheckPseudoHasArgumentTraversalScope::kAllNextSiblingSubtrees

  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
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
      document, "div1", ":has(+ .a ~ .b .c)",
      /* expected_match_result */ false,
      /* expected_result_cache_count */ 4,
      {{"main", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultChecked, kSameAsCached},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div12", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div13", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div131", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div132", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div14", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div141", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div15", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div21", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div3", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div31", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div4", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div41", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div5", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div51", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div6", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div61", kCheckPseudoHasResultNotCached, kAlreadyNotMatched}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(
      document, "div11", ":has(+ .a ~ .b .c)",
      /* expected_match_result */ true,
      /* expected_result_cache_count */ 5,
      {{"#div1", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div11", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div12", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div13", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div131", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div132",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div14",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div141", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div15", kCheckPseudoHasResultNotCached, kAlreadyNotMatched}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(
      document, "div12", ":has(+ .a ~ .b .c)",
      /* expected_match_result */ false,
      /* expected_result_cache_count */ 4,
      {{"#div1", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div11", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div12", kCheckPseudoHasResultChecked, kSameAsCached},
       {"#div13",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div131", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div132", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div14", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div141", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div15", kCheckPseudoHasResultNotCached, kAlreadyNotMatched}},
      /* expected_fast_reject_filter_cache_count */ 1,
      /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(
      document, "main", ":has(+ .a ~ .b .c)", {"div11", "div4"},
      /* expected_result_cache_count */ 9,
      {{"main", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div1",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div11", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div12", kCheckPseudoHasResultChecked, kSameAsCached},
       {"#div13",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked |
            kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div131", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div132",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div14",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div141", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div15", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div21", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div3", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div31", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div4", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div41", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div5", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div51", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div6", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div61", kCheckPseudoHasResultNotCached, kAlreadyNotMatched}},
      /* expected_fast_reject_filter_cache_count */ 3,
      /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(
      document, "main", ":has(+ .a ~ .b .c), :has(+ .d ~ .e .f)",
      {"div11", "div4"}, /* expected_result_cache_count */ 9,
      {{"main", kCheckPseudoHasResultSomeChildrenChecked, kNotYetChecked},
       {"#div1",
        kCheckPseudoHasResultChecked | kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div11", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div12", kCheckPseudoHasResultChecked, kSameAsCached},
       {"#div13",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked |
            kCheckPseudoHasResultSomeChildrenChecked,
        kSameAsCached},
       {"#div131", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div132",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div14",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div141", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div15", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div2",
        kCheckPseudoHasResultChecked |
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked,
        kSameAsCached},
       {"#div21", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div3", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div31", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div4", kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched,
        kSameAsCached},
       {"#div41", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div5", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div51", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div6", kCheckPseudoHasResultNotCached, kAlreadyNotMatched},
       {"#div61", kCheckPseudoHasResultNotCached, kAlreadyNotMatched}},
      /* expected_fast_reject_filter_cache_count */ 3,
      /* expected_bloom_filter_allocation_count */ 2);
}

TEST_F(CheckPseudoHasCacheScopeContextTest, QuerySelectorAllCase5) {
  // CheckPseudoHasArgumentTraversalScope::kOneNextSibling

  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
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
              {{"#main", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div12", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div13", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div22", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div23", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div32", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div33", kCheckPseudoHasResultNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 0,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div21", ":has(+ .a)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 0,
              {{"#main", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div12", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div13", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div22", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div23", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div32", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div33", kCheckPseudoHasResultNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 0,
              /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(
      document, "main", ":has(+ .a)", {"div2", "div21"},
      /* expected_result_cache_count */ 0,
      {{"#main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div12", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div13", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div22", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div23", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div32", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div33", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 0,
      /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest, QuerySelectorAllCase6) {
  // CheckPseudoHasArgumentTraversalScope::kFixedDepthDescendants

  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
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
              {{"#main", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div111", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div112", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div1121", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div1122", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div1123", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div113", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div12", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div121", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div122", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div123", kCheckPseudoHasResultNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 0,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div112", ":has(> .a)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 0,
              {{"#main", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div111", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div112", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div1121", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div1122", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div1123", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div113", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div12", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div121", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div122", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div123", kCheckPseudoHasResultNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 0,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div12", ":has(> .a)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 0,
              {{"#main", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div111", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div112", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div1121", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div1122", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div1123", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div113", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div12", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div121", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div122", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div123", kCheckPseudoHasResultNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 0,
              /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(
      document, "main", ":has(> .a)", {"div1", "div112", "div12"},
      /* expected_result_cache_count */ 0,
      {{"#main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div111", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div112", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1121", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1122", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1123", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div113", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div12", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div121", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div122", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div123", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 0,
      /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest, QuerySelectorAllCase7) {
  // CheckPseudoHasArgumentTraversalScope::kOneNextSiblingFixedDepthDescendants

  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
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
              {{"#main", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div12", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div13", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div22", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div23", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div231", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div232", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div233", kCheckPseudoHasResultNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 0,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div22", ":has(+ .a > .b)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 0,
              {{"#main", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div12", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div13", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div22", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div23", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div231", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div232", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div233", kCheckPseudoHasResultNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 0,
              /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(
      document, "main", ":has(+ .a > .b)", {"div1", "div22"},
      /* expected_result_cache_count */ 0,
      {{"#main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div12", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div13", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div22", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div23", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div231", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div232", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div233", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 0,
      /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest, QuerySelectorAllCase8) {
  // CheckPseudoHasArgumentTraversalScope::kAllNextSiblingsFixedDepthDescendants

  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
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
              {{"#main", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div12", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div13", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div22", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div221", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div222", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div223", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div23", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div32", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div33", kCheckPseudoHasResultNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 0,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div2", ":has(~ .a > .b)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 0,
              {{"#main", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div12", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div13", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div22", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div221", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div222", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div223", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div23", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div32", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div33", kCheckPseudoHasResultNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 0,
              /* expected_bloom_filter_allocation_count */ 0);

  TestMatches(document, "div21", ":has(~ .a > .b)",
              /* expected_match_result */ true,
              /* expected_result_cache_count */ 0,
              {{"#main", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div12", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div13", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div22", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div221", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div222", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div223", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div23", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div32", kCheckPseudoHasResultNotCached, kNotYetChecked},
               {"#div33", kCheckPseudoHasResultNotCached, kNotYetChecked}},
              /* expected_fast_reject_filter_cache_count */ 0,
              /* expected_bloom_filter_allocation_count */ 0);

  TestQuerySelectorAll(
      document, "main", ":has(~ .a > .b)", {"div1", "div2", "div21"},
      /* expected_result_cache_count */ 0,
      {{"#main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div12", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div13", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div22", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div221", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div222", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div223", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div23", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div32", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div33", kCheckPseudoHasResultNotCached, kNotYetChecked}},
      /* expected_fast_reject_filter_cache_count */ 0,
      /* expected_bloom_filter_allocation_count */ 0);
}

TEST_F(CheckPseudoHasCacheScopeContextTest, QuerySelectorAllCase9) {
  // CheckPseudoHasArgumentTraversalScope::kShadowRootSubtree

  // TODO(blee@igalia.com) Need cache support for this case - :has() checks a
  // relationship between shadow root and its descendant. (e.g. :host:has(.a))

  Document* document = &GetDocument();
  document->body()->setHTMLUnsafe(R"HTML(
    <!DOCTYPE html>
    <main id="main">
      <div id="host">
        <template shadowrootmode="open">
          <div id="div1" class="b">
            <div id="div11"></div>
          </div>
          <div id="div2">
            <div id="div21"></div>
            <div id="div22" class="a">
              <div id="div221" class="b"></div>
            </div>
            <div id="div23"></div>
          </div>
          <div id="div3">
            <div id="div31" class="b"></div>
          </div>
        </template>
      </div>
    </main>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  TestMatches(
      document, "div1", ":host:has(.a) .b", /* expected_match_result */ true,
      /* expected_result_cache_count */ 0,
      {{"#main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#host", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div22", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div221", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div23", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"}},
      /* expected_fast_reject_filter_cache_count */ 0,
      /* expected_bloom_filter_allocation_count */ 0,
      /* shadow_host_id */ "host");

  TestMatches(
      document, "div221", ":host:has(.a) .b", /* expected_match_result */ true,
      /* expected_result_cache_count */ 0,
      {{"#main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#host", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div22", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div221", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div23", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"}},
      /* expected_fast_reject_filter_cache_count */ 0,
      /* expected_bloom_filter_allocation_count */ 0,
      /* shadow_host_id */ "host");

  TestMatches(
      document, "div31", ":host:has(.a) .b", /* expected_match_result */ true,
      /* expected_result_cache_count */ 0,
      {{"#main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#host", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div22", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div221", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div23", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"}},
      /* expected_fast_reject_filter_cache_count */ 0,
      /* expected_bloom_filter_allocation_count */ 0,
      /* shadow_host_id */ "host");

  TestQuerySelectorAll(
      document, nullptr, ":host:has(.a) .b", {"div1", "div221", "div31"},
      /* expected_result_cache_count */ 0,
      {{"#main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#host", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div22", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div221", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div23", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"}},
      /* expected_fast_reject_filter_cache_count */ 0,
      /* expected_bloom_filter_allocation_count */ 0,
      /* shadow_host_id */ "host");
}

TEST_F(CheckPseudoHasCacheScopeContextTest, QuerySelectorAllCase10) {
  // CheckPseudoHasArgumentTraversalScope::kShadowRootFixedDepthDescendants

  Document* document = &GetDocument();
  document->body()->setHTMLUnsafe(R"HTML(
    <!DOCTYPE html>
    <main id="main">
      <div id="host">
        <template shadowrootmode="open">
          <div id="div1" class="b">
            <div id="div11"></div>
          </div>
          <div id="div2">
            <div id="div21"></div>
            <div id="div22" class="a">
              <div id="div221" class="b"></div>
            </div>
            <div id="div23"></div>
          </div>
          <div id="div3">
            <div id="div31" class="b"></div>
          </div>
        </template>
      </div>
    </main>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  TestMatches(
      document, "div1", ":host:has(> div > .a) .b",
      /* expected_match_result */ true, /* expected_result_cache_count */ 0,
      {{"#main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#host", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div22", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div221", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div23", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"}},
      /* expected_fast_reject_filter_cache_count */ 0,
      /* expected_bloom_filter_allocation_count */ 0,
      /* shadow_host_id */ "host");

  TestMatches(
      document, "div221", ":host:has(> div > .a) .b",
      /* expected_match_result */ true, /* expected_result_cache_count */ 0,
      {{"#main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#host", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div22", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div221", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div23", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"}},
      /* expected_fast_reject_filter_cache_count */ 0,
      /* expected_bloom_filter_allocation_count */ 0,
      /* shadow_host_id */ "host");

  TestMatches(
      document, "div31", ":host:has(> div > .a) .b",
      /* expected_match_result */ true, /* expected_result_cache_count */ 0,
      {{"#main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#host", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div22", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div221", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div23", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"}},
      /* expected_fast_reject_filter_cache_count */ 0,
      /* expected_bloom_filter_allocation_count */ 0,
      /* shadow_host_id */ "host");

  TestQuerySelectorAll(
      document, nullptr, ":host:has(> div > .a) .b",
      {"div1", "div221", "div31"}, /* expected_result_cache_count */ 0,
      {{"#main", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#host", kCheckPseudoHasResultNotCached, kNotYetChecked},
       {"#div1", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div11", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div2", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div21", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div22", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div221", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div23", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div3", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"},
       {"#div31", kCheckPseudoHasResultNotCached, kNotYetChecked, "host"}},
      /* expected_fast_reject_filter_cache_count */ 0,
      /* expected_bloom_filter_allocation_count */ 0,
      /* shadow_host_id */ "host");
}

}  // namespace blink
