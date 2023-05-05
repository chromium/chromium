/*
 * Copyright (c) 2014, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Opera Software ASA nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/rule_set.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"
#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

using ::testing::ElementsAreArray;

namespace blink {

namespace {

StyleRule* CreateDummyStyleRule() {
  css_test_helpers::TestStyleSheet sheet;
  sheet.AddCSSRules("#id { color: tomato; }");
  const RuleSet& rule_set = sheet.GetRuleSet();
  base::span<const RuleData> rules = rule_set.IdRules("id");
  DCHECK_EQ(1u, rules.size());
  return rules.front().Rule();
}

}  // namespace

TEST(RuleSetTest, findBestRuleSetAndAdd_CustomPseudoElements) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules("summary::-webkit-details-marker { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  AtomicString str("-webkit-details-marker");
  base::span<const RuleData> rules = rule_set.UAShadowPseudoElementRules(str);
  ASSERT_EQ(1u, rules.size());
  ASSERT_EQ(str, rules.front().Selector().Value());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_Id) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules("#id { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  AtomicString str("id");
  base::span<const RuleData> rules = rule_set.IdRules(str);
  ASSERT_EQ(1u, rules.size());
  ASSERT_EQ(str, rules.front().Selector().Value());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_NthChild) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules("div:nth-child(2) { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  AtomicString str("div");
  base::span<const RuleData> rules = rule_set.TagRules(str);
  ASSERT_EQ(1u, rules.size());
  ASSERT_EQ(str, rules.front().Selector().TagQName().LocalName());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_ClassThenId) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules(".class#id { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  AtomicString str("id");
  // id is prefered over class even if class preceeds it in the selector.
  base::span<const RuleData> rules = rule_set.IdRules(str);
  ASSERT_EQ(1u, rules.size());
  AtomicString class_str("class");
  ASSERT_EQ(class_str, rules.front().Selector().Value());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_IdThenClass) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules("#id.class { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  AtomicString str("id");
  base::span<const RuleData> rules = rule_set.IdRules(str);
  ASSERT_EQ(1u, rules.size());
  ASSERT_EQ(str, rules.front().Selector().Value());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_AttrThenId) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules("[attr]#id { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  AtomicString str("id");
  base::span<const RuleData> rules = rule_set.IdRules(str);
  ASSERT_EQ(1u, rules.size());
  AtomicString attr_str("attr");
  ASSERT_EQ(attr_str, rules.front().Selector().Attribute().LocalName());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_TagThenAttrThenId) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules("div[attr]#id { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  AtomicString str("id");
  base::span<const RuleData> rules = rule_set.IdRules(str);
  ASSERT_EQ(1u, rules.size());
  AtomicString tag_str("div");
  ASSERT_EQ(tag_str, rules.front().Selector().TagQName().LocalName());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_TagThenAttr) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules("div[attr] { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  ASSERT_EQ(1u, rule_set.AttrRules("attr").size());
  ASSERT_TRUE(rule_set.TagRules("div").empty());
}

// It's arbitrary which of these we choose, but it needs to match
// the behavior in IsCoveredByBucketing().
TEST(RuleSetTest, findBestRuleSetAndAdd_ThreeClasses) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules(".a.b.c { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  EXPECT_EQ(0u, rule_set.ClassRules("a").size());
  EXPECT_EQ(0u, rule_set.ClassRules("b").size());
  EXPECT_EQ(1u, rule_set.ClassRules("c").size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_AttrThenClass) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules("[attr].class { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  ASSERT_TRUE(rule_set.AttrRules("attr").empty());
  ASSERT_EQ(1u, rule_set.ClassRules("class").size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_Host) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules(":host { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  const base::span<const RuleData> rules = rule_set.ShadowHostRules();
  ASSERT_EQ(1u, rules.size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_HostWithId) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules(":host(#x) { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  const base::span<const RuleData> rules = rule_set.ShadowHostRules();
  ASSERT_EQ(1u, rules.size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_HostContext) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules(":host-context(*) { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  const base::span<const RuleData> rules = rule_set.ShadowHostRules();
  ASSERT_EQ(1u, rules.size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_HostContextWithId) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules(":host-context(#x) { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  const base::span<const RuleData> rules = rule_set.ShadowHostRules();
  ASSERT_EQ(1u, rules.size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_HostAndHostContextNotInRightmost) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules(":host-context(#x) .y, :host(.a) > #b  { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  const base::span<const RuleData> shadow_rules = rule_set.ShadowHostRules();
  base::span<const RuleData> id_rules = rule_set.IdRules("b");
  base::span<const RuleData> class_rules = rule_set.ClassRules("y");
  ASSERT_EQ(0u, shadow_rules.size());
  ASSERT_EQ(1u, id_rules.size());
  ASSERT_EQ(1u, class_rules.size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_HostAndClass) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules(".foo:host { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  const base::span<const RuleData> rules = rule_set.ShadowHostRules();
  ASSERT_EQ(0u, rules.size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_HostContextAndClass) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules(".foo:host-context(*) { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  const base::span<const RuleData> rules = rule_set.ShadowHostRules();
  ASSERT_EQ(0u, rules.size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_Focus) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules(":focus { }");
  sheet.AddCSSRules("[attr]:focus { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  ASSERT_EQ(1u, rule_set.FocusPseudoClassRules().size());
  ASSERT_EQ(1u, rule_set.AttrRules("attr").size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_LinkVisited) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules(":link { }");
  sheet.AddCSSRules("[attr]:link { }");
  sheet.AddCSSRules(":visited { }");
  sheet.AddCSSRules("[attr]:visited { }");
  sheet.AddCSSRules(":-webkit-any-link { }");
  sheet.AddCSSRules("[attr]:-webkit-any-link { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  // Visited-dependent rules (which include selectors that contain :link)
  // are added twice.
  ASSERT_EQ(5u, rule_set.LinkPseudoClassRules().size());
  ASSERT_EQ(5u, rule_set.AttrRules("attr").size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_Cue) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules("::cue(b) { }");
  sheet.AddCSSRules("video::cue(u) { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  const base::span<const RuleData> rules = rule_set.CuePseudoRules();
  ASSERT_EQ(2u, rules.size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_PlaceholderPseudo) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules("::placeholder { }");
  sheet.AddCSSRules("input::placeholder { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  base::span<const RuleData> rules =
      rule_set.UAShadowPseudoElementRules("-webkit-input-placeholder");
  ASSERT_EQ(2u, rules.size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_PartPseudoElements) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules("::part(dummy):focus, #id::part(dummy) { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  const base::span<const RuleData> rules = rule_set.PartPseudoRules();
  ASSERT_EQ(2u, rules.size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_IsSingleArg) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules(":is(.a) { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  base::span<const RuleData> rules = rule_set.ClassRules("a");
  ASSERT_FALSE(rules.empty());
  ASSERT_EQ(1u, rules.size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_WhereSingleArg) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules(":where(.a) { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  base::span<const RuleData> rules = rule_set.ClassRules("a");
  ASSERT_FALSE(rules.empty());
  ASSERT_EQ(1u, rules.size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_WhereSingleArgNested) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules(":where(:is(.a)) { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  base::span<const RuleData> rules = rule_set.ClassRules("a");
  ASSERT_FALSE(rules.empty());
  ASSERT_EQ(1u, rules.size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_IsMultiArg) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules(":is(.a, .b) { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  const base::span<const RuleData> rules = rule_set.UniversalRules();
  ASSERT_EQ(1u, rules.size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_WhereMultiArg) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules(":where(.a, .b) { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  const base::span<const RuleData> rules = rule_set.UniversalRules();
  ASSERT_EQ(1u, rules.size());
}

static void AddManyAttributeRules(base::test::ScopedFeatureList& feature_list,
                                  css_test_helpers::TestStyleSheet& sheet) {
  // Create more than 50 rules, in order to trigger building the Aho-Corasick
  // tree.
  for (int i = 0; i < 100; ++i) {
    char buf[256];
    snprintf(buf, sizeof(buf), "[attr=\"value%d\"] {}", i);
    sheet.AddCSSRules(buf);
  }
}

TEST(RuleSetTest, LargeNumberOfAttributeRules) {
  base::test::ScopedFeatureList feature_list;
  css_test_helpers::TestStyleSheet sheet;
  AddManyAttributeRules(feature_list, sheet);

  sheet.AddCSSRules("[otherattr=\"value\"] {}");

  RuleSet& rule_set = sheet.GetRuleSet();
  base::span<const RuleData> list = rule_set.AttrRules("attr");
  ASSERT_FALSE(list.empty());

  EXPECT_TRUE(rule_set.CanIgnoreEntireList(list, "attr", "notfound"));
  EXPECT_FALSE(rule_set.CanIgnoreEntireList(list, "attr", "value20"));
  EXPECT_FALSE(rule_set.CanIgnoreEntireList(list, "attr", "VALUE20"));

  // A false positive that we expect (value20 is a substring, even though
  // the rule said = and not =*, so we need to check the entire set).
  EXPECT_FALSE(rule_set.CanIgnoreEntireList(list, "attr", "--value20--"));

  // One rule is not enough to build a tree, so we will not mass-reject
  // anything on otherattr.
  base::span<const RuleData> list2 = rule_set.AttrRules("otherattr");
  EXPECT_FALSE(rule_set.CanIgnoreEntireList(list2, "otherattr", "notfound"));
}

TEST(RuleSetTest, LargeNumberOfAttributeRulesWithEmpty) {
  base::test::ScopedFeatureList feature_list;
  css_test_helpers::TestStyleSheet sheet;
  AddManyAttributeRules(feature_list, sheet);

  sheet.AddCSSRules("[attr=\"\"] {}");

  RuleSet& rule_set = sheet.GetRuleSet();
  base::span<const RuleData> list = rule_set.AttrRules("attr");
  ASSERT_FALSE(list.empty());
  EXPECT_TRUE(rule_set.CanIgnoreEntireList(list, "attr", "notfound"));
  EXPECT_FALSE(rule_set.CanIgnoreEntireList(list, "attr", ""));
}

TEST(RuleSetTest, LargeNumberOfAttributeRulesWithCatchAll) {
  base::test::ScopedFeatureList feature_list;
  css_test_helpers::TestStyleSheet sheet;
  AddManyAttributeRules(feature_list, sheet);

  // This should match everything, so we cannot reject anything.
  sheet.AddCSSRules("[attr] {}");

  RuleSet& rule_set = sheet.GetRuleSet();

  base::span<const RuleData> list = rule_set.AttrRules("attr");
  ASSERT_FALSE(list.empty());
  EXPECT_FALSE(rule_set.CanIgnoreEntireList(list, "attr", "notfound"));
  EXPECT_FALSE(rule_set.CanIgnoreEntireList(list, "attr", ""));
}

TEST(RuleSetTest, LargeNumberOfAttributeRulesWithCatchAll2) {
  base::test::ScopedFeatureList feature_list;
  css_test_helpers::TestStyleSheet sheet;
  AddManyAttributeRules(feature_list, sheet);

  // This should _also_ match everything, so we cannot reject anything.
  sheet.AddCSSRules("[attr^=\"\"] {}");

  RuleSet& rule_set = sheet.GetRuleSet();

  base::span<const RuleData> list = rule_set.AttrRules("attr");
  ASSERT_FALSE(list.empty());
  EXPECT_FALSE(rule_set.CanIgnoreEntireList(list, "attr", "notfound"));
  EXPECT_FALSE(rule_set.CanIgnoreEntireList(list, "attr", ""));
}

#if DCHECK_IS_ON()  // Requires all_rules_, to find back the rules we add.

// Parse the given selector, buckets it and returns which of the constituent
// simple selectors were marked as covered by that bucketing. Note the the
// result value is stored in the order the selector is stored, which means
// that the order of the compound selectors are reversed (see comment in
// CSSSelectorParser::ConsumeComplexSelector()).
//
// A single selector may produce more than one RuleData, since visited-dependent
// rules are added to the RuleSet twice. The `rule_index` parameter can used
// to specify which of the added RuleData objects we want to produce bucket-
// coverage information from.
std::deque<bool> CoveredByBucketing(const String& selector_text,
                                    wtf_size_t rule_index = 0) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules(selector_text + " { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  const HeapVector<RuleData>& rules = rule_set.AllRulesForTest();
  EXPECT_LT(rule_index, rules.size());
  if (rule_index >= rules.size()) {
    return {};
  } else {
    const CSSSelector* selector = &rules[rule_index].Selector();

    std::deque<bool> covered;
    while (selector) {
      covered.push_back(selector->IsCoveredByBucketing());
      selector = selector->NextSimpleSelector();
    }
    return covered;
  }
}

wtf_size_t RuleCount(const String& selector_text) {
  css_test_helpers::TestStyleSheet sheet;
  sheet.AddCSSRules(selector_text + " { }");
  return sheet.GetRuleSet().AllRulesForTest().size();
}

TEST(RuleSetTest, IsCoveredByBucketing) {
  // Base cases.
  EXPECT_THAT(CoveredByBucketing(".c"), ElementsAreArray({true}));
  EXPECT_THAT(CoveredByBucketing("#id.c"), ElementsAreArray({true, false}));
  EXPECT_THAT(CoveredByBucketing(".c .c.c"),
              ElementsAreArray({true, true, false}));
  EXPECT_THAT(
      CoveredByBucketing(".a.b.c"),
      ElementsAreArray(
          {false, false, true}));  // See findBestRuleSetAndAdd_ThreeClasses.
  EXPECT_THAT(CoveredByBucketing(".c > [attr]"),
              ElementsAreArray({false, false}));
  EXPECT_THAT(CoveredByBucketing("*"), ElementsAreArray({false}));

  // Tag namespacing.
  EXPECT_THAT(CoveredByBucketing("div"), ElementsAreArray({true}));
  EXPECT_THAT(CoveredByBucketing("*|div"), ElementsAreArray({true}));
  EXPECT_THAT(
      CoveredByBucketing("@namespace ns \"http://example.org\";\nns|div"),
      ElementsAreArray({false}));
  EXPECT_THAT(CoveredByBucketing("@namespace \"http://example.org\";\ndiv"),
              ElementsAreArray({false}));

  // Attribute selectors.
  EXPECT_THAT(CoveredByBucketing("[attr]"), ElementsAreArray({false}));
  EXPECT_THAT(CoveredByBucketing("div[attr]"),
              ElementsAreArray({false, false}));

  // Link pseudo-class behavior due to visited multi-bucketing.
  EXPECT_THAT(CoveredByBucketing(":any-link"), ElementsAreArray({true}));
  EXPECT_THAT(CoveredByBucketing(":visited:link"),
              ElementsAreArray({false, false}));
  EXPECT_THAT(CoveredByBucketing(":visited:any-link"),
              ElementsAreArray({false, false}));
  EXPECT_THAT(CoveredByBucketing(":any-link:visited"),
              ElementsAreArray({false, false}));

  // The second rule added by visited-dependent selectors must not have the
  // covered-by-bucketing flag set.
  EXPECT_THAT(CoveredByBucketing(":visited", /* rule_index */ 1u),
              ElementsAreArray({false}));
  EXPECT_THAT(CoveredByBucketing(":link", /* rule_index */ 1u),
              ElementsAreArray({false}));

  // Some more pseudos.
  EXPECT_THAT(CoveredByBucketing(":focus"), ElementsAreArray({true}));
  EXPECT_THAT(CoveredByBucketing(":focus-visible"), ElementsAreArray({true}));
  EXPECT_THAT(CoveredByBucketing(":host"), ElementsAreArray({false}));
}

