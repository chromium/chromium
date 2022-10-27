// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_column_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/ng_base_layout_algorithm_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_column_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {
namespace {

class NGColumnLayoutAlgorithmTest
    : public NGBaseLayoutAlgorithmTest,
      private ScopedLayoutNGBlockFragmentationForTest {
 protected:
  NGColumnLayoutAlgorithmTest()
      : ScopedLayoutNGBlockFragmentationForTest(true) {}

  const NGPhysicalBoxFragment* RunBlockLayoutAlgorithm(Element* element) {
    NGBlockNode container(element->GetLayoutBox());
    NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
        {WritingMode::kHorizontalTb, TextDirection::kLtr},
        LogicalSize(LayoutUnit(1000), kIndefiniteSize));
    return NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithm(container, space);
  }

  String DumpFragmentTree(const NGPhysicalBoxFragment* fragment) {
    NGPhysicalFragment::DumpFlags flags =
        NGPhysicalFragment::DumpHeaderText | NGPhysicalFragment::DumpSubtree |
        NGPhysicalFragment::DumpIndentation | NGPhysicalFragment::DumpOffset |
        NGPhysicalFragment::DumpSize;

    return fragment->DumpFragmentTree(flags);
  }

  String DumpFragmentTree(Element* element) {
    auto* fragment = RunBlockLayoutAlgorithm(element);
    return DumpFragmentTree(fragment);
  }
};

TEST_F(NGColumnLayoutAlgorithmTest, EmptyEditable) {
  LoadAhem();
  InsertStyleElement(
      "body { font: 10px/20px Ahem; }"
      "#multicol1, #multicol2 { columns: 3; }");
  SetBodyInnerHTML(
      "<div contenteditable id=single></div>"
      "<div contenteditable id=multicol1><br></div>"
      "<div contenteditable id=multicol2></div>");

  EXPECT_EQ(20, GetElementById("single")->OffsetHeight());
  EXPECT_EQ(20, GetElementById("multicol1")->OffsetHeight());
  EXPECT_EQ(20, GetElementById("multicol2")->OffsetHeight());
}

TEST_F(NGColumnLayoutAlgorithmTest, EmptyEditableWithFloat) {
  LoadAhem();
  InsertStyleElement(
      "body { font: 10px/20px Ahem; }"
      "float { float:right; width: 50px; height: 50px; background:pink; }"
      "#multicol1, #multicol2 { columns: 3; }");
  SetBodyInnerHTML(
      "<div contenteditable id=single><float></float></div>"
      // Note: <float> spreads into all columns.
      "<div contenteditable id=multicol1><float></float><br></div>"
      "<div contenteditable id=multicol2><float></float></div>");

  EXPECT_EQ(20, GetElementById("single")->OffsetHeight());
  EXPECT_EQ(20, GetElementById("multicol1")->OffsetHeight());
  EXPECT_EQ(20, GetElementById("multicol2")->OffsetHeight());
}

TEST_F(NGColumnLayoutAlgorithmTest, EmptyMulticol) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 2;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 210px;
      }
    </style>
    <div id="container">
      <div id="parent"></div>
    </div>
  )HTML");

  NGBlockNode container(GetLayoutBoxByElementId("container"));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize));
  const NGPhysicalBoxFragment* parent_fragment =
      NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithm(container, space);
  FragmentChildIterator iterator(parent_fragment);
  const auto* fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(PhysicalSize(210, 100), fragment->Size());
  EXPECT_EQ(1UL, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());

  // A multicol container will always create at least one fragmentainer.
  fragment = FragmentChildIterator(fragment).NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(PhysicalSize(100, 100), fragment->Size());
  EXPECT_EQ(0UL, fragment->Children().size());

  EXPECT_FALSE(iterator.NextChild());
}

