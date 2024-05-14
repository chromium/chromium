// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/table/table_borders.h"

#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_column_visitor.h"
#include "third_party/blink/renderer/core/layout/table/table_layout_algorithm_types.h"
#include "third_party/blink/renderer/core/layout/table/table_layout_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

// https://www.w3.org/TR/css-tables-3/#conflict-resolution-for-collapsed-borders
bool IsSourceMoreSpecificThanEdge(EBorderStyle source_style,
                                  LayoutUnit source_width,
                                  const TableBorders::Edge& edge) {
  if (edge.edge_side == TableBorders::EdgeSide::kDoNotFill) {
    return false;
  }

  if (!edge.style || source_style == EBorderStyle::kHidden)
    return true;

  EBorderStyle edge_border_style =
      TableBorders::BorderStyle(edge.style.Get(), edge.edge_side);
  if (edge_border_style == EBorderStyle::kHidden)
    return false;

  LayoutUnit edge_width =
      TableBorders::BorderWidth(edge.style.Get(), edge.edge_side);
  if (source_width < edge_width)
    return false;
  if (source_width > edge_width)
    return true;
  return source_style > edge_border_style;
}

class ColBordersMarker {
  STACK_ALLOCATED();

 public:
  void VisitCol(const LayoutInputNode& column,
                wtf_size_t start_column_index,
                wtf_size_t span) {
    for (wtf_size_t i = 0; i < span; ++i) {
      wtf_size_t current_column_index = start_column_index + i;
      borders.MergeBorders(0, current_column_index, table_row_count, 1,
                           column.Style(), TableBorders::EdgeSource::kColumn,
                           box_order, table_writing_direction);
    }
  }
  void EnterColgroup(const LayoutInputNode& colgroup,
                     wtf_size_t start_column_index) {}
  void LeaveColgroup(const LayoutInputNode& colgroup,
                     wtf_size_t start_column_index,
                     wtf_size_t span,
                     bool has_children) {}
  ColBordersMarker(wtf_size_t table_row_count,
                   wtf_size_t box_order,
                   WritingDirectionMode table_writing_direction,
                   TableBorders& borders)
      : table_row_count(table_row_count),
        box_order(box_order),
        table_writing_direction(table_writing_direction),
        borders(borders) {}
  const wtf_size_t table_row_count;
  const wtf_size_t box_order;
  const WritingDirectionMode table_writing_direction;
  TableBorders& borders;
};

class ColgroupBordersMarker {
  STACK_ALLOCATED();

 public:
  void VisitCol(const LayoutInputNode& column,
                wtf_size_t start_column_index,
                wtf_size_t span) {}
  void EnterColgroup(const LayoutInputNode& colgroup,
                     wtf_size_t start_column_index) {}
  void LeaveColgroup(const LayoutInputNode& colgroup,
                     wtf_size_t start_column_index,
                     wtf_size_t span,
                     bool has_children) {
    borders.MergeBorders(0, start_column_index, table_row_count, span,
                         colgroup.Style(), TableBorders::EdgeSource::kColumn,
                         box_order, table_writing_direction);
  }
  ColgroupBordersMarker(wtf_size_t table_row_count,
                        wtf_size_t box_order,
                        WritingDirectionMode table_writing_direction,
                        TableBorders& borders)
      : table_row_count(table_row_count),
        box_order(box_order),
        table_writing_direction(table_writing_direction),
        borders(borders) {}
  const wtf_size_t table_row_count;
  const wtf_size_t box_order;
  const WritingDirectionMode table_writing_direction;
  TableBorders& borders;
};

}  // namespace

