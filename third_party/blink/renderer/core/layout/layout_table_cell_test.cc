/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/layout_table_cell.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutTableCellDeathTest : public RenderingTest,
                                 private ScopedLayoutNGForTest {
 protected:
  // These tests test Legacy behavior only.
  LayoutTableCellDeathTest() : ScopedLayoutNGForTest(false) {}

  void SetUp() override {
    RenderingTest::SetUp();
    auto builder =
        GetDocument().GetStyleResolver().CreateComputedStyleBuilder();
    builder.SetDisplay(EDisplay::kTableCell);
    cell_ = LayoutTableCell::CreateAnonymous(
        &GetDocument(), builder.TakeStyle(), LegacyLayout::kAuto);
  }

  void TearDown() override {
    cell_->Destroy();
    RenderingTest::TearDown();
  }

  Persistent<LayoutTableCell> cell_;
};

TEST_F(LayoutTableCellDeathTest, CanSetColumn) {
  static const unsigned kColumnIndex = 10;
  cell_->SetAbsoluteColumnIndex(kColumnIndex);
  EXPECT_EQ(kColumnIndex, cell_->AbsoluteColumnIndex());
}

TEST_F(LayoutTableCellDeathTest, CanSetColumnToMaxColumnIndex) {
  cell_->SetAbsoluteColumnIndex(kMaxColumnIndex);
  EXPECT_EQ(kMaxColumnIndex, cell_->AbsoluteColumnIndex());
}

// Death tests don't work properly on Android.
#if defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)

TEST_F(LayoutTableCellDeathTest, CrashIfColumnOverflowOnSetting) {
  ASSERT_DEATH(cell_->SetAbsoluteColumnIndex(kMaxColumnIndex + 1), "");
}

TEST_F(LayoutTableCellDeathTest, CrashIfSettingUnsetColumnIndex) {
  ASSERT_DEATH(cell_->SetAbsoluteColumnIndex(kUnsetColumnIndex), "");
}

#endif

class LayoutTableCellTest : public RenderingTest {
 protected:
  bool IsInStartColumn(const LayoutTableCell* cell) {
    return cell->IsInStartColumn();
  }
  bool IsInEndColumn(const LayoutTableCell* cell) {
    return cell->IsInEndColumn();
  }
  LayoutTableCell* GetCellByElementId(const char* id) {
    return To<LayoutTableCell>(GetLayoutObjectByElementId(id));
  }
  LayoutNGTableCellInterface* GetCellInterfaceByElementId(const char* id) {
    return ToInterface<LayoutNGTableCellInterface>(
        GetLayoutObjectByElementId(id));
  }
};

TEST_F(LayoutTableCellTest, ResetColspanIfTooBig) {
  SetBodyInnerHTML("<table><td id='cell' colspan='14000'></td></table>");
  ASSERT_EQ(GetCellInterfaceByElementId("cell")->ColSpan(), 1000U);
}

TEST_F(LayoutTableCellTest, DoNotResetColspanJustBelowBoundary) {
  SetBodyInnerHTML("<table><td id='cell' colspan='1000'></td></table>");
  ASSERT_EQ(GetCellInterfaceByElementId("cell")->ColSpan(), 1000U);
}

TEST_F(LayoutTableCellTest, ResetRowspanIfTooBig) {
  SetBodyInnerHTML("<table><td id='cell' rowspan='70000'></td></table>");
  ASSERT_EQ(GetCellInterfaceByElementId("cell")->ResolvedRowSpan(), 65534U);
}

TEST_F(LayoutTableCellTest, DoNotResetRowspanJustBelowBoundary) {
  SetBodyInnerHTML("<table><td id='cell' rowspan='65534'></td></table>");
  ASSERT_EQ(GetCellInterfaceByElementId("cell")->ResolvedRowSpan(), 65534U);
}

TEST_F(LayoutTableCellTest,
       BackgroundIsKnownToBeOpaqueWithLayerAndCollapsedBorder) {
  SetBodyInnerHTML(R"HTML(
    <table style='border-collapse: collapse'>
      <td id='cell' style='will-change: transform; background-color: blue'>
        Cell
      </td>
    </table>
  )HTML");

  EXPECT_FALSE(
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("cell"))
          ->BackgroundIsKnownToBeOpaqueInRect(PhysicalRect(0, 0, 1, 1)));
}

