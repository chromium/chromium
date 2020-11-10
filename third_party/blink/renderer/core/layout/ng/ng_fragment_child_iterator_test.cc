// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_fragment_child_iterator.h"
#include "third_party/blink/renderer/core/layout/ng/ng_base_layout_algorithm_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {
namespace {

class NGFragmentChildIteratorTest
    : public NGBaseLayoutAlgorithmTest,
      private ScopedLayoutNGBlockFragmentationForTest,
      private ScopedLayoutNGFragmentItemForTest {
 protected:
  NGFragmentChildIteratorTest()
      : ScopedLayoutNGBlockFragmentationForTest(true),
        ScopedLayoutNGFragmentItemForTest(true) {}

  scoped_refptr<const NGPhysicalBoxFragment> RunBlockLayoutAlgorithm(
      Element* element) {
    NGBlockNode container(element->GetLayoutBox());
    NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
        {WritingMode::kHorizontalTb, TextDirection::kLtr},
        LogicalSize(LayoutUnit(1000), kIndefiniteSize));
    return NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithm(container, space);
  }
};

TEST_F(NGFragmentChildIteratorTest, Basic) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      <div id="child1">
        <div id="grandchild"></div>
      </div>
      <div id="child2"></div>
    </div>
  )HTML");

  const LayoutObject* child1 = GetLayoutObjectByElementId("child1");
  const LayoutObject* child2 = GetLayoutObjectByElementId("child2");
  const LayoutObject* grandchild = GetLayoutObjectByElementId("grandchild");

  scoped_refptr<const NGPhysicalBoxFragment> container =
      RunBlockLayoutAlgorithm(GetElementById("container"));
  NGFragmentChildIterator iterator1(*container.get());
  EXPECT_FALSE(iterator1.IsAtEnd());

  const NGPhysicalBoxFragment* fragment = iterator1->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), child1);
  EXPECT_FALSE(iterator1.IsAtEnd());

  NGFragmentChildIterator iterator2 = iterator1.Descend();
  EXPECT_FALSE(iterator2.IsAtEnd());
  fragment = iterator2->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), grandchild);
  EXPECT_FALSE(iterator2.IsAtEnd());
  EXPECT_FALSE(iterator2.Advance());
  EXPECT_TRUE(iterator2.IsAtEnd());

  EXPECT_TRUE(iterator1.Advance());
  fragment = iterator1->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), child2);
  EXPECT_FALSE(iterator1.IsAtEnd());

  // #child2 has no children.
  EXPECT_TRUE(iterator1.Descend().IsAtEnd());

  // No more children left.
  EXPECT_FALSE(iterator1.Advance());
  EXPECT_TRUE(iterator1.IsAtEnd());
}

TEST_F(NGFragmentChildIteratorTest, BasicInline) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      xxx
      <span id="span1" style="border:solid;">
        <div id="float1" style="float:left;"></div>
        xxx
      </span>
      xxx
    </div>
  )HTML");

  const LayoutObject* span1 = GetLayoutObjectByElementId("span1");
  const LayoutObject* float1 = GetLayoutObjectByElementId("float1");

  scoped_refptr<const NGPhysicalBoxFragment> container =
      RunBlockLayoutAlgorithm(GetElementById("container"));
  NGFragmentChildIterator iterator1(*container.get());

  EXPECT_FALSE(iterator1->BoxFragment());
  const NGFragmentItem* fragment_item = iterator1->FragmentItem();
  ASSERT_TRUE(fragment_item);
  EXPECT_EQ(fragment_item->Type(), NGFragmentItem::kLine);

  // Descend into the line box.
  NGFragmentChildIterator iterator2 = iterator1.Descend();
  fragment_item = iterator2->FragmentItem();
  ASSERT_TRUE(fragment_item);
  EXPECT_TRUE(fragment_item->IsText());

  EXPECT_TRUE(iterator2.Advance());
  const NGPhysicalBoxFragment* fragment = iterator2->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), span1);

  // Descend into children of #span1.
  NGFragmentChildIterator iterator3 = iterator2.Descend();
  fragment = iterator3->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), float1);

  EXPECT_TRUE(iterator3.Advance());
  fragment_item = iterator3->FragmentItem();
  ASSERT_TRUE(fragment_item);
  EXPECT_TRUE(fragment_item->IsText());
  EXPECT_FALSE(iterator3.Advance());

  // Continue with siblings of #span1.
  EXPECT_TRUE(iterator2.Advance());
  fragment_item = iterator2->FragmentItem();
  ASSERT_TRUE(fragment_item);
  EXPECT_TRUE(fragment_item->IsText());

  EXPECT_FALSE(iterator2.Advance());
  EXPECT_FALSE(iterator1.Advance());
}

