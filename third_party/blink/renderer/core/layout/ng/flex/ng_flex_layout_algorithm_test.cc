// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/flex/layout_ng_flexible_box.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/layout/flexible_box_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_base_layout_algorithm_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {
namespace {

class NGFlexLayoutAlgorithmTest : public NGBaseLayoutAlgorithmTest {
 protected:
  const DevtoolsFlexInfo* LayoutForDevtools(const String& body_content) {
    SetBodyInnerHTML(body_content);
    return LayoutForDevtools();
  }

  const DevtoolsFlexInfo* LayoutForDevtools() {
    LayoutObject* generic_flex = GetLayoutObjectByElementId("flexbox");
    EXPECT_NE(generic_flex, nullptr);
    LayoutNGFlexibleBox* ng_flex = DynamicTo<LayoutNGFlexibleBox>(generic_flex);
    if (!ng_flex)
      return nullptr;
    ng_flex->SetNeedsLayoutForDevtools();
    UpdateAllLifecyclePhasesForTest();
    return ng_flex->FlexLayoutData();
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
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <div style="display:flex; width: 100px;" id=flexbox>
      <div style="flex-grow: 1; height: 50px;"></div>
      <div style="flex-grow: 1"></div>
    </div>
  )HTML");
  DCHECK(devtools);
  EXPECT_EQ(devtools->lines.size(), 1u);
  EXPECT_EQ(devtools->lines[0].items.size(), 2u);
  EXPECT_EQ(devtools->lines[0].items[0].rect, PhysicalRect(0, 0, 50, 50));
  EXPECT_EQ(devtools->lines[0].items[0].rect, PhysicalRect(0, 0, 50, 50));
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsWrap) {
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <div style="display:flex; width: 100px; flex-wrap: wrap;" id=flexbox>
      <div style="min-width: 100px; height: 50px;"></div>
      <div style="flex: 1 0 20px; height: 90px;"></div>
    </div>
  )HTML");
  DCHECK(devtools);
  EXPECT_EQ(devtools->lines.size(), 2u);
  EXPECT_EQ(devtools->lines[0].items.size(), 1u);
  EXPECT_EQ(devtools->lines[0].items[0].rect, PhysicalRect(0, 0, 100, 50));
  EXPECT_EQ(devtools->lines[1].items.size(), 1u);
  EXPECT_EQ(devtools->lines[1].items[0].rect, PhysicalRect(0, 50, 100, 90));
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsCoordinates) {
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <div style="display:flex; width: 100px; flex-wrap: wrap; border-top: 2px solid; padding-top: 3px; border-left: 3px solid; padding-left: 5px; margin-left: 19px;" id=flexbox>
      <div style="margin-left: 5px; min-width: 95px; height: 50px;"></div>
      <div style="flex: 1 0 20px; height: 90px;"></div>
    </div>
  )HTML");
  DCHECK(devtools);
  EXPECT_EQ(devtools->lines.size(), 2u);
  EXPECT_EQ(devtools->lines[0].items.size(), 1u);
  EXPECT_EQ(devtools->lines[0].items[0].rect, PhysicalRect(8, 5, 100, 50));
  EXPECT_EQ(devtools->lines[1].items.size(), 1u);
  EXPECT_EQ(devtools->lines[1].items[0].rect, PhysicalRect(8, 55, 100, 90));
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsOverflow) {
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <div style="display:flex; width: 100px; border-left: 1px solid; border-right: 3px solid;" id=flexbox>
      <div style="min-width: 150px; height: 75px;"></div>
    </div>
  )HTML");
  DCHECK(devtools);
  EXPECT_EQ(devtools->lines[0].items[0].rect, PhysicalRect(1, 0, 150, 75));
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsWithRelPosItem) {
  // Devtools' heuristic algorithm shows two lines for this case, but layout
  // knows there's only one line.
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
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
  DCHECK(devtools);
  EXPECT_EQ(devtools->lines.size(), 1u);
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsBaseline) {
  LoadAhem();
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <div style="display:flex; align-items: baseline; flex-wrap: wrap; width: 250px; margin: 10px;" id=flexbox>
      <div style="width: 100px; margin: 10px; font: 10px/2 Ahem;">Test</div>
      <div style="width: 100px; margin: 10px; font: 10px/1 Ahem;">Test</div>
      <div style="width: 100px; margin: 10px; font: 10px/1 Ahem;">Test</div>
      <div style="width: 100px; margin: 10px; font: 10px/1 Ahem;">Test</div>
    </div>
  )HTML");
  DCHECK(devtools);
  EXPECT_EQ(devtools->lines.size(), 2u);
  EXPECT_EQ(devtools->lines[0].items.size(), 2u);
  EXPECT_GT(devtools->lines[0].items[0].baseline,
            devtools->lines[0].items[1].baseline);
  EXPECT_EQ(devtools->lines[1].items.size(), 2u);
  EXPECT_EQ(devtools->lines[1].items[0].baseline,
            devtools->lines[1].items[1].baseline);
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsOneImageItemCrash) {
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <div style="display: flex;" id=flexbox><img></div>
  )HTML");
  DCHECK(devtools);
  EXPECT_EQ(devtools->lines.size(), 1u);
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsColumnWrap) {
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <div style="display: flex; flex-flow: column wrap; width: 300px; height: 100px;" id=flexbox>
      <div style="height: 200px">
        <div style="height: 90%"></div>
      </div>
    </div>
  )HTML");
  DCHECK(devtools);
  EXPECT_EQ(devtools->lines.size(), 1u);
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsColumnWrapOrtho) {
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <div style="display: flex; flex-flow: column wrap; width: 300px; height: 100px;" id=flexbox>
      <div style="height: 200px; writing-mode: vertical-lr;">
        <div style="width: 90%"></div>
      </div>
    </div>
  )HTML");
  DCHECK(devtools);
  EXPECT_EQ(devtools->lines.size(), 1u);
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsRowWrapOrtho) {
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <div style="display: flex; flex-flow: wrap; width: 300px; height: 100px;" id=flexbox>
      <div style="height: 200px; writing-mode: vertical-lr;">
        <div style="width: 90%"></div>
        <div style="height: 90%"></div>
      </div>
    </div>
  )HTML");
  DCHECK(devtools);
  EXPECT_EQ(devtools->lines.size(), 1u);
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsLegacyItem) {
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <div style="display: flex;" id=flexbox>
      <div style="columns: 1">
        <div style="display:flex;"></div>
        <div style="display:grid;"></div>
        <div style="display:table;"></div>
      </div>
    </div>
  )HTML");
  DCHECK(devtools);
  EXPECT_EQ(devtools->lines.size(), 1u);
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsFragmentedItemDoesntCrash) {
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <div style="columns: 2; height: 300px; width: 300px; background: orange;">
      <div style="display: flex; background: blue;" id=flexbox>
        <div style="width: 100px; height: 300px; background: grey;"></div>
      </div>
    </div>
  )HTML");
  // We don't currently set DevtoolsFlexInfo when fragmenting.
  DCHECK(!devtools);
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsAutoScrollbar) {
  // Pass if we get a devtools info object and don't crash.
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <style>
      ::-webkit-scrollbar {
        width: 10px;
      }
    </style>
    <div id="flexbox" style="display:flex; height:100px;">
      <div style="overflow:auto; width:100px;">
        <div id="inner" style="height:200px;"></div>
      </div>
    </div>
  )HTML");
  EXPECT_TRUE(devtools);

  // Make the inner child short enough to eliminate the need for a scrollbar.
  Element* inner = GetDocument().getElementById(AtomicString("inner"));
  inner->SetInlineStyleProperty(CSSPropertyID::kHeight, "50px");

  devtools = LayoutForDevtools();
  EXPECT_TRUE(devtools);
}

