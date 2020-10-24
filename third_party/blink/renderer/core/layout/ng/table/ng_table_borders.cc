// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/ng_table_borders.h"

#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_column_visitor.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_helpers.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_types.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

// https://www.w3.org/TR/css-tables-3/#conflict-resolution-for-collapsed-borders
bool IsSourceMoreSpecificThanEdge(EBorderStyle source_style,
                                  LayoutUnit source_width,
                                  const NGTableBorders::Edge& edge) {
  if (edge.edge_side == NGTableBorders::EdgeSide::kDoNotFill)
    return false;

  if (!edge.style || source_style == EBorderStyle::kHidden)
    return true;

  EBorderStyle edge_border_style =
      NGTableBorders::BorderStyle(edge.style.get(), edge.edge_side);
  if (edge_border_style == EBorderStyle::kHidden)
    return false;

  LayoutUnit edge_width =
      NGTableBorders::BorderWidth(edge.style.get(), edge.edge_side);
  if (source_width < edge_width)
    return false;
  if (source_width > edge_width)
    return true;
  return source_style > edge_border_style;
}

// Side of the style the collapsed border belongs to.
enum class LogicalEdgeSide { kInlineStart, kInlineEnd, kBlockStart, kBlockEnd };

NGTableBorders::EdgeSide LogicalEdgeToPhysical(
    LogicalEdgeSide logical_side,
    WritingDirectionMode table_writing_direction) {
  // https://www.w3.org/TR/css-writing-modes-4/#logical-to-physical
  switch (logical_side) {
    case LogicalEdgeSide::kInlineStart:
      switch (table_writing_direction.GetWritingMode()) {
        case WritingMode::kHorizontalTb:
          return table_writing_direction.Direction() == TextDirection::kLtr
                     ? NGTableBorders::EdgeSide::kLeft
                     : NGTableBorders::EdgeSide::kRight;
        case WritingMode::kVerticalLr:
        case WritingMode::kVerticalRl:
        case WritingMode::kSidewaysRl:
          return table_writing_direction.Direction() == TextDirection::kLtr
                     ? NGTableBorders::EdgeSide::kTop
                     : NGTableBorders::EdgeSide::kBottom;
        case WritingMode::kSidewaysLr:
          return table_writing_direction.Direction() == TextDirection::kLtr
                     ? NGTableBorders::EdgeSide::kBottom
                     : NGTableBorders::EdgeSide::kTop;
      }
    case LogicalEdgeSide::kInlineEnd:
      switch (table_writing_direction.GetWritingMode()) {
        case WritingMode::kHorizontalTb:
          return table_writing_direction.Direction() == TextDirection::kLtr
                     ? NGTableBorders::EdgeSide::kRight
                     : NGTableBorders::EdgeSide::kLeft;
        case WritingMode::kVerticalLr:
        case WritingMode::kVerticalRl:
        case WritingMode::kSidewaysRl:
          return table_writing_direction.Direction() == TextDirection::kLtr
                     ? NGTableBorders::EdgeSide::kBottom
                     : NGTableBorders::EdgeSide::kTop;
        case WritingMode::kSidewaysLr:
          return table_writing_direction.Direction() == TextDirection::kLtr
                     ? NGTableBorders::EdgeSide::kTop
                     : NGTableBorders::EdgeSide::kBottom;
      }
    case LogicalEdgeSide::kBlockStart:
      switch (table_writing_direction.GetWritingMode()) {
        case WritingMode::kHorizontalTb:
          return NGTableBorders::EdgeSide::kTop;
        case WritingMode::kVerticalLr:
        case WritingMode::kSidewaysLr:
          return NGTableBorders::EdgeSide::kLeft;
        case WritingMode::kVerticalRl:
        case WritingMode::kSidewaysRl:
          return NGTableBorders::EdgeSide::kRight;
      }
    case LogicalEdgeSide::kBlockEnd:
      switch (table_writing_direction.GetWritingMode()) {
        case WritingMode::kHorizontalTb:
          return NGTableBorders::EdgeSide::kBottom;
        case WritingMode::kVerticalLr:
        case WritingMode::kSidewaysLr:
          return NGTableBorders::EdgeSide::kRight;
        case WritingMode::kVerticalRl:
        case WritingMode::kSidewaysRl:
          return NGTableBorders::EdgeSide::kLeft;
      }
  }
}