TEST(RuleSetTest, VisitedDependentRuleCount) {
  EXPECT_EQ(2u, RuleCount(":link"));
  EXPECT_EQ(2u, RuleCount(":visited"));
  // Not visited-dependent:
  EXPECT_EQ(1u, RuleCount("#a"));
  EXPECT_EQ(1u, RuleCount(":any-link"));
}

#endif  // DCHECK_IS_ON()

TEST(RuleSetTest, SelectorIndexLimit) {
  // It's not feasible to run this test for a large number of bits. If the
  // number of bits have increased to a large number, consider removing this
  // test and making do with RuleSetTest.RuleDataSelectorIndexLimit.
  static_assert(
      RuleData::kSelectorIndexBits == 13,
      "Please manually consider whether this test should be removed.");

  StringBuilder builder;

  // We use 13 bits to storing the selector start index in RuleData. This is a
  // test to check that we don't regress. We WONTFIX issues asking for more
  // since 2^13 simple selectors in a style rule is already excessive.
  for (unsigned i = 0; i < 8191; i++) {
    builder.Append("div,");
  }

  builder.Append("b,span {}");

  css_test_helpers::TestStyleSheet sheet;
  sheet.AddCSSRules(builder.ToString());
  const RuleSet& rule_set = sheet.GetRuleSet();
  base::span<const RuleData> rules = rule_set.TagRules("b");
  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ("b", rules.front().Selector().TagQName().LocalName());
  EXPECT_TRUE(rule_set.TagRules("span").empty());
}