TEST_F(NGFlexLayoutAlgorithmTest, AbsPosUma1) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-flow: column; width:100px; height:100px;" id=flexbox>
      <div style="position: absolute; justify-self: stretch; align-self: flex-end; width:50px; height:50px;" id=item></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  LayoutNGFlexibleBox* flex =
      To<LayoutNGFlexibleBox>(GetLayoutObjectByElementId("flexbox"));
  LayoutObject* item = GetLayoutObjectByElementId("item");
  ItemPosition pos = FlexLayoutAlgorithm::AlignmentForChild(flex->StyleRef(),
                                                            item->StyleRef());
  ASSERT_EQ(pos, ItemPosition::kFlexEnd);
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kFlexboxNewAbsPos));
}

TEST_F(NGFlexLayoutAlgorithmTest, AbsPosUmaDifferent) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-flow: column; width:100px; height:100px;">
      <div style="position: absolute; height:50px; width:50px; justify-self: start; align-self: end"></div>
    </div>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kFlexboxNewAbsPos))
      << "justify and align are clearly different";
}

TEST_F(NGFlexLayoutAlgorithmTest, AbsPosUmaDifferentButRow) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; width:100px; height:100px;">
      <div style="position: absolute; height:50px; width:50px; justify-self: start; align-self: end"></div>
    </div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kFlexboxNewAbsPos))
      << "justify and align are clearly different but we don't count row "
         "flexboxes";
}

