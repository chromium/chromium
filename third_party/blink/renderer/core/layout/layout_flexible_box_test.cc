// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"

#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutFlexibleBoxTest : public testing::WithParamInterface<bool>,
                              private ScopedLayoutNGForTest,
                              public RenderingTest {
 public:
  LayoutFlexibleBoxTest() : ScopedLayoutNGForTest(GetParam()) {}

 protected:
  void ExpectSameAsRowHTB();
  void ExpectSameAsRowVLR();
  void ExpectSameAsRowVRL();
  void ExpectSameAsRowReverseVLR();
  void ExpectSameAsRowReverseVRL();
  void ExpectSameAsRTLRowHTB();

  LayoutBox* GetLayoutBoxByElementId(const char* id) const {
    return ToLayoutBox(GetLayoutObjectByElementId(id));
  }
};

INSTANTIATE_TEST_SUITE_P(All, LayoutFlexibleBoxTest, testing::Bool());

static String CommonStyle() {
  return R"HTML(
    <style>
      ::-webkit-scrollbar { width: 15px; height: 16px; background: yellow; }
      .rtl { direction: rtl; }
      .htb { writing-mode: horizontal-tb; }
      .vlr { writing-mode: vertical-lr; }
      .vrl { writing-mode: vertical-rl; }
      .row { flex-direction: row; }
      .row-reverse { flex-direction: row-reverse; }
      .column { flex-direction: column; }
      .column-reverse { flex-direction: column-reverse; }
      #flex-box {
        display: flex;
        width: 400px;
        height: 300px;
        overflow: auto;
        padding: 10px 20px 30px 40px;
        border-width: 20px 30px 40px 50px;
        border-style: solid;
      }
      #child {
        width: 2000px;
        height: 1000px;
        flex: none;
      }
    </style>
  )HTML";
}

static void CheckFlexBoxPhysicalGeometries(const LayoutBox* flex_box) {
  // 540 = border_left + padding_left + width + padding_right + border_right
  // 400 = border_top + padding_top + height + padding_bottom + border_bottom
  EXPECT_EQ(LayoutRect(0, 0, 540, 400), flex_box->BorderBoxRect());
  if (!flex_box->ShouldPlaceVerticalScrollbarOnLeft()) {
    // This excludes borders and scrollbars from BorderBoxRect.
    EXPECT_EQ(PhysicalRect(50, 20, 445, 324),
              flex_box->PhysicalPaddingBoxRect());
    // This excludes paddings from PhysicalPaddingBoxRect.
    EXPECT_EQ(PhysicalRect(90, 30, 385, 284),
              flex_box->PhysicalContentBoxRect());
  } else {
    // There is scrollbar on the left, so shift content to the right.
    EXPECT_EQ(PhysicalRect(65, 20, 445, 324),
              flex_box->PhysicalPaddingBoxRect());
    EXPECT_EQ(PhysicalRect(105, 30, 385, 284),
              flex_box->PhysicalContentBoxRect());
  }

  EXPECT_EQ(LayoutSize(), flex_box->ScrolledContentOffset());
  EXPECT_EQ(ScrollOffset(), flex_box->GetScrollableArea()->GetScrollOffset());
}

void LayoutFlexibleBoxTest::ExpectSameAsRowHTB() {
  const auto* flex_box = GetLayoutBoxByElementId("flex-box");
  const auto* scrollable_area = flex_box->GetScrollableArea();
  CheckFlexBoxPhysicalGeometries(flex_box);

  EXPECT_EQ(IntSize(), flex_box->OriginAdjustmentForScrollbars());
  // 50 = border_left, 20 = border_top
  // 2040 = child_width (2000) + padding_left (40) (without padding_right which
  //        is in flow-end direction)
  // 1040 = child_height (1000) + padding_top (10) + padding_bottom (30)
  EXPECT_EQ(LayoutRect(50, 20, 2040, 1040), flex_box->LayoutOverflowRect());
  // 1595 = layout_overflow_width (2040) - client_width (445 -> see below).
  // 716 = layout_overflow_height (1040) - client_height (324 -> see below).
  EXPECT_EQ(IntSize(1595, 716), scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(IntSize(), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(IntPoint(), scrollable_area->ScrollOrigin());
  EXPECT_EQ(FloatPoint(), scrollable_area->ScrollPosition());

  const auto* child = GetLayoutBoxByElementId("child");
  EXPECT_EQ(LayoutPoint(90, 30), child->Location());
  EXPECT_EQ(PhysicalOffset(90, 30), child->PhysicalLocation());
}

TEST_P(LayoutFlexibleBoxTest, GeometriesWithScrollbarsRowHTB) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="row htb">
      <div id="child"></div>
    </div>
  )HTML");
  ExpectSameAsRowHTB();
}