TEST_F(NGColumnLayoutAlgorithmTest, EmptyBlock) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 2;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 210px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child"></div>
      </div>
    </div>
  )HTML");

  NGBlockNode container(GetLayoutBoxByElementId("container"));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize));
  const NGPhysicalBoxFragment* parent_fragment =
      NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithm(container, space);
  FragmentChildIterator iterator(parent_fragment);
  const auto* fragment = iterator.NextChild();
  EXPECT_EQ(PhysicalSize(210, 100), fragment->Size());
  ASSERT_TRUE(fragment);
  EXPECT_FALSE(iterator.NextChild());
  iterator.SetParent(fragment);

  // first column fragment
  PhysicalOffset offset;
  fragment = iterator.NextChild(&offset);
  ASSERT_TRUE(fragment);
  EXPECT_EQ(PhysicalOffset(), offset);
  EXPECT_EQ(PhysicalSize(100, 100), fragment->Size());
  EXPECT_FALSE(iterator.NextChild());

  // #child fragment in first column
  iterator.SetParent(fragment);
  fragment = iterator.NextChild(&offset);
  ASSERT_TRUE(fragment);
  EXPECT_EQ(PhysicalOffset(), offset);
  EXPECT_EQ(PhysicalSize(100, 0), fragment->Size());
  EXPECT_EQ(0UL, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

TEST_F(NGColumnLayoutAlgorithmTest, BlockInOneColumn) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 2;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 310px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child" style="width:60%; height:100%"></div>
      </div>
    </div>
  )HTML");

  NGBlockNode container(GetLayoutBoxByElementId("container"));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize));
  const NGPhysicalBoxFragment* parent_fragment =
      NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithm(container, space);

  FragmentChildIterator iterator(parent_fragment);
  const auto* fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(PhysicalSize(310, 100), fragment->Size());
  EXPECT_FALSE(iterator.NextChild());
  iterator.SetParent(fragment);

  // first column fragment
  PhysicalOffset offset;
  fragment = iterator.NextChild(&offset);
  ASSERT_TRUE(fragment);
  EXPECT_EQ(PhysicalOffset(), offset);
  EXPECT_EQ(PhysicalSize(150, 100), fragment->Size());
  EXPECT_FALSE(iterator.NextChild());

  // #child fragment in first column
  iterator.SetParent(fragment);
  fragment = iterator.NextChild(&offset);
  ASSERT_TRUE(fragment);
  EXPECT_EQ(PhysicalOffset(), offset);
  EXPECT_EQ(PhysicalSize(90, 100), fragment->Size());
  EXPECT_EQ(0UL, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

TEST_F(NGColumnLayoutAlgorithmTest, ZeroHeightBlockAtFragmentainerBoundary) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 2;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 210px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:33px; height:200px;"></div>
        <div style="width:44px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:210x100
      offset:0,0 size:100x100
        offset:0,0 size:33x100
      offset:110,0 size:100x100
        offset:0,0 size:33x100
        offset:0,100 size:44x0
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, BlockInTwoColumns) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 2;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 210px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child" style="width:75%; height:150px"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:210x100
      offset:0,0 size:100x100
        offset:0,0 size:75x100
      offset:110,0 size:100x100
        offset:0,0 size:75x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, BlockInThreeColumns) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child" style="width:75%; height:250px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:75x100
      offset:110,0 size:100x100
        offset:0,0 size:75x100
      offset:220,0 size:100x100
        offset:0,0 size:75x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ActualColumnCountGreaterThanSpecified) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 2;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 210px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child" style="width:1px; height:250px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:210x100
      offset:0,0 size:100x100
        offset:0,0 size:1x100
      offset:110,0 size:100x100
        offset:0,0 size:1x100
      offset:220,0 size:100x100
        offset:0,0 size:1x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, TwoBlocksInTwoColumns) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child1" style="width:75%; height:60px;"></div>
        <div id="child2" style="width:85%; height:60px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:75x60
        offset:0,60 size:85x40
      offset:110,0 size:100x100
        offset:0,0 size:85x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ZeroHeight) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        height: 0;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent"></div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x0
    offset:0,0 size:320x0
      offset:0,0 size:100x0
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ZeroHeightWithContent) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        height: 0;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:20px; height:5px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x0
    offset:0,0 size:320x0
      offset:0,0 size:100x0
        offset:0,0 size:20x1
      offset:110,0 size:100x0
        offset:0,0 size:20x1
      offset:220,0 size:100x0
        offset:0,0 size:20x1
      offset:330,0 size:100x0
        offset:0,0 size:20x1
      offset:440,0 size:100x0
        offset:0,0 size:20x1
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, OverflowedBlock) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child1" style="width:75%; height:60px;">
          <div id="grandchild1" style="width:50px; height:120px;"></div>
          <div id="grandchild2" style="width:40px; height:20px;"></div>
        </div>
        <div id="child2" style="width:85%; height:10px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:75x60
          offset:0,0 size:50x100
        offset:0,60 size:85x10
      offset:110,0 size:100x100
        offset:0,0 size:75x0
          offset:0,0 size:50x20
          offset:0,20 size:40x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, OverflowedBlock2) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:75%; height:10px;">
          <div style="width:50px; height:220px;"></div>
        </div>
        <div style="width:85%; height:10px;"></div>
        <div style="width:65%; height:10px;">
          <div style="width:51px; height:220px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:75x10
          offset:0,0 size:50x100
        offset:0,10 size:85x10
        offset:0,20 size:65x10
          offset:0,0 size:51x80
      offset:110,0 size:100x100
        offset:0,0 size:75x0
          offset:0,0 size:50x100
        offset:0,0 size:65x0
          offset:0,0 size:51x100
      offset:220,0 size:100x100
        offset:0,0 size:75x0
          offset:0,0 size:50x20
        offset:0,0 size:65x0
          offset:0,0 size:51x40
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, OverflowedBlock3) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:75%; height:60px;">
          <div style="width:50px; height:220px;"></div>
        </div>
        <div style="width:85%; height:10px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:75x60
          offset:0,0 size:50x100
        offset:0,60 size:85x10
      offset:110,0 size:100x100
        offset:0,0 size:75x0
          offset:0,0 size:50x100
      offset:220,0 size:100x100
        offset:0,0 size:75x0
          offset:0,0 size:50x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, UnusedSpaceInBlock) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="height:300px;">
          <div style="width:20px; height:20px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x100
          offset:0,0 size:20x20
      offset:110,0 size:100x100
        offset:0,0 size:100x100
      offset:220,0 size:100x100
        offset:0,0 size:100x100
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, FloatInOneColumn) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child" style="float:left; width:75%; height:100px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:75x100
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, TwoFloatsInOneColumn) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child1" style="float:left; width:15%; height:100px;"></div>
        <div id="child2" style="float:right; width:16%; height:100px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:15x100
        offset:84,0 size:16x100
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, TwoFloatsInTwoColumns) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child1" style="float:left; width:15%; height:150px;"></div>
        <div id="child2" style="float:right; width:16%; height:150px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:15x100
        offset:84,0 size:16x100
      offset:110,0 size:100x100
        offset:0,0 size:15x50
        offset:84,0 size:16x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, FloatWithForcedBreak) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="height:50px;"></div>
        <div style="float:left; width:77px;">
           <div style="width:66px; height:30px;"></div>
           <div style="break-before:column; width:55px; height:30px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x50
        offset:0,50 size:77x50
          offset:0,0 size:66x30
      offset:110,0 size:100x100
        offset:0,0 size:77x30
          offset:0,0 size:55x30
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, FloatWithMargin) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="float:left; width:77px; margin-top:10px; height:140px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,10 size:77x90
      offset:110,0 size:100x100
        offset:0,0 size:77x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, FloatWithMarginBelowFloat) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="float:left; width:66px; height:40px;"></div>
        <div style="float:left; width:77px; margin-top:10px; height:70px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:66x40
        offset:0,50 size:77x50
      offset:110,0 size:100x100
        offset:0,0 size:77x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, FloatWithLastResortBreak) {
  // Breaking inside the line is not possible, and breaking between the
  // block-start content edge and the first child should be avoided.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
        line-height: 20px;
        orphans: 1;
        widows: 1;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:99px; height:90px;"></div>
        <div style="float:left; width:88px;">
          <br>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:99x90
      offset:110,0 size:100x100
        offset:0,0 size:88x20
          offset:0,0 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, FloatWithAvoidBreak) {
  // We want to avoid breaking inside the float child, and breaking before it
  // should be avoided (not a valid breakpoint).
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:99px; height:90px;"></div>
        <div style="float:left; width:88px;">
          <div class="content" style="width:77px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:99x90
      offset:110,0 size:100x100
        offset:0,0 size:88x20
          offset:0,0 size:77x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, FloatWithMarginAndAvoidBreak) {
  // We want to avoid breaking inside the float child, and breaking before it
  // should be avoided (not a valid breakpoint). The top margin should be kept
  // in the next column.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:99px; height:90px;"></div>
        <div style="float:left; width:88px; margin-top:5px;">
          <div class="content" style="width:77px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:99x90
      offset:110,0 size:100x100
        offset:0,5 size:88x20
          offset:0,0 size:77x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, UnbreakableFloatBeforeBreakable) {
  // https://www.w3.org/TR/CSS22/visuren.html#float-position
  //
  // "The outer top of a floating box may not be higher than the outer top of
  // any block or floated box generated by an element earlier in the source
  // document."
  //
  // This means that if we decide to break before one float, we also need to
  // break before all subsequent floats, even if such floats don't require that
  // on their own.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:99px; height:90px;"></div>
        <div style="float:left; width:22px; height:50px;">
          <div class="content" style="width:11px;"></div>
        </div>
        <div style="float:left; width:33px; height:50px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:99x90
      offset:110,0 size:100x100
        offset:0,0 size:22x50
          offset:0,0 size:11x20
        offset:22,0 size:33x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, BlockWithTopMarginInThreeColumns) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:50px; height:70px;"></div>
        <div style="margin-top:10px; width:60px; height:150px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:50x70
        offset:0,80 size:60x20
      offset:110,0 size:100x100
        offset:0,0 size:60x100
      offset:220,0 size:100x100
        offset:0,0 size:60x30
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, BlockStartAtColumnBoundary) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:50px; height:100px;"></div>
        <div style="width:60px; height:100px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:50x100
      offset:110,0 size:100x100
        offset:0,0 size:60x100
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, NestedBlockAfterBlock) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="height:10px;"></div>
        <div>
          <div style="width:60px; height:120px;"></div>
          <div style="width:50px; height:20px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x10
        offset:0,10 size:100x90
          offset:0,0 size:60x90
      offset:110,0 size:100x100
        offset:0,0 size:100x50
          offset:0,0 size:60x30
          offset:0,30 size:50x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, BreakInsideAvoid) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:10px; height:50px;"></div>
        <div style="break-inside:avoid; width:20px; height:70px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:10x50
      offset:110,0 size:100x100
        offset:0,0 size:20x70
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, BreakInsideAvoidColumn) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:10px; height:50px;"></div>
        <div style="break-inside:avoid-column; width:20px; height:70px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:10x50
      offset:110,0 size:100x100
        offset:0,0 size:20x70
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, BreakInsideAvoidPage) {
  // break-inside:avoid-page has no effect, unless we're breaking into pages.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:10px; height:50px;"></div>
        <div style="break-inside:avoid-page; width:20px; height:70px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:10x50
        offset:0,50 size:20x50
      offset:110,0 size:100x100
        offset:0,0 size:20x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, BreakInsideAvoidTallBlock) {
  // The block that has break-inside:avoid is too tall to fit in one
  // fragmentainer. So a break is unavoidable. Let's check that:
  // 1. The block is still shifted to the start of the next fragmentainer
  // 2. We give up shifting it any further (would cause infinite an loop)
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:10px; height:50px;"></div>
        <div style="break-inside:avoid; width:20px; height:170px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:10x50
      offset:110,0 size:100x100
        offset:0,0 size:20x100
      offset:220,0 size:100x100
        offset:0,0 size:20x70
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, NestedBreakInsideAvoid) {
  // If there were no break-inside:avoid on the outer DIV here, there'd be a
  // break between the two inner ones, since they wouldn't both fit in the first
  // column. However, since the outer DIV does have such a declaration,
  // everything is supposed to be pushed to the second column, with no space
  // between the children.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:10px; height:50px;"></div>
        <div style="break-inside:avoid; width:30px;">
          <div style="break-inside:avoid; width:21px; height:30px;"></div>
          <div style="break-inside:avoid; width:22px; height:30px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:10x50
      offset:110,0 size:100x100
        offset:0,0 size:30x60
          offset:0,0 size:21x30
          offset:0,30 size:22x30
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, NestedBreakInsideAvoidTall) {
  // Here the outer DIV with break-inside:avoid is too tall to fit where it
  // occurs naturally, so it needs to be pushed to the second column. It's not
  // going to fit fully there either, though, since its two children don't fit
  // together. Its second child wants to avoid breaks inside, so it will be
  // moved to the third column.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:10px; height:50px;"></div>
        <div style="break-inside:avoid; width:30px;">
          <div style="width:21px; height:30px;"></div>
          <div style="break-inside:avoid; width:22px; height:80px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:10x50
      offset:110,0 size:100x100
        offset:0,0 size:30x100
          offset:0,0 size:21x30
      offset:220,0 size:100x100
        offset:0,0 size:30x80
          offset:0,0 size:22x80
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, BreakInsideAvoidAtColumnBoundary) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="height:90px;"></div>
        <div>
          <div style="break-inside:avoid; width:20px; height:20px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x90
      offset:110,0 size:100x100
        offset:0,0 size:100x20
          offset:0,0 size:20x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, MarginTopPastEndOfFragmentainer) {
  // A block whose border box would start past the end of the current
  // fragmentainer should start exactly at the start of the next fragmentainer,
  // discarding what's left of the margin.
  // https://www.w3.org/TR/css-break-3/#break-margins
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="height:90px;"></div>
        <div style="margin-top:20px; width:20px; height:20px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x90
      offset:110,0 size:100x100
        offset:0,0 size:20x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, MarginBottomPastEndOfFragmentainer) {
  // A block whose border box would start past the end of the current
  // fragmentainer should start exactly at the start of the next fragmentainer,
  // discarding what's left of the margin.
  // https://www.w3.org/TR/css-break-3/#break-margins
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="margin-bottom:20px; height:90px;"></div>
        <div style="width:20px; height:20px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x90
      offset:110,0 size:100x100
        offset:0,0 size:20x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, MarginTopAtEndOfFragmentainer) {
  // A block whose border box is flush with the end of the fragmentainer
  // shouldn't produce an empty fragment there - only one fragment in the next
  // fragmentainer.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="height:90px;"></div>
        <div style="margin-top:10px; width:20px; height:20px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x90
      offset:110,0 size:100x100
        offset:0,0 size:20x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, MarginBottomAtEndOfFragmentainer) {
  // A block whose border box is flush with the end of the fragmentainer
  // shouldn't produce an empty fragment there - only one fragment in the next
  // fragmentainer.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="margin-bottom:10px; height:90px;"></div>
        <div style="width:20px; height:20px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x90
      offset:110,0 size:100x100
        offset:0,0 size:20x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, LinesInMulticolExtraSpace) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 50px;
        line-height: 20px;
        orphans: 1;
        widows: 1;
      }
    </style>
    <div id="container">
      <div id="parent">
        <br>
        <br>
        <br>
        <br>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x50
    offset:0,0 size:320x50
      offset:0,0 size:100x50
        offset:0,0 size:100x50
          offset:0,0 size:0x20
          offset:0,20 size:0x20
      offset:110,0 size:100x50
        offset:0,0 size:100x40
          offset:0,0 size:0x20
          offset:0,20 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, LinesInMulticolExactFit) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 40px;
        line-height: 20px;
        orphans: 1;
        widows: 1;
      }
    </style>
    <div id="container">
      <div id="parent">
        <br>
        <br>
        <br>
        <br>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:320x40
      offset:0,0 size:100x40
        offset:0,0 size:100x40
          offset:0,0 size:0x20
          offset:0,20 size:0x20
      offset:110,0 size:100x40
        offset:0,0 size:100x40
          offset:0,0 size:0x20
          offset:0,20 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, LinesInMulticolChildExtraSpace) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 50px;
        line-height: 20px;
        orphans: 1;
        widows: 1;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:77px;">
          <br>
          <br>
          <br>
          <br>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x50
    offset:0,0 size:320x50
      offset:0,0 size:100x50
        offset:0,0 size:77x50
          offset:0,0 size:0x20
          offset:0,20 size:0x20
      offset:110,0 size:100x50
        offset:0,0 size:77x40
          offset:0,0 size:0x20
          offset:0,20 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, LinesInMulticolChildExactFit) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 40px;
        line-height: 20px;
        orphans: 1;
        widows: 1;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:77px;">
          <br>
          <br>
          <br>
          <br>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:320x40
      offset:0,0 size:100x40
        offset:0,0 size:77x40
          offset:0,0 size:0x20
          offset:0,20 size:0x20
      offset:110,0 size:100x40
        offset:0,0 size:77x40
          offset:0,0 size:0x20
          offset:0,20 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, LinesInMulticolChildNoSpaceForFirst) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 50px;
        line-height: 20px;
        orphans: 1;
        widows: 1;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="height:50px;"></div>
        <div style="width:77px;">
          <br>
          <br>
          <br>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x50
    offset:0,0 size:320x50
      offset:0,0 size:100x50
        offset:0,0 size:100x50
      offset:110,0 size:100x50
        offset:0,0 size:77x50
          offset:0,0 size:0x20
          offset:0,20 size:0x20
      offset:220,0 size:100x50
        offset:0,0 size:77x20
          offset:0,0 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest,
       LinesInMulticolChildInsufficientSpaceForFirst) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 50px;
        line-height: 20px;
        orphans: 1;
        widows: 1;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="height:40px;"></div>
        <div style="width:77px;">
          <br>
          <br>
          <br>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x50
    offset:0,0 size:320x50
      offset:0,0 size:100x50
        offset:0,0 size:100x40
      offset:110,0 size:100x50
        offset:0,0 size:77x50
          offset:0,0 size:0x20
          offset:0,20 size:0x20
      offset:220,0 size:100x50
        offset:0,0 size:77x20
          offset:0,0 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, LineAtColumnBoundaryInFirstBlock) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 50px;
        line-height: 20px;
        orphans: 1;
        widows: 1;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:66px; padding-top:40px;">
          <br>
        </div>
      </div>
    </div>
  )HTML");

  // It's not ideal to break before a first child that's flush with the content
  // edge of its container, but if there are no earlier break opportunities, we
  // may still have to do that. There's no class A, B or C break point [1]
  // between the DIV and the line established for the BR, but since a line is
  // monolithic content [1], we really have to try to avoid breaking inside it.
  //
  // [1] https://www.w3.org/TR/css-break-3/#possible-breaks

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x50
    offset:0,0 size:320x50
      offset:0,0 size:100x50
        offset:0,0 size:66x50
      offset:110,0 size:100x50
        offset:0,0 size:66x20
          offset:0,0 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, LinesAndFloatsMulticol) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 70px;
        line-height: 20px;
        orphans: 1;
        widows: 1;
      }
    </style>
    <div id="container">
      <div id="parent">
        <br>
        <div style="float:left; width:10px; height:120px;"></div>
        <br>
        <div style="float:left; width:11px; height:120px;"></div>
        <br>
        <br>
        <br>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x70
    offset:0,0 size:320x70
      offset:0,0 size:100x70
        offset:0,0 size:100x70
          offset:0,0 size:0x20
          offset:10,20 size:0x20
          offset:21,40 size:0x20
      offset:110,0 size:100x70
        offset:0,0 size:100x40
          offset:0,0 size:10x70
          offset:10,0 size:11x70
          offset:21,0 size:0x20
          offset:21,20 size:0x20
      offset:220,0 size:100x70
        offset:0,0 size:100x0
          offset:0,0 size:11x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, FloatBelowLastLineInColumn) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 70px;
        line-height: 20px;
        orphans: 1;
        widows: 1;
      }
    </style>
    <div id="container">
      <div id="parent">
        <br>
        <br>
        <br>
        <div style="float:left; width:11px; height:120px;"></div>
        <br>
        <br>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x70
    offset:0,0 size:320x70
      offset:0,0 size:100x70
        offset:0,0 size:100x70
          offset:0,0 size:0x20
          offset:0,20 size:0x20
          offset:0,40 size:0x20
      offset:110,0 size:100x70
        offset:0,0 size:100x40
          offset:11,0 size:0x20
          offset:11,20 size:0x20
      offset:220,0 size:100x70
        offset:0,0 size:100x0
          offset:0,0 size:11x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, Orphans) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 90px;
        line-height: 20px;
        orphans: 3;
        widows: 1;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="height:40px;"></div>
        <div style="width:77px;">
          <br>
          <br>
          <br>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x90
    offset:0,0 size:320x90
      offset:0,0 size:100x90
        offset:0,0 size:100x40
      offset:110,0 size:100x90
        offset:0,0 size:77x60
          offset:0,0 size:0x20
          offset:0,20 size:0x20
          offset:0,40 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, OrphansUnsatisfiable) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 90px;
        line-height: 20px;
        orphans: 100;
        widows: 1;
      }
    </style>
    <div id="container">
      <div id="parent">
        <br>
        <br>
        <br>
        <br>
        <br>
        <br>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x90
    offset:0,0 size:320x90
      offset:0,0 size:100x90
        offset:0,0 size:100x90
          offset:0,0 size:0x20
          offset:0,20 size:0x20
          offset:0,40 size:0x20
          offset:0,60 size:0x20
      offset:110,0 size:100x90
        offset:0,0 size:100x40
          offset:0,0 size:0x20
          offset:0,20 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, Widows) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 110px;
        line-height: 20px;
        orphans: 1;
        widows: 3;
      }
    </style>
    <div id="container">
      <div id="parent">
        <br>
        <br>
        <br>
        <br>
        <br>
        <br>
        <br>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x110
    offset:0,0 size:320x110
      offset:0,0 size:100x110
        offset:0,0 size:100x110
          offset:0,0 size:0x20
          offset:0,20 size:0x20
          offset:0,40 size:0x20
          offset:0,60 size:0x20
      offset:110,0 size:100x110
        offset:0,0 size:100x60
          offset:0,0 size:0x20
          offset:0,20 size:0x20
          offset:0,40 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, WidowsUnsatisfiable) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 90px;
        line-height: 20px;
        orphans: 1;
        widows: 100;
      }
    </style>
    <div id="container">
      <div id="parent">
        <br>
        <br>
        <br>
        <br>
        <br>
        <br>
        <br>
        <br>
        <br>
        <br>
        <br>
        <br>
        <br>
        <br>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x90
    offset:0,0 size:320x90
      offset:0,0 size:100x90
        offset:0,0 size:100x90
          offset:0,0 size:0x20
      offset:110,0 size:100x90
        offset:0,0 size:100x90
          offset:0,0 size:0x20
          offset:0,20 size:0x20
          offset:0,40 size:0x20
          offset:0,60 size:0x20
      offset:220,0 size:100x90
        offset:0,0 size:100x90
          offset:0,0 size:0x20
          offset:0,20 size:0x20
          offset:0,40 size:0x20
          offset:0,60 size:0x20
      offset:330,0 size:100x90
        offset:0,0 size:100x90
          offset:0,0 size:0x20
          offset:0,20 size:0x20
          offset:0,40 size:0x20
          offset:0,60 size:0x20
      offset:440,0 size:100x90
        offset:0,0 size:100x20
          offset:0,0 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, OrphansAndUnsatisfiableWidows) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 70px;
        line-height: 20px;
        orphans: 2;
        widows: 3;
      }
    </style>
    <div id="container">
      <div id="parent">
        <br>
        <br>
        <br>
        <br>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x70
    offset:0,0 size:320x70
      offset:0,0 size:100x70
        offset:0,0 size:100x70
          offset:0,0 size:0x20
          offset:0,20 size:0x20
      offset:110,0 size:100x70
        offset:0,0 size:100x40
          offset:0,0 size:0x20
          offset:0,20 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, UnsatisfiableOrphansAndWidows) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 70px;
        line-height: 20px;
        orphans: 4;
        widows: 4;
      }
    </style>
    <div id="container">
      <div id="parent">
        <br>
        <br>
        <br>
        <br>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x70
    offset:0,0 size:320x70
      offset:0,0 size:100x70
        offset:0,0 size:100x70
          offset:0,0 size:0x20
          offset:0,20 size:0x20
          offset:0,40 size:0x20
      offset:110,0 size:100x70
        offset:0,0 size:100x20
          offset:0,0 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, WidowsAndAbspos) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 70px;
        line-height: 20px;
        orphans: 1;
        widows: 3;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="position:relative;">
          <br>
          <br>
          <br>
          <br>
          <div style="position:absolute; width:33px; height:33px;"></div>
          <br>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x70
    offset:0,0 size:320x70
      offset:0,0 size:100x70
        offset:0,0 size:100x70
          offset:0,0 size:0x20
          offset:0,20 size:0x20
      offset:110,0 size:100x70
        offset:0,0 size:100x60
          offset:0,0 size:0x20
          offset:0,20 size:0x20
          offset:0,40 size:0x20
        offset:0,40 size:33x30
      offset:220,0 size:100x70
        offset:0,0 size:33x3
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, BreakBetweenLinesNotBefore) {
  // Just breaking where we run out of space is perfect, since it won't violate
  // the orphans/widows requirement, since there'll be two lines both before and
  // after the break.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
        line-height: 20px;
        orphans: 2;
        widows: 2;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:44px; height:60px;"></div>
        <div style="width:55px;">
          <br>
          <br>
          <br>
          <br>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:44x60
        offset:0,60 size:55x40
          offset:0,0 size:0x20
          offset:0,20 size:0x20
      offset:110,0 size:100x100
        offset:0,0 size:55x40
          offset:0,0 size:0x20
          offset:0,20 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, BreakBetweenLinesNotBefore2) {
  // Prefer breaking between lines and violate an orphans requirement, rather
  // than violating break-before:avoid.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
        line-height: 20px;
        orphans: 2;
        widows: 1;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:44px; height:80px;"></div>
        <div style="break-before:avoid; width:55px;">
          <br>
          <br>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:44x80
        offset:0,80 size:55x20
          offset:0,0 size:0x20
      offset:110,0 size:100x100
        offset:0,0 size:55x20
          offset:0,0 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, BreakBetweenLinesNotBefore3) {
  // Prefer breaking between lines and violate a widows requirement, rather than
  // violating break-before:avoid.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
        line-height: 20px;
        orphans: 1;
        widows: 2;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:44px; height:80px;"></div>
        <div style="break-before:avoid; width:55px;">
          <br>
          <br>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:44x80
        offset:0,80 size:55x20
          offset:0,0 size:0x20
      offset:110,0 size:100x100
        offset:0,0 size:55x20
          offset:0,0 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, FloatInBlockMovedByOrphans) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 70px;
        line-height: 20px;
        orphans: 2;
        widows: 1;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:11px; height:40px;"></div>
        <div style="width:77px;">
          <br>
          <div style="float:left; width:10px; height:10px;"></div>
          <br>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x70
    offset:0,0 size:320x70
      offset:0,0 size:100x70
        offset:0,0 size:11x40
      offset:110,0 size:100x70
        offset:0,0 size:77x40
          offset:0,0 size:0x20
          offset:10,20 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, FloatMovedWithWidows) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 90px;
        line-height: 20px;
        orphans: 1;
        widows: 4;
      }
    </style>
    <div id="container">
      <div id="parent">
        <br>
        <br>
        <br>
        <div style="float:left; width:10px; height:10px;"></div>
        <br>
        <br>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x90
    offset:0,0 size:320x90
      offset:0,0 size:100x90
        offset:0,0 size:100x90
          offset:0,0 size:0x20
      offset:110,0 size:100x90
        offset:0,0 size:100x80
          offset:0,0 size:0x20
          offset:0,20 size:0x20
          offset:10,40 size:0x20
          offset:0,60 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, BorderAndPadding) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent" style="border:3px solid; padding:2px;">
        <div style="width:30px; height:150px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x110
    offset:0,0 size:330x110
      offset:5,5 size:100x100
        offset:0,0 size:30x100
      offset:115,5 size:100x100
        offset:0,0 size:30x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, BreakInsideWithBorder) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="height:85px;"></div>
        <div style="border:10px solid;">
          <div style="height:10px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x85
        offset:0,85 size:100x15
          offset:10,10 size:80x5
      offset:110,0 size:100x100
        offset:0,0 size:100x15
          offset:10,0 size:80x5
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ForcedBreaks) {
  // This tests that forced breaks are honored, but only at valid class A break
  // points (i.e. *between* in-flow block siblings).
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="float:left; width:1px; height:1px;"></div>
        <div style="break-before:column; break-after:column;">
          <div style="float:left; width:1px; height:1px;"></div>
          <div style="break-after:column; width:50px; height:10px;"></div>
          <div style="break-before:column; width:60px; height:10px;"></div>
          <div>
            <div>
              <div style="break-after:column; width:70px; height:10px;"></div>
            </div>
          </div>
          <div style="width:80px; height:10px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:1x1
        offset:0,0 size:100x100
          offset:1,0 size:1x1
          offset:0,0 size:50x10
      offset:110,0 size:100x100
        offset:0,0 size:100x100
          offset:0,0 size:60x10
          offset:0,10 size:100x10
            offset:0,0 size:100x10
              offset:0,0 size:70x10
      offset:220,0 size:100x100
        offset:0,0 size:100x10
          offset:0,0 size:80x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ForcedBreakInSecondChild) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:33px; height:20px;"></div>
        <div style="width:34px;">
          <div style="width:35px; height:20px;"></div>
          <div style="break-before:column; width:36px; height:20px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:33x20
        offset:0,20 size:34x80
          offset:0,0 size:35x20
      offset:110,0 size:100x100
        offset:0,0 size:34x20
          offset:0,0 size:36x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ForcedAndUnforcedBreaksAtSameBoundary) {
  // We have two parallel flows, one with a forced break inside and one with an
  // unforced break. Check that we handle the block-start margins correctly
  // (i.e. truncate at unforced breaks but not at forced breaks).
  //
  // Note about the #blockchildifier DIV in the test: it's there to force block
  // layout, as our fragmentation support for floats inside an inline formatting
  // context is borked; see crbug.com/915929
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="blockchildifier"></div>
        <div style="float:left; width:33px;">
          <div style="width:10px; height:70px;"></div>
          <div style="break-before:column; margin-top:50px; width:20px; height:20px;"></div>
       </div>
       <div style="float:left; width:34px;">
         <div style="width:10px; height:70px;"></div>
        <div style="margin-top:50px; width:20px; height:20px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x0
        offset:0,0 size:33x100
          offset:0,0 size:10x70
        offset:33,0 size:34x100
          offset:0,0 size:10x70
      offset:110,0 size:100x100
        offset:0,0 size:33x70
          offset:0,50 size:20x20
        offset:33,0 size:34x20
          offset:0,0 size:20x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ResumeInsideFormattingContextRoot) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="display:flow-root; width:33px;">
          <div style="width:10px; height:70px;"></div>
          <div style="margin-top:50px; width:20px; height:20px;"></div>
       </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:33x100
          offset:0,0 size:10x70
      offset:110,0 size:100x100
        offset:0,0 size:33x20
          offset:0,0 size:20x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, NewFcAtColumnBoundary) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:22px; height:100px;"></div>
        <div style="display:flow-root; width:33px; height:50px;"></div>
       </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:22x100
      offset:110,0 size:100x100
        offset:0,0 size:33x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, NewFcWithMargin) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:22px; height:50px;"></div>
        <div style="display:flow-root; margin-top:30px; width:33px; height:50px;"></div>
       </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:22x50
        offset:0,80 size:33x20
      offset:110,0 size:100x100
        offset:0,0 size:33x30
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, NewFcBelowFloat) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="float:left; width:22px; height:50px;"></div>
        <div style="display:flow-root; margin-top:40px; width:88px; height:70px;"></div>
       </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:22x50
        offset:0,50 size:88x50
      offset:110,0 size:100x100
        offset:0,0 size:88x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, NewFcWithMarginPastColumnBoundary) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:22px; height:80px;"></div>
        <div style="display:flow-root; margin-top:30px; width:33px; height:50px;"></div>
       </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:22x80
      offset:110,0 size:100x100
        offset:0,0 size:33x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, MinMax) {
  // The multicol container here contains two inline-blocks with a line break
  // opportunity between them. We'll test what min/max values we get for the
  // multicol container when specifying both column-count and column-width, only
  // column-count, and only column-width.
  SetBodyInnerHTML(R"HTML(
    <style>
      #multicol {
        column-gap: 10px;
        width: fit-content;
      }
      #multicol span { display:inline-block; width:50px; height:50px; }
    </style>
    <div id="container">
      <div id="multicol">
        <div>
          <span></span><wbr><span></span>
        </div>
      </div>
    </div>
  )HTML");

  LayoutObject* layout_object = GetLayoutObjectByElementId("multicol");
  ASSERT_TRUE(layout_object);
  NGBlockNode node = NGBlockNode(To<LayoutBox>(layout_object));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize));
  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /* break_token */ nullptr);
  NGColumnLayoutAlgorithm algorithm({node, fragment_geometry, space});
  absl::optional<MinMaxSizes> sizes;

  // Both column-count and column-width set. See
  // https://www.w3.org/TR/2016/WD-css-sizing-3-20160510/#multicol-intrinsic
  // (which is the only thing resembling spec that we currently have); in
  // particular, if column-width is non-auto, we ignore column-count for min
  // inline-size, and also clamp it down to the specified column-width.
  ComputedStyleBuilder builder(layout_object->StyleRef());
  builder.SetColumnCount(3);
  builder.SetColumnWidth(80);
  layout_object->SetStyle(builder.TakeStyle(),
                          LayoutObject::ApplyStyleChanges::kNo);
  sizes = algorithm.ComputeMinMaxSizes(MinMaxSizesFloatInput()).sizes;
  ASSERT_TRUE(sizes.has_value());
  EXPECT_EQ(LayoutUnit(50), sizes->min_size);
  EXPECT_EQ(LayoutUnit(320), sizes->max_size);

  // Only column-count set.
  builder = ComputedStyleBuilder(layout_object->StyleRef());
  builder.SetHasAutoColumnWidth();
  layout_object->SetStyle(builder.TakeStyle(),
                          LayoutObject::ApplyStyleChanges::kNo);
  sizes = algorithm.ComputeMinMaxSizes(MinMaxSizesFloatInput()).sizes;
  ASSERT_TRUE(sizes.has_value());
  EXPECT_EQ(LayoutUnit(170), sizes->min_size);
  EXPECT_EQ(LayoutUnit(320), sizes->max_size);

  // Only column-width set.
  builder = ComputedStyleBuilder(layout_object->StyleRef());
  builder.SetColumnWidth(80);
  builder.SetHasAutoColumnCount();
  layout_object->SetStyle(builder.TakeStyle(),
                          LayoutObject::ApplyStyleChanges::kNo);
  sizes = algorithm.ComputeMinMaxSizes(MinMaxSizesFloatInput()).sizes;
  ASSERT_TRUE(sizes.has_value());
  EXPECT_EQ(LayoutUnit(50), sizes->min_size);
  EXPECT_EQ(LayoutUnit(100), sizes->max_size);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancing) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent" style="border:3px solid; padding:2px;">
        <div style="width:30px; height:150px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x60
    offset:0,0 size:330x60
      offset:5,5 size:100x50
        offset:0,0 size:30x50
      offset:115,5 size:100x50
        offset:0,0 size:30x50
      offset:225,5 size:100x50
        offset:0,0 size:30x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingFixedHeightExactMatch) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        height: 50px;
      }
    </style>
    <div id="container">
      <div id="parent" style="border:3px solid; padding:2px;">
        <div style="width:30px; height:150px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x60
    offset:0,0 size:330x60
      offset:5,5 size:100x50
        offset:0,0 size:30x50
      offset:115,5 size:100x50
        offset:0,0 size:30x50
      offset:225,5 size:100x50
        offset:0,0 size:30x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingFixedHeightLessContent) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent" style="border:3px solid; padding:2px;">
        <div style="width:30px; height:150px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x110
    offset:0,0 size:330x110
      offset:5,5 size:100x50
        offset:0,0 size:30x50
      offset:115,5 size:100x50
        offset:0,0 size:30x50
      offset:225,5 size:100x50
        offset:0,0 size:30x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest,
       ColumnBalancingFixedHeightOverflowingContent) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        height: 35px;
      }
    </style>
    <div id="container">
      <div id="parent" style="border:3px solid; padding:2px;">
        <div style="width:30px; height:150px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x45
    offset:0,0 size:330x45
      offset:5,5 size:100x35
        offset:0,0 size:30x35
      offset:115,5 size:100x35
        offset:0,0 size:30x35
      offset:225,5 size:100x35
        offset:0,0 size:30x35
      offset:335,5 size:100x35
        offset:0,0 size:30x35
      offset:445,5 size:100x35
        offset:0,0 size:30x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingMinHeight) {
  // Min-height has no effect on the columns, only on the multicol
  // container. Balanced columns should never be taller than they have to be.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        min-height:70px;
      }
    </style>
    <div id="container">
      <div id="parent" style="border:3px solid; padding:2px;">
        <div style="width:30px; height:150px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x80
    offset:0,0 size:330x80
      offset:5,5 size:100x50
        offset:0,0 size:30x50
      offset:115,5 size:100x50
        offset:0,0 size:30x50
      offset:225,5 size:100x50
        offset:0,0 size:30x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingMaxHeight) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        max-height:40px;
      }
    </style>
    <div id="container">
      <div id="parent" style="border:3px solid; padding:2px;">
        <div style="width:30px; height:150px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x50
    offset:0,0 size:330x50
      offset:5,5 size:100x40
        offset:0,0 size:30x40
      offset:115,5 size:100x40
        offset:0,0 size:30x40
      offset:225,5 size:100x40
        offset:0,0 size:30x40
      offset:335,5 size:100x40
        offset:0,0 size:30x30
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest,
       ColumnBalancingMinHeightLargerThanMaxHeight) {
  // Min-height has no effect on the columns, only on the multicol
  // container. Balanced columns should never be taller than they have to be.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        min-height:70px;
        max-height:50px;
      }
    </style>
    <div id="container">
      <div id="parent" style="border:3px solid; padding:2px;">
        <div style="width:30px; height:150px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x80
    offset:0,0 size:330x80
      offset:5,5 size:100x50
        offset:0,0 size:30x50
      offset:115,5 size:100x50
        offset:0,0 size:30x50
      offset:225,5 size:100x50
        offset:0,0 size:30x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingFixedHeightMinHeight) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        height:40px;
        max-height:30px;
      }
    </style>
    <div id="container">
      <div id="parent" style="border:3px solid; padding:2px;">
        <div style="width:30px; height:150px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:330x40
      offset:5,5 size:100x30
        offset:0,0 size:30x30
      offset:115,5 size:100x30
        offset:0,0 size:30x30
      offset:225,5 size:100x30
        offset:0,0 size:30x30
      offset:335,5 size:100x30
        offset:0,0 size:30x30
      offset:445,5 size:100x30
        offset:0,0 size:30x30
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancing100By3) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent { columns: 3; }
    </style>
    <div id="container">
      <div id="parent">
        <div style="height:100px;"></div>
      </div>
    </div>
  )HTML");

  const NGPhysicalBoxFragment* parent_fragment =
      RunBlockLayoutAlgorithm(GetElementById("container"));

  FragmentChildIterator iterator(parent_fragment);
  const auto* multicol = iterator.NextChild();
  ASSERT_TRUE(multicol);

  // Actual column-count should be 3. I.e. no overflow columns.
  EXPECT_EQ(3U, multicol->Children().size());
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingEmpty) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent"></div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x0
    offset:0,0 size:320x0
      offset:0,0 size:100x0
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingEmptyBlock) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:20px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x0
    offset:0,0 size:320x0
      offset:0,0 size:100x0
        offset:0,0 size:20x0
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingSingleLine) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        line-height: 20px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <br>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x20
    offset:0,0 size:320x20
      offset:0,0 size:100x20
        offset:0,0 size:100x20
          offset:0,0 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingSingleLineInNested) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        line-height: 20px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="columns:2; column-gap:10px;">
          <br>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x20
    offset:0,0 size:320x20
      offset:0,0 size:100x20
        offset:0,0 size:100x20
          offset:0,0 size:45x20
            offset:0,0 size:45x20
              offset:0,0 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingSingleLineInNestedSpanner) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        line-height: 20px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="columns:2; column-gap:0;">
          <div style="column-span:all;">
            <br>
          </div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x20
    offset:0,0 size:320x20
      offset:0,0 size:100x20
        offset:0,0 size:100x20
          offset:0,0 size:50x0
          offset:0,0 size:100x20
            offset:0,0 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingOverflow) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:30px; height:20px;">
          <div style="width:33px; height:300px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:30x20
          offset:0,0 size:33x100
      offset:110,0 size:100x100
        offset:0,0 size:30x0
          offset:0,0 size:33x100
      offset:220,0 size:100x100
        offset:0,0 size:30x0
          offset:0,0 size:33x100
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingLines) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        line-height: 20px;
        orphans: 1;
        widows: 1;
      }
    </style>
    <div id="container">
      <div id="parent">
        <br><br><br><br><br>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:320x40
      offset:0,0 size:100x40
        offset:0,0 size:100x40
          offset:0,0 size:0x20
          offset:0,20 size:0x20
      offset:110,0 size:100x40
        offset:0,0 size:100x40
          offset:0,0 size:0x20
          offset:0,20 size:0x20
      offset:220,0 size:100x40
        offset:0,0 size:100x20
          offset:0,0 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingLinesOrphans) {
  // We have 6 lines and 3 columns. If we make the columns tall enough to hold 2
  // lines each, it should all fit. But then there's an orphans request that 3
  // lines be placed together in the same column...
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        line-height: 20px;
        orphans: 1;
        widows: 1;
      }
    </style>
    <div id="container">
      <div id="parent">
        <br>
        <div style="orphans:3;">
           <br><br><br>
        </div>
        <br><br>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x60
    offset:0,0 size:320x60
      offset:0,0 size:100x60
        offset:0,0 size:100x20
          offset:0,0 size:0x20
      offset:110,0 size:100x60
        offset:0,0 size:100x60
          offset:0,0 size:0x20
          offset:0,20 size:0x20
          offset:0,40 size:0x20
      offset:220,0 size:100x60
        offset:0,0 size:100x40
          offset:0,0 size:0x20
          offset:0,20 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingLinesForcedBreak) {
  // We have 6 lines and 3 columns. If we make the columns tall enough to hold 2
  // lines each, it should all fit. But then there's a forced break after the
  // first line, so that the remaining 5 lines have to be distributed into the 2
  // remaining columns...
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        line-height: 20px;
        orphans: 1;
        widows: 1;
      }
    </style>
    <div id="container">
      <div id="parent">
        <br>
        <div style="break-before:column;">
           <br><br><br><br><br>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x60
    offset:0,0 size:320x60
      offset:0,0 size:100x60
        offset:0,0 size:100x20
          offset:0,0 size:0x20
      offset:110,0 size:100x60
        offset:0,0 size:100x60
          offset:0,0 size:0x20
          offset:0,20 size:0x20
          offset:0,40 size:0x20
      offset:220,0 size:100x60
        offset:0,0 size:100x40
          offset:0,0 size:0x20
          offset:0,20 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingLinesForcedBreak2) {
  // We have 7+5 lines and 3 columns. There's a forced break after 7 lines, then
  // 5 more lines. There will be another implicit break among the first 7 lines,
  // while the columns will have to fit 5 lines, because of the 5 lines after
  // the forced break. The first column will have 5 lines. The second one will
  // have 2. The third one (after the break) will have 5.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        line-height: 20px;
        orphans: 1;
        widows: 1;
      }
    </style>
    <div id="container">
      <div id="parent">
        <br><br><br><br><br><br><br>
        <div style="width:99px; break-before:column;"></div>
        <br><br><br><br><br>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x100
          offset:0,0 size:0x20
          offset:0,20 size:0x20
          offset:0,40 size:0x20
          offset:0,60 size:0x20
          offset:0,80 size:0x20
      offset:110,0 size:100x100
        offset:0,0 size:100x40
          offset:0,0 size:0x20
          offset:0,20 size:0x20
      offset:220,0 size:100x100
        offset:0,0 size:99x0
        offset:0,0 size:100x100
          offset:0,0 size:0x20
          offset:0,20 size:0x20
          offset:0,40 size:0x20
          offset:0,60 size:0x20
          offset:0,80 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingLinesForcedBreak3) {
  // We have 7+5 lines and 3 columns. There's a forced break after 7 lines, then
  // 5 more lines. There will be another implicit break among the first 7 lines,
  // while the columns will have to fit 5 lines, because of the 5 lines after
  // the forced break. The first column will have 5 lines. The second one will
  // have 2. The third one (after the break) will have 5. The lines are wrapped
  // inside a block child of the multicol container.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        line-height: 20px;
        orphans: 1;
        widows: 1;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:66px;">
          <br><br><br><br><br><br><br>
          <div style="width:99px; break-before:column;"></div>
          <br><br><br><br><br>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:66x100
          offset:0,0 size:66x100
            offset:0,0 size:0x20
            offset:0,20 size:0x20
            offset:0,40 size:0x20
            offset:0,60 size:0x20
            offset:0,80 size:0x20
      offset:110,0 size:100x100
        offset:0,0 size:66x100
          offset:0,0 size:66x40
            offset:0,0 size:0x20
            offset:0,20 size:0x20
      offset:220,0 size:100x100
        offset:0,0 size:66x100
          offset:0,0 size:99x0
          offset:0,0 size:66x100
            offset:0,0 size:0x20
            offset:0,20 size:0x20
            offset:0,40 size:0x20
            offset:0,60 size:0x20
            offset:0,80 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingLinesAvoidBreakInside) {
  // We have 6 lines and 3 columns. If we make the columns tall enough to hold 2
  // lines each, it should all fit. But then there's a block with 3 lines and
  // break-inside:avoid...
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        line-height: 20px;
        orphans: 1;
        widows: 1;
      }
    </style>
    <div id="container">
      <div id="parent">
        <br>
        <div style="break-inside:avoid;">
           <br><br><br>
        </div>
        <br><br>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x60
    offset:0,0 size:320x60
      offset:0,0 size:100x60
        offset:0,0 size:100x20
          offset:0,0 size:0x20
      offset:110,0 size:100x60
        offset:0,0 size:100x60
          offset:0,0 size:0x20
          offset:0,20 size:0x20
          offset:0,40 size:0x20
      offset:220,0 size:100x60
        offset:0,0 size:100x40
          offset:0,0 size:0x20
          offset:0,20 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingLinesAvoidBreakInside2) {
  // We have 5 lines and 3 columns. If we make the columns tall enough to hold 2
  // lines each, it should all fit. But then there's a block with 3 lines and
  // break-inside:avoid...
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        line-height: 20px;
        orphans: 1;
        widows: 1;
      }
    </style>
    <div id="container">
      <div id="parent">
        <br>
        <div style="break-inside:avoid;">
           <br><br><br>
        </div>
        <br>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x60
    offset:0,0 size:320x60
      offset:0,0 size:100x60
        offset:0,0 size:100x20
          offset:0,0 size:0x20
      offset:110,0 size:100x60
        offset:0,0 size:100x60
          offset:0,0 size:0x20
          offset:0,20 size:0x20
          offset:0,40 size:0x20
      offset:220,0 size:100x60
        offset:0,0 size:100x20
          offset:0,0 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingUnderflow) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="break-inside:avoid; margin-top:-100px; width:55px; height:110px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x10
    offset:0,0 size:320x10
      offset:0,0 size:100x10
        offset:0,-100 size:55x110
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ClassCBreakPointBeforeBfc) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height:100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:50px; height:50px;"></div>
        <div style="float:left; width:100%; height:40px;"></div>
        <div style="width:55px;">
          <div style="display:flow-root; break-inside:avoid; width:44px; height:60px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:50x50
        offset:0,50 size:100x40
        offset:0,50 size:55x50
      offset:110,0 size:100x100
        offset:0,0 size:55x60
          offset:0,0 size:44x60
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, NoClassCBreakPointBeforeBfc) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height:100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:50px; height:50px;"></div>
        <div style="float:left; width:100%; height:40px;"></div>
        <div id="container" style="clear:both; width:55px;">
          <div style="display:flow-root; break-inside:avoid; width:44px; height:60px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:50x50
        offset:0,50 size:100x40
      offset:110,0 size:100x100
        offset:0,0 size:55x60
          offset:0,0 size:44x60
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ClassCBreakPointBeforeBfcWithClearance) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height:100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:50px; height:50px;"></div>
        <div style="float:left; width:1px; height:40px;"></div>
        <div style="width:55px;">
          <div style="clear:both; display:flow-root; break-inside:avoid; width:44px; height:60px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:50x50
        offset:0,50 size:1x40
        offset:0,50 size:55x50
      offset:110,0 size:100x100
        offset:0,0 size:55x60
          offset:0,0 size:44x60
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ClassCBreakPointBeforeBfcWithMargin) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height:100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:50px; height:50px;"></div>
        <div style="float:left; width:100%; height:40px;"></div>
        <div style="width:55px;">
          <div style="margin-top:39px; display:flow-root; break-inside:avoid; width:44px; height:60px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:50x50
        offset:0,50 size:100x40
        offset:0,50 size:55x50
      offset:110,0 size:100x100
        offset:0,0 size:55x60
          offset:0,0 size:44x60
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest,
       ClassCBreakPointBeforeBlockMarginCollapsing) {
  // We get a class C break point here, because we get clearance, because the
  // (collapsed) margin isn't large enough to take the block below the float on
  // its own.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height:100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:50px; height:70px;"></div>
        <div style="float:left; width:100%; height:20px;"></div>
        <div style="border:1px solid; width:55px;">
          <div style="clear:left; width:44px; margin-top:10px;">
            <div style="margin-top:18px; break-inside:avoid; width:33px; height:20px;"></div>
          </div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:50x70
        offset:0,70 size:100x20
        offset:0,70 size:57x30
      offset:110,0 size:100x100
        offset:0,0 size:57x21
          offset:1,0 size:44x20
            offset:0,0 size:33x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest,
       NoClassCBreakPointBeforeBlockMarginCollapsing) {
  // No class C break point here, because there's no clearance, because the
  // (collapsed) margin is large enough to take the block below the float on its
  // own.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height:100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:50px; height:70px;"></div>
        <div style="float:left; width:100%; height:20px;"></div>
        <div style="border:1px solid; width:55px;">
          <div style="clear:left; width:44px; margin-top:10px;">
            <div style="margin-top:19px; break-inside:avoid; width:33px; height:20px;"></div>
          </div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:50x70
        offset:0,70 size:100x20
      offset:110,0 size:100x100
        offset:0,0 size:57x41
          offset:1,20 size:44x20
            offset:0,0 size:33x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ClassCBreakPointBeforeLine) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height:100px;
        line-height: 20px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:50px; height:70px;"></div>
        <div style="float:left; width:100%; height:20px;"></div>
        <div style="width:55px;">
          <div style="display:inline-block; width:33px; height:11px; vertical-align:top;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:50x70
        offset:0,70 size:100x20
        offset:0,70 size:55x30
      offset:110,0 size:100x100
        offset:0,0 size:55x20
          offset:0,0 size:33x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ForcedBreakAtClassCBreakPoint) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height:100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:50px; height:50px;"></div>
        <div style="float:left; width:100%; height:40px;"></div>
        <div style="width:55px;">
          <div style="display:flow-root; break-before:column; width:44px; height:20px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:50x50
        offset:0,50 size:100x40
        offset:0,50 size:55x50
      offset:110,0 size:100x100
        offset:0,0 size:55x20
          offset:0,0 size:44x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, Nested) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:3; height:50px; column-fill:auto; width:320px; }
      .inner { columns:2; height:100px; column-fill:auto; padding:1px; }
      .outer, .inner { column-gap:10px; }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div class="outer">
        <div class="content" style="width:5px;"></div>
        <div class="inner">
          <div class="content" style="width:10px;"></div>
          <div class="content" style="width:20px;"></div>
          <div class="content" style="width:30px;"></div>
          <div class="content" style="width:40px;"></div>
          <div class="content" style="width:50px;"></div>
          <div class="content" style="width:60px;"></div>
          <div class="content" style="width:70px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x50
    offset:0,0 size:320x50
      offset:0,0 size:100x50
        offset:0,0 size:5x20
        offset:0,20 size:100x30
          offset:1,1 size:44x29
            offset:0,0 size:10x20
          offset:55,1 size:44x29
            offset:0,0 size:20x20
      offset:110,0 size:100x50
        offset:0,0 size:100x50
          offset:1,0 size:44x50
            offset:0,0 size:30x20
            offset:0,20 size:40x20
          offset:55,0 size:44x50
            offset:0,0 size:50x20
            offset:0,20 size:60x20
      offset:220,0 size:100x50
        offset:0,0 size:100x22
          offset:1,0 size:44x21
            offset:0,0 size:70x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, NestedWithEdibleMargin) {
  // There's a block-start margin after an unforced break. It should be eaten by
  // the fragmentainer boundary.
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:3; height:50px; column-fill:auto; width:320px; }
      .inner { columns:2; height:100px; column-fill:auto; }
      .outer, .inner { column-gap:10px; }
    </style>
    <div id="container">
      <div class="outer">
        <div class="inner">
          <div style="width:5px; height:80px;"></div>
          <div style="break-inside:avoid; margin-top:30px; width:10px; height:10px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x50
    offset:0,0 size:320x50
      offset:0,0 size:100x50
        offset:0,0 size:100x50
          offset:0,0 size:45x50
            offset:0,0 size:5x50
          offset:55,0 size:45x50
            offset:0,0 size:5x30
      offset:110,0 size:100x50
        offset:0,0 size:100x50
          offset:0,0 size:45x50
            offset:0,0 size:10x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, NestedNoInnerContent) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:3; height:50px; column-fill:auto; width:320px; }
      .inner { columns:2; height:100px; column-fill:auto; padding:1px; }
      .outer, .inner { column-gap:10px; }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div class="outer">
        <div class="content" style="width:5px;"></div>
        <div class="inner"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x50
    offset:0,0 size:320x50
      offset:0,0 size:100x50
        offset:0,0 size:5x20
        offset:0,20 size:100x30
          offset:1,1 size:44x29
      offset:110,0 size:100x50
        offset:0,0 size:100x50
      offset:220,0 size:100x50
        offset:0,0 size:100x22
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, NestedSomeInnerContent) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:3; height:50px; column-fill:auto; width:320px; }
      .inner { columns:2; height:100px; column-fill:auto; padding:1px; }
      .outer, .inner { column-gap:10px; }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div class="outer">
        <div class="content" style="width:5px;"></div>
        <div class="inner">
          <div class="content" style="width:6px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x50
    offset:0,0 size:320x50
      offset:0,0 size:100x50
        offset:0,0 size:5x20
        offset:0,20 size:100x30
          offset:1,1 size:44x29
            offset:0,0 size:6x20
      offset:110,0 size:100x50
        offset:0,0 size:100x50
      offset:220,0 size:100x50
        offset:0,0 size:100x22
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, NestedLimitedHeight) {
  // This tests that we don't advance to the next outer fragmentainer when we've
  // reached the bottom of an inner multicol container. We should create inner
  // columns that overflow in the inline direction in that case.
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:2; height:50px; column-fill:auto; width:210px; }
      .inner { columns:2; height:80px; column-fill:auto; }
      .outer, .inner { column-gap:10px; }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div class="outer">
        <div class="content" style="width:5px;"></div>
        <div class="inner">
          <div class="content" style="width:10px;"></div>
          <div class="content" style="width:20px;"></div>
          <div class="content" style="width:30px;"></div>
          <div class="content" style="width:40px;"></div>
          <div class="content" style="width:50px;"></div>
          <div class="content" style="width:60px;"></div>
          <div class="content" style="width:70px;"></div>
          <div class="content" style="width:80px;"></div>
          <div class="content" style="width:90px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x50
    offset:0,0 size:210x50
      offset:0,0 size:100x50
        offset:0,0 size:5x20
        offset:0,20 size:100x30
          offset:0,0 size:45x30
            offset:0,0 size:10x20
          offset:55,0 size:45x30
            offset:0,0 size:20x20
      offset:110,0 size:100x50
        offset:0,0 size:100x50
          offset:0,0 size:45x50
            offset:0,0 size:30x20
            offset:0,20 size:40x20
          offset:55,0 size:45x50
            offset:0,0 size:50x20
            offset:0,20 size:60x20
          offset:110,0 size:45x50
            offset:0,0 size:70x20
            offset:0,20 size:80x20
          offset:165,0 size:45x50
            offset:0,0 size:90x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, NestedLimitedHeightWithPadding) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:3; width:320px; height:100px; }
      .inner { columns:2; height:100px; padding-top:50px; }
      .outer, .inner { column-gap:10px; column-fill:auto; }
    </style>
    <div id="container">
      <div class="outer">
        <div class="inner">
          <div style="width:22px; height:200px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x100
          offset:0,50 size:45x50
            offset:0,0 size:22x50
          offset:55,50 size:45x50
            offset:0,0 size:22x50
      offset:110,0 size:100x100
        offset:0,0 size:100x50
          offset:0,0 size:45x50
            offset:0,0 size:22x50
          offset:55,0 size:45x50
            offset:0,0 size:22x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, NestedUnbalancedInnerAutoHeight) {
  // The fragments generated by an inner multicol are block-size constrained by
  // the outer multicol, so if column-fill is auto, we shouldn't forcefully
  // balance.
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:2; height:50px; column-fill:auto; width:210px; }
      .inner { columns:2; column-fill:auto; }
      .outer, .inner { column-gap:10px; }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div class="outer">
        <div class="inner">
          <div class="content"></div>
          <div class="content"></div>
          <div class="content"></div>
          <div class="content"></div>
          <div class="content"></div>
          <div class="content"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x50
    offset:0,0 size:210x50
      offset:0,0 size:100x50
        offset:0,0 size:100x50
          offset:0,0 size:45x50
            offset:0,0 size:45x20
            offset:0,20 size:45x20
          offset:55,0 size:45x50
            offset:0,0 size:45x20
            offset:0,20 size:45x20
      offset:110,0 size:100x50
        offset:0,0 size:100x40
          offset:0,0 size:45x50
            offset:0,0 size:45x20
            offset:0,20 size:45x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, NestedAtOuterBoundary) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:3; height:100px; width:320px; }
      .inner { columns:2; height:50px; }
      .outer, .inner { column-gap:10px; column-fill:auto; }
    </style>
    <div id="container">
      <div class="outer">
        <div style="width:11px; height:100px;"></div>
        <div class="inner">
          <div style="width:22px; height:70px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:11x100
      offset:110,0 size:100x100
        offset:0,0 size:100x50
          offset:0,0 size:45x50
            offset:0,0 size:22x50
          offset:55,0 size:45x50
            offset:0,0 size:22x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, NestedZeroHeightAtOuterBoundary) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:3; height:100px; width:320px; }
      .inner { columns:2; }
      .outer, .inner { column-gap:10px; column-fill:auto; }
    </style>
    <div id="container">
      <div class="outer">
        <div style="width:11px; height:100px;"></div>
        <div class="inner">
          <div style="width:22px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:11x100
        offset:0,100 size:100x0
          offset:0,0 size:45x0
            offset:0,0 size:22x0
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, NestedWithMarginAtOuterBoundary) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:3; height:100px; width:320px; }
      .inner { columns:2; height:50px; margin-top:20px; }
      .outer, .inner { column-gap:10px; column-fill:auto; }
    </style>
    <div id="container">
      <div class="outer">
        <div style="width:11px; height:90px;"></div>
        <div class="inner">
          <div style="width:22px; height:70px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:11x90
      offset:110,0 size:100x100
        offset:0,0 size:100x50
          offset:0,0 size:45x50
            offset:0,0 size:22x50
          offset:55,0 size:45x50
            offset:0,0 size:22x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, NestedWithTallBorder) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:3; height:100px; width:320px; }
      .inner { columns:2; height:50px; border-top:100px solid; }
      .outer, .inner { column-gap:10px; column-fill:auto; }
    </style>
    <div id="container">
      <div class="outer">
        <div class="inner">
          <div style="width:22px; height:70px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x100
      offset:110,0 size:100x100
        offset:0,0 size:100x50
          offset:0,0 size:45x50
            offset:0,0 size:22x50
          offset:55,0 size:45x50
            offset:0,0 size:22x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, NestedWithTallSpanner) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:3; height:100px; width:320px; column-fill:auto; }
      .inner { columns:2; }
      .outer, .inner { column-gap:10px; }
    </style>
    <div id="container">
      <div class="outer">
        <div class="inner">
          <div style="column-span:all; width:22px; height:100px;"></div>
          <div style="width:22px; height:70px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x100
          offset:0,0 size:45x0
          offset:0,0 size:22x100
      offset:110,0 size:100x100
        offset:0,0 size:100x35
          offset:0,0 size:45x35
            offset:0,0 size:22x35
          offset:55,0 size:45x35
            offset:0,0 size:22x35
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, AbsposFitsInOneColumn) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      <div style="columns:3; width:320px; height:100px; column-gap:10px; column-fill:auto;">
        <div style="position:relative; width:222px; height:250px;">
          <div style="position:absolute; width:111px; height:50px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:222x100
        offset:0,0 size:111x50
      offset:110,0 size:100x100
        offset:0,0 size:222x100
      offset:220,0 size:100x100
        offset:0,0 size:222x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, Spanner) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        border: 1px solid;
      }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div id="parent">
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div style="column-span:all; height:44px;"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x126
    offset:0,0 size:322x126
      offset:1,1 size:100x40
        offset:0,0 size:100x20
        offset:0,20 size:100x20
      offset:111,1 size:100x40
        offset:0,0 size:100x20
        offset:0,20 size:100x20
      offset:221,1 size:100x40
        offset:0,0 size:100x20
      offset:1,41 size:320x44
      offset:1,85 size:100x40
        offset:0,0 size:100x20
        offset:0,20 size:100x20
      offset:111,85 size:100x40
        offset:0,0 size:100x20
        offset:0,20 size:100x20
      offset:221,85 size:100x40
        offset:0,0 size:100x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, SpannerWithContent) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        border: 1px solid;
      }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div id="parent">
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div style="column-span:all; padding:1px;">
          <div class="content"></div>
          <div class="content"></div>
          <div class="content"></div>
        </div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x144
    offset:0,0 size:322x144
      offset:1,1 size:100x40
        offset:0,0 size:100x20
        offset:0,20 size:100x20
      offset:111,1 size:100x40
        offset:0,0 size:100x20
        offset:0,20 size:100x20
      offset:221,1 size:100x40
        offset:0,0 size:100x20
      offset:1,41 size:320x62
        offset:1,1 size:318x20
        offset:1,21 size:318x20
        offset:1,41 size:318x20
      offset:1,103 size:100x40
        offset:0,0 size:100x20
        offset:0,20 size:100x20
      offset:111,103 size:100x40
        offset:0,0 size:100x20
        offset:0,20 size:100x20
      offset:221,103 size:100x40
        offset:0,0 size:100x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, TwoSpannersPercentWidth) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        border: 1px solid;
      }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div id="parent">
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div style="column-span:all; width:50%; height:44px;"></div>
        <div style="column-span:all; width:50%; height:1px;"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x127
    offset:0,0 size:322x127
      offset:1,1 size:100x40
        offset:0,0 size:100x20
        offset:0,20 size:100x20
      offset:111,1 size:100x40
        offset:0,0 size:100x20
        offset:0,20 size:100x20
      offset:221,1 size:100x40
        offset:0,0 size:100x20
      offset:1,41 size:160x44
      offset:1,85 size:160x1
      offset:1,86 size:100x40
        offset:0,0 size:100x20
        offset:0,20 size:100x20
      offset:111,86 size:100x40
        offset:0,0 size:100x20
        offset:0,20 size:100x20
      offset:221,86 size:100x40
        offset:0,0 size:100x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, SpannerNoBalancing) {
  // Even if column-fill is auto and block-size is restricted, we have to
  // balance column contents in front of a spanner (but not after).
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        height: 200px;
        width: 320px;
        border: 1px solid;
      }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div id="parent">
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div style="column-span:all; height:44px;"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x202
    offset:0,0 size:322x202
      offset:1,1 size:100x40
        offset:0,0 size:100x20
        offset:0,20 size:100x20
      offset:111,1 size:100x40
        offset:0,0 size:100x20
        offset:0,20 size:100x20
      offset:221,1 size:100x40
        offset:0,0 size:100x20
      offset:1,41 size:320x44
      offset:1,85 size:100x116
        offset:0,0 size:100x20
        offset:0,20 size:100x20
        offset:0,40 size:100x20
        offset:0,60 size:100x20
        offset:0,80 size:100x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, SpannerAtStart) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        border: 1px solid;
      }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div id="parent">
        <div style="column-span:all; height:44px;"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x86
    offset:0,0 size:322x86
      offset:1,1 size:100x0
      offset:1,1 size:320x44
      offset:1,45 size:100x40
        offset:0,0 size:100x20
        offset:0,20 size:100x20
      offset:111,45 size:100x40
        offset:0,0 size:100x20
        offset:0,20 size:100x20
      offset:221,45 size:100x40
        offset:0,0 size:100x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, SpannerAtEnd) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        border: 1px solid;
      }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div id="parent">
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div style="column-span:all; height:44px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x86
    offset:0,0 size:322x86
      offset:1,1 size:100x40
        offset:0,0 size:100x20
        offset:0,20 size:100x20
      offset:111,1 size:100x40
        offset:0,0 size:100x20
        offset:0,20 size:100x20
      offset:221,1 size:100x40
        offset:0,0 size:100x20
      offset:1,41 size:320x44
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, SpannerAlone) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        border: 1px solid;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="column-span:all; height:44px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x46
    offset:0,0 size:322x46
      offset:1,1 size:100x0
      offset:1,1 size:320x44
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, SpannerInBlock) {
  // Spanners don't have to be direct children of the multicol container, but
  // have to be defined in the same block formatting context as the one
  // established by the multicol container.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        border: 1px solid;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:11px;">
          <div style="column-span:all; height:44px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x46
    offset:0,0 size:322x46
      offset:1,1 size:100x0
        offset:0,0 size:11x0
      offset:1,1 size:320x44
      offset:1,45 size:100x0
        offset:0,0 size:11x0
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, SpannerWithSiblingsInBlock) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        border: 1px solid;
      }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:11px;">
          <div style="column-span:all; height:44px;"></div>
          <div class="content"></div>
          <div class="content"></div>
          <div class="content"></div>
          <div class="content"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x86
    offset:0,0 size:322x86
      offset:1,1 size:100x0
        offset:0,0 size:11x0
      offset:1,1 size:320x44
      offset:1,45 size:100x40
        offset:0,0 size:11x40
          offset:0,0 size:11x20
          offset:0,20 size:11x20
      offset:111,45 size:100x40
        offset:0,0 size:11x40
          offset:0,0 size:11x20
          offset:0,20 size:11x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, SpannerInBlockWithSiblings) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        border: 1px solid;
      }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:11px;">
          <div style="column-span:all; height:44px;"></div>
        </div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
        <div class="content"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x86
    offset:0,0 size:322x86
      offset:1,1 size:100x0
        offset:0,0 size:11x0
      offset:1,1 size:320x44
      offset:1,45 size:100x40
        offset:0,0 size:11x0
        offset:0,0 size:100x20
        offset:0,20 size:100x20
      offset:111,45 size:100x40
        offset:0,0 size:100x20
        offset:0,20 size:100x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, SpannerMargins) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
      }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div id="parent">
        <div style="column-span:all; margin:10px; width:33px; height:10px;"></div>
        <div class="content"></div>
        <div style="column-span:all; margin:10px auto; width:44px; height:10px;"></div>
        <div style="column-span:all; margin:20px; width:55px;"></div>
        <div style="column-span:all; margin:10px; width:66px; height:10px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x130
    offset:0,0 size:320x130
      offset:0,0 size:100x0
      offset:10,10 size:33x10
      offset:0,30 size:100x20
        offset:0,0 size:100x20
      offset:138,60 size:44x10
      offset:20,90 size:55x0
      offset:10,110 size:66x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, SpannerMarginsRtl) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        direction: rtl;
      }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div id="parent">
        <div style="column-span:all; margin:10px; width:33px; height:10px;"></div>
        <div class="content"></div>
        <div style="column-span:all; margin:10px auto; width:44px; height:10px;"></div>
        <div style="column-span:all; margin:20px; width:55px;"></div>
        <div style="column-span:all; margin:10px; width:66px; height:10px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x130
    offset:0,0 size:320x130
      offset:220,0 size:100x0
      offset:277,10 size:33x10
      offset:220,30 size:100x20
        offset:0,0 size:100x20
      offset:138,60 size:44x10
      offset:245,90 size:55x0
      offset:244,110 size:66x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, FixedSizeMulticolWithSpanner) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height: 300px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:33px; height:300px;"></div>
        <div style="column-span:all; width:44px; height:50px;"></div>
        <div style="width:55px; height:450px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x300
    offset:0,0 size:320x300
      offset:0,0 size:100x100
        offset:0,0 size:33x100
      offset:110,0 size:100x100
        offset:0,0 size:33x100
      offset:220,0 size:100x100
        offset:0,0 size:33x100
      offset:0,100 size:44x50
      offset:0,150 size:100x150
        offset:0,0 size:55x150
      offset:110,150 size:100x150
        offset:0,0 size:55x150
      offset:220,150 size:100x150
        offset:0,0 size:55x150
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, MarginAndBorderTopWithSpanner) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:22px; margin-top:200px; border-top:100px solid;">
          <div style="column-span:all; width:33px; height:100px;"></div>
          <div style="width:44px; height:300px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x500
    offset:0,0 size:320x500
      offset:0,0 size:100x300
        offset:0,200 size:22x100
      offset:0,300 size:33x100
      offset:0,400 size:100x100
        offset:0,0 size:22x100
          offset:0,0 size:44x100
      offset:110,400 size:100x100
        offset:0,0 size:22x100
          offset:0,0 size:44x100
      offset:220,400 size:100x100
        offset:0,0 size:22x100
          offset:0,0 size:44x100
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, BreakInsideSpannerWithMargins) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        column-fill: auto;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="columns:2; column-gap:0;">
          <div style="column-span:all; margin-top:10px; margin-bottom:20px; width:33px; height:100px;"></div>
          <div style="column-span:all; width:44px; height:10px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x100
          offset:0,0 size:50x0
          offset:0,10 size:33x90
      offset:110,0 size:100x100
        offset:0,0 size:100x40
          offset:0,0 size:33x10
          offset:0,30 size:44x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, InvalidSpanners) {
  // Spanners cannot exist inside new formatting context roots. They will just
  // be treated as normal column content then.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        border: 1px solid;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="float:left; width:10px;">
          <div style="column-span:all; height:30px;"></div>
        </div>
        <div style="display:flow-root;">
          <div style="column-span:all; height:30px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x12
    offset:0,0 size:322x12
      offset:1,1 size:100x10
        offset:0,0 size:10x10
          offset:0,0 size:10x10
        offset:10,0 size:90x10
          offset:0,0 size:90x10
      offset:111,1 size:100x10
        offset:0,0 size:10x10
          offset:0,0 size:10x10
        offset:10,0 size:90x10
          offset:0,0 size:90x10
      offset:221,1 size:100x10
        offset:0,0 size:10x10
          offset:0,0 size:10x10
        offset:10,0 size:90x10
          offset:0,0 size:90x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, BreakInsideSpanner) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:3; height:50px; column-fill:auto; width:320px; }
      .inner { columns:2; height:100px; column-fill:auto; padding:1px; }
      .outer, .inner { column-gap:10px; }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div class="outer">
        <div class="content"></div>
        <div class="inner">
          <div class="content"></div>
          <div class="content"></div>
          <div style="column-span:all; height:35px;"></div>
          <div class="content" style="width:7px;"></div>
          <div class="content" style="width:8px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x50
    offset:0,0 size:320x50
      offset:0,0 size:100x50
        offset:0,0 size:100x20
        offset:0,20 size:100x30
          offset:1,1 size:44x20
            offset:0,0 size:44x20
          offset:55,1 size:44x20
            offset:0,0 size:44x20
          offset:1,21 size:98x9
      offset:110,0 size:100x50
        offset:0,0 size:100x50
          offset:1,0 size:98x26
          offset:1,26 size:44x24
            offset:0,0 size:7x20
          offset:55,26 size:44x24
            offset:0,0 size:8x20
      offset:220,0 size:100x50
        offset:0,0 size:100x22
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, BreakInsideSpannerTwice) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:3; height:50px; column-fill:auto; width:320px; }
      .inner { columns:2; height:150px; column-fill:auto; padding:1px; }
      .outer, .inner { column-gap:10px; }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div class="outer">
        <div class="content"></div>
        <div class="inner">
          <div class="content"></div>
          <div class="content"></div>
          <div style="column-span:all; height:85px;"></div>
          <div class="content" style="width:7px;"></div>
          <div class="content" style="width:8px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x50
    offset:0,0 size:320x50
      offset:0,0 size:100x50
        offset:0,0 size:100x20
        offset:0,20 size:100x30
          offset:1,1 size:44x20
            offset:0,0 size:44x20
          offset:55,1 size:44x20
            offset:0,0 size:44x20
          offset:1,21 size:98x9
      offset:110,0 size:100x50
        offset:0,0 size:100x50
          offset:1,0 size:98x50
      offset:220,0 size:100x50
        offset:0,0 size:100x50
          offset:1,0 size:98x26
          offset:1,26 size:44x24
            offset:0,0 size:7x20
          offset:55,26 size:44x24
            offset:0,0 size:8x20
      offset:330,0 size:100x50
        offset:0,0 size:100x22
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, BreakInsideSpannerWithContent) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:3; height:50px; column-fill:auto; width:320px; }
      .inner { columns:2; height:98px; column-fill:auto; padding:1px; }
      .outer, .inner { column-gap:10px; }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div class="outer">
        <div class="inner">
          <div class="content"></div>
          <div class="content"></div>
          <div style="column-span:all;">
            <div style="width:3px;" class="content"></div>
            <div style="width:4px;" class="content"></div>
          </div>
          <div class="content" style="width:7px;"></div>
          <div class="content" style="width:8px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x50
    offset:0,0 size:320x50
      offset:0,0 size:100x50
        offset:0,0 size:100x50
          offset:1,1 size:44x20
            offset:0,0 size:44x20
          offset:55,1 size:44x20
            offset:0,0 size:44x20
          offset:1,21 size:98x29
            offset:0,0 size:3x20
      offset:110,0 size:100x50
        offset:0,0 size:100x50
          offset:1,0 size:98x20
            offset:0,0 size:4x20
          offset:1,20 size:44x29
            offset:0,0 size:7x20
          offset:55,20 size:44x29
            offset:0,0 size:8x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ForcedBreakBetweenSpanners) {
  // There are two spanners in a nested multicol. They could fit in the same
  // outer column, but there's a forced break between them.
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:3; height:100px; column-fill:auto; column-gap:10px; width:320px; }
      .inner { columns:2; column-gap:0; }
    </style>
    <div id="container">
      <div class="outer">
        <div class="inner">
          <div style="column-span:all; break-inside:avoid; width:55px; height:40px;"></div>
          <div style="column-span:all; break-before:column; break-inside:avoid; width:66px; height:40px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x100
          offset:0,0 size:50x0
          offset:0,0 size:55x40
      offset:110,0 size:100x100
        offset:0,0 size:100x40
          offset:0,0 size:66x40
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ForcedBreakBetweenSpanners2) {
  // There are two spanners in a nested multicol. They could fit in the same
  // outer column, but there's a forced break between them.
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:3; height:100px; column-fill:auto; column-gap:10px; width:320px; }
      .inner { columns:2; column-gap:0; }
    </style>
    <div id="container">
      <div class="outer">
        <div class="inner">
          <div style="column-span:all; break-after:column; break-inside:avoid; width:55px; height:40px;"></div>
          <div style="column-span:all; break-inside:avoid; width:66px; height:40px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x100
          offset:0,0 size:50x0
          offset:0,0 size:55x40
      offset:110,0 size:100x100
        offset:0,0 size:100x40
          offset:0,0 size:66x40
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ForcedBreakBetweenSpanners3) {
  // There are two spanners in a nested multicol. They could fit in the same
  // outer column, but there's a forced break after the last child of the first
  // spanner.
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:3; height:100px; column-fill:auto; column-gap:10px; width:320px; }
      .inner { columns:2; column-gap:0; }
    </style>
    <div id="container">
      <div class="outer">
        <div class="inner">
          <div style="column-span:all; break-inside:avoid; width:55px; height:40px;">
            <div style="width:33px; height:10px;"></div>
            <div style="break-after:column; width:44px; height:10px;"></div>
          </div>
          <div style="column-span:all; break-inside:avoid; width:66px; height:40px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x100
          offset:0,0 size:50x0
          offset:0,0 size:55x40
            offset:0,0 size:33x10
            offset:0,10 size:44x10
      offset:110,0 size:100x100
        offset:0,0 size:100x40
          offset:0,0 size:66x40
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ForcedBreakBetweenSpanners4) {
  // There are two spanners in a nested multicol. They could fit in the same
  // outer column, but there's a forced break before the first child of the
  // last spanner.
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:3; height:100px; column-fill:auto; column-gap:10px; width:320px; }
      .inner { columns:2; column-gap:0; }
    </style>
    <div id="container">
      <div class="outer">
        <div class="inner">
          <div style="column-span:all; break-inside:avoid; width:55px; height:40px;"></div>
          <div style="column-span:all; break-inside:avoid; width:66px; height:40px;">
            <div style="break-before:column; width:33px; height:10px;"></div>
            <div style="width:44px; height:10px;"></div>
          </div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x100
          offset:0,0 size:50x0
          offset:0,0 size:55x40
      offset:110,0 size:100x100
        offset:0,0 size:100x40
          offset:0,0 size:66x40
            offset:0,0 size:33x10
            offset:0,10 size:44x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ForcedBreakBetweenSpanners5) {
  // There are two spanners in a nested multicol. They could fit in the same
  // outer column, but there's a forced break between them. The second spanner
  // has a top margin, which should be retained, due to the forced break.
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:3; height:100px; column-fill:auto; column-gap:10px; width:320px; }
      .inner { columns:2; column-gap:0; }
    </style>
    <div id="container">
      <div class="outer">
        <div class="inner">
          <div style="column-span:all; break-inside:avoid; width:55px; height:40px;"></div>
          <div style="column-span:all; break-before:column; break-inside:avoid; width:66px; height:40px; margin-top:10px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x100
          offset:0,0 size:50x0
          offset:0,0 size:55x40
      offset:110,0 size:100x100
        offset:0,0 size:100x50
          offset:0,10 size:66x40
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, SoftBreakBetweenSpanners) {
  // There are two spanners in a nested multicol. They won't fit in the same
  // outer column, and we don't want to break inside. So we should break between
  // them.
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:3; height:100px; column-fill:auto; column-gap:10px; width:320px; }
      .inner { columns:2; column-gap:0; }
    </style>
    <div id="container">
      <div class="outer">
        <div class="inner">
          <div style="column-span:all; break-inside:avoid; width:55px; height:60px;"></div>
          <div style="column-span:all; break-inside:avoid; width:66px; height:60px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x100
          offset:0,0 size:50x0
          offset:0,0 size:55x60
      offset:110,0 size:100x100
        offset:0,0 size:100x60
          offset:0,0 size:66x60
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, SoftBreakBetweenSpanners2) {
  // There are two spanners in a nested multicol. They won't fit in the same
  // outer column, and we don't want to break inside. So we should break between
  // them. The second spanner has a top margin, but it should be truncated since
  // it's at a soft break.
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:3; height:100px; column-fill:auto; column-gap:10px; width:320px; }
      .inner { columns:2; column-gap:0; }
    </style>
    <div id="container">
      <div class="outer">
        <div class="inner">
          <div style="column-span:all; break-inside:avoid; width:55px; height:60px;"></div>
          <div style="column-span:all; break-inside:avoid; width:66px; height:60px; margin-top:10px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x100
          offset:0,0 size:50x0
          offset:0,0 size:55x60
      offset:110,0 size:100x100
        offset:0,0 size:100x60
          offset:0,0 size:66x60
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, AvoidSoftBreakBetweenSpanners) {
  // There are three spanners in a nested multicol. The first two could fit in
  // the same outer column, but the third one is too tall, and we also don't
  // want to break before that one.So we should break between the two first
  // spanners.
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:3; height:100px; column-fill:auto; column-gap:10px; width:320px; }
      .inner { columns:2; column-gap:0; }
    </style>
    <div id="container">
      <div class="outer">
        <div class="inner">
          <div style="column-span:all; break-inside:avoid; width:55px; height:40px;"></div>
          <div style="column-span:all; break-inside:avoid; width:66px; height:40px;"></div>
          <div style="column-span:all; break-inside:avoid; break-before:avoid; width:77px; height:60px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x100
          offset:0,0 size:50x0
          offset:0,0 size:55x40
      offset:110,0 size:100x100
        offset:0,0 size:100x100
          offset:0,0 size:66x40
          offset:0,40 size:77x60
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, AvoidSoftBreakBetweenSpanners2) {
  // There are two spanners in a nested multicol. They won't fit in the same
  // outer column, but we don't want to break inside the second one, and also
  // not between the spanners. The first spanner is breakable, so we should
  // break at the most appealing breakpoint there, i.e. before its last child.
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:3; height:100px; column-fill:auto; column-gap:10px; width:320px; }
      .inner { columns:2; column-gap:0; }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div class="outer">
        <div class="inner">
          <div style="column-span:all; width:11px;">
            <div class="content" style="width:22px;"></div>
            <div class="content" style="width:33px;"></div>
            <div class="content" style="width:44px;"></div>
          </div>
          <div style="column-span:all; break-inside:avoid; break-before:avoid; width:55px; height:60px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x100
          offset:0,0 size:50x0
          offset:0,0 size:11x100
            offset:0,0 size:22x20
            offset:0,20 size:33x20
      offset:110,0 size:100x100
        offset:0,0 size:100x80
          offset:0,0 size:11x20
            offset:0,0 size:44x20
          offset:0,20 size:55x60
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, AvoidSoftBreakBetweenSpanners3) {
  // Violate orphans and widows requests rather than break-between avoidance
  // requests.
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer {
        columns:3;
        height:100px;
        column-fill:auto;
        column-gap:10px;
        width:320px;
        line-height: 20px;
        orphans: 3;
        widows: 3;
      }
      .inner { columns:2; column-gap:0; }
    </style>
    <div id="container">
      <div class="outer">
        <div class="inner">
          <div style="column-span:all; width:11px;">
            <br>
            <br>
            <br>
          </div>
          <div style="column-span:all; break-inside:avoid; break-before:avoid; width:55px; height:60px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x100
          offset:0,0 size:50x0
          offset:0,0 size:11x100
            offset:0,0 size:0x20
            offset:0,20 size:0x20
      offset:110,0 size:100x100
        offset:0,0 size:100x80
          offset:0,0 size:11x20
            offset:0,0 size:0x20
          offset:0,20 size:55x60
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, SoftBreakBetweenRowAndSpanner) {
  // We have a nested multicol with some column content, followed by a
  // spanner. Everything won't fit in the same outer column, and we don't want
  // to break inside the spanner. Break between the row of columns and the
  // spanner.
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer {
        columns:3;
        height:100px;
        column-fill:auto;
        column-gap:10px;
        width:320px;
      }
      .inner { columns:2; column-gap:10px; }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div class="outer">
        <div class="inner">
          <div class="content" style="width:11px;"></div>
          <div class="content" style="width:22px;"></div>
          <div class="content" style="width:33px;"></div>
          <div style="column-span:all; break-inside:avoid; width:44px; height:70px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x100
          offset:0,0 size:45x40
            offset:0,0 size:11x20
            offset:0,20 size:22x20
          offset:55,0 size:45x40
            offset:0,0 size:33x20
      offset:110,0 size:100x100
        offset:0,0 size:100x70
          offset:0,0 size:44x70
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, SpannerAsMulticol) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .outer { columns:3; height:50px; column-fill:auto; width:320px; }
      .middle { columns:2; height:140px; column-fill:auto; }
      .inner { column-span:all; columns:2; height:80px; column-fill:auto; }
      .outer, .middle, .inner { column-gap:10px; }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div class="outer">
        <div class="middle">
          <div class="inner">
            <div class="content" style="width:131px;"></div>
            <div class="content" style="width:132px;"></div>
            <div class="content" style="width:133px;"></div>
            <div class="content" style="width:134px;"></div>
            <div class="content" style="width:135px;"></div>
            <div class="content" style="width:136px;"></div>
          </div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x50
    offset:0,0 size:320x50
      offset:0,0 size:100x50
        offset:0,0 size:100x50
          offset:0,0 size:45x0
          offset:0,0 size:100x50
            offset:0,0 size:45x50
              offset:0,0 size:131x20
              offset:0,20 size:132x20
            offset:55,0 size:45x50
              offset:0,0 size:133x20
              offset:0,20 size:134x20
      offset:110,0 size:100x50
        offset:0,0 size:100x50
          offset:0,0 size:100x30
            offset:0,0 size:45x30
              offset:0,0 size:135x20
            offset:55,0 size:45x30
              offset:0,0 size:136x20
      offset:220,0 size:100x50
        offset:0,0 size:100x40
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, AvoidBreakBetween) {
  // Breaking exactly where we run out of space would violate a
  // break-before:avoid rule. There's a perfect break opportunity before the
  // previous sibling, so use that one instead.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height: 100px;
      }
      .content { break-inside:avoid; height:30px; }
    </style>
    <div id="container">
      <div id="parent">
        <div class="content" style="width:81px;"></div>
        <div class="content" style="width:82px;"></div>
        <div class="content" style="width:83px;"></div>
        <div class="content" style="width:84px; break-before:avoid;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:81x30
        offset:0,30 size:82x30
      offset:110,0 size:100x100
        offset:0,0 size:83x30
        offset:0,30 size:84x30
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, AvoidAndForceBreakBetween) {
  // If we're both told to avoid and force breaking at a breakpoint, forcing
  // always wins.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height: 100px;
      }
      .content { break-inside:avoid; height:30px; }
    </style>
    <div id="container">
      <div id="parent">
        <div class="content" style="width:81px;"></div>
        <div class="content" style="width:82px;"></div>
        <div class="content" style="width:83px; break-after:column;"></div>
        <div class="content" style="width:84px; break-before:avoid;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:81x30
        offset:0,30 size:82x30
        offset:0,60 size:83x30
      offset:110,0 size:100x100
        offset:0,0 size:84x30
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, AvoidBreakBetweenInFloat) {
  // There are two parallel flows here; one for the float, and one for its
  // sibling. They don't affect each other as far as breaking is concerned.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height: 100px;
      }
      .content { break-inside:avoid; height:30px; }
    </style>
    <div id="container">
      <div id="parent">
        <div style="float:left; width:100%;">
          <div class="content" style="width:81px;"></div>
          <div class="content" style="width:82px;"></div>
          <div class="content" style="width:83px;"></div>
          <div class="content" style="width:84px; break-before:avoid;"></div>
        </div>
        <div style="height:150px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x100
          offset:0,0 size:81x30
          offset:0,30 size:82x30
        offset:0,0 size:100x100
      offset:110,0 size:100x100
        offset:0,0 size:100x60
          offset:0,0 size:83x30
          offset:0,30 size:84x30
        offset:0,0 size:100x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest,
       IgnoreBreakInsideAvoidBecauseBreakBetweenAvoid) {
  // We want to avoid breaks between all the children, and at the same time
  // avoid breaks inside of them. This is impossible to honor in this test,
  // since the content is taller than one column. There are no ideal
  // breakpoints; all are equally bad. The spec is explicit about the fact that
  // it "does not suggest a precise algorithm" when it comes to picking which
  // breaking rule to violate before others, so whether we should drop
  // break-before or break-inside first is undefined. However, the spec does
  // also mention that we should break as few times as possible, which suggests
  // that we should favor whatever gives more progression.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height: 100px;
      }
      .content { break-inside:avoid; height:30px; }
    </style>
    <div id="container">
      <div id="parent">
        <div class="content" style="width:81px;"></div>
        <div class="content" style="width:82px; break-before:avoid;"></div>
        <div class="content" style="width:83px; break-before:avoid;"></div>
        <div class="content" style="width:84px; break-before:avoid;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:81x30
        offset:0,30 size:82x30
        offset:0,60 size:83x30
        offset:0,90 size:84x10
      offset:110,0 size:100x100
        offset:0,0 size:84x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, AvoidBreakBetweenAndInsideIgnoreInside) {
  // This one isn't obvious, spec-wise, since it's not defined which rules to
  // disregard first (break-inside vs. break-before, and break-inside on a child
  // vs. on its container), but it seems right to disregard break-inside:avoid
  // on the container, and at the same time honor break avoidance specified
  // further within (smaller pieces, more progression), rather than e.g. giving
  // up on everything and breaking wherever.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height: 100px;
      }
      .content { break-inside:avoid; height:30px; }
    </style>
    <div id="container">
      <div id="parent">
        <div style="break-inside:avoid;">
          <div style="width:80px; height:20px;"></div>
          <div class="content" style="width:81px;"></div>
          <div class="content" style="width:82px;"></div>
          <div class="content" style="width:83px; break-before:avoid;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x100
          offset:0,0 size:80x20
          offset:0,20 size:81x30
      offset:110,0 size:100x100
        offset:0,0 size:100x60
          offset:0,0 size:82x30
          offset:0,30 size:83x30
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, AvoidBreakBetweenAndInside) {
  // When looking for possible breaks inside #middle, we need to take into
  // account that we're supposed to avoid breaking inside. The only breakpoint
  // that doesn't violate any rules in this test is *before* #middle.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height: 100px;
      }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div id="parent">
        <div class="content" style="width:32px;"></div>
        <div id="middle" style="break-inside:avoid; break-after:avoid;">
          <div class="content" style="width:33px;"></div>
          <div class="content" style="width:34px;"></div>
          <div class="content" style="width:35px;"></div>
          <div class="content" style="width:36px;"></div>
        </div>
        <div class="content" style="width:37px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:32x20
      offset:110,0 size:100x100
        offset:0,0 size:100x80
          offset:0,0 size:33x20
          offset:0,20 size:34x20
          offset:0,40 size:35x20
          offset:0,60 size:36x20
        offset:0,80 size:37x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, AvoidBreakBetweenInsideBreakableParent) {
  // There's a perfect breakpoint between the two direct children of the
  // multicol container - i.e. between #first and #second. We should avoid
  // breaking between between any of the children of #second (we run out of
  // space between the third and the fourth child). There are no restrictions on
  // breaking between the children inside #first, but we should progress as much
  // as possible, so the correct thing to do is to break between #first and
  // #second.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height: 100px;
      }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div id="parent">
        <div id="#first">
          <div class="content" style="width:33px;"></div>
          <div class="content" style="width:34px;"></div>
        </div>
        <div id="#second">
          <div class="content" style="width:35px;"></div>
          <div class="content" style="width:36px; break-before:avoid;"></div>
          <div class="content" style="width:37px; break-before:avoid;"></div>
          <div class="content" style="width:38px; break-before:avoid;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x40
          offset:0,0 size:33x20
          offset:0,20 size:34x20
      offset:110,0 size:100x100
        offset:0,0 size:100x80
          offset:0,0 size:35x20
          offset:0,20 size:36x20
          offset:0,40 size:37x20
          offset:0,60 size:38x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, AvoidBreakBetweenAfterBreakableSibling) {
  // We should avoid breaking between the two direct children of the multicol
  // container - i.e. between #first and #second. We should also avoid breaking
  // between between the children of #second (we run out of space before its
  // second child). The only restriction inside #first is between the third and
  // fourth child, while there are perfect breakpoints between the first and the
  // second, and between the second and the third. We should progress as much as
  // possible, so the correct thing to do is to break between the second and
  // third child of #first.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height: 100px;
      }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div id="parent">
        <div style="break-after:avoid;">
          <div class="content" style="width:33px;"></div>
          <div class="content" style="width:34px;"></div>
          <div class="content" style="width:35px;"></div>
          <div class="content" style="width:36px; break-before:avoid;"></div>
        </div>
        <div>
          <div class="content" style="width:37px;"></div>
          <div class="content" style="width:38px; break-before:avoid;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x100
          offset:0,0 size:33x20
          offset:0,20 size:34x20
      offset:110,0 size:100x100
        offset:0,0 size:100x40
          offset:0,0 size:35x20
          offset:0,20 size:36x20
        offset:0,40 size:100x40
          offset:0,0 size:37x20
          offset:0,20 size:38x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest,
       AvoidBreakBetweenBreakInsidePreviousSibling) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height: 100px;
      }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div id="parent">
        <div class="content" style="width:32px;"></div>
        <div style="break-after:avoid;">
          <div class="content" style="width:33px;"></div>
          <div class="content" style="width:34px;"></div>
          <div class="content" style="width:35px;"></div>
          <div class="content" style="width:36px;"></div>
        </div>
        <div class="content" style="width:37px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:32x20
        offset:0,20 size:100x80
          offset:0,0 size:33x20
          offset:0,20 size:34x20
          offset:0,40 size:35x20
      offset:110,0 size:100x100
        offset:0,0 size:100x20
          offset:0,0 size:36x20
        offset:0,20 size:37x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, AvoidBreakBetweenHonorOrphansWidows) {
  // We run out of space at .content, but this isn't a good location, because of
  // break-before:avoid. Break between the lines. Honor orphans and widows, so
  // that two of the four lines will be pushed to the second column.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height: 100px;
        line-height: 20px;
        orphans: 2;
        widows: 2;
      }
      .content { break-inside:avoid; height:30px; }
    </style>
    <div id="container">
      <div id="parent">
        <br>
        <br>
        <br>
        <br>
        <div class="content" style="break-before:avoid;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x100
          offset:0,0 size:0x20
          offset:0,20 size:0x20
      offset:110,0 size:100x100
        offset:0,0 size:100x40
          offset:0,0 size:0x20
          offset:0,20 size:0x20
        offset:0,40 size:100x30
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, AvoidBreakBetweenHonorOrphansWidows2) {
  // We run out of space at .content, but this isn't a good location, because of
  // break-before:avoid. Break between the first block and the two lines, in
  // order to honor orphans and widows.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height: 100px;
        line-height: 20px;
        orphans: 2;
        widows: 2;
      }
      .content { break-inside:avoid; height:30px; }
    </style>
    <div id="container">
      <div id="parent">
        <div style="height:40px;"></div>
        <br>
        <br>
        <div class="content" style="break-before:avoid;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x40
      offset:110,0 size:100x100
        offset:0,0 size:100x40
          offset:0,0 size:0x20
          offset:0,20 size:0x20
        offset:0,40 size:100x30
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, AvoidBreakBetweenHonorOrphansWidows3) {
  // We run out of space between the first and the second line in the second
  // container, but this isn't a good location, because of the orphans and
  // widows requirement. Break between the second and third line inside the
  // first container instead. We should not break between the two containers,
  // because of break-before:avoid.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height: 100px;
        line-height: 20px;
        orphans: 2;
        widows: 2;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div>
          <br>
          <br>
          <br>
          <br>
        </div>
        <div style="break-before:avoid;">
          <br>
          <br>
          <br>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x100
          offset:0,0 size:0x20
          offset:0,20 size:0x20
      offset:110,0 size:100x100
        offset:0,0 size:100x40
          offset:0,0 size:0x20
          offset:0,20 size:0x20
        offset:0,40 size:100x60
          offset:0,0 size:0x20
          offset:0,20 size:0x20
          offset:0,40 size:0x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, AvoidBreakBetweenIgnoreOrphansWidows) {
  // We run out of space at .content, but this isn't a good location, because of
  // break-before:avoid. Break between the two lines, even if that will violate
  // the orphans and widows requirement. According to the spec, this is better
  // then ignoring the the break-after:avoid declaration on the first child.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height: 100px;
        line-height: 20px;
        orphans: 2;
        widows: 2;
      }
      .content { break-inside:avoid; height:30px; }
    </style>
    <div id="container">
      <div id="parent">
        <div style="height:40px; break-after:avoid;"></div>
        <br>
        <br>
        <div class="content" style="break-before:avoid;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x40
        offset:0,40 size:100x60
          offset:0,0 size:0x20
      offset:110,0 size:100x100
        offset:0,0 size:100x20
          offset:0,0 size:0x20
        offset:0,20 size:100x30
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, AvoidBreakBetweenLinesInsideBreakAvoid) {
  // We run out of space at the second line inside the last container, and we're
  // not supposed to break inside it. We're also not supposed to break between
  // the lines in the previous container (since it has break-inside:avoid,
  // albeit no orphans/widows restrictions). Breaking before that container
  // instead is as far as we get without breaking any rules.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height: 100px;
        line-height: 20px;
        orphans: 1;
        widows: 1;
      }
      .content { break-inside:avoid; height:20px; }
    </style>
    <div id="container">
      <div id="parent">
        <div class="content" style="width:33px;"></div>
        <div class="content" style="width:34px;"></div>
        <div style="break-inside:avoid; width:35px;">
          <br>
          <br>
        </div>
        <div class="content" style="break-before:avoid; width:36px; height:30px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:33x20
        offset:0,20 size:34x20
      offset:110,0 size:100x100
        offset:0,0 size:35x40
          offset:0,0 size:0x20
          offset:0,20 size:0x20
        offset:0,40 size:36x30
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, AvoidBreakBetweenBreakAtEarlyClassC) {
  // The early break is a class C breakpoint, and this is also exactly where the
  // BFC block-offset is resolved. There are no possible breaks as long as we
  // don't know our BFC offset, but breaking just before the box that resolves
  // the BFC block-offset should be allowed.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:22px;">
          <div style="float:left; width:100%; width:33px; height:20px;"></div>
          <div style="display:flow-root; width:44px; height:20px;"></div>
        </div>
        <div style="break-before:avoid; break-inside:avoid; width:55px; height:70px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:22x100
          offset:0,0 size:33x20
      offset:110,0 size:100x100
        offset:0,0 size:22x20
          offset:0,0 size:44x20
        offset:0,20 size:55x70
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, AvoidBreakBeforeBlockReplacedContent) {
  // Replaced content is unbreakable. Don't break right before it if we have
  // break-before:avoid, though.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:22px; height:40px;"></div>
        <div style="width:33px; height:50px; break-inside:avoid;"></div>
        <img style="break-before:avoid; display:block; width:44px; height:50px;">
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:22x40
      offset:110,0 size:100x100
        offset:0,0 size:33x50
        offset:0,50 size:44x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, TallReplacedContent) {
  // Replaced content is unbreakable. Let it overflow the column.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        column-fill: auto;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <img style="display:block; width:44px; height:150px;">
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:44x150
)DUMP";
  EXPECT_EQ(expectation, dump);
}

}  // anonymous namespace
}  // namespace blink
