// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_LAYOUT_ALGORITHM_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_LAYOUT_ALGORITHM_UTILS_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_types.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

class NGBlockNode;
class NGBoxFragment;
class NGBoxFragmentBuilder;
class NGConstraintSpaceBuilder;
class NGTableBorders;
enum class NGCacheSlot;

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

  // Computes a cell's block-size, and if its initial block-size should be
  // considered indefinite.
  struct CellBlockSizeData {
    LayoutUnit block_size;
    bool is_initial_block_size_indefinite;
  };
  static CellBlockSizeData ComputeCellBlockSize(
      const NGTableTypes::CellBlockConstraint& cell_block_constraint,
      const NGTableTypes::Rows& rows,
      wtf_size_t row_index,
      const LogicalSize& border_spacing,
      bool is_table_block_size_specified);

  // Sets up a constraint space builder for a table-cell.
  //
  // In order to make the cache as effective as possible, we try and keep
  // creating the constraint-space for table-cells as consistent as possible.
  static void SetupTableCellConstraintSpaceBuilder(
      const WritingDirectionMode table_writing_direction,
      const NGBlockNode cell,
      const NGBoxStrut& cell_borders,
      const Vector<NGTableColumnLocation>& column_locations,
      LayoutUnit cell_block_size,
      LayoutUnit percentage_inline_size,
      absl::optional<LayoutUnit> alignment_baseline,
      wtf_size_t start_column,
      bool is_initial_block_size_indefinite,
      bool is_restricted_block_size_table,
      bool has_collapsed_borders,
      NGCacheSlot,
      NGConstraintSpaceBuilder*);

  static wtf_size_t ComputeMaximumNonMergeableColumnCount(
      const HeapVector<NGBlockNode>& columns,
      bool is_fixed_layout);

  static scoped_refptr<NGTableTypes::Columns> ComputeColumnConstraints(
      const NGBlockNode& table,
      const NGTableGroupedChildren&,
      const NGTableBorders& table_borders,
      const NGBoxStrut& border_padding);

  static void ComputeSectionMinimumRowBlockSizes(
      const NGBlockNode& section,
      const LayoutUnit cell_percentage_resolution_inline_size,
      const bool is_table_block_size_specified,
      const Vector<NGTableColumnLocation>& column_locations,
      const NGTableBorders& table_borders,
      const LayoutUnit block_border_spacing,
      wtf_size_t section_index,
      bool treat_section_as_tbody,
      NGTableTypes::Sections* sections,
      NGTableTypes::Rows* rows,
      NGTableTypes::CellBlockConstraints* cell_block_constraints);

  // Performs any final adjustments for table-cells at the end of layout.
  static void FinalizeTableCellLayout(
      LayoutUnit unconstrained_intrinsic_block_size,
      NGBoxFragmentBuilder*);
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

// NGRowBaselineTabulator computes baseline information for row.
// Standard: https://www.w3.org/TR/css-tables-3/#row-layout
// Baseline is either max-baseline of baseline-aligned cells,
// or bottom content edge of non-baseline-aligned cells.
class NGRowBaselineTabulator {
 public:
  void ProcessCell(const NGBoxFragment& fragment,
                   bool is_baseline_aligned,
                   bool is_rowspanned,
                   bool descendant_depends_on_percentage_block_size);

  LayoutUnit ComputeRowBlockSize(const LayoutUnit max_cell_block_size);

  LayoutUnit ComputeBaseline(const LayoutUnit row_block_size);

  bool BaselineDependsOnPercentageBlockDescendant();

 private:
  // Cell baseline is computed from baseline-aligned cells.
  absl::optional<LayoutUnit> max_cell_ascent_;
  absl::optional<LayoutUnit> max_cell_descent_;
  bool max_cell_baseline_depends_on_percentage_block_descendant_ = false;

  // Non-baseline aligned cells are used to compute baseline if baseline
  // cells are not available.
  absl::optional<LayoutUnit> fallback_cell_descent_;
  bool fallback_cell_depends_on_percentage_block_descendant_ = false;
};

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::NGColspanCellTabulator::Cell)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_LAYOUT_ALGORITHM_UTILS_H_
