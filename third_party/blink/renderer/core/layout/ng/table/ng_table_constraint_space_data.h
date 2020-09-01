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
    bool operator==(const Section& other) const {
      return start_row_index == other.start_row_index &&
             rowspan == other.rowspan;
    }
    wtf_size_t start_row_index;  // first section row in table grid.
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
    bool operator==(const Row& other) const {
      return baseline == other.baseline && block_size == other.block_size &&
             start_cell_index == other.start_cell_index &&
             cell_count == other.cell_count &&
             has_baseline_aligned_percentage_block_size_descendants ==
                 other.has_baseline_aligned_percentage_block_size_descendants &&
             is_collapsed == other.is_collapsed;
    }
    bool operator!=(const Row& other) const { return !(*this == other); }
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
         bool is_constrained)
        : border_box_borders(border_box_borders),
          block_size(block_size),
          start_column(start_column),
          is_constrained(is_constrained) {}
    bool operator==(const Cell& other) const {
      return border_box_borders == other.border_box_borders &&
             block_size == other.block_size &&
             is_constrained == other.is_constrained;
    }
    bool operator!=(const Cell& other) const { return !(*this == other); }
    // Size of borders drawn on the inside of the border box.
    NGBoxStrut border_box_borders;
    // Size of the cell. Need this for cells that span multiple rows.
    LayoutUnit block_size;
    wtf_size_t start_column;
    bool is_constrained;
  };

  bool EqualTableSpecificData(const NGTableConstraintSpaceData* other) const {
    return table_inline_size == other->table_inline_size &&
           table_writing_direction == other->table_writing_direction &&
           table_border_spacing == other->table_border_spacing &&
           treat_table_block_size_as_constrained ==
               other->treat_table_block_size_as_constrained &&
           hide_table_cell_if_empty == other->hide_table_cell_if_empty &&
           column_locations == other->column_locations;
  }

  bool MaySkipRowLayout(const NGTableConstraintSpaceData* other,
                        wtf_size_t row_index) const {
    if (other->rows.size() <= row_index)
      return false;
    if (rows[row_index] != other->rows[row_index])
      return false;
    DCHECK_LT(row_index, rows.size());
    wtf_size_t end_index =
        rows[row_index].start_cell_index + rows[row_index].cell_count;
    for (wtf_size_t cell_index = rows[row_index].start_cell_index;
         cell_index < end_index; ++cell_index) {
      if (cells[cell_index] != other->cells[cell_index])
        return false;
    }
    return true;
  }

  bool MaySkipSectionLayout(const NGTableConstraintSpaceData* other,
                            wtf_size_t section_index) const {
    if (other->sections.size() <= section_index)
      return false;
    DCHECK_LT(section_index, sections.size());
    wtf_size_t end_index = sections[section_index].start_row_index +
                           sections[section_index].rowspan;
    for (wtf_size_t row_index = sections[section_index].start_row_index;
         row_index < end_index; ++row_index) {
      if (!MaySkipRowLayout(other, row_index))
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
  bool treat_table_block_size_as_constrained;
  bool hide_table_cell_if_empty;  // currently on regular constraint space.
  bool has_collapsed_borders;
};

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::NGTableConstraintSpaceData::ColumnLocation)
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::NGTableConstraintSpaceData::Section)
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::NGTableConstraintSpaceData::Row)
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::NGTableConstraintSpaceData::Cell)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_CONSTRAINT_SPACE_DATA_H_