TEST_F(NGFlexLayoutAlgorithmTest, AbsPosUmaBothCenter) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-flow: column; width:100px; height:100px;">
      <div style="position: absolute; height:50px; width:50px; justify-self: center; align-self: center; "></div>
    </div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kFlexboxNewAbsPos))
      << "justify and align are both center";
}

TEST_F(NGFlexLayoutAlgorithmTest, AbsPosUmaEndFlexEnd) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-flow: column; width:100px; height:100px;">
      <div style="position: absolute; height:50px; width:50px; justify-self: flex-end; align-self: end; "></div>
    </div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kFlexboxNewAbsPos))
      << "justify and align map to same even though specified differently";
}

TEST_F(NGFlexLayoutAlgorithmTest, AbsPosUmaLeftEnd) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-flow: column; width:100px; height:100px;">
      <div style="position: absolute; height:50px; width:50px; justify-self: left; align-self: end;"></div>
    </div>
  )HTML");
  // current: top right
  // proposed: top left
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kFlexboxNewAbsPos));
}

TEST_F(NGFlexLayoutAlgorithmTest, AbsPosUmaVerticalWritingMode) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-flow: column; width:100px; height:100px; writing-mode: vertical-rl;">
      <div style="position: absolute; height:50px; width:50px; justify-self: start; align-self: end;"></div>
    </div>
  )HTML");
  // current: left bottom
  // proposed: left top
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kFlexboxNewAbsPos));
}

TEST_F(NGFlexLayoutAlgorithmTest, AbsPosUmaOrthogonalWritingMode) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-flow: column; width:100px; height:100px;">
      <div style="position: absolute; height:50px; width:50px; justify-self: end; align-self: self-start; writing-mode: vertical-rl;"></div>
    </div>
  )HTML");
  // current: right top
  // proposed: right top
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kFlexboxNewAbsPos));
}

// column-reverse switches main-axis order (start placing items at block-end)
TEST_F(NGFlexLayoutAlgorithmTest, AbsPosUmaFlexEndReverseStart) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-flow: column-reverse; width:100px; height:100px;">
      <div style="position: absolute; height:50px; width:50px; justify-self: flex-end; align-self: start;"></div>
    </div>
  )HTML");
  // current: item is at bottom (b/c column-reverse) left (b/c start)
  // proposed: item is at bottom (b/c column-reverse) right (b/c flex-end)
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kFlexboxNewAbsPos));
}