void LayoutFlexibleBoxTest::ExpectSameAsRowVLR() {
  const auto* flex_box = GetLayoutBoxByElementId("flex-box");
  const auto* scrollable_area = flex_box->GetScrollableArea();
  CheckFlexBoxPhysicalGeometries(flex_box);

  EXPECT_EQ(IntSize(), flex_box->OriginAdjustmentForScrollbars());
  // 50 = border_left, 20 = border_top
  // 2060 = child_width (2000) + padding_left (40) + padding_right (20)
  // 1010 = child_height (300) + padding_top (10) (without padding_bottom which
  //        is in flow-end direction)
  EXPECT_EQ(LayoutRect(50, 20, 2060, 1010), flex_box->LayoutOverflowRect());
  EXPECT_EQ(IntSize(1615, 686), scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(IntSize(), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(IntPoint(), scrollable_area->ScrollOrigin());
  EXPECT_EQ(FloatPoint(), scrollable_area->ScrollPosition());

  const auto* child = GetLayoutBoxByElementId("child");
  EXPECT_EQ(LayoutPoint(90, 30), child->Location());
  EXPECT_EQ(PhysicalOffset(90, 30), child->PhysicalLocation());
}

TEST_P(LayoutFlexibleBoxTest, GeometriesWithScrollbarsRowVLR) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="row vlr">
      <div id="child"></div>
    </div>
  )HTML");
  ExpectSameAsRowVLR();
}

void LayoutFlexibleBoxTest::ExpectSameAsRowVRL() {
  const auto* flex_box = GetLayoutBoxByElementId("flex-box");
  const auto* scrollable_area = flex_box->GetScrollableArea();
  CheckFlexBoxPhysicalGeometries(flex_box);

  EXPECT_EQ(IntSize(), flex_box->OriginAdjustmentForScrollbars());
  // 45 = border_right (30) + vertical_scrollbar_width (15)
  // 20 = border_top
  // 2060 = child_width (2000) + padding_left (40) + padding_right (20)
  // 1010 is the same as RowVRL
  EXPECT_EQ(LayoutRect(45, 20, 2060, 1010), flex_box->LayoutOverflowRect());
  EXPECT_EQ(IntSize(0, 686), scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(IntSize(-1615, 0), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(IntPoint(1615, 0), scrollable_area->ScrollOrigin());
  EXPECT_EQ(FloatPoint(1615, 0), scrollable_area->ScrollPosition());

  const auto* child = GetLayoutBoxByElementId("child");
  // 65 = border_right (30) + padding_right (20) + vertical_scrollbar_width (15)
  EXPECT_EQ(LayoutPoint(65, 30), child->Location());
  // -1525 = full_flex_box_width (540) - 65 - child_width (2000))
  EXPECT_EQ(PhysicalOffset(-1525, 30), child->PhysicalLocation());
}

TEST_P(LayoutFlexibleBoxTest, GeometriesWithScrollbarsRowVRL) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="row vrl">
      <div id="child"></div>
    </div>
  )HTML");
  ExpectSameAsRowVRL();
}

TEST_P(LayoutFlexibleBoxTest, GeometriesWithScrollbarsRowReverseHTB) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="row-reverse htb">
      <div id="child"></div>
    </div>
  )HTML");

  const auto* flex_box = GetLayoutBoxByElementId("flex-box");
  const auto* scrollable_area = flex_box->GetScrollableArea();
  CheckFlexBoxPhysicalGeometries(flex_box);

  EXPECT_EQ(IntSize(), flex_box->OriginAdjustmentForScrollbars());
  // -1525 = full_flex_box_width (540) - border-right (30) - padding_right (20)
  //         - vertical_scrollbar_width (15) - child_width (2000)
  // 20 = border_top
  // 2020 = child_width (2000) + padding_right (20) (without padding_left which
  //        is in flow-end direction)
  // 1040 = child_height (1000) + padding_top (10) + padding_bottom (30)
  EXPECT_EQ(LayoutRect(-1525, 20, 2020, 1040), flex_box->LayoutOverflowRect());
  // 716 = layout_overflow_height (1040) - client_height (324)
  EXPECT_EQ(IntSize(0, 716), scrollable_area->MaximumScrollOffsetInt());
  // -1575 = -(layout_overflow_width (2020) - client_width (445))
  EXPECT_EQ(IntSize(-1575, 0), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(IntPoint(1575, 0), scrollable_area->ScrollOrigin());
  EXPECT_EQ(FloatPoint(1575, 0), scrollable_area->ScrollPosition());

  const auto* child = GetLayoutBoxByElementId("child");
  EXPECT_EQ(LayoutPoint(-1525, 30), child->Location());
  EXPECT_EQ(PhysicalOffset(-1525, 30), child->PhysicalLocation());
}

