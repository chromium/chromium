// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_LAYOUT_ALGORITHM_TYPES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_LAYOUT_ALGORITHM_TYPES_H_

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ComputedStyle;
class NGBlockNode;
class NGLayoutInputNode;

// Define constraint classes for NGTableLayoutAlgorithm.
class CORE_EXPORT NGTableTypes {
 public:
  static constexpr LayoutUnit kTableMaxInlineSize = LayoutUnit::Max();

  // Inline constraint for a single cell.
  // Takes into account the cell style, and min/max content-sizes.
  struct CellInlineConstraint {
    DISALLOW_NEW();
    LayoutUnit min_inline_size;
    LayoutUnit max_inline_size;
    base::Optional<float> percent;  // 100% is stored as 100.0f
    LayoutUnit percent_border_padding;  // Border/padding used for percentage
                                        // size resolution.
    bool is_constrained;  // True if this cell has a specified inline-size.

    void Encompass(const CellInlineConstraint&);
  };

  // Inline constraints for a cell that span multiple columns.
  struct ColspanCell {
    DISALLOW_NEW();
    CellInlineConstraint cell_inline_constraint;
    wtf_size_t start_column;
    wtf_size_t span;
    ColspanCell(const CellInlineConstraint& cell_inline_constraint,
                unsigned start_column,
                unsigned span)
        : cell_inline_constraint(cell_inline_constraint),
          start_column(start_column),
          span(span) {}
  };

  // Constraint for a column.
  struct Column {
    DISALLOW_NEW();
    // These members are initialized from <col> and <colgroup>, then they
    // accumulate data from |CellInlineConstraint|s.
    base::Optional<LayoutUnit> min_inline_size;
    base::Optional<LayoutUnit> max_inline_size;
    base::Optional<float> percent;  // 100% is stored as 100.0f
    LayoutUnit percent_border_padding;  // Border/padding used for percentage
                                        // size resolution.
    // True if any cell for this column is constrained.
    bool is_constrained = false;
    bool is_collapsed = false;

    void Encompass(const base::Optional<NGTableTypes::CellInlineConstraint>&);
    LayoutUnit ResolvePercentInlineSize(
        LayoutUnit percentage_resolution_inline_size) const {
      return std::max(
          min_inline_size.value_or(LayoutUnit()),
          LayoutUnit(*percent * percentage_resolution_inline_size / 100) +
              percent_border_padding);
    }
    bool IsFixed() const {
      return is_constrained && !percent && max_inline_size;
    }
  };

  // Block constraint for a single cell.
  struct CellBlockConstraint {
    DISALLOW_NEW();
    LayoutUnit min_block_size;
    LayoutUnit baseline;
    NGBoxStrut border_box_borders;
    wtf_size_t row_index;
    wtf_size_t column_index;
    wtf_size_t rowspan;
    EVerticalAlign vertical_align;
    bool is_constrained;  // True if this cell has a specified block-size.
    CellBlockConstraint(LayoutUnit min_block_size,
                        LayoutUnit baseline,
                        NGBoxStrut border_box_borders,
                        wtf_size_t row_index,
                        wtf_size_t column_index,
                        wtf_size_t rowspan,
                        EVerticalAlign vertical_align,
                        bool is_constrained)
        : min_block_size(min_block_size),
          baseline(baseline),
          border_box_borders(border_box_borders),
          row_index(row_index),
          column_index(column_index),
          rowspan(rowspan),
          vertical_align(vertical_align),
          is_constrained(is_constrained) {}
  };

  // RowspanCells span multiple rows.
  struct RowspanCell {
    DISALLOW_NEW();
    CellBlockConstraint cell_block_constraint;
    wtf_size_t start_row;
    wtf_size_t span;
    RowspanCell(wtf_size_t start_row,
                wtf_size_t span,
                const CellBlockConstraint& cell_block_constraint)
        : cell_block_constraint(cell_block_constraint),
          start_row(start_row),
          span(span) {}

    // Original Legacy sorting criteria from
    // CompareRowspanCellsInHeightDistributionOrder
    bool operator<(const NGTableTypes::RowspanCell& rhs) const {
      // Returns true if a |RowspanCell| is completely contained within another
      // |RowspanCell|.
      auto IsEnclosed = [](const NGTableTypes::RowspanCell& c1,
                           const NGTableTypes::RowspanCell& c2) {
        return (c1.start_row >= c2.start_row) &&
               (c1.start_row + c1.span) <= (c2.start_row + c2.span);
      };

      // If cells span the same rows, bigger cell is distributed first.
      if (start_row == rhs.start_row && span == rhs.span) {
        return cell_block_constraint.min_block_size >
               rhs.cell_block_constraint.min_block_size;
      }
      // If one cell is fully enclosed by another, inner cell wins.
      if (IsEnclosed(*this, rhs))
        return true;
      if (IsEnclosed(rhs, *this))
        return false;
      // Lower rows wins.
      return start_row < rhs.start_row;
    }
  };

