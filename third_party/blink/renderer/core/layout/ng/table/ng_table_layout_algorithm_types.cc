// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_types.h"

#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_caption.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_column.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_section.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

// Gathers css sizes. CSS values might be modified to enforce universal
// invariants: css_max_inline_size >= css_min_inline_size
// css_percentage_inline_size <= css_percentage_max_inline_size
inline void InlineSizesFromStyle(
    const ComputedStyle& style,
    LayoutUnit inline_border_padding,
    bool is_parallel,
    base::Optional<LayoutUnit>* inline_size,
    base::Optional<LayoutUnit>* min_inline_size,
    base::Optional<LayoutUnit>* max_inline_size,
    base::Optional<float>* percentage_inline_size) {
  const Length& length =
      is_parallel ? style.LogicalWidth() : style.LogicalHeight();
  const Length& min_length =
      is_parallel ? style.LogicalMinWidth() : style.LogicalMinHeight();
  const Length& max_length =
      is_parallel ? style.LogicalMaxWidth() : style.LogicalMaxHeight();
  bool is_content_box = style.BoxSizing() == EBoxSizing::kContentBox;
  if (length.IsFixed()) {
    *inline_size = LayoutUnit(length.Value());
    if (is_content_box)
      *inline_size = **inline_size + inline_border_padding;
  }
  if (min_length.IsFixed()) {
    *min_inline_size = LayoutUnit(min_length.Value());
    if (is_content_box)
      *min_inline_size = **min_inline_size + inline_border_padding;
  }
  if (max_length.IsFixed()) {
    *max_inline_size = LayoutUnit(max_length.Value());
    if (is_content_box)
      *max_inline_size = **max_inline_size + inline_border_padding;
    if (*min_inline_size)
      *max_inline_size = std::max(**min_inline_size, **max_inline_size);
  }
  if (length.IsPercent())
    *percentage_inline_size = length.Percent();

  if (*percentage_inline_size && max_length.IsPercent()) {
    *percentage_inline_size =
        std::min(**percentage_inline_size, max_length.Percent());
  }
  if (*min_inline_size && *max_inline_size)
    DCHECK_GE(**max_inline_size, **min_inline_size);
}

}  // namespace

constexpr LayoutUnit NGTableTypes::kTableMaxInlineSize;

// Implements https://www.w3.org/TR/css-tables-3/#computing-cell-measures
// "outer min-content and outer max-content widths for colgroups"
NGTableTypes::Column NGTableTypes::CreateColumn(
    const ComputedStyle& style,
    base::Optional<LayoutUnit> default_inline_size) {
  base::Optional<LayoutUnit> inline_size;
  base::Optional<LayoutUnit> min_inline_size;
  base::Optional<LayoutUnit> max_inline_size;
  base::Optional<float> percentage_inline_size;
  InlineSizesFromStyle(style, /* inline_border_padding */ LayoutUnit(),
                       /* is_parallel */ true, &inline_size, &min_inline_size,
                       &max_inline_size, &percentage_inline_size);
  if (!inline_size)
    inline_size = default_inline_size;
  if (min_inline_size && inline_size)
    inline_size = std::max(*inline_size, *min_inline_size);
  bool is_constrained = inline_size.has_value();
  if (percentage_inline_size && *percentage_inline_size == 0.0f)
    percentage_inline_size.reset();
  bool is_collapsed = style.Visibility() == EVisibility::kCollapse;
  return Column{min_inline_size.value_or(LayoutUnit()),
                inline_size,
                percentage_inline_size,
                LayoutUnit() /* percent_border_padding */,
                is_constrained,
                is_collapsed,
                kIndefiniteSize};
}

