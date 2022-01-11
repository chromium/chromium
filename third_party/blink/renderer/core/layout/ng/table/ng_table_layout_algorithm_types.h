// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_LAYOUT_ALGORITHM_TYPES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_LAYOUT_ALGORITHM_TYPES_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
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
  static constexpr LayoutUnit kTableMaxInlineSize =
      LayoutUnit(static_cast<uint64_t>(1000000));

  // Inline constraint for a single cell.
  // Takes into account the cell style, and min/max content-sizes.
  struct CellInlineConstraint {
    DISALLOW_NEW();
    LayoutUnit min_inline_size;
    LayoutUnit max_inline_size;
    absl::optional<float> percent;      // 100% is stored as 100.0f
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
    Column(const absl::optional<LayoutUnit>& min_inline_size,
           const absl::optional<LayoutUnit>& max_inline_size,
           const absl::optional<float>& percent,
           LayoutUnit percent_border_padding,
           bool is_constrained,
           bool is_collapsed,
           bool is_table_fixed,
           bool is_mergeable)
        : min_inline_size(min_inline_size),
          max_inline_size(max_inline_size),
          percent(percent),
          percent_border_padding(percent_border_padding),
          is_constrained(is_constrained),
          is_collapsed(is_collapsed),
          is_table_fixed(is_table_fixed),
          is_mergeable(is_mergeable) {}
    Column() = default;

    bool operator==(const Column& other) const {
      return min_inline_size == other.min_inline_size &&
             max_inline_size == other.max_inline_size &&
             percent == other.percent &&
             percent_border_padding == other.percent_border_padding &&
             is_constrained == other.is_constrained &&
             is_collapsed == other.is_collapsed &&
             is_table_fixed == other.is_table_fixed &&
             is_mergeable == other.is_mergeable;
    }
    bool operator!=(const Column& other) const { return !(*this == other); }

    // These members are initialized from <col> and <colgroup>, then they
    // accumulate data from |CellInlineConstraint|s.
    absl::optional<LayoutUnit> min_inline_size;
    absl::optional<LayoutUnit> max_inline_size;
    absl::optional<float> percent;      // 100% is stored as 100.0f
    LayoutUnit percent_border_padding;  // Border/padding used for percentage
                                        // size resolution.
    // True if any cell for this column is constrained.
    bool is_constrained = false;
    bool is_collapsed = false;
    bool is_table_fixed = false;
    bool is_mergeable = false;

    void Encompass(const absl::optional<NGTableTypes::CellInlineConstraint>&);
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
    NGBoxStrut borders;
    wtf_size_t column_index;
    wtf_size_t rowspan;
    bool is_constrained;  // True if this cell has a specified block-size.
    CellBlockConstraint(LayoutUnit min_block_size,
                        NGBoxStrut borders,
                        wtf_size_t column_index,
                        wtf_size_t rowspan,
                        bool is_constrained)
        : min_block_size(min_block_size),
          borders(borders),
          column_index(column_index),
          rowspan(rowspan),
          is_constrained(is_constrained) {}
  };

  // RowspanCells span multiple rows.
  struct RowspanCell {
    DISALLOW_NEW();
    wtf_size_t start_row;
    wtf_size_t span;
    LayoutUnit min_block_size;
    RowspanCell(wtf_size_t start_row,
                wtf_size_t span,
                LayoutUnit min_block_size)
        : start_row(start_row), span(span), min_block_size(min_block_size) {}

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

      // If cells span the same rows, the bigger cell is distributed first.
      if (start_row == rhs.start_row && span == rhs.span)
        return min_block_size > rhs.min_block_size;

      // If one cell is fully enclosed by another, the inner cell wins.
      if (IsEnclosed(*this, rhs))
        return true;
      if (IsEnclosed(rhs, *this))
        return false;
      // Lowest row wins.
      return start_row < rhs.start_row;
    }
  };

  struct Row {
    DISALLOW_NEW();
    LayoutUnit block_size;
    LayoutUnit baseline;
    absl::optional<float> percent;  // 100% is stored as 100.0f
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
    wtf_size_t row_count;
    LayoutUnit block_size;
    absl::optional<float> percent;
    bool is_constrained;
    bool is_tbody;
    bool needs_redistribution;
  };

  static Column CreateColumn(const ComputedStyle&,
                             absl::optional<LayoutUnit> default_inline_size,
                             bool is_table_fixed);

  static CellInlineConstraint CreateCellInlineConstraint(
      const NGBlockNode&,
      WritingMode table_writing_mode,
      bool is_fixed_layout,
      const NGBoxStrut& cell_border,
      const NGBoxStrut& cell_padding);

  static Section CreateSection(const NGLayoutInputNode&,
                               wtf_size_t start_row,
                               wtf_size_t row_count,
                               LayoutUnit block_size,
                               bool treat_as_tbody);

  // Columns are cached by LayoutNGTable, and need to be RefCounted.
  typedef base::RefCountedData<WTF::Vector<Column>> Columns;
  // Inline constraints are optional because we need to distinguish between an
  // empty cell, and a non-existent cell.
  using CellInlineConstraints = Vector<absl::optional<CellInlineConstraint>>;
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

  NGBlockNode header;          // first THEAD
  Vector<NGBlockNode> bodies;  // TBODY/multiple THEAD/TFOOT
  NGBlockNode footer;          // first TFOOT

  // Default iterators iterate over tbody-like (THEAD/TBODY/TFOOT) elements.
  NGTableGroupedChildrenIterator begin() const;
  NGTableGroupedChildrenIterator end() const;
};

// Iterates table's sections in order:
// thead, tbody, tfoot
class NGTableGroupedChildrenIterator {
  STACK_ALLOCATED();
  enum CurrentSection { kNone, kHead, kBody, kFoot, kEnd };

 public:
  explicit NGTableGroupedChildrenIterator(
      const NGTableGroupedChildren& grouped_children,
      bool is_end = false);

  NGTableGroupedChildrenIterator& operator++();
  NGBlockNode operator*() const;
  bool operator==(const NGTableGroupedChildrenIterator& rhs) const;
  bool operator!=(const NGTableGroupedChildrenIterator& rhs) const;
  // True if section should be treated as tbody
  bool TreatAsTBody() const { return current_section_ == kBody; }

 private:
  void AdvanceToNonEmptySection();
  const NGTableGroupedChildren& grouped_children_;
  Vector<NGBlockNode>::const_iterator body_iterator_;
  CurrentSection current_section_{kNone};
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
