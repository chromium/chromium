// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ephemeral_range.h"

#include <sstream>
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

namespace blink {

class EphemeralRangeTest : public EditingTestBase {
 protected:
  template <typename Traversal = NodeTraversal>
  std::string TraverseRange(Range*) const;

  template <typename Strategy>
  std::string TraverseRange(const EphemeralRangeTemplate<Strategy>&) const;

  Range* GetBodyRange() const;
};

template <typename Traversal>
std::string EphemeralRangeTest::TraverseRange(Range* range) const {
  std::stringstream nodes_content;
  for (Node* node = range->FirstNode(); node != range->PastLastNode();
       node = Traversal::Next(*node)) {
    nodes_content << "[" << *node << "]";
  }

  return nodes_content.str();
}

template <typename Strategy>
std::string EphemeralRangeTest::TraverseRange(
    const EphemeralRangeTemplate<Strategy>& range) const {
  std::stringstream nodes_content;
  for (const Node& node : range.Nodes())
    nodes_content << "[" << node << "]";

  return nodes_content.str();
}

Range* EphemeralRangeTest::GetBodyRange() const {
  Range* range = Range::Create(GetDocument());
  range->selectNode(GetDocument().body());
  return range;
}

// Tests that |EphemeralRange::nodes()| will traverse the whole range exactly as
// |for (Node* n = firstNode(); n != pastLastNode(); n = Traversal::next(*n))|
// does.
TEST_F(EphemeralRangeTest, rangeTraversalDOM) {
  const char* body_content =
      "<p id='host'>"
      "<b id='zero'>0</b>"
      "<b id='one'>1</b>"
      "<b id='two'>22</b>"
      "<span id='three'>333</span>"
      "</p>";
  SetBodyContent(body_content);

  const std::string expected_nodes(
      "[BODY][P id=\"host\"][B id=\"zero\"][#text \"0\"][B id=\"one\"][#text "
      "\"1\"][B id=\"two\"][#text \"22\"][SPAN id=\"three\"][#text \"333\"]");

  // Check two ways to traverse.
  EXPECT_EQ(expected_nodes, TraverseRange<>(GetBodyRange()));
  EXPECT_EQ(TraverseRange<>(GetBodyRange()),
            TraverseRange(EphemeralRange(GetBodyRange())));

  EXPECT_EQ(expected_nodes, TraverseRange<FlatTreeTraversal>(GetBodyRange()));
  EXPECT_EQ(TraverseRange<FlatTreeTraversal>(GetBodyRange()),
            TraverseRange(EphemeralRangeInFlatTree(GetBodyRange())));
}

// Tests that |inRange| helper will traverse the whole range with shadow DOM.
TEST_F(EphemeralRangeTest, rangeShadowTraversal) {
  const char* body_content =
      "<b id='zero'>0</b>"
      "<p id='host'>"
      "<b slot='#one' id='one'>1</b>"
      "<b slot='#two' id='two'>22</b>"
      "<b id='three'>333</b>"
      "</p>"
      "<b id='four'>4444</b>";
  const char* shadow_content =
      "<p id='five'>55555</p>"
      "<slot name=#two></slot>"
      "<slot name=#one></slot>"
      "<span id='six'>666666</span>"
      "<p id='seven'>7777777</p>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  const std::string expected_nodes(
      "[BODY][B id=\"zero\"][#text \"0\"][P id=\"host\"][P id=\"five\"][#text "
      "\"55555\"][SLOT][B id=\"two\"][#text \"22\"][SLOT][B id=\"one\"][#text "
      "\"1\"][SPAN id=\"six\"][#text \"666666\"][P id=\"seven\"][#text "
      "\"7777777\"][B id=\"four\"][#text \"4444\"]");

  EXPECT_EQ(expected_nodes, TraverseRange<FlatTreeTraversal>(GetBodyRange()));
  EXPECT_EQ(TraverseRange<FlatTreeTraversal>(GetBodyRange()),
            TraverseRange(EphemeralRangeInFlatTree(GetBodyRange())));
  // Node 'three' should not appear in FlatTreeTraversal.
  EXPECT_EQ(expected_nodes.find("three") == std::string::npos, true);
}

// Limit a range and check that it will be traversed correctly.
TEST_F(EphemeralRangeTest, rangeTraversalLimitedDOM) {
  const char* body_content =
      "<p id='host'>"
      "<b id='zero'>0</b>"
      "<b id='one'>1</b>"
      "<b id='two'>22</b>"
      "<span id='three'>333</span>"
      "</p>";
  SetBodyContent(body_content);

  Range* until_b = GetBodyRange();
  until_b->setEnd(GetDocument().getElementById(AtomicString("one")), 0,
                  IGNORE_EXCEPTION_FOR_TESTING);
  EXPECT_EQ("[BODY][P id=\"host\"][B id=\"zero\"][#text \"0\"][B id=\"one\"]",
            TraverseRange<>(until_b));
  EXPECT_EQ(TraverseRange<>(until_b), TraverseRange(EphemeralRange(until_b)));

  Range* from_b_to_span = GetBodyRange();
  from_b_to_span->setStart(GetDocument().getElementById(AtomicString("one")), 0,
                           IGNORE_EXCEPTION_FOR_TESTING);
  from_b_to_span->setEnd(GetDocument().getElementById(AtomicString("three")), 0,
                         IGNORE_EXCEPTION_FOR_TESTING);
  EXPECT_EQ("[#text \"1\"][B id=\"two\"][#text \"22\"][SPAN id=\"three\"]",
            TraverseRange<>(from_b_to_span));
  EXPECT_EQ(TraverseRange<>(from_b_to_span),
            TraverseRange(EphemeralRange(from_b_to_span)));
}