TEST_F(LayoutTableCellTest, RepaintContentInTableCell) {
  const char* body_content =
      "<table id='table' style='position: absolute; left: 1px;'>"
      "  <tr>"
      "    <td id='cell'>"
      "      <div style='display: inline-block; height: 20px; width: 20px'>"
      "    </td>"
      "  </tr>"
      "</table>";
  SetBodyInnerHTML(body_content);

  // Create an overflow recalc.
  Element* cell = GetDocument().getElementById("cell");
  cell->setAttribute(html_names::kStyleAttr, "outline: 1px solid black;");
  // Trigger a layout on the table that doesn't require cell layout.
  Element* table = GetDocument().getElementById("table");
  table->setAttribute(html_names::kStyleAttr, "position: absolute; left: 2px;");
  UpdateAllLifecyclePhasesForTest();

  // Check that overflow was calculated on the cell.
  auto* input_block = To<LayoutBlock>(cell->GetLayoutObject());
  EXPECT_EQ(PhysicalRect(-1, -1, 24, 24), input_block->LocalVisualRect());
}

TEST_F(LayoutTableCellTest, IsInStartAndEndColumn) {
  // TablesNG does not have Start/End column API.
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <table id='table'>
      <tr>
        <td id='cell11' colspan='2000'></td>
        <td id='cell12'></td>
        <td id='cell13'></td>
      </tr>
      <tr>
        <td id='cell21' rowspan='2'></td>
        <td id='cell22'></td>
        <td id='cell23' colspan='2000'></td>
      </tr>
      <tr>
        <td id='cell31'></td>
        <td id='cell32'></td>
      </tr>
    </table>
  )HTML");

  const auto* cell11 = GetCellByElementId("cell11");
  const auto* cell12 = GetCellByElementId("cell12");
  const auto* cell13 = GetCellByElementId("cell13");
  const auto* cell21 = GetCellByElementId("cell21");
  const auto* cell22 = GetCellByElementId("cell22");
  const auto* cell23 = GetCellByElementId("cell23");
  const auto* cell31 = GetCellByElementId("cell31");
  const auto* cell32 = GetCellByElementId("cell32");

  EXPECT_TRUE(IsInStartColumn(cell11));
  EXPECT_FALSE(IsInEndColumn(cell11));
  EXPECT_FALSE(IsInStartColumn(cell12));
  EXPECT_FALSE(IsInEndColumn(cell12));
  EXPECT_FALSE(IsInStartColumn(cell13));
  EXPECT_TRUE(IsInEndColumn(cell13));

  EXPECT_TRUE(IsInStartColumn(cell21));
  EXPECT_FALSE(IsInEndColumn(cell21));
  EXPECT_FALSE(IsInStartColumn(cell22));
  EXPECT_FALSE(IsInEndColumn(cell22));
  EXPECT_FALSE(IsInStartColumn(cell23));
  EXPECT_TRUE(IsInEndColumn(cell23));

  EXPECT_FALSE(IsInStartColumn(cell31));
  EXPECT_FALSE(IsInEndColumn(cell31));
  EXPECT_FALSE(IsInStartColumn(cell32));
  EXPECT_FALSE(IsInEndColumn(cell32));
}

TEST_F(LayoutTableCellTest, IsInStartAndEndColumnRTL) {
  // TablesNG does not have Start/End column API.
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
      table { direction: rtl }
      td { direction: ltr }
    </style>
    <table id='table'>
      <tr>
        <td id='cell11' colspan='2000'></td>
        <td id='cell12'></td>
        <td id='cell13'></td>
      </tr>
      <tr>
        <td id='cell21' rowspan='2'></td>
        <td id='cell22'></td>
        <td id='cell23' colspan='2000'></td>
      </tr>
      <tr>
        <td id='cell31'></td>
        <td id='cell32'></td>
      </tr>
    </table>
  )HTML");

  const auto* cell11 = GetCellByElementId("cell11");
  const auto* cell12 = GetCellByElementId("cell12");
  const auto* cell13 = GetCellByElementId("cell13");
  const auto* cell21 = GetCellByElementId("cell21");
  const auto* cell22 = GetCellByElementId("cell22");
  const auto* cell23 = GetCellByElementId("cell23");
  const auto* cell31 = GetCellByElementId("cell31");
  const auto* cell32 = GetCellByElementId("cell32");

  EXPECT_TRUE(IsInStartColumn(cell11));
  EXPECT_FALSE(IsInEndColumn(cell11));
  EXPECT_FALSE(IsInStartColumn(cell12));
  EXPECT_FALSE(IsInEndColumn(cell12));
  EXPECT_FALSE(IsInStartColumn(cell13));
  EXPECT_TRUE(IsInEndColumn(cell13));

  EXPECT_TRUE(IsInStartColumn(cell21));
  EXPECT_FALSE(IsInEndColumn(cell21));
  EXPECT_FALSE(IsInStartColumn(cell22));
  EXPECT_FALSE(IsInEndColumn(cell22));
  EXPECT_FALSE(IsInStartColumn(cell23));
  EXPECT_TRUE(IsInEndColumn(cell23));

  EXPECT_FALSE(IsInStartColumn(cell31));
  EXPECT_FALSE(IsInEndColumn(cell31));
  EXPECT_FALSE(IsInStartColumn(cell32));
  EXPECT_FALSE(IsInEndColumn(cell32));
}

