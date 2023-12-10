// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutFlexibleBoxTest : public RenderingTest {
 public:
  LayoutFlexibleBoxTest() = default;

 protected:
  void ExpectSameAsRowHTB();
  void ExpectSameAsRowVLR();
  void ExpectSameAsRowVRL();
  void ExpectSameAsRowReverseVLR();
  void ExpectSameAsRowReverseVRL();
  void ExpectSameAsRTLRowHTB();
};

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
  EXPECT_EQ(PhysicalRect(0, 0, 540, 400), flex_box->PhysicalBorderBoxRect());
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

  EXPECT_EQ(PhysicalOffset(), flex_box->ScrolledContentOffset());
  EXPECT_EQ(ScrollOffset(), flex_box->GetScrollableArea()->GetScrollOffset());
}

void LayoutFlexibleBoxTest::ExpectSameAsRowHTB() {
  const auto* flex_box = GetLayoutBoxByElementId("flex-box");
  const auto* scrollable_area = flex_box->GetScrollableArea();
  CheckFlexBoxPhysicalGeometries(flex_box);

  EXPECT_EQ(gfx::Vector2d(), flex_box->OriginAdjustmentForScrollbars());
  // 1040 = child_height (1000) + padding_top (10) + padding_bottom (30)
  EXPECT_EQ(PhysicalRect(50, 20, 2060, 1040),
            flex_box->ScrollableOverflowRect());
  EXPECT_EQ(gfx::Vector2d(1615, 716),
            scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(gfx::Vector2d(), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(gfx::Point(), scrollable_area->ScrollOrigin());
  EXPECT_EQ(gfx::PointF(), scrollable_area->ScrollPosition());

  const auto* child = GetLayoutBoxByElementId("child");
  EXPECT_EQ(PhysicalOffset(90, 30), child->PhysicalLocation());
}

TEST_F(LayoutFlexibleBoxTest, GeometriesWithScrollbarsRowHTB) {
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

  EXPECT_EQ(gfx::Vector2d(), flex_box->OriginAdjustmentForScrollbars());
  EXPECT_EQ(PhysicalRect(50, 20, 2060, 1040),
            flex_box->ScrollableOverflowRect());
  EXPECT_EQ(gfx::Vector2d(1615, 716),
            scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(gfx::Vector2d(), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(gfx::Point(), scrollable_area->ScrollOrigin());
  EXPECT_EQ(gfx::PointF(), scrollable_area->ScrollPosition());

  const auto* child = GetLayoutBoxByElementId("child");
  EXPECT_EQ(PhysicalOffset(90, 30), child->PhysicalLocation());
}

TEST_F(LayoutFlexibleBoxTest, GeometriesWithScrollbarsRowVLR) {
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

  EXPECT_EQ(gfx::Vector2d(), flex_box->OriginAdjustmentForScrollbars());
  EXPECT_EQ(PhysicalRect(-1565, 20, 2060, 1040),
            flex_box->ScrollableOverflowRect());
  EXPECT_EQ(gfx::Vector2d(0, 716), scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(gfx::Vector2d(-1615, 0), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(gfx::Point(1615, 0), scrollable_area->ScrollOrigin());
  EXPECT_EQ(gfx::PointF(1615, 0), scrollable_area->ScrollPosition());

  const auto* child = GetLayoutBoxByElementId("child");
  // 65 = border_right (30) + padding_right (20) + vertical_scrollbar_width (15)
  // -1525 = full_flex_box_width (540) - 65 - child_width (2000))
  EXPECT_EQ(PhysicalOffset(-1525, 30), child->PhysicalLocation());
}

TEST_F(LayoutFlexibleBoxTest, GeometriesWithScrollbarsRowVRL) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="row vrl">
      <div id="child"></div>
    </div>
  )HTML");
  ExpectSameAsRowVRL();
}

TEST_F(LayoutFlexibleBoxTest, GeometriesWithScrollbarsRowReverseHTB) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="row-reverse htb">
      <div id="child"></div>
    </div>
  )HTML");

  const auto* flex_box = GetLayoutBoxByElementId("flex-box");
  const auto* scrollable_area = flex_box->GetScrollableArea();
  CheckFlexBoxPhysicalGeometries(flex_box);

  EXPECT_EQ(gfx::Vector2d(), flex_box->OriginAdjustmentForScrollbars());
  EXPECT_EQ(PhysicalRect(-1565, 20, 2060, 1040),
            flex_box->ScrollableOverflowRect());
  EXPECT_EQ(gfx::Vector2d(0, 716), scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(gfx::Vector2d(-1615, 0), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(gfx::Point(1615, 0), scrollable_area->ScrollOrigin());
  EXPECT_EQ(gfx::PointF(1615, 0), scrollable_area->ScrollPosition());

  const auto* child = GetLayoutBoxByElementId("child");
  EXPECT_EQ(PhysicalOffset(-1525, 30), child->PhysicalLocation());
}

void LayoutFlexibleBoxTest::ExpectSameAsRowReverseVLR() {
  const auto* flex_box = GetLayoutBoxByElementId("flex-box");
  const auto* scrollable_area = flex_box->GetScrollableArea();
  CheckFlexBoxPhysicalGeometries(flex_box);

  EXPECT_EQ(gfx::Vector2d(), flex_box->OriginAdjustmentForScrollbars());
  EXPECT_EQ(PhysicalRect(50, -696, 2060, 1040),
            flex_box->ScrollableOverflowRect());
  EXPECT_EQ(gfx::Vector2d(1615, 0), scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(gfx::Vector2d(0, -716), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(gfx::Point(0, 716), scrollable_area->ScrollOrigin());
  EXPECT_EQ(gfx::PointF(0, 716), scrollable_area->ScrollPosition());

  const auto* child = GetLayoutBoxByElementId("child");
  EXPECT_EQ(PhysicalOffset(90, -686), child->PhysicalLocation());
}

TEST_F(LayoutFlexibleBoxTest, GeometriesWithScrollbarsRowReverseVLR) {
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

  EXPECT_EQ(gfx::Vector2d(), flex_box->OriginAdjustmentForScrollbars());
  EXPECT_EQ(PhysicalRect(-1565, -696, 2060, 1040),
            flex_box->ScrollableOverflowRect());
  EXPECT_EQ(gfx::Vector2d(), scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(gfx::Vector2d(-1615, -716),
            scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(gfx::Point(1615, 716), scrollable_area->ScrollOrigin());
  EXPECT_EQ(gfx::PointF(1615, 716), scrollable_area->ScrollPosition());

  const auto* child = GetLayoutBoxByElementId("child");
  // 65 = border_right (30) + padding_right (20) + vertical_scrollbar_width (15)
  // -1525 = full_flex_box_width (540) - 65 - child_width (2000))
  EXPECT_EQ(PhysicalOffset(-1525, -686), child->PhysicalLocation());
}

TEST_F(LayoutFlexibleBoxTest, GeometriesWithScrollbarsRowReverseVRL) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="row-reverse vrl">
      <div id="child"></div>
    </div>
  )HTML");
  ExpectSameAsRowReverseVRL();
}

TEST_F(LayoutFlexibleBoxTest, GeometriesWithScrollbarsColumnHTB) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="column htb">
      <div id="child"></div>
    </div>
  )HTML");
  ExpectSameAsRowHTB();
}

TEST_F(LayoutFlexibleBoxTest, GeometriesWithScrollbarsColumnVLR) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="column vlr">
      <div id="child"></div>
    </div>
  )HTML");
  ExpectSameAsRowVLR();
}

TEST_F(LayoutFlexibleBoxTest, GeometriesWithScrollbarsColumnVRL) {
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

TEST_F(LayoutFlexibleBoxTest, GeometriesWithScrollbarsColumnReverseHTB) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="column-reverse htb">
      <div id="child"></div>
    </div>
  )HTML");

  const auto* flex_box = GetLayoutBoxByElementId("flex-box");
  const auto* scrollable_area = flex_box->GetScrollableArea();
  CheckFlexBoxPhysicalGeometries(flex_box);

  EXPECT_EQ(gfx::Vector2d(), flex_box->OriginAdjustmentForScrollbars());
  EXPECT_EQ(PhysicalRect(50, -696, 2060, 1040),
            flex_box->ScrollableOverflowRect());
  EXPECT_EQ(gfx::Vector2d(1615, 0), scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(gfx::Vector2d(0, -716), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(gfx::Point(0, 716), scrollable_area->ScrollOrigin());
  EXPECT_EQ(gfx::PointF(0, 716), scrollable_area->ScrollPosition());

  const auto* child = GetLayoutBoxByElementId("child");
  EXPECT_EQ(PhysicalOffset(90, -686), child->PhysicalLocation());
}

TEST_F(LayoutFlexibleBoxTest, GeometriesWithScrollbarsColumnReverseVLR) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="column-reverse vlr">
      <div id="child"></div>
    </div>
  )HTML");

  const auto* flex_box = GetLayoutBoxByElementId("flex-box");
  const auto* scrollable_area = flex_box->GetScrollableArea();
  CheckFlexBoxPhysicalGeometries(flex_box);

  EXPECT_EQ(gfx::Vector2d(), flex_box->OriginAdjustmentForScrollbars());
  EXPECT_EQ(PhysicalRect(-1565, 20, 2060, 1040),
            flex_box->ScrollableOverflowRect());
  EXPECT_EQ(gfx::Vector2d(0, 716), scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(gfx::Vector2d(-1615, 0), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(gfx::Point(1615, 0), scrollable_area->ScrollOrigin());
  EXPECT_EQ(gfx::PointF(1615, 0), scrollable_area->ScrollPosition());

  const auto* child = GetLayoutBoxByElementId("child");
  EXPECT_EQ(PhysicalOffset(-1525, 30), child->PhysicalLocation());
}

TEST_F(LayoutFlexibleBoxTest, GeometriesWithScrollbarsColumnReverseVRL) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="column-reverse vrl">
      <div id="child"></div>
    </div>
  )HTML");

  const auto* flex_box = GetLayoutBoxByElementId("flex-box");
  const auto* scrollable_area = flex_box->GetScrollableArea();
  CheckFlexBoxPhysicalGeometries(flex_box);

  EXPECT_EQ(gfx::Vector2d(), flex_box->OriginAdjustmentForScrollbars());
  EXPECT_EQ(PhysicalRect(50, 20, 2060, 1040),
            flex_box->ScrollableOverflowRect());
  EXPECT_EQ(gfx::Vector2d(1615, 716),
            scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(gfx::Vector2d(), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(gfx::Point(), scrollable_area->ScrollOrigin());
  EXPECT_EQ(gfx::PointF(), scrollable_area->ScrollPosition());

  const auto* child = GetLayoutBoxByElementId("child");
  EXPECT_EQ(PhysicalOffset(90, 30), child->PhysicalLocation());
}

void LayoutFlexibleBoxTest::ExpectSameAsRTLRowHTB() {
  const auto* flex_box = GetLayoutBoxByElementId("flex-box");
  const auto* scrollable_area = flex_box->GetScrollableArea();
  CheckFlexBoxPhysicalGeometries(flex_box);

  // Additional origin due to the scrollbar on the left.
  EXPECT_EQ(gfx::Vector2d(15, 0), flex_box->OriginAdjustmentForScrollbars());
  EXPECT_EQ(PhysicalRect(-1550, 20, 2060, 1040),
            flex_box->ScrollableOverflowRect());
  EXPECT_EQ(gfx::Vector2d(0, 716), scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(gfx::Vector2d(-1615, 0), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(gfx::Point(1615, 0), scrollable_area->ScrollOrigin());
  EXPECT_EQ(gfx::PointF(1615, 0), scrollable_area->ScrollPosition());

  const auto* child = GetLayoutBoxByElementId("child");
  EXPECT_EQ(PhysicalOffset(-1510, 30), child->PhysicalLocation());
}

TEST_F(LayoutFlexibleBoxTest, GeometriesWithScrollbarsRTLRowHTB) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="rtl row htb">
      <div id="child"></div>
    </div>
  )HTML");
  ExpectSameAsRTLRowHTB();
}

TEST_F(LayoutFlexibleBoxTest, GeometriesWithScrollbarsRTLRowVLR) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="rtl row vlr">
      <div id="child"></div>
    </div>
  )HTML");
  ExpectSameAsRowReverseVLR();
}

TEST_F(LayoutFlexibleBoxTest, GeometriesWithScrollbarsRTLRowVRL) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="rtl row vrl">
      <div id="child"></div>
    </div>
  )HTML");
  ExpectSameAsRowReverseVRL();
}

