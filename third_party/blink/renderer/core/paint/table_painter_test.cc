// Copyright 2015 The Chromium Authors
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

class TablePainterTest : public PaintControllerPaintTest,
                         private ScopedLayoutNGForTest {
 protected:
  TablePainterTest() : ScopedLayoutNGForTest(false) {}
};

// using TablePainterTest = PaintControllerPaintTest;
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

  InvalidateAll();
  UpdateAllLifecyclePhasesExceptPaint();
  PaintContents(gfx::Rect(0, 0, 200, 200));

  EXPECT_THAT(
      ContentDisplayItems(),
      ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                  IsSameId(row1.Id(), DisplayItem::kBoxDecorationBackground)));

  UpdateAllLifecyclePhasesExceptPaint();
  PaintContents(gfx::Rect(0, 300, 200, 1000));

  EXPECT_THAT(
      ContentDisplayItems(),
      ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                  IsSameId(row2.Id(), DisplayItem::kBoxDecorationBackground)));
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

  InvalidateAll();
  UpdateAllLifecyclePhasesExceptPaint();
  // Intersects cell1 and the spacing between cell1 and cell2.
  PaintContents(gfx::Rect(0, 200, 200, 150));

  EXPECT_THAT(
      ContentDisplayItems(),
      ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                  IsSameId(row1.Id(), DisplayItem::kBoxDecorationBackground),
                  IsSameId(cell1.Id(), DisplayItem::kBoxDecorationBackground)));

  UpdateAllLifecyclePhasesExceptPaint();
  // Intersects the spacing only.
  PaintContents(gfx::Rect(0, 250, 100, 100));

  EXPECT_THAT(
      ContentDisplayItems(),
      ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                  IsSameId(row1.Id(), DisplayItem::kBoxDecorationBackground)));

  UpdateAllLifecyclePhasesExceptPaint();
  // Intersects cell2 only.
  PaintContents(gfx::Rect(0, 350, 200, 150));

  EXPECT_THAT(
      ContentDisplayItems(),
      ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                  IsSameId(row2.Id(), DisplayItem::kBoxDecorationBackground),
                  IsSameId(cell2.Id(), DisplayItem::kBoxDecorationBackground)));
}

TEST_P(TablePainterTest, BackgroundInSelfPaintingRow) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0 }
      td { width: 200px; height: 200px; border: 0; background-color: green; }
      tr { background-color: blue; opacity: 0.5; }
      table { border: none; border-spacing: 100px; border-collapse: separate; }
    </style>
    <table>
      <tr id='row'><td id='cell1'><td id='cell2'></td></tr>
    </table>
  )HTML");

  LayoutObject& cell1 = *GetLayoutObjectByElementId("cell1");
  LayoutObject& cell2 = *GetLayoutObjectByElementId("cell2");
  LayoutObject& row = *GetLayoutObjectByElementId("row");

  InvalidateAll();
  UpdateAllLifecyclePhasesExceptPaint();
  // Intersects cell1 and the spacing between cell1 and cell2.
  PaintContents(gfx::Rect(200, 0, 200, 200));

  EXPECT_THAT(
      ContentDisplayItems(),
      ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                  IsSameId(row.Id(), DisplayItem::kBoxDecorationBackground),
                  IsSameId(cell1.Id(), DisplayItem::kBoxDecorationBackground)));

  UpdateAllLifecyclePhasesExceptPaint();
  // Intersects the spacing only.
  PaintContents(gfx::Rect(300, 0, 100, 100));

  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM));

  UpdateAllLifecyclePhasesExceptPaint();
  // Intersects cell2 only.
  PaintContents(gfx::Rect(450, 0, 200, 200));

  EXPECT_THAT(
      ContentDisplayItems(),
      ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                  IsSameId(row.Id(), DisplayItem::kBoxDecorationBackground),
                  IsSameId(cell2.Id(), DisplayItem::kBoxDecorationBackground)));
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
  InvalidateAll();
  UpdateAllLifecyclePhasesExceptPaint();
  // Intersects the overflowing part of cell but not border box.
  PaintContents(gfx::Rect(0, 0, 100, 100));

  // We should paint all display items of cell.
  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(cell_layout_object->Id(),
                                   DisplayItem::kBoxDecorationBackground),
                          IsSameId(cell->RowInterface()->ToLayoutObject()->Id(),
                                   DisplayItem::kTableCollapsedBorders),
                          IsSameId(cell_layout_object->Id(),
                                   DisplayItem::PaintPhaseToDrawingType(
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
  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(table1->Id(), kBackgroundType),
                          IsSameId(table_1_descendant->Id(), kBackgroundType),
                          IsSameId(table_2_descendant->Id(),
                                   DisplayItem::kTableCollapsedBorders)));
}

TEST_P(TablePainterTest, TouchActionOnTable) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      table {
        width: 100px;
        height: 100px;
        touch-action: none;
      }
    </style>
    <table></table>
  )HTML");
  const auto& paint_chunk = *ContentPaintChunks().begin();
  EXPECT_EQ(paint_chunk.hit_test_data->touch_action_rects[0].rect,
            gfx::Rect(0, 0, 100, 100));
}

TEST_P(TablePainterTest, TouchActionOnTableCell) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      table {
        border-collapse: collapse;
      }
      td {
        width: 100px;
        height: 100px;
        touch-action: none;
        padding: 0;
      }
    </style>
    <table><tr><td></td></tr></table>
  )HTML");
  const auto& paint_chunk = *ContentPaintChunks().begin();
  EXPECT_EQ(paint_chunk.hit_test_data->touch_action_rects[0].rect,
            gfx::Rect(0, 0, 100, 100));
}

}  // namespace blink
