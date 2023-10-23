// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

namespace {

class LayoutTableSectionTest : public RenderingTest {
 protected:
  LayoutBox* GetSectionByElementIdAsBox(const char* id) {
    return To<LayoutBox>(GetLayoutObjectByElementId(id));
  }
};

TEST_F(LayoutTableSectionTest,
       BackgroundIsKnownToBeOpaqueWithLayerAndCollapsedBorder) {
  SetBodyInnerHTML(R"HTML(
    <table style='border-collapse: collapse'>
      <thead id='section' style='will-change: transform;
           background-color: blue'>
        <tr><td>Cell</td></tr>
      </thead>
    </table>
  )HTML");

  auto* section = GetSectionByElementIdAsBox("section");
  EXPECT_TRUE(section);
  EXPECT_FALSE(
      section->BackgroundIsKnownToBeOpaqueInRect(PhysicalRect(0, 0, 1, 1)));
}

TEST_F(LayoutTableSectionTest, BackgroundIsKnownToBeOpaqueWithBorderSpacing) {
  SetBodyInnerHTML(R"HTML(
    <table style='border-spacing: 10px'>
      <thead id='section' style='background-color: blue'>
        <tr><td>Cell</td></tr>
      </thead>
    </table>
  )HTML");

  auto* section = GetSectionByElementIdAsBox("section");
  EXPECT_TRUE(section);
  EXPECT_FALSE(
      section->BackgroundIsKnownToBeOpaqueInRect(PhysicalRect(0, 0, 1, 1)));
}

TEST_F(LayoutTableSectionTest, BackgroundIsKnownToBeOpaqueWithEmptyCell) {
  SetBodyInnerHTML(R"HTML(
    <table style='border-spacing: 10px'>
      <thead id='section' style='background-color: blue'>
        <tr><td>Cell</td></tr>
        <tr><td>Cell</td><td>Cell</td></tr>
      </thead>
    </table>
  )HTML");

  auto* section = GetSectionByElementIdAsBox("section");
  EXPECT_TRUE(section);
  EXPECT_FALSE(
      section->BackgroundIsKnownToBeOpaqueInRect(PhysicalRect(0, 0, 1, 1)));
}

TEST_F(LayoutTableSectionTest, VisualOverflowWithCollapsedBorders) {
  SetBodyInnerHTML(R"HTML(
    <style>
      table { border-collapse: collapse }
      td { border: 0px solid blue; padding: 0 }
      div { width: 100px; height: 100px }
    </style>
    <table>
      <tbody id='section'>
        <tr>
          <td style='border-bottom-width: 10px;
              outline: 3px solid blue'><div></div></td>
          <td style='border-width: 3px 15px'><div></div></td>
        </tr>
        <tr style='outline: 8px solid green'><td><div></div></td></tr>
      </tbody>
    </table>
  )HTML");

  auto* section = GetSectionByElementIdAsBox("section");

  // The section's self visual overflow doesn't cover the collapsed borders.
  EXPECT_EQ(section->PhysicalBorderBoxRect(),
            section->SelfVisualOverflowRect());

  // The section's visual overflow covers self visual overflow and visual
  // overflows rows.
  PhysicalRect expected_visual_overflow = section->PhysicalBorderBoxRect();
  expected_visual_overflow.ExpandEdges(LayoutUnit(3), LayoutUnit(8),
                                       LayoutUnit(8), LayoutUnit(8));
  EXPECT_EQ(expected_visual_overflow, section->VisualOverflowRect());
}

TEST_F(LayoutTableSectionTest, RowCollapseNegativeHeightCrash) {
  // Table % height triggers the heuristic check for relayout of cells at
  // https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/layout/layout_table_section.cc?rcl=5ea6fa63d8809f990d662182d971facbf557f812&l=1899
  // Cell child needs a % height to set cell_children_flex at line 1907, which
  // caused a negative override height to get set at 1929, which DCHECKed.
  SetBodyInnerHTML(R"HTML(
    <table style="height:50%">
      <tr style="visibility:collapse">
        <td>
          <div style="height:50%"></div>
        </td>
      </tr>
    </table>
  )HTML");
}

}  // anonymous namespace

}  // namespace blink
