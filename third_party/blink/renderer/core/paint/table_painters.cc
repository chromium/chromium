// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/table_painters.h"

#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_fragment_link.h"
#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_column.h"
#include "third_party/blink/renderer/core/layout/table/table_borders.h"
#include "third_party/blink/renderer/core/paint/box_background_paint_context.h"
#include "third_party/blink/renderer/core/paint/box_border_painter.h"
#include "third_party/blink/renderer/core/paint/box_decoration_data.h"
#include "third_party/blink/renderer/core/paint/box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/box_model_object_painter.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"
#include "third_party/blink/renderer/core/paint/theme_painter.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

namespace {

// TableCollapsedEdge represents collapsed border edge for painting.
class TableCollapsedEdge {
  STACK_ALLOCATED();

 public:
  TableCollapsedEdge(const TableBorders& borders, wtf_size_t edge_index)
      : borders_(borders) {
    edge_index_ = edge_index < borders_.EdgeCount() ? edge_index : UINT_MAX;
    InitCachedProps();
  }
  TableCollapsedEdge(const TableCollapsedEdge& source, int offset)
      : borders_(source.borders_) {
    // If edge index would have been negative.
    if (offset < 0 &&
        source.edge_index_ < static_cast<wtf_size_t>(std::abs(offset))) {
      edge_index_ = UINT_MAX;
    } else {
      edge_index_ = source.edge_index_ + offset;
      if (edge_index_ >= borders_.EdgeCount())
        edge_index_ = UINT_MAX;
    }
    InitCachedProps();
  }

  TableCollapsedEdge(const TableCollapsedEdge& edge)
      : TableCollapsedEdge(edge, 0) {}

