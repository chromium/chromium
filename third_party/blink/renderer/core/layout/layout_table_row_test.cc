/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/layout_table_row.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

namespace {

class LayoutTableRowDeathTest : public RenderingTest {
 protected:
  void SetUp() override {
    RenderingTest::SetUp();
    row_ = LayoutTableRow::CreateAnonymous(&GetDocument());
  }

  void TearDown() override { row_->Destroy(); }

  LayoutTableRow* row_;
};

TEST_F(LayoutTableRowDeathTest, CanSetRow) {
  static const unsigned kRowIndex = 10;
  row_->SetRowIndex(kRowIndex);
  EXPECT_EQ(kRowIndex, row_->RowIndex());
}

TEST_F(LayoutTableRowDeathTest, CanSetRowToMaxRowIndex) {
  row_->SetRowIndex(kMaxRowIndex);
  EXPECT_EQ(kMaxRowIndex, row_->RowIndex());
}

// Death tests don't work properly on Android.
#if defined(GTEST_HAS_DEATH_TEST) && !defined(OS_ANDROID)

TEST_F(LayoutTableRowDeathTest, CrashIfRowOverflowOnSetting) {
  ASSERT_DEATH(row_->SetRowIndex(kMaxRowIndex + 1), "");
}

TEST_F(LayoutTableRowDeathTest, CrashIfSettingUnsetRowIndex) {
  ASSERT_DEATH(row_->SetRowIndex(kUnsetRowIndex), "");
}

#endif

class LayoutTableRowTest : public RenderingTest {
 protected:
  LayoutBox* GetRowByElementId(const char* id) {
    return ToLayoutBox(GetLayoutObjectByElementId(id));
  }
};

TEST_F(LayoutTableRowTest,
       BackgroundIsKnownToBeOpaqueWithLayerAndCollapsedBorder) {
  SetBodyInnerHTML(R"HTML(
    <table style='border-collapse: collapse'>
      <tr id='row' style='will-change: transform;
          background-color: blue'>
        <td>Cell</td>
      </tr>
    </table>
  )HTML");

  EXPECT_FALSE(GetRowByElementId("row")->BackgroundIsKnownToBeOpaqueInRect(
      PhysicalRect(0, 0, 1, 1)));
}

TEST_F(LayoutTableRowTest, BackgroundIsKnownToBeOpaqueWithBorderSpacing) {
  SetBodyInnerHTML(R"HTML(
    <table style='border-spacing: 10px'>
      <tr id='row' style='background-color: blue'><td>Cell</td></tr>
    </table>
  )HTML");

  EXPECT_FALSE(GetRowByElementId("row")->BackgroundIsKnownToBeOpaqueInRect(
      PhysicalRect(0, 0, 1, 1)));
}

TEST_F(LayoutTableRowTest, BackgroundIsKnownToBeOpaqueWithEmptyCell) {
  SetBodyInnerHTML(R"HTML(
    <table style='border-spacing: 10px'>
      <tr id='row' style='background-color: blue'><td>Cell</td></tr>
      <tr style='background-color: blue'><td>Cell</td><td>Cell</td></tr>
    </table>
  )HTML");

  EXPECT_FALSE(GetRowByElementId("row")->BackgroundIsKnownToBeOpaqueInRect(
      PhysicalRect(0, 0, 1, 1)));
}

TEST_F(LayoutTableRowTest, VisualOverflow) {
  // +---+---+---+
  // | A |   |   |      row1
  // |---| B |   |---+
  // | D |   | C |   |  row2
  // |---|---|   | E |
  // | F |   |   |   |  row3
  // +---+   +---+---+
  // Cell D has an outline which creates overflow.
  SetBodyInnerHTML(R"HTML(
    <style>
      td { width: 100px; height: 100px; padding: 0 }
    </style>
    <table style='border-spacing: 10px'>
      <tr id='row1'>
        <td>A</td>
        <td rowspan='2'>B</td>
        <td rowspan='3'>C</td>
      </tr>
      <tr id='row2'>
        <td style='outline: 10px solid blue'>D</td>
        <td rowspan='2'>E</td>
      </tr>
      <tr id='row3'>
        <td>F</td>
      </tr>
    </table>
  )HTML");

  auto* row1 = GetRowByElementId("row1");
  EXPECT_EQ(LayoutRect(120, 0, 210, 320), row1->ContentsVisualOverflowRect());
  EXPECT_EQ(LayoutRect(0, 0, 450, 320), row1->SelfVisualOverflowRect());

  auto* row2 = GetRowByElementId("row2");
  EXPECT_EQ(LayoutRect(0, -10, 440, 220), row2->ContentsVisualOverflowRect());
  EXPECT_EQ(LayoutRect(0, 0, 450, 210), row2->SelfVisualOverflowRect());

  auto* row3 = GetRowByElementId("row3");
  EXPECT_EQ(LayoutRect(), row3->ContentsVisualOverflowRect());
  EXPECT_EQ(LayoutRect(0, 0, 450, 100), row3->SelfVisualOverflowRect());
}

TEST_F(LayoutTableRowTest, VisualOverflowWithCollapsedBorders) {
  SetBodyInnerHTML(R"HTML(
    <style>
      table { border-collapse: collapse }
      td { border: 0px solid blue; padding: 0 }
      div { width: 100px; height: 100px }
    </style>
    <table>
      <tr id='row'>
        <td style='border-bottom-width: 10px;
            outline: 3px solid blue'><div></div></td>
        <td style='border-width: 3px 15px'><div></div></td>
      </tr>
    </table>
  )HTML");

  auto* row = GetRowByElementId("row");

  // The row's self visual overflow covers the collapsed borders.
  LayoutRect expected_self_visual_overflow = row->BorderBoxRect();
  expected_self_visual_overflow.ExpandEdges(LayoutUnit(1), LayoutUnit(8),
                                            LayoutUnit(5), LayoutUnit(0));
  EXPECT_EQ(expected_self_visual_overflow, row->SelfVisualOverflowRect());

  // The row's visual overflow covers self visual overflow and visual overflows
  // of all cells.
  LayoutRect expected_visual_overflow = row->BorderBoxRect();
  expected_visual_overflow.ExpandEdges(LayoutUnit(3), LayoutUnit(8),
                                       LayoutUnit(5), LayoutUnit(3));
  EXPECT_EQ(expected_visual_overflow, row->VisualOverflowRect());
}

TEST_F(LayoutTableRowTest, LayoutOverflow) {
  SetBodyInnerHTML(R"HTML(
    <table style='border-spacing: 0'>
      <tr id='row'>
        <td style='100px; height: 100px; padding: 0'>
          <div style='position: relative; top: 50px; left: 50px;
              width: 100px; height: 100px'></div>
        </td>
      </tr>
    </table>
  )HTML");

  EXPECT_EQ(LayoutRect(0, 0, 150, 150),
            GetRowByElementId("row")->LayoutOverflowRect());
}

}  // anonymous namespace

}  // namespace blink