TEST_F(NGFragmentChildIteratorTest, InlineBlock) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      xxx
      <span id="inlineblock">
        <div id="float1" style="float:left;"></div>
      </span>
      xxx
    </div>
  )HTML");

  const LayoutObject* inlineblock = GetLayoutObjectByElementId("inlineblock");
  const LayoutObject* float1 = GetLayoutObjectByElementId("float1");

  scoped_refptr<const NGPhysicalBoxFragment> container =
      RunBlockLayoutAlgorithm(GetElementById("container"));
  NGFragmentChildIterator iterator1(*container.get());

  EXPECT_FALSE(iterator1->BoxFragment());
  const NGFragmentItem* fragment_item = iterator1->FragmentItem();
  ASSERT_TRUE(fragment_item);
  EXPECT_EQ(fragment_item->Type(), NGFragmentItem::kLine);

  // Descend into the line box.
  NGFragmentChildIterator iterator2 = iterator1.Descend();
  fragment_item = iterator2->FragmentItem();
  ASSERT_TRUE(fragment_item);
  EXPECT_TRUE(fragment_item->IsText());

  EXPECT_TRUE(iterator2.Advance());
  const NGPhysicalBoxFragment* fragment = iterator2->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), inlineblock);

  // Descend into children of #inlineblock.
  NGFragmentChildIterator iterator3 = iterator2.Descend();
  fragment = iterator3->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), float1);
  EXPECT_FALSE(iterator3.Advance());

  // Continue with siblings of #inlineblock.
  EXPECT_TRUE(iterator2.Advance());
  fragment_item = iterator2->FragmentItem();
  ASSERT_TRUE(fragment_item);
  EXPECT_TRUE(fragment_item->IsText());

  EXPECT_FALSE(iterator2.Advance());
  EXPECT_FALSE(iterator1.Advance());
}

TEST_F(NGFragmentChildIteratorTest, FloatsInInline) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      <span id="span1" style="border:solid;">
        <div id="float1" style="float:left;">
          <div id="child"></div>
        </div>
      </span>
    </div>
  )HTML");

  const LayoutObject* span1 = GetLayoutObjectByElementId("span1");
  const LayoutObject* float1 = GetLayoutObjectByElementId("float1");
  const LayoutObject* child = GetLayoutObjectByElementId("child");

  scoped_refptr<const NGPhysicalBoxFragment> container =
      RunBlockLayoutAlgorithm(GetElementById("container"));
  NGFragmentChildIterator iterator1(*container.get());

  const NGPhysicalBoxFragment* fragment = iterator1->BoxFragment();
  EXPECT_FALSE(fragment);
  const NGFragmentItem* item = iterator1->FragmentItem();
  ASSERT_TRUE(item);
  EXPECT_EQ(item->Type(), NGFragmentItem::kLine);

  // Descend into the line box.
  NGFragmentChildIterator iterator2 = iterator1.Descend();
  fragment = iterator2->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), span1);

  // Descend into children of #span1.
  NGFragmentChildIterator iterator3 = iterator2.Descend();
  fragment = iterator3->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), float1);

  // Descend into children of #float1.
  NGFragmentChildIterator iterator4 = iterator3.Descend();
  fragment = iterator4->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), child);

  EXPECT_FALSE(iterator4.Advance());
  EXPECT_FALSE(iterator3.Advance());
  EXPECT_FALSE(iterator2.Advance());
  EXPECT_FALSE(iterator1.Advance());
}

