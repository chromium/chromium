// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_table_painters.h"

#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/ng/ng_link.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_column.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_borders.h"
#include "third_party/blink/renderer/core/paint/background_image_geometry.h"
#include "third_party/blink/renderer/core/paint/box_decoration_data.h"
#include "third_party/blink/renderer/core/paint/box_model_object_painter.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"
#include "third_party/blink/renderer/core/paint/theme_painter.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

namespace {

// NGTableCollapsedEdge represents collapsed border edge for painting.
class NGTableCollapsedEdge {
 public:
  NGTableCollapsedEdge(const NGTableBorders& borders, wtf_size_t edge_index)
      : borders_(borders) {
    edge_index_ = edge_index < borders_.EdgeCount() ? edge_index : UINT_MAX;
    InitCachedProps();
  }
  NGTableCollapsedEdge(const NGTableCollapsedEdge& source, int offset)
      : borders_(source.borders_) {
    // If edge index would have been negative.
    if (offset < 0 && edge_index_ < static_cast<wtf_size_t>(abs(offset))) {
      edge_index_ = UINT_MAX;
      return;
    }
    edge_index_ = source.edge_index_ + offset;
    if (edge_index_ >= borders_.EdgeCount())
      edge_index_ = UINT_MAX;
    InitCachedProps();
  }

  bool Exists() const { return edge_index_ != UINT_MAX; }

  bool CanPaint() const {
    if (!Exists())
      return false;
    if (border_style_ == EBorderStyle::kNone ||
        border_style_ == EBorderStyle::kHidden)
      return false;
    if (border_width_ == 0)
      return false;
    return true;
  }

  EBorderStyle BorderStyle() const { return border_style_; }

  LayoutUnit BorderWidth() const { return border_width_; }

  Color BorderColor() const { return borders_.BorderColor(edge_index_); }

  int CompareBoxOrder(wtf_size_t other_edge_index) const {
    wtf_size_t box_order = borders_.BoxOrder(edge_index_);
    wtf_size_t other_box_order = borders_.BoxOrder(other_edge_index);
    if (box_order < other_box_order)
      return 1;
    if (box_order > other_box_order)
      return -1;
    return 0;
  }

  bool IsInlineAxis() const {
    DCHECK(Exists());
    DCHECK_NE(edge_index_, UINT_MAX);
    return edge_index_ % borders_.EdgesPerRow() % 2 != 0;
  }

  wtf_size_t TableColumn() const {
    DCHECK(Exists());
    return edge_index_ % borders_.EdgesPerRow() / 2;
  }

  wtf_size_t TableRow() const {
    DCHECK(Exists());
    return edge_index_ / borders_.EdgesPerRow();
  }

  // Which edge gets to paint the joint intersection?
  // Returns -1 if this edge wins, 1 if other edge wins, 0 if tie.
  static int CompareForPaint(const NGTableCollapsedEdge& lhs,
                             const NGTableCollapsedEdge& rhs) {
    if (lhs.edge_index_ == rhs.edge_index_)
      return 0;
    bool lhs_paints = lhs.CanPaint();
    bool rhs_paints = rhs.CanPaint();
    if (lhs_paints && rhs_paints) {
      // Compare widths.
      if (lhs.border_width_ > rhs.border_width_) {
        return 1;
      } else if (lhs.border_width_ < rhs.border_width_) {
        return -1;
      } else {  // Compare styles.
        // Paint border style comparison for paint has different
        // rules than for winning edge border (hidden does not win).
        if (lhs.border_style_ == rhs.border_style_)
          return lhs.CompareBoxOrder(rhs.edge_index_);
        if (rhs.border_style_ == EBorderStyle::kHidden)
          return 1;
        if (lhs.border_style_ == EBorderStyle::kHidden)
          return -1;
        if (lhs.border_style_ > rhs.border_style_)
          return 1;
        return -1;
      }
    }
    if (!lhs_paints && !rhs_paints)
      return 0;
    if (!lhs_paints)
      return -1;
    DCHECK(!rhs_paints);
    return 1;
  }

