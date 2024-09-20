// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/layout/table/table_layout_algorithm_types.h"

#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_caption.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_column.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_section.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"

namespace blink {

namespace {

// Gathers css sizes. CSS values might be modified to enforce universal
// invariants: css_max_inline_size >= css_min_inline_size
// css_percentage_inline_size <= css_percentage_max_inline_size
inline void InlineSizesFromStyle(const ComputedStyle& style,
                                 LayoutUnit inline_border_padding,
                                 bool is_parallel,
                                 std::optional<LayoutUnit>* inline_size,
                                 std::optional<LayoutUnit>* min_inline_size,
                                 std::optional<LayoutUnit>* max_inline_size,
                                 std::optional<float>* percentage_inline_size) {
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
    else
      *inline_size = std::max(**inline_size, inline_border_padding);
  }
  if (min_length.IsFixed()) {
    *min_inline_size = LayoutUnit(min_length.Value());
    if (is_content_box)
      *min_inline_size = **min_inline_size + inline_border_padding;
    else
      *min_inline_size = std::max(**min_inline_size, inline_border_padding);
  }
  if (max_length.IsFixed()) {
    *max_inline_size = LayoutUnit(max_length.Value());
    if (is_content_box)
      *max_inline_size = **max_inline_size + inline_border_padding;
    else
      *max_inline_size = std::max(**max_inline_size, inline_border_padding);

    if (*min_inline_size)
      *max_inline_size = std::max(**min_inline_size, **max_inline_size);
  }
  if (length.IsPercent()) {
    *percentage_inline_size = length.Percent();
  } else if (length.IsCalculated() &&
             !length.GetCalculationValue().IsExpression()) {
    // crbug.com/1154376 Style engine should handle %+0px case automatically.
    PixelsAndPercent pixels_and_percent = length.GetPixelsAndPercent();
    if (pixels_and_percent.pixels == 0.0f)
      *percentage_inline_size = pixels_and_percent.percent;
  }

  if (*percentage_inline_size && max_length.IsPercent()) {
    *percentage_inline_size =
        std::min(**percentage_inline_size, max_length.Percent());
  }
  if (*min_inline_size && *max_inline_size)
    DCHECK_GE(**max_inline_size, **min_inline_size);
}

}  // namespace

constexpr LayoutUnit TableTypes::kTableMaxInlineSize;

// Implements https://www.w3.org/TR/css-tables-3/#computing-cell-measures
// "outer min-content and outer max-content widths for colgroups"
TableTypes::Column TableTypes::CreateColumn(
    const ComputedStyle& style,
    std::optional<LayoutUnit> default_inline_size,
    bool is_table_fixed) {
  std::optional<LayoutUnit> inline_size;
  std::optional<LayoutUnit> min_inline_size;
  std::optional<LayoutUnit> max_inline_size;
  std::optional<float> percentage_inline_size;
  InlineSizesFromStyle(style, /* inline_border_padding */ LayoutUnit(),
                       /* is_parallel */ true, &inline_size, &min_inline_size,
                       &max_inline_size, &percentage_inline_size);
  bool is_mergeable;
  if (!inline_size)
    inline_size = default_inline_size;
  if (min_inline_size && inline_size)
    inline_size = std::max(*inline_size, *min_inline_size);
  bool is_constrained = inline_size.has_value();
  if (percentage_inline_size && *percentage_inline_size == 0.0f)
    percentage_inline_size.reset();
  bool is_collapsed = style.UsedVisibility() == EVisibility::kCollapse;
  if (is_table_fixed) {
    is_mergeable = false;
  } else {
    is_mergeable = (inline_size.value_or(LayoutUnit()) == LayoutUnit()) &&
                   (percentage_inline_size.value_or(0.0f) == 0.0f);
  }
  return Column(min_inline_size.value_or(LayoutUnit()), inline_size,
                percentage_inline_size,
                LayoutUnit() /* percent_border_padding */, is_constrained,
                is_collapsed, is_table_fixed, is_mergeable);
}

