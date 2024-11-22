// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/element_rule_collector.h"

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/resolver/element_resolve_context.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/selector_filter.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

using css_test_helpers::ParseRule;

static RuleSet* RuleSetFromSingleRule(Document& document, const String& text) {
  auto* style_rule =
      DynamicTo<StyleRule>(css_test_helpers::ParseRule(document, text));
  if (style_rule == nullptr) {
    return nullptr;
  }
  RuleSet* rule_set = MakeGarbageCollected<RuleSet>();
  MediaQueryEvaluator* medium =
      MakeGarbageCollected<MediaQueryEvaluator>(document.GetFrame());
  rule_set->AddStyleRule(style_rule, /*parent_rule=*/nullptr, *medium,
                         kRuleHasNoSpecialState, /*within_mixin=*/false);
  return rule_set;
}

class ElementRuleCollectorTest : public PageTestBase {
 public:
  EInsideLink InsideLink(Element* element) {
    if (!element) {
      return EInsideLink::kNotInsideLink;
    }
    if (element->IsLink()) {
      ElementResolveContext context(*element);
      return context.ElementLinkState();
    }
    return InsideLink(DynamicTo<Element>(FlatTreeTraversal::Parent(*element)));
  }

  // Matches an element against a selector via ElementRuleCollector.
  //
  // Upon successful match, the combined CSSSelector::LinkMatchMask of
  // of all matched rules is returned, or std::nullopt if no-match.
  std::optional<unsigned> Match(Element* element,
                                const String& selector,
                                const ContainerNode* scope = nullptr) {
    ElementResolveContext context(*element);
    SelectorFilter filter;
    MatchResult result;
    ElementRuleCollector collector(context, StyleRecalcContext(), filter,
                                   result, InsideLink(element));

    String rule = selector + " { color: green }";
    RuleSet* rule_set = RuleSetFromSingleRule(GetDocument(), rule);
    if (!rule_set) {
      return std::nullopt;
    }

    MatchRequest request(rule_set, scope);

    collector.CollectMatchingRules(request, /*part_names*/ nullptr);
    collector.SortAndTransferMatchedRules(CascadeOrigin::kAuthor,
                                          /*is_vtt_embedded_style=*/false,
                                          /*tracker=*/nullptr);

    const MatchedPropertiesVector& vector = result.GetMatchedProperties();
    if (!vector.size()) {
      return std::nullopt;
    }

    // Either the normal rules matched, the visited dependent rules matched,
    // or both. There should be nothing else.
    DCHECK(vector.size() == 1 || vector.size() == 2);

    unsigned link_match_type = 0;
    for (const auto& matched_properties : vector) {
      link_match_type |= matched_properties.data_.link_match_type;
    }
    return link_match_type;
  }

  Vector<MatchedRule> GetAllMatchedRules(Element* element, RuleSet* rule_set) {
    ElementResolveContext context(*element);
    SelectorFilter filter;
    MatchResult result;
    ElementRuleCollector collector(context, StyleRecalcContext(), filter,
                                   result, InsideLink(element));

    MatchRequest request(rule_set, {});

    collector.CollectMatchingRules(request, /*part_names*/ nullptr);
    return Vector<MatchedRule>{collector.MatchedRulesForTest()};
  }

