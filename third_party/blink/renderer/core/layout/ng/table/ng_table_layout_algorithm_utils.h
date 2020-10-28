// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_LAYOUT_ALGORITHM_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_LAYOUT_ALGORITHM_UTILS_H_

#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_types.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

struct LogicalSize;
enum class NGCacheSlot;
class NGConstraintSpace;
class NGTableBorders;
class NGBlockNode;

// Table size distribution algorithms.
class NGTableAlgorithmUtils {
 public:
  static bool IsBaseline(EVerticalAlign align) {
    return align == EVerticalAlign::kBaseline ||
           align == EVerticalAlign::kBaselineMiddle ||
           align == EVerticalAlign::kSub || align == EVerticalAlign::kSuper ||
           align == EVerticalAlign::kTextTop ||
           align == EVerticalAlign::kTextBottom ||
           align == EVerticalAlign::kLength;
  }

  // Creates a constraint-space for a table-cell.
  //
  // In order to make the cache as effective as possible, we try and keep
  // creating the constraint-space for table-cells as consistent as possible.
  static NGConstraintSpace CreateTableCellConstraintSpace(
      const WritingDirectionMode table_writing_direction,
      const NGBlockNode cell,
      const NGBoxStrut& cell_borders,
      LogicalSize cell_size,
      LayoutUnit percentage_inline_size,
      base::Optional<LayoutUnit> alignment_baseline,
      wtf_size_t column_index,
      bool is_fixed_block_size_indefinite,
      bool is_restricted_block_size_table,
      bool is_hidden_for_paint,
      bool has_collapsed_borders,
      NGCacheSlot);

  static scoped_refptr<NGTableTypes::Columns> ComputeColumnConstraints(
      const NGBlockNode& table,
      const NGTableGroupedChildren&,
      const NGTableBorders& table_borders,
      const NGBoxStrut& border_padding);

  static void ComputeSectionMinimumRowBlockSizes(
      const NGBlockNode& section,
      const LayoutUnit cell_percentage_resolution_inline_size,
      const bool is_restricted_block_size_table,
      const NGTableTypes::ColumnLocations& column_locations,
      const NGTableBorders& table_borders,
      const LayoutUnit block_border_spacing,
      wtf_size_t section_index,
      NGTableTypes::Sections* sections,
      NGTableTypes::Rows* rows,
      NGTableTypes::CellBlockConstraints* cell_block_constraints);
};

// NGColspanCellTabulator keeps track of columns occupied by colspanned cells
// when traversing rows in a section. It is used to compute cell's actual
// column.
// Usage:
//   NGColspanCellTabulator colspan_cell_tabulator;
//   for (Row r : section.rows) {
//      colspan_cell_tabulator.StartRow();
//      for (Cell c : row.cells) {
//        colspan_cell_tabulator.FindNextFreeColumn();
//        // colspan_cell_tabulator.CurrentColumn() has a valid value here.
//        colspan_cell_tabulator.ProcessCell();
//      }
//      colspan_cell_tabulator.EndRow();
//   }
class NGColspanCellTabulator {
 public:
  unsigned CurrentColumn() { return current_column_; }
  void StartRow();
  void FindNextFreeColumn();
  void ProcessCell(const NGBlockNode& cell);
  void EndRow();

  struct Cell {
    Cell(unsigned column_start, unsigned span, unsigned remaining_rows)
        : column_start(column_start),
          span(span),
          remaining_rows(remaining_rows) {}
    unsigned column_start;
    unsigned span;
    unsigned remaining_rows;
  };

 private:
  unsigned current_column_ = 0;
  Vector<Cell> colspanned_cells_;
};

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::NGColspanCellTabulator::Cell)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_LAYOUT_ALGORITHM_UTILS_H_
