// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/block_child_iterator.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

const BlockBreakToken* CreateBreakToken(
    LayoutInputNode node,
    const BreakTokenVector* child_break_tokens = nullptr,
    bool has_seen_all_children = false) {
  WritingDirectionMode writing_direction(WritingMode::kHorizontalTb,
                                         TextDirection::kLtr);
  ConstraintSpaceBuilder space_builder(writing_direction.GetWritingMode(),
                                       writing_direction,
                                       /* is_new_fc */ true);
  BoxFragmentBuilder fragment_builder(
      node, &node.Style(), space_builder.ToConstraintSpace(), writing_direction,
      /*previous_break_token=*/nullptr);
  DCHECK(!fragment_builder.HasBreakTokenData());
  fragment_builder.SetBreakTokenData(
      MakeGarbageCollected<BlockBreakTokenData>());
  if (has_seen_all_children) {
    fragment_builder.SetHasSeenAllChildren();
  }
  if (child_break_tokens) {
    for (const BreakToken* token : *child_break_tokens) {
      fragment_builder.AddBreakToken(token);
    }
  }
  return BlockBreakToken::Create(&fragment_builder);
}

using BlockChildIteratorTest = RenderingTest;

TEST_F(BlockChildIteratorTest, NullFirstChild) {
  BlockChildIterator iterator(nullptr, nullptr);
  ASSERT_EQ(BlockChildIterator::Entry(nullptr, nullptr), iterator.NextChild());
}

TEST_F(BlockChildIteratorTest, NoBreakToken) {
  SetBodyInnerHTML(R"HTML(
      <div id='child1'></div>
      <div id='child2'></div>
      <div id='child3'></div>
    )HTML");
  LayoutInputNode node1 = BlockNode(GetLayoutBoxByElementId("child1"));
  LayoutInputNode node2 = node1.NextSibling();
  LayoutInputNode node3 = node2.NextSibling();

  // The iterator should loop through three children.
  BlockChildIterator iterator(node1, nullptr);
  ASSERT_EQ(BlockChildIterator::Entry(node1, nullptr), iterator.NextChild());
  ASSERT_EQ(BlockChildIterator::Entry(node2, nullptr), iterator.NextChild());
  ASSERT_EQ(BlockChildIterator::Entry(node3, nullptr), iterator.NextChild());
  ASSERT_EQ(BlockChildIterator::Entry(nullptr, nullptr), iterator.NextChild());
}

TEST_F(BlockChildIteratorTest, BreakTokens) {
  SetBodyInnerHTML(R"HTML(
      <div id='container'>
        <div id='child1'></div>
        <div id='child2'></div>
        <div id='child3'></div>
        <div id='child4'></div>
      </div>
    )HTML");
  BlockNode container = BlockNode(GetLayoutBoxByElementId("container"));
  LayoutInputNode node1 = container.FirstChild();
  LayoutInputNode node2 = node1.NextSibling();
  LayoutInputNode node3 = node2.NextSibling();
  LayoutInputNode node4 = node3.NextSibling();

  BreakTokenVector empty_tokens_list;
  const BreakToken* child_token1 = CreateBreakToken(node1);
  const BreakToken* child_token2 = CreateBreakToken(node2);
  const BreakToken* child_token3 = CreateBreakToken(node3);

  BreakTokenVector child_break_tokens;
  child_break_tokens.push_back(child_token1);
  const BlockBreakToken* parent_token =
      CreateBreakToken(container, &child_break_tokens);

  BlockChildIterator iterator(node1, parent_token);
  ASSERT_EQ(BlockChildIterator::Entry(node1, child_token1),
            iterator.NextChild());
  ASSERT_EQ(BlockChildIterator::Entry(node2, nullptr), iterator.NextChild());
  ASSERT_EQ(BlockChildIterator::Entry(node3, nullptr), iterator.NextChild());
  ASSERT_EQ(BlockChildIterator::Entry(node4, nullptr), iterator.NextChild());
  ASSERT_EQ(BlockChildIterator::Entry(nullptr, nullptr), iterator.NextChild());

  child_break_tokens.clear();
  child_break_tokens.push_back(child_token1);
  child_break_tokens.push_back(child_token2);
  parent_token = CreateBreakToken(container, &child_break_tokens);

  iterator = BlockChildIterator(node1, parent_token);
  ASSERT_EQ(BlockChildIterator::Entry(node1, child_token1),
            iterator.NextChild());
  ASSERT_EQ(BlockChildIterator::Entry(node2, child_token2),
            iterator.NextChild());
  ASSERT_EQ(BlockChildIterator::Entry(node3, nullptr), iterator.NextChild());
  ASSERT_EQ(BlockChildIterator::Entry(node4, nullptr), iterator.NextChild());
  ASSERT_EQ(BlockChildIterator::Entry(nullptr, nullptr), iterator.NextChild());

  child_break_tokens.clear();
  child_break_tokens.push_back(child_token2);
  child_break_tokens.push_back(child_token3);
  parent_token = CreateBreakToken(container, &child_break_tokens);

  iterator = BlockChildIterator(node1, parent_token);
  ASSERT_EQ(BlockChildIterator::Entry(node2, child_token2),
            iterator.NextChild());
  ASSERT_EQ(BlockChildIterator::Entry(node3, child_token3),
            iterator.NextChild());
  ASSERT_EQ(BlockChildIterator::Entry(node4, nullptr), iterator.NextChild());
  ASSERT_EQ(BlockChildIterator::Entry(nullptr, nullptr), iterator.NextChild());

  child_break_tokens.clear();
  child_break_tokens.push_back(child_token1);
  child_break_tokens.push_back(child_token3);
  parent_token = CreateBreakToken(container, &child_break_tokens);

  iterator = BlockChildIterator(node1, parent_token);
  ASSERT_EQ(BlockChildIterator::Entry(node1, child_token1),
            iterator.NextChild());
  ASSERT_EQ(BlockChildIterator::Entry(node3, child_token3),
            iterator.NextChild());
  ASSERT_EQ(BlockChildIterator::Entry(node4, nullptr), iterator.NextChild());
  ASSERT_EQ(BlockChildIterator::Entry(nullptr, nullptr), iterator.NextChild());
}