class ColBordersMarker {
  STACK_ALLOCATED();

 public:
  void VisitCol(const NGLayoutInputNode& column,
                wtf_size_t start_column_index,
                wtf_size_t span) {
    for (wtf_size_t i = 0; i < span; ++i) {
      wtf_size_t current_column_index = start_column_index + i;
      borders.MergeBorders(0, current_column_index, table_row_count, 1,
                           &column.Style(), NGTableBorders::EdgeSource::kColumn,
                           table_writing_direction);
    }
  }
  void EnterColgroup(const NGLayoutInputNode& colgroup,
                     wtf_size_t start_column_index) {}
  void LeaveColgroup(const NGLayoutInputNode& colgroup,
                     wtf_size_t start_column_index,
                     wtf_size_t span,
                     bool has_children) {}
  ColBordersMarker(wtf_size_t table_row_count,
                   WritingDirectionMode table_writing_direction,
                   NGTableBorders& borders)
      : table_row_count(table_row_count),
        table_writing_direction(table_writing_direction),
        borders(borders) {}
  wtf_size_t table_row_count;
  WritingDirectionMode table_writing_direction;
  NGTableBorders& borders;
};

class ColgroupBordersMarker {
  STACK_ALLOCATED();

 public:
  void VisitCol(const NGLayoutInputNode& column,
                wtf_size_t start_column_index,
                wtf_size_t span) {}
  void EnterColgroup(const NGLayoutInputNode& colgroup,
                     wtf_size_t start_column_index) {}
  void LeaveColgroup(const NGLayoutInputNode& colgroup,
                     wtf_size_t start_column_index,
                     wtf_size_t span,
                     bool has_children) {
    borders.MergeBorders(0, start_column_index, table_row_count, span,
                         &colgroup.Style(), NGTableBorders::EdgeSource::kColumn,
                         table_writing_direction);
  }
  ColgroupBordersMarker(wtf_size_t table_row_count,
                        WritingDirectionMode table_writing_direction,
                        NGTableBorders& borders)
      : table_row_count(table_row_count),
        table_writing_direction(table_writing_direction),
        borders(borders) {}
  wtf_size_t table_row_count;
  WritingDirectionMode table_writing_direction;
  NGTableBorders& borders;
};

}  // namespace

