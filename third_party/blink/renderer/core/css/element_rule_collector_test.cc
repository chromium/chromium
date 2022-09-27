// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/element_rule_collector.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/selector_filter.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class ElementRuleCollectorTest : public PageTestBase {
 public:
  EInsideLink InsideLink(Element* element) {
    if (!element)
      return EInsideLink::kNotInsideLink;
    if (element->IsLink()) {
      ElementResolveContext context(*element);
      return context.ElementLinkState();
    }
    return InsideLink(DynamicTo<Element>(FlatTreeTraversal::Parent(*element)));
  }

  // Matches an element against a selector via ElementRuleCollector.
  //
  // Upon successful match, the combined CSSSelector::LinkMatchMask of
  // of all matched rules is returned, or absl::nullopt if no-match.
  absl::optional<unsigned> Match(Element* element,
                                 const String& selector,
                                 const ContainerNode* scope = nullptr) {
    ElementResolveContext context(*element);
    SelectorFilter filter;
    MatchResult result;
    auto style = GetDocument().GetStyleResolver().CreateComputedStyle();
    ElementRuleCollector collector(context, StyleRecalcContext(), filter,
                                   result, style.get(), InsideLink(element));

    String rule = selector + " { color: green }";
    auto* style_rule =
        DynamicTo<StyleRule>(css_test_helpers::ParseRule(GetDocument(), rule));
    if (!style_rule)
      return absl::nullopt;
    RuleSet* rule_set = MakeGarbageCollected<RuleSet>();
    rule_set->AddStyleRule(style_rule, kRuleHasNoSpecialState);

    MatchRequest request(rule_set, scope);

    collector.CollectMatchingRules(request);
    collector.SortAndTransferMatchedRules();

    const MatchedPropertiesVector& vector = result.GetMatchedProperties();
    if (!vector.size())
      return absl::nullopt;

    // Either the normal rules matched, the visited dependent rules matched,
    // or both. There should be nothing else.
    DCHECK(vector.size() == 1 || vector.size() == 2);

    unsigned link_match_type = 0;
    for (const auto& matched_propeties : vector)
      link_match_type |= matched_propeties.types_.link_match_type;
    return link_match_type;
  }
};

