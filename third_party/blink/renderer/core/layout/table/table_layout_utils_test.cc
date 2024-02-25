// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/table/table_layout_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/table/table_node.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class TableLayoutUtilsTest : public RenderingTest {
 public:
  TableTypes::Column MakeColumn(int min_width,
                                int max_width,
                                std::optional<float> percent = std::nullopt,
                                bool is_constrained = false) {
    return {LayoutUnit(min_width),
            LayoutUnit(max_width),
            percent,
            /* border_padding */ LayoutUnit(),
            is_constrained,
            /* is_collapsed */ false,
            /* is_table_fixed */ false,
            /* is_mergeable */ false};
  }

  TableTypes::Row MakeRow(int block_size,
                          bool is_constrained = false,
                          bool has_rowspan_start = false,
                          std::optional<float> percent = std::nullopt) {
    return {LayoutUnit(block_size), 0,       0,
            std::nullopt,           percent, is_constrained,
            has_rowspan_start,      false};
  }

  TableTypes::Section MakeSection(TableTypes::Rows* rows,
                                  int block_size,
                                  wtf_size_t rowspan = 1,
                                  std::optional<float> percent = std::nullopt) {
    wtf_size_t start_row = rows->size();
    for (wtf_size_t i = 0; i < rowspan; i++)
      rows->push_back(MakeRow(10));
    bool is_constrained = percent || block_size != 0;
    bool is_tbody = true;
    return TableTypes::Section{
        start_row, rowspan, LayoutUnit(block_size), percent, is_constrained,
        is_tbody,  false};
  }
};

TEST_F(TableLayoutUtilsTest, DistributeColspanAutoPercent) {
  TableTypes::ColspanCell colspan_cell(TableTypes::CellInlineConstraint(), 0,
                                       3);
  colspan_cell.start_column = 0;
  colspan_cell.span = 3;

  scoped_refptr<TableTypes::Columns> column_constraints =
      base::MakeRefCounted<TableTypes::Columns>();

  colspan_cell.cell_inline_constraint.percent = 60.0f;
  TableTypes::ColspanCells colspan_cells;
  colspan_cells.push_back(colspan_cell);

  // Distribute over non-percent columns proportial to max size.
  // Columns: 10px, 20px, 30%
  // Distribute 60%: 10% 20% 30%
  column_constraints->data.Shrink(0);
  column_constraints->data.push_back(MakeColumn(0, 10));
  column_constraints->data.push_back(MakeColumn(0, 20));
  column_constraints->data.push_back(MakeColumn(0, 10, 30));
  DistributeColspanCellsToColumns(colspan_cells, LayoutUnit(), false,
                                  column_constraints.get());
  EXPECT_EQ(column_constraints->data[0].percent, 10);
  EXPECT_EQ(column_constraints->data[1].percent, 20);

  // Distribute evenly over empty columns.
  // Columns: 0px 0px 10%
  // Distribute 60%: 25% 25% 10%
  column_constraints->data.Shrink(0);
  column_constraints->data.push_back(MakeColumn(0, 0));
  column_constraints->data.push_back(MakeColumn(0, 0));
  column_constraints->data.push_back(MakeColumn(0, 10, 10));
  DistributeColspanCellsToColumns(colspan_cells, LayoutUnit(), false,
                                  column_constraints.get());
  EXPECT_EQ(column_constraints->data[0].percent, 25);
  EXPECT_EQ(column_constraints->data[1].percent, 25);
}

