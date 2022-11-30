// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_LAYOUT_ALGORITHM_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_LAYOUT_ALGORITHM_HELPERS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_types.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

class NGTableNode;

// Table size distribution algorithms.
class CORE_EXPORT NGTableAlgorithmHelpers {
 public:
  // Compute maximum number of table columns that can deduced from single cell
  // and its colspan.
  static wtf_size_t ComputeMaxColumn(wtf_size_t current_column,
                                     wtf_size_t colspan,
                                     bool is_fixed_table_layout) {
    // In fixed mode, every column is preserved.
    if (is_fixed_table_layout)
      return current_column + colspan;
    return current_column + 1;
  }

  // |undistributable_space| is size of space not occupied by cells
  // (borders, border spacing).
  static MinMaxSizes ComputeGridInlineMinMax(
      const NGTableNode& node,
      const NGTableTypes::Columns& column_constraints,
      LayoutUnit undistributable_space,
      bool is_fixed_layout,
      bool is_layout_pass);

  static void DistributeColspanCellsToColumns(
      const NGTableTypes::ColspanCells& colspan_cells,
      LayoutUnit inline_border_spacing,
      bool is_fixed_layout,
      NGTableTypes::Columns* column_constraints);

  static Vector<LayoutUnit> SynchronizeAssignableTableInlineSizeAndColumns(
      LayoutUnit assignable_table_inline_size,
      bool is_fixed_layout,
      const NGTableTypes::Columns& column_constraints);

  static void DistributeRowspanCellToRows(
      const NGTableTypes::RowspanCell& rowspan_cell,
      LayoutUnit border_block_spacing,
      NGTableTypes::Rows* rows);

  static void DistributeSectionFixedBlockSizeToRows(
      const wtf_size_t start_row,
      const wtf_size_t end_row,
      LayoutUnit section_fixed_block_size,
      LayoutUnit border_block_spacing,
      LayoutUnit percentage_resolution_block_size,
      NGTableTypes::Rows* rows);

  static void DistributeTableBlockSizeToSections(
      LayoutUnit border_block_spacing,
      LayoutUnit table_block_size,
      NGTableTypes::Sections* sections,
      NGTableTypes::Rows* rows);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_LAYOUT_ALGORITHM_HELPERS_H_