  TableCollapsedEdge& operator=(const TableCollapsedEdge& edge) {
    edge_index_ = edge.edge_index_;
    border_width_ = edge.border_width_;
    border_style_ = edge.border_style_;
    return *this;
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
  static int CompareForPaint(const TableCollapsedEdge& lhs,
                             const TableCollapsedEdge& rhs) {
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
  TableCollapsedEdge EdgeBeforeStartIntersection() const {
    if (IsInlineAxis()) {
      return TableCollapsedEdge(*this, -2);
    } else {
      return TableCollapsedEdge(*this, -1);
    }
  }
  TableCollapsedEdge EdgeAfterStartIntersection() const {
    if (IsInlineAxis()) {
      return TableCollapsedEdge(*this, 0);
    } else {
      return TableCollapsedEdge(*this, 1);
    }
  }
  TableCollapsedEdge EdgeOverStartIntersection() const {
    if (IsInlineAxis()) {
      return TableCollapsedEdge(*this, -(borders_.EdgesPerRow() + 1));
    } else {
      return TableCollapsedEdge(*this, -borders_.EdgesPerRow());
    }
  }
  TableCollapsedEdge EdgeUnderStartIntersection() const {
    if (IsInlineAxis()) {
      return TableCollapsedEdge(*this, -1);
    } else {
      return TableCollapsedEdge(*this, 0);
    }
  }
  TableCollapsedEdge EdgeBeforeEndIntersection() const {
    if (IsInlineAxis()) {
      return TableCollapsedEdge(*this, 0);
    } else {
      return TableCollapsedEdge(*this, borders_.EdgesPerRow() - 1);
    }
  }
  TableCollapsedEdge EdgeAfterEndIntersection() const {
    if (IsInlineAxis()) {
      return TableCollapsedEdge(*this, 2);
    } else {
      return TableCollapsedEdge(*this, borders_.EdgesPerRow() + 1);
    }
  }
  TableCollapsedEdge EdgeOverEndIntersection() const {
    if (IsInlineAxis()) {
      return TableCollapsedEdge(*this, -(borders_.EdgesPerRow() - 1));
    } else {
      return TableCollapsedEdge(*this, 0);
    }
  }
  TableCollapsedEdge EdgeUnderEndIntersection() const {
    if (IsInlineAxis()) {
      return TableCollapsedEdge(*this, 1);
    } else {
      return TableCollapsedEdge(*this, borders_.EdgesPerRow());
    }
  }
  TableCollapsedEdge EmptyEdge() const {
    return TableCollapsedEdge(borders_, UINT_MAX);
  }

  TableCollapsedEdge& operator++() {
    DCHECK_NE(edge_index_, UINT_MAX);
    if (++edge_index_ >= borders_.EdgeCount())
      edge_index_ = UINT_MAX;
    InitCachedProps();
    return *this;
  }
  bool operator==(const TableCollapsedEdge& rhs) const {
    return edge_index_ == rhs.edge_index_;
  }
  bool operator!=(const TableCollapsedEdge& rhs) const {
    return !(*this == rhs);
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

  const TableBorders& borders_;
  wtf_size_t edge_index_;  // UINT_MAX means end.
  // cache paint properties
  LayoutUnit border_width_;
  EBorderStyle border_style_;
};

// Computes a rectangle for start/end joint.
// start/end_wins is set to true if examined edge won.
// Examined edge should shrink/expand its size to fill the joints.
void ComputeEdgeJoints(const TableBorders& collapsed_borders,
                       const TableCollapsedEdge& edge,
                       bool is_over_edge_fragmentation_boundary,
                       bool is_under_edge_fragmentation_boundary,
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
  TableCollapsedEdge before_edge = edge.EdgeBeforeStartIntersection();
  TableCollapsedEdge after_edge = edge.EdgeAfterStartIntersection();
  TableCollapsedEdge over_edge = is_over_edge_fragmentation_boundary
                                     ? edge.EmptyEdge()
                                     : edge.EdgeOverStartIntersection();
  TableCollapsedEdge under_edge =
      is_under_edge_fragmentation_boundary && edge.IsInlineAxis()
          ? edge.EmptyEdge()
          : edge.EdgeUnderStartIntersection();

  int inline_compare =
      TableCollapsedEdge::CompareForPaint(before_edge, after_edge);
  start_joint.block_size = inline_compare == 1 ? before_edge.BorderWidth()
                                               : after_edge.BorderWidth();
  if (is_over_edge_fragmentation_boundary ||
      (is_under_edge_fragmentation_boundary && edge.IsInlineAxis())) {
    start_joint.block_size = LayoutUnit();
  }

  // Compare over and under edges.
  int block_compare =
      TableCollapsedEdge::CompareForPaint(over_edge, under_edge);
  start_joint.inline_size =
      block_compare == 1 ? over_edge.BorderWidth() : under_edge.BorderWidth();
  int inline_vs_block = TableCollapsedEdge::CompareForPaint(
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
  over_edge = is_over_edge_fragmentation_boundary && edge.IsInlineAxis()
                  ? edge.EmptyEdge()
                  : edge.EdgeOverEndIntersection();
  under_edge = is_under_edge_fragmentation_boundary
                   ? edge.EmptyEdge()
                   : edge.EdgeUnderEndIntersection();

  inline_compare = TableCollapsedEdge::CompareForPaint(before_edge, after_edge);
  end_joint.block_size = inline_compare == 1 ? before_edge.BorderWidth()
                                             : after_edge.BorderWidth();
  if ((is_over_edge_fragmentation_boundary && edge.IsInlineAxis()) ||
      is_under_edge_fragmentation_boundary) {
    end_joint.block_size = LayoutUnit();
  }

  block_compare = TableCollapsedEdge::CompareForPaint(over_edge, under_edge);
  end_joint.inline_size =
      block_compare == 1 ? over_edge.BorderWidth() : under_edge.BorderWidth();
  inline_vs_block = TableCollapsedEdge::CompareForPaint(
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

// Computes the stitched columns-rect relative to the current fragment.
// The columns-rect is the union of all the sections in the table.
PhysicalRect ComputeColumnsRect(const PhysicalBoxFragment& fragment) {
  const auto writing_direction = fragment.Style().GetWritingDirection();
  LogicalRect columns_rect;
  LayoutUnit stitched_block_size;
  LayoutUnit fragment_block_offset;

  bool is_first_section = true;
  for (const PhysicalBoxFragment& walker :
       To<LayoutBox>(fragment.GetLayoutObject())->PhysicalFragments()) {
    if (&walker == &fragment)
      fragment_block_offset = stitched_block_size;

    WritingModeConverter converter(writing_direction, walker.Size());
    for (const auto& child : walker.Children()) {
      if (!child->IsTableSection()) {
        continue;
      }

      LogicalRect section_rect =
          converter.ToLogical({child.offset, child->Size()});
      section_rect.offset.block_offset += stitched_block_size;

      if (is_first_section) {
        columns_rect = section_rect;
        is_first_section = false;
      } else {
        columns_rect.UniteEvenIfEmpty(section_rect);
      }
    }

    stitched_block_size +=
        LogicalFragment(writing_direction, walker).BlockSize();
  }

  // Make the rect relative to the fragment we are currently painting.
  columns_rect.offset.block_offset -= fragment_block_offset;

  WritingModeConverter converter(writing_direction, fragment.Size());
  return converter.ToPhysical(columns_rect);
}

// When painting background in a cell (for the cell or its ancestor table part),
// if any ancestor table part has a layer and the table collapses borders, the
// background is painted after the collapsed borders. We need to clip the
// background to prevent it from covering the collapsed borders around the cell.
// TODO(crbug.com/1181813): Investigate other methods.
class TableCellBackgroundClipper {
  STACK_ALLOCATED();

 public:
  TableCellBackgroundClipper(
      GraphicsContext& context,
      const LayoutTableCell& table_cell,
      const PhysicalRect& cell_rect,
      bool is_painting_background_in_contents_space = false)
      : context_(context),
        needs_clip_(!is_painting_background_in_contents_space &&
                    (table_cell.HasLayer() || table_cell.Parent()->HasLayer() ||
                     table_cell.Parent()->Parent()->HasLayer()) &&
                    table_cell.Table()->HasCollapsedBorders()) {
    if (!needs_clip_)
      return;

    PhysicalRect clip_rect = cell_rect;
    clip_rect.Contract(table_cell.BorderOutsets());
    context.Save();
    context.Clip(ToPixelSnappedRect(clip_rect));
  }

  ~TableCellBackgroundClipper() {
    if (needs_clip_)
      context_.Restore();
  }

 private:
  GraphicsContext& context_;
  bool needs_clip_;
};

}  // namespace

bool TablePainter::WillCheckColumnBackgrounds() {
  return fragment_.TableColumnGeometries();
}

void TablePainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const PhysicalRect& paint_rect,
    const BoxDecorationData& box_decoration_data) {
  WritingModeConverter converter(fragment_.Style().GetWritingDirection(),
                                 fragment_.Size());
  PhysicalRect grid_paint_rect =
      converter.ToPhysical(fragment_.TableGridRect());
  grid_paint_rect.offset += paint_rect.offset;

  // Paint the table background on the grid-rect.
  if (box_decoration_data.ShouldPaint()) {
    BoxFragmentPainter(fragment_).PaintBoxDecorationBackgroundWithRectImpl(
        paint_info, grid_paint_rect, box_decoration_data);
  }

  // Optimization: only traverse colgroups with backgrounds.
  const TableFragmentData::ColumnGeometries* column_geometries_original =
      fragment_.TableColumnGeometries();
  TableFragmentData::ColumnGeometries column_geometries_with_background;
  if (column_geometries_original) {
    for (const auto& column_geometry : *column_geometries_original) {
      if (column_geometry.node.Style().HasBoxDecorationBackground()) {
        column_geometries_with_background.push_back(column_geometry);
      }
    }
  }

  if (column_geometries_with_background.empty())
    return;

  // Paint <colgroup>/<col> backgrounds.
  PhysicalRect columns_paint_rect = ComputeColumnsRect(fragment_);
  columns_paint_rect.offset += paint_rect.offset;
  for (const PhysicalFragmentLink& child : fragment_.Children()) {
    if (!child.fragment->IsTableSection()) {
      continue;
    }
    TableSectionPainter(To<PhysicalBoxFragment>(*child.fragment))
        .PaintColumnsBackground(paint_info, paint_rect.offset + child.offset,
                                columns_paint_rect,
                                column_geometries_with_background);
  }
}

namespace {

const PhysicalFragment* StartSection(const PhysicalBoxFragment& table) {
  for (const auto& child : table.Children()) {
    if (!child->IsTableSection()) {
      continue;
    }
    return child.get();
  }
  return nullptr;
}

const PhysicalFragment* EndSection(const PhysicalBoxFragment& table) {
  const auto children = table.Children();
  for (auto it = children.rbegin(); it != children.rend(); ++it) {
    const auto& child = *it;
    if (!child->IsTableSection()) {
      continue;
    }
    return child.get();
  }
  return nullptr;
}

bool IsStartRowFragmented(const PhysicalBoxFragment& section) {
  for (const auto& child : section.Children()) {
    if (!child->IsTableRow()) {
      continue;
    }

    return IsBreakInside(
        FindPreviousBreakToken(To<PhysicalBoxFragment>(*child)));
  }

  return false;
}

bool IsEndRowFragmented(const PhysicalBoxFragment& section) {
  const auto children = section.Children();
  for (auto it = children.rbegin(); it != children.rend(); ++it) {
    const auto& child = *it;
    if (!child->IsTableRow()) {
      continue;
    }
    const auto* break_token = To<BlockBreakToken>(child->GetBreakToken());
    return IsBreakInside(break_token) && !break_token->IsAtBlockEnd();
  }
  return false;
}

}  // namespace

void TablePainter::PaintCollapsedBorders(const PaintInfo& paint_info,
                                         const PhysicalOffset& paint_offset,
                                         const gfx::Rect& visual_rect) {
  const TableBorders* collapsed_borders = fragment_.TableCollapsedBorders();
  if (!collapsed_borders)
    return;
  const TableFragmentData::CollapsedBordersGeometry*
      collapsed_borders_geometry = fragment_.TableCollapsedBordersGeometry();
  CHECK(collapsed_borders_geometry);

  const auto& layout_table = *To<LayoutTable>(fragment_.GetLayoutObject());
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_table, paint_info.phase))
    return;
  DrawingRecorder recorder(paint_info.context, layout_table, paint_info.phase,
                           visual_rect);
  AutoDarkMode auto_dark_mode(PaintAutoDarkMode(
      fragment_.Style(), DarkModeFilter::ElementRole::kBorder));

  const wtf_size_t edges_per_row = collapsed_borders->EdgesPerRow();
  const wtf_size_t total_row_count =
      collapsed_borders->EdgeCount() / edges_per_row;

  const auto* start_section = StartSection(fragment_);
  const auto* end_section = EndSection(fragment_);

  // We paint collapsed-borders section-by-section for fragmentation purposes.
  // This means that we need to track the final row we've painted in each
  // section to avoid double painting.
  std::optional<wtf_size_t> previous_painted_row_index;

  for (const auto& child : fragment_.Children()) {
    if (!child->IsTableSection()) {
      continue;
    }

    const auto& section = To<PhysicalBoxFragment>(*child);
    const std::optional<wtf_size_t> section_start_row_index =
        section.TableSectionStartRowIndex();
    if (!section_start_row_index)
      continue;

    const auto& section_row_offsets = *section.TableSectionRowOffsets();
    const wtf_size_t start_edge_index =
        *section_start_row_index * edges_per_row;

    // Determine if we have (table) content in the next/previous fragmentainer.
    // We'll use this information to paint "half" borders if required.
    bool has_content_in_previous_fragmentainer =
        (start_section == &section) && (*section_start_row_index > 0u);
    bool has_content_in_next_fragmentainer =
        (end_section == &section) &&
        (*section_start_row_index + section_row_offsets.size() <
         total_row_count);

    // If our row was fragmented we skip painting the borders at that edge.
    bool is_start_row_fragmented = IsStartRowFragmented(section);
    bool is_end_row_fragmented = IsEndRowFragmented(section);

    WritingModeConverter converter(fragment_.Style().GetWritingDirection(),
                                   section.Size());

    for (auto edge = TableCollapsedEdge(*collapsed_borders, start_edge_index);
         edge.Exists(); ++edge) {
      const wtf_size_t table_row = edge.TableRow();
      const wtf_size_t table_column = edge.TableColumn();
      const wtf_size_t fragment_table_row =
          table_row - *section_start_row_index;

      // Check if we've exhausted the rows in this section.
      if (fragment_table_row >= section_row_offsets.size()) {
        // Store the final row which we painted (if it wasn't fragmented).
        if (is_end_row_fragmented)
          previous_painted_row_index = std::nullopt;
        else
          previous_painted_row_index = table_row - 1;
        break;
      }

      if (!edge.CanPaint())
        continue;

      bool is_start_row = fragment_table_row == 0u;
      bool is_start_fragmented = is_start_row && is_start_row_fragmented;
      bool is_start_at_fragmentation_boundary =
          is_start_row && has_content_in_previous_fragmentainer;

      const LayoutUnit row_start_offset =
          section_row_offsets[fragment_table_row];
      const LayoutUnit column_start_offset =
          collapsed_borders_geometry->columns[table_column];

      LayoutUnit inline_start;
      LayoutUnit block_start;
      LayoutUnit inline_size;
      LayoutUnit block_size;

      if (edge.IsInlineAxis()) {
        // NOTE: This crash has been observed, but we aren't able to find a
        // reproducible testcase. See: crbug.com/1179369.
        if (table_column + 1 >= collapsed_borders_geometry->columns.size()) {
          NOTREACHED_IN_MIGRATION();
          continue;
        }

        // Check if we have painted this inline border in a previous section.
        if (previous_painted_row_index &&
            *previous_painted_row_index == table_row) {
          continue;
        }

        bool is_end_row = fragment_table_row == section_row_offsets.size() - 1u;
        bool is_end_fragmented = is_end_row && is_end_row_fragmented;
        bool is_end_at_fragmentation_boundary =
            is_end_row && has_content_in_next_fragmentainer;

        // If the current row has been fragmented, omit the inline border.
        if (is_start_fragmented || is_end_fragmented)
          continue;

        inline_start = column_start_offset;
        inline_size = collapsed_borders_geometry->columns[table_column + 1] -
                      column_start_offset;
        block_start = is_start_at_fragmentation_boundary
                          ? row_start_offset
                          : row_start_offset - edge.BorderWidth() / 2;
        block_size = is_start_at_fragmentation_boundary ||
                             is_end_at_fragmentation_boundary
                         ? edge.BorderWidth() / 2
                         : edge.BorderWidth();

        LogicalSize start_joint;
        LogicalSize end_joint;
        bool start_wins;
        bool end_wins;
        ComputeEdgeJoints(*collapsed_borders, edge,
                          is_start_at_fragmentation_boundary,
                          is_end_at_fragmentation_boundary, start_joint,
                          end_joint, start_wins, end_wins);
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
        // Check if this block border exists in this section.
        if (fragment_table_row + 1 >= section_row_offsets.size())
          continue;

        bool is_end_row =
            fragment_table_row + 1u == section_row_offsets.size() - 1u;
        bool is_end_fragmented = is_end_row && is_end_row_fragmented;
        bool is_end_at_fragmentation_boundary =
            is_end_row && has_content_in_next_fragmentainer;

        block_start = row_start_offset;
        block_size =
            section_row_offsets[fragment_table_row + 1] - row_start_offset;
        inline_start = column_start_offset - edge.BorderWidth() / 2;
        inline_size = edge.BorderWidth();

        LogicalSize start_joint;
        LogicalSize end_joint;
        bool start_wins;
        bool end_wins;
        ComputeEdgeJoints(*collapsed_borders, edge,
                          is_start_at_fragmentation_boundary,
                          is_end_at_fragmentation_boundary, start_joint,
                          end_joint, start_wins, end_wins);
        if (is_start_fragmented) {
          // We don't need to perform any adjustment if we've been start
          // fragmented as there isn't a joint here.
        } else if (start_wins) {
          block_start -= start_joint.block_size / 2;
          block_size += start_joint.block_size / 2;
        } else {
          block_start += start_joint.block_size / 2;
          block_size -= start_joint.block_size / 2;
        }
        if (is_end_fragmented) {
          // We don't need to perform any adjustment if we've been end
          // fragmented as there isn't a joint here.
        } else if (end_wins) {
          block_size += end_joint.block_size / 2;
        } else {
          block_size -= end_joint.block_size / 2;
        }
      }
      const LogicalRect logical_border_rect(inline_start, block_start,
                                            inline_size, block_size);
      PhysicalRect physical_border_rect =
          converter.ToPhysical(logical_border_rect);
      physical_border_rect.offset += child.offset + paint_offset;

      BoxSide box_side;
      if (fragment_.Style().IsHorizontalWritingMode()) {
        box_side = edge.IsInlineAxis() ? BoxSide::kTop : BoxSide::kLeft;
      } else {
        box_side = edge.IsInlineAxis() ? BoxSide::kLeft : BoxSide::kTop;
      }
      BoxBorderPainter::DrawBoxSide(
          paint_info.context, ToPixelSnappedRect(physical_border_rect),
          box_side, edge.BorderColor(), edge.BorderStyle(), auto_dark_mode);
    }
  }
}

void TableSectionPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const PhysicalRect& paint_rect,
    const BoxDecorationData& box_decoration_data) {
  DCHECK(box_decoration_data.ShouldPaint());
  if (box_decoration_data.ShouldPaintShadow()) {
    BoxPainterBase::PaintNormalBoxShadow(
        paint_info, paint_rect, fragment_.Style(), PhysicalBoxSides(),
        !box_decoration_data.ShouldPaintBackground());
  }

  // If we are fragmented - determine the total part size, relative to the
  // current fragment.
  PhysicalRect part_rect = paint_rect;
  if (!fragment_.IsOnlyForNode())
    part_rect.offset -= OffsetInStitchedFragments(fragment_, &part_rect.size);

  for (const PhysicalFragmentLink& child : fragment_.Children()) {
    const auto& child_fragment = *child;
    DCHECK(child_fragment.IsBox());
    if (!child_fragment.IsTableRow()) {
      continue;
    }
    TableRowPainter(To<PhysicalBoxFragment>(child_fragment))
        .PaintTablePartBackgroundIntoCells(
            paint_info, *To<LayoutBox>(fragment_.GetLayoutObject()), part_rect,
            paint_rect.offset + child.offset);
  }
  if (box_decoration_data.ShouldPaintShadow()) {
    BoxPainterBase::PaintInsetBoxShadowWithInnerRect(paint_info, paint_rect,
                                                     fragment_.Style());
  }
}

void TableSectionPainter::PaintColumnsBackground(
    const PaintInfo& paint_info,
    const PhysicalOffset& section_paint_offset,
    const PhysicalRect& columns_paint_rect,
    const TableFragmentData::ColumnGeometries& column_geometries) {
  for (const PhysicalFragmentLink& row : fragment_.Children()) {
    if (!row.fragment->IsTableRow()) {
      continue;
    }
    TableRowPainter(To<PhysicalBoxFragment>(*row.fragment))
        .PaintColumnsBackground(paint_info, section_paint_offset + row.offset,
                                columns_paint_rect, column_geometries);
  }
}

void TableRowPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const PhysicalRect& paint_rect,
    const BoxDecorationData& box_decoration_data) {
  DCHECK(box_decoration_data.ShouldPaint());
  if (box_decoration_data.ShouldPaintShadow()) {
    BoxPainterBase::PaintNormalBoxShadow(
        paint_info, paint_rect, fragment_.Style(), PhysicalBoxSides(),
        !box_decoration_data.ShouldPaintBackground());
  }

  // If we are fragmented - determine the total part size, relative to the
  // current fragment.
  PhysicalRect part_rect = paint_rect;
  if (!fragment_.IsOnlyForNode())
    part_rect.offset -= OffsetInStitchedFragments(fragment_, &part_rect.size);

  PaintTablePartBackgroundIntoCells(paint_info,
                                    *To<LayoutBox>(fragment_.GetLayoutObject()),
                                    part_rect, paint_rect.offset);
  if (box_decoration_data.ShouldPaintShadow()) {
    BoxPainterBase::PaintInsetBoxShadowWithInnerRect(paint_info, paint_rect,
                                                     fragment_.Style());
  }
}

void TableRowPainter::PaintTablePartBackgroundIntoCells(
    const PaintInfo& paint_info,
    const LayoutBox& table_part,
    const PhysicalRect& table_part_paint_rect,
    const PhysicalOffset& row_paint_offset) {
  for (const PhysicalFragmentLink& child : fragment_.Children()) {
    DCHECK(child.fragment->IsBox());
    DCHECK(child.fragment->GetLayoutObject()->IsTableCell() ||
           child.fragment->GetLayoutObject()->IsOutOfFlowPositioned());
    const auto& child_fragment = *child;
    if (!child_fragment.IsTableCell()) {
      continue;
    }
    TableCellPainter(To<PhysicalBoxFragment>(child_fragment))
        .PaintBackgroundForTablePart(paint_info, table_part,
                                     table_part_paint_rect,
                                     row_paint_offset + child.offset);
  }
}

void TableRowPainter::PaintColumnsBackground(
    const PaintInfo& paint_info,
    const PhysicalOffset& row_paint_offset,
    const PhysicalRect& columns_paint_rect,
    const TableFragmentData::ColumnGeometries& column_geometries) {
  WritingModeConverter converter(fragment_.Style().GetWritingDirection(),
                                 columns_paint_rect.size);
  for (const PhysicalFragmentLink& child : fragment_.Children()) {
    if (!child.fragment->IsTableCell()) {
      continue;
    }
    const wtf_size_t cell_column =
        To<PhysicalBoxFragment>(child.fragment.Get())->TableCellColumnIndex();
    for (const auto& column_geometry : column_geometries) {
      wtf_size_t current_start = column_geometry.start_column;
      wtf_size_t current_end =
          column_geometry.start_column + column_geometry.span - 1;
      if (cell_column < current_start || cell_column > current_end)
        continue;

      LogicalSize column_size = converter.ToLogical(columns_paint_rect.size);
      column_size.inline_size = column_geometry.inline_size;

      PhysicalRect column_paint_rect;
      column_paint_rect.size = converter.ToPhysical(column_size);
      column_paint_rect.offset =
          columns_paint_rect.offset +
          converter.ToPhysical({column_geometry.inline_offset, LayoutUnit()},
                               column_paint_rect.size);

      TableCellPainter(To<PhysicalBoxFragment>(*child.fragment))
          .PaintBackgroundForTablePart(
              paint_info, *column_geometry.node.GetLayoutBox(),
              column_paint_rect, row_paint_offset + child.offset);
    }
  }
}

void TableCellPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const PhysicalRect& paint_rect,
    const BoxDecorationData& box_decoration_data) {
  DCHECK(box_decoration_data.ShouldPaint());
  TableCellBackgroundClipper clipper(
      paint_info.context, *To<LayoutTableCell>(fragment_.GetLayoutObject()),
      paint_rect, box_decoration_data.IsPaintingBackgroundInContentsSpace());
  BoxFragmentPainter(fragment_).PaintBoxDecorationBackgroundWithRectImpl(
      paint_info, paint_rect, box_decoration_data);
}