scoped_refptr<NGTableBorders> NGTableBorders::ComputeTableBorders(
    const NGBlockNode& table) {
  const ComputedStyle& table_style = table.Style();
  NGBoxStrut intrinsic_borders(LayoutUnit(table_style.BorderStartWidth()),
                               LayoutUnit(table_style.BorderEndWidth()),
                               LayoutUnit(table_style.BorderBeforeWidth()),
                               LayoutUnit(table_style.BorderAfterWidth()));
  scoped_refptr<NGTableBorders> table_borders =
      base::MakeRefCounted<NGTableBorders>(table_style, intrinsic_borders);

  if (table_style.BorderCollapse() != EBorderCollapse::kCollapse)
    return table_borders;

  NGTableGroupedChildren grouped_children(table);
  bool hide_empty_cells = table_style.EmptyCells() == EEmptyCells::kHide;
  WritingDirectionMode table_writing_direction =
      table.Style().GetWritingDirection();
  wtf_size_t table_column_count = 0;
  wtf_size_t table_row_index = 0;

  // Mark cell borders.
  bool found_multispan_cells = false;
  for (const NGBlockNode section : grouped_children) {
    wtf_size_t section_start_row = table_row_index;
    NGColspanCellTabulator tabulator;
    for (NGBlockNode row = To<NGBlockNode>(section.FirstChild()); row;
         row = To<NGBlockNode>(row.NextSibling())) {
      tabulator.StartRow();
      for (NGBlockNode cell = To<NGBlockNode>(row.FirstChild()); cell;
           cell = To<NGBlockNode>(cell.NextSibling())) {
        tabulator.FindNextFreeColumn();
        // https://stackoverflow.com/questions/18758373/why-do-the-css-property-border-collapse-and-empty-cells-conflict
        if (hide_empty_cells && !To<NGBlockNode>(cell).FirstChild()) {
          tabulator.ProcessCell(cell);
          continue;
        }
        wtf_size_t cell_colspan = cell.TableCellColspan();
        found_multispan_cells |=
            cell.TableCellRowspan() > 1 || cell_colspan > 1;
        // Rowspan has to be limited by section size. Since we do not know
        // section size, we have to rerun cell distribution with limited
        // rowspans.
        table_column_count = std::max(
            table_column_count, NGTableAlgorithmHelpers::ComputeMaxColumn(
                                    tabulator.CurrentColumn(), cell_colspan,
                                    table.Style().IsFixedTableLayout()));
        if (!found_multispan_cells) {
          table_borders->MergeBorders(
              table_row_index, tabulator.CurrentColumn(),
              cell.TableCellRowspan(), cell_colspan, &cell.Style(),
              NGTableBorders::EdgeSource::kCell, table_writing_direction);
        }
        tabulator.ProcessCell(cell);
      }
      tabulator.EndRow();
      ++table_row_index;
    }
    table_borders->AddSection(section_start_row,
                              table_row_index - section_start_row);
  }
  table_borders->SetLastColumnIndex(table_column_count);

  wtf_size_t table_row_count = table_row_index;
  table_row_index = 0;

  // Mark cell borders again with limited rowspan.
  // If any cells have rowspan, need to redistribute cell borders.
  if (found_multispan_cells) {
    wtf_size_t section_index = 0;
    for (NGBlockNode section : grouped_children) {
      NGColspanCellTabulator tabulator;
      for (NGBlockNode row = To<NGBlockNode>(section.FirstChild()); row;
           row = To<NGBlockNode>(row.NextSibling())) {
        tabulator.StartRow();
        for (NGBlockNode cell = To<NGBlockNode>(row.FirstChild()); cell;
             cell = To<NGBlockNode>(cell.NextSibling())) {
          tabulator.FindNextFreeColumn();
          if (hide_empty_cells && !To<NGBlockNode>(cell).FirstChild()) {
            tabulator.ProcessCell(cell);
            continue;
          }
          table_borders->MergeBorders(
              table_row_index, tabulator.CurrentColumn(),
              cell.TableCellRowspan(), cell.TableCellColspan(), &cell.Style(),
              NGTableBorders::EdgeSource::kCell, table_writing_direction,
              section_index);
          tabulator.ProcessCell(cell);
        }
        tabulator.EndRow();
        ++table_row_index;
      }
      ++section_index;
    }
  }

  // Mark row borders.
  table_row_index = 0;
  for (NGBlockNode section : grouped_children) {
    for (NGBlockNode row = To<NGBlockNode>(section.FirstChild()); row;
         row = To<NGBlockNode>(row.NextSibling())) {
      table_borders->MergeBorders(
          table_row_index, 0, 1, table_column_count, &row.Style(),
          NGTableBorders::EdgeSource::kRow, table_writing_direction);
      ++table_row_index;
    }
  }

  // Mark section borders.
  // It is tempting to traverse sections at the same time as rows,
  // but it would cause precedence errors.
  wtf_size_t section_index = 0;
  for (NGBlockNode section : grouped_children) {
    NGTableBorders::Section section_info =
        table_borders->GetSection(section_index);
    table_borders->MergeBorders(
        section_info.start_row, 0, section_info.row_count, table_column_count,
        &section.Style(), NGTableBorders::EdgeSource::kSection,
        table_writing_direction);
    ++section_index;
  }

  // Mark column borders.
  // COL borders have precedence over COLGROUP borders.
  // We have to traverse COL first, then COLGROUP.
  ColBordersMarker col_borders_marker(table_row_count, table_writing_direction,
                                      *table_borders.get());
  VisitLayoutNGTableColumn(
      const_cast<Vector<NGBlockNode>&>(grouped_children.columns),
      table_column_count, &col_borders_marker);
  ColgroupBordersMarker colgroup_borders_marker(
      table_row_count, table_writing_direction, *table_borders.get());
  VisitLayoutNGTableColumn(
      const_cast<Vector<NGBlockNode>&>(grouped_children.columns),
      table_column_count, &colgroup_borders_marker);

  // Mark table borders.
  table_borders->MergeBorders(0, 0, table_row_count, table_column_count,
                              &table_style, NGTableBorders::EdgeSource::kTable,
                              table_writing_direction);

  table_borders->ComputeCollapsedTableBorderPadding(table_row_count,
                                                    table_column_count);
  return table_borders;
}