// Implements https://www.w3.org/TR/css-tables-3/#computing-cell-measures
// "outer min-content and outer max-content widths for table cells"
// Note: this method calls BlockNode::ComputeMinMaxSizes.
TableTypes::CellInlineConstraint TableTypes::CreateCellInlineConstraint(
    const BlockNode& node,
    WritingDirectionMode table_writing_direction,
    bool is_fixed_layout,
    const BoxStrut& cell_border,
    const BoxStrut& cell_padding) {
  std::optional<LayoutUnit> css_inline_size;
  std::optional<LayoutUnit> css_min_inline_size;
  std::optional<LayoutUnit> css_max_inline_size;
  std::optional<float> css_percentage_inline_size;
  const auto& style = node.Style();
  const auto table_writing_mode = table_writing_direction.GetWritingMode();
  const bool is_parallel =
      IsParallelWritingMode(table_writing_mode, style.GetWritingMode());

  // Be lazy when determining the min/max sizes, as in some circumstances we
  // don't need to call this (relatively) expensive function.
  std::optional<MinMaxSizes> cached_min_max_sizes;
  auto MinMaxSizesFunc = [&]() -> MinMaxSizes {
    if (!cached_min_max_sizes) {
      const auto cell_writing_direction = style.GetWritingDirection();
      ConstraintSpaceBuilder builder(table_writing_mode, cell_writing_direction,
                                     /* is_new_fc */ true);
      builder.SetTableCellBorders(cell_border, cell_writing_direction,
                                  table_writing_direction);
      builder.SetIsTableCell(true);
      builder.SetCacheSlot(LayoutResultCacheSlot::kMeasure);
      if (!is_parallel) {
        // Only consider the ICB-size for the orthogonal fallback inline-size
        // (don't use the size of the containing-block).
        const PhysicalSize icb_size = node.InitialContainingBlockSize();
        builder.SetOrthogonalFallbackInlineSize(
            IsHorizontalWritingMode(table_writing_mode) ? icb_size.height
                                                        : icb_size.width);
      }
      builder.SetAvailableSize({kIndefiniteSize, kIndefiniteSize});
      const auto space = builder.ToConstraintSpace();

      cached_min_max_sizes =
          node.ComputeMinMaxSizes(table_writing_mode, SizeType::kIntrinsic,
                                  space)
              .sizes;
    }

    return *cached_min_max_sizes;
  };

  InlineSizesFromStyle(style, (cell_border + cell_padding).InlineSum(),
                       is_parallel, &css_inline_size, &css_min_inline_size,
                       &css_max_inline_size, &css_percentage_inline_size);

  // Compute the resolved min inline-size.
  LayoutUnit resolved_min_inline_size;
  if (!is_fixed_layout) {
    resolved_min_inline_size = std::max(
        MinMaxSizesFunc().min_size, css_min_inline_size.value_or(LayoutUnit()));
    // https://quirks.spec.whatwg.org/#the-table-cell-nowrap-minimum-width-calculation-quirk
    bool has_nowrap_attribute =
        node.GetDOMNode() && To<Element>(node.GetDOMNode())
                                 ->FastHasAttribute(html_names::kNowrapAttr);
    if (css_inline_size && node.GetDocument().InQuirksMode() &&
        has_nowrap_attribute) {
      resolved_min_inline_size =
          std::max(resolved_min_inline_size, *css_inline_size);
    }
  }

  // Compute the resolved max inline-size.
  LayoutUnit content_max = css_inline_size.value_or(MinMaxSizesFunc().max_size);
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
      style.BoxSizing() == EBoxSizing::kContentBox)
    percent_border_padding = (cell_border + cell_padding).InlineSum();

  DCHECK_GE(resolved_max_inline_size, percent_border_padding);
  return TableTypes::CellInlineConstraint{
      resolved_min_inline_size, resolved_max_inline_size,
      css_percentage_inline_size, percent_border_padding, is_constrained};
}

TableTypes::Section TableTypes::CreateSection(const LayoutInputNode& section,
                                              wtf_size_t start_row,
                                              wtf_size_t row_count,
                                              LayoutUnit block_size,
                                              bool treat_as_tbody) {
  const Length& section_css_block_size = section.Style().LogicalHeight();
  // TODO(crbug.com/1105272): Decide what to do with |Length::IsCalculated()|.
  bool is_constrained =
      section_css_block_size.IsFixed() || section_css_block_size.IsPercent();
  std::optional<float> percent;
  if (section_css_block_size.IsPercent())
    percent = section_css_block_size.Percent();
  return Section{start_row,
                 row_count,
                 block_size,
                 percent,
                 is_constrained,
                 treat_as_tbody,
                 /* needs_redistribution */ false};
}

void TableTypes::CellInlineConstraint::Encompass(
    const TableTypes::CellInlineConstraint& other) {
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
  if (other.percent > percent) {
    percent = other.percent;
    percent_border_padding = other.percent_border_padding;
  }
}