  RuleIndexList* GetMatchedCSSRuleList(Element* element,
                                       RuleSet* rule_set,
                                       const CSSStyleSheet* sheet) {
    ElementResolveContext context(*element);
    SelectorFilter filter;
    MatchResult result;
    ElementRuleCollector collector(context, StyleRecalcContext(), filter,
                                   result, InsideLink(element));

    MatchRequest request(rule_set, {}, sheet);

    collector.SetMode(SelectorChecker::kCollectingCSSRules);
    collector.CollectMatchingRules(request, /*part_names*/ nullptr);
    collector.SortAndTransferMatchedRules(CascadeOrigin::kAuthor,
                                          /*is_vtt_embedded_style=*/false,
                                          /*tracker=*/nullptr);

    return collector.MatchedCSSRuleList();
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
  Element* foo = GetDocument().getElementById(AtomicString("foo"));
  Element* bar = GetDocument().getElementById(AtomicString("bar"));
  Element* visited = GetDocument().getElementById(AtomicString("visited"));
  Element* link = GetDocument().getElementById(AtomicString("link"));
  Element* unvisited_span =
      GetDocument().getElementById(AtomicString("unvisited_span"));
  Element* visited_span =
      GetDocument().getElementById(AtomicString("visited_span"));
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

  EXPECT_EQ(Match(foo, "#bar"), std::nullopt);
  EXPECT_EQ(Match(visited, "#foo"), std::nullopt);
  EXPECT_EQ(Match(link, "#foo"), std::nullopt);

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
  EXPECT_EQ(Match(visited, ":link:visited"), std::nullopt);
  EXPECT_EQ(Match(visited, ":visited:link"), std::nullopt);
  EXPECT_EQ(Match(visited, "#visited:visited"), kMatchVisited);
  EXPECT_EQ(Match(visited, ":visited#visited"), kMatchVisited);
  EXPECT_EQ(Match(visited, "body :link"), kMatchLink);
  EXPECT_EQ(Match(visited, "body > :link"), kMatchLink);
  EXPECT_EQ(Match(visited_span, ":link span"), kMatchLink);
  EXPECT_EQ(Match(visited_span, ":visited span"), kMatchVisited);
  EXPECT_EQ(Match(visited, ":not(:visited)"), kMatchLink);
  EXPECT_EQ(Match(visited, ":not(:link)"), kMatchVisited);
  EXPECT_EQ(Match(visited, ":not(:link):not(:visited)"), std::nullopt);
  EXPECT_EQ(Match(visited, ":is(:not(:link))"), kMatchVisited);
  EXPECT_EQ(Match(visited, ":is(:not(:visited))"), kMatchLink);
  EXPECT_EQ(Match(visited, ":is(:link, :not(:link))"), kMatchAll);
  EXPECT_EQ(Match(visited, ":is(:not(:visited), :not(:link))"), kMatchAll);
  EXPECT_EQ(Match(visited, ":is(:not(:visited):not(:link))"), std::nullopt);
  EXPECT_EQ(Match(visited, ":is(:not(:visited):link)"), kMatchLink);
  EXPECT_EQ(Match(visited, ":not(:is(:link))"), kMatchVisited);
  EXPECT_EQ(Match(visited, ":not(:is(:visited))"), kMatchLink);
  EXPECT_EQ(Match(visited, ":not(:is(:not(:visited)))"), kMatchVisited);
  EXPECT_EQ(Match(visited, ":not(:is(:link, :visited))"), std::nullopt);
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
  EXPECT_EQ(Match(visited, ":is(:link):is(:visited)"), std::nullopt);
  EXPECT_EQ(Match(visited, ":is(:link:visited)"), std::nullopt);
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
  EXPECT_EQ(Match(bar, ":visited + #bar"), std::nullopt);
  EXPECT_EQ(Match(bar, ":is(:link + #bar)"), kMatchLink);
  EXPECT_EQ(Match(bar, ":is(:visited ~ #bar)"), std::nullopt);
  EXPECT_EQ(Match(bar, ":not(:is(:link + #bar))"), std::nullopt);
  EXPECT_EQ(Match(bar, ":not(:is(:visited ~ #bar))"), kMatchLink);
}

TEST_F(ElementRuleCollectorTest, LinkMatchTypeHostContext) {
  SetBodyInnerHTML(R"HTML(
    <a href=""><div id="visited_host"></div></a>
    <a href="unvisited"><div id="unvisited_host"></div></a>
  )HTML");

  Element* visited_host =
      GetDocument().getElementById(AtomicString("visited_host"));
  Element* unvisited_host =
      GetDocument().getElementById(AtomicString("unvisited_host"));
  ASSERT_TRUE(visited_host);
  ASSERT_TRUE(unvisited_host);

  ShadowRoot& visited_root =
      visited_host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  ShadowRoot& unvisited_root =
      unvisited_host->AttachShadowRootForTesting(ShadowRootMode::kOpen);

  visited_root.setInnerHTML(R"HTML(
    <style id=style></style>
    <div id=div></div>
  )HTML");
  unvisited_root.setInnerHTML(R"HTML(
    <style id=style></style>
    <div id=div></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* visited_style = visited_root.getElementById(AtomicString("style"));
  Element* unvisited_style =
      unvisited_root.getElementById(AtomicString("style"));
  ASSERT_TRUE(visited_style);
  ASSERT_TRUE(unvisited_style);

  Element* visited_div = visited_root.getElementById(AtomicString("div"));
  Element* unvisited_div = unvisited_root.getElementById(AtomicString("div"));
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
  SegmentedBuffer data;
  data.Append(markup.Utf8().data(), markup.length());
  GetFrame().ForceSynchronousDocumentInstall(AtomicString("text/xml"),
                                             std::move(data));

  // Creates a StyleSheetContents with selector and optional default @namespace,
  // matches rules for originating element, then returns the non-universal flag
  // for ::highlight(x) or the given PseudoId.
  auto run = [&](Element& element, String selector,
                 std::optional<AtomicString> defaultNamespace) {
    auto* parser_context = MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext);
    auto* sheet = MakeGarbageCollected<StyleSheetContents>(parser_context);
    sheet->ParserAddNamespace(AtomicString("bar"),
                              AtomicString("http://example.org/bar"));
    if (defaultNamespace) {
      sheet->ParserAddNamespace(g_null_atom, *defaultNamespace);
    }
    MediaQueryEvaluator* medium =
        MakeGarbageCollected<MediaQueryEvaluator>(GetDocument().GetFrame());
    RuleSet& rules = sheet->EnsureRuleSet(*medium);
    auto* rule = To<StyleRule>(CSSParser::ParseRule(
        sheet->ParserContext(), sheet, CSSNestingType::kNone,
        /*parent_rule_for_nesting=*/nullptr, selector + " { color: green }"));
    rules.AddStyleRule(rule, /*parent_rule=*/nullptr, *medium,
                       kRuleHasNoSpecialState, /*within_mixin=*/false);

    MatchResult result;
    ElementResolveContext context{element};
    ElementRuleCollector collector(context, StyleRecalcContext(),
                                   SelectorFilter(), result,
                                   EInsideLink::kNotInsideLink);
    collector.CollectMatchingRules(MatchRequest{&sheet->GetRuleSet(), nullptr},
                                   /*part_names*/ nullptr);

    // Pretty-print the arguments for debugging.
    StringBuilder args{};
    args.Append("(<");
    args.Append(element.ToString());
    args.Append(">, ");
    args.Append(selector);
    args.Append(", ");
    if (defaultNamespace) {
      args.Append(String("\"" + *defaultNamespace + "\""));
    } else {
      args.Append("{}");
    }
    args.Append(")");

    return result.HasNonUniversalHighlightPseudoStyles();
  };

  Element& body = *GetDocument().body();
  Element& none = *body.QuerySelector(AtomicString("none"));
  Element& bar = *body.QuerySelector(AtomicString("bar"));
  Element& def = *body.QuerySelector(AtomicString("default"));
  AtomicString defNs("http://example.org/default");

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
  for (auto& ns : Vector<std::optional<AtomicString>>{{}, defNs}) {
    // no default ::highlight(x), default *|*::highlight(x)
    EXPECT_FALSE(run(body, "*|*::highlight(x)", ns));
    // no default .foo::highlight(x), default *|*.foo::highlight(x)
    EXPECT_TRUE(run(body, "*|*.foo::highlight(x)", ns));
    EXPECT_TRUE(run(none, "|*::highlight(x)", ns));    // empty|*::highlight(x)
    EXPECT_TRUE(run(bar, "bar|*::highlight(x)", ns));  // bar|*::highlight(x)
  }
}

TEST_F(ElementRuleCollectorTest, DirectNesting) {
  SetBodyInnerHTML(R"HTML(
    <div id="foo" class="a">
      <div id="bar" class="b">
         <div id="baz" class="b">
         </div>
      </div>
    </div>
  )HTML");
  String rule = R"CSS(
    #foo {
       color: green;
       &.a { color: red; }
       & > .b { color: navy; }
    }
  )CSS";
  RuleSet* rule_set = RuleSetFromSingleRule(GetDocument(), rule);
  ASSERT_NE(nullptr, rule_set);

  Element* foo = GetDocument().getElementById(AtomicString("foo"));
  Element* bar = GetDocument().getElementById(AtomicString("bar"));
  Element* baz = GetDocument().getElementById(AtomicString("baz"));
  ASSERT_NE(nullptr, foo);
  ASSERT_NE(nullptr, bar);
  ASSERT_NE(nullptr, baz);

  Vector<MatchedRule> foo_rules = GetAllMatchedRules(foo, rule_set);
  ASSERT_EQ(2u, foo_rules.size());
  EXPECT_EQ("#foo", foo_rules[0].GetRuleData()->Selector().SelectorText());
  EXPECT_EQ("&.a", foo_rules[1].GetRuleData()->Selector().SelectorText());

  Vector<MatchedRule> bar_rules = GetAllMatchedRules(bar, rule_set);
  ASSERT_EQ(1u, bar_rules.size());
  EXPECT_EQ("& > .b", bar_rules[0].GetRuleData()->Selector().SelectorText());

  Vector<MatchedRule> baz_rules = GetAllMatchedRules(baz, rule_set);
  ASSERT_EQ(0u, baz_rules.size());
}