// Inspired by TableCellPainter::PaintBackground.
void TableCellPainter::PaintBackgroundForTablePart(
    const PaintInfo& paint_info,
    const LayoutBox& table_part,
    const PhysicalRect& table_part_paint_rect,
    const PhysicalOffset& table_cell_paint_offset) {
  if (fragment_.Style().UsedVisibility() != EVisibility::kVisible) {
    return;
  }
  const auto& layout_table_cell =
      *To<LayoutTableCell>(fragment_.GetLayoutObject());
  if (layout_table_cell.BackgroundTransfersToView())
    return;  // cargo-culted from other painters.

  Color color = table_part.StyleRef().VisitedDependentColor(
      GetCSSPropertyBackgroundColor());
  const FillLayer& background_layers = table_part.StyleRef().BackgroundLayers();
  if (background_layers.AnyLayerHasImage() || !color.IsFullyTransparent()) {
    PhysicalRect cell_paint_rect(table_cell_paint_offset, fragment_.Size());
    TableCellBackgroundClipper clipper(paint_info.context, layout_table_cell,
                                       cell_paint_rect);
    BoxBackgroundPaintContext bg_paint_context(
        layout_table_cell,
        table_cell_paint_offset - table_part_paint_rect.offset, table_part,
        table_part_paint_rect.size);
    BoxFragmentPainter(fragment_).PaintFillLayers(
        paint_info, color, background_layers, cell_paint_rect,
        bg_paint_context);
  }
}

}  // namespace blink
