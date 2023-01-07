// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_block_child_iterator.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"

namespace blink {

const NGBlockBreakToken* CreateBreakToken(
    NGLayoutInputNode node,
    const NGBreakTokenVector* child_break_tokens = nullptr,
    bool has_seen_all_children = false) {
  NGBoxFragmentBuilder builder(
      node, &node.Style(), NGConstraintSpace(),
      WritingDirectionMode(WritingMode::kHorizontalTb, TextDirection::kLtr));
  DCHECK(!builder.HasBreakTokenData());
  builder.SetBreakTokenData(MakeGarbageCollected<NGBlockBreakTokenData>());
  if (has_seen_all_children)
    builder.SetHasSeenAllChildren();
  if (child_break_tokens) {
    for (const NGBreakToken* token : *child_break_tokens)
      builder.AddBreakToken(token);
  }
  return NGBlockBreakToken::Create(&builder);
}

using NGBlockChildIteratorTest = NGLayoutTest;

TEST_F(NGBlockChildIteratorTest, NullFirstChild) {
  NGBlockChildIterator iterator(nullptr, nullptr);
  ASSERT_EQ(NGBlockChildIterator::Entry(nullptr, nullptr),
            iterator.NextChild());
}

TEST_F(NGBlockChildIteratorTest, NoBreakToken) {
  SetBodyInnerHTML(R"HTML(
      <div id='child1'></div>
      <div id='child2'></div>
      <div id='child3'></div>
    )HTML");
  NGLayoutInputNode node1 = NGBlockNode(GetLayoutBoxByElementId("child1"));
  NGLayoutInputNode node2 = node1.NextSibling();
  NGLayoutInputNode node3 = node2.NextSibling();

  // The iterator should loop through three children.
  NGBlockChildIterator iterator(node1, nullptr);
  ASSERT_EQ(NGBlockChildIterator::Entry(node1, nullptr), iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node2, nullptr), iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node3, nullptr), iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(nullptr, nullptr),
            iterator.NextChild());
}

TEST_F(NGBlockChildIteratorTest, BreakTokens) {
  SetBodyInnerHTML(R"HTML(
      <div id='container'>
        <div id='child1'></div>
        <div id='child2'></div>
        <div id='child3'></div>
        <div id='child4'></div>
      </div>
    )HTML");
  NGBlockNode container = NGBlockNode(GetLayoutBoxByElementId("container"));
  NGLayoutInputNode node1 = container.FirstChild();
  NGLayoutInputNode node2 = node1.NextSibling();
  NGLayoutInputNode node3 = node2.NextSibling();
  NGLayoutInputNode node4 = node3.NextSibling();

  NGBreakTokenVector empty_tokens_list;
  const NGBreakToken* child_token1 = CreateBreakToken(node1);
  const NGBreakToken* child_token2 = CreateBreakToken(node2);
  const NGBreakToken* child_token3 = CreateBreakToken(node3);

  NGBreakTokenVector child_break_tokens;
  child_break_tokens.push_back(child_token1);
  const NGBlockBreakToken* parent_token =
      CreateBreakToken(container, &child_break_tokens);

  NGBlockChildIterator iterator(node1, parent_token);
  ASSERT_EQ(NGBlockChildIterator::Entry(node1, child_token1),
            iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node2, nullptr), iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node3, nullptr), iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node4, nullptr), iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(nullptr, nullptr),
            iterator.NextChild());

  child_break_tokens.clear();
  child_break_tokens.push_back(child_token1);
  child_break_tokens.push_back(child_token2);
  parent_token = CreateBreakToken(container, &child_break_tokens);

  iterator = NGBlockChildIterator(node1, parent_token);
  ASSERT_EQ(NGBlockChildIterator::Entry(node1, child_token1),
            iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node2, child_token2),
            iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node3, nullptr), iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node4, nullptr), iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(nullptr, nullptr),
            iterator.NextChild());

  child_break_tokens.clear();
  child_break_tokens.push_back(child_token2);
  child_break_tokens.push_back(child_token3);
  parent_token = CreateBreakToken(container, &child_break_tokens);

  iterator = NGBlockChildIterator(node1, parent_token);
  ASSERT_EQ(NGBlockChildIterator::Entry(node2, child_token2),
            iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node3, child_token3),
            iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node4, nullptr), iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(nullptr, nullptr),
            iterator.NextChild());

  child_break_tokens.clear();
  child_break_tokens.push_back(child_token1);
  child_break_tokens.push_back(child_token3);
  parent_token = CreateBreakToken(container, &child_break_tokens);

  iterator = NGBlockChildIterator(node1, parent_token);
  ASSERT_EQ(NGBlockChildIterator::Entry(node1, child_token1),
            iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node3, child_token3),
            iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node4, nullptr), iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(nullptr, nullptr),
            iterator.NextChild());
}

TEST_F(NGBlockChildIteratorTest, SeenAllChildren) {
  SetBodyInnerHTML(R"HTML(
      <div id='container'>
        <div id='child1'></div>
        <div id='child2'></div>
      </div>
    )HTML");
  NGBlockNode container = NGBlockNode(GetLayoutBoxByElementId("container"));
  NGLayoutInputNode node1 = container.FirstChild();

  const NGBlockBreakToken* child_token1 = CreateBreakToken(node1);

  NGBreakTokenVector child_break_tokens;
  child_break_tokens.push_back(child_token1);
  const NGBlockBreakToken* parent_token = CreateBreakToken(
      container, &child_break_tokens, /* has_seen_all_children*/ true);

  // We have a break token for #child1, but have seen all children. This happens
  // e.g. when #child1 has overflow into a new fragmentainer, while #child2 was
  // finished in an earlier fragmentainer.

  NGBlockChildIterator iterator(node1, parent_token);
  ASSERT_EQ(NGBlockChildIterator::Entry(node1, child_token1),
            iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(nullptr, nullptr),
            iterator.NextChild());

  parent_token = CreateBreakToken(container, /* child_break_tokens */ nullptr,
                                  /* has_seen_all_children*/ true);

  // We have no break tokens, but have seen all children. This happens e.g. when
  // we have a large container with fixed block-size, with empty space at the
  // end, not occupied by any children.

  iterator = NGBlockChildIterator(node1, parent_token);
  ASSERT_EQ(NGBlockChildIterator::Entry(nullptr, nullptr),
            iterator.NextChild());
}

TEST_F(NGBlockChildIteratorTest, DeleteNodeWhileIteration) {
  SetBodyInnerHTML(R"HTML(
      <div id='child1'></div>
      <div id='child2'></div>
      <div id='child3'></div>
    )HTML");
  NGLayoutInputNode node1 = NGBlockNode(GetLayoutBoxByElementId("child1"));
  NGLayoutInputNode node2 = node1.NextSibling();
  NGLayoutInputNode node3 = node2.NextSibling();

  using Entry = NGBlockChildIterator::Entry;
  NGBlockChildIterator iterator(node1, nullptr);
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
