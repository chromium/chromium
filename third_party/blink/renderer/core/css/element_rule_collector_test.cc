// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/element_rule_collector.h"

#include <optional>

#include "base/test/trace_event_analyzer.h"
#include "base/test/trace_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/resolver/element_resolve_context.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/selector_filter.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/inspector/invalidation_set_to_selector_map.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
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
  RuleSet::ApplyMixinsStack apply_mixins_stack;
  rule_set->AddStyleRule(style_rule, /*parent_rule=*/nullptr, *medium,
                         /*mixins=*/{}, kRuleHasNoSpecialState,
                         apply_mixins_stack);
  rule_set->CompactRulesIfNeeded();
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

    RuleSetGroup rule_set_group(/*rule_set_group_index=*/0u);
    rule_set_group.AddRuleSet(rule_set);

    collector.CollectMatchingRules(MatchRequest(rule_set_group, scope),
                                   /*part_names*/ nullptr);
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

  HeapVector<MatchedRule> GetAllMatchedRules(Element* element,
                                             RuleSet* rule_set) {
    ElementResolveContext context(*element);
    SelectorFilter filter;
    MatchResult result;
    ElementRuleCollector collector(context, StyleRecalcContext(), filter,
                                   result, InsideLink(element));

    RuleSetGroup rule_set_group(/*rule_set_group_index=*/0u);
    rule_set_group.AddRuleSet(rule_set);

    collector.CollectMatchingRules(
        MatchRequest(rule_set_group, /*scope=*/nullptr),
        /*part_names*/ nullptr);
    return HeapVector<MatchedRule>{collector.MatchedRulesForTest()};
  }

  RuleIndexList* GetMatchedCSSRuleList(Element* element, RuleSet* rule_set) {
    ElementResolveContext context(*element);
    SelectorFilter filter;
    MatchResult result;
    ElementRuleCollector collector(context, StyleRecalcContext(), filter,
                                   result, InsideLink(element));

    RuleSetGroup rule_set_group(/*rule_set_group_index=*/0u);
    rule_set_group.AddRuleSet(rule_set);

    collector.SetMode(SelectorChecker::kCollectingCSSRules);
    collector.CollectMatchingRules(
        MatchRequest(rule_set_group, /*scope=*/nullptr),
        /*part_names*/ nullptr);
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

  visited_root.SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style id=style></style>
    <div id=div></div>
  )HTML");
  unvisited_root.SetInnerHTMLWithoutTrustedTypes(R"HTML(
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
  data.Append(markup.Utf8());
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
    RuleSet& rules = sheet->EnsureRuleSet(*medium, /*mixins=*/{});
    auto* rule = To<StyleRule>(CSSParser::ParseRule(
        sheet->ParserContext(), sheet, CSSNestingType::kNone,
        /*parent_rule_for_nesting=*/nullptr, selector + " { color: green }"));
    RuleSet::ApplyMixinsStack apply_mixins_stack;
    rules.AddStyleRule(rule, /*parent_rule=*/nullptr, *medium, /*mixins=*/{},
                       kRuleHasNoSpecialState, apply_mixins_stack);

    MatchResult result;
    ElementResolveContext context{element};
    ElementRuleCollector collector(context, StyleRecalcContext(),
                                   SelectorFilter(), result,
                                   EInsideLink::kNotInsideLink);
    sheet->GetRuleSet().CompactRulesIfNeeded();
    RuleSetGroup rule_set_group(/*rule_set_group_index=*/0u);
    rule_set_group.AddRuleSet(&sheet->GetRuleSet());
    collector.CollectMatchingRules(
        MatchRequest(rule_set_group, /*scope=*/nullptr),
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

  HeapVector<MatchedRule> foo_rules = GetAllMatchedRules(foo, rule_set);
  ASSERT_EQ(2u, foo_rules.size());
  EXPECT_EQ("#foo", foo_rules[0].Selector().SelectorText());
  EXPECT_EQ("&.a", foo_rules[1].Selector().SelectorText());

  HeapVector<MatchedRule> bar_rules = GetAllMatchedRules(bar, rule_set);
  ASSERT_EQ(1u, bar_rules.size());
  EXPECT_EQ("& > .b", bar_rules[0].Selector().SelectorText());

  HeapVector<MatchedRule> baz_rules = GetAllMatchedRules(baz, rule_set);
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

  HeapVector<MatchedRule> foo_rules = GetAllMatchedRules(foo, rule_set);
  ASSERT_EQ(1u, foo_rules.size());
  EXPECT_EQ("#foo", foo_rules[0].Selector().SelectorText());

  HeapVector<MatchedRule> bar_rules = GetAllMatchedRules(bar, rule_set);
  ASSERT_EQ(1u, bar_rules.size());
  EXPECT_EQ(":not(&)", bar_rules[0].Selector().SelectorText());
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

  HeapVector<MatchedRule> foo_rules = GetAllMatchedRules(foo, rule_set);
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

  HeapVector<MatchedRule> foo_rules = GetAllMatchedRules(foo, rule_set);
  ASSERT_EQ(1u, foo_rules.size());
  EXPECT_EQ("#foo", foo_rules[0].Selector().SelectorText());

  HeapVector<MatchedRule> bar_rules = GetAllMatchedRules(bar, rule_set);
  ASSERT_EQ(1u, bar_rules.size());
  EXPECT_EQ("& .c", bar_rules[0].Selector().SelectorText());

  HeapVector<MatchedRule> baz_rules = GetAllMatchedRules(baz, rule_set);
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

  RuleIndexList* foo_css_rules = GetMatchedCSSRuleList(foo, rule_set);
  ASSERT_EQ(2u, foo_css_rules->size());
  CSSRule* foo_css_rule_1 = foo_css_rules->at(0).rule.Get();
  EXPECT_EQ("#foo", DynamicTo<CSSStyleRule>(foo_css_rule_1)->selectorText());
  CSSRule* foo_css_rule_2 = foo_css_rules->at(1).rule.Get();
  EXPECT_EQ("&.a", DynamicTo<CSSStyleRule>(foo_css_rule_2)->selectorText());

  RuleIndexList* bar_css_rules = GetMatchedCSSRuleList(bar, rule_set);
  ASSERT_EQ(1u, bar_css_rules->size());
  CSSRule* bar_css_rule_1 = bar_css_rules->at(0).rule.Get();
  EXPECT_EQ("& > .b", DynamicTo<CSSStyleRule>(bar_css_rule_1)->selectorText());
}

TEST_F(ElementRuleCollectorTest, FirstLineUseCounted) {
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kFirstLinePseudoElement));
  SetBodyInnerHTML(R"HTML(
    <style>
      div::first-line {
        text-decoration: underline;
      }
    </style>
    <div>Some text</div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kFirstLinePseudoElement));
}