TEST_F(NGFragmentChildIteratorTest, AbsposAndLine) {
  SetBodyInnerHTML(R"HTML(
    <div id="container" style="position:relative;">
      <div id="abspos" style="position:absolute;"></div>
      xxx
    </div>
  )HTML");

  const LayoutObject* abspos = GetLayoutObjectByElementId("abspos");

  scoped_refptr<const NGPhysicalBoxFragment> container =
      RunBlockLayoutAlgorithm(GetElementById("container"));
  NGFragmentChildIterator iterator1(*container.get());

  const NGPhysicalBoxFragment* fragment = iterator1->BoxFragment();
  EXPECT_FALSE(fragment);
  const NGFragmentItem* item = iterator1->FragmentItem();
  ASSERT_TRUE(item);
  EXPECT_EQ(item->Type(), NGFragmentItem::kLine);

  // Descend into the line box.
  NGFragmentChildIterator iterator2 = iterator1.Descend();

  fragment = iterator2->BoxFragment();
  EXPECT_FALSE(fragment);
  item = iterator2->FragmentItem();
  ASSERT_TRUE(item);
  EXPECT_TRUE(item->IsText());
  EXPECT_FALSE(iterator2.Advance());

  // The abspos is a sibling of the line box.
  EXPECT_TRUE(iterator1.Advance());
  fragment = iterator1->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), abspos);
  EXPECT_FALSE(iterator1.Advance());
}

TEST_F(NGFragmentChildIteratorTest, BasicMulticol) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      <div id="mc" style="columns:3; padding:2px; column-fill:auto; column-gap:10px; width:320px; height:100px;">
        <div id="child" style="margin-top:30px; margin-left:4px; height:200px;"></div>
      </div>
    </div>
  )HTML");

  const LayoutObject* mc = GetLayoutObjectByElementId("mc");
  const LayoutObject* child = GetLayoutObjectByElementId("child");

  scoped_refptr<const NGPhysicalBoxFragment> container =
      RunBlockLayoutAlgorithm(GetElementById("container"));
  NGFragmentChildIterator iterator(*container.get());

  const NGPhysicalBoxFragment* fragment = iterator->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), mc);

  // First column.
  NGFragmentChildIterator child_iterator = iterator.Descend();
  fragment = child_iterator->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsColumnBox());
  EXPECT_EQ(child_iterator->Link().offset.top, LayoutUnit(2));
  EXPECT_EQ(child_iterator->Link().offset.left, LayoutUnit(2));
  EXPECT_EQ(fragment->Size().height, LayoutUnit(100));
  EXPECT_FALSE(fragment->GetLayoutObject());
  EXPECT_FALSE(child_iterator->BlockBreakToken());

  NGFragmentChildIterator grandchild_iterator = child_iterator.Descend();
  fragment = grandchild_iterator->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(grandchild_iterator->Link().offset.top, LayoutUnit(30));
  EXPECT_EQ(grandchild_iterator->Link().offset.left, LayoutUnit(4));
  EXPECT_EQ(fragment->Size().height, LayoutUnit(70));
  EXPECT_EQ(fragment->GetLayoutObject(), child);
  EXPECT_FALSE(grandchild_iterator.Advance());
  EXPECT_FALSE(grandchild_iterator->BlockBreakToken());

  // Second column.
  ASSERT_TRUE(child_iterator.Advance());
  fragment = child_iterator->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsColumnBox());
  EXPECT_EQ(child_iterator->Link().offset.top, LayoutUnit(2));
  EXPECT_EQ(child_iterator->Link().offset.left, LayoutUnit(112));
  EXPECT_EQ(fragment->Size().height, LayoutUnit(100));
  EXPECT_FALSE(fragment->GetLayoutObject());
  const auto* break_token = child_iterator->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_EQ(break_token->ConsumedBlockSize(), LayoutUnit(100));
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), mc);

  grandchild_iterator = child_iterator.Descend();
  fragment = grandchild_iterator->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(grandchild_iterator->Link().offset.top, LayoutUnit(0));
  EXPECT_EQ(grandchild_iterator->Link().offset.left, LayoutUnit(4));
  EXPECT_EQ(fragment->Size().height, LayoutUnit(100));
  EXPECT_EQ(fragment->GetLayoutObject(), child);
  EXPECT_FALSE(grandchild_iterator.Advance());
  break_token = grandchild_iterator->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_EQ(break_token->ConsumedBlockSize(), LayoutUnit(70));
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), child);

  // Third column.
  ASSERT_TRUE(child_iterator.Advance());
  fragment = child_iterator->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsColumnBox());
  EXPECT_EQ(child_iterator->Link().offset.top, LayoutUnit(2));
  EXPECT_EQ(child_iterator->Link().offset.left, LayoutUnit(222));
  EXPECT_EQ(fragment->Size().height, LayoutUnit(100));
  EXPECT_FALSE(fragment->GetLayoutObject());
  break_token = child_iterator->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_EQ(break_token->ConsumedBlockSize(), LayoutUnit(200));
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), mc);

  grandchild_iterator = child_iterator.Descend();
  fragment = grandchild_iterator->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(grandchild_iterator->Link().offset.top, LayoutUnit(0));
  EXPECT_EQ(grandchild_iterator->Link().offset.left, LayoutUnit(4));
  EXPECT_EQ(fragment->Size().height, LayoutUnit(30));
  EXPECT_EQ(fragment->GetLayoutObject(), child);
  EXPECT_FALSE(grandchild_iterator.Advance());
  break_token = grandchild_iterator->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_EQ(break_token->ConsumedBlockSize(), LayoutUnit(170));
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), child);

  EXPECT_FALSE(child_iterator.Advance());
  EXPECT_FALSE(iterator.Advance());
}

