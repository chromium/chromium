// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_block_child_iterator.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"

namespace blink {
namespace {

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
  NGLayoutInputNode node1 =
      NGBlockNode(ToLayoutBox(GetLayoutObjectByElementId("child1")));
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
  NGBlockNode container =
      NGBlockNode(ToLayoutBox(GetLayoutObjectByElementId("container")));
  NGLayoutInputNode node1 = container.FirstChild();
  NGLayoutInputNode node2 = node1.NextSibling();
  NGLayoutInputNode node3 = node2.NextSibling();
  NGLayoutInputNode node4 = node3.NextSibling();

  NGBreakTokenVector empty_tokens_list;
  scoped_refptr<NGBreakToken> child_token1 = NGBlockBreakToken::Create(
      node1, LayoutUnit(), empty_tokens_list, kBreakAppealPerfect,
      /* has_seen_all_children */ false);
  scoped_refptr<NGBreakToken> child_token2 = NGBlockBreakToken::Create(
      node2, LayoutUnit(), empty_tokens_list, kBreakAppealPerfect,
      /* has_seen_all_children */ false);
  scoped_refptr<NGBreakToken> child_token3 = NGBlockBreakToken::Create(
      node3, LayoutUnit(), empty_tokens_list, kBreakAppealPerfect,
      /* has_seen_all_children */ false);

  NGBreakTokenVector child_break_tokens;
  child_break_tokens.push_back(child_token1);
  scoped_refptr<NGBlockBreakToken> parent_token = NGBlockBreakToken::Create(
      container, LayoutUnit(), child_break_tokens, kBreakAppealPerfect,
      /* has_seen_all_children */ false);

  NGBlockChildIterator iterator(node1, parent_token.get());
  ASSERT_EQ(NGBlockChildIterator::Entry(node1, child_token1.get()),
            iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node2, nullptr), iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node3, nullptr), iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node4, nullptr), iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(nullptr, nullptr),
            iterator.NextChild());

  child_break_tokens.clear();
  child_break_tokens.push_back(child_token1);
  child_break_tokens.push_back(child_token2);
  parent_token = NGBlockBreakToken::Create(
      container, LayoutUnit(), child_break_tokens, kBreakAppealPerfect,
      /* has_seen_all_children */ false);

  iterator = NGBlockChildIterator(node1, parent_token.get());
  ASSERT_EQ(NGBlockChildIterator::Entry(node1, child_token1.get()),
            iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node2, child_token2.get()),
            iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node3, nullptr), iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node4, nullptr), iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(nullptr, nullptr),
            iterator.NextChild());

  child_break_tokens.clear();
  child_break_tokens.push_back(child_token2);
  child_break_tokens.push_back(child_token3);
  parent_token = NGBlockBreakToken::Create(
      container, LayoutUnit(), child_break_tokens, kBreakAppealPerfect,
      /* has_seen_all_children */ false);

  iterator = NGBlockChildIterator(node1, parent_token.get());
  ASSERT_EQ(NGBlockChildIterator::Entry(node2, child_token2.get()),
            iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node3, child_token3.get()),
            iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node4, nullptr), iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(nullptr, nullptr),
            iterator.NextChild());

  child_break_tokens.clear();
  child_break_tokens.push_back(child_token1);
  child_break_tokens.push_back(child_token3);
  parent_token = NGBlockBreakToken::Create(
      container, LayoutUnit(), child_break_tokens, kBreakAppealPerfect,
      /* has_seen_all_children */ false);

  iterator = NGBlockChildIterator(node1, parent_token.get());
  ASSERT_EQ(NGBlockChildIterator::Entry(node1, child_token1.get()),
            iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node3, child_token3.get()),
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
  NGBlockNode container =
      NGBlockNode(ToLayoutBox(GetLayoutObjectByElementId("container")));
  NGLayoutInputNode node1 = container.FirstChild();

  NGBreakTokenVector empty_tokens_list;
  scoped_refptr<NGBreakToken> child_token1 = NGBlockBreakToken::Create(
      node1, LayoutUnit(), empty_tokens_list, kBreakAppealPerfect,
      /* has_seen_all_children */ false);

  NGBreakTokenVector child_break_tokens;
  child_break_tokens.push_back(child_token1);
  scoped_refptr<NGBlockBreakToken> parent_token = NGBlockBreakToken::Create(
      container, LayoutUnit(), child_break_tokens, kBreakAppealPerfect,
      /* has_seen_all_children */ true);

  // We have a break token for #child1, but have seen all children. This happens
  // e.g. when #child1 has overflow into a new fragmentainer, while #child2 was
  // finished in an earlier fragmentainer.

  NGBlockChildIterator iterator(node1, parent_token.get());
  ASSERT_EQ(NGBlockChildIterator::Entry(node1, child_token1.get()),
            iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(nullptr, nullptr),
            iterator.NextChild());

  child_break_tokens.clear();
  parent_token = NGBlockBreakToken::Create(
      container, LayoutUnit(), child_break_tokens, kBreakAppealPerfect,
      /* has_seen_all_children */ true);

  // We have no break tokens, but have seen all children. This happens e.g. when
  // we have a large container with fixed block-size, with empty space at the
  // end, not occupied by any children.

  iterator = NGBlockChildIterator(node1, parent_token.get());
  ASSERT_EQ(NGBlockChildIterator::Entry(nullptr, nullptr),
            iterator.NextChild());
}

}  // namespace
}  // namespace blink
