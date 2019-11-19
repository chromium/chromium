// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"

// This file contains tests testing TablePainter, TableSectionPainter,
// TableRowPainter and TableCellPainter. It's difficult to separate the tests
// into individual files because of dependencies among the painter classes.

using testing::ElementsAre;

namespace blink {

using TablePainterTest = PaintControllerPaintTest;
INSTANTIATE_PAINT_TEST_SUITE_P(TablePainterTest);

TEST_P(TablePainterTest, Background) {
  SetBodyInnerHTML(R"HTML(
    <style>
      td { width: 200px; height: 200px; padding: 0; border: none; }
      tr { background-color: blue; }
      table { border: none; border-spacing: 0 }
    </style>
    <table>
      <tr id='row1'><td></td></tr>
      <tr id='row2'><td></td></tr>
    </table>
  )HTML");

  LayoutObject& row1 = *GetLayoutObjectByElementId("row1");
  LayoutObject& row2 = *GetLayoutObjectByElementId("row2");

  InvalidateAll(RootPaintController());
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  Paint(IntRect(0, 0, 200, 200));

  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&ViewScrollingBackgroundClient(),
                           DisplayItem::kDocumentBackground),
                  IsSameId(&row1, DisplayItem::kBoxDecorationBackground)));

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  Paint(IntRect(0, 300, 200, 1000));

  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&ViewScrollingBackgroundClient(),
                           DisplayItem::kDocumentBackground),
                  IsSameId(&row2, DisplayItem::kBoxDecorationBackground)));
}

TEST_P(TablePainterTest, BackgroundWithCellSpacing) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      td { width: 200px; height: 150px; border: 0; background-color: green;
      }
      tr { background-color: blue; }
      table { border: none; border-spacing: 100px; border-collapse:
    separate; }
    </style>
    <table>
      <tr id='row1'><td id='cell1'></td></tr>
      <tr id='row2'><td id='cell2'></td></tr>
    </table>
  )HTML");

  LayoutObject& row1 = *GetLayoutObjectByElementId("row1");
  LayoutObject& row2 = *GetLayoutObjectByElementId("row2");
  LayoutObject& cell1 = *GetLayoutObjectByElementId("cell1");
  LayoutObject& cell2 = *GetLayoutObjectByElementId("cell2");

  InvalidateAll(RootPaintController());
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  // Intersects cell1 and the spacing between cell1 and cell2.
  Paint(IntRect(0, 200, 200, 150));

  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&ViewScrollingBackgroundClient(),
                           DisplayItem::kDocumentBackground),
                  IsSameId(&row1, DisplayItem::kBoxDecorationBackground),
                  IsSameId(&cell1, DisplayItem::kBoxDecorationBackground)));

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  // Intersects the spacing only.
  Paint(IntRect(0, 250, 100, 100));

  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&ViewScrollingBackgroundClient(),
                           DisplayItem::kDocumentBackground),
                  IsSameId(&row1, DisplayItem::kBoxDecorationBackground)));

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  // Intersects cell2 only.
  Paint(IntRect(0, 350, 200, 150));

  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&ViewScrollingBackgroundClient(),
                           DisplayItem::kDocumentBackground),
                  IsSameId(&row2, DisplayItem::kBoxDecorationBackground),
                  IsSameId(&cell2, DisplayItem::kBoxDecorationBackground)));
}

TEST_P(TablePainterTest, BackgroundInSelfPaintingRow) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0 }
      td { width: 200px; height: 200px; border: 0; background-color: green;
    }
      tr { background-color: blue; opacity: 0.5; }
      table { border: none; border-spacing: 100px; border-collapse:
    separate; }
    </style>
    <table>
      <tr id='row'><td id='cell1'><td id='cell2'></td></tr>
    </table>
  )HTML");

  LayoutObject& cell1 = *GetLayoutObjectByElementId("cell1");
  LayoutObject& cell2 = *GetLayoutObjectByElementId("cell2");
  LayoutObject& row = *GetLayoutObjectByElementId("row");

  InvalidateAll(RootPaintController());
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  // Intersects cell1 and the spacing between cell1 and cell2.
  Paint(IntRect(200, 0, 200, 200));

  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&ViewScrollingBackgroundClient(),
                           DisplayItem::kDocumentBackground),
                  IsSameId(&row, DisplayItem::kBoxDecorationBackground),
                  IsSameId(&cell1, DisplayItem::kBoxDecorationBackground)));

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  // Intersects the spacing only.
  Paint(IntRect(300, 0, 100, 100));

  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&ViewScrollingBackgroundClient(),
                                   DisplayItem::kDocumentBackground)));

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  // Intersects cell2 only.
  Paint(IntRect(450, 0, 200, 200));

  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&ViewScrollingBackgroundClient(),
                           DisplayItem::kDocumentBackground),
                  IsSameId(&row, DisplayItem::kBoxDecorationBackground),
                  IsSameId(&cell2, DisplayItem::kBoxDecorationBackground)));
}

TEST_P(TablePainterTest, CollapsedBorderAndOverflow) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0 }
      td { width: 100px; height: 100px; border: 100px solid blue; outline:
    100px solid yellow; background: green; }
      table { margin: 100px; border-collapse: collapse; }
    </style>
    <table>
      <tr><td id='cell'></td></tr>
    </table>
  )HTML");

  const LayoutObject* cell_layout_object = GetLayoutObjectByElementId("cell");
  const LayoutNGTableCellInterface* cell =
      ToInterface<LayoutNGTableCellInterface>(cell_layout_object);
  InvalidateAll(RootPaintController());
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  // Intersects the overflowing part of cell but not border box.
  Paint(IntRect(0, 0, 100, 100));

  // We should paint all display items of cell.
  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(
          IsSameId(&ViewScrollingBackgroundClient(),
                   DisplayItem::kDocumentBackground),
          IsSameId(cell_layout_object, DisplayItem::kBoxDecorationBackground),
          IsSameId(cell->RowInterface()->ToLayoutObject(),
                   DisplayItem::kTableCollapsedBorders),
          IsSameId(cell_layout_object, DisplayItem::PaintPhaseToDrawingType(
                                           PaintPhase::kSelfOutlineOnly))));
}

TEST_P(TablePainterTest, DontPaintEmptyDecorationBackground) {
  SetBodyInnerHTML(R"HTML(
    <table id="table1" style="border: 1px solid yellow">
      <tr>
        <td style="width: 100px; height: 100px; border: 2px solid blue"></td>
      </tr>
    </tr>
    <table id="table2"
           style="border-collapse: collapse; border: 1px solid yellow">
      <tr>
        <td style="width: 100px; height: 100px; border: 2px solid blue"></td>
      </tr>
    </tr>
  )HTML");

  auto* table1 = GetLayoutObjectByElementId("table1");
  auto* table2 = GetLayoutObjectByElementId("table2");
  const LayoutObject* table_1_descendant =
      ToInterface<LayoutNGTableInterface>(table1)
          ->FirstBodyInterface()
          ->FirstRowInterface()
          ->FirstCellInterface()
          ->ToLayoutObject();
  const LayoutObject* table_2_descendant =
      ToInterface<LayoutNGTableInterface>(table2)
          ->FirstBodyInterface()
          ->FirstRowInterface()
          ->ToLayoutObject();
  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(
          IsSameId(&ViewScrollingBackgroundClient(), kDocumentBackgroundType),
          IsSameId(table1, kBackgroundType),
          IsSameId(table_1_descendant, kBackgroundType),
          IsSameId(table_2_descendant, DisplayItem::kTableCollapsedBorders)));
}

}  // namespace blink
