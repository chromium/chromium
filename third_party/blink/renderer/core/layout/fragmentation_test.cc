// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/base_layout_algorithm_test.h"
#include "third_party/blink/renderer/core/layout/block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"

namespace blink {
namespace {

class FragmentationTest : public BaseLayoutAlgorithmTest {
 protected:
  const PhysicalBoxFragment* RunBlockLayoutAlgorithm(Element* element) {
    BlockNode container(element->GetLayoutBox());
    ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
        {WritingMode::kHorizontalTb, TextDirection::kLtr},
        LogicalSize(LayoutUnit(1000), kIndefiniteSize));
    return BaseLayoutAlgorithmTest::RunBlockLayoutAlgorithm(container, space);
  }
};

TEST_F(FragmentationTest, MultipleFragments) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      <div style="columns:3; width:620px; column-fill:auto; height:100px; column-gap:10px;">
        <div id="outer1" style="height:150px;">
          <div id="inner1" style="height:250px;"></div>
          <div id="inner2" style="height:10px;"></div>
        </div>
        <div id="outer2" style="height:90px;"></div>
      </div>
    </div>
  )HTML");

  RunBlockLayoutAlgorithm(GetElementById("container"));
  const LayoutBox* outer1 = GetLayoutBoxByElementId("outer1");
  const LayoutBox* outer2 = GetLayoutBoxByElementId("outer2");
  const LayoutBox* inner1 = GetLayoutBoxByElementId("inner1");
  const LayoutBox* inner2 = GetLayoutBoxByElementId("inner2");

  EXPECT_EQ(outer1->PhysicalFragmentCount(), 3u);
  EXPECT_EQ(outer2->PhysicalFragmentCount(), 2u);
  EXPECT_EQ(inner1->PhysicalFragmentCount(), 3u);
  EXPECT_EQ(inner2->PhysicalFragmentCount(), 1u);

  // While the #outer1 box itself only needs two fragments, we need to create a
  // third fragment to hold the overflowing children in the third column.
  EXPECT_EQ(outer1->GetPhysicalFragment(0)->Size(), PhysicalSize(200, 100));
  EXPECT_EQ(outer1->GetPhysicalFragment(1)->Size(), PhysicalSize(200, 50));
  EXPECT_EQ(outer1->GetPhysicalFragment(2)->Size(), PhysicalSize(200, 0));

  // #inner1 overflows its parent and uses three columns.
  EXPECT_EQ(inner1->GetPhysicalFragment(0)->Size(), PhysicalSize(200, 100));
  EXPECT_EQ(inner1->GetPhysicalFragment(1)->Size(), PhysicalSize(200, 100));
  EXPECT_EQ(inner1->GetPhysicalFragment(2)->Size(), PhysicalSize(200, 50));

  // #inner2 is tiny, and only needs some space in one column (the third one).
  EXPECT_EQ(inner2->GetPhysicalFragment(0)->Size(), PhysicalSize(200, 10));

  // #outer2 starts in the second column and ends in the third.
  EXPECT_EQ(outer2->GetPhysicalFragment(0)->Size(), PhysicalSize(200, 50));
  EXPECT_EQ(outer2->GetPhysicalFragment(1)->Size(), PhysicalSize(200, 40));
}