NGTableBorders::NGTableBorders(const ComputedStyle& table_style,
                               const NGBoxStrut& table_border)
    : is_collapsed_(table_style.BorderCollapse() ==
                    EBorderCollapse::kCollapse) {
  if (!is_collapsed_) {
    cached_table_border_ = table_border;
  }
}

#if DCHECK_IS_ON()
String NGTableBorders::DumpEdges() {
  if (edges_per_row_ == 0)
    return "No edges";

  StringBuilder edge_string;
  wtf_size_t row_count = edges_.size() / edges_per_row_;
  for (wtf_size_t row = 0; row < row_count; ++row) {
    for (wtf_size_t i = 0; i < edges_per_row_; ++i) {
      const auto& edge = edges_[edges_per_row_ * row + i];
      if (edge.style) {
        switch (edge.edge_side) {
          case EdgeSide::kTop:
            edge_string.Append('-');
            break;
          case EdgeSide::kBottom:
            edge_string.Append('_');
            break;
          case EdgeSide::kLeft:
            edge_string.Append('[');
            break;
          case EdgeSide::kRight:
            edge_string.Append(']');
            break;
          case EdgeSide::kDoNotFill:
            edge_string.Append('?');
            break;
        }
      } else {  // no style.
        if (edge.edge_side == EdgeSide::kDoNotFill)
          edge_string.Append('X');
        else
          edge_string.Append('.');
      }
      if (i & 1)  // i is odd.
        edge_string.Append(' ');
    }
    edge_string.Append('\n');
  }
  return edge_string.ToString();
}

void NGTableBorders::ShowEdges() {
  LOG(INFO) << "\n" << DumpEdges().Utf8();
}

#endif

NGBoxStrut NGTableBorders::GetCellBorders(wtf_size_t row,
                                          wtf_size_t column,
                                          wtf_size_t rowspan,
                                          wtf_size_t colspan) const {
  NGBoxStrut border_strut;
  if (edges_per_row_ == 0)
    return border_strut;
  DCHECK_EQ(edges_.size() % edges_per_row_, 0u);
  if (column * 2 >= edges_per_row_ || row >= edges_.size() / edges_per_row_)
    return border_strut;

  // Compute inline border widths.
  wtf_size_t first_inline_start_edge = row * edges_per_row_ + column * 2;
  wtf_size_t first_inline_end_edge = first_inline_start_edge + colspan * 2;
  for (wtf_size_t i = 0; i < rowspan; ++i) {
    wtf_size_t start_edge_index = first_inline_start_edge + i * edges_per_row_;
    border_strut.inline_start =
        std::max(border_strut.inline_start, HasEdgeAtIndex(start_edge_index)
                                                ? BorderWidth(start_edge_index)
                                                : LayoutUnit());
    if (start_edge_index >= edges_.size())
      break;
    wtf_size_t end_edge_index = first_inline_end_edge + i * edges_per_row_;
    border_strut.inline_end =
        std::max(border_strut.inline_end, HasEdgeAtIndex(end_edge_index)
                                              ? BorderWidth(end_edge_index)
                                              : LayoutUnit());
  }
  // Compute block border widths.
  wtf_size_t start_edge_column_index = column * 2 + 1;
  for (wtf_size_t i = 0; i < colspan; ++i) {
    wtf_size_t current_column_index = start_edge_column_index + i * 2;
    if (current_column_index >= edges_per_row_)
      break;
    wtf_size_t start_edge_index = row * edges_per_row_ + current_column_index;
    border_strut.block_start =
        std::max(border_strut.block_start, HasEdgeAtIndex(start_edge_index)
                                               ? BorderWidth(start_edge_index)
                                               : LayoutUnit());
    wtf_size_t end_edge_index = start_edge_index + rowspan * edges_per_row_;
    border_strut.block_end =
        std::max(border_strut.block_end, HasEdgeAtIndex(end_edge_index)
                                             ? BorderWidth(end_edge_index)
                                             : LayoutUnit());
  }
  DCHECK(is_collapsed_);
  // If borders are not divisible by 2, two half borders will not add up
  // to original border size (off by 1/64px). This is ok, because
  // pixel snapping will round to physical pixels.
  border_strut.block_start /= 2;
  border_strut.block_end /= 2;
  border_strut.inline_start /= 2;
  border_strut.inline_end /= 2;
  return border_strut;
}