TEST_F(TableLayoutUtilsTest, DistributeColspanAutoSizeUnconstrained) {
  TableTypes::ColspanCell colspan_cell(TableTypes::CellInlineConstraint(), 0,
                                       3);
  colspan_cell.start_column = 0;
  colspan_cell.span = 3;

  scoped_refptr<TableTypes::Columns> column_constraints =
      base::MakeRefCounted<TableTypes::Columns>();

  // Columns distributing over auto columns.
  colspan_cell.cell_inline_constraint.min_inline_size = LayoutUnit(100);
  colspan_cell.cell_inline_constraint.max_inline_size = LayoutUnit(100);
  TableTypes::ColspanCells colspan_cells;
  colspan_cells.push_back(colspan_cell);
  // Distribute over non-percent columns proportial to max size.
  // Columns min/max: 0/10, 0/10, 0/20
  // Distribute 25, 25, 50
  column_constraints->data.Shrink(0);
  column_constraints->data.push_back(MakeColumn(0, 10));
  column_constraints->data.push_back(MakeColumn(0, 10));
  column_constraints->data.push_back(MakeColumn(0, 20));
  DistributeColspanCellsToColumns(colspan_cells, LayoutUnit(), false,
                                  column_constraints.get());
  EXPECT_EQ(column_constraints->data[0].min_inline_size, 25);
  EXPECT_EQ(column_constraints->data[1].min_inline_size, 25);
  EXPECT_EQ(column_constraints->data[2].min_inline_size, 50);
}

TEST_F(TableLayoutUtilsTest, DistributeColspanAutoSizeConstrained) {
  TableTypes::ColspanCell colspan_cell(TableTypes::CellInlineConstraint(), 0,
                                       3);
  colspan_cell.start_column = 0;
  colspan_cell.span = 3;

  scoped_refptr<TableTypes::Columns> column_constraints =
      base::MakeRefCounted<TableTypes::Columns>();

  // Columns distributing over auto columns.
  colspan_cell.cell_inline_constraint.min_inline_size = LayoutUnit(100);
  colspan_cell.cell_inline_constraint.max_inline_size = LayoutUnit(100);
  TableTypes::ColspanCells colspan_cells;
  colspan_cells.push_back(colspan_cell);
  // Distribute over fixed columns proportial to:
  // Columns min/max: 0/10, 0/10, 0/20
  // Distribute 25, 25, 50
  column_constraints->data.Shrink(0);
  column_constraints->data.push_back(MakeColumn(0, 10, std::nullopt, true));
  column_constraints->data.push_back(MakeColumn(10, 10, std::nullopt, true));
  column_constraints->data.push_back(MakeColumn(0, 20, std::nullopt, true));
  DistributeColspanCellsToColumns(colspan_cells, LayoutUnit(), false,
                                  column_constraints.get());
  EXPECT_EQ(column_constraints->data[0].min_inline_size, 25);
  EXPECT_EQ(column_constraints->data[1].min_inline_size, 25);
  EXPECT_EQ(column_constraints->data[2].min_inline_size, 50);
}

TEST_F(TableLayoutUtilsTest, DistributeColspanAutoExactMaxSize) {
  // If column widths sum match table widths exactly, column widths
  // should not be redistributed at all.
  // The error occurs if widths are redistributed, and column widths
  // change due to floating point rounding.
  LayoutUnit column_widths[] = {LayoutUnit(0.1), LayoutUnit(22.123456),
                                LayoutUnit(33.789012), LayoutUnit(2000.345678)};
  scoped_refptr<TableTypes::Columns> column_constraints =
      base::MakeRefCounted<TableTypes::Columns>();
  column_constraints->data.Shrink(0);
  column_constraints->data.push_back(
      TableTypes::Column{LayoutUnit(0), column_widths[0], std::nullopt,
                         LayoutUnit(), false, false, false, false});
  column_constraints->data.push_back(
      TableTypes::Column{LayoutUnit(3.33333), column_widths[1], std::nullopt,
                         LayoutUnit(), false, false, false, false});
  column_constraints->data.push_back(
      TableTypes::Column{LayoutUnit(3.33333), column_widths[2], std::nullopt,
                         LayoutUnit(), false, false, false, false});
  column_constraints->data.push_back(
      TableTypes::Column{LayoutUnit(0), column_widths[3], std::nullopt,
                         LayoutUnit(), false, false, false, false});

  LayoutUnit assignable_table_inline_size =
      column_widths[0] + column_widths[1] + column_widths[2] + column_widths[3];
  Vector<LayoutUnit> column_sizes =
      SynchronizeAssignableTableInlineSizeAndColumns(
          assignable_table_inline_size, false, *column_constraints);
  EXPECT_EQ(column_sizes[0], column_widths[0]);
  EXPECT_EQ(column_sizes[1], column_widths[1]);
  EXPECT_EQ(column_sizes[2], column_widths[2]);
  EXPECT_EQ(column_sizes[3], column_widths[3]);
}