void LayoutFlexibleBoxTest::ExpectSameAsRowReverseVLR() {
  const auto* flex_box = GetLayoutBoxByElementId("flex-box");
  const auto* scrollable_area = flex_box->GetScrollableArea();
  CheckFlexBoxPhysicalGeometries(flex_box);

  EXPECT_EQ(IntSize(), flex_box->OriginAdjustmentForScrollbars());
  // 50 = border_left
  // -686 = full_flex_box_height (400) - border-bottom (40) -
  //        padding_bottom (30) - horizontal_scrollbar_height (16) -
  //        child_width (1000)
  // 2060 = child_width (2000) + padding_left (40) + padding_right (20)
  // 1030 = child_height (300) + padding_bottom (30) (without padding_top which
  //        is in flow-end direction)
  EXPECT_EQ(LayoutRect(50, -686, 2060, 1030), flex_box->LayoutOverflowRect());
  // 1615 = layout_overflow_width (2060) - client_width (445)
  EXPECT_EQ(IntSize(1615, 0), scrollable_area->MaximumScrollOffsetInt());
  // -706 = -(layout_overflow_height (1030) - client_height (324))
  EXPECT_EQ(IntSize(0, -706), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(IntPoint(0, 706), scrollable_area->ScrollOrigin());
  EXPECT_EQ(FloatPoint(0, 706), scrollable_area->ScrollPosition());

  const auto* child = GetLayoutBoxByElementId("child");
  EXPECT_EQ(LayoutPoint(90, -686), child->Location());
  EXPECT_EQ(PhysicalOffset(90, -686), child->PhysicalLocation());
}

TEST_P(LayoutFlexibleBoxTest, GeometriesWithScrollbarsRowReverseVLR) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="row-reverse vlr">
      <div id="child"></div>
    </div>
  )HTML");
  ExpectSameAsRowReverseVLR();
}

void LayoutFlexibleBoxTest::ExpectSameAsRowReverseVRL() {
  const auto* flex_box = GetLayoutBoxByElementId("flex-box");
  const auto* scrollable_area = flex_box->GetScrollableArea();
  CheckFlexBoxPhysicalGeometries(flex_box);

  EXPECT_EQ(IntSize(), flex_box->OriginAdjustmentForScrollbars());
  // 45 = border_right (30) + vertical_scrollbar_width (15)
  // -686 is the same as RowReverseVLR.
  // 2060 = child_width (2000) + padding_left (40) + padding_right (20)
  // 1030 is the same as RowReverseVLR.
  EXPECT_EQ(LayoutRect(45, -686, 2060, 1030), flex_box->LayoutOverflowRect());
  EXPECT_EQ(IntSize(), scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(IntSize(-1615, -706), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(IntPoint(1615, 706), scrollable_area->ScrollOrigin());
  EXPECT_EQ(FloatPoint(1615, 706), scrollable_area->ScrollPosition());

  const auto* child = GetLayoutBoxByElementId("child");
  // 65 = border_right (30) + padding_right (20) + vertical_scrollbar_width (15)
  EXPECT_EQ(LayoutPoint(65, -686), child->Location());
  // -1525 = full_flex_box_width (540) - 65 - child_width (2000))
  EXPECT_EQ(PhysicalOffset(-1525, -686), child->PhysicalLocation());
}

TEST_P(LayoutFlexibleBoxTest, GeometriesWithScrollbarsRowReverseVRL) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="row-reverse vrl">
      <div id="child"></div>
    </div>
  )HTML");
  ExpectSameAsRowReverseVRL();
}

TEST_P(LayoutFlexibleBoxTest, GeometriesWithScrollbarsColumnHTB) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="column htb">
      <div id="child"></div>
    </div>
  )HTML");
  ExpectSameAsRowHTB();
}