TEST_F(LayoutTableCellTest, BorderWidthsWithCollapsedBorders) {
  SetBodyInnerHTML(R"HTML(
    <style>
      table { border-collapse: collapse }
      td { border: 0px solid blue; padding: 0 }
      div { width: 100px; height: 100px }
    </style>
    <table id="table">
      <tr>
        <td id='cell1' style='border-bottom-width: 10px;
            outline: 3px solid blue'><div></div></td>
        <td id='cell2' style='border-width: 3px 15px'><div></div></td>
      </tr>
    </table>
  )HTML");

  LayoutNGTableInterface* table_interface =
      ToInterface<LayoutNGTableInterface>(GetLayoutObjectByElementId("table"));
  LayoutBoxModelObject* cell1box =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("cell1"));
  LayoutBoxModelObject* cell2box =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("cell2"));

  EXPECT_TRUE(table_interface->ShouldCollapseBorders());

  LayoutUnit border_right = RuntimeEnabledFeatures::LayoutNGEnabled()
                                ? LayoutUnit(7.5)
                                : LayoutUnit(7);
  LayoutUnit border_left = RuntimeEnabledFeatures::LayoutNGEnabled()
                               ? LayoutUnit(7.5)
                               : LayoutUnit(8);
  LayoutUnit border_top = RuntimeEnabledFeatures::LayoutNGEnabled()
                              ? LayoutUnit(1.5)
                              : LayoutUnit(2);
  LayoutUnit border_bottom = RuntimeEnabledFeatures::LayoutNGEnabled()
                                 ? LayoutUnit(1.5)
                                 : LayoutUnit(1);

  EXPECT_EQ(0, cell1box->BorderLeft());
  EXPECT_EQ(border_right, cell1box->BorderRight());
  EXPECT_EQ(0, cell1box->BorderTop());
  EXPECT_EQ(5, cell1box->BorderBottom());
  EXPECT_EQ(border_left, cell2box->BorderLeft());
  EXPECT_EQ(border_right, cell2box->BorderRight());
  EXPECT_EQ(border_top, cell2box->BorderTop());
  EXPECT_EQ(border_bottom, cell2box->BorderBottom());

  // TablesNG do not have CollapsedBorder APIs.
  if (!RuntimeEnabledFeatures::LayoutNGEnabled()) {
    auto* cell1 = GetCellByElementId("cell1");
    auto* cell2 = GetCellByElementId("cell2");
    EXPECT_EQ(0u, cell1->CollapsedInnerBorderStart());
    EXPECT_EQ(7u, cell1->CollapsedInnerBorderEnd());
    EXPECT_EQ(0u, cell1->CollapsedInnerBorderBefore());
    EXPECT_EQ(5u, cell1->CollapsedInnerBorderAfter());
    EXPECT_EQ(8u, cell2->CollapsedInnerBorderStart());
    EXPECT_EQ(7u, cell2->CollapsedInnerBorderEnd());
    EXPECT_EQ(2u, cell2->CollapsedInnerBorderBefore());
    EXPECT_EQ(1u, cell2->CollapsedInnerBorderAfter());

    EXPECT_EQ(0u, cell1->CollapsedOuterBorderStart());
    EXPECT_EQ(8u, cell1->CollapsedOuterBorderEnd());
    EXPECT_EQ(0u, cell1->CollapsedOuterBorderBefore());
    EXPECT_EQ(5u, cell1->CollapsedOuterBorderAfter());
    EXPECT_EQ(7u, cell2->CollapsedOuterBorderStart());
    EXPECT_EQ(8u, cell2->CollapsedOuterBorderEnd());
    EXPECT_EQ(1u, cell2->CollapsedOuterBorderBefore());
    EXPECT_EQ(2u, cell2->CollapsedOuterBorderAfter());
  }

  To<Element>(table_interface->ToLayoutObject()->GetNode())
      ->setAttribute(html_names::kStyleAttr,
                     "writing-mode: vertical-rl; direction: rtl");
  UpdateAllLifecyclePhasesForTest();

  NGPhysicalBoxStrut cell1_borders(LayoutUnit(8), LayoutUnit(), LayoutUnit(),
                                   LayoutUnit(5));
  NGPhysicalBoxStrut cell2_borders(LayoutUnit(8), LayoutUnit(1), LayoutUnit(7),
                                   LayoutUnit(2));

  // TableNG results are completely different, because
  // Legacy has a bug, and rotates the borders.
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    cell1_borders = NGPhysicalBoxStrut(LayoutUnit(1.5), LayoutUnit(0),
                                       LayoutUnit(5), LayoutUnit(0));
    cell2_borders = NGPhysicalBoxStrut(LayoutUnit(1.5), LayoutUnit(7.5),
                                       LayoutUnit(1.5), LayoutUnit(7.5));
  }

  EXPECT_EQ(cell1_borders.left, cell1box->BorderLeft());
  EXPECT_EQ(cell1_borders.right, cell1box->BorderRight());
  EXPECT_EQ(cell1_borders.top, cell1box->BorderTop());
  EXPECT_EQ(cell1_borders.bottom, cell1box->BorderBottom());
  EXPECT_EQ(cell2_borders.left, cell2box->BorderLeft());
  EXPECT_EQ(cell2_borders.right, cell2box->BorderRight());
  EXPECT_EQ(cell2_borders.top, cell2box->BorderTop());
  EXPECT_EQ(cell2_borders.bottom, cell2box->BorderBottom());

  // TablesNG do not have CollapsedBorder APIs.
  if (!RuntimeEnabledFeatures::LayoutNGEnabled()) {
    auto* cell1 = GetCellByElementId("cell1");
    auto* cell2 = GetCellByElementId("cell2");
    EXPECT_EQ(0u, cell1->CollapsedInnerBorderStart());
    EXPECT_EQ(8u, cell1->CollapsedInnerBorderEnd());
    EXPECT_EQ(0u, cell1->CollapsedInnerBorderBefore());
    EXPECT_EQ(5u, cell1->CollapsedInnerBorderAfter());
    EXPECT_EQ(7u, cell2->CollapsedInnerBorderStart());
    EXPECT_EQ(8u, cell2->CollapsedInnerBorderEnd());
    EXPECT_EQ(1u, cell2->CollapsedInnerBorderBefore());
    EXPECT_EQ(2u, cell2->CollapsedInnerBorderAfter());

    EXPECT_EQ(0u, cell1->CollapsedOuterBorderStart());
    EXPECT_EQ(7u, cell1->CollapsedOuterBorderEnd());
    EXPECT_EQ(0u, cell1->CollapsedOuterBorderBefore());
    EXPECT_EQ(5u, cell1->CollapsedOuterBorderAfter());
    EXPECT_EQ(8u, cell2->CollapsedOuterBorderStart());
    EXPECT_EQ(7u, cell2->CollapsedOuterBorderEnd());
    EXPECT_EQ(2u, cell2->CollapsedOuterBorderBefore());
    EXPECT_EQ(1u, cell2->CollapsedOuterBorderAfter());
  }
}

TEST_F(LayoutTableCellTest, HasNonCollapsedBorderDecoration) {
  // TablesNG does not use HasNonCollapsedBorderDecoration.
  // It has been replaced by HasCollapsedBorders flag on PhysicalFragment.
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    return;
  SetBodyInnerHTML(R"HTML(
    <table>
      <tr><td id="cell"></td></tr>
    </table>
  )HTML");
  auto* cell = GetCellByElementId("cell");
  EXPECT_FALSE(cell->HasNonCollapsedBorderDecoration());

  To<Element>(cell->GetNode())
      ->setAttribute(html_names::kStyleAttr, "border: 1px solid black");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(cell->HasNonCollapsedBorderDecoration());

  To<Element>(cell->Table()->GetNode())
      ->setAttribute(html_names::kStyleAttr, "border-collapse: collapse");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(cell->HasNonCollapsedBorderDecoration());

  To<Element>(cell->GetNode())
      ->setAttribute(html_names::kStyleAttr, "border: 2px solid black");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(cell->HasNonCollapsedBorderDecoration());

  To<Element>(cell->Table()->GetNode())
      ->setAttribute(html_names::kStyleAttr, "");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(cell->HasNonCollapsedBorderDecoration());
}

}  // namespace blink
