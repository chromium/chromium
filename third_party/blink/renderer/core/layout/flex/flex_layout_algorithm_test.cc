// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/layout/base_layout_algorithm_test.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/flex/flexible_box_algorithm.h"
#include "third_party/blink/renderer/core/layout/flex/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {
namespace {

class FlexLayoutAlgorithmTest : public BaseLayoutAlgorithmTest {
 protected:
  const DevtoolsFlexInfo* LayoutForDevtools(const String& body_content) {
    SetBodyInnerHTML(body_content);
    return LayoutForDevtools();
  }

  const DevtoolsFlexInfo* LayoutForDevtools() {
    LayoutObject* generic_flex = GetLayoutObjectByElementId("flexbox");
    EXPECT_NE(generic_flex, nullptr);
    auto* flex = DynamicTo<LayoutFlexibleBox>(generic_flex);
    if (!flex) {
      return nullptr;
    }
    flex->SetNeedsLayoutForDevtools();
    UpdateAllLifecyclePhasesForTest();
    return flex->FlexLayoutData();
  }
};

TEST_F(FlexLayoutAlgorithmTest, DetailsFlexDoesntCrash) {
  SetBodyInnerHTML(R"HTML(
    <details style="display:flex"></details>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  // No crash is good.
}

TEST_F(FlexLayoutAlgorithmTest, ReplacedAspectRatioPrecision) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-direction: column; width: 50px">
      <svg width="29" height="22" style="width: auto; height: auto;
                                         margin: auto"></svg>
    </div>
  )HTML");

  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), kIndefiniteSize));
  BlockNode box(GetDocument().body()->GetLayoutBox());

  const PhysicalBoxFragment* fragment = RunBlockLayoutAlgorithm(box, space);
  EXPECT_EQ(PhysicalSize(84, 22), fragment->Size());
  ASSERT_EQ(1u, fragment->Children().size());
  fragment = To<PhysicalBoxFragment>(fragment->Children()[0].get());
  EXPECT_EQ(PhysicalSize(50, 22), fragment->Size());
  ASSERT_EQ(1u, fragment->Children().size());
  EXPECT_EQ(PhysicalSize(29, 22), fragment->Children()[0]->Size());
}

TEST_F(FlexLayoutAlgorithmTest, DevtoolsBasic) {
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

TEST_F(FlexLayoutAlgorithmTest, DevtoolsWrap) {
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

TEST_F(FlexLayoutAlgorithmTest, DevtoolsCoordinates) {
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

TEST_F(FlexLayoutAlgorithmTest, DevtoolsOverflow) {
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <div style="display:flex; width: 100px; border-left: 1px solid; border-right: 3px solid;" id=flexbox>
      <div style="min-width: 150px; height: 75px;"></div>
    </div>
  )HTML");
  DCHECK(devtools);
  EXPECT_EQ(devtools->lines[0].items[0].rect, PhysicalRect(1, 0, 150, 75));
}

TEST_F(FlexLayoutAlgorithmTest, DevtoolsWithRelPosItem) {
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

TEST_F(FlexLayoutAlgorithmTest, DevtoolsBaseline) {
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

TEST_F(FlexLayoutAlgorithmTest, DevtoolsOneImageItemCrash) {
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <div style="display: flex;" id=flexbox><img></div>
  )HTML");
  DCHECK(devtools);
  EXPECT_EQ(devtools->lines.size(), 1u);
}

TEST_F(FlexLayoutAlgorithmTest, DevtoolsColumnWrap) {
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

TEST_F(FlexLayoutAlgorithmTest, DevtoolsColumnWrapOrtho) {
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

TEST_F(FlexLayoutAlgorithmTest, DevtoolsRowWrapOrtho) {
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

TEST_F(FlexLayoutAlgorithmTest, DevtoolsLegacyItem) {
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

TEST_F(FlexLayoutAlgorithmTest, DevtoolsFragmentedItemDoesntCrash) {
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

TEST_F(FlexLayoutAlgorithmTest, DevtoolsAutoScrollbar) {
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
  Element* inner = GetElementById("inner");
  inner->SetInlineStyleProperty(CSSPropertyID::kHeight, "50px");

  devtools = LayoutForDevtools();
  EXPECT_TRUE(devtools);
}

}  // namespace
}  // namespace blink
