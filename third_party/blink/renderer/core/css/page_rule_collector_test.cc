// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/page_rule_collector.h"

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/css/resolver/style_cascade.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class PageRuleCollectorTest : public PageTestBase {
 public:
  const ComputedStyle* ComputePageStyle(String ua_sheet_string,
                                        String author_sheet_string) {
    RuleSet* ua_ruleset =
        css_test_helpers::CreateRuleSet(GetDocument(), ua_sheet_string);
    RuleSet* author_ruleset =
        css_test_helpers::CreateRuleSet(GetDocument(), author_sheet_string);

    const ComputedStyle& initial_style =
        GetDocument().GetStyleResolver().InitialStyle();
    Element* root_element = GetDocument().documentElement();

    StyleResolverState state(GetDocument(), *root_element);
    state.CreateNewStyle(initial_style, initial_style);

    STACK_UNINITIALIZED StyleCascade cascade(state);

    PageRuleCollector collector(
        &initial_style, CSSAtRuleID::kCSSAtRulePage, /* page_index */ 0,
        /* page_name */ AtomicString("page"), cascade.MutableMatchResult());

    collector.MatchPageRules(ua_ruleset, CascadeOrigin::kUserAgent,
                             nullptr /* tree_scope */, nullptr /* layer_map */);

    collector.MatchPageRules(author_ruleset, CascadeOrigin::kAuthor,
                             &GetDocument() /* tree_scope */,
                             nullptr /* layer_map */);

    cascade.Apply();

    return state.TakeStyle();
  }
};

TEST_F(PageRuleCollectorTest, UserAgent) {
  String ua_sheet_string = "@page { margin: 1px; }";
  String author_sheet_string = "@page { margin: 2px; }";

  const ComputedStyle* style =
      ComputePageStyle(ua_sheet_string, author_sheet_string);

  EXPECT_EQ(Length::Fixed(2), style->MarginLeft());
}

TEST_F(PageRuleCollectorTest, UserAgentImportant) {
  String ua_sheet_string = "@page { margin: 1px !important; }";
  String author_sheet_string = "@page { margin: 2px; }";

  const ComputedStyle* style =
      ComputePageStyle(ua_sheet_string, author_sheet_string);

  EXPECT_EQ(Length::Fixed(1), style->MarginLeft());
}

}  // namespace blink
