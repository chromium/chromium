// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/selector_checker.h"
#include "base/optional.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/resolver/element_resolve_context.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

#include "testing/gtest/include/gtest/gtest.h"

#include <iostream>

namespace blink {

class SelectorCheckerTest : public PageTestBase {
 public:
  bool IsInsideVisitedLink(Element* element) {
    if (!element)
      return false;
    if (element->IsLink()) {
      ElementResolveContext context(*element);
      return context.ElementLinkState() == EInsideLink::kInsideVisitedLink;
    }
    return IsInsideVisitedLink(
        DynamicTo<Element>(FlatTreeTraversal::Parent(*element)));
  }

  // Returns the link_match_type upon successful match, or base::nullopt
  // on failure.
  base::Optional<unsigned> Match(
      const SelectorChecker::SelectorCheckingContext& context,
      const String& selector) {
    CSSSelectorList selector_list =
        css_test_helpers::ParseSelectorList(selector);
    DCHECK(selector_list.First()) << "Invalid selector: " << selector;

    SelectorChecker::Init init;
    SelectorChecker checker(init);
    SelectorChecker::SelectorCheckingContext local_context(context);
    local_context.selector = selector_list.First();
    local_context.is_inside_visited_link = IsInsideVisitedLink(context.element);
    SelectorChecker::MatchResult result;
    if (checker.Match(local_context, result))
      return result.link_match_type;

    return base::nullopt;
  }