  struct Row {
    DISALLOW_NEW();
    LayoutUnit block_size;
    LayoutUnit baseline;
    base::Optional<float> percent;  // 100% is stored as 100.0f
    wtf_size_t start_cell_index;
    wtf_size_t cell_count;
    // |is_constrained| is true if row has specified block-size, or contains
    // constrained cells.
    bool is_constrained;
    bool has_baseline_aligned_percentage_block_size_descendants;
    bool has_rowspan_start;  // True if row originates a TD with rowspan > 1
    bool is_collapsed;
  };

  struct ColumnLocation {
    LayoutUnit offset;  // inline offset from table edge.
    LayoutUnit size;
    bool is_collapsed;
  };

  struct Section {
    wtf_size_t start_row;
    wtf_size_t rowspan;
    LayoutUnit block_size;
    base::Optional<float> percent;
    bool is_constrained;
    bool is_tbody;
    bool needs_redistribution;
  };

  static Column CreateColumn(const ComputedStyle&,
                             base::Optional<LayoutUnit> default_inline_size);

  static CellInlineConstraint CreateCellInlineConstraint(
      const NGBlockNode&,
      WritingMode table_writing_mode,
      bool is_fixed_layout,
      const NGBoxStrut& cell_border,
      const NGBoxStrut& cell_padding,
      bool has_collapsed_borders);

  static Section CreateSection(const NGLayoutInputNode&,
                               wtf_size_t start_row,
                               wtf_size_t rowspan,
                               LayoutUnit block_size);

  static CellBlockConstraint CreateCellBlockConstraint(
      const NGLayoutInputNode&,
      LayoutUnit computed_block_size,
      LayoutUnit baseline,
      const NGBoxStrut& border_box_borders,
      wtf_size_t row_index,
      wtf_size_t column_index,
      wtf_size_t rowspan);

  static RowspanCell CreateRowspanCell(
      wtf_size_t row_index,
      wtf_size_t rowspan,
      CellBlockConstraint*,
      base::Optional<LayoutUnit> css_block_size);

  // Columns are cached by LayoutNGTable, and need to be RefCounted.
  typedef base::RefCountedData<WTF::Vector<Column>> Columns;
  // Inline constraints are optional because we need to distinguish between an
  // empty cell, and a non-existent cell.
  using CellInlineConstraints = Vector<base::Optional<CellInlineConstraint>>;
  using ColspanCells = Vector<ColspanCell>;
  using Caption = MinMaxSizes;
  using CellBlockConstraints = Vector<CellBlockConstraint>;
  using RowspanCells = Vector<RowspanCell>;
  using Rows = Vector<Row>;
  using Sections = Vector<Section>;
  using ColumnLocations = Vector<ColumnLocation>;
};

class NGTableGroupedChildrenIterator;

// Table's children grouped by type.
// When iterating through members, make sure to handle out_of_flows correctly.
struct NGTableGroupedChildren {
  DISALLOW_NEW();

 public:
  explicit NGTableGroupedChildren(const NGBlockNode& table);

  Vector<NGBlockNode> captions;  // CAPTION
  Vector<NGBlockNode> columns;   // COLGROUP, COL

  Vector<NGBlockNode> headers;  // THEAD
  Vector<NGBlockNode> bodies;   // TBODY
  Vector<NGBlockNode> footers;  // TFOOT

  // Default iterators iterate over tbody-like (THEAD/TBODY/TFOOT) elements.
  NGTableGroupedChildrenIterator begin() const;
  NGTableGroupedChildrenIterator end() const;
};

// Iterates table's sections in order:
// thead, tbody, tfoot
class NGTableGroupedChildrenIterator {
 public:
  explicit NGTableGroupedChildrenIterator(
      const NGTableGroupedChildren& grouped_children,
      bool is_end = false);

  NGTableGroupedChildrenIterator& operator++();
  NGBlockNode operator*() const;
  bool operator==(const NGTableGroupedChildrenIterator& rhs) const;
  bool operator!=(const NGTableGroupedChildrenIterator& rhs) const;

 private:
  void AdvanceToNonEmptySection();
  const NGTableGroupedChildren& grouped_children_;
  const Vector<NGBlockNode>* current_vector_;
  Vector<NGBlockNode>::const_iterator current_iterator_;
};

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::NGTableTypes::CellInlineConstraint)
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::NGTableTypes::ColspanCell)
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::NGTableTypes::Column)
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::NGTableTypes::CellBlockConstraint)
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::NGTableTypes::RowspanCell)
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::NGTableTypes::Row)
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::NGTableTypes::ColumnLocation)
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::NGTableTypes::Section)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_LAYOUT_ALGORITHM_TYPES_H_