TEST_F(TableLayoutUtilsTest, ComputeGridInlineMinMax) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex;">
      <table id=target></table>
    <div>
  )HTML");
  TableNode node(To<LayoutBox>(GetLayoutObjectByElementId("target")));

  scoped_refptr<TableTypes::Columns> column_constraints =
      base::MakeRefCounted<TableTypes::Columns>();

  LayoutUnit undistributable_space;
  bool is_fixed_layout = false;
  bool is_layout_pass = true;

  // No percentages, just sums up min/max.
  column_constraints->data.push_back(MakeColumn(10, 100));
  column_constraints->data.push_back(MakeColumn(20, 200));
  column_constraints->data.push_back(MakeColumn(30, 300));

  MinMaxSizes minmax =
      ComputeGridInlineMinMax(node, *column_constraints, undistributable_space,
                              is_fixed_layout, is_layout_pass);
  EXPECT_EQ(minmax.min_size, LayoutUnit(60));
  EXPECT_EQ(minmax.max_size, LayoutUnit(600));

  // Percentage: 99px max size/10% cell =>
  // table max size of 100%/10% * 99px
  column_constraints->data.Shrink(0);
  column_constraints->data.push_back(MakeColumn(10, 99, 10));
  column_constraints->data.push_back(MakeColumn(10, 10));
  column_constraints->data.push_back(MakeColumn(10, 10));
  minmax =
      ComputeGridInlineMinMax(node, *column_constraints, undistributable_space,
                              is_fixed_layout, is_layout_pass);
  EXPECT_EQ(minmax.min_size, LayoutUnit(30));
  EXPECT_EQ(minmax.max_size, LayoutUnit(990));

  is_layout_pass = false;
  minmax =
      ComputeGridInlineMinMax(node, *column_constraints, undistributable_space,
                              is_fixed_layout, is_layout_pass);
  EXPECT_EQ(minmax.min_size, LayoutUnit(30));
  EXPECT_EQ(minmax.max_size, LayoutUnit(119));

  // Percentage: total percentage of 20%, and non-percent width of 800 =>
  // table max size of 800 + (20% * 800/80%) = 1000
  is_layout_pass = true;
  column_constraints->data.Shrink(0);
  column_constraints->data.push_back(MakeColumn(10, 100, 10));
  column_constraints->data.push_back(MakeColumn(10, 10, 10));
  column_constraints->data.push_back(MakeColumn(10, 800));
  minmax =
      ComputeGridInlineMinMax(node, *column_constraints, undistributable_space,
                              is_fixed_layout, is_layout_pass);
  EXPECT_EQ(minmax.min_size, LayoutUnit(30));
  EXPECT_EQ(minmax.max_size, LayoutUnit(1000));
}

TEST_F(TableLayoutUtilsTest, DistributeRowspanCellToRows) {
  TableTypes::RowspanCell rowspan_cell = {0, 3, LayoutUnit(300)};
  TableTypes::Rows rows;

  // Distribute to regular rows, rows grow in proportion to size.
  rows.push_back(MakeRow(10));
  rows.push_back(MakeRow(20));
  rows.push_back(MakeRow(30));
  DistributeRowspanCellToRows(rowspan_cell, LayoutUnit(), &rows);
  EXPECT_EQ(rows[0].block_size, LayoutUnit(50));
  EXPECT_EQ(rows[1].block_size, LayoutUnit(100));
  EXPECT_EQ(rows[2].block_size, LayoutUnit(150));

  // If some rows are empty, non-empty row gets everything
  rows.Shrink(0);
  rows.push_back(MakeRow(0));
  rows.push_back(MakeRow(10));
  rows.push_back(MakeRow(0));
  DistributeRowspanCellToRows(rowspan_cell, LayoutUnit(), &rows);
  EXPECT_EQ(rows[0].block_size, LayoutUnit(0));
  EXPECT_EQ(rows[1].block_size, LayoutUnit(300));
  EXPECT_EQ(rows[2].block_size, LayoutUnit(0));

  // If all rows are empty,last row gets everything.
  rows.Shrink(0);
  rows.push_back(MakeRow(0));
  rows.push_back(MakeRow(0));
  rows.push_back(MakeRow(0));
  DistributeRowspanCellToRows(rowspan_cell, LayoutUnit(), &rows);
  EXPECT_EQ(rows[0].block_size, LayoutUnit(0));
  EXPECT_EQ(rows[1].block_size, LayoutUnit(0));
  EXPECT_EQ(rows[2].block_size, LayoutUnit(300));
}

