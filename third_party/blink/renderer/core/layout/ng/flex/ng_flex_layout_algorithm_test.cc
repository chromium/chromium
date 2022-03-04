// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/flex/layout_ng_flexible_box.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/layout/ng/ng_base_layout_algorithm_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {
namespace {

class NGFlexLayoutAlgorithmTest : public NGBaseLayoutAlgorithmTest {
 protected:
  DevtoolsFlexInfo LayoutForDevtools(const String& body_content) {
    SetBodyInnerHTML(body_content);
    LayoutNGFlexibleBox* flex =
        To<LayoutNGFlexibleBox>(GetLayoutObjectByElementId("flexbox"));
    EXPECT_NE(flex, nullptr);
    flex->SetNeedsLayoutForDevtools();
    UpdateAllLifecyclePhasesForTest();
    return *flex->FlexLayoutData();
  }
};

TEST_F(NGFlexLayoutAlgorithmTest, DetailsFlexDoesntCrash) {
  SetBodyInnerHTML(R"HTML(
    <details style="display:flex"></details>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  // No crash is good.
}

TEST_F(NGFlexLayoutAlgorithmTest, ReplacedAspectRatioPrecision) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-direction: column; width: 50px">
      <svg width="29" height="22" style="width: auto; height: auto;
                                         margin: auto"></svg>
    </div>
  )HTML");

  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), kIndefiniteSize));
  NGBlockNode box(GetDocument().body()->GetLayoutBox());

  const NGPhysicalBoxFragment* fragment = RunBlockLayoutAlgorithm(box, space);
  EXPECT_EQ(PhysicalSize(84, 22), fragment->Size());
  ASSERT_EQ(1u, fragment->Children().size());
  fragment = To<NGPhysicalBoxFragment>(fragment->Children()[0].get());
  EXPECT_EQ(PhysicalSize(50, 22), fragment->Size());
  ASSERT_EQ(1u, fragment->Children().size());
  EXPECT_EQ(PhysicalSize(29, 22), fragment->Children()[0]->Size());
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsBasic) {
  DevtoolsFlexInfo devtools = LayoutForDevtools(R"HTML(
    <div style="display:flex; width: 100px;" id=flexbox>
      <div style="flex-grow: 1; height: 50px;"></div>
      <div style="flex-grow: 1"></div>
    </div>
  )HTML");
  EXPECT_EQ(devtools.lines.size(), 1u);
  EXPECT_EQ(devtools.lines[0].items.size(), 2u);
  EXPECT_EQ(devtools.lines[0].items[0].rect, PhysicalRect(0, 0, 50, 50));
  EXPECT_EQ(devtools.lines[0].items[0].rect, PhysicalRect(0, 0, 50, 50));
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsWrap) {
  DevtoolsFlexInfo devtools = LayoutForDevtools(R"HTML(
    <div style="display:flex; width: 100px; flex-wrap: wrap;" id=flexbox>
      <div style="min-width: 100px; height: 50px;"></div>
      <div style="flex: 1 0 20px; height: 90px;"></div>
    </div>
  )HTML");
  EXPECT_EQ(devtools.lines.size(), 2u);
  EXPECT_EQ(devtools.lines[0].items.size(), 1u);
  EXPECT_EQ(devtools.lines[0].items[0].rect, PhysicalRect(0, 0, 100, 50));
  EXPECT_EQ(devtools.lines[1].items.size(), 1u);
  EXPECT_EQ(devtools.lines[1].items[0].rect, PhysicalRect(0, 50, 100, 90));
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsCoordinates) {
  DevtoolsFlexInfo devtools = LayoutForDevtools(R"HTML(
    <div style="display:flex; width: 100px; flex-wrap: wrap; border-top: 2px solid; padding-top: 3px; border-left: 3px solid; padding-left: 5px; margin-left: 19px;" id=flexbox>
      <div style="margin-left: 5px; min-width: 95px; height: 50px;"></div>
      <div style="flex: 1 0 20px; height: 90px;"></div>
    </div>
  )HTML");
  EXPECT_EQ(devtools.lines.size(), 2u);
  EXPECT_EQ(devtools.lines[0].items.size(), 1u);
  EXPECT_EQ(devtools.lines[0].items[0].rect, PhysicalRect(8, 5, 100, 50));
  EXPECT_EQ(devtools.lines[1].items.size(), 1u);
  EXPECT_EQ(devtools.lines[1].items[0].rect, PhysicalRect(8, 55, 100, 90));
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsOverflow) {
  DevtoolsFlexInfo devtools = LayoutForDevtools(R"HTML(
    <div style="display:flex; width: 100px; border-left: 1px solid; border-right: 3px solid;" id=flexbox>
      <div style="min-width: 150px; height: 75px;"></div>
    </div>
  )HTML");
  EXPECT_EQ(devtools.lines[0].items[0].rect, PhysicalRect(1, 0, 150, 75));
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsWithRelPosItem) {
  // Devtools' heuristic algorithm shows two lines for this case, but layout
  // knows there's only one line.
  DevtoolsFlexInfo devtools = LayoutForDevtools(R"HTML(
  <style>
  .item {
    flex: 0 0 50px;
    height: 50px;
  }
  </style>
  <div style="display: flex;" id=flexbox>
    <div class=item></div>
    <div class=item style="position: relative; top: 60px; left: -10px"></div>
  </div>
  )HTML");
  EXPECT_EQ(devtools.lines.size(), 1u);
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsBaseline) {
  LoadAhem();
  DevtoolsFlexInfo devtools = LayoutForDevtools(R"HTML(
    <div style="display:flex; align-items: baseline; flex-wrap: wrap; width: 250px; margin: 10px;" id=flexbox>
      <div style="width: 100px; margin: 10px; font: 10px/2 Ahem;">Test</div>
      <div style="width: 100px; margin: 10px; font: 10px/1 Ahem;">Test</div>
      <div style="width: 100px; margin: 10px; font: 10px/1 Ahem;">Test</div>
      <div style="width: 100px; margin: 10px; font: 10px/1 Ahem;">Test</div>
    </div>
  )HTML");
  EXPECT_EQ(devtools.lines.size(), 2u);
  EXPECT_EQ(devtools.lines[0].items.size(), 2u);
  EXPECT_GT(devtools.lines[0].items[0].baseline,
            devtools.lines[0].items[1].baseline);
  EXPECT_EQ(devtools.lines[1].items.size(), 2u);
  EXPECT_EQ(devtools.lines[1].items[0].baseline,
            devtools.lines[1].items[1].baseline);
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsOneImageItemCrash) {
  DevtoolsFlexInfo devtools = LayoutForDevtools(R"HTML(
    <div style="display: flex;" id=flexbox><img></div>
  )HTML");
  EXPECT_EQ(devtools.lines.size(), 1u);
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsColumnWrap) {
  DevtoolsFlexInfo devtools = LayoutForDevtools(R"HTML(
    <div style="display: flex; flex-flow: column wrap; width: 300px; height: 100px;" id=flexbox>
      <div style="height: 200px">
        <div style="height: 90%"></div>
      </div>
    </div>
  )HTML");
  EXPECT_EQ(devtools.lines.size(), 1u);
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsColumnWrapOrtho) {
  DevtoolsFlexInfo devtools = LayoutForDevtools(R"HTML(
    <div style="display: flex; flex-flow: column wrap; width: 300px; height: 100px;" id=flexbox>
      <div style="height: 200px; writing-mode: vertical-lr;">
        <div style="width: 90%"></div>
      </div>
    </div>
  )HTML");
  EXPECT_EQ(devtools.lines.size(), 1u);
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsRowWrapOrtho) {
  DevtoolsFlexInfo devtools = LayoutForDevtools(R"HTML(
    <div style="display: flex; flex-flow: wrap; width: 300px; height: 100px;" id=flexbox>
      <div style="height: 200px; writing-mode: vertical-lr;">
        <div style="width: 90%"></div>
        <div style="height: 90%"></div>
      </div>
    </div>
  )HTML");
  EXPECT_EQ(devtools.lines.size(), 1u);
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsLegacyItem) {
  DevtoolsFlexInfo devtools = LayoutForDevtools(R"HTML(
    <div style="display: flex;" id=flexbox>
      <div style="columns: 1">
        <div style="display:flex;"></div>
        <div style="display:grid;"></div>
        <div style="display:table;"></div>
      </div>
    </div>
  )HTML");
  EXPECT_EQ(devtools.lines.size(), 1u);
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsFragmentedItemDoesntCrash) {
  const String& body_content = R"HTML(
    <div style="columns: 2; height: 300px; width: 300px; background: orange;">
      <div style="display: flex; background: blue;" id=flexbox>
        <div style="width: 100px; height: 300px; background: grey;"></div>
      </div>
    </div>
  )HTML";
  // TODO(crbug.com/660611): Remove next 6 lines when flex fragmentation ships.
  SetBodyInnerHTML(body_content);
  UpdateAllLifecyclePhasesForTest();
  LayoutObject* flexbox = GetLayoutObjectByElementId("flexbox");
  EXPECT_NE(flexbox, nullptr);
  if (!flexbox->IsLayoutNGFlexibleBox())
    return;
  DevtoolsFlexInfo devtools = LayoutForDevtools(body_content);
  EXPECT_EQ(devtools.lines.size(), 1u);
}

