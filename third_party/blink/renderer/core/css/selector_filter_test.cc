// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/selector_filter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_scope.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class SelectorFilterTest : public PageTestBase {};

namespace {

Vector<unsigned> CollectIdentifierHashesFromInnerRule(Document& document,
                                                      String rule_text) {
  Vector<unsigned> result(4);
  const auto* outer_rule = DynamicTo<StyleRuleGroup>(
      css_test_helpers::ParseRule(document, rule_text));
  CHECK(outer_rule);
  CHECK_EQ(1u, outer_rule->ChildRules().size());

  const auto* inner_style_rule =
      DynamicTo<StyleRule>(outer_rule->ChildRules()[0].Get());
  CHECK(inner_style_rule);
  CHECK(inner_style_rule->FirstSelector());

  SelectorFilter::CollectIdentifierHashes(*inner_style_rule->FirstSelector(),
                                          result.data(), result.size());
  return result;
}

}  // namespace

TEST_F(SelectorFilterTest, CollectHashesScopeSubject) {
  Vector<unsigned> hashes = CollectIdentifierHashesFromInnerRule(GetDocument(),
                                                                 R"CSS(
    @scope (.a) {
      .b.c .d:scope {
        color: green;
      }
    }
  )CSS");

  ASSERT_EQ(4u, hashes.size());
  EXPECT_NE(0u, hashes[0]);  // .b
  EXPECT_NE(0u, hashes[1]);  // .c
  EXPECT_EQ(0u, hashes[2]);
  EXPECT_EQ(0u, hashes[3]);
}

TEST_F(SelectorFilterTest, CollectHashesScopeNonSubject) {
  Vector<unsigned> hashes = CollectIdentifierHashesFromInnerRule(GetDocument(),
                                                                 R"CSS(
    @scope (.a) {
      .b.c:scope .d {
        color: green;
      }
    }
  )CSS");

  ASSERT_EQ(4u, hashes.size());
  EXPECT_NE(0u, hashes[0]);  // .b
  EXPECT_NE(0u, hashes[1]);  // .c
  EXPECT_EQ(0u, hashes[2]);
  EXPECT_EQ(0u, hashes[3]);
}

TEST_F(SelectorFilterTest, CollectHashesScopeImplied) {
  Vector<unsigned> hashes = CollectIdentifierHashesFromInnerRule(GetDocument(),
                                                                 R"CSS(
    @scope (.a) {
      .b.c .d {
        color: green;
      }
      /* Note that the above is equivalent to ":scope .b.c .d". */
    }
  )CSS");

  ASSERT_EQ(4u, hashes.size());
  EXPECT_NE(0u, hashes[0]);  // .b
  EXPECT_NE(0u, hashes[1]);  // .c
  EXPECT_EQ(0u, hashes[2]);
  EXPECT_EQ(0u, hashes[3]);
}

}  // namespace blink