TEST_F(FragmentationTest, MultipleFragmentsAndColumnSpanner) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      <div id="multicol" style="columns:3; width:620px; column-gap:10px; orphans:1; widows:1; line-height:20px;">
        <div id="outer">
          <div id="inner1"><br><br><br><br></div>
          <div id="spanner1" style="column-span:all;"></div>
          <div id="inner2"><br><br><br><br><br></div>
          <div id="spanner2" style="column-span:all;"></div>
          <div id="inner3"><br><br><br><br><br><br><br></div>
        </div>
      </div>
    </div>
  )HTML");

  RunBlockLayoutAlgorithm(GetElementById("container"));
  const LayoutBox* multicol = GetLayoutBoxByElementId("multicol");
  const LayoutBox* outer = GetLayoutBoxByElementId("outer");
  const LayoutBox* inner1 = GetLayoutBoxByElementId("inner1");
  const LayoutBox* inner2 = GetLayoutBoxByElementId("inner2");
  const LayoutBox* inner3 = GetLayoutBoxByElementId("inner3");
  const LayoutBox* spanner1 = GetLayoutBoxByElementId("spanner1");
  const LayoutBox* spanner2 = GetLayoutBoxByElementId("spanner2");

  EXPECT_EQ(multicol->PhysicalFragmentCount(), 1u);

  // #outer will create 8 fragments: 2 for the 2 columns before the first
  // spanner, 3 for the 3 columns between the two spanners, and 3 for the 3
  // columns after the last spanner.
  EXPECT_EQ(outer->PhysicalFragmentCount(), 8u);

  // #inner1 has 4 lines split into 2 columns.
  EXPECT_EQ(inner1->PhysicalFragmentCount(), 2u);

  // #inner2 has 5 lines split into 3 columns.
  EXPECT_EQ(inner2->PhysicalFragmentCount(), 3u);

  // #inner3 has 8 lines split into 3 columns.
  EXPECT_EQ(inner3->PhysicalFragmentCount(), 3u);

  EXPECT_EQ(spanner1->PhysicalFragmentCount(), 1u);
  EXPECT_EQ(spanner2->PhysicalFragmentCount(), 1u);

  EXPECT_EQ(multicol->GetPhysicalFragment(0)->Size(), PhysicalSize(620, 140));
  EXPECT_EQ(outer->GetPhysicalFragment(0)->Size(), PhysicalSize(200, 40));
  EXPECT_EQ(outer->GetPhysicalFragment(1)->Size(), PhysicalSize(200, 40));
  EXPECT_EQ(outer->GetPhysicalFragment(2)->Size(), PhysicalSize(200, 40));
  EXPECT_EQ(outer->GetPhysicalFragment(3)->Size(), PhysicalSize(200, 40));
  EXPECT_EQ(outer->GetPhysicalFragment(4)->Size(), PhysicalSize(200, 20));
  EXPECT_EQ(outer->GetPhysicalFragment(5)->Size(), PhysicalSize(200, 60));
  EXPECT_EQ(outer->GetPhysicalFragment(6)->Size(), PhysicalSize(200, 60));
  EXPECT_EQ(outer->GetPhysicalFragment(7)->Size(), PhysicalSize(200, 20));
  EXPECT_EQ(inner1->GetPhysicalFragment(0)->Size(), PhysicalSize(200, 40));
  EXPECT_EQ(inner1->GetPhysicalFragment(1)->Size(), PhysicalSize(200, 40));
  EXPECT_EQ(inner2->GetPhysicalFragment(0)->Size(), PhysicalSize(200, 40));
  EXPECT_EQ(inner2->GetPhysicalFragment(1)->Size(), PhysicalSize(200, 40));
  EXPECT_EQ(inner2->GetPhysicalFragment(2)->Size(), PhysicalSize(200, 20));
  EXPECT_EQ(inner3->GetPhysicalFragment(0)->Size(), PhysicalSize(200, 60));
  EXPECT_EQ(inner3->GetPhysicalFragment(1)->Size(), PhysicalSize(200, 60));
  EXPECT_EQ(inner3->GetPhysicalFragment(2)->Size(), PhysicalSize(200, 20));
  EXPECT_EQ(spanner1->GetPhysicalFragment(0)->Size(), PhysicalSize(620, 0));
  EXPECT_EQ(spanner2->GetPhysicalFragment(0)->Size(), PhysicalSize(620, 0));
}

