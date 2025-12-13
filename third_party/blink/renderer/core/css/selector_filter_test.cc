// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/selector_filter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_scope.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class SelectorFilterTest : public PageTestBase {};

namespace {

Vector<uint16_t> CollectIdentifierHashesFromInnerRule(Document& document,
                                                      String rule_text) {
  Vector<uint16_t> result;
  const auto* outer_rule = DynamicTo<StyleRuleGroup>(
      css_test_helpers::ParseRule(document, rule_text));
  CHECK(outer_rule);
  CHECK_EQ(1u, outer_rule->ChildRules().size());

  const auto* inner_style_rule =
      DynamicTo<StyleRule>(outer_rule->ChildRules()[0].Get());
  CHECK(inner_style_rule);
  CHECK(inner_style_rule->FirstSelector());

  const auto* scope_rule = DynamicTo<StyleRuleScope>(outer_rule);
  const StyleScope* style_scope =
      scope_rule ? &scope_rule->GetStyleScope() : nullptr;

  Element::TinyBloomFilter subject_filter;
  SelectorFilter::CollectIdentifierHashes(*inner_style_rule->FirstSelector(),
                                          style_scope, result, subject_filter);
  return result;
}

}  // namespace

TEST_F(SelectorFilterTest, CollectHashesScopeSubject) {
  Vector<uint16_t> hashes = CollectIdentifierHashesFromInnerRule(GetDocument(),
                                                                 R"CSS(
    @scope (.a) {
      .b.c .d:scope {
        color: green;
      }
    }
  )CSS");

  ASSERT_EQ(2u, hashes.size());
  EXPECT_NE(0u, hashes[0]);  // .b
  EXPECT_NE(0u, hashes[1]);  // .c
}

TEST_F(SelectorFilterTest, CollectHashesScopeNonSubject) {
  Vector<uint16_t> hashes = CollectIdentifierHashesFromInnerRule(GetDocument(),
                                                                 R"CSS(
    @scope (.a) {
      .b.c:scope .d {
        color: green;
      }
    }
  )CSS");

  ASSERT_EQ(3u, hashes.size());
  EXPECT_NE(0u, hashes[0]);  // .b
  EXPECT_NE(0u, hashes[1]);  // .c
  EXPECT_NE(0u, hashes[2]);  // .a
}

TEST_F(SelectorFilterTest, CollectHashesScopeImplied) {
  Vector<uint16_t> hashes = CollectIdentifierHashesFromInnerRule(GetDocument(),
                                                                 R"CSS(
    @scope (.a) {
      .b.c .d {
        color: green;
      }
      /* Note that the above is equivalent to ":scope .b.c .d". */
    }
  )CSS");

  ASSERT_EQ(3u, hashes.size());
  EXPECT_NE(0u, hashes[0]);  // .b
  EXPECT_NE(0u, hashes[1]);  // .c
  EXPECT_NE(0u, hashes[2]);  // .a
}

Element::TinyBloomFilter SubjectFilterForSelector(const char* selector) {
  HeapVector<CSSSelector> arena;
  base::span<CSSSelector> selector_vector = CSSParser::ParseSelector(
      MakeGarbageCollected<CSSParserContext>(
          kHTMLStandardMode, SecureContextMode::kInsecureContext),
      CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr, nullptr,
      selector, arena);
  CSSSelectorList* selectors =
      CSSSelectorList::AdoptSelectorVector(selector_vector);

  Element::TinyBloomFilter subject_filter;

  Vector<uint16_t> selector_hashes;
  SelectorFilter::CollectIdentifierHashes(*selectors->First(),
                                          /* style_scope */ nullptr,
                                          selector_hashes, subject_filter);
  return subject_filter;
}

TEST_F(SelectorFilterTest, UniversalSelectorHasNoSubjectBits) {
  EXPECT_EQ(SubjectFilterForSelector("*"), 0u);
  EXPECT_EQ(SubjectFilterForSelector("div *"), 0u);
  EXPECT_EQ(SubjectFilterForSelector("div > *"), 0u);
}

TEST_F(SelectorFilterTest, ClassHasSubjectBits) {
  EXPECT_NE(SubjectFilterForSelector(".a"), 0u);
  EXPECT_EQ(SubjectFilterForSelector(".a"), SubjectFilterForSelector("div .a"));
  EXPECT_EQ(SubjectFilterForSelector(".a"),
            SubjectFilterForSelector("div > .a"));
  EXPECT_EQ(SubjectFilterForSelector(".a"),
            SubjectFilterForSelector("div + .a"));
}

TEST_F(SelectorFilterTest, AttributeHasSubjectBits) {
  EXPECT_NE(SubjectFilterForSelector("[a]"), 0u);
  // Different due to uppercasing of attributes. (The odds of being equal
  // by accident are very low.)
  EXPECT_NE(SubjectFilterForSelector("[a]"), SubjectFilterForSelector(".a"));
}

TEST_F(SelectorFilterTest, MultipleClassesShouldBeStrictSupersetOfSingle) {
  EXPECT_NE(SubjectFilterForSelector(".a.b"), SubjectFilterForSelector(".a"));
  EXPECT_EQ(SubjectFilterForSelector(".a.b") & SubjectFilterForSelector(".a"),
            SubjectFilterForSelector(".a"));
}

TEST_F(SelectorFilterTest, IsAndWhere) {
  EXPECT_EQ(SubjectFilterForSelector(":is(.a)"),
            SubjectFilterForSelector(".a"));
  EXPECT_EQ(SubjectFilterForSelector(":is(.a.b)"),
            SubjectFilterForSelector(".a.b"));
  EXPECT_EQ(SubjectFilterForSelector(".a:is(.b)"),
            SubjectFilterForSelector(".a.b"));
  EXPECT_EQ(SubjectFilterForSelector(":where(.a)"),
            SubjectFilterForSelector(".a"));
  EXPECT_EQ(SubjectFilterForSelector(":has(.a)"), 0u);
  EXPECT_EQ(SubjectFilterForSelector(":is(.a,.b)"), 0u);
}

TEST_F(SelectorFilterTest, IsWithMultipleParameters) {
  EXPECT_EQ(SubjectFilterForSelector(":is(.a, #a)"), 0);

  EXPECT_EQ(SubjectFilterForSelector(":is(.a, .a)"),
            SubjectFilterForSelector(".a"));

  // Deliberately invoke a collision (with high probability),
  // despite none of the entries actually matching.
  EXPECT_NE(
      SubjectFilterForSelector(":is(.a1.b1.c1.d1.e1.f1.g1.h1.i1.j1.k1.l1.m1.n1,"
                               " .a2.b2.c2.d2.e2.f2.g2.h2.i2.j2.k2.l2.m2.n2)"),
      0);
}

}  // namespace blink