TEST_F(ElementRuleCollectorTest, RuleNotStartingWithAmpersand) {
  SetBodyInnerHTML(R"HTML(
    <div id="foo"></div>
    <div id="bar"></div>
  )HTML");
  String rule = R"CSS(
    #foo {
       color: green;
       :not(&) { color: red; }
    }
  )CSS";
  RuleSet* rule_set = RuleSetFromSingleRule(GetDocument(), rule);
  ASSERT_NE(nullptr, rule_set);

  Element* foo = GetDocument().getElementById(AtomicString("foo"));
  Element* bar = GetDocument().getElementById(AtomicString("bar"));
  ASSERT_NE(nullptr, foo);
  ASSERT_NE(nullptr, bar);

  Vector<MatchedRule> foo_rules = GetAllMatchedRules(foo, rule_set);
  ASSERT_EQ(1u, foo_rules.size());
  EXPECT_EQ("#foo", foo_rules[0].GetRuleData()->Selector().SelectorText());

  Vector<MatchedRule> bar_rules = GetAllMatchedRules(bar, rule_set);
  ASSERT_EQ(1u, bar_rules.size());
  EXPECT_EQ(":not(&)", bar_rules[0].GetRuleData()->Selector().SelectorText());
}

TEST_F(ElementRuleCollectorTest, NestingAtToplevelMatchesNothing) {
  SetBodyInnerHTML(R"HTML(
    <div id="foo"></div>
  )HTML");
  String rule = R"CSS(
    & { color: red; }
  )CSS";
  RuleSet* rule_set = RuleSetFromSingleRule(GetDocument(), rule);
  ASSERT_NE(nullptr, rule_set);

  Element* foo = GetDocument().getElementById(AtomicString("foo"));
  ASSERT_NE(nullptr, foo);

  Vector<MatchedRule> foo_rules = GetAllMatchedRules(foo, rule_set);
  EXPECT_EQ(0u, foo_rules.size());
}