  base::Optional<unsigned> Match(Element* element, const String& selector) {
    SelectorChecker::SelectorCheckingContext context(element);
    return Match(context, selector);
  }
};

TEST_F(SelectorCheckerTest, LinkMatchType) {
  SetBodyInnerHTML(R"HTML(
    <div id=foo></div>
    <a id=visited href="">
      <span id=visited_span></span>
    </a>
    <a id=link href="unvisited">
      <span id=unvisited_span></span>
    </a>
    <div id=bar></div>
  )HTML");
  Element* foo = GetDocument().getElementById("foo");
  Element* bar = GetDocument().getElementById("bar");
  Element* visited = GetDocument().getElementById("visited");
  Element* link = GetDocument().getElementById("link");
  Element* unvisited_span = GetDocument().getElementById("unvisited_span");
  Element* visited_span = GetDocument().getElementById("visited_span");
  ASSERT_TRUE(foo);
  ASSERT_TRUE(bar);
  ASSERT_TRUE(visited);
  ASSERT_TRUE(link);
  ASSERT_TRUE(unvisited_span);
  ASSERT_TRUE(visited_span);

  ASSERT_TRUE(IsInsideVisitedLink(visited));
  ASSERT_TRUE(IsInsideVisitedLink(visited_span));
  ASSERT_FALSE(IsInsideVisitedLink(foo));
  ASSERT_FALSE(IsInsideVisitedLink(link));
  ASSERT_FALSE(IsInsideVisitedLink(unvisited_span));
  ASSERT_FALSE(IsInsideVisitedLink(bar));

  const auto kMatchLink = CSSSelector::kMatchLink;
  const auto kMatchVisited = CSSSelector::kMatchVisited;
  const auto kMatchAll = CSSSelector::kMatchAll;
  ASSERT_TRUE(kMatchLink);
  ASSERT_TRUE(kMatchVisited);
  ASSERT_TRUE(kMatchAll);

  EXPECT_EQ(Match(foo, "#bar"), base::nullopt);
  EXPECT_EQ(Match(visited, "#foo"), base::nullopt);
  EXPECT_EQ(Match(link, "#foo"), base::nullopt);

  EXPECT_EQ(Match(foo, "#foo"), kMatchAll);
  EXPECT_EQ(Match(link, ":visited"), base::nullopt);
  // Note that |link| isn't a _visited_ link, hence it gets regular treatment by
  // SelectorChecker::Match. And for regular treatments we always get kMatchAll
  // if the selector matches.
  EXPECT_EQ(Match(link, ":link"), kMatchAll);
  EXPECT_EQ(Match(foo, ":not(:visited)"), kMatchAll);
  EXPECT_EQ(Match(foo, ":not(:link)"), kMatchAll);
  EXPECT_EQ(Match(foo, ":not(:link):not(:visited)"), kMatchAll);

  EXPECT_EQ(Match(visited, ":link"), kMatchLink);
  EXPECT_EQ(Match(visited, ":visited"), kMatchVisited);
  EXPECT_EQ(Match(visited, ":link:visited"), base::nullopt);
  EXPECT_EQ(Match(visited, ":visited:link"), base::nullopt);
  EXPECT_EQ(Match(visited, "#visited:visited"), kMatchVisited);
  EXPECT_EQ(Match(visited, ":visited#visited"), kMatchVisited);
  EXPECT_EQ(Match(visited, "body :link"), kMatchLink);
  EXPECT_EQ(Match(visited, "body > :link"), kMatchLink);
  EXPECT_EQ(Match(visited_span, ":link span"), kMatchLink);
  EXPECT_EQ(Match(visited_span, ":visited span"), kMatchVisited);
  EXPECT_EQ(Match(visited, ":not(:visited)"), kMatchLink);
  EXPECT_EQ(Match(visited, ":not(:link)"), kMatchVisited);
  EXPECT_EQ(Match(visited, ":not(:link):not(:visited)"), base::nullopt);
  EXPECT_EQ(Match(visited, ":is(:not(:link))"), kMatchVisited);
  EXPECT_EQ(Match(visited, ":is(:not(:visited))"), kMatchLink);
  EXPECT_EQ(Match(visited, ":is(:link, :not(:link))"), kMatchAll);
  EXPECT_EQ(Match(visited, ":is(:not(:visited), :not(:link))"), kMatchAll);
  EXPECT_EQ(Match(visited, ":is(:not(:visited):not(:link))"), base::nullopt);
  EXPECT_EQ(Match(visited, ":is(:not(:visited):link)"), kMatchLink);
  EXPECT_EQ(Match(visited, ":not(:is(:link))"), kMatchVisited);
  EXPECT_EQ(Match(visited, ":not(:is(:visited))"), kMatchLink);
  EXPECT_EQ(Match(visited, ":not(:is(:not(:visited)))"), kMatchVisited);
  EXPECT_EQ(Match(visited, ":not(:is(:link, :visited))"), base::nullopt);
  EXPECT_EQ(Match(visited, ":not(:is(:link:visited))"), kMatchAll);
  EXPECT_EQ(Match(visited, ":not(:is(:not(:link):visited))"), kMatchLink);
  EXPECT_EQ(Match(visited, ":not(:is(:not(:link):not(:visited)))"), kMatchAll);

  EXPECT_EQ(Match(visited, ":is(#visited)"), kMatchAll);
  EXPECT_EQ(Match(visited, ":is(#visited, :visited)"), kMatchAll);
  EXPECT_EQ(Match(visited, ":is(#visited, :link)"), kMatchAll);
  EXPECT_EQ(Match(visited, ":is(#unrelated, :link)"), kMatchLink);
  EXPECT_EQ(Match(visited, ":is(:visited, :is(#unrelated))"), kMatchVisited);
  EXPECT_EQ(Match(visited, ":is(:visited, #visited)"), kMatchAll);
  EXPECT_EQ(Match(visited, ":is(:link, #visited)"), kMatchAll);
  EXPECT_EQ(Match(visited, ":is(:visited)"), kMatchVisited);
  EXPECT_EQ(Match(visited, ":is(:link)"), kMatchLink);
  EXPECT_EQ(Match(visited, ":is(:link):is(:visited)"), base::nullopt);
  EXPECT_EQ(Match(visited, ":is(:link:visited)"), base::nullopt);
  EXPECT_EQ(Match(visited, ":is(:link, :link)"), kMatchLink);
  EXPECT_EQ(Match(visited, ":is(:is(:link))"), kMatchLink);
  EXPECT_EQ(Match(visited, ":is(:link, :visited)"), kMatchAll);
  EXPECT_EQ(Match(visited, ":is(:link, :visited):link"), kMatchLink);
  EXPECT_EQ(Match(visited, ":is(:link, :visited):visited"), kMatchVisited);
  EXPECT_EQ(Match(visited, ":link:is(:link, :visited)"), kMatchLink);
  EXPECT_EQ(Match(visited, ":visited:is(:link, :visited)"), kMatchVisited);

  // When using :link/:visited in a sibling selector, we expect special
  // behavior for privacy reasons.
  // https://developer.mozilla.org/en-US/docs/Web/CSS/Privacy_and_the_:visited_selector
  EXPECT_EQ(Match(bar, ":link + #bar"), kMatchAll);
  EXPECT_EQ(Match(bar, ":visited + #bar"), base::nullopt);
  EXPECT_EQ(Match(bar, ":is(:link + #bar)"), kMatchAll);
  EXPECT_EQ(Match(bar, ":is(:visited ~ #bar)"), base::nullopt);
  EXPECT_EQ(Match(bar, ":not(:is(:link + #bar))"), base::nullopt);
  EXPECT_EQ(Match(bar, ":not(:is(:visited ~ #bar))"), kMatchAll);
}

TEST_F(SelectorCheckerTest, LinkMatchTypeHostContext) {
  SetBodyInnerHTML(R"HTML(
    <a href=""><div id="visited_host"></div></a>
    <a href="unvisited"><div id="unvisited_host"></div></a>
  )HTML");

  Element* visited_host = GetDocument().getElementById("visited_host");
  Element* unvisited_host = GetDocument().getElementById("unvisited_host");
  ASSERT_TRUE(visited_host);
  ASSERT_TRUE(unvisited_host);

  ShadowRoot& visited_root =
      visited_host->AttachShadowRootInternal(ShadowRootType::kOpen);
  ShadowRoot& unvisited_root =
      unvisited_host->AttachShadowRootInternal(ShadowRootType::kOpen);

  visited_root.setInnerHTML(R"HTML(
    <style id=style></style>
    <div id=div></div>
  )HTML");
  unvisited_root.setInnerHTML(R"HTML(
    <style id=style></style>
    <div id=div></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* visited_style = visited_root.getElementById("style");
  Element* unvisited_style = unvisited_root.getElementById("style");
  ASSERT_TRUE(visited_style);
  ASSERT_TRUE(unvisited_style);

  Element* visited_div = visited_root.getElementById("div");
  Element* unvisited_div = unvisited_root.getElementById("div");
  ASSERT_TRUE(visited_div);
  ASSERT_TRUE(unvisited_div);

  const auto kMatchLink = CSSSelector::kMatchLink;
  const auto kMatchVisited = CSSSelector::kMatchVisited;
  const auto kMatchAll = CSSSelector::kMatchAll;

  {
    SelectorChecker::SelectorCheckingContext context(visited_div);
    context.scope = visited_style;

    EXPECT_EQ(Match(context, ":host-context(a) div"), kMatchAll);
    EXPECT_EQ(Match(context, ":host-context(:link) div"), kMatchLink);
    EXPECT_EQ(Match(context, ":host-context(:visited) div"), kMatchVisited);
    EXPECT_EQ(Match(context, ":host-context(:is(:visited, :link)) div"),
              kMatchAll);

    // :host-context(:not(:visited/link)) matches the host itself.
    EXPECT_EQ(Match(context, ":host-context(:not(:visited)) div"), kMatchAll);
    EXPECT_EQ(Match(context, ":host-context(:not(:link)) div"), kMatchAll);
  }

  {
    SelectorChecker::SelectorCheckingContext context(unvisited_div);
    context.scope = unvisited_style;

    EXPECT_EQ(Match(context, ":host-context(a) div"), kMatchAll);
    EXPECT_EQ(Match(context, ":host-context(:link) div"), kMatchAll);
    EXPECT_EQ(Match(context, ":host-context(:visited) div"), base::nullopt);
    EXPECT_EQ(Match(context, ":host-context(:is(:visited, :link)) div"),
              kMatchAll);
  }
}

}  // namespace blink
