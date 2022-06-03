// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_CONSTRAINT_SPACE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_CONSTRAINT_SPACE_DATA_H_

#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// Same NGTableConstraintSpaceData object gets passed on ConstraintSpace to
// all rows and sections in a single table. It contains all the geometry
// information needed to layout sections/rows/cells. This is different from most
// other algorithms, where constraint space data is not shared.
class NGTableConstraintSpaceData
    : public RefCounted<NGTableConstraintSpaceData> {
 public:
  // Table grid columns are used to compute cell geometry.
  struct ColumnLocation {
    ColumnLocation(LayoutUnit offset, LayoutUnit inline_size, bool is_collapsed)
        : offset(offset),
          inline_size(inline_size),
          is_collapsed(is_collapsed) {}
    const LayoutUnit offset;
    const LayoutUnit inline_size;
    const bool is_collapsed;
    bool operator==(const ColumnLocation& other) const {
      return offset == other.offset && inline_size == other.inline_size &&
             is_collapsed == other.is_collapsed;
    }
  };

  // Section hold row index information used to map between table and
  // section row indexes.
  struct Section {
    Section(wtf_size_t start_row_index, wtf_size_t rowspan)
        : start_row_index(start_row_index), rowspan(rowspan) {}

    bool MaySkipLayout(const Section& other) const {
      // We don't compare |start_row_index| as this is allowed to change.
      return rowspan == other.rowspan;
    }

    wtf_size_t start_row_index;  // First section row in table grid.
    wtf_size_t rowspan;
  };

  // Data needed by row layout algorithm.
  struct Row {
    Row(LayoutUnit baseline,
        LayoutUnit block_size,
        wtf_size_t start_cell_index,
        wtf_size_t cell_count,
        bool has_baseline_aligned_percentage_block_size_descendants,
        bool is_collapsed)
        : baseline(baseline),
          block_size(block_size),
          start_cell_index(start_cell_index),
          cell_count(cell_count),
          has_baseline_aligned_percentage_block_size_descendants(
              has_baseline_aligned_percentage_block_size_descendants),
          is_collapsed(is_collapsed) {}

    bool MaySkipLayout(const Row& other) const {
      // We don't compare |start_cell_index| as this is allowed to change.
      return baseline == other.baseline && block_size == other.block_size &&
             cell_count == other.cell_count &&
             has_baseline_aligned_percentage_block_size_descendants ==
                 other.has_baseline_aligned_percentage_block_size_descendants &&
             is_collapsed == other.is_collapsed;
    }

    LayoutUnit baseline;
    LayoutUnit block_size;
    wtf_size_t start_cell_index;
    wtf_size_t cell_count;
    bool has_baseline_aligned_percentage_block_size_descendants;
    bool is_collapsed;
  };

  // Data needed to layout a single cell.
  struct Cell {
    Cell(NGBoxStrut border_box_borders,
         LayoutUnit block_size,
         wtf_size_t start_column,
         bool has_grown,
         bool is_constrained)
        : border_box_borders(border_box_borders),
          block_size(block_size),
          start_column(start_column),
          has_grown(has_grown),
          is_constrained(is_constrained) {}
    bool operator==(const Cell& other) const {
      return border_box_borders == other.border_box_borders &&
             block_size == other.block_size &&
             start_column == other.start_column &&
             has_grown == other.has_grown &&
             is_constrained == other.is_constrained;
    }
    bool operator!=(const Cell& other) const { return !(*this == other); }
    // Size of borders drawn on the inside of the border box.
    NGBoxStrut border_box_borders;
    // Size of the cell. Need this for cells that span multiple rows.
    LayoutUnit block_size;
    wtf_size_t start_column;
    bool has_grown;
    bool is_constrained;
  };

  bool IsTableSpecificDataEqual(const NGTableConstraintSpaceData& other) const {
    return table_inline_size == other.table_inline_size &&
           table_writing_direction == other.table_writing_direction &&
           table_border_spacing == other.table_border_spacing &&
           is_table_block_size_specified ==
               other.is_table_block_size_specified &&
           hide_table_cell_if_empty == other.hide_table_cell_if_empty &&
           column_locations == other.column_locations;
  }

  bool MaySkipRowLayout(const NGTableConstraintSpaceData& other,
                        wtf_size_t new_row_index,
                        wtf_size_t old_row_index) const {
    DCHECK_LT(new_row_index, rows.size());
    DCHECK_LT(old_row_index, other.rows.size());

    const Row& new_row = rows[new_row_index];
    const Row& old_row = other.rows[old_row_index];
    if (!new_row.MaySkipLayout(old_row))
      return false;

    DCHECK_EQ(new_row.cell_count, old_row.cell_count);

    const wtf_size_t new_start_cell_index = new_row.start_cell_index;
    const wtf_size_t old_start_cell_index = old_row.start_cell_index;

    const wtf_size_t new_end_cell_index =
        new_start_cell_index + new_row.cell_count;
    const wtf_size_t old_end_cell_index =
        old_start_cell_index + old_row.cell_count;

    for (wtf_size_t new_cell_index = new_start_cell_index,
                    old_cell_index = old_start_cell_index;
         new_cell_index < new_end_cell_index &&
         old_cell_index < old_end_cell_index;
         ++new_cell_index, ++old_cell_index) {
      if (cells[new_cell_index] != other.cells[old_cell_index])
        return false;
    }

    return true;
  }

  bool MaySkipSectionLayout(const NGTableConstraintSpaceData& other,
                            wtf_size_t new_section_index,
                            wtf_size_t old_section_index) const {
    DCHECK_LE(new_section_index, sections.size());
    DCHECK_LE(old_section_index, other.sections.size());

    const Section& new_section = sections[new_section_index];
    const Section& old_section = other.sections[old_section_index];
    if (!new_section.MaySkipLayout(old_section))
      return false;

    DCHECK_EQ(new_section.rowspan, old_section.rowspan);

    const wtf_size_t new_start_row_index = new_section.start_row_index;
    const wtf_size_t old_start_row_index = old_section.start_row_index;

    const wtf_size_t new_end_row_index =
        new_start_row_index + new_section.rowspan;
    const wtf_size_t old_end_row_index =
        old_start_row_index + old_section.rowspan;

    for (wtf_size_t new_row_index = new_start_row_index,
                    old_row_index = old_start_row_index;
         new_row_index < new_end_row_index && old_row_index < old_end_row_index;
         ++new_row_index, ++old_row_index) {
      if (!MaySkipRowLayout(other, new_row_index, old_row_index))
        return false;
    }

    return true;
  }

  Vector<ColumnLocation> column_locations;
  Vector<Section> sections;
  Vector<Row> rows;
  Vector<Cell> cells;
  LayoutUnit table_inline_size;
  WritingDirectionMode table_writing_direction =
      WritingDirectionMode(WritingMode::kHorizontalTb, TextDirection::kLtr);
  LogicalSize table_border_spacing;

  // If the block-size of the table is specified (not 'auto').
  bool is_table_block_size_specified;
  bool hide_table_cell_if_empty;  // currently on regular constraint space.
  bool has_collapsed_borders;
};

}  // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(
    blink::NGTableConstraintSpaceData::ColumnLocation)
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::NGTableConstraintSpaceData::Section)
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(
    blink::NGTableConstraintSpaceData::Row)
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(
    blink::NGTableConstraintSpaceData::Cell)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_CONSTRAINT_SPACE_DATA_H_