TEST_F(EphemeralRangeTest, rangeTraversalLimitedFlatTree) {
  const char* body_content =
      "<b id='zero'>0</b>"
      "<p id='host'>"
      "<b slot='#one' id='one'>1</b>"
      "<b slot='#two' id='two'>22</b>"
      "</p>"
      "<b id='three'>333</b>";
  const char* shadow_content =
      "<p id='four'>4444</p>"
      "<slot name=#two></slot>"
      "<slot name=#one></slot>"
      "<span id='five'>55555</span>"
      "<p id='six'>666666</p>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  const PositionInFlatTree start_position(
      GetDocument().getElementById(AtomicString("one")), 0);
  const PositionInFlatTree limit_position(
      shadow_root->getElementById(AtomicString("five")), 0);
  const PositionInFlatTree end_position(
      shadow_root->getElementById(AtomicString("six")), 0);
  const EphemeralRangeInFlatTree from_b_to_span(start_position, limit_position);
  EXPECT_EQ("[#text \"1\"][SPAN id=\"five\"]", TraverseRange(from_b_to_span));

  const EphemeralRangeInFlatTree from_span_to_end(limit_position, end_position);
  EXPECT_EQ("[#text \"55555\"][P id=\"six\"]", TraverseRange(from_span_to_end));
}

TEST_F(EphemeralRangeTest, traversalEmptyRanges) {
  const char* body_content =
      "<p id='host'>"
      "<b id='one'>1</b>"
      "</p>";
  SetBodyContent(body_content);

  // Expect no iterations in loop for an empty EphemeralRange.
  EXPECT_EQ(std::string(), TraverseRange(EphemeralRange()));

  auto iterable = EphemeralRange().Nodes();
  // Tree iterators have only |operator !=| ATM.
  EXPECT_FALSE(iterable.begin() != iterable.end());

  const EphemeralRange single_position_range(GetBodyRange()->StartPosition());
  EXPECT_FALSE(single_position_range.IsNull());
  EXPECT_EQ(std::string(), TraverseRange(single_position_range));
  EXPECT_EQ(single_position_range.StartPosition().NodeAsRangeFirstNode(),
            single_position_range.EndPosition().NodeAsRangePastLastNode());
}

TEST_F(EphemeralRangeTest, commonAncesstorDOM) {
  const char* body_content =
      "<p id='host'>00"
      "<b id='one'>11</b>"
      "<b id='two'>22</b>"
      "<b id='three'>33</b>"
      "</p>";
  SetBodyContent(body_content);

  const Position start_position(
      GetDocument().getElementById(AtomicString("one")), 0);
  const Position end_position(GetDocument().getElementById(AtomicString("two")),
                              0);
  const EphemeralRange range(start_position, end_position);
  EXPECT_EQ(GetDocument().getElementById(AtomicString("host")),
            range.CommonAncestorContainer());
}

TEST_F(EphemeralRangeTest, commonAncesstorFlatTree) {
  const char* body_content =
      "<b id='zero'>0</b>"
      "<p id='host'>"
      "<b slot='#one' id='one'>1</b>"
      "<b slot='#two' id='two'>22</b>"
      "</p>"
      "<b id='three'>333</b>";
  const char* shadow_content =
      "<p id='four'>4444</p>"
      "<slot name=#two></slot>"
      "<slot name=#one></slot>"
      "<p id='five'>55555</p>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  const PositionInFlatTree start_position(
      GetDocument().getElementById(AtomicString("one")), 0);
  const PositionInFlatTree end_position(
      shadow_root->getElementById(AtomicString("five")), 0);
  const EphemeralRangeInFlatTree range(start_position, end_position);
  EXPECT_EQ(GetDocument().getElementById(AtomicString("host")),
            range.CommonAncestorContainer());
}

TEST_F(EphemeralRangeTest, EquivalentPositions) {
  SetBodyContent(
      "<div id='first'></div>"
      "<div id='last'></div>");
  Element* first = GetDocument().getElementById(AtomicString("first"));
  Element* last = GetDocument().getElementById(AtomicString("last"));
  Position after_first = Position::AfterNode(*first);
  Position before_last = Position::BeforeNode(*last);

  // Test ranges created with different but equivalent positions.
  EXPECT_NE(after_first, before_last);
  EXPECT_TRUE(after_first.IsEquivalent(before_last));

  EphemeralRange range1(after_first, before_last);
  EXPECT_TRUE(range1.IsCollapsed());
  EXPECT_EQ(after_first, range1.StartPosition());
  EXPECT_EQ(after_first, range1.EndPosition());

  EphemeralRange range2(before_last, after_first);
  EXPECT_TRUE(range2.IsCollapsed());
  EXPECT_EQ(before_last, range2.StartPosition());
  EXPECT_EQ(before_last, range2.EndPosition());
}

}  // namespace blink