TEST_F(LayoutFlexibleBoxTest, GeometriesWithScrollbarsRTLRowReverseHTB) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="rtl row-reverse htb">
      <div id="child"></div>
    </div>
  )HTML");

  const auto* flex_box = GetLayoutBoxByElementId("flex-box");
  const auto* scrollable_area = flex_box->GetScrollableArea();
  CheckFlexBoxPhysicalGeometries(flex_box);

  // Additional origin due to the scrollbar on the left.
  EXPECT_EQ(gfx::Vector2d(15, 0), flex_box->OriginAdjustmentForScrollbars());
  EXPECT_EQ(PhysicalRect(65, 20, 2060, 1040),
            flex_box->ScrollableOverflowRect());
  EXPECT_EQ(gfx::Vector2d(1615, 716),
            scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(gfx::Vector2d(0, 0), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(gfx::Point(0, 0), scrollable_area->ScrollOrigin());
  EXPECT_EQ(gfx::PointF(0, 0), scrollable_area->ScrollPosition());

  const auto* child = GetLayoutBoxByElementId("child");
  EXPECT_EQ(PhysicalOffset(105, 30), child->PhysicalLocation());
}

TEST_F(LayoutFlexibleBoxTest, GeometriesWithScrollbarsRTLRowReverseVLR) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="rtl row-reverse vlr">
      <div id="child"></div>
    </div>
  )HTML");
  ExpectSameAsRowVLR();
}