// Current:  item is at top of container.
// Proposed: item is at bottom of container.
TEST_F(NGFlexLayoutAlgorithmTest, UseCounter1) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; align-content: flex-end; height: 50px">
      <div style="height:20px;"></div>
    </div>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

TEST_F(NGFlexLayoutAlgorithmTest, UseCounter1b) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; align-content: flex-end; height: 50px; flex-wrap: wrap;">
      <div style="height:20px;"></div>
    </div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

TEST_F(NGFlexLayoutAlgorithmTest, UseCounter2) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; align-content: baseline; height: 50px">
      <div style="height:20px;"></div>
    </div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

TEST_F(NGFlexLayoutAlgorithmTest, UseCounter2b) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; height: 50px; align-content: end;">
      <div style="height:20px;"></div>
    </div>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

TEST_F(NGFlexLayoutAlgorithmTest, UseCounter2c) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; height: 50px; align-content: end;">
      <div style="height:20px; align-self: baseline;">other stuff</div>
    </div>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

TEST_F(NGFlexLayoutAlgorithmTest, UseCounter2d) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; height: 50px; align-content: end;">
      <div style="align-self: baseline;">other stuff</div>
    </div>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

TEST_F(NGFlexLayoutAlgorithmTest, UseCounter2e) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; height: 50px; align-content: start;">
      <div style="align-self: baseline;">other stuff</div>
    </div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