  // Returns logical neighbor edges around edge intersections.
  NGTableCollapsedEdge EdgeBeforeStartIntersection() const {
    if (IsInlineAxis()) {
      return NGTableCollapsedEdge(*this, -2);
    } else {
      return NGTableCollapsedEdge(*this, -1);
    }
  }
  NGTableCollapsedEdge EdgeAfterStartIntersection() const {
    if (IsInlineAxis()) {
      return NGTableCollapsedEdge(*this, 0);
    } else {
      return NGTableCollapsedEdge(*this, 1);
    }
  }
  NGTableCollapsedEdge EdgeOverStartIntersection() const {
    if (IsInlineAxis()) {
      return NGTableCollapsedEdge(*this, -(borders_.EdgesPerRow() + 1));
    } else {
      return NGTableCollapsedEdge(*this, -borders_.EdgesPerRow());
    }
  }
  NGTableCollapsedEdge EdgeUnderStartIntersection() const {
    if (IsInlineAxis()) {
      return NGTableCollapsedEdge(*this, -1);
    } else {
      return NGTableCollapsedEdge(*this, 0);
    }
  }
  NGTableCollapsedEdge EdgeBeforeEndIntersection() const {
    if (IsInlineAxis()) {
      return NGTableCollapsedEdge(*this, 0);
    } else {
      return NGTableCollapsedEdge(*this, borders_.EdgesPerRow() - 1);
    }
  }
  NGTableCollapsedEdge EdgeAfterEndIntersection() const {
    if (IsInlineAxis()) {
      return NGTableCollapsedEdge(*this, 2);
    } else {
      return NGTableCollapsedEdge(*this, borders_.EdgesPerRow() + 1);
    }
  }
  NGTableCollapsedEdge EdgeOverEndIntersection() const {
    if (IsInlineAxis()) {
      return NGTableCollapsedEdge(*this, -(borders_.EdgesPerRow() - 1));
    } else {
      return NGTableCollapsedEdge(*this, 0);
    }
  }
  NGTableCollapsedEdge EdgeUnderEndIntersection() const {
    if (IsInlineAxis()) {
      return NGTableCollapsedEdge(*this, 1);
    } else {
      return NGTableCollapsedEdge(*this, borders_.EdgesPerRow());
    }
  }

  NGTableCollapsedEdge& operator++() {
    DCHECK_NE(edge_index_, UINT_MAX);
    if (++edge_index_ >= borders_.EdgeCount())
      edge_index_ = UINT_MAX;
    InitCachedProps();
    return *this;
  }
  bool operator==(const NGTableCollapsedEdge& rhs) const {
    return edge_index_ == rhs.edge_index_;
  }
  bool operator!=(const NGTableCollapsedEdge& rhs) const {
    return !(*this == rhs);
  }

  NGTableCollapsedEdge& operator=(const NGTableCollapsedEdge& edge) {
    edge_index_ = edge.edge_index_;
    border_width_ = edge.border_width_;
    border_style_ = edge.border_style_;
    return *this;
  }

 private:
  void InitCachedProps() {
    if (edge_index_ == UINT_MAX) {
      border_width_ = LayoutUnit();
      border_style_ = EBorderStyle::kNone;
    } else {
      border_width_ = borders_.BorderWidth(edge_index_);
      border_style_ = borders_.BorderStyle(edge_index_);
    }
  }