TEST(RuleSetTest, RuleDataPositionLimit) {
  StyleRule* rule = CreateDummyStyleRule();
  AddRuleFlags flags = kRuleHasNoSpecialState;
  const unsigned selector_index = 0;
  const ContainerQuery* container_query = nullptr;
  const CascadeLayer* cascade_layer = nullptr;
  const StyleScope* style_scope = nullptr;

  auto* rule_set = MakeGarbageCollected<RuleSet>();
  for (int i = 0; i < (1 << RuleData::kPositionBits) + 1; ++i) {
    rule_set->AddRule(rule, selector_index, flags, container_query,
                      cascade_layer, style_scope);
  }
  EXPECT_EQ(1u << RuleData::kPositionBits, rule_set->RuleCount());
}

TEST(RuleSetTest, RuleCountNotIncreasedByInvalidRuleData) {
  auto* rule_set = MakeGarbageCollected<RuleSet>();
  EXPECT_EQ(0u, rule_set->RuleCount());

  AddRuleFlags flags = kRuleHasNoSpecialState;
  StyleRule* rule = CreateDummyStyleRule();

  // Add with valid selector_index=0.
  rule_set->AddRule(rule, 0, flags, nullptr /* container_query */,
                    nullptr /* cascade_layer */, nullptr /* scope */);
  EXPECT_EQ(1u, rule_set->RuleCount());

  // Adding with invalid selector_index should not lead to a change in count.
  rule_set->AddRule(rule, 1 << RuleData::kSelectorIndexBits, flags,
                    nullptr /* container_query */, nullptr /* cascade_layer */,
                    nullptr /* scope */);
  EXPECT_EQ(1u, rule_set->RuleCount());
}