TEST_F(NGFlexLayoutAlgorithmTest, UseCounter2f) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; height: 50px; align-content: center;">
      <div style="align-self: baseline;">other stuff</div>
    </div>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

TEST_F(NGFlexLayoutAlgorithmTest, UseCounter2g) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; height: 50px; align-content: end;">
      <div style="align-self: baseline;">blah<br>blah</div>
      <div style="align-self: baseline;">other stuff</div>
    </div>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

TEST_F(NGFlexLayoutAlgorithmTest, UseCounter3) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; align-content: initial; height: 50px">
      <div style="height:20px;"></div>
    </div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

TEST_F(NGFlexLayoutAlgorithmTest, UseCounter4) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; align-content: stretch; height: 50px">
      <div style="height:20px;"></div>
    </div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

TEST_F(NGFlexLayoutAlgorithmTest, UseCounter5) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; align-content: flex-start; height: 50px">
      <div style="height:20px;"></div>
    </div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

TEST_F(NGFlexLayoutAlgorithmTest, UseCounter6) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; height: 50px">
      <div style="height:20px;"></div>
    </div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

TEST_F(NGFlexLayoutAlgorithmTest, UseCounter7) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; align-content: flex-end;">
      <div style="height:20px;"></div>
    </div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

// Current:  item gets 50px height.
// Proposed: item gets 0px height and abuts bottom edge of container.
TEST_F(NGFlexLayoutAlgorithmTest, UseCounter9) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; align-content: flex-end; height: 50px;">
      <div></div>
    </div>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