TEST_F(ElementRuleCollectorTest, NestedRulesInMediaQuery) {
  SetBodyInnerHTML(R"HTML(
    <div id="foo"><div id="bar" class="c"></div></div>
    <div id="baz"></div>
  )HTML");
  String rule = R"CSS(
    #foo {
        color: oldlace;
        @media screen {
            & .c { color: palegoldenrod; }
        }
    }
  )CSS";
  RuleSet* rule_set = RuleSetFromSingleRule(GetDocument(), rule);
  ASSERT_NE(nullptr, rule_set);

  Element* foo = GetDocument().getElementById(AtomicString("foo"));
  Element* bar = GetDocument().getElementById(AtomicString("bar"));
  Element* baz = GetDocument().getElementById(AtomicString("baz"));
  ASSERT_NE(nullptr, foo);
  ASSERT_NE(nullptr, bar);
  ASSERT_NE(nullptr, baz);

  Vector<MatchedRule> foo_rules = GetAllMatchedRules(foo, rule_set);
  ASSERT_EQ(1u, foo_rules.size());
  EXPECT_EQ("#foo", foo_rules[0].GetRuleData()->Selector().SelectorText());

  Vector<MatchedRule> bar_rules = GetAllMatchedRules(bar, rule_set);
  ASSERT_EQ(1u, bar_rules.size());
  EXPECT_EQ("& .c", bar_rules[0].GetRuleData()->Selector().SelectorText());

  Vector<MatchedRule> baz_rules = GetAllMatchedRules(baz, rule_set);
  EXPECT_EQ(0u, baz_rules.size());
}

TEST_F(ElementRuleCollectorTest, FindStyleRuleWithNesting) {
  SetBodyInnerHTML(R"HTML(
    <style id="style">
      #foo {
        color: green;
        &.a { color: red; }
        & > .b { color: navy; }
      }
    </style>
    <div id="foo" class="a">
      <div id="bar" class="b">
      </div>
    </div>
  )HTML");
  CSSStyleSheet* sheet =
      To<HTMLStyleElement>(GetDocument().getElementById(AtomicString("style")))
          ->sheet();

  RuleSet* rule_set = &sheet->Contents()->GetRuleSet();
  ASSERT_NE(nullptr, rule_set);

  Element* foo = GetDocument().getElementById(AtomicString("foo"));
  Element* bar = GetDocument().getElementById(AtomicString("bar"));
  ASSERT_NE(nullptr, foo);
  ASSERT_NE(nullptr, bar);

  RuleIndexList* foo_css_rules = GetMatchedCSSRuleList(foo, rule_set, sheet);
  ASSERT_EQ(2u, foo_css_rules->size());
  CSSRule* foo_css_rule_1 = foo_css_rules->at(0).first;
  EXPECT_EQ("#foo", DynamicTo<CSSStyleRule>(foo_css_rule_1)->selectorText());
  CSSRule* foo_css_rule_2 = foo_css_rules->at(1).first;
  EXPECT_EQ("&.a", DynamicTo<CSSStyleRule>(foo_css_rule_2)->selectorText());

  RuleIndexList* bar_css_rules = GetMatchedCSSRuleList(bar, rule_set, sheet);
  ASSERT_EQ(1u, bar_css_rules->size());
  CSSRule* bar_css_rule_1 = bar_css_rules->at(0).first;
  EXPECT_EQ("& > .b", DynamicTo<CSSStyleRule>(bar_css_rule_1)->selectorText());
}

}  // namespace blink