void NGTableBorders::ComputeCollapsedTableBorderPadding(
    wtf_size_t table_row_count,
    wtf_size_t table_column_count) {
  DCHECK(is_collapsed_);
  // https://www.w3.org/TR/CSS2/tables.html#collapsing-borders
  // block[start|end] borders are computed by traversing all the edges.
  // inline[start|end] borders are computed by looking at first/last edge.
  if (edges_per_row_ == 0) {
    cached_table_border_ = NGBoxStrut();
    return;
  }
  DCHECK_GE((table_column_count + 1) * 2, edges_per_row_);
  // We still need visual border rect.
  NGBoxStrut borders =
      GetCellBorders(0, 0, table_row_count, table_column_count);
  collapsed_visual_inline_start_ = borders.inline_start;
  collapsed_visual_inline_end_ = borders.inline_end;
  wtf_size_t inline_start_edge = 0;
  wtf_size_t inline_end_edge = 2 * table_column_count;
  borders.inline_start = HasEdgeAtIndex(inline_start_edge)
                             ? BorderWidth(inline_start_edge) / 2
                             : LayoutUnit();
  borders.inline_end = HasEdgeAtIndex(inline_end_edge)
                           ? BorderWidth(inline_end_edge) / 2
                           : LayoutUnit();
  cached_table_border_ = borders;
}

NGBoxStrut NGTableBorders::CellBorder(
    const NGBlockNode& cell,
    wtf_size_t row,
    wtf_size_t column,
    wtf_size_t section,
    WritingDirectionMode table_writing_direction) const {
  if (is_collapsed_) {
    return GetCellBorders(row, column,
                          ClampRowspan(section, row, cell.TableCellRowspan()),
                          ClampColspan(column, cell.TableCellColspan()));
  }
  return ComputeBorders(
      NGConstraintSpaceBuilder(table_writing_direction.GetWritingMode(),
                               table_writing_direction.GetWritingMode(),
                               /* is_new_fc */ false)
          .ToConstraintSpace(),
      cell);
}

// As we are determining the intrinsic size of the table at this stage,
// %-padding resolves against an indefinite size.
NGBoxStrut NGTableBorders::CellPaddingForMeasure(
    const ComputedStyle& cell_style,
    WritingDirectionMode table_writing_direction) const {
  if (!cell_style.MayHavePadding())
    return NGBoxStrut();
  return ComputePadding(
      NGConstraintSpaceBuilder(table_writing_direction.GetWritingMode(),
                               table_writing_direction.GetWritingMode(),
                               /* is_new_fc */ false)
          .ToConstraintSpace(),
      cell_style);
}

