// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/has_argument_match_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"

namespace blink {

namespace {

const int kMax = std::numeric_limits<int>::max();

void RunTest(const String& selector_text,
             CSSSelector::RelationType expected_leftmost_relation,
             int expected_adjacent_distance_limit,
             int expected_depth_limit,
             HasArgumentMatchTraversalScope expected_traversal_scope) {
  CSSSelectorList selector_list =
      css_test_helpers::ParseSelectorList(selector_text);
  HasArgumentMatchContext context(
      selector_list.First()->SelectorList()->First());

  EXPECT_EQ(expected_leftmost_relation, context.LeftmostRelation())
      << "Failed : " << selector_text;
  EXPECT_EQ(expected_adjacent_distance_limit, context.AdjacentDistanceLimit())
      << "Failed : " << selector_text;
  EXPECT_EQ(expected_depth_limit, context.DepthLimit())
      << "Failed : " << selector_text;
  EXPECT_EQ(expected_traversal_scope, context.TraversalScope())
      << "Failed : " << selector_text;
}

}  // namespace

TEST(HasArgumentMatchContextTest, TestArgumentMatchContext) {
  RunTest(":has(.a)", CSSSelector::kRelativeDescendant,
          /* expected_adjacent_distance_limit */ 0,
          /* expected_depth_limit */ kMax, kSubtree);
  RunTest(":has(.a ~ .b)", CSSSelector::kRelativeDescendant,
          /* expected_adjacent_distance_limit */ 0,
          /* expected_depth_limit */ kMax, kSubtree);
  RunTest(":has(.a ~ .b > .c)", CSSSelector::kRelativeDescendant,
          /* expected_adjacent_distance_limit */ 0,
          /* expected_depth_limit */ kMax, kSubtree);
  RunTest(":has(.a > .b)", CSSSelector::kRelativeDescendant,
          /* expected_adjacent_distance_limit */ 0,
          /* expected_depth_limit */ kMax, kSubtree);
  RunTest(":has(.a + .b)", CSSSelector::kRelativeDescendant,
          /* expected_adjacent_distance_limit */ 0,
          /* expected_depth_limit */ kMax, kSubtree);
  RunTest(":has(> .a .b)", CSSSelector::kRelativeChild,
          /* expected_adjacent_distance_limit */ 0,
          /* expected_depth_limit */ kMax, kSubtree);
  RunTest(":has(> .a ~ .b .c)", CSSSelector::kRelativeChild,
          /* expected_adjacent_distance_limit */ 0,
          /* expected_depth_limit */ kMax, kSubtree);
  RunTest(":has(> .a + .b .c)", CSSSelector::kRelativeChild,
          /* expected_adjacent_distance_limit */ 0,
          /* expected_depth_limit */ kMax, kSubtree);
  RunTest(":has(> .a)", CSSSelector::kRelativeChild,
          /* expected_adjacent_distance_limit */ 0,
          /* expected_depth_limit */ 1, kFixedDepthDescendants);
  RunTest(":has(> .a > .b)", CSSSelector::kRelativeChild,
          /* expected_adjacent_distance_limit */ 0,
          /* expected_depth_limit */ 2, kFixedDepthDescendants);
  RunTest(":has(> .a + .b)", CSSSelector::kRelativeChild,
          /* expected_adjacent_distance_limit */ 0,
          /* expected_depth_limit */ 1, kFixedDepthDescendants);
  RunTest(":has(> .a ~ .b)", CSSSelector::kRelativeChild,
          /* expected_adjacent_distance_limit */ 0,
          /* expected_depth_limit */ 1, kFixedDepthDescendants);
  RunTest(":has(> .a ~ .b > .c)", CSSSelector::kRelativeChild,
          /* expected_adjacent_distance_limit */ 0,
          /* expected_depth_limit */ 2, kFixedDepthDescendants);
  RunTest(":has(~ .a .b)", CSSSelector::kRelativeIndirectAdjacent,
          /* expected_adjacent_distance_limit */ kMax,
          /* expected_depth_limit */ kMax, kAllNextSiblingSubtrees);
  RunTest(":has(~ .a + .b > .c ~ .d .e)",
          CSSSelector::kRelativeIndirectAdjacent,
          /* expected_adjacent_distance_limit */ kMax,
          /* expected_depth_limit */ kMax, kAllNextSiblingSubtrees);
  RunTest(":has(~ .a)", CSSSelector::kRelativeIndirectAdjacent,
          /* expected_adjacent_distance_limit */ kMax,
          /* expected_depth_limit */ 0, kAllNextSiblings);
  RunTest(":has(~ .a ~ .b)", CSSSelector::kRelativeIndirectAdjacent,
          /* expected_adjacent_distance_limit */ kMax,
          /* expected_depth_limit */ 0, kAllNextSiblings);
  RunTest(":has(~ .a + .b)", CSSSelector::kRelativeIndirectAdjacent,
          /* expected_adjacent_distance_limit */ kMax,
          /* expected_depth_limit */ 0, kAllNextSiblings);
  RunTest(":has(~ .a + .b ~ .c)", CSSSelector::kRelativeIndirectAdjacent,
          /* expected_adjacent_distance_limit */ kMax,
          /* expected_depth_limit */ 0, kAllNextSiblings);
  RunTest(":has(~ .a > .b)", CSSSelector::kRelativeIndirectAdjacent,
          /* expected_adjacent_distance_limit */ kMax,
          /* expected_depth_limit */ 1, kAllNextSiblingsFixedDepthDescendants);
  RunTest(":has(~ .a + .b > .c ~ .d > .e)",
          CSSSelector::kRelativeIndirectAdjacent,
          /* expected_adjacent_distance_limit */ kMax,
          /* expected_depth_limit */ 2, kAllNextSiblingsFixedDepthDescendants);
  RunTest(":has(+ .a ~ .b .c)", CSSSelector::kRelativeDirectAdjacent,
          /* expected_adjacent_distance_limit */ kMax,
          /* expected_depth_limit */ kMax, kAllNextSiblingSubtrees);
  RunTest(":has(+ .a ~ .b > .c + .d .e)", CSSSelector::kRelativeDirectAdjacent,
          /* expected_adjacent_distance_limit */ kMax,
          /* expected_depth_limit */ kMax, kAllNextSiblingSubtrees);
  RunTest(":has(+ .a ~ .b)", CSSSelector::kRelativeDirectAdjacent,
          /* expected_adjacent_distance_limit */ kMax,
          /* expected_depth_limit */ 0, kAllNextSiblings);
  RunTest(":has(+ .a + .b ~ .c)", CSSSelector::kRelativeDirectAdjacent,
          /* expected_adjacent_distance_limit */ kMax,
          /* expected_depth_limit */ 0, kAllNextSiblings);
  RunTest(":has(+ .a ~ .b > .c)", CSSSelector::kRelativeDirectAdjacent,
          /* expected_adjacent_distance_limit */ kMax,
          /* expected_depth_limit */ 1, kAllNextSiblingsFixedDepthDescendants);
  RunTest(":has(+ .a ~ .b > .c + .d > .e)",
          CSSSelector::kRelativeDirectAdjacent,
          /* expected_adjacent_distance_limit */ kMax,
          /* expected_depth_limit */ 2, kAllNextSiblingsFixedDepthDescendants);
  RunTest(":has(+ .a .b)", CSSSelector::kRelativeDirectAdjacent,
          /* expected_adjacent_distance_limit */ 1,
          /* expected_depth_limit */ kMax, kOneNextSiblingSubtree);
  RunTest(":has(+ .a > .b .c)", CSSSelector::kRelativeDirectAdjacent,
          /* expected_adjacent_distance_limit */ 1,
          /* expected_depth_limit */ kMax, kOneNextSiblingSubtree);
  RunTest(":has(+ .a .b > .c)", CSSSelector::kRelativeDirectAdjacent,
          /* expected_adjacent_distance_limit */ 1,
          /* expected_depth_limit */ kMax, kOneNextSiblingSubtree);
  RunTest(":has(+ .a .b ~ .c)", CSSSelector::kRelativeDirectAdjacent,
          /* expected_adjacent_distance_limit */ 1,
          /* expected_depth_limit */ kMax, kOneNextSiblingSubtree);
  RunTest(":has(+ .a + .b .c)", CSSSelector::kRelativeDirectAdjacent,
          /* expected_adjacent_distance_limit */ 2,
          /* expected_depth_limit */ kMax, kOneNextSiblingSubtree);
  RunTest(":has(+ .a > .b + .c .d)", CSSSelector::kRelativeDirectAdjacent,
          /* expected_adjacent_distance_limit */ 1,
          /* expected_depth_limit */ kMax, kOneNextSiblingSubtree);
  RunTest(":has(+ .a + .b > .c .d)", CSSSelector::kRelativeDirectAdjacent,
          /* expected_adjacent_distance_limit */ 2,
          /* expected_depth_limit */ kMax, kOneNextSiblingSubtree);
  RunTest(":has(+ .a)", CSSSelector::kRelativeDirectAdjacent,
          /* expected_adjacent_distance_limit */ 1,
          /* expected_depth_limit */ 0, kOneNextSibling);
  RunTest(":has(+ .a + .b)", CSSSelector::kRelativeDirectAdjacent,
          /* expected_adjacent_distance_limit */ 2,
          /* expected_depth_limit */ 0, kOneNextSibling);
  RunTest(":has(+ .a + .b + .c)", CSSSelector::kRelativeDirectAdjacent,
          /* expected_adjacent_distance_limit */ 3,
          /* expected_depth_limit */ 0, kOneNextSibling);
  RunTest(":has(+ .a > .b)", CSSSelector::kRelativeDirectAdjacent,
          /* expected_adjacent_distance_limit */ 1,
          /* expected_depth_limit */ 1, kOneNextSiblingFixedDepthDescendants);
  RunTest(":has(+ .a > .b ~ .c)", CSSSelector::kRelativeDirectAdjacent,
          /* expected_adjacent_distance_limit */ 1,
          /* expected_depth_limit */ 1, kOneNextSiblingFixedDepthDescendants);
  RunTest(":has(+ .a + .b > .c ~ .d > .e)",
          CSSSelector::kRelativeDirectAdjacent,
          /* expected_adjacent_distance_limit */ 2,
          /* expected_depth_limit */ 2, kOneNextSiblingFixedDepthDescendants);
}

}  // namespace blink