TEST(RuleSetTest, NoStyleScope) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules("#b {}");
  RuleSet& rule_set = sheet.GetRuleSet();
  base::span<const RuleData> rules = rule_set.IdRules("b");
  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ(0u, rule_set.ScopeIntervals().size());
}

TEST(RuleSetTest, StyleScope) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules("@scope (.a) { #b {} }");
  RuleSet& rule_set = sheet.GetRuleSet();
  base::span<const RuleData> rules = rule_set.IdRules("b");
  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ(1u, rule_set.ScopeIntervals().size());
}

TEST(RuleSetTest, NestedStyleScope) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules(R"CSS(
    @scope (.a) {
      #a {}
      @scope (.b) {
        #b {}
      }
    }
  )CSS");
  RuleSet& rule_set = sheet.GetRuleSet();
  base::span<const RuleData> a_rules = rule_set.IdRules("a");
  base::span<const RuleData> b_rules = rule_set.IdRules("b");

  ASSERT_EQ(1u, a_rules.size());
  ASSERT_EQ(1u, b_rules.size());

  ASSERT_EQ(2u, rule_set.ScopeIntervals().size());

  EXPECT_EQ(a_rules.front().GetPosition(),
            rule_set.ScopeIntervals()[0].start_position);
  const StyleScope* a_rule_scope = rule_set.ScopeIntervals()[0].value;

  EXPECT_EQ(b_rules.front().GetPosition(),
            rule_set.ScopeIntervals()[1].start_position);
  const StyleScope* b_rule_scope = rule_set.ScopeIntervals()[1].value;

  EXPECT_NE(nullptr, a_rule_scope);
  EXPECT_EQ(nullptr, a_rule_scope->Parent());

  EXPECT_NE(nullptr, b_rule_scope);
  EXPECT_EQ(a_rule_scope, b_rule_scope->Parent());

  EXPECT_NE(nullptr, b_rule_scope->Parent());
  EXPECT_EQ(nullptr, b_rule_scope->Parent()->Parent());
}