TEST_F(TableLayoutUtilsTest, DistributeSectionFixedBlockSizeToRows) {
  TableTypes::Rows rows;

  // Percentage rows get percentage, rest is distributed evenly.
  rows.push_back(MakeRow(100));
  rows.push_back(MakeRow(100, true, false, 50));
  rows.push_back(MakeRow(100));
  DistributeSectionFixedBlockSizeToRows(0, 3, LayoutUnit(1000), LayoutUnit(),
                                        LayoutUnit(1000), &rows);
  EXPECT_EQ(rows[0].block_size, LayoutUnit(250));
  EXPECT_EQ(rows[1].block_size, LayoutUnit(500));
  EXPECT_EQ(rows[2].block_size, LayoutUnit(250));
}

TEST_F(TableLayoutUtilsTest, DistributeTableBlockSizeToSections) {
  TableTypes::Sections sections;
  TableTypes::Rows rows;

  // Empty sections only grow if there are no other growable sections.
  sections.push_back(MakeSection(&rows, 0));
  sections.push_back(MakeSection(&rows, 100));
  DistributeTableBlockSizeToSections(LayoutUnit(), LayoutUnit(500), &sections,
                                     &rows);
  EXPECT_EQ(sections[0].block_size, LayoutUnit(400));

  // Sections with % block size grow to percentage.
  sections.Shrink(0);
  rows.Shrink(0);
  sections.push_back(MakeSection(&rows, 100, 1, 90));
  sections.push_back(MakeSection(&rows, 100));
  DistributeTableBlockSizeToSections(LayoutUnit(), LayoutUnit(1000), &sections,
                                     &rows);
  EXPECT_EQ(sections[0].block_size, LayoutUnit(900));
  EXPECT_EQ(sections[1].block_size, LayoutUnit(100));

  // When table height is greater than sum of intrinsic heights,
  // intrinsic heights are computed, and then they grow in
  // proportion to intrinsic height.
  sections.Shrink(0);
  rows.Shrink(0);
  sections.push_back(MakeSection(&rows, 100, 1, 30));
  sections.push_back(MakeSection(&rows, 100));
  // 30% section grows to 300px.
  // Extra 600 px is distributed between 300 and 100 px proportionally.
  // TODO(atotic) Is this what we want? FF/Edge/Legacy all disagree.
  DistributeTableBlockSizeToSections(LayoutUnit(), LayoutUnit(1000), &sections,
                                     &rows);
  EXPECT_EQ(sections[0].block_size, LayoutUnit(300));
  EXPECT_EQ(sections[1].block_size, LayoutUnit(700));

  // If there is a constrained section, and an unconstrained section,
  // unconstrained section grows.
  sections.Shrink(0);
  rows.Shrink(0);
  TableTypes::Section section(MakeSection(&rows, 100));
  section.is_constrained = false;
  sections.push_back(section);
  sections.push_back(MakeSection(&rows, 100));
  DistributeTableBlockSizeToSections(LayoutUnit(), LayoutUnit(1000), &sections,
                                     &rows);
  EXPECT_EQ(sections[0].block_size, LayoutUnit(900));
  EXPECT_EQ(sections[1].block_size, LayoutUnit(100));
}

}  // namespace blink