void TableTypes::Column::Encompass(
    const std::optional<TableTypes::CellInlineConstraint>& cell) {
  if (!cell)
    return;

  // Constrained columns in fixed tables take precedence over cells.
  if (is_constrained && is_table_fixed)
    return;
  if (!is_table_fixed)
    is_mergeable = false;
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

TableGroupedChildren::TableGroupedChildren(const BlockNode& table)
    : header(BlockNode(nullptr)), footer(BlockNode(nullptr)) {
  for (LayoutInputNode child = table.FirstChild(); child;
       child = child.NextSibling()) {
    BlockNode block_child = To<BlockNode>(child);
    if (block_child.IsTableCaption()) {
      captions.push_back(block_child);
    } else {
      switch (child.Style().Display()) {
        case EDisplay::kTableColumn:
        case EDisplay::kTableColumnGroup:
          columns.push_back(block_child);
          break;
        case EDisplay::kTableHeaderGroup:
          if (!header)
            header = block_child;
          else
            bodies.push_back(block_child);
          break;
        case EDisplay::kTableRowGroup:
          bodies.push_back(block_child);
          break;
        case EDisplay::kTableFooterGroup:
          if (!footer)
            footer = block_child;
          else
            bodies.push_back(block_child);
          break;
        default:
          NOTREACHED_IN_MIGRATION() << "unexpected table child";
      }
    }
  }
}

void TableGroupedChildren::Trace(Visitor* visitor) const {
  visitor->Trace(captions);
  visitor->Trace(columns);
  visitor->Trace(header);
  visitor->Trace(bodies);
  visitor->Trace(footer);
}

TableGroupedChildrenIterator TableGroupedChildren::begin() const {
  return TableGroupedChildrenIterator(*this);
}

TableGroupedChildrenIterator TableGroupedChildren::end() const {
  return TableGroupedChildrenIterator(*this, /* is_end */ true);
}

TableGroupedChildrenIterator::TableGroupedChildrenIterator(
    const TableGroupedChildren& grouped_children,
    bool is_end)
    : grouped_children_(grouped_children) {
  if (is_end) {
    current_section_ = kEnd;
    return;
  }
  current_section_ = kNone;
  AdvanceForwardToNonEmptySection();
}

TableGroupedChildrenIterator& TableGroupedChildrenIterator::operator++() {
  switch (current_section_) {
    case kHead:
    case kFoot:
      AdvanceForwardToNonEmptySection();
      break;
    case kBody:
      ++position_;
      if (body_vector_->begin() + position_ == grouped_children_.bodies.end())
        AdvanceForwardToNonEmptySection();
      break;
    case kEnd:
      break;
    case kNone:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return *this;
}

TableGroupedChildrenIterator& TableGroupedChildrenIterator::operator--() {
  switch (current_section_) {
    case kHead:
    case kFoot:
      AdvanceBackwardToNonEmptySection();
      break;
    case kBody:
      if (position_ == 0)
        AdvanceBackwardToNonEmptySection();
      else
        --position_;
      break;
    case kEnd:
      AdvanceBackwardToNonEmptySection();
      break;
    case kNone:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return *this;
}

BlockNode TableGroupedChildrenIterator::operator*() const {
  switch (current_section_) {
    case kHead:
      return grouped_children_.header;
    case kFoot:
      return grouped_children_.footer;
    case kBody:
      return body_vector_->at(position_);
    case kEnd:
    case kNone:
      NOTREACHED_IN_MIGRATION();
      return BlockNode(nullptr);
  }
}

bool TableGroupedChildrenIterator::operator==(
    const TableGroupedChildrenIterator& rhs) const {
  if (current_section_ != rhs.current_section_)
    return false;
  if (current_section_ == kBody)
    return rhs.body_vector_ == body_vector_ && rhs.position_ == position_;
  return true;
}

bool TableGroupedChildrenIterator::operator!=(
    const TableGroupedChildrenIterator& rhs) const {
  return !(*this == rhs);
}

void TableGroupedChildrenIterator::AdvanceForwardToNonEmptySection() {
  switch (current_section_) {
    case kNone:
      current_section_ = kHead;
      if (!grouped_children_.header)
        AdvanceForwardToNonEmptySection();
      break;
    case kHead:
      current_section_ = kBody;
      body_vector_ = &grouped_children_.bodies;
      position_ = 0;
      if (body_vector_->size() == 0)
        AdvanceForwardToNonEmptySection();
      break;
    case kBody:
      current_section_ = kFoot;
      if (!grouped_children_.footer)
        AdvanceForwardToNonEmptySection();
      break;
    case kFoot:
      current_section_ = kEnd;
      break;
    case kEnd:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void TableGroupedChildrenIterator::AdvanceBackwardToNonEmptySection() {
  switch (current_section_) {
    case kNone:
      NOTREACHED_IN_MIGRATION();
      break;
    case kHead:
      current_section_ = kNone;
      break;
    case kBody:
      current_section_ = kHead;
      if (!grouped_children_.header)
        AdvanceBackwardToNonEmptySection();
      break;
    case kFoot:
      current_section_ = kBody;
      body_vector_ = &grouped_children_.bodies;
      if (body_vector_->size() == 0)
        AdvanceBackwardToNonEmptySection();
      else
        position_ = body_vector_->size() - 1;
      break;
    case kEnd:
      current_section_ = kFoot;
      if (!grouped_children_.footer)
        AdvanceBackwardToNonEmptySection();
      break;
  }
}

}  // namespace blink