// wrap-reverse switches cross-axis order (of the lines)
TEST_F(NGFlexLayoutAlgorithmTest, AbsPosUmaFlexEndWrapReverse) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-flow: column wrap-reverse; width:100px; height:100px;">
      <div style="position: absolute; height:50px; width:50px; justify-self: flex-end; align-self: start;"></div>
    </div>
  )HTML");
  // current: item is at top left
  // proposed: item is at top left
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kFlexboxNewAbsPos))
      << "justify and align map to same even though specified differently";
}

TEST_F(NGFlexLayoutAlgorithmTest, AbsPosUmaAlignItemsSame) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-flow: column; width:100px; height:100px; align-items: end;">
      <div style="position: absolute; height:50px; width:50px; justify-self: end;"></div>
    </div>
  )HTML");
  // current: top right
  // proposed: top right
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kFlexboxNewAbsPos))
      << "align-items' default value for align-self is same as "
         "justify-self";
}

TEST_F(NGFlexLayoutAlgorithmTest, AbsPosUmaAlignItemsDifferent) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-flow: column; width:100px; height:100px; align-items: end;">
      <div style="position: absolute; height:50px; width:50px; justify-self: start;"></div>
    </div>
  )HTML");
  // current: top right
  // proposed: top left
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kFlexboxNewAbsPos))
      << "align-items' default value for align-self differs from justify-self";
}

// These next 4 tests are disabled because we don't have a good way to compare
// the abspos size to the static position rectangle. This means we overcount the
// number of pages that will be changed by the abspos proposal in
// https://github.com/w3c/csswg-drafts/issues/5843.

TEST_F(NGFlexLayoutAlgorithmTest, DISABLED_AbsPosUma0px) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-flow: column;">
      <div style="position: absolute; justify-self: start; align-self: end;">
      </div>
    </div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kFlexboxNewAbsPos))
      << "static pos rectangle and item are both 0px";
}

TEST_F(NGFlexLayoutAlgorithmTest, DISABLED_AbsPosUmaSameSize) {
  SetBodyInnerHTML(R"HTML(
    <div style="position: relative; width: 80px;">
      <div style="display: flex; flex-flow: column; width: 70px; height: 100px;">
        <div style="position: absolute; justify-self: start; align-self: end; width: 80px; height: 50px;">
        </div>
      </div>
    </div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kFlexboxNewAbsPos))
      << "static pos rectangle is same size as item's margin box";
}

TEST_F(NGFlexLayoutAlgorithmTest, DISABLED_AbsPosUmaSameSizeWithMargin) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-flow: column; width: 70px; height: 100px;">
      <div style="position: absolute; justify-self: start; align-self: end; margin: 25px 25px; height: 50px;">
      </div>
    </div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kFlexboxNewAbsPos))
      << "static pos rectangle is same size as item's margin box";
}

TEST_F(NGFlexLayoutAlgorithmTest, DISABLED_AbsPosUmaAutoInsetsSameSize) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-flow: column; width:100px; height:100px;">
      <div style="position: absolute; height:50px; width:100px; justify-self: start; align-self: end; inset: 1px auto 1px auto"></div>
    </div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kFlexboxNewAbsPos))
      << "auto insets in the axis of same size means no change";
}

TEST_F(NGFlexLayoutAlgorithmTest, AbsPosUmaNoAutoInsets) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-flow: column; width:100px; height:100px;">
      <div style="position: absolute; height:50px; width:50px; justify-self: start; align-self: end; inset: 1px 1px auto auto"></div>
    </div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kFlexboxNewAbsPos))
      << "justify and align are different but has non-auto insets";
}

TEST_F(NGFlexLayoutAlgorithmTest, AbsPosUmaAutoInsetsDifferentSize) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-flow: column; width:100px; height:100px;">
      <div style="position: absolute; height:100px; width:50px; justify-self: start; align-self: end; inset: 1px auto 1px auto"></div>
    </div>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kFlexboxNewAbsPos))
      << "auto insets in the axis of different size means change";
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