// Implements https://www.w3.org/TR/css-tables-3/#computing-cell-measures
// "outer min-content and outer max-content widths for table cells"
// Note: this method calls NGBlockNode::ComputeMinMaxSizes.
NGTableTypes::CellInlineConstraint NGTableTypes::CreateCellInlineConstraint(
    const NGBlockNode& node,
    WritingMode table_writing_mode,
    bool is_fixed_layout,
    const NGBoxStrut& cell_border,
    const NGBoxStrut& cell_padding,
    bool is_collapsed) {
  base::Optional<LayoutUnit> css_inline_size;
  base::Optional<LayoutUnit> css_min_inline_size;
  base::Optional<LayoutUnit> css_max_inline_size;
  base::Optional<float> css_percentage_inline_size;
  bool is_parallel =
      IsParallelWritingMode(table_writing_mode, node.Style().GetWritingMode());

  // Algorithm:
  // - Compute cell's minmax sizes.
  // - Constrain by css inline-size/max-inline-size.
  InlineSizesFromStyle(node.Style(), (cell_border + cell_padding).InlineSum(),
                       is_parallel, &css_inline_size, &css_min_inline_size,
                       &css_max_inline_size, &css_percentage_inline_size);

  MinMaxSizesInput input(kIndefiniteSize, MinMaxSizesType::kIntrinsic);
  MinMaxSizesResult min_max_size;
  bool need_constraint_space = is_collapsed || !is_parallel;
  if (need_constraint_space) {
    NGConstraintSpaceBuilder builder(table_writing_mode,
                                     node.Style().GetWritingMode(),
                                     /* is_new_fc */ true);
    builder.SetTableCellBorders(cell_border);
    builder.SetIsTableCell(true, /* is_legacy_table_cell */ false);
    builder.SetCacheSlot(NGCacheSlot::kMeasure);
    if (!is_parallel) {
      PhysicalSize icb_size = node.InitialContainingBlockSize();
      builder.SetOrthogonalFallbackInlineSize(
          IsHorizontalWritingMode(table_writing_mode) ? icb_size.height
                                                      : icb_size.width);

      builder.SetIsShrinkToFit(node.Style().LogicalWidth().IsAuto());
      builder.SetTextDirection(node.Style().Direction());
    }
    NGConstraintSpace space = builder.ToConstraintSpace();
    // It'd be nice to avoid computing minmax if not needed, but the criteria
    // is not clear.
    min_max_size = node.ComputeMinMaxSizes(table_writing_mode, input, &space);
  } else {
    min_max_size = node.ComputeMinMaxSizes(table_writing_mode, input);
  }
  // Compute min inline size.
  LayoutUnit resolved_min_inline_size;
  if (!is_fixed_layout) {
    resolved_min_inline_size =
        std::max(min_max_size.sizes.min_size,
                 css_min_inline_size.value_or(LayoutUnit()));
    // https://quirks.spec.whatwg.org/#the-table-cell-nowrap-minimum-width-calculation-quirk
    // Has not worked in Legacy, might be pulled out.
    if (css_inline_size && node.GetDocument().InQuirksMode()) {
      bool has_nowrap_attribute =
          !To<Element>(node.GetDOMNode())
               ->FastGetAttribute(html_names::kNowrapAttr)
               .IsNull();
      if (has_nowrap_attribute && node.Style().AutoWrap()) {
        resolved_min_inline_size =
            std::max(resolved_min_inline_size, *css_inline_size);
      }
    }
  }

  // Compute resolved max inline size.
  LayoutUnit content_max;
  if (css_inline_size) {
    content_max = *css_inline_size;
  } else {
    content_max = min_max_size.sizes.max_size;
  }
  if (css_max_inline_size) {
    content_max = std::min(content_max, *css_max_inline_size);
    resolved_min_inline_size =
        std::min(resolved_min_inline_size, *css_max_inline_size);
  }
  LayoutUnit resolved_max_inline_size =
      std::max(resolved_min_inline_size, content_max);

  bool is_constrained = css_inline_size.has_value();

  DCHECK_LE(resolved_min_inline_size, resolved_max_inline_size);

  // Only fixed tables use border padding in percentage size computations.
  LayoutUnit percent_border_padding;
  if (is_fixed_layout && css_percentage_inline_size &&
      node.Style().BoxSizing() == EBoxSizing::kContentBox) {
    percent_border_padding = (cell_border + cell_padding).InlineSum();
  }

  DCHECK_GE(resolved_max_inline_size, percent_border_padding);
  return NGTableTypes::CellInlineConstraint{
      resolved_min_inline_size, resolved_max_inline_size,
      css_percentage_inline_size, percent_border_padding, is_constrained};
}