// Current:  item abuts left edge of container.
// Proposed: item abuts right edge of container.
TEST_F(NGFlexLayoutAlgorithmTest, UseCounter10) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-flow: column; align-content: flex-end;">
      <div style="width:20px;"></div>
    </div>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

TEST_F(NGFlexLayoutAlgorithmTest, UseCounter11) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-flow: column; align-content: flex-end;">
      <div style="width:20px;"></div>
      <div></div>
    </div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

// Current:  items abut left edge of container.
// Proposed: items abut right edge of container.
TEST_F(NGFlexLayoutAlgorithmTest, UseCounter12) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-flow: column; align-content: flex-end;">
      <div style="width:20px;"></div>
      <div style="width:20px;"></div>
    </div>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

TEST_F(NGFlexLayoutAlgorithmTest, UseCounter14) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-flow: column; align-content: flex-end; width: 200px">
      <div style="align-self: flex-end"></div>
    </div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

TEST_F(NGFlexLayoutAlgorithmTest, UseCounter15) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-flow: column; align-content: flex-end; width: 200px">
      <div style="align-self: flex-end; width: 100px;"></div>
    </div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

// Current: item at top
// Proposed: item at bottom
TEST_F(NGFlexLayoutAlgorithmTest, UseCounter15b) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; align-content: end; height: 200px">
      <div style="align-self: flex-start; height: 100px;"></div>
    </div>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

TEST_F(NGFlexLayoutAlgorithmTest, UseCounter15c) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; align-content: end; height: 200px;">
      <div style="height: 100px; align-self: self-end;"></div>
    </div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

// Current: item at top
// Proposed: item in center
TEST_F(NGFlexLayoutAlgorithmTest, UseCounter15d) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; align-content: space-around; height: 200px;">
      <div style="height: 100px;"></div>
    </div>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

TEST_F(NGFlexLayoutAlgorithmTest, UseCounter15e) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; align-content: space-around; height: 200px;">
      <div style="height: 100px; align-self: center;"></div>
    </div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

TEST_F(NGFlexLayoutAlgorithmTest, UseCounter15f) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; align-content: space-between; height: 200px;">
      <div style="height: 100px;"></div>
    </div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

// Current: first item is on the top
// Proposed: first item is on the bottom
TEST_F(NGFlexLayoutAlgorithmTest, UseCounter15g) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; align-content: end; height: 200px;">
      <div style="height: 100px; align-self: start"></div>
      <div style="height: 100px; align-self: end"></div>
    </div>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

// Current: item is on the right.
// Proposed: item is on the left.
TEST_F(NGFlexLayoutAlgorithmTest, UseCounter16) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-flow: column; align-content: flex-start; width: 200px">
      <div style="align-self: flex-end; width: 100px;"></div>
    </div>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

// Current: first item's right edge abuts container's right edge
//          second item is horizontally centered
// Proposal: both abut container's right edge
TEST_F(NGFlexLayoutAlgorithmTest, UseCounter17) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-flow: column; align-content: flex-end; width: 200px">
      <div style="align-self: flex-end; width: 100px;"></div>
      <div style="align-self: center; width: 100px;"></div>
    </div>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

// Current: first item's bottom edge abuts container's bottom edge
//          second item is vertically centered
// Proposal: both abut container's bottom edge
TEST_F(NGFlexLayoutAlgorithmTest, UseCounter18) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; align-content: flex-end; height: 200px">
      <div style="align-self: flex-end; height: 100px;"></div>
      <div style="align-self: center; height: 100px;"></div>
    </div>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

// This case has no behavior change but checking the used width of each item
// against the flex container's width is too difficult without fully
// implementing the new behavior.
TEST_F(NGFlexLayoutAlgorithmTest, UseCounter19) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-flow: column; align-content: flex-end; width: 20px">
      <div style="width:20px;"></div>
      <div style="width:10px;"></div>
    </div>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kFlexboxAlignSingleLineDifference));
}

}  // namespace
}  // namespace blink