TEST_F(ElementRuleCollectorTest, FirstLetterUseCounted) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kFirstLetterPseudoElement));
  SetBodyInnerHTML(R"HTML(
    <style>
      div::first-letter {
        text-decoration: underline;
      }
    </style>
    <div>Some text</div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kFirstLetterPseudoElement));
}

TEST_F(ElementRuleCollectorTest, CheckMarkAndPickerIconUseCounted) {
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kCheckMarkPseudoElement));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kPickerIconPseudoElement));
  SetBodyInnerHTML(R"HTML(
    <style>
      select::picker(select) {
        appearance: base-select;
      }
    </style>
    <select aria-label="Pets">
      <option>Dog</option>
      <option>Cat</option>
      <option>Donkey</option>
    </select>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kCheckMarkPseudoElement));
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kPickerIconPseudoElement));
}

TEST_F(ElementRuleCollectorTest, BeforeAfterUseCounted) {
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kBeforePseudoElement));
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kAfterPseudoElement));
  SetBodyInnerHTML(R"HTML(
    <style>
      p::before {
        content: "Before";
      }
      p::after {
        content: "After";
      }
    </style>
    <p>Some text</p>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kBeforePseudoElement));
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kAfterPseudoElement));
}

TEST_F(ElementRuleCollectorTest, MarkerUseCounted) {
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kMarkerPseudoElement));
  SetBodyInnerHTML(R"HTML(
    <style>
      ::marker {
        color: green;
      }
    </style>
    <ul>
      <li>Some text</li>
    </ul>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kMarkerPseudoElement));
}

TEST_F(ElementRuleCollectorTest, BackdropUseCounted) {
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kBackdropPseudoElement));
  SetBodyInnerHTML(R"HTML(
    <style>
      ::backdrop {
        background-color: green;
      }
    </style>
    <p>Some text</p>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kBackdropPseudoElement));
}