TEST_F(ElementRuleCollectorTest, LinkMatchType) {
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

  ASSERT_EQ(EInsideLink::kInsideVisitedLink, InsideLink(visited));
  ASSERT_EQ(EInsideLink::kInsideVisitedLink, InsideLink(visited_span));
  ASSERT_EQ(EInsideLink::kNotInsideLink, InsideLink(foo));
  ASSERT_EQ(EInsideLink::kInsideUnvisitedLink, InsideLink(link));
  ASSERT_EQ(EInsideLink::kInsideUnvisitedLink, InsideLink(unvisited_span));
  ASSERT_EQ(EInsideLink::kNotInsideLink, InsideLink(bar));

  const auto kMatchLink = CSSSelector::kMatchLink;
  const auto kMatchVisited = CSSSelector::kMatchVisited;
  const auto kMatchAll = CSSSelector::kMatchAll;

  EXPECT_EQ(Match(foo, "#bar"), absl::nullopt);
  EXPECT_EQ(Match(visited, "#foo"), absl::nullopt);
  EXPECT_EQ(Match(link, "#foo"), absl::nullopt);

  EXPECT_EQ(Match(foo, "#foo"), kMatchLink);
  EXPECT_EQ(Match(link, ":visited"), kMatchVisited);
  EXPECT_EQ(Match(link, ":link"), kMatchLink);
  // Note that for elements that are not inside links at all, we always
  // expect kMatchLink, since kMatchLink represents the regular (non-visited)
  // style.
  EXPECT_EQ(Match(foo, ":not(:visited)"), kMatchLink);
  EXPECT_EQ(Match(foo, ":not(:link)"), kMatchLink);
  EXPECT_EQ(Match(foo, ":not(:link):not(:visited)"), kMatchLink);

  EXPECT_EQ(Match(visited, ":link"), kMatchLink);
  EXPECT_EQ(Match(visited, ":visited"), kMatchVisited);
  EXPECT_EQ(Match(visited, ":link:visited"), absl::nullopt);
  EXPECT_EQ(Match(visited, ":visited:link"), absl::nullopt);
  EXPECT_EQ(Match(visited, "#visited:visited"), kMatchVisited);
  EXPECT_EQ(Match(visited, ":visited#visited"), kMatchVisited);
  EXPECT_EQ(Match(visited, "body :link"), kMatchLink);
  EXPECT_EQ(Match(visited, "body > :link"), kMatchLink);
  EXPECT_EQ(Match(visited_span, ":link span"), kMatchLink);
  EXPECT_EQ(Match(visited_span, ":visited span"), kMatchVisited);
  EXPECT_EQ(Match(visited, ":not(:visited)"), kMatchLink);
  EXPECT_EQ(Match(visited, ":not(:link)"), kMatchVisited);
  EXPECT_EQ(Match(visited, ":not(:link):not(:visited)"), absl::nullopt);
  EXPECT_EQ(Match(visited, ":is(:not(:link))"), kMatchVisited);
  EXPECT_EQ(Match(visited, ":is(:not(:visited))"), kMatchLink);
  EXPECT_EQ(Match(visited, ":is(:link, :not(:link))"), kMatchAll);
  EXPECT_EQ(Match(visited, ":is(:not(:visited), :not(:link))"), kMatchAll);
  EXPECT_EQ(Match(visited, ":is(:not(:visited):not(:link))"), absl::nullopt);
  EXPECT_EQ(Match(visited, ":is(:not(:visited):link)"), kMatchLink);
  EXPECT_EQ(Match(visited, ":not(:is(:link))"), kMatchVisited);
  EXPECT_EQ(Match(visited, ":not(:is(:visited))"), kMatchLink);
  EXPECT_EQ(Match(visited, ":not(:is(:not(:visited)))"), kMatchVisited);
  EXPECT_EQ(Match(visited, ":not(:is(:link, :visited))"), absl::nullopt);
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
  EXPECT_EQ(Match(visited, ":is(:link):is(:visited)"), absl::nullopt);
  EXPECT_EQ(Match(visited, ":is(:link:visited)"), absl::nullopt);
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
  EXPECT_EQ(Match(bar, ":link + #bar"), kMatchLink);
  EXPECT_EQ(Match(bar, ":visited + #bar"), absl::nullopt);
  EXPECT_EQ(Match(bar, ":is(:link + #bar)"), kMatchLink);
  EXPECT_EQ(Match(bar, ":is(:visited ~ #bar)"), absl::nullopt);
  EXPECT_EQ(Match(bar, ":not(:is(:link + #bar))"), absl::nullopt);
  EXPECT_EQ(Match(bar, ":not(:is(:visited ~ #bar))"), kMatchLink);
}

TEST_F(ElementRuleCollectorTest, LinkMatchTypeHostContext) {
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
    Element* element = visited_div;
    const ContainerNode* scope = visited_style;

    EXPECT_EQ(Match(element, ":host-context(a) div", scope), kMatchAll);
    EXPECT_EQ(Match(element, ":host-context(:link) div", scope), kMatchLink);
    EXPECT_EQ(Match(element, ":host-context(:visited) div", scope),
              kMatchVisited);
    EXPECT_EQ(Match(element, ":host-context(:is(:visited, :link)) div", scope),
              kMatchAll);

    // :host-context(:not(:visited/link)) matches the host itself.
    EXPECT_EQ(Match(element, ":host-context(:not(:visited)) div", scope),
              kMatchAll);
    EXPECT_EQ(Match(element, ":host-context(:not(:link)) div", scope),
              kMatchAll);
  }

  {
    Element* element = unvisited_div;
    const ContainerNode* scope = unvisited_style;

    EXPECT_EQ(Match(element, ":host-context(a) div", scope), kMatchAll);
    EXPECT_EQ(Match(element, ":host-context(:link) div", scope), kMatchLink);
    EXPECT_EQ(Match(element, ":host-context(:visited) div", scope),
              kMatchVisited);
    EXPECT_EQ(Match(element, ":host-context(:is(:visited, :link)) div", scope),
              kMatchAll);
  }
}