TEST_F(FragmentationTest, MultipleFragmentsNestedMulticol) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      <div id="outer_multicol" style="columns:3; column-fill:auto; height:100px; width:620px; column-gap:10px;">
        <div id="inner_multicol" style="columns:2; column-fill:auto;">
          <div id="child1" style="width:11px; height:350px;"></div>
          <div id="child2" style="width:22px; height:350px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  RunBlockLayoutAlgorithm(GetElementById("container"));
  const LayoutBox* outer_multicol = GetLayoutBoxByElementId("outer_multicol");
  const LayoutBox* inner_multicol = GetLayoutBoxByElementId("inner_multicol");
  const LayoutBox* child1 = GetLayoutBoxByElementId("child1");
  const LayoutBox* child2 = GetLayoutBoxByElementId("child2");

  EXPECT_EQ(outer_multicol->PhysicalFragmentCount(), 1u);

  // The content is too tall (350px + 350px, column height 100px, 2*3 columns =
  // 600px) and will use one more column than we have specified.
  EXPECT_EQ(inner_multicol->PhysicalFragmentCount(), 4u);

  // 350px tall content with a column height of 100px will require 4 fragments.
  EXPECT_EQ(child1->PhysicalFragmentCount(), 4u);
  EXPECT_EQ(child2->PhysicalFragmentCount(), 4u);

  EXPECT_EQ(outer_multicol->GetPhysicalFragment(0)->Size(),
            PhysicalSize(620, 100));

  EXPECT_EQ(inner_multicol->GetPhysicalFragment(0)->Size(),
            PhysicalSize(200, 100));
  EXPECT_EQ(inner_multicol->GetPhysicalFragment(1)->Size(),
            PhysicalSize(200, 100));
  EXPECT_EQ(inner_multicol->GetPhysicalFragment(2)->Size(),
            PhysicalSize(200, 100));
  EXPECT_EQ(inner_multicol->GetPhysicalFragment(3)->Size(),
            PhysicalSize(200, 100));

  // #child1 starts at the beginning of a column, so the last fragment will be
  // shorter than the rest.
  EXPECT_EQ(child1->GetPhysicalFragment(0)->Size(), PhysicalSize(11, 100));
  EXPECT_EQ(child1->GetPhysicalFragment(1)->Size(), PhysicalSize(11, 100));
  EXPECT_EQ(child1->GetPhysicalFragment(2)->Size(), PhysicalSize(11, 100));
  EXPECT_EQ(child1->GetPhysicalFragment(3)->Size(), PhysicalSize(11, 50));

  // #child2 starts in the middle of a column, so the first fragment will be
  // shorter than the rest.
  EXPECT_EQ(child2->GetPhysicalFragment(0)->Size(), PhysicalSize(22, 50));
  EXPECT_EQ(child2->GetPhysicalFragment(1)->Size(), PhysicalSize(22, 100));
  EXPECT_EQ(child2->GetPhysicalFragment(2)->Size(), PhysicalSize(22, 100));
  EXPECT_EQ(child2->GetPhysicalFragment(3)->Size(), PhysicalSize(22, 100));
}

TEST_F(FragmentationTest, HasSeenAllChildrenIfc) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      <div style="columns:3; column-fill:auto; height:50px; line-height:20px; orphans:1; widows:1;">
        <div id="ifc" style="height:300px;">
          <br><br>
          <br><br>
          <br><br>
          <br>
        </div>
      </div>
    </div>
  )HTML");

  RunBlockLayoutAlgorithm(GetElementById("container"));

  const LayoutBox* ifc = GetLayoutBoxByElementId("ifc");
  ASSERT_EQ(ifc->PhysicalFragmentCount(), 6u);
  const PhysicalBoxFragment* fragment = ifc->GetPhysicalFragment(0);
  const BlockBreakToken* break_token = fragment->GetBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_FALSE(break_token->HasSeenAllChildren());

  fragment = ifc->GetPhysicalFragment(1);
  break_token = fragment->GetBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_FALSE(break_token->HasSeenAllChildren());

  fragment = ifc->GetPhysicalFragment(2);
  break_token = fragment->GetBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_FALSE(break_token->HasSeenAllChildren());

  fragment = ifc->GetPhysicalFragment(3);
  break_token = fragment->GetBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_TRUE(break_token->HasSeenAllChildren());

  fragment = ifc->GetPhysicalFragment(4);
  break_token = fragment->GetBreakToken();
  ASSERT_TRUE(break_token);
  EXPECT_TRUE(break_token->HasSeenAllChildren());

  fragment = ifc->GetPhysicalFragment(5);
  break_token = fragment->GetBreakToken();
  EXPECT_FALSE(break_token);
}