TEST_F(ElementRuleCollectorTest, HighlightsUseCounted) {
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kSelectionPseudoElement));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kSearchTextPseudoElement));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kTargetTextPseudoElement));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kCustomHighlightPseudoElement));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kSpellingErrorPseudoElement));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kGrammarErrorPseudoElement));
  SetBodyInnerHTML(R"HTML(
    <style>
      ::selection {
        background-color: green;
      }
      ::search-text {
        background-color: blue;
      }
      ::target-text {
        background-color: red;
      }
      ::highlight(foo) {
        background-color: purple;
      }
      ::spelling-error {
        background-color: yellow;
      }
      ::grammar-error {
        background-color: cyan;
      }
    </style>
    <p>Some text</p>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kSelectionPseudoElement));
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kSearchTextPseudoElement));
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kTargetTextPseudoElement));
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kCustomHighlightPseudoElement));
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kSpellingErrorPseudoElement));
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kGrammarErrorPseudoElement));
}

CORE_EXPORT const CSSStyleSheet* FindStyleSheet(
    const TreeScope* tree_scope_containing_rule,
    const Document& document,
    const StyleRule* rule);

TEST_F(ElementRuleCollectorTest, FindStyleSheet) {
  base::test::TracingEnvironment tracing_environment;
  trace_analyzer::Start(
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"));
  InvalidationSetToSelectorMap::StartOrStopTrackingIfNeeded(
      GetDocument(), GetDocument().GetStyleEngine());
  EXPECT_TRUE(InvalidationSetToSelectorMap::IsTracking());

  SetBodyInnerHTML(R"HTML(
    <style id=target>
      .a .b { color: red; }
    </style>
  )HTML");

  const CSSStyleSheet* author_sheet =
      To<HTMLStyleElement>(GetElementById("target"))->sheet();
  const StyleRule* author_rule =
      To<StyleRule>(author_sheet->Contents()->ChildRules()[0].Get());
  EXPECT_EQ(FindStyleSheet(&GetDocument(), GetDocument(), author_rule),
            author_sheet);

  StyleSheetContents* user_contents = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  user_contents->ParseString(".c .d { color: green; }");
  StyleSheetKey user_key("user");
  GetDocument().GetStyleEngine().InjectSheet(user_key, user_contents,
                                             WebCssOrigin::kUser);
  UpdateAllLifecyclePhasesForTest();
  const StyleRule* user_rule =
      To<StyleRule>(user_contents->ChildRules()[0].Get());
  EXPECT_EQ(FindStyleSheet(nullptr, GetDocument(), user_rule)->Contents(),
            user_contents);

  const StyleRule* rule_not_in_sheet = To<StyleRule>(
      css_test_helpers::ParseRule(GetDocument(), ".e .f { color: blue; }"));
  EXPECT_EQ(FindStyleSheet(nullptr, GetDocument(), rule_not_in_sheet), nullptr);

  trace_analyzer::Stop();
  InvalidationSetToSelectorMap::StartOrStopTrackingIfNeeded(
      GetDocument(), GetDocument().GetStyleEngine());
  EXPECT_FALSE(InvalidationSetToSelectorMap::IsTracking());
}

// https://crbug.com/416699692
TEST_F(ElementRuleCollectorTest, TraceRuleIndexList) {
  SetBodyInnerHTML(R"HTML(
    <style id=style>
      #e {
        color: green;
      }
    </style>
    <thing id=e></thing>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Persistent<RuleIndexList> rule_index_list;

  // All of this stuff should go out of scope, except `rule_index_list`.
  {
    Element* sheet_element =
        GetDocument().getElementById(AtomicString("style"));
    ASSERT_TRUE(sheet_element);
    CSSStyleSheet* sheet = To<HTMLStyleElement>(sheet_element)->sheet();
    RuleSet* rule_set = &sheet->Contents()->GetRuleSet();
    ASSERT_TRUE(rule_set);
    Element* e = GetDocument().getElementById(AtomicString("e"));
    ASSERT_TRUE(e);
    rule_index_list = GetMatchedCSSRuleList(e, rule_set);
  }

  {
    ASSERT_TRUE(rule_index_list);
    ASSERT_EQ(1u, rule_index_list->size());
    CSSRule* css_rule = rule_index_list->at(0).rule.Get();
    ASSERT_TRUE(IsA<CSSStyleRule>(css_rule));
    EXPECT_EQ("#e", DynamicTo<CSSStyleRule>(css_rule)->selectorText());
  }

  ThreadState::Current()->CollectAllGarbageForTesting();

  // After collecting garbage, the objects reachable from `rule_index_list`
  // must still be valid. (crbug.com/416699692)
  {
    ASSERT_TRUE(rule_index_list);
    ASSERT_EQ(1u, rule_index_list->size());
    CSSRule* css_rule = rule_index_list->at(0).rule.Get();
    ASSERT_TRUE(IsA<CSSStyleRule>(css_rule));
    EXPECT_EQ("#e", DynamicTo<CSSStyleRule>(css_rule)->selectorText());
  }
}

}  // namespace blink