TEST_F(BlockChildIteratorTest, SeenAllChildren) {
  SetBodyInnerHTML(R"HTML(
      <div id='container'>
        <div id='child1'></div>
        <div id='child2'></div>
      </div>
    )HTML");
  BlockNode container = BlockNode(GetLayoutBoxByElementId("container"));
  LayoutInputNode node1 = container.FirstChild();

  const BlockBreakToken* child_token1 = CreateBreakToken(node1);

  BreakTokenVector child_break_tokens;
  child_break_tokens.push_back(child_token1);
  const BlockBreakToken* parent_token = CreateBreakToken(
      container, &child_break_tokens, /* has_seen_all_children*/ true);

  // We have a break token for #child1, but have seen all children. This happens
  // e.g. when #child1 has overflow into a new fragmentainer, while #child2 was
  // finished in an earlier fragmentainer.

  BlockChildIterator iterator(node1, parent_token);
  ASSERT_EQ(BlockChildIterator::Entry(node1, child_token1),
            iterator.NextChild());
  ASSERT_EQ(BlockChildIterator::Entry(nullptr, nullptr), iterator.NextChild());

  parent_token = CreateBreakToken(container, /* child_break_tokens */ nullptr,
                                  /* has_seen_all_children*/ true);

  // We have no break tokens, but have seen all children. This happens e.g. when
  // we have a large container with fixed block-size, with empty space at the
  // end, not occupied by any children.

  iterator = BlockChildIterator(node1, parent_token);
  ASSERT_EQ(BlockChildIterator::Entry(nullptr, nullptr), iterator.NextChild());
}

TEST_F(BlockChildIteratorTest, DeleteNodeWhileIteration) {
  SetBodyInnerHTML(R"HTML(
      <div id='child1'></div>
      <div id='child2'></div>
      <div id='child3'></div>
    )HTML");
  LayoutInputNode node1 = BlockNode(GetLayoutBoxByElementId("child1"));
  LayoutInputNode node2 = node1.NextSibling();
  LayoutInputNode node3 = node2.NextSibling();

  using Entry = BlockChildIterator::Entry;
  BlockChildIterator iterator(node1, nullptr);
  EXPECT_EQ(Entry(node1, nullptr), iterator.NextChild());
  {
    // Set the container query flag to pass LayoutObject::
    // IsAllowedToModifyLayoutTreeStructure() check.
    base::AutoReset<bool> cq_recalc(
        &GetDocument().GetStyleEngine().in_container_query_style_recalc_, true);
    node2.GetLayoutBox()->Remove();
  }
  EXPECT_EQ(Entry(node3, nullptr), iterator.NextChild());
  EXPECT_EQ(Entry(nullptr, nullptr), iterator.NextChild());
}

}  // namespace blink