TEST_F(ElementRuleCollectorTest, MatchesNonUniversalHighlights) {
  String markup =
      "<html xmlns='http://www.w3.org/1999/xhtml'><body class='foo'>"
      "<none xmlns=''/>"
      "<bar xmlns='http://example.org/bar'/>"
      "<default xmlns='http://example.org/default'/>"
      "</body></html>";
  scoped_refptr<SharedBuffer> data =
      SharedBuffer::Create(markup.Utf8().data(), markup.length());
  GetFrame().ForceSynchronousDocumentInstall("text/xml", data);

  // Creates a StyleSheetContents with selector and optional default @namespace,
  // matches rules for originating element, then returns the non-universal flag
  // for ::highlight(x) or the given PseudoId.
  auto run = [&](Element& element, String selector,
                 absl::optional<AtomicString> defaultNamespace) {
    auto* parser_context = MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext);
    auto* sheet = MakeGarbageCollected<StyleSheetContents>(parser_context);
    sheet->ParserAddNamespace("bar", "http://example.org/bar");
    if (defaultNamespace)
      sheet->ParserAddNamespace(g_null_atom, *defaultNamespace);
    RuleSet& rules = sheet->EnsureRuleSet(
        MediaQueryEvaluator(GetDocument().GetFrame()), kRuleHasNoSpecialState);
    auto* rule = To<StyleRule>(CSSParser::ParseRule(
        sheet->ParserContext(), sheet, selector + " { color: green }"));
    rules.AddStyleRule(rule, kRuleHasNoSpecialState);

    MatchResult result;
    auto style = GetDocument().GetStyleResolver().CreateComputedStyle();
    ElementResolveContext context{element};
    ElementRuleCollector collector(context, StyleRecalcContext(),
                                   SelectorFilter(), result, style.get(),
                                   EInsideLink::kNotInsideLink);
    collector.CollectMatchingRules(MatchRequest{&sheet->GetRuleSet(), nullptr});

    // Pretty-print the arguments for debugging.
    StringBuilder args{};
    args.Append("(<");
    args.Append(element.ToString());
    args.Append(">, ");
    args.Append(selector);
    args.Append(", ");
    if (defaultNamespace)
      args.Append(String("\"" + *defaultNamespace + "\""));
    else
      args.Append("{}");
    args.Append(")");

    return style->HasNonUniversalHighlightPseudoStyles();
  };

  Element& body = *GetDocument().body();
  Element& none = *body.QuerySelector("none");
  Element& bar = *body.QuerySelector("bar");
  Element& def = *body.QuerySelector("default");
  AtomicString defNs = "http://example.org/default";

  // Cases that only make sense without a default @namespace.
  // ::selection kSubSelector :window-inactive
  EXPECT_TRUE(run(body, "::selection:window-inactive", {}));
  EXPECT_TRUE(run(body, "body::highlight(x)", {}));    // body::highlight(x)
  EXPECT_TRUE(run(body, ".foo::highlight(x)", {}));    // .foo::highlight(x)
  EXPECT_TRUE(run(body, "* ::highlight(x)", {}));      // ::highlight(x) *
  EXPECT_TRUE(run(body, "* body::highlight(x)", {}));  // body::highlight(x) *

  // Cases that depend on whether there is a default @namespace.
  EXPECT_FALSE(run(def, "::highlight(x)", {}));     // ::highlight(x)
  EXPECT_FALSE(run(def, "*::highlight(x)", {}));    // ::highlight(x)
  EXPECT_TRUE(run(def, "::highlight(x)", defNs));   // null|*::highlight(x)
  EXPECT_TRUE(run(def, "*::highlight(x)", defNs));  // null|*::highlight(x)

  // Cases that are independent of whether there is a default @namespace.
  for (auto& ns : Vector<absl::optional<AtomicString>>{{}, defNs}) {
    // no default ::highlight(x), default *|*::highlight(x)
    EXPECT_FALSE(run(body, "*|*::highlight(x)", ns));
    // no default .foo::highlight(x), default *|*.foo::highlight(x)
    EXPECT_TRUE(run(body, "*|*.foo::highlight(x)", ns));
    EXPECT_TRUE(run(none, "|*::highlight(x)", ns));    // empty|*::highlight(x)
    EXPECT_TRUE(run(bar, "bar|*::highlight(x)", ns));  // bar|*::highlight(x)
  }
}

}  // namespace blink