NGTableTypes::Section NGTableTypes::CreateSection(
    const NGLayoutInputNode& section,
    wtf_size_t start_row,
    wtf_size_t rows,
    LayoutUnit block_size) {
  const Length& section_css_block_size = section.Style().LogicalHeight();
  // TODO(crbug.com/1105272): Decide what to do with |Length::IsCalculated()|.
  bool is_constrained =
      section_css_block_size.IsFixed() || section_css_block_size.IsPercent();
  base::Optional<float> percent;
  if (section_css_block_size.IsPercent())
    percent = section_css_block_size.Percent();
  bool is_tbody =
      section.GetDOMNode()->HasTagName(html_names::kTbodyTag);
  return Section{start_row,
                 rows,
                 block_size,
                 percent,
                 is_constrained,
                 is_tbody,
                 /* needs_redistribution */ false};
}

NGTableTypes::CellBlockConstraint NGTableTypes::CreateCellBlockConstraint(
    const NGLayoutInputNode& node,
    LayoutUnit computed_block_size,
    LayoutUnit baseline,
    const NGBoxStrut& border_box_borders,
    wtf_size_t row_index,
    wtf_size_t column_index,
    wtf_size_t rowspan) {
  bool is_constrained = node.Style().LogicalHeight().IsFixed();
  return CellBlockConstraint{computed_block_size,
                             baseline,
                             border_box_borders,
                             row_index,
                             column_index,
                             rowspan,
                             node.Style().VerticalAlign(),
                             is_constrained};
}

NGTableTypes::RowspanCell NGTableTypes::CreateRowspanCell(
    wtf_size_t row_index,
    wtf_size_t rowspan,
    CellBlockConstraint* cell_block_constraint,
    base::Optional<LayoutUnit> css_cell_block_size) {
  if (css_cell_block_size) {
    cell_block_constraint->min_block_size =
        std::max(cell_block_constraint->min_block_size, *css_cell_block_size);
  }
  return RowspanCell{row_index, rowspan, *cell_block_constraint};
}

void NGTableTypes::CellInlineConstraint::Encompass(
    const NGTableTypes::CellInlineConstraint& other) {
  // Standard says:
  // "A column is constrained if any of the cells spanning only that column has
  // a computed width that is not "auto", and is not a percentage. This means
  // that <td width=50></td><td max-width=100> would be treated with constrained
  // column with width of 100.
  if (other.min_inline_size > min_inline_size)
    min_inline_size = other.min_inline_size;
  if (is_constrained == other.is_constrained) {
    max_inline_size = std::max(max_inline_size, other.max_inline_size);
  } else if (is_constrained) {
    max_inline_size = std::max(max_inline_size, other.min_inline_size);
  } else {
    DCHECK(other.is_constrained);
    max_inline_size = std::max(min_inline_size, other.max_inline_size);
  }
  is_constrained = is_constrained || other.is_constrained;
  max_inline_size = std::max(max_inline_size, other.max_inline_size);
  percent = std::max(percent, other.percent);
  if (other.percent > percent) {
    percent = other.percent;
    percent_border_padding = other.percent_border_padding;
  }
}