void NGTableBorders::MergeBorders(wtf_size_t cell_start_row,
                                  wtf_size_t cell_start_column,
                                  wtf_size_t rowspan,
                                  wtf_size_t colspan,
                                  const ComputedStyle* source_style,
                                  EdgeSource source,
                                  WritingDirectionMode table_writing_direction,
                                  wtf_size_t section_index) {
  DCHECK(is_collapsed_);
  // Can be 0 in empty table parts.
  if (rowspan == 0 || colspan == 0)
    return;

  wtf_size_t clamped_colspan = ClampColspan(cell_start_column, colspan);
  wtf_size_t clamped_rowspan =
      source == EdgeSource::kCell
          ? ClampRowspan(section_index, cell_start_row, rowspan)
          : rowspan;
  bool mark_inner_borders = source == EdgeSource::kCell &&
                            (clamped_rowspan > 1 || clamped_colspan > 1);

  if (mark_inner_borders) {
    EnsureCellColumnFits(cell_start_column + clamped_colspan - 1);
    EnsureCellRowFits(cell_start_row + clamped_rowspan - 1);
  } else {
    PhysicalToLogical<EBorderStyle> border_style(
        table_writing_direction, source_style->BorderTopStyle(),
        source_style->BorderRightStyle(), source_style->BorderBottomStyle(),
        source_style->BorderLeftStyle());
    if (border_style.InlineStart() == EBorderStyle::kNone &&
        border_style.InlineEnd() == EBorderStyle::kNone &&
        border_style.BlockStart() == EBorderStyle::kNone &&
        border_style.BlockEnd() == EBorderStyle::kNone) {
      return;
    }
    // Only need to ensure edges that will be assigned exist.
    if (border_style.InlineEnd() == EBorderStyle::kNone &&
        border_style.BlockStart() == EBorderStyle::kNone &&
        border_style.BlockEnd() == EBorderStyle::kNone) {
      EnsureCellColumnFits(cell_start_column);
    } else {
      EnsureCellColumnFits(cell_start_column + clamped_colspan - 1);
    }
    if (border_style.InlineStart() == EBorderStyle::kNone &&
        border_style.InlineEnd() == EBorderStyle::kNone &&
        border_style.BlockEnd() == EBorderStyle::kNone) {
      EnsureCellRowFits(cell_start_row);
    } else {
      EnsureCellRowFits(cell_start_row + clamped_rowspan - 1);
    }
  }
  MergeRowAxisBorder(cell_start_row, cell_start_column, clamped_colspan,
                     source_style,
                     LogicalEdgeToPhysical(LogicalEdgeSide::kBlockStart,
                                           table_writing_direction));
  MergeRowAxisBorder(cell_start_row + clamped_rowspan, cell_start_column,
                     clamped_colspan, source_style,
                     LogicalEdgeToPhysical(LogicalEdgeSide::kBlockEnd,
                                           table_writing_direction));
  MergeColumnAxisBorder(cell_start_row, cell_start_column, clamped_rowspan,
                        source_style,
                        LogicalEdgeToPhysical(LogicalEdgeSide::kInlineStart,
                                              table_writing_direction));
  MergeColumnAxisBorder(cell_start_row, cell_start_column + clamped_colspan,
                        clamped_rowspan, source_style,
                        LogicalEdgeToPhysical(LogicalEdgeSide::kInlineEnd,
                                              table_writing_direction));
  if (mark_inner_borders) {
    MarkInnerBordersAsDoNotFill(cell_start_row, cell_start_column,
                                clamped_rowspan, clamped_colspan);
  }
}

void NGTableBorders::MergeRowAxisBorder(wtf_size_t start_row,
                                        wtf_size_t start_column,
                                        wtf_size_t colspan,
                                        const ComputedStyle* source_style,
                                        EdgeSide physical_side) {
  EBorderStyle source_border_style = BorderStyle(source_style, physical_side);
  if (source_border_style == EBorderStyle::kNone)
    return;
  LayoutUnit source_border_width = BorderWidth(source_style, physical_side);
  wtf_size_t start_edge = edges_per_row_ * start_row + start_column * 2 + 1;
  wtf_size_t end_edge = start_edge + colspan * 2;
  for (wtf_size_t current_edge = start_edge; current_edge < end_edge;
       current_edge += 2) {
    // https://www.w3.org/TR/css-tables-3/#border-specificity
    if (IsSourceMoreSpecificThanEdge(source_border_style, source_border_width,
                                     edges_[current_edge])) {
      edges_[current_edge].style = source_style;
      edges_[current_edge].edge_side = physical_side;
    }
  }
}

void NGTableBorders::MergeColumnAxisBorder(wtf_size_t start_row,
                                           wtf_size_t start_column,
                                           wtf_size_t rowspan,
                                           const ComputedStyle* source_style,
                                           EdgeSide physical_side) {
  EBorderStyle source_border_style = BorderStyle(source_style, physical_side);
  if (source_border_style == EBorderStyle::kNone)
    return;
  LayoutUnit source_border_width = BorderWidth(source_style, physical_side);
  wtf_size_t start_edge = edges_per_row_ * start_row + start_column * 2;
  wtf_size_t end_edge = start_edge + (rowspan * edges_per_row_);
  for (wtf_size_t current_edge = start_edge; current_edge < end_edge;
       current_edge += edges_per_row_) {
    // https://www.w3.org/TR/css-tables-3/#border-specificity
    if (IsSourceMoreSpecificThanEdge(source_border_style, source_border_width,
                                     edges_[current_edge])) {
      edges_[current_edge].style = source_style;
      edges_[current_edge].edge_side = physical_side;
    }
  }
}