class RuleSetCascadeLayerTest : public SimTest {
 public:
  using LayerName = StyleRuleBase::LayerName;

 protected:
  const RuleSet& GetRuleSet() {
    RuleSet& rule_set =
        To<HTMLStyleElement>(GetDocument().QuerySelector("style"))
            ->sheet()
            ->Contents()
            ->EnsureRuleSet(MediaQueryEvaluator(GetDocument().GetFrame()));
    rule_set.CompactRulesIfNeeded();
    return rule_set;
  }

  const CascadeLayer* GetLayerByRule(const RuleData& rule) {
    return GetRuleSet().GetLayerForTest(rule);
  }

  const CascadeLayer* GetLayerByName(const LayerName name) {
    return const_cast<CascadeLayer*>(ImplicitOuterLayer())
        ->GetOrAddSubLayer(name);
  }

  const CascadeLayer* ImplicitOuterLayer() {
    return GetRuleSet().implicit_outer_layer_;
  }

  const RuleData& GetIdRule(const AtomicString& key) {
    return GetRuleSet().IdRules(key).front();
  }

  const CascadeLayer* GetLayerByIdRule(const AtomicString& key) {
    return GetLayerByRule(GetIdRule(key));
  }

  String LayersToString() {
    return GetRuleSet().CascadeLayers().ToStringForTesting();
  }
};