TEST_F(LayoutFlexibleBoxTest, GeometriesWithScrollbarsRTLRowReverseVRL) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" class="rtl row-reverse vrl">
      <div id="child"></div>
    </div>
  )HTML");
  ExpectSameAsRowVRL();
}

TEST_F(LayoutFlexibleBoxTest, ResizedFlexChildRequiresVisualOverflowRecalc) {
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
  child2_element->setAttribute(html_names::kStyleAttr,
                               AtomicString("height: 100px;"));
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);

  auto* child1_box = To<LayoutBox>(child1_element->GetLayoutObject());
  ASSERT_TRUE(child1_box->HasSelfPaintingLayer());
  EXPECT_TRUE(child1_box->Layer()->NeedsVisualOverflowRecalc());

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(child1_box->VisualOverflowRect(), PhysicalRect(0, 0, 105, 960));
}

TEST_F(LayoutFlexibleBoxTest, PercentDefiniteGapUseCounter) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div id="flex-box" style="gap: 20%;"></div>
  )HTML");
  RunDocumentLifecycle();

  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kFlexGapPositive));
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kFlexGapSpecified));
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kFlexRowGapPercent));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kFlexRowGapPercentIndefinite));
}

TEST_F(LayoutFlexibleBoxTest, PercentIndefiniteGapUseCounter) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div style="display: flex; row-gap: 20%;"></div>
  )HTML");
  RunDocumentLifecycle();

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kFlexGapPositive));
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kFlexGapSpecified));
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kFlexRowGapPercent));
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kFlexRowGapPercentIndefinite));
}

TEST_F(LayoutFlexibleBoxTest, ZeroGapUseCounter) {
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div style="display: flex; gap: 0;"></div>
  )HTML");
  RunDocumentLifecycle();

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kFlexGapPositive));
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kFlexGapSpecified));
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kFlexRowGapPercent));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kFlexRowGapPercentIndefinite));
}

TEST_F(LayoutFlexibleBoxTest, NormalGapUseCounter) {
  // 'normal' is the initial value. It resolves to non-zero for multi-col but 0
  // for flex.
  SetBodyInnerHTML(CommonStyle() + R"HTML(
    <div style="display: flex; gap: normal"></div>
    <div style="display: flex; gap: auto"></div>
    <div style="display: flex; gap: initial"></div>
    <div style="display: flex; gap: -10px"></div>
    <div style="display: flex; gap: 1hz"></div>
  )HTML");
  RunDocumentLifecycle();

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kFlexGapPositive));
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kFlexGapSpecified));
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kFlexRowGapPercent));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kFlexRowGapPercentIndefinite));
}

}  // namespace blink