TEST_F(NGFragmentChildIteratorTest, ColumnSpanner) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      <div id="mc" style="columns:2;">
        <div id="child">
          <div id="grandchild1" style="height:150px;"></div>
          <div id="spanner" style="column-span:all; height:11px;"></div>
          <div id="grandchild2" style="height:66px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  scoped_refptr<const NGPhysicalBoxFragment> container =
      RunBlockLayoutAlgorithm(GetElementById("container"));
  NGFragmentChildIterator iterator1(*container.get());

  const LayoutObject* mc = GetLayoutObjectByElementId("mc");
  const LayoutObject* child = GetLayoutObjectByElementId("child");
  const LayoutObject* spanner = GetLayoutObjectByElementId("spanner");
  const LayoutObject* grandchild1 = GetLayoutObjectByElementId("grandchild1");
  const LayoutObject* grandchild2 = GetLayoutObjectByElementId("grandchild2");

  const NGPhysicalBoxFragment* fragment = iterator1->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), mc);

  // First column before spanner.
  NGFragmentChildIterator iterator2 = iterator1.Descend();
  fragment = iterator2->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsColumnBox());
  EXPECT_EQ(fragment->Size().height, LayoutUnit(75));
  EXPECT_FALSE(fragment->GetLayoutObject());
  EXPECT_FALSE(iterator2->BlockBreakToken());

  // First fragment for #child.
  NGFragmentChildIterator iterator3 = iterator2.Descend();
  fragment = iterator3->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->Size().height, LayoutUnit(75));
  EXPECT_EQ(fragment->GetLayoutObject(), child);
  EXPECT_FALSE(iterator3->BlockBreakToken());

  // First fragment for #grandchild1.
  NGFragmentChildIterator iterator4 = iterator3.Descend();
  fragment = iterator4->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->Size().height, LayoutUnit(75));
  EXPECT_EQ(fragment->GetLayoutObject(), grandchild1);
  EXPECT_FALSE(iterator4->BlockBreakToken());
  EXPECT_FALSE(iterator4.Advance());
  EXPECT_FALSE(iterator3.Advance());

  // Second column before spanner.
  EXPECT_TRUE(iterator2.Advance());
  fragment = iterator2->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsColumnBox());
  EXPECT_EQ(fragment->Size().height, LayoutUnit(75));
  EXPECT_FALSE(fragment->GetLayoutObject());
  const auto* break_token = iterator2->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_EQ(break_token->ConsumedBlockSize(), LayoutUnit(75));
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), mc);

  // Second fragment for #child.
  iterator3 = iterator2.Descend();
  fragment = iterator3->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->Size().height, LayoutUnit(75));
  EXPECT_EQ(fragment->GetLayoutObject(), child);
  break_token = iterator3->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_EQ(break_token->ConsumedBlockSize(), LayoutUnit(75));
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), child);

  // Second fragment for #grandchild1.
  iterator4 = iterator3.Descend();
  fragment = iterator4->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->Size().height, LayoutUnit(75));
  EXPECT_EQ(fragment->GetLayoutObject(), grandchild1);
  break_token = iterator4->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_EQ(break_token->ConsumedBlockSize(), LayoutUnit(75));
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), grandchild1);
  EXPECT_FALSE(iterator4.Advance());
  EXPECT_FALSE(iterator3.Advance());

  // The spanner.
  EXPECT_TRUE(iterator2.Advance());
  fragment = iterator2->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->Size().height, LayoutUnit(11));
  EXPECT_EQ(fragment->GetLayoutObject(), spanner);
  EXPECT_FALSE(iterator2->BlockBreakToken());

  // First column after spanner.
  EXPECT_TRUE(iterator2.Advance());
  fragment = iterator2->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsColumnBox());
  EXPECT_EQ(fragment->Size().height, LayoutUnit(33));
  EXPECT_FALSE(fragment->GetLayoutObject());
  break_token = iterator2->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_EQ(break_token->ConsumedBlockSize(), LayoutUnit(150));
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), mc);

  // Third fragment for #child.
  iterator3 = iterator2.Descend();
  fragment = iterator3->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->Size().height, LayoutUnit(33));
  EXPECT_EQ(fragment->GetLayoutObject(), child);
  break_token = iterator3->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_EQ(break_token->ConsumedBlockSize(), LayoutUnit(150));
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), child);

  // First fragment for #grandchild2.
  iterator4 = iterator3.Descend();
  fragment = iterator4->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->Size().height, LayoutUnit(33));
  EXPECT_EQ(fragment->GetLayoutObject(), grandchild2);
  break_token = iterator4->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_TRUE(break_token->IsBreakBefore());
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), grandchild2);
  EXPECT_FALSE(iterator4.Advance());
  EXPECT_FALSE(iterator3.Advance());

  // Second column after spanner.
  EXPECT_TRUE(iterator2.Advance());
  fragment = iterator2->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsColumnBox());
  EXPECT_EQ(fragment->Size().height, LayoutUnit(33));
  EXPECT_FALSE(fragment->GetLayoutObject());
  break_token = iterator2->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_EQ(break_token->ConsumedBlockSize(), LayoutUnit(183));
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), mc);

  // Fourth fragment for #child.
  iterator3 = iterator2.Descend();
  fragment = iterator3->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->Size().height, LayoutUnit(33));
  EXPECT_EQ(fragment->GetLayoutObject(), child);
  break_token = iterator3->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_EQ(break_token->ConsumedBlockSize(), LayoutUnit(183));
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), child);

  // Second fragment for #grandchild2.
  iterator4 = iterator3.Descend();
  fragment = iterator4->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->Size().height, LayoutUnit(33));
  EXPECT_EQ(fragment->GetLayoutObject(), grandchild2);
  break_token = iterator4->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_EQ(break_token->ConsumedBlockSize(), LayoutUnit(33));
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), grandchild2);
  EXPECT_FALSE(iterator4.Advance());
  EXPECT_FALSE(iterator3.Advance());

  EXPECT_FALSE(iterator2.Advance());
  EXPECT_FALSE(iterator1.Advance());
}