const TableBorders* TableBorders::ComputeTableBorders(const BlockNode& table) {
  const ComputedStyle& table_style = table.Style();
  const bool is_collapsed =
      table_style.BorderCollapse() == EBorderCollapse::kCollapse;
  TableBorders* table_borders = MakeGarbageCollected<TableBorders>(
      ComputeNonCollapsedTableBorders(table_style), is_collapsed);

  if (!is_collapsed)
    return table_borders;

  TableGroupedChildren grouped_children(table);
  WritingDirectionMode table_writing_direction =
      table.Style().GetWritingDirection();

  wtf_size_t box_order = 0;
  wtf_size_t table_column_count = ComputeMaximumNonMergeableColumnCount(
      grouped_children.columns, table.Style().IsFixedTableLayout());
  wtf_size_t table_row_index = 0;
  // Mark cell borders.
  bool found_multispan_cells = false;
  for (const BlockNode section : grouped_children) {
    wtf_size_t section_start_row = table_row_index;
    ColspanCellTabulator tabulator;
    for (BlockNode row = To<BlockNode>(section.FirstChild()); row;
         row = To<BlockNode>(row.NextSibling())) {
      tabulator.StartRow();
      for (BlockNode cell = To<BlockNode>(row.FirstChild()); cell;
           cell = To<BlockNode>(cell.NextSibling())) {
        tabulator.FindNextFreeColumn();
        wtf_size_t cell_colspan = cell.TableCellColspan();
        found_multispan_cells |=
            cell.TableCellRowspan() > 1 || cell_colspan > 1;
        // Rowspan has to be limited by section size. Since we do not know
        // section size, we have to rerun cell distribution with limited
        // rowspans.
        table_column_count =
            std::max(table_column_count,
                     ComputeMaxColumn(tabulator.CurrentColumn(), cell_colspan,
                                      table.Style().IsFixedTableLayout()));
        if (!found_multispan_cells) {
          table_borders->MergeBorders(
              table_row_index, tabulator.CurrentColumn(),
              cell.TableCellRowspan(), cell_colspan, cell.Style(),
              TableBorders::EdgeSource::kCell, ++box_order,
              table_writing_direction);
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
    for (BlockNode section : grouped_children) {
      ColspanCellTabulator tabulator;
      for (BlockNode row = To<BlockNode>(section.FirstChild()); row;
           row = To<BlockNode>(row.NextSibling())) {
        tabulator.StartRow();
        for (BlockNode cell = To<BlockNode>(row.FirstChild()); cell;
             cell = To<BlockNode>(cell.NextSibling())) {
          tabulator.FindNextFreeColumn();
          table_borders->MergeBorders(
              table_row_index, tabulator.CurrentColumn(),
              cell.TableCellRowspan(), cell.TableCellColspan(), cell.Style(),
              TableBorders::EdgeSource::kCell, ++box_order,
              table_writing_direction, section_index);
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
  for (BlockNode section : grouped_children) {
    for (BlockNode row = To<BlockNode>(section.FirstChild()); row;
         row = To<BlockNode>(row.NextSibling())) {
      table_borders->MergeBorders(table_row_index, 0, 1, table_column_count,
                                  row.Style(), TableBorders::EdgeSource::kRow,
                                  ++box_order, table_writing_direction);
      ++table_row_index;
    }
  }

  // Mark section borders.
  // It is tempting to traverse sections at the same time as rows,
  // but it would cause precedence errors.
  wtf_size_t section_index = 0;
  for (BlockNode section : grouped_children) {
    TableBorders::Section section_info =
        table_borders->GetSection(section_index);
    table_borders->MergeBorders(
        section_info.start_row, 0, section_info.row_count, table_column_count,
        section.Style(), TableBorders::EdgeSource::kSection, ++box_order,
        table_writing_direction);
    ++section_index;
  }

  // Mark column borders.
  // COL borders have precedence over COLGROUP borders.
  // We have to traverse COL first, then COLGROUP.
  ColBordersMarker col_borders_marker(table_row_count, ++box_order,
                                      table_writing_direction, *table_borders);
  VisitLayoutTableColumn(
      const_cast<HeapVector<BlockNode>&>(grouped_children.columns),
      table_column_count, &col_borders_marker);
  ColgroupBordersMarker colgroup_borders_marker(
      table_row_count, ++box_order, table_writing_direction, *table_borders);
  VisitLayoutTableColumn(
      const_cast<HeapVector<BlockNode>&>(grouped_children.columns),
      table_column_count, &colgroup_borders_marker);

  // Mark table borders.
  table_borders->MergeBorders(0, 0, table_row_count, table_column_count,
                              table_style, TableBorders::EdgeSource::kTable,
                              ++box_order, table_writing_direction);

  table_borders->UpdateTableBorder(table_row_count, table_column_count);

  return table_borders;
}

TableBorders::TableBorders(const BoxStrut& table_border,
                           const bool is_collapsed)
    : table_border_(table_border), is_collapsed_(is_collapsed) {}

Color TableBorders::BorderColor(const ComputedStyle* style,
                                EdgeSide edge_side) {
  switch (edge_side) {
    case EdgeSide::kLeft:
      return style->VisitedDependentColor(GetCSSPropertyBorderLeftColor());
    case EdgeSide::kRight:
      return style->VisitedDependentColor(GetCSSPropertyBorderRightColor());
    case EdgeSide::kTop:
      return style->VisitedDependentColor(GetCSSPropertyBorderTopColor());
    case EdgeSide::kBottom:
      return style->VisitedDependentColor(GetCSSPropertyBorderBottomColor());
    case EdgeSide::kDoNotFill:
      NOTREACHED_IN_MIGRATION();
      return style->VisitedDependentColor(GetCSSPropertyBorderBottomColor());
  }
}

#if DCHECK_IS_ON()
String TableBorders::DumpEdges() {
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

void TableBorders::ShowEdges() {
  LOG(INFO) << "\n" << DumpEdges().Utf8();
}

bool TableBorders::operator==(const TableBorders& other) const {
  // Compare by traversal, because we must call edge comparison function.
  if (edges_.size() != other.edges_.size())
    return false;
  for (unsigned i = 0; i < edges_.size(); i++) {
    if (edges_[i].edge_side != other.edges_[i].edge_side)
      return false;
    if (edges_[i].box_order != other.edges_[i].box_order)
      return false;
  }
  return sections_ == other.sections_ &&
         edges_per_row_ == other.edges_per_row_ &&
         table_border_ == other.table_border_ &&
         last_column_index_ == other.last_column_index_ &&
         is_collapsed_ == other.is_collapsed_;
}

#endif

BoxStrut TableBorders::GetCellBorders(wtf_size_t row,
                                      wtf_size_t column,
                                      wtf_size_t rowspan,
                                      wtf_size_t colspan) const {
  BoxStrut border_strut;
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
        std::max(border_strut.inline_start, CanPaint(start_edge_index)
                                                ? BorderWidth(start_edge_index)
                                                : LayoutUnit());
    if (start_edge_index >= edges_.size())
      break;
    wtf_size_t end_edge_index = first_inline_end_edge + i * edges_per_row_;
    border_strut.inline_end = std::max(
        border_strut.inline_end,
        CanPaint(end_edge_index) ? BorderWidth(end_edge_index) : LayoutUnit());
  }
  // Compute block border widths.
  wtf_size_t start_edge_column_index = column * 2 + 1;
  for (wtf_size_t i = 0; i < colspan; ++i) {
    wtf_size_t current_column_index = start_edge_column_index + i * 2;
    if (current_column_index >= edges_per_row_)
      break;
    wtf_size_t start_edge_index = row * edges_per_row_ + current_column_index;
    border_strut.block_start =
        std::max(border_strut.block_start, CanPaint(start_edge_index)
                                               ? BorderWidth(start_edge_index)
                                               : LayoutUnit());
    wtf_size_t end_edge_index = start_edge_index + rowspan * edges_per_row_;
    border_strut.block_end = std::max(
        border_strut.block_end,
        CanPaint(end_edge_index) ? BorderWidth(end_edge_index) : LayoutUnit());
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

void TableBorders::UpdateTableBorder(wtf_size_t table_row_count,
                                     wtf_size_t table_column_count) {
  DCHECK(is_collapsed_);
  if (edges_per_row_ == 0) {
    table_border_ = BoxStrut();
    return;
  }
  DCHECK_GE((table_column_count + 1) * 2, edges_per_row_);
  table_border_ = GetCellBorders(0, 0, table_row_count, table_column_count);
}

BoxStrut TableBorders::CellBorder(
    const BlockNode& cell,
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
      ConstraintSpaceBuilder(table_writing_direction.GetWritingMode(),
                             table_writing_direction, /* is_new_fc */ false)
          .ToConstraintSpace(),
      cell);
}

// As we are determining the intrinsic size of the table at this stage,
// %-padding resolves against an indefinite size.
BoxStrut TableBorders::CellPaddingForMeasure(
    const ComputedStyle& cell_style,
    WritingDirectionMode table_writing_direction) const {
  if (!cell_style.MayHavePadding())
    return BoxStrut();
  return ComputePadding(
      ConstraintSpaceBuilder(table_writing_direction.GetWritingMode(),
                             table_writing_direction,
                             /* is_new_fc */ false)
          .ToConstraintSpace(),
      cell_style);
}

void TableBorders::MergeBorders(wtf_size_t cell_start_row,
                                wtf_size_t cell_start_column,
                                wtf_size_t rowspan,
                                wtf_size_t colspan,
                                const ComputedStyle& source_style,
                                EdgeSource source,
                                const wtf_size_t box_order,
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
        table_writing_direction, source_style.BorderTopStyle(),
        source_style.BorderRightStyle(), source_style.BorderBottomStyle(),
        source_style.BorderLeftStyle());
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

  PhysicalToLogical<EdgeSide> edge_side(table_writing_direction, EdgeSide::kTop,
                                        EdgeSide::kRight, EdgeSide::kBottom,
                                        EdgeSide::kLeft);
  MergeRowAxisBorder(cell_start_row, cell_start_column, clamped_colspan,
                     source_style, box_order, edge_side.BlockStart());
  MergeRowAxisBorder(cell_start_row + clamped_rowspan, cell_start_column,
                     clamped_colspan, source_style, box_order,
                     edge_side.BlockEnd());
  MergeColumnAxisBorder(cell_start_row, cell_start_column, clamped_rowspan,
                        source_style, box_order, edge_side.InlineStart());
  MergeColumnAxisBorder(cell_start_row, cell_start_column + clamped_colspan,
                        clamped_rowspan, source_style, box_order,
                        edge_side.InlineEnd());
  if (mark_inner_borders) {
    MarkInnerBordersAsDoNotFill(cell_start_row, cell_start_column,
                                clamped_rowspan, clamped_colspan);
  }
}

void TableBorders::MergeRowAxisBorder(wtf_size_t start_row,
                                      wtf_size_t start_column,
                                      wtf_size_t colspan,
                                      const ComputedStyle& source_style,
                                      const wtf_size_t box_order,
                                      EdgeSide physical_side) {
  EBorderStyle source_border_style = BorderStyle(&source_style, physical_side);
  if (source_border_style == EBorderStyle::kNone)
    return;
  LayoutUnit source_border_width = BorderWidth(&source_style, physical_side);
  wtf_size_t start_edge = edges_per_row_ * start_row + start_column * 2 + 1;
  wtf_size_t end_edge = start_edge + colspan * 2;
  for (wtf_size_t current_edge = start_edge; current_edge < end_edge;
       current_edge += 2) {
    // https://www.w3.org/TR/css-tables-3/#border-specificity
    if (IsSourceMoreSpecificThanEdge(source_border_style, source_border_width,
                                     edges_[current_edge])) {
      edges_[current_edge].style = &source_style;
      edges_[current_edge].edge_side = physical_side;
      edges_[current_edge].box_order = box_order;
    }
  }
}

void TableBorders::MergeColumnAxisBorder(wtf_size_t start_row,
                                         wtf_size_t start_column,
                                         wtf_size_t rowspan,
                                         const ComputedStyle& source_style,
                                         const wtf_size_t box_order,
                                         EdgeSide physical_side) {
  EBorderStyle source_border_style = BorderStyle(&source_style, physical_side);
  if (source_border_style == EBorderStyle::kNone)
    return;
  LayoutUnit source_border_width = BorderWidth(&source_style, physical_side);
  wtf_size_t start_edge = edges_per_row_ * start_row + start_column * 2;
  wtf_size_t end_edge = start_edge + (rowspan * edges_per_row_);
  for (wtf_size_t current_edge = start_edge; current_edge < end_edge;
       current_edge += edges_per_row_) {
    // https://www.w3.org/TR/css-tables-3/#border-specificity
    if (IsSourceMoreSpecificThanEdge(source_border_style, source_border_width,
                                     edges_[current_edge])) {
      edges_[current_edge].style = &source_style;
      edges_[current_edge].edge_side = physical_side;
      edges_[current_edge].box_order = box_order;
    }
  }
}

// Rowspanned/colspanned cells need to mark inner edges as do-not-fill to
// prevent tables parts from drawing into them.
void TableBorders::MarkInnerBordersAsDoNotFill(wtf_size_t start_row,
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
void TableBorders::EnsureCellColumnFits(wtf_size_t cell_column) {
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
void TableBorders::EnsureCellRowFits(wtf_size_t cell_row) {
  DCHECK_NE(edges_per_row_, 0u);
  wtf_size_t current_block_edges = edges_.size() / edges_per_row_;
  wtf_size_t desired_block_edges = cell_row + 2;
  if (desired_block_edges <= current_block_edges)
    return;
  edges_.resize(desired_block_edges * edges_per_row_);
}

}  // namespace blink