void NGTableTypes::Column::Encompass(
    const base::Optional<NGTableTypes::CellInlineConstraint>& cell) {
  if (!cell)
    return;

  if (min_inline_size) {
    if (min_inline_size < cell->min_inline_size) {
      min_inline_size = cell->min_inline_size;
    }
    if (is_constrained) {
      if (cell->is_constrained)
        max_inline_size = std::max(*max_inline_size, cell->max_inline_size);
      else
        max_inline_size = std::max(*max_inline_size, cell->min_inline_size);
    } else {  // !is_constrained
      max_inline_size = std::max(max_inline_size.value_or(LayoutUnit()),
                                 cell->max_inline_size);
    }
  } else {
    min_inline_size = cell->min_inline_size;
    max_inline_size = cell->max_inline_size;
  }
  if (min_inline_size && max_inline_size) {
    max_inline_size = std::max(*min_inline_size, *max_inline_size);
  }

  if (cell->percent > percent) {
    percent = cell->percent;
    percent_border_padding = cell->percent_border_padding;
  }
  is_constrained |= cell->is_constrained;
}

NGTableGroupedChildren::NGTableGroupedChildren(const NGBlockNode& table) {
  for (NGLayoutInputNode child = table.FirstChild(); child;
       child = child.NextSibling()) {
    NGBlockNode block_child = To<NGBlockNode>(child);
    if (block_child.IsTableCaption()) {
      captions.push_back(block_child);
    } else {
      switch (child.Style().Display()) {
        case EDisplay::kTableColumn:
        case EDisplay::kTableColumnGroup:
          columns.push_back(block_child);
          break;
        case EDisplay::kTableHeaderGroup:
          headers.push_back(block_child);
          break;
        case EDisplay::kTableRowGroup:
          bodies.push_back(block_child);
          break;
        case EDisplay::kTableFooterGroup:
          footers.push_back(block_child);
          break;
        default:
          NOTREACHED() << "unexpected table child";
      }
    }
  }
}

NGTableGroupedChildrenIterator NGTableGroupedChildren::begin() const {
  return NGTableGroupedChildrenIterator(*this);
}

NGTableGroupedChildrenIterator NGTableGroupedChildren::end() const {
  return NGTableGroupedChildrenIterator(*this, /* is_end */ true);
}

NGTableGroupedChildrenIterator::NGTableGroupedChildrenIterator(
    const NGTableGroupedChildren& grouped_children,
    bool is_end)
    : grouped_children_(grouped_children), current_vector_(nullptr) {
  if (is_end) {
    current_vector_ = &grouped_children_.footers;
    current_iterator_ = current_vector_->end();
    return;
  }
  AdvanceToNonEmptySection();
}

NGTableGroupedChildrenIterator& NGTableGroupedChildrenIterator::operator++() {
  ++current_iterator_;
  if (current_iterator_ == current_vector_->end())
    AdvanceToNonEmptySection();
  return *this;
}

NGBlockNode NGTableGroupedChildrenIterator::operator*() const {
  return *current_iterator_;
}

bool NGTableGroupedChildrenIterator::operator==(
    const NGTableGroupedChildrenIterator& rhs) const {
  return rhs.current_vector_ == current_vector_ &&
         rhs.current_iterator_ == current_iterator_;
}

bool NGTableGroupedChildrenIterator::operator!=(
    const NGTableGroupedChildrenIterator& rhs) const {
  return !(*this == rhs);
}

void NGTableGroupedChildrenIterator::AdvanceToNonEmptySection() {
  if (current_vector_ == &grouped_children_.footers)
    return;
  if (!current_vector_) {
    current_vector_ = &grouped_children_.headers;
  } else if (current_vector_ == &grouped_children_.headers) {
    current_vector_ = &grouped_children_.bodies;
  } else if (current_vector_ == &grouped_children_.bodies) {
    current_vector_ = &grouped_children_.footers;
  }
  current_iterator_ = current_vector_->begin();
  // If new group is empty, recursively advance.
  if (current_iterator_ == current_vector_->end()) {
    AdvanceToNonEmptySection();
  }
}

}  // namespace blink