TEST_F(NGFragmentChildIteratorTest, NestedWithColumnSpanner) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      <div id="mc1" style="columns:2; column-fill:auto; height:100px;">
        <div id="mc2" style="columns:2;">
          <div id="child1" style="height:150px;"></div>
          <div id="spanner1" style="column-span:all;">
            <div id="spanner1child" style="height:55px;"></div>
          </div>
          <div id="child2" style="height:50px;"></div>
          <div id="spanner2" style="column-span:all;">
            <div id="spanner2child" style="height:20px;"></div>
          </div>
          <div id="child3" style="height:20px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  scoped_refptr<const NGPhysicalBoxFragment> container =
      RunBlockLayoutAlgorithm(GetElementById("container"));
  NGFragmentChildIterator iterator1(*container.get());

  const LayoutObject* mc1 = GetLayoutObjectByElementId("mc1");
  const LayoutObject* mc2 = GetLayoutObjectByElementId("mc2");
  const LayoutObject* child1 = GetLayoutObjectByElementId("child1");
  const LayoutObject* child2 = GetLayoutObjectByElementId("child2");
  const LayoutObject* child3 = GetLayoutObjectByElementId("child3");
  const LayoutObject* spanner1 = GetLayoutObjectByElementId("spanner1");
  const LayoutObject* spanner2 = GetLayoutObjectByElementId("spanner2");
  const LayoutObject* spanner1child =
      GetLayoutObjectByElementId("spanner1child");
  const LayoutObject* spanner2child =
      GetLayoutObjectByElementId("spanner2child");

  const NGPhysicalBoxFragment* fragment = iterator1->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), mc1);

  // First outer column.
  NGFragmentChildIterator iterator2 = iterator1.Descend();
  fragment = iterator2->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsColumnBox());
  EXPECT_FALSE(fragment->GetLayoutObject());
  EXPECT_FALSE(iterator2->BlockBreakToken());

  // First fragment for #mc2.
  NGFragmentChildIterator iterator3 = iterator2.Descend();
  fragment = iterator3->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), mc2);
  EXPECT_FALSE(iterator3->BlockBreakToken());

  // First inner column in first outer column.
  NGFragmentChildIterator iterator4 = iterator3.Descend();
  fragment = iterator4->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsColumnBox());
  EXPECT_FALSE(fragment->GetLayoutObject());
  EXPECT_FALSE(iterator4->BlockBreakToken());

  // First fragment for #child1.
  NGFragmentChildIterator iterator5 = iterator4.Descend();
  fragment = iterator5->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), child1);
  EXPECT_FALSE(iterator5->BlockBreakToken());
  EXPECT_FALSE(iterator5.Advance());

  // Second inner column in first outer column.
  EXPECT_TRUE(iterator4.Advance());
  fragment = iterator4->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsColumnBox());
  EXPECT_FALSE(fragment->GetLayoutObject());
  const auto* break_token = iterator4->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_EQ(break_token->ConsumedBlockSize(), LayoutUnit(75));
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), mc2);

  // Second fragment for #child1.
  iterator5 = iterator4.Descend();
  fragment = iterator5->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), child1);
  break_token = iterator5->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_EQ(break_token->ConsumedBlockSize(), LayoutUnit(75));
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), child1);

  // First fragment for #spanner1 (it's split into the first and second outer
  // columns).
  EXPECT_TRUE(iterator4.Advance());
  fragment = iterator4->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), spanner1);
  EXPECT_FALSE(iterator4->BlockBreakToken());

  // First fragment for #spanner1child
  iterator5 = iterator4.Descend();
  fragment = iterator5->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), spanner1child);
  EXPECT_FALSE(iterator5->BlockBreakToken());
  EXPECT_FALSE(iterator5.Advance());
  EXPECT_FALSE(iterator4.Advance());
  EXPECT_FALSE(iterator3.Advance());

  // Second outer column
  EXPECT_TRUE(iterator2.Advance());
  fragment = iterator2->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsColumnBox());
  EXPECT_FALSE(fragment->GetLayoutObject());
  break_token = iterator2->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_EQ(break_token->ConsumedBlockSize(), LayoutUnit(100));
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), mc1);

  // Second fragment for #mc2.
  iterator3 = iterator2.Descend();
  fragment = iterator3->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), mc2);
  break_token = iterator3->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_EQ(break_token->ConsumedBlockSize(), LayoutUnit(100));
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), mc2);

  // Second fragment for #spanner1 (it's split into the first and second outer
  // columns).
  iterator4 = iterator3.Descend();
  fragment = iterator4->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), spanner1);
  break_token = iterator4->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_EQ(break_token->ConsumedBlockSize(), LayoutUnit(25));
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), spanner1);

  // Second fragment for #spanner1child.
  iterator5 = iterator4.Descend();
  fragment = iterator5->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), spanner1child);
  break_token = iterator5->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_EQ(break_token->ConsumedBlockSize(), LayoutUnit(25));
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), spanner1child);
  EXPECT_FALSE(iterator5.Advance());

  // First inner column after first spanner in second outer column.
  EXPECT_TRUE(iterator4.Advance());
  fragment = iterator4->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsColumnBox());
  EXPECT_FALSE(fragment->GetLayoutObject());
  break_token = iterator4->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_EQ(break_token->ConsumedBlockSize(), LayoutUnit(150));
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), mc2);

  // First fragment for #child2.
  iterator5 = iterator4.Descend();
  fragment = iterator5->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), child2);
  break_token = iterator5->BlockBreakToken();
  EXPECT_TRUE(break_token);
  EXPECT_TRUE(break_token->IsBreakBefore());
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), child2);
  EXPECT_FALSE(iterator5.Advance());

  // Second inner column after first spanner in second outer column.
  EXPECT_TRUE(iterator4.Advance());
  fragment = iterator4->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsColumnBox());
  EXPECT_FALSE(fragment->GetLayoutObject());
  break_token = iterator4->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_EQ(break_token->ConsumedBlockSize(), LayoutUnit(175));
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), mc2);

  // Second fragment for #child2.
  iterator5 = iterator4.Descend();
  fragment = iterator5->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), child2);
  break_token = iterator5->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_EQ(break_token->ConsumedBlockSize(), LayoutUnit(25));
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), child2);
  EXPECT_FALSE(iterator5.Advance());

  // The only fragment for #spanner2
  EXPECT_TRUE(iterator4.Advance());
  fragment = iterator4->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), spanner2);
  EXPECT_FALSE(iterator4->BlockBreakToken());

  // First fragment for #spanner2child
  iterator5 = iterator4.Descend();
  fragment = iterator5->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), spanner2child);
  EXPECT_FALSE(iterator5->BlockBreakToken());
  EXPECT_FALSE(iterator5.Advance());

  // First inner column after second spanner in second outer column.
  EXPECT_TRUE(iterator4.Advance());
  fragment = iterator4->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsColumnBox());
  EXPECT_FALSE(fragment->GetLayoutObject());
  break_token = iterator4->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_EQ(break_token->ConsumedBlockSize(), LayoutUnit(200));
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), mc2);

  // First fragment for #child3.
  iterator5 = iterator4.Descend();
  fragment = iterator5->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), child3);
  break_token = iterator5->BlockBreakToken();
  EXPECT_TRUE(break_token);
  EXPECT_TRUE(break_token->IsBreakBefore());
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), child3);
  EXPECT_FALSE(iterator5.Advance());

  // Second inner column after second spanner in second outer column.
  EXPECT_TRUE(iterator4.Advance());
  fragment = iterator4->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsColumnBox());
  EXPECT_FALSE(fragment->GetLayoutObject());
  break_token = iterator4->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_EQ(break_token->ConsumedBlockSize(), LayoutUnit(210));
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), mc2);

  // Second fragment for #child3.
  iterator5 = iterator4.Descend();
  fragment = iterator5->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), child3);
  break_token = iterator5->BlockBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_EQ(break_token->ConsumedBlockSize(), LayoutUnit(10));
  EXPECT_EQ(break_token->InputNode().GetLayoutBox(), child3);
  EXPECT_FALSE(iterator5.Advance());
  EXPECT_FALSE(iterator4.Advance());
  EXPECT_FALSE(iterator3.Advance());
  EXPECT_FALSE(iterator2.Advance());
  EXPECT_FALSE(iterator1.Advance());
}

}  // anonymous namespace
}  // namespace blink