TEST_F(RuleSetCascadeLayerTest, NoLayer) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      #no-layers { }
    </style>
  )HTML");

  EXPECT_FALSE(GetRuleSet().HasCascadeLayers());
  EXPECT_FALSE(ImplicitOuterLayer());
}

TEST_F(RuleSetCascadeLayerTest, Basic) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      #zero { }
      @layer foo {
        #one { }
        #two { }
        @layer bar {
          #three { }
          #four { }
        }
        #five { }
      }
      #six { }
    </style>
  )HTML");

  EXPECT_EQ("foo,foo.bar", LayersToString());

  EXPECT_EQ(ImplicitOuterLayer(), GetLayerByIdRule("zero"));
  EXPECT_EQ(GetLayerByName(LayerName({"foo"})), GetLayerByIdRule("one"));
  EXPECT_EQ(GetLayerByName(LayerName({"foo"})), GetLayerByIdRule("two"));
  EXPECT_EQ(GetLayerByName(LayerName({"foo", "bar"})),
            GetLayerByIdRule("three"));
  EXPECT_EQ(GetLayerByName(LayerName({"foo", "bar"})),
            GetLayerByIdRule("four"));
  EXPECT_EQ(GetLayerByName(LayerName({"foo"})), GetLayerByIdRule("five"));
  EXPECT_EQ(ImplicitOuterLayer(), GetLayerByIdRule("six"));
}