TEST_P(LayoutFlexibleBoxTest, GeometriesWithScrollbarsColumnVLR) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="column vlr">
      <div id="child"></div>
    </div>
  )HTML");
  ExpectSameAsRowVLR();
}

TEST_P(LayoutFlexibleBoxTest, GeometriesWithScrollbarsColumnVRL) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="column vrl">
      <div id="child"></div>
    </div>
  )HTML");
  ExpectSameAsRowVRL();
}

// The numbers in the following tests are just different combinations of the
// numbers in the above tests. See the explanation of the same number in the
// above tests for the steps of calculations.

TEST_P(LayoutFlexibleBoxTest, GeometriesWithScrollbarsColumnReverseHTB) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="column-reverse htb">
      <div id="child"></div>
    </div>
  )HTML");

  const auto* flex_box = GetLayoutBoxByElementId("flex-box");
  const auto* scrollable_area = flex_box->GetScrollableArea();
  CheckFlexBoxPhysicalGeometries(flex_box);

  EXPECT_EQ(IntSize(), flex_box->OriginAdjustmentForScrollbars());
  EXPECT_EQ(LayoutRect(50, -686, 2040, 1030), flex_box->LayoutOverflowRect());
  EXPECT_EQ(IntSize(1595, 0), scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(IntSize(0, -706), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(IntPoint(0, 706), scrollable_area->ScrollOrigin());
  EXPECT_EQ(FloatPoint(0, 706), scrollable_area->ScrollPosition());

  const auto* child = GetLayoutBoxByElementId("child");
  EXPECT_EQ(LayoutPoint(90, -686), child->Location());
  EXPECT_EQ(PhysicalOffset(90, -686), child->PhysicalLocation());
}

TEST_P(LayoutFlexibleBoxTest, GeometriesWithScrollbarsColumnReverseVLR) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="column-reverse vlr">
      <div id="child"></div>
    </div>
  )HTML");

  const auto* flex_box = GetLayoutBoxByElementId("flex-box");
  const auto* scrollable_area = flex_box->GetScrollableArea();
  CheckFlexBoxPhysicalGeometries(flex_box);

  EXPECT_EQ(IntSize(), flex_box->OriginAdjustmentForScrollbars());
  EXPECT_EQ(LayoutRect(-1525, 20, 2020, 1010), flex_box->LayoutOverflowRect());
  EXPECT_EQ(IntSize(0, 686), scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(IntSize(-1575, 0), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(IntPoint(1575, 0), scrollable_area->ScrollOrigin());
  EXPECT_EQ(FloatPoint(1575, 0), scrollable_area->ScrollPosition());

  const auto* child = GetLayoutBoxByElementId("child");
  EXPECT_EQ(LayoutPoint(-1525, 30), child->Location());
  EXPECT_EQ(PhysicalOffset(-1525, 30), child->PhysicalLocation());
}

TEST_P(LayoutFlexibleBoxTest, GeometriesWithScrollbarsColumnReverseVRL) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="column-reverse vrl">
      <div id="child"></div>
    </div>
  )HTML");

  const auto* flex_box = GetLayoutBoxByElementId("flex-box");
  const auto* scrollable_area = flex_box->GetScrollableArea();
  CheckFlexBoxPhysicalGeometries(flex_box);

  EXPECT_EQ(IntSize(), flex_box->OriginAdjustmentForScrollbars());
  EXPECT_EQ(LayoutRect(-1550, 20, 2040, 1010), flex_box->LayoutOverflowRect());
  EXPECT_EQ(IntSize(1595, 686), scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(IntSize(), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(IntPoint(), scrollable_area->ScrollOrigin());
  EXPECT_EQ(FloatPoint(), scrollable_area->ScrollPosition());

  const auto* child = GetLayoutBoxByElementId("child");
  EXPECT_EQ(LayoutPoint(-1550, 30), child->Location());
  EXPECT_EQ(PhysicalOffset(90, 30), child->PhysicalLocation());
}

void LayoutFlexibleBoxTest::ExpectSameAsRTLRowHTB() {
  const auto* flex_box = GetLayoutBoxByElementId("flex-box");
  const auto* scrollable_area = flex_box->GetScrollableArea();
  CheckFlexBoxPhysicalGeometries(flex_box);

  // Additional origin due to the scrollbar on the left.
  EXPECT_EQ(IntSize(15, 0), flex_box->OriginAdjustmentForScrollbars());
  EXPECT_EQ(LayoutRect(-1510, 20, 2020, 1040), flex_box->LayoutOverflowRect());
  EXPECT_EQ(IntSize(0, 716), scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(IntSize(-1575, 0), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(IntPoint(1575, 0), scrollable_area->ScrollOrigin());
  EXPECT_EQ(FloatPoint(1575, 0), scrollable_area->ScrollPosition());

  const auto* child = GetLayoutBoxByElementId("child");
  EXPECT_EQ(LayoutPoint(-1510, 30), child->Location());
  EXPECT_EQ(PhysicalOffset(-1510, 30), child->PhysicalLocation());
}

TEST_P(LayoutFlexibleBoxTest, GeometriesWithScrollbarsRTLRowHTB) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="rtl row htb">
      <div id="child"></div>
    </div>
  )HTML");
  ExpectSameAsRTLRowHTB();
}

TEST_P(LayoutFlexibleBoxTest, GeometriesWithScrollbarsRTLRowVLR) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="rtl row vlr">
      <div id="child"></div>
    </div>
  )HTML");
  ExpectSameAsRowReverseVLR();
}