  const NGTableBorders& borders_;
  wtf_size_t edge_index_;  // UINT_MAX means end.
  // cache paint properties
  LayoutUnit border_width_;
  EBorderStyle border_style_;
};

// Computes a rectangle for start/end joint.
// start/end_wins is set to true if examined edge won.
// Examined edge should shrink/expand its size to fill the joints.
void ComputeEdgeJoints(const NGTableBorders& collapsed_borders,
                       const NGTableCollapsedEdge& edge,
                       LogicalSize& start_joint,
                       LogicalSize& end_joint,
                       bool& start_wins,
                       bool& end_wins) {
  // Interesting question:
  // Should multiple edges ever paint inside the same joint?
  // - if one edge clearly wins, it should occupy the entire joint.
  // - if edge equals another edge, we have a choice:
  //   a) both edges can win.
  //      If edges are transparent, multiple paint will be visible.
  //   b) pick winners by edge orders. This results in ugly staggered borders.
  //  I've picked a), which is how Legacy does it.

  // Border precedence around the joint. Highest priority is after, then
  // clockwise: after, under, before, over.
  start_wins = false;
  end_wins = false;
  // Find winner for the start of the inline edge.
  NGTableCollapsedEdge before_edge = edge.EdgeBeforeStartIntersection();
  NGTableCollapsedEdge after_edge = edge.EdgeAfterStartIntersection();
  NGTableCollapsedEdge over_edge = edge.EdgeOverStartIntersection();
  NGTableCollapsedEdge under_edge = edge.EdgeUnderStartIntersection();

  int inline_compare =
      NGTableCollapsedEdge::CompareForPaint(before_edge, after_edge);
  start_joint.block_size = inline_compare == 1 ? before_edge.BorderWidth()
                                               : after_edge.BorderWidth();
  // Compare over and under edges.
  int block_compare =
      NGTableCollapsedEdge::CompareForPaint(over_edge, under_edge);
  start_joint.inline_size =
      block_compare == 1 ? over_edge.BorderWidth() : under_edge.BorderWidth();
  int inline_vs_block = NGTableCollapsedEdge::CompareForPaint(
      inline_compare == 1 ? before_edge : after_edge,
      block_compare == 1 ? over_edge : under_edge);

  if (edge.IsInlineAxis()) {
    if (inline_vs_block != -1 && inline_compare != 1)
      start_wins = true;
  } else {
    if (inline_vs_block != 1 && block_compare != 1)
      start_wins = true;
  }
  // Find the winner for the end joint of the inline edge.
  before_edge = edge.EdgeBeforeEndIntersection();
  after_edge = edge.EdgeAfterEndIntersection();
  over_edge = edge.EdgeOverEndIntersection();
  under_edge = edge.EdgeUnderEndIntersection();

  inline_compare =
      NGTableCollapsedEdge::CompareForPaint(before_edge, after_edge);
  end_joint.block_size = inline_compare == 1 ? before_edge.BorderWidth()
                                             : after_edge.BorderWidth();

  block_compare = NGTableCollapsedEdge::CompareForPaint(over_edge, under_edge);
  end_joint.inline_size =
      block_compare == 1 ? over_edge.BorderWidth() : under_edge.BorderWidth();
  inline_vs_block = NGTableCollapsedEdge::CompareForPaint(
      inline_compare == 1 ? before_edge : after_edge,
      block_compare == 1 ? over_edge : under_edge);

  if (edge.IsInlineAxis()) {
    if (inline_vs_block != -1 && inline_compare != -1)
      end_wins = true;
  } else {
    if (inline_vs_block != 1 && block_compare != -1)
      end_wins = true;
  }
}

}  // namespace

void NGTablePainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset,
    const IntRect& visual_rect) {
  const NGTableFragmentData::ColumnGeometries* column_geometries_original =
      fragment_.TableColumnGeometries();
  NGTableFragmentData::ColumnGeometries column_geometries_with_background;
  // Optimization: only traverse colgroups with backgrounds.
  if (column_geometries_original) {
    for (const auto& column_geometry : *column_geometries_original) {
      if (column_geometry.node.Style().HasBoxDecorationBackground()) {
        column_geometries_with_background.push_back(column_geometry);
      }
    }
  }
  BoxDecorationData box_decoration_data(paint_info, fragment_);
  const LayoutNGTable& layout_table =
      *To<LayoutNGTable>(fragment_.GetLayoutObject());
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_table, paint_info.phase))
    return;
  DrawingRecorder recorder(paint_info.context, layout_table, paint_info.phase,
                           visual_rect);

  // BoxDecorationData is not aware of column backgrounds, so
  // there is an explicit check. This will go away once columns
  // start generating their own fragments.
  bool should_paint_background = box_decoration_data.ShouldPaint() ||
                                 column_geometries_with_background.size() != 0;
  if (!should_paint_background)
    return;
  PhysicalRect grid_paint_rect = fragment_.TableGridRect();
  grid_paint_rect.offset += paint_offset;
  NGBoxFragmentPainter(fragment_).PaintBoxDecorationBackgroundWithRectImpl(
      paint_info, grid_paint_rect, box_decoration_data);
  const ComputedStyle& table_style = fragment_.Style();
  if (should_paint_background) {
    // Paint table background on table grid rect.
    // Paint colgroup/col backgrounds.
    if (column_geometries_with_background.size() > 0) {
      NGPhysicalBoxStrut table_borders_padding =
          fragment_.Borders() + fragment_.Padding();

      PhysicalSize border_spacing = ToPhysicalSize(
          layout_table.BorderSpacing(), table_style.GetWritingMode());
      PhysicalSize column_size{
          grid_paint_rect.size.width - table_borders_padding.HorizontalSum() -
              border_spacing.width * 2,
          grid_paint_rect.size.height - table_borders_padding.VerticalSum() -
              border_spacing.height * 2};
      PhysicalOffset column_offset{
          grid_paint_rect.offset.left + table_borders_padding.left +
              border_spacing.width,
          grid_paint_rect.offset.top + table_borders_padding.top +
              border_spacing.height};
      PhysicalRect columns_paint_rect(column_offset, column_size);
      for (const NGLink& child : fragment_.Children()) {
        if (!child.fragment->IsTableNGSection())
          continue;  // child is a caption.
        PhysicalOffset section_offset =
            child.offset - fragment_.TableGridRect().offset;
        section_offset.top -= table_borders_padding.top + border_spacing.height;
        section_offset.left -=
            table_borders_padding.left + border_spacing.width;
        NGTableSectionPainter(To<NGPhysicalBoxFragment>(*child.fragment))
            .PaintColumnsBackground(paint_info, section_offset,
                                    columns_paint_rect,
                                    column_geometries_with_background);
      }
    }
  }
}