TEST_F(FragmentationTest, InkOverflowInline) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #container {
      font-size: 10px;
      column-width: 100px;
      column-gap: 10px;
      width: 210px;
      line-height: 15px;
      height: 15px;
    }
    atomic {
      display: inline-block;
      width: 100px;
      height: 10px;
      background: blue;
    }
    .w15 {
      width: 150px;
      background: orange;
    }
    </style>
    <div id="container">
      <div>
        <!-- 1st column does not have ink overflow. -->
        <atomic></atomic>
        <!-- 2nd column has 50px ink overflow to right. -->
        <atomic><atomic class="w15"></atomic></atomic>
      </div>
    </div>
  )HTML");
  const auto* container =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  const auto* flow_thread = To<LayoutBlockFlow>(container->FirstChild());
  DCHECK(flow_thread->IsLayoutFlowThread());
  // |flow_thread| is in the stitched coordinate system.
  // Legacy had (0, 0, 150, 30), but NG doesn't compute for |LayoutFlowThread|.
  EXPECT_EQ(flow_thread->VisualOverflowRect(), PhysicalRect(0, 0, 100, 30));
  EXPECT_EQ(container->VisualOverflowRect(), PhysicalRect(0, 0, 260, 15));
}

TEST_F(FragmentationTest, OffsetFromOwnerLayoutBoxFloat) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #columns {
      column-width: 100px;
      column-gap: 10px;
      column-fill: auto;
      width: 320px;
      height: 500px;
    }
    #float {
      float: left;
      width: 50px;
      height: 500px;
      background: orange;
    }
    </style>
    <div id="columns" style="background: blue">
      <!-- A spacer to make `target` start at 2nd column. -->
      <div style="height: 800px"></div>
      <div id="float"></div>
      Text
    </div>
  )HTML");
  const auto* target = GetLayoutBoxByElementId("float");
  EXPECT_EQ(target->PhysicalFragmentCount(), 2u);
  const PhysicalBoxFragment* fragment0 = target->GetPhysicalFragment(0);
  EXPECT_EQ(fragment0->OffsetFromOwnerLayoutBox(), PhysicalOffset());
  const PhysicalBoxFragment* fragment1 = target->GetPhysicalFragment(1);
  EXPECT_EQ(fragment1->OffsetFromOwnerLayoutBox(), PhysicalOffset(110, -300));
}

TEST_F(FragmentationTest, OffsetFromOwnerLayoutBoxNested) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
    }
    #outer-columns {
      column-width: 100px;
      column-gap: 10px;
      column-fill: auto;
      width: 320px;
      height: 500px;
    }
    #inner-columns {
      column-width: 45px;
      column-gap: 10px;
      column-fill: auto;
      width: 100px;
      height: 800px;
    }
    </style>
    <div id="outer-columns" style="background: blue">
      <!-- A spacer to make `inner-columns` start at 2nd column. -->
      <div style="height: 700px"></div>
      <div id="inner-columns" style="height: 800px; background: purple">
        <!-- A spacer to make `target` start at 2nd column. -->
        <div style="height: 400px"></div>
        <div id="target" style="background: orange; height: 1000px"></div>
      </div>
    </div>
  )HTML");
  const auto* target = GetLayoutBoxByElementId("target");
  EXPECT_EQ(target->PhysicalFragmentCount(), 3u);
  const PhysicalBoxFragment* fragment0 = target->GetPhysicalFragment(0);
  EXPECT_EQ(fragment0->OffsetFromOwnerLayoutBox(), PhysicalOffset());
  const PhysicalBoxFragment* fragment1 = target->GetPhysicalFragment(1);
  EXPECT_EQ(fragment1->OffsetFromOwnerLayoutBox(), PhysicalOffset(55, -300));
  const PhysicalBoxFragment* fragment2 = target->GetPhysicalFragment(2);
  EXPECT_EQ(fragment2->OffsetFromOwnerLayoutBox(), PhysicalOffset(110, -300));
}

}  // anonymous namespace
}  // namespace blink