TEST_P(LayoutFlexibleBoxTest, GeometriesWithScrollbarsRTLRowVRL) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="rtl row vrl">
      <div id="child"></div>
    </div>
  )HTML");
  ExpectSameAsRowReverseVRL();
}

TEST_P(LayoutFlexibleBoxTest, GeometriesWithScrollbarsRTLRowReverseHTB) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="rtl row-reverse htb">
      <div id="child"></div>
    </div>
  )HTML");

  const auto* flex_box = GetLayoutBoxByElementId("flex-box");
  const auto* scrollable_area = flex_box->GetScrollableArea();
  CheckFlexBoxPhysicalGeometries(flex_box);

  // Additional origin due to the scrollbar on the left.
  EXPECT_EQ(IntSize(15, 0), flex_box->OriginAdjustmentForScrollbars());
  EXPECT_EQ(LayoutRect(65, 20, 2040, 1040), flex_box->LayoutOverflowRect());
  EXPECT_EQ(IntSize(1595, 716), scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(IntSize(0, 0), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(IntPoint(0, 0), scrollable_area->ScrollOrigin());
  EXPECT_EQ(FloatPoint(0, 0), scrollable_area->ScrollPosition());

  const auto* child = GetLayoutBoxByElementId("child");
  EXPECT_EQ(LayoutPoint(105, 30), child->Location());
  EXPECT_EQ(PhysicalOffset(105, 30), child->PhysicalLocation());
}

TEST_P(LayoutFlexibleBoxTest, GeometriesWithScrollbarsRTLRowReverseVLR) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="rtl row-reverse vlr">
      <div id="child"></div>
    </div>
  )HTML");
  ExpectSameAsRowVLR();
}

TEST_P(LayoutFlexibleBoxTest, GeometriesWithScrollbarsRTLRowReverseVRL) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="rtl row-reverse vrl">
      <div id="child"></div>
    </div>
  )HTML");
  ExpectSameAsRowVRL();
}

TEST_P(LayoutFlexibleBoxTest, ResizedFlexChildRequiresVisualOverflowRecalc) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        display: flex;
        flex-direction: column;
        width: 100px;
        height: 1000px;
      }
      #child1 {
        flex-grow: 1;
        width: 100px;
        will-change: transform;
      }
      #overflow-child {
        width: 100px;
        height: 950px;
        box-shadow: 5px 10px;
      }
      #child2 {
        width: 100px;
      }
    </style>
    <div id="parent">
      <div id="child1">
        <div id="overflow-child"></div>
      </div>
      <div id="child2"></div>
    </div>
  )HTML");
  auto* child1_element = GetElementById("child1");
  auto* child2_element = GetElementById("child2");
  child2_element->setAttribute(html_names::kStyleAttr, "height: 100px;");
  GetDocument().View()->UpdateLifecycleToLayoutClean();

  auto* child1_box = ToLayoutBox(child1_element->GetLayoutObject());
  ASSERT_TRUE(child1_box->HasSelfPaintingLayer());
  EXPECT_TRUE(child1_box->Layer()->NeedsVisualOverflowRecalcForTesting());

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(child1_box->PhysicalVisualOverflowRect(),
            PhysicalRect(0, 0, 105, 960));
}

}  // namespace blink