void NGTablePainter::PaintCollapsedBorders(const PaintInfo& paint_info,
                                           const PhysicalOffset& paint_offset,
                                           const IntRect& visual_rect) {
  DCHECK_EQ(paint_info.phase, PaintPhase::kForeground);
  const NGTableBorders* collapsed_borders = fragment_.TableCollapsedBorders();
  const NGTableFragmentData::CollapsedBordersGeometry*
      collapsed_borders_geometry = fragment_.TableCollapsedBordersGeometry();
  if (!collapsed_borders)
    return;

  BoxDecorationData box_decoration_data(paint_info, fragment_);
  const LayoutNGTable& layout_table =
      *To<LayoutNGTable>(fragment_.GetLayoutObject());
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_table, paint_info.phase))
    return;
  DrawingRecorder recorder(paint_info.context, layout_table, paint_info.phase,
                           visual_rect);

  PhysicalRect grid_paint_rect = fragment_.TableGridRect();
  grid_paint_rect.offset += paint_offset;

  WritingModeConverter grid_converter(fragment_.Style().GetWritingDirection(),
                                      grid_paint_rect.size);

  for (NGTableCollapsedEdge edge = NGTableCollapsedEdge(*collapsed_borders, 0);
       edge.Exists(); ++edge) {
    if (!edge.CanPaint())
      continue;
    LayoutUnit inline_start;
    LayoutUnit block_start;
    LayoutUnit inline_size;
    LayoutUnit block_size;

    wtf_size_t table_row = edge.TableRow();
    wtf_size_t table_column = edge.TableColumn();
    if (edge.IsInlineAxis()) {
      inline_start = collapsed_borders_geometry->columns[table_column];
      inline_size = collapsed_borders_geometry->columns[table_column + 1] -
                    collapsed_borders_geometry->columns[table_column];
      block_size = edge.BorderWidth();
      block_start =
          collapsed_borders_geometry->rows[table_row] - edge.BorderWidth() / 2;
      LogicalSize start_joint;
      LogicalSize end_joint;
      bool start_wins;
      bool end_wins;
      ComputeEdgeJoints(*collapsed_borders, edge, start_joint, end_joint,
                        start_wins, end_wins);
      if (start_wins) {
        inline_start -= start_joint.inline_size / 2;
        inline_size += start_joint.inline_size / 2;
      } else {
        inline_start += start_joint.inline_size / 2;
        inline_size -= start_joint.inline_size / 2;
      }
      if (end_wins) {
        inline_size += end_joint.inline_size / 2;
      } else {
        inline_size -= end_joint.inline_size / 2;
      }
    } else {  // block_axis
      block_start = collapsed_borders_geometry->rows[table_row];
      block_size =
          collapsed_borders_geometry->rows[table_row + 1] - block_start;
      inline_start = collapsed_borders_geometry->columns[table_column] -
                     edge.BorderWidth() / 2;
      inline_size = edge.BorderWidth();
      LogicalSize start_joint;
      LogicalSize end_joint;
      bool start_wins;
      bool end_wins;
      ComputeEdgeJoints(*collapsed_borders, edge, start_joint, end_joint,
                        start_wins, end_wins);
      if (start_wins) {
        block_start -= start_joint.block_size / 2;
        block_size += start_joint.block_size / 2;
      } else {
        block_start += start_joint.block_size / 2;
        block_size -= start_joint.block_size / 2;
      }
      if (end_wins) {
        block_size += end_joint.block_size / 2;
      } else {
        block_size -= end_joint.block_size / 2;
      }
    }
    const LogicalRect logical_border_rect(inline_start, block_start,
                                          inline_size, block_size);
    PhysicalRect physical_border_rect =
        grid_converter.ToPhysical(logical_border_rect);
    physical_border_rect.offset += grid_paint_rect.offset;

    BoxSide box_side;
    if (IsHorizontalWritingMode(fragment_.Style().GetWritingMode())) {
      box_side = edge.IsInlineAxis() ? BoxSide::kTop : BoxSide::kLeft;
    } else {
      box_side = edge.IsInlineAxis() ? BoxSide::kLeft : BoxSide::kTop;
    }
    ObjectPainter::DrawLineForBoxSide(
        paint_info.context, physical_border_rect.offset.left,
        physical_border_rect.offset.top,
        physical_border_rect.offset.left + physical_border_rect.size.width,
        physical_border_rect.offset.top + physical_border_rect.size.height,
        box_side, edge.BorderColor(), edge.BorderStyle(), 0, 0, true);
  }
}

void NGTableSectionPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset,
    const IntRect& visual_rect) {
  BoxDecorationData box_decoration_data(paint_info, fragment_);
  if (!box_decoration_data.ShouldPaint())
    return;
  const LayoutBox& layout_section = *ToLayoutBox(fragment_.GetLayoutObject());
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_section, paint_info.phase))
    return;
  DrawingRecorder recorder(paint_info.context, layout_section, paint_info.phase,
                           visual_rect);

  PhysicalRect shadow_box{paint_offset, fragment_.Size()};
  if (box_decoration_data.ShouldPaintShadow()) {
    BoxPainterBase::PaintNormalBoxShadow(
        paint_info, shadow_box, fragment_.Style(), PhysicalBoxSides(),
        !box_decoration_data.ShouldPaintBackground());
  }
  for (const NGLink& child : fragment_.Children()) {
    DCHECK(child.fragment->IsBox());
    NGTableRowPainter(To<NGPhysicalBoxFragment>(*child.fragment))
        .PaintTablePartBackgroundIntoCells(
            paint_info, layout_section,
            PhysicalRect(paint_offset, fragment_.Size()), child.offset);
  }
  if (box_decoration_data.ShouldPaintShadow()) {
    BoxPainterBase::PaintInsetBoxShadowWithInnerRect(paint_info, shadow_box,
                                                     fragment_.Style());
  }
}