TEST_F(RuleSetCascadeLayerTest, NestingAndFlatListName) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      @layer foo {
        @layer bar {
          #zero { }
          #one { }
        }
      }
      @layer foo.bar {
        #two { }
        #three { }
      }
    </style>
  )HTML");

  EXPECT_EQ("foo,foo.bar", LayersToString());

  EXPECT_EQ(GetLayerByName(LayerName({"foo", "bar"})),
            GetLayerByIdRule("zero"));
  EXPECT_EQ(GetLayerByName(LayerName({"foo", "bar"})), GetLayerByIdRule("one"));
  EXPECT_EQ(GetLayerByName(LayerName({"foo", "bar"})), GetLayerByIdRule("two"));
  EXPECT_EQ(GetLayerByName(LayerName({"foo", "bar"})),
            GetLayerByIdRule("three"));
}

TEST_F(RuleSetCascadeLayerTest, LayerStatementOrdering) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      @layer foo, bar, foo.baz;
      @layer bar {
        #zero { }
      }
      @layer foo {
        #one { }
        @layer baz {
          #two { }
        }
      }
    </style>
  )HTML");

  EXPECT_EQ("foo,foo.baz,bar", LayersToString());

  EXPECT_EQ(GetLayerByName(LayerName({"bar"})), GetLayerByIdRule("zero"));
  EXPECT_EQ(GetLayerByName(LayerName({"foo"})), GetLayerByIdRule("one"));
  EXPECT_EQ(GetLayerByName(LayerName({"foo", "baz"})), GetLayerByIdRule("two"));
}

TEST_F(RuleSetCascadeLayerTest, LayeredImport) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest sub_resource("https://example.com/sheet.css",
                                     "text/css");

  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      @import url(/sheet.css) layer(foo);
      @layer foo.bar {
        #two { }
        #three { }
      }
    </style>
  )HTML");
  sub_resource.Complete(R"CSS(
    #zero { }
    @layer bar {
      #one { }
    }
  )CSS");

  test::RunPendingTasks();

  EXPECT_EQ(GetLayerByName(LayerName({"foo"})), GetLayerByIdRule("zero"));
  EXPECT_EQ(GetLayerByName(LayerName({"foo", "bar"})), GetLayerByIdRule("one"));
  EXPECT_EQ(GetLayerByName(LayerName({"foo", "bar"})), GetLayerByIdRule("two"));
  EXPECT_EQ(GetLayerByName(LayerName({"foo", "bar"})),
            GetLayerByIdRule("three"));
}

TEST_F(RuleSetCascadeLayerTest, LayerStatementsBeforeAndAfterImport) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest sub_resource("https://example.com/sheet.css",
                                     "text/css");

  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      @layer foo, bar;
      @import url(/sheet.css) layer(bar);
      @layer baz, bar, foo;
      @layer foo {
        #two { }
        #three { }
      }
      @layer baz {
        #four { }
      }
    </style>
  )HTML");
  sub_resource.Complete(R"CSS(
    #zero { }
    #one { }
  )CSS");

  test::RunPendingTasks();

  EXPECT_EQ("foo,bar,baz", LayersToString());

  EXPECT_EQ(GetLayerByName(LayerName({"bar"})), GetLayerByIdRule("zero"));
  EXPECT_EQ(GetLayerByName(LayerName({"bar"})), GetLayerByIdRule("one"));
  EXPECT_EQ(GetLayerByName(LayerName({"foo"})), GetLayerByIdRule("two"));
  EXPECT_EQ(GetLayerByName(LayerName({"foo"})), GetLayerByIdRule("three"));
  EXPECT_EQ(GetLayerByName(LayerName({"baz"})), GetLayerByIdRule("four"));
}

}  // namespace blink
