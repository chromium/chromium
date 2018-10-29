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

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_test_helper.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

StyleRule* CreateDummyStyleRule() {
  CSSTestHelper helper;
  helper.AddCSSRules("#id { color: tomato; }");
  const RuleSet& rule_set = helper.GetRuleSet();
  const HeapVector<Member<const RuleData>>* rules = rule_set.IdRules("id");
  DCHECK_EQ(1u, rules->size());
  return rules->at(0)->Rule();
}

}  // namespace

TEST(RuleSetTest, findBestRuleSetAndAdd_CustomPseudoElements) {
  CSSTestHelper helper;

  helper.AddCSSRules("summary::-webkit-details-marker { }");
  RuleSet& rule_set = helper.GetRuleSet();
  AtomicString str("-webkit-details-marker");
  const HeapVector<Member<const RuleData>>* rules =
      rule_set.ShadowPseudoElementRules(str);
  ASSERT_EQ(1u, rules->size());
  ASSERT_EQ(str, rules->at(0)->Selector().Value());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_Id) {
  CSSTestHelper helper;

  helper.AddCSSRules("#id { }");
  RuleSet& rule_set = helper.GetRuleSet();
  AtomicString str("id");
  const HeapVector<Member<const RuleData>>* rules = rule_set.IdRules(str);
  ASSERT_EQ(1u, rules->size());
  ASSERT_EQ(str, rules->at(0)->Selector().Value());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_NthChild) {
  CSSTestHelper helper;

  helper.AddCSSRules("div:nth-child(2) { }");
  RuleSet& rule_set = helper.GetRuleSet();
  AtomicString str("div");
  const HeapVector<Member<const RuleData>>* rules = rule_set.TagRules(str);
  ASSERT_EQ(1u, rules->size());
  ASSERT_EQ(str, rules->at(0)->Selector().TagQName().LocalName());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_ClassThenId) {
  CSSTestHelper helper;

  helper.AddCSSRules(".class#id { }");
  RuleSet& rule_set = helper.GetRuleSet();
  AtomicString str("id");
  // id is prefered over class even if class preceeds it in the selector.
  const HeapVector<Member<const RuleData>>* rules = rule_set.IdRules(str);
  ASSERT_EQ(1u, rules->size());
  AtomicString class_str("class");
  ASSERT_EQ(class_str, rules->at(0)->Selector().Value());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_IdThenClass) {
  CSSTestHelper helper;

  helper.AddCSSRules("#id.class { }");
  RuleSet& rule_set = helper.GetRuleSet();
  AtomicString str("id");
  const HeapVector<Member<const RuleData>>* rules = rule_set.IdRules(str);
  ASSERT_EQ(1u, rules->size());
  ASSERT_EQ(str, rules->at(0)->Selector().Value());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_AttrThenId) {
  CSSTestHelper helper;

  helper.AddCSSRules("[attr]#id { }");
  RuleSet& rule_set = helper.GetRuleSet();
  AtomicString str("id");
  const HeapVector<Member<const RuleData>>* rules = rule_set.IdRules(str);
  ASSERT_EQ(1u, rules->size());
  AtomicString attr_str("attr");
  ASSERT_EQ(attr_str, rules->at(0)->Selector().Attribute().LocalName());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_TagThenAttrThenId) {
  CSSTestHelper helper;

  helper.AddCSSRules("div[attr]#id { }");
  RuleSet& rule_set = helper.GetRuleSet();
  AtomicString str("id");
  const HeapVector<Member<const RuleData>>* rules = rule_set.IdRules(str);
  ASSERT_EQ(1u, rules->size());
  AtomicString tag_str("div");
  ASSERT_EQ(tag_str, rules->at(0)->Selector().TagQName().LocalName());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_DivWithContent) {
  CSSTestHelper helper;

  helper.AddCSSRules("div::content { }");
  RuleSet& rule_set = helper.GetRuleSet();
  AtomicString str("div");
  const HeapVector<Member<const RuleData>>* rules = rule_set.TagRules(str);
  ASSERT_EQ(1u, rules->size());
  AtomicString value_str("content");
  ASSERT_EQ(value_str, rules->at(0)->Selector().TagHistory()->Value());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_Host) {
  CSSTestHelper helper;

  helper.AddCSSRules(":host { }");
  RuleSet& rule_set = helper.GetRuleSet();
  const HeapVector<Member<const RuleData>>* rules = rule_set.ShadowHostRules();
  ASSERT_EQ(1u, rules->size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_HostWithId) {
  CSSTestHelper helper;

  helper.AddCSSRules(":host(#x) { }");
  RuleSet& rule_set = helper.GetRuleSet();
  const HeapVector<Member<const RuleData>>* rules = rule_set.ShadowHostRules();
  ASSERT_EQ(1u, rules->size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_HostContext) {
  CSSTestHelper helper;

  helper.AddCSSRules(":host-context(*) { }");
  RuleSet& rule_set = helper.GetRuleSet();
  const HeapVector<Member<const RuleData>>* rules = rule_set.ShadowHostRules();
  ASSERT_EQ(1u, rules->size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_HostContextWithId) {
  CSSTestHelper helper;

  helper.AddCSSRules(":host-context(#x) { }");
  RuleSet& rule_set = helper.GetRuleSet();
  const HeapVector<Member<const RuleData>>* rules = rule_set.ShadowHostRules();
  ASSERT_EQ(1u, rules->size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_HostAndHostContextNotInRightmost) {
  CSSTestHelper helper;

  helper.AddCSSRules(":host-context(#x) .y, :host(.a) > #b  { }");
  RuleSet& rule_set = helper.GetRuleSet();
  const HeapVector<Member<const RuleData>>* shadow_rules =
      rule_set.ShadowHostRules();
  const HeapVector<Member<const RuleData>>* id_rules = rule_set.IdRules("b");
  const HeapVector<Member<const RuleData>>* class_rules =
      rule_set.ClassRules("y");
  ASSERT_EQ(0u, shadow_rules->size());
  ASSERT_EQ(1u, id_rules->size());
  ASSERT_EQ(1u, class_rules->size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_HostAndClass) {
  CSSTestHelper helper;

  helper.AddCSSRules(".foo:host { }");
  RuleSet& rule_set = helper.GetRuleSet();
  const HeapVector<Member<const RuleData>>* rules = rule_set.ShadowHostRules();
  ASSERT_EQ(0u, rules->size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_HostContextAndClass) {
  CSSTestHelper helper;

  helper.AddCSSRules(".foo:host-context(*) { }");
  RuleSet& rule_set = helper.GetRuleSet();
  const HeapVector<Member<const RuleData>>* rules = rule_set.ShadowHostRules();
  ASSERT_EQ(0u, rules->size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_Focus) {
  CSSTestHelper helper;

  helper.AddCSSRules(":focus { }");
  helper.AddCSSRules("[attr]:focus { }");
  RuleSet& rule_set = helper.GetRuleSet();
  const HeapVector<Member<const RuleData>>* rules =
      rule_set.FocusPseudoClassRules();
  ASSERT_EQ(2u, rules->size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_LinkVisited) {
  CSSTestHelper helper;

  helper.AddCSSRules(":link { }");
  helper.AddCSSRules("[attr]:link { }");
  helper.AddCSSRules(":visited { }");
  helper.AddCSSRules("[attr]:visited { }");
  helper.AddCSSRules(":-webkit-any-link { }");
  helper.AddCSSRules("[attr]:-webkit-any-link { }");
  RuleSet& rule_set = helper.GetRuleSet();
  const HeapVector<Member<const RuleData>>* rules =
      rule_set.LinkPseudoClassRules();
  ASSERT_EQ(6u, rules->size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_Cue) {
  CSSTestHelper helper;

  helper.AddCSSRules("::cue(b) { }");
  helper.AddCSSRules("video::cue(u) { }");
  RuleSet& rule_set = helper.GetRuleSet();
  const HeapVector<Member<const RuleData>>* rules = rule_set.CuePseudoRules();
  ASSERT_EQ(2u, rules->size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_PlaceholderPseudo) {
  CSSTestHelper helper;

  helper.AddCSSRules("::placeholder { }");
  helper.AddCSSRules("input::placeholder { }");
  RuleSet& rule_set = helper.GetRuleSet();
  auto* rules = rule_set.ShadowPseudoElementRules("-webkit-input-placeholder");
  ASSERT_EQ(2u, rules->size());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_PseudoMatches) {
  CSSTestHelper helper;

  helper.AddCSSRules(".a :matches(.b+.c, .d>:matches(.e, .f)) { }");
  RuleSet& rule_set = helper.GetRuleSet();
  {
    AtomicString str("c");
    const HeapVector<Member<const RuleData>>* rules = rule_set.ClassRules(str);
    ASSERT_EQ(1u, rules->size());
    ASSERT_EQ(str, rules->at(0)->Selector().Value());
  }
  {
    AtomicString str("e");
    const HeapVector<Member<const RuleData>>* rules = rule_set.ClassRules(str);
    ASSERT_EQ(1u, rules->size());
    ASSERT_EQ(str, rules->at(0)->Selector().Value());
  }
  {
    AtomicString str("f");
    const HeapVector<Member<const RuleData>>* rules = rule_set.ClassRules(str);
    ASSERT_EQ(1u, rules->size());
    ASSERT_EQ(str, rules->at(0)->Selector().Value());
  }
}

TEST(RuleSetTest, findBestRuleSetAndAdd_PseudoWhere) {
  CSSTestHelper helper;

  helper.AddCSSRules(".a :where(.b+.c, .d>:where(.e, .f)) { }");
  RuleSet& rule_set = helper.GetRuleSet();
  {
    AtomicString str("c");
    const HeapVector<Member<const RuleData>>* rules = rule_set.ClassRules(str);
    ASSERT_EQ(1u, rules->size());
    ASSERT_EQ(str, rules->at(0)->Selector().Value());
  }
  {
    AtomicString str("e");
    const HeapVector<Member<const RuleData>>* rules = rule_set.ClassRules(str);
    ASSERT_EQ(1u, rules->size());
    ASSERT_EQ(str, rules->at(0)->Selector().Value());
  }
  {
    AtomicString str("f");
    const HeapVector<Member<const RuleData>>* rules = rule_set.ClassRules(str);
    ASSERT_EQ(1u, rules->size());
    ASSERT_EQ(str, rules->at(0)->Selector().Value());
  }
}

TEST(RuleSetTest, findBestRuleSetAndAdd_PseudoMatchesTooLarge) {
  // RuleData cannot support selectors at index 8192 or beyond so the expansion
  // is limited to this size
  CSSTestHelper helper;

  helper.AddCSSRules(
      ":matches(.a#a, .b#b, .c#c, .d#d) + "
      ":matches(.e#e, .f#f, .g#g, .h#h) + "
      ":matches(.i#i, .j#j, .k#k, .l#l) + "
      ":matches(.m#m, .n#n, .o#o, .p#p) + "
      ":matches(.q#q, .r#r, .s#s, .t#t) + "
      ":matches(.u#u, .v#v, .w#w, .x#x) { }",
      true);

  RuleSet& rule_set = helper.GetRuleSet();
  ASSERT_EQ(0u, rule_set.RuleCount());
}

TEST(RuleSetTest, findBestRuleSetAndAdd_PseudoWhereTooLarge) {
  // RuleData cannot support selectors at index 8192 or beyond so the expansion
  // is limited to this size
  CSSTestHelper helper;

  helper.AddCSSRules(
      ":where(.a#a, .b#b, .c#c, .d#d) + :where(.e#e, .f#f, .g#g, .h#h) + "
      ":where(.i#i, .j#j, .k#k, .l#l) + :where(.m#m, .n#n, .o#o, .p#p) + "
      ":where(.q#q, .r#r, .s#s, .t#t) + :where(.u#u, .v#v, .w#w, .x#x) { }",
      true);

  RuleSet& rule_set = helper.GetRuleSet();
  ASSERT_EQ(0u, rule_set.RuleCount());
}

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
  for (unsigned i = 0; i < 8191; i++)
    builder.Append("div,");

  builder.Append("b,span {}");

  CSSTestHelper helper;
  helper.AddCSSRules(builder.ToString().Ascii().data());
  const RuleSet& rule_set = helper.GetRuleSet();
  const HeapVector<Member<const RuleData>>* rules = rule_set.TagRules("b");
  ASSERT_EQ(1u, rules->size());
  EXPECT_EQ("b", rules->at(0)->Selector().TagQName().LocalName());
  EXPECT_FALSE(rule_set.TagRules("span"));
}

TEST(RuleSetTest, RuleDataSelectorIndexLimit) {
  StyleRule* rule = CreateDummyStyleRule();
  AddRuleFlags flags = kRuleHasNoSpecialState;
  const unsigned position = 0;
  EXPECT_TRUE(RuleData::MaybeCreate(rule, 0, position, flags));
  EXPECT_FALSE(RuleData::MaybeCreate(rule, (1 << RuleData::kSelectorIndexBits),
                                     position, flags));
  EXPECT_FALSE(RuleData::MaybeCreate(
      rule, (1 << RuleData::kSelectorIndexBits) + 1, position, flags));
}

TEST(RuleSetTest, RuleDataPositionLimit) {
  StyleRule* rule = CreateDummyStyleRule();
  AddRuleFlags flags = kRuleHasNoSpecialState;
  const unsigned selector_index = 0;
  EXPECT_TRUE(RuleData::MaybeCreate(rule, selector_index, 0, flags));
  EXPECT_FALSE(RuleData::MaybeCreate(rule, selector_index,
                                     (1 << RuleData::kPositionBits), flags));
  EXPECT_FALSE(RuleData::MaybeCreate(
      rule, selector_index, (1 << RuleData::kPositionBits) + 1, flags));
}

TEST(RuleSetTest, RuleCountNotIncreasedByInvalidRuleData) {
  RuleSet* rule_set = RuleSet::Create();
  EXPECT_EQ(0u, rule_set->RuleCount());

  AddRuleFlags flags = kRuleHasNoSpecialState;
  StyleRule* rule = CreateDummyStyleRule();

  // Add with valid selector_index=0.
  rule_set->AddRule(rule, 0, flags);
  EXPECT_EQ(1u, rule_set->RuleCount());

  // Adding with invalid selector_index should not lead to a change in count.
  rule_set->AddRule(rule, 1 << RuleData::kSelectorIndexBits, flags);
  EXPECT_EQ(1u, rule_set->RuleCount());
}

}  // namespace blink