void NGTableSectionPainter::PaintColumnsBackground(
    const PaintInfo& paint_info,
    const PhysicalOffset& section_offset,
    const PhysicalRect& columns_paint_rect,
    const NGTableFragmentData::ColumnGeometries& column_geometries) {
  for (const NGLink& row : fragment_.Children()) {
    NGTableRowPainter(To<NGPhysicalBoxFragment>(*row.fragment))
        .PaintColumnsBackground(paint_info, section_offset + row.offset,
                                columns_paint_rect, column_geometries);
  }
}

void NGTableRowPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset,
    const IntRect& visual_rect) {
  BoxDecorationData box_decoration_data(paint_info, fragment_);
  if (!box_decoration_data.ShouldPaint())
    return;
  const LayoutBox& layout_row = *ToLayoutBox(fragment_.GetLayoutObject());
  if (DrawingRecorder::UseCachedDrawingIfPossible(paint_info.context,
                                                  layout_row, paint_info.phase))
    return;
  DrawingRecorder recorder(paint_info.context, layout_row, paint_info.phase,
                           visual_rect);

  PhysicalRect shadow_box{paint_offset, fragment_.Size()};
  if (box_decoration_data.ShouldPaintShadow()) {
    BoxPainterBase::PaintNormalBoxShadow(
        paint_info, shadow_box, fragment_.Style(), PhysicalBoxSides(),
        !box_decoration_data.ShouldPaintBackground());
  }

  PaintTablePartBackgroundIntoCells(
      paint_info, layout_row, PhysicalRect(paint_offset, fragment_.Size()),
      PhysicalOffset());
  if (box_decoration_data.ShouldPaintShadow()) {
    BoxPainterBase::PaintInsetBoxShadowWithInnerRect(paint_info, shadow_box,
                                                     fragment_.Style());
  }
}

void NGTableRowPainter::PaintTablePartBackgroundIntoCells(
    const PaintInfo& paint_info,
    const LayoutBox& table_part,
    const PhysicalRect& table_part_paint_rect,
    const PhysicalOffset& row_offset) {
  for (const NGLink& child : fragment_.Children()) {
    DCHECK(child.fragment->IsBox());
    DCHECK(child.fragment->GetLayoutObject()->IsTableCell());
    NGTableCellPainter(To<NGPhysicalBoxFragment>(*child.fragment))
        .PaintBackgroundForTablePart(paint_info, table_part,
                                     table_part_paint_rect,
                                     row_offset + child.offset);
  }
}

void NGTableRowPainter::PaintColumnsBackground(
    const PaintInfo& paint_info,
    const PhysicalOffset& row_offset,
    const PhysicalRect& columns_paint_rect,
    const NGTableFragmentData::ColumnGeometries& column_geometries) {
  wtf_size_t current_column = 0;
  wtf_size_t smallest_viable_column = 0;
  bool is_horizontal =
      IsHorizontalWritingMode(fragment_.Style().GetWritingMode());
  for (const NGLink& child : fragment_.Children()) {
    wtf_size_t cell_column =
        To<NGPhysicalBoxFragment>(child.fragment)->TableCellColumnIndex();
    // if cell is in the column, generate column physical rect
    for (current_column = smallest_viable_column;
         current_column < column_geometries.size(); ++current_column) {
      wtf_size_t current_start = column_geometries[current_column].start_column;
      wtf_size_t current_end = column_geometries[current_column].start_column +
                               column_geometries[current_column].span - 1;
      if (cell_column >= current_start && cell_column <= current_end) {
        PhysicalSize column_size;
        PhysicalOffset column_offset = columns_paint_rect.offset;
        PhysicalOffset cell_offset = row_offset + child.offset;
        if (is_horizontal) {
          column_size =
              PhysicalSize(column_geometries[current_column].inline_size,
                           columns_paint_rect.size.height);
          column_offset.left += column_geometries[current_column].inline_offset;
          cell_offset.left -= column_geometries[current_column].inline_offset;
        } else {
          column_size =
              PhysicalSize(columns_paint_rect.size.width,
                           column_geometries[current_column].inline_size);
          column_offset.top += column_geometries[current_column].inline_offset;
          cell_offset.top -= column_geometries[current_column].inline_offset;
        }
        NGTableCellPainter(To<NGPhysicalBoxFragment>(*child.fragment))
            .PaintBackgroundForTablePart(
                paint_info,
                *column_geometries[current_column].node.GetLayoutBox(),
                PhysicalRect(column_offset, column_size), cell_offset);
      }
    }
    // Optimization: no more columns to paint.
    if (smallest_viable_column == column_geometries.size())
      break;
  }
}

void NGTableCellPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset,
    const IntRect& visual_rect) {
  BoxDecorationData box_decoration_data(paint_info, fragment_);

  if (!box_decoration_data.ShouldPaint())
    return;
  const LayoutObject* layout_cell = fragment_.GetLayoutObject();
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, *layout_cell,
          DisplayItem::kBoxDecorationBackground))
    return;

  DrawingRecorder recorder(paint_info.context, *layout_cell,
                           DisplayItem::kBoxDecorationBackground, visual_rect);

  PhysicalRect paint_rect(paint_offset, fragment_.Size());

  const NGPaintFragment* paint_fragment =
      To<LayoutNGTableCell>(fragment_.GetLayoutObject())->PaintFragment();
  if (paint_fragment) {
    NGBoxFragmentPainter(*paint_fragment)
        .PaintBoxDecorationBackgroundWithRectImpl(paint_info, paint_rect,
                                                  box_decoration_data);
  } else {
    NGBoxFragmentPainter(fragment_).PaintBoxDecorationBackgroundWithRectImpl(
        paint_info, paint_rect, box_decoration_data);
  }
}

// Inspired by TableCellPainter::PaintBackground.
void NGTableCellPainter::PaintBackgroundForTablePart(
    const PaintInfo& paint_info,
    const LayoutBox& table_part,
    const PhysicalRect& table_part_paint_rect,
    const PhysicalOffset& table_cell_offset) {
  if (fragment_.Style().Visibility() == EVisibility::kHidden)
    return;
  const LayoutNGTableCell& layout_table_cell =
      *To<LayoutNGTableCell>(fragment_.GetLayoutObject());
  if (layout_table_cell.BackgroundTransfersToView())
    return;  // cargo-culted from other painters.

  Color color = table_part.StyleRef().VisitedDependentColor(
      GetCSSPropertyBackgroundColor());
  const FillLayer& background_layers = table_part.StyleRef().BackgroundLayers();
  if (background_layers.AnyLayerHasImage() || color.Alpha()) {
    BackgroundImageGeometry geometry(layout_table_cell, table_cell_offset,
                                     table_part, table_part_paint_rect.size);
    PhysicalRect cell_rect(table_part_paint_rect.offset + table_cell_offset,
                           fragment_.Size());

    bool should_clip =
        table_part.HasLayer() && table_part.IsTableRow() &&
        layout_table_cell.Parent()->Parent()->StyleRef().BorderCollapse() ==
            EBorderCollapse::kCollapse;
    GraphicsContextStateSaver state_saver(paint_info.context, should_clip);
    if (should_clip) {
      // Clipping does not pixel-snap correctly.
      // If we clip too much, background is not painted.
      // If we clip too little, borders are painted over by background.
      // FF does not clip at all, that is another option.
      //
      // When table part has a paint layer, it will paint after collapsed
      // borders are painted.
      // To avoid painting over the collapsed borders, background is clipped.
      // Because pixel snapping does not guarantee correct snapping between
      // different layers, borders might still disappear, or not enough
      // background will be painted.
      // This happens less often in Legacy layout, because it snaps column
      // locations during layout.
      PhysicalRect clip_rect = cell_rect;
      paint_info.context.Clip(FloatRect(clip_rect));
    }
    BoxModelObjectPainter(layout_table_cell)
        .PaintFillLayers(paint_info, color, background_layers, cell_rect,
                         geometry);
  }
}

}  // namespace blink