// Rowspanned/colspanned cells need to mark inner edges as do-not-fill to
// prevent tables parts from drawing into them.
void NGTableBorders::MarkInnerBordersAsDoNotFill(wtf_size_t start_row,
                                                 wtf_size_t start_column,
                                                 wtf_size_t rowspan,
                                                 wtf_size_t colspan) {
  // Mark block axis edges.
  wtf_size_t start_edge = (start_column * 2) + 2;
  wtf_size_t end_edge = start_edge + (colspan - 1) * 2;
  for (wtf_size_t row = start_row;
       row < start_row + rowspan && start_edge != end_edge; ++row) {
    wtf_size_t row_offset = row * edges_per_row_;
    for (wtf_size_t edge = row_offset + start_edge;
         edge < row_offset + end_edge; edge += 2) {
      // DCHECK(!edges_[edge].style) is true in most tables. But,
      // when two cells overlap each other, (really an error)
      // style might already be assigned.
      if (!edges_[edge].style)
        edges_[edge].edge_side = EdgeSide::kDoNotFill;
    }
  }
  // Mark inline axis edges.
  start_edge = start_column * 2 + 1;
  end_edge = start_edge + colspan * 2;
  for (wtf_size_t row = start_row + 1; row < start_row + rowspan; ++row) {
    wtf_size_t row_offset = row * edges_per_row_;
    for (wtf_size_t edge = row_offset + start_edge;
         edge < row_offset + end_edge; edge += 2) {
      if (!edges_[edge].style)
        edges_[edge].edge_side = EdgeSide::kDoNotFill;
    }
  }
}

// Inline edges are edges between columns.
void NGTableBorders::EnsureCellColumnFits(wtf_size_t cell_column) {
  wtf_size_t desired_edges_per_row = (cell_column + 2) * 2;
  if (desired_edges_per_row <= edges_per_row_)
    return;

  // When number of columns changes, all rows have to be resized.
  // Edges must be copied to new positions. This can be expensive.
  // Most tables do not change number of columns after the 1st row.
  wtf_size_t row_count =
      edges_per_row_ == 0 ? 1 : edges_.size() / edges_per_row_;
  edges_.resize(row_count * desired_edges_per_row);
  for (wtf_size_t row_index = row_count - 1; row_index > 0; --row_index) {
    wtf_size_t new_edge = desired_edges_per_row - 1;
    bool done = false;
    // while loop is necessary to count down with unsigned.
    do {
      wtf_size_t new_edge_index = row_index * desired_edges_per_row + new_edge;
      if (new_edge < edges_per_row_) {
        wtf_size_t old_edge_index = row_index * edges_per_row_ + new_edge;
        DCHECK_LT(row_index * edges_per_row_ + new_edge, edges_.size());
        edges_[new_edge_index] = edges_[old_edge_index];
      } else {
        edges_[new_edge_index].style = nullptr;
        edges_[new_edge_index].edge_side = EdgeSide::kTop;
      }
      done = new_edge-- == 0;
    } while (!done);
  }
  // Previous loop does not clear out new cells in the first row.
  for (wtf_size_t edge_index = edges_per_row_;
       edge_index < desired_edges_per_row; ++edge_index) {
    edges_[edge_index].style = nullptr;
    edges_[edge_index].edge_side = EdgeSide::kTop;
  }
  edges_per_row_ = desired_edges_per_row;
}

// Block edges are edges between rows.
void NGTableBorders::EnsureCellRowFits(wtf_size_t cell_row) {
  DCHECK_NE(edges_per_row_, 0u);
  wtf_size_t current_block_edges = edges_.size() / edges_per_row_;
  wtf_size_t desired_block_edges = cell_row + 2;
  if (desired_block_edges <= current_block_edges)
    return;
  edges_.resize(desired_block_edges * edges_per_row_);
}

}  // namespace blink
