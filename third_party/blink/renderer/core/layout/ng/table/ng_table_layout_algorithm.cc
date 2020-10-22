// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_caption.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_column.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_column_visitor.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_section.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_borders.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_constraint_space_data.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_fragment_data.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_helpers.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_utils.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"

namespace blink {

namespace {

// TODO(atotic, ikilpatrick)
// Copy of ShouldScaleColumnsForParent from table_layout_algorithm_auto.cc
// The real fix would be for containers to understand that
// Table max really means: max should be trimmed to available inline size.
bool ShouldIgnorePercentagesForMinMax(const LayoutBox& table) {
  return false;
}

// Another MinMax variant of ShouldIgnorePercentagesForMinMax
// This one is only for flexbox/grid. CSS size should be ignored.
bool ShouldIgnoreCssSizesForMinMax(const LayoutBlock& table) {
  return false;
}

NGTableTypes::Caption ComputeCaptionConstraint(
    const ComputedStyle& table_style,
    const NGTableGroupedChildren& grouped_children) {
  // Caption inline size constraints.
  NGTableTypes::Caption caption_min_max;
  for (const NGBlockNode& caption : grouped_children.captions) {
    // Caption %-block-sizes are treated as auto, as there isn't a reasonable
    // block-size to resolve against.
    NGBoxStrut margins = ComputeMinMaxMargins(table_style, caption);
    MinMaxSizes min_max_size =
        ComputeMinAndMaxContentContribution(
            table_style, caption,
            MinMaxSizesInput(kIndefiniteSize, MinMaxSizesType::kContent))
            .sizes;
    min_max_size += margins.InlineSum();
    caption_min_max.Encompass(min_max_size);
  }
  return caption_min_max;
}

LayoutUnit ComputeUndistributableTableSpace(
    const NGTableTypes::Columns& column_constraints,
    LayoutUnit inline_table_border_padding,
    LayoutUnit inline_border_spacing) {
  unsigned inline_space_count = 2 + (column_constraints.data.size() > 1
                                         ? column_constraints.data.size() - 1
                                         : 0);
  return inline_table_border_padding +
         inline_space_count * inline_border_spacing;
}

void ComputeTableCssInlineSizes(
    const ComputedStyle& table_style,
    const NGConstraintSpace& constraint_space,
    const NGBoxStrut& table_border_padding,
    const MinMaxSizes& grid_min_max,
    LayoutUnit* table_css_min_inline_size,
    base::Optional<LayoutUnit>* table_css_inline_size,
    base::Optional<LayoutUnit>* table_css_max_inline_size) {
  const Length& table_min_length = table_style.LogicalMinWidth();
  *table_css_min_inline_size =
      table_min_length.IsSpecified()
          ? ResolveMinInlineLength<base::Optional<MinMaxSizes>>(
                constraint_space, table_style, table_border_padding,
                base::nullopt, table_min_length, LengthResolvePhase::kLayout)
          : LayoutUnit();

  // Compute standard "used width of a table".
  const Length& table_length = table_style.LogicalWidth();
  if (!table_length.IsAuto()) {
    *table_css_inline_size = ResolveMainInlineLength(
        constraint_space, table_style, table_border_padding,
        [grid_min_max](MinMaxSizesType) {
          return MinMaxSizesResult{
              grid_min_max,
              /* depends_on_percentage_block_size */ false};
        },
        table_length);
  }

  const Length& table_max_length = table_style.LogicalMaxWidth();
  if (table_max_length.IsSpecified()) {
    *table_css_max_inline_size =
        ResolveMaxInlineLength<base::Optional<MinMaxSizes>>(
            constraint_space, table_style, table_border_padding, base::nullopt,
            table_max_length, LengthResolvePhase::kLayout);
    *table_css_max_inline_size =
        std::max(**table_css_max_inline_size, *table_css_min_inline_size);
  }
}

// Empty table sizes have been a source of many inconsistencies
// between browsers.
LayoutUnit ComputeEmptyTableInlineSize(
    const NGConstraintSpace& space,
    const ComputedStyle& table_style,
    const LayoutUnit assignable_table_inline_size,
    const LayoutUnit undistributable_space,
    const NGTableTypes::Caption& caption_constraint,
    const NGBoxStrut& table_border_padding,
    const bool is_collapsed) {
  // If table has a css inline size, use that.
  if (space.IsFixedInlineSize() || !table_style.LogicalWidth().IsAuto() ||
      !table_style.LogicalMinWidth().IsAuto()) {
    return assignable_table_inline_size + undistributable_space;
  }
  // If there is a caption, it defines table wrapper inline size.
  if (caption_constraint.min_size) {
    return std::max(caption_constraint.min_size,
                    table_border_padding.InlineSum());
  }
  // Table is defined by its border/padding.
  if (is_collapsed) {
    return LayoutUnit();
  }
  return assignable_table_inline_size + table_border_padding.InlineSum();
}

// standard: https://www.w3.org/TR/css-tables-3/#computing-the-table-width
LayoutUnit ComputeAssignableTableInlineSize(
    const NGBlockNode& table,
    const NGConstraintSpace& space,
    const NGTableTypes::Columns& column_constraints,
    const NGTableTypes::Caption& caption_constraint,
    const LayoutUnit undistributable_space,
    const NGBoxStrut& table_border_padding,
    const bool is_fixed_layout,
    const bool is_collapsed) {
  if (space.IsFixedInlineSize()) {
    return (space.AvailableSize().inline_size - undistributable_space)
        .ClampNegativeToZero();
  }
  MinMaxSizes grid_min_max = NGTableAlgorithmHelpers::ComputeGridInlineMinMax(
      column_constraints, undistributable_space, is_fixed_layout,
      /* containing_block_expects_minmax_without_percentages */ false,
      /* skip_collapsed_columns */ false);

  LayoutUnit table_css_min_inline_size;
  base::Optional<LayoutUnit> table_css_inline_size;
  base::Optional<LayoutUnit> table_css_max_inline_size;
  ComputeTableCssInlineSizes(table.Style(), space, table_border_padding,
                             grid_min_max, &table_css_min_inline_size,
                             &table_css_inline_size,
                             &table_css_max_inline_size);

  LayoutUnit table_min_inline_size =
      std::max({table_css_min_inline_size, caption_constraint.min_size,
                grid_min_max.min_size});

  // Standard: "used width of the table".
  LayoutUnit used_inline_size_of_the_table;
  if (table_css_inline_size) {
    used_inline_size_of_the_table = *table_css_inline_size;
  } else {
    NGBoxStrut margins = ComputeMarginsForSelf(space, table.Style());
    used_inline_size_of_the_table =
        std::min(grid_min_max.max_size,
                 (space.AvailableSize().inline_size - margins.InlineSum())
                     .ClampNegativeToZero());
  }
  if (table_css_max_inline_size) {
    used_inline_size_of_the_table =
        std::min(used_inline_size_of_the_table, *table_css_max_inline_size);
  }
  used_inline_size_of_the_table =
      std::max(used_inline_size_of_the_table, table_min_inline_size);

  // Standard: The assignable table width is the "used width of the table"
  // minus the total horizontal border spacing.
  LayoutUnit assignable_table_inline_size =
      used_inline_size_of_the_table - undistributable_space;

  return assignable_table_inline_size;
}

// If |shrink_collapsed| is true, collapsed columns have zero width.
void ComputeLocationsFromColumns(
    const NGTableTypes::Columns& column_constraints,
    const Vector<LayoutUnit>& column_sizes,
    LayoutUnit inline_border_spacing,
    bool shrink_collapsed,
    NGTableTypes::ColumnLocations* column_locations,
    bool* has_collapsed_columns) {
  *has_collapsed_columns = false;
  column_locations->resize(column_constraints.data.size());
  if (column_locations->IsEmpty())
    return;
  LayoutUnit column_offset = inline_border_spacing;
  for (wtf_size_t i = 0; i < column_constraints.data.size(); ++i) {
    auto& column_location = (*column_locations)[i];
    auto& column_constraint = column_constraints.data[i];
    *has_collapsed_columns =
        *has_collapsed_columns || column_constraint.is_collapsed;
    column_location.offset = column_offset;
    if (shrink_collapsed && column_constraint.is_collapsed) {
      column_location.is_collapsed = true;
      column_location.size = LayoutUnit();
    } else {
      column_location.is_collapsed = false;
      column_location.size =
          column_sizes[i] != kIndefiniteSize ? column_sizes[i] : LayoutUnit();
      column_offset += column_location.size + inline_border_spacing;
    }
  }
}

scoped_refptr<NGTableConstraintSpaceData> CreateConstraintSpaceData(
    const ComputedStyle& style,
    const NGTableTypes::ColumnLocations& column_locations,
    const NGTableTypes::Sections& sections,
    const NGTableTypes::Rows& rows,
    const NGTableTypes::CellBlockConstraints& cell_block_constraints,
    const LayoutUnit table_inline_size,
    const LogicalSize& border_spacing) {
  scoped_refptr<NGTableConstraintSpaceData> data =
      base::MakeRefCounted<NGTableConstraintSpaceData>();
  data->table_inline_size = table_inline_size;
  data->table_writing_direction = style.GetWritingDirection();
  data->table_border_spacing = border_spacing;
  data->treat_table_block_size_as_constrained = !style.LogicalHeight().IsAuto();
  data->hide_table_cell_if_empty =
      style.EmptyCells() == EEmptyCells::kHide &&
      style.BorderCollapse() == EBorderCollapse::kSeparate;
  data->has_collapsed_borders =
      style.BorderCollapse() == EBorderCollapse::kCollapse;

  data->column_locations.ReserveCapacity(column_locations.size());
  for (const auto& location : column_locations) {
    data->column_locations.emplace_back(location.offset, location.size,
                                        location.is_collapsed);
  }
  data->sections.ReserveCapacity(sections.size());
  for (const auto& section : sections)
    data->sections.emplace_back(section.start_row, section.rowspan);
  data->rows.ReserveCapacity(rows.size());
  for (const auto& row : rows) {
    data->rows.emplace_back(
        row.baseline, row.block_size, row.start_cell_index, row.cell_count,
        row.has_baseline_aligned_percentage_block_size_descendants,
        row.is_collapsed);
  }
  data->cells.ReserveCapacity(cell_block_constraints.size());
  // Traversing from section is necessary to limit cell's rowspan to the
  // section. The cell does not know what section it is in.
  for (const auto& section : sections) {
    for (wtf_size_t row_index = section.start_row;
         row_index < section.start_row + section.rowspan; ++row_index) {
      for (wtf_size_t cell_index = rows[row_index].start_cell_index;
           cell_index <
           rows[row_index].start_cell_index + rows[row_index].cell_count;
           ++cell_index) {
        wtf_size_t max_rowspan =
            section.start_row + section.rowspan - row_index;
        wtf_size_t rowspan =
            std::min(cell_block_constraints[cell_index].rowspan, max_rowspan);
        // Compute cell's size.
        LayoutUnit cell_block_size;
        for (wtf_size_t i = 0; i < rowspan; ++i) {
          if (!rows[row_index + i].is_collapsed) {
            cell_block_size += rows[row_index + i].block_size;
            if (i != 0)
              cell_block_size += border_spacing.block_size;
          }
        }
        data->cells.emplace_back(
            cell_block_constraints[cell_index].border_box_borders,
            cell_block_size, cell_block_constraints[cell_index].column_index,
            cell_block_constraints[cell_index].is_constrained);
      }
    }
  }
  return data;
}

// Columns do not generate fragments.
// Column geometry is needed for painting, and is stored
// in NGTableFragmentData. Geometry data is also copied
// back to LayoutObject.
class ColumnGeometriesBuilder {
 public:
  void VisitCol(const NGLayoutInputNode& col,
                wtf_size_t start_column_index,
                wtf_size_t span) {
    wtf_size_t end_column_index = start_column_index + span - 1;
    DCHECK_LE(end_column_index, column_locations.size() - 1);
    LayoutUnit column_width = column_locations[end_column_index].offset +
                              column_locations[end_column_index].size -
                              column_locations[start_column_index].offset;
    col.GetLayoutBox()->SetLogicalWidth(column_width);
    col.GetLayoutBox()->SetLogicalHeight(table_grid_block_size);
    for (unsigned i = 0; i < span; ++i) {
      wtf_size_t current_column_index = start_column_index + i;
      column_geometries.emplace_back(
          current_column_index, /* span */ 1,
          column_locations[current_column_index].offset -
              border_spacing.inline_size,
          column_locations[current_column_index].size, col);
    }
  }

  void EnterColgroup(const NGLayoutInputNode& colgroup,
                     wtf_size_t start_column_index) {}

  void LeaveColgroup(const NGLayoutInputNode& colgroup,
                     wtf_size_t start_column_index,
                     wtf_size_t span,
                     bool has_children) {
    if (span == 0)
      return;
    if (has_children) {
      wtf_size_t last_column_index = start_column_index + span - 1;
      LayoutUnit colgroup_size = column_locations[last_column_index].offset +
                                 column_locations[last_column_index].size -
                                 column_locations[start_column_index].offset;
      colgroup.GetLayoutBox()->SetLogicalWidth(colgroup_size);
      colgroup.GetLayoutBox()->SetLogicalHeight(table_grid_block_size);
      column_geometries.emplace_back(
          start_column_index, span,
          column_locations[start_column_index].offset -
              border_spacing.inline_size,
          colgroup_size, colgroup);
    } else {
      for (unsigned i = 0; i < span; ++i) {
        wtf_size_t current_column_index = start_column_index + i;
        column_geometries.emplace_back(
            current_column_index, /* span */ 1,
            column_locations[current_column_index].offset -
                border_spacing.inline_size,
            column_locations[current_column_index].size, colgroup);
      }
    }
  }

  void Sort() {
    // Geometries need to be sorted because this must be true:
    // - parent COLGROUP must come before child COLs.
    // - child COLs are in ascending order.
    std::sort(column_geometries.begin(), column_geometries.end(),
              [](const NGTableFragmentData::ColumnGeometry& a,
                 const NGTableFragmentData::ColumnGeometry& b) {
                if (a.node.IsTableCol() && b.node.IsTableCol()) {
                  return a.start_column < b.start_column;
                }
                if (a.node.IsTableColgroup()) {
                  if (a.start_column <= b.start_column &&
                      (a.start_column + a.span) > b.start_column) {
                    return true;
                  }
                  return a.start_column < b.start_column;
                } else {
                  DCHECK(b.node.IsTableColgroup());
                  if (b.start_column <= a.start_column &&
                      (b.start_column + b.span) > a.start_column) {
                    return false;
                  }
                  return b.start_column >= a.start_column;
                }
              });
  }

  ColumnGeometriesBuilder(const NGTableTypes::ColumnLocations& column_locations,
                          LayoutUnit table_grid_block_size,
                          const LogicalSize& border_spacing)
      : column_locations(column_locations),
        table_grid_block_size(table_grid_block_size),
        border_spacing(border_spacing) {}
  NGTableFragmentData::ColumnGeometries column_geometries;
  const NGTableTypes::ColumnLocations& column_locations;
  const LayoutUnit table_grid_block_size;
  const LogicalSize& border_spacing;
};

LayoutUnit ComputeTableSizeFromColumns(
    const NGTableTypes::ColumnLocations column_locations,
    const NGBoxStrut& table_border_padding,
    const LogicalSize& border_spacing) {
  return column_locations.back().offset + column_locations.back().size +
         table_border_padding.InlineSum() + border_spacing.inline_size;
}

}  // namespace

LayoutUnit NGTableLayoutAlgorithm::ComputeTableInlineSize(
    const NGBlockNode& table,
    const NGConstraintSpace& space,
    const NGBoxStrut& table_border_padding) {
  const bool is_fixed_layout = table.Style().IsFixedTableLayout();
  // Tables need autosizer.
  base::Optional<TextAutosizer::TableLayoutScope> text_autosizer;
  if (!is_fixed_layout)
    text_autosizer.emplace(To<LayoutNGTable>(table.GetLayoutBox()));

  const LogicalSize border_spacing = table.Style().TableBorderSpacing();
  NGTableGroupedChildren grouped_children(table);
  scoped_refptr<const NGTableBorders> table_borders = table.GetTableBorders();

  // Compute min/max inline constraints.
  const scoped_refptr<const NGTableTypes::Columns> column_constraints =
      table.GetColumnConstraints(grouped_children, table_border_padding);

  const NGTableTypes::Caption caption_constraint =
      ComputeCaptionConstraint(table.Style(), grouped_children);

  const LayoutUnit undistributable_space = ComputeUndistributableTableSpace(
      *column_constraints, table_border_padding.InlineSum(),
      border_spacing.inline_size);

  const LayoutUnit assignable_table_inline_size =
      ComputeAssignableTableInlineSize(
          table, space, *column_constraints, caption_constraint,
          undistributable_space, table_border_padding, is_fixed_layout,
          table_borders->IsCollapsed());
  if (column_constraints->data.IsEmpty()) {
    return ComputeEmptyTableInlineSize(
        space, table.Style(), assignable_table_inline_size,
        undistributable_space, caption_constraint, table_border_padding,
        table_borders->IsCollapsed());
  }

  const Vector<LayoutUnit> column_sizes =
      NGTableAlgorithmHelpers::SynchronizeAssignableTableInlineSizeAndColumns(
          assignable_table_inline_size, border_spacing.inline_size,
          is_fixed_layout, *column_constraints);

  // Final inline size must depend on column locations, because columns can be
  // hidden.
  NGTableTypes::ColumnLocations column_locations;
  bool has_collapsed_columns;
  ComputeLocationsFromColumns(
      *column_constraints, column_sizes, border_spacing.inline_size,
      /* collapse_columns */ true, &column_locations, &has_collapsed_columns);
  return ComputeTableSizeFromColumns(column_locations, table_border_padding,
                                     border_spacing);
}

scoped_refptr<const NGLayoutResult> NGTableLayoutAlgorithm::Layout() {
  DCHECK(!BreakToken());
  const bool is_fixed_layout = Style().IsFixedTableLayout();

  // TODO(atotic) review autosizer usage in TablesNG.
  // Legacy has:
  //  LayoutTable::UpdateLayout
  //    TextAutosizer::LayoutScope
  // TableLayoutAlgorithmAuto::ComputeIntrinsicLogicalWidths
  //    TextAutosizer::TableLayoutScope
  base::Optional<TextAutosizer::TableLayoutScope> text_autosizer;
  if (!is_fixed_layout)
    text_autosizer.emplace(To<LayoutNGTable>(Node().GetLayoutBox()));

  const LogicalSize border_spacing = Style().TableBorderSpacing();
  NGTableGroupedChildren grouped_children(Node());
  const scoped_refptr<const NGTableBorders> table_borders =
      Node().GetTableBorders();
  DCHECK(table_borders.get());
  const NGBoxStrut border_padding = container_builder_.BorderPadding();

  // Algorithm:
  // - Compute inline constraints.
  // - Redistribute assignble table inline size to inline constraints.
  // - Compute column locations.
  // - Compute row block sizes.
  // - Generate fragment.
  const scoped_refptr<const NGTableTypes::Columns> column_constraints =
      Node().GetColumnConstraints(grouped_children, border_padding);
  const NGTableTypes::Caption caption_constraint =
      ComputeCaptionConstraint(Style(), grouped_children);
  // Compute assignable table inline size.
  // Standard: https://www.w3.org/TR/css-tables-3/#width-distribution
  const LayoutUnit undistributable_space = ComputeUndistributableTableSpace(
      *column_constraints, border_padding.InlineSum(),
      border_spacing.inline_size);

  const LayoutUnit assignable_table_inline_size =
      ComputeAssignableTableInlineSize(
          Node(), ConstraintSpace(), *column_constraints, caption_constraint,
          undistributable_space, border_padding, is_fixed_layout,
          table_borders->IsCollapsed());

  // Distribute assignable table width.
  const Vector<LayoutUnit> column_sizes =
      NGTableAlgorithmHelpers::SynchronizeAssignableTableInlineSizeAndColumns(
          assignable_table_inline_size, border_spacing.inline_size,
          is_fixed_layout, *column_constraints);

  NGTableTypes::ColumnLocations column_locations;
  bool has_collapsed_columns;
  ComputeLocationsFromColumns(
      *column_constraints, column_sizes, border_spacing.inline_size,
      /* shrink_collapsed */ false, &column_locations, &has_collapsed_columns);

  LayoutUnit table_inline_size_before_collapse;
  const bool is_grid_empty = column_locations.IsEmpty();
  if (is_grid_empty) {
    table_inline_size_before_collapse = ComputeEmptyTableInlineSize(
        ConstraintSpace(), Style(), assignable_table_inline_size,
        undistributable_space, caption_constraint, border_padding,
        table_borders->IsCollapsed());
  } else {
    table_inline_size_before_collapse = ComputeTableSizeFromColumns(
        column_locations, border_padding, border_spacing);
  }

  NGTableTypes::Rows rows;
  NGTableTypes::CellBlockConstraints cell_block_constraints;
  NGTableTypes::Sections sections;
  LayoutUnit minimal_table_grid_block_size;
  ComputeRows(table_inline_size_before_collapse - border_padding.InlineSum(),
              grouped_children, column_locations, *table_borders,
              border_spacing, border_padding, is_fixed_layout, &rows,
              &cell_block_constraints, &sections,
              &minimal_table_grid_block_size);

  if (has_collapsed_columns) {
    ComputeLocationsFromColumns(
        *column_constraints, column_sizes, border_spacing.inline_size,
        /* shrink_collapsed */ true, &column_locations, &has_collapsed_columns);
  }
#if DCHECK_IS_ON()
  LayoutUnit table_inline_size;
  if (has_collapsed_columns) {
    table_inline_size = ComputeTableSizeFromColumns(
        column_locations, border_padding, border_spacing);
    table_inline_size =
        std::max(table_inline_size, caption_constraint.min_size);
  } else {
    table_inline_size = table_inline_size_before_collapse;
  }
  DCHECK_EQ(table_inline_size, container_builder_.InlineSize());
#endif

  return GenerateFragment(
      container_builder_.InlineSize(), minimal_table_grid_block_size,
      grouped_children, column_locations, rows, cell_block_constraints,
      sections, *table_borders, is_grid_empty ? LogicalSize() : border_spacing);
}

MinMaxSizesResult NGTableLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesInput& input) const {
  LayoutNGTable* layout_table = To<LayoutNGTable>(Node().GetLayoutBox());
  const bool is_fixed_layout = Style().IsFixedTableLayout();
  // Tables need autosizer.
  base::Optional<TextAutosizer::TableLayoutScope> text_autosizer;
  if (!is_fixed_layout)
    text_autosizer.emplace(layout_table);

  const LogicalSize border_spacing = Style().TableBorderSpacing();
  const WritingMode table_writing_mode = Style().GetWritingMode();
  NGTableGroupedChildren grouped_children(Node());
  const scoped_refptr<const NGTableBorders> table_borders =
      Node().GetTableBorders();
  const NGBoxStrut border_padding = container_builder_.BorderPadding();

  const scoped_refptr<const NGTableTypes::Columns> column_constraints =
      Node().GetColumnConstraints(grouped_children, border_padding);
  const NGTableTypes::Caption caption_constraint =
      ComputeCaptionConstraint(Style(), grouped_children);

  const LayoutUnit undistributable_space = ComputeUndistributableTableSpace(
      *column_constraints, border_padding.InlineSum(),
      border_spacing.inline_size);

  const MinMaxSizes grid_min_max =
      NGTableAlgorithmHelpers::ComputeGridInlineMinMax(
          *column_constraints, undistributable_space, is_fixed_layout,
          ShouldIgnorePercentagesForMinMax(*layout_table),
          /* skip_collapsed_columns */ true);

  MinMaxSizes min_max{
      std::max(grid_min_max.min_size, caption_constraint.min_size),
      std::max(grid_min_max.max_size, caption_constraint.min_size)};

  if (ShouldIgnoreCssSizesForMinMax(*layout_table)) {
    return MinMaxSizesResult{min_max,
                             /* depends_on_percentage_block_size */ false};
  }

  NGConstraintSpaceBuilder min_space_builder(ConstraintSpace(),
                                             table_writing_mode, true);
  min_space_builder.SetAvailableSize({LayoutUnit(), kIndefiniteSize});
  LayoutUnit min_measure_table_css_min_inline_size;
  base::Optional<LayoutUnit> min_measure_table_css_inline_size;
  base::Optional<LayoutUnit> measure_table_css_max_inline_size;

  ComputeTableCssInlineSizes(
      Style(), min_space_builder.ToConstraintSpace(), border_padding,
      grid_min_max, &min_measure_table_css_min_inline_size,
      &min_measure_table_css_inline_size, &measure_table_css_max_inline_size);

  // Table min/max sizes are unusual in how the specified sizes affects them.
  // If table_css_inline_size is defined:
  //   min_max_sizes is std::max(
  //     table_css_inline_size,
  //     grid_min_max.min_size,
  //     caption_constraint.min_size)
  //  (min_size and max_size are the same value).
  //
  // If table_css_inline_size is not defined:
  //   min_max_sizes.min_size is std::max(
  //     grid_min_max.min_size, caption_constraint.min_size)
  //   min_max_sizes.max_size is std::max(
  //     grid_min_max.max_size, caption_constraint.min_size)
  if (min_measure_table_css_inline_size) {
    NGConstraintSpaceBuilder max_space_builder(ConstraintSpace(),
                                               table_writing_mode, true);
    max_space_builder.SetAvailableSize(
        {grid_min_max.max_size, kIndefiniteSize});
    LayoutUnit max_measure_table_css_min_inline_size;
    base::Optional<LayoutUnit> max_measure_table_css_inline_size;
    ComputeTableCssInlineSizes(
        Style(), max_space_builder.ToConstraintSpace(), border_padding,
        grid_min_max, &max_measure_table_css_min_inline_size,
        &max_measure_table_css_inline_size, &measure_table_css_max_inline_size);
    // Compute minimum.
    min_max.min_size =
        std::max({min_max.min_size, min_measure_table_css_min_inline_size,
                  *min_measure_table_css_inline_size});
    // Compute maximum.
    min_max.max_size =
        std::max({max_measure_table_css_min_inline_size,
                  *max_measure_table_css_inline_size, grid_min_max.min_size,
                  caption_constraint.min_size});
    if (is_fixed_layout && Style().LogicalWidth().IsPercentOrCalc())
      min_max.max_size = NGTableTypes::kTableMaxInlineSize;
  }
  DCHECK_LE(min_max.min_size, min_max.max_size);
  return MinMaxSizesResult{min_max,
                           /* depends_on_percentage_block_size */ false};
}

void NGTableLayoutAlgorithm::ComputeRows(
    const LayoutUnit table_grid_inline_size,
    const NGTableGroupedChildren& grouped_children,
    const NGTableTypes::ColumnLocations& column_locations,
    const NGTableBorders& table_borders,
    const LogicalSize& border_spacing,
    const NGBoxStrut& table_border_padding,
    bool is_fixed,
    NGTableTypes::Rows* rows,
    NGTableTypes::CellBlockConstraints* cell_block_constraints,
    NGTableTypes::Sections* sections,
    LayoutUnit* minimal_table_grid_block_size) {
  DCHECK_EQ(rows->size(), 0u);
  DCHECK_EQ(cell_block_constraints->size(), 0u);

  // Initially resolve the table's block-size with an indefinite size.
  LayoutUnit css_table_block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), table_border_padding, kIndefiniteSize,
      table_grid_inline_size);

  LayoutUnit total_table_block_size;
  wtf_size_t section_index = 0;
  for (const NGBlockNode& section : grouped_children) {
    NGTableAlgorithmUtils::ComputeSectionMinimumRowBlockSizes(
        section, table_grid_inline_size,
        /* is_restricted_block_size_table */ css_table_block_size !=
            kIndefiniteSize,
        column_locations, table_borders, border_spacing.block_size,
        section_index++, sections, rows, cell_block_constraints);
    total_table_block_size += sections->back().block_size;
  }

  // Re-resolve the block-size if we have a min block-size which is resolvable.
  // We'll redistribute sections/rows into this space.
  if (!BlockLengthUnresolvable(ConstraintSpace(), Style().LogicalMinHeight(),
                               LengthResolvePhase::kLayout)) {
    css_table_block_size = ComputeBlockSizeForFragment(
        ConstraintSpace(), Style(), table_border_padding,
        table_border_padding.BlockSum(), table_grid_inline_size);
  }

  // In quirks mode, empty tables ignore css block size.
  bool is_empty_quirks_mode_table =
      Node().GetDocument().InQuirksMode() &&
      grouped_children.begin() == grouped_children.end();
  // Redistribute CSS table block size if necessary.
  if (css_table_block_size != kIndefiniteSize && !is_empty_quirks_mode_table) {
    *minimal_table_grid_block_size = css_table_block_size;
    LayoutUnit distributable_block_size = std::max(
        LayoutUnit(), css_table_block_size - table_border_padding.BlockSum());
    if (distributable_block_size > total_table_block_size) {
      NGTableAlgorithmHelpers::DistributeTableBlockSizeToSections(
          border_spacing.block_size, distributable_block_size, sections, rows);
    }
  }

  // Collapsed rows get 0 block-size, and shrink the minimum table size.
  for (NGTableTypes::Row& row : *rows) {
    if (row.is_collapsed) {
      if (*minimal_table_grid_block_size != LayoutUnit()) {
        *minimal_table_grid_block_size -= row.block_size;
        if (rows->size() > 1)
          *minimal_table_grid_block_size -= border_spacing.block_size;
      }
      row.block_size = LayoutUnit();
    }
  }
  minimal_table_grid_block_size->ClampNegativeToZero();
}

// Method also sets LogicalWidth/Height on columns.
void NGTableLayoutAlgorithm::ComputeTableSpecificFragmentData(
    const NGTableGroupedChildren& grouped_children,
    const NGTableTypes::ColumnLocations& column_locations,
    const NGTableTypes::Rows& rows,
    const NGTableBorders& table_borders,
    const PhysicalRect& table_grid_rect,
    const LogicalSize& border_spacing,
    const LayoutUnit table_grid_block_size) {
  // TODO(atotic) SetHasNonCollapsedBorderDecoration should be a fragment
  // property, not a flag on LayoutObject.
  container_builder_.SetTableGridRect(table_grid_rect);
  container_builder_.SetTableColumnCount(column_locations.size());
  container_builder_.SetHasCollapsedBorders(table_borders.IsCollapsed());
  // Column geometries.
  if (grouped_children.columns.size() > 0) {
    ColumnGeometriesBuilder geometry_builder(
        column_locations, table_grid_block_size, border_spacing);
    VisitLayoutNGTableColumn(grouped_children.columns, column_locations.size(),
                             &geometry_builder);
    geometry_builder.Sort();
    container_builder_.SetTableColumnGeometry(
        geometry_builder.column_geometries);
  }
  // Collapsed borders.
  if (!table_borders.IsEmpty()) {
    LayoutUnit grid_inline_start = table_borders.TableBorder().inline_start;
    std::unique_ptr<NGTableFragmentData::CollapsedBordersGeometry>
        fragment_borders_geometry =
            std::make_unique<NGTableFragmentData::CollapsedBordersGeometry>();
    for (const auto& column : column_locations) {
      fragment_borders_geometry->columns.push_back(column.offset +
                                                   grid_inline_start);
    }
    fragment_borders_geometry->columns.push_back(
        column_locations.back().offset + column_locations.back().size +
        grid_inline_start);
    LayoutUnit row_offset = table_borders.TableBorder().block_start;
    for (const auto& row : rows) {
      fragment_borders_geometry->rows.push_back(row_offset);
      row_offset += row.block_size;
    }
    fragment_borders_geometry->rows.push_back(row_offset);
    container_builder_.SetTableCollapsedBorders(table_borders);
    container_builder_.SetTableCollapsedBordersGeometry(
        std::move(fragment_borders_geometry));
  }
}

void NGTableLayoutAlgorithm::GenerateCaptionFragments(
    const NGTableGroupedChildren& grouped_children,
    const LayoutUnit table_inline_size,
    ECaptionSide caption_side,
    LayoutUnit* table_block_offset) {
  const LogicalSize available_size = {table_inline_size, kIndefiniteSize};
  for (NGBlockNode caption : grouped_children.captions) {
    if (caption.Style().CaptionSide() != caption_side)
      continue;
    NGConstraintSpaceBuilder builder(ConstraintSpace(),
                                     caption.Style().GetWritingMode(),
                                     /* is_new_fc */ true);
    SetOrthogonalFallbackInlineSizeIfNeeded(Style(), caption, &builder);
    builder.SetTextDirection(caption.Style().Direction());
    builder.SetAvailableSize(available_size);
    builder.SetPercentageResolutionSize(available_size);
    NGConstraintSpace caption_constraint_space = builder.ToConstraintSpace();
    scoped_refptr<const NGLayoutResult> caption_result =
        caption.Layout(caption_constraint_space);
    NGBoxStrut margins = ComputeMarginsFor(caption_constraint_space,
                                           caption.Style(), ConstraintSpace());
    NGFragment fragment(ConstraintSpace().GetWritingDirection(),
                        caption_result->PhysicalFragment());
    ResolveInlineMargins(caption.Style(), Style(), table_inline_size,
                         fragment.InlineSize(), &margins);
    *table_block_offset += margins.block_start;
    container_builder_.AddResult(
        *caption_result,
        LogicalOffset(margins.inline_start, *table_block_offset));
    caption.StoreMargins(
        margins.ConvertToPhysical(ConstraintSpace().GetWritingDirection()));
    *table_block_offset += fragment.BlockSize() + margins.block_end;
  }
}

// Generated fragment structure
// +---- table wrapper fragment ----+
// |     top caption fragments      |
// |     table border/padding       |
// |       block_spacing            |
// |         section                |
// |       block_spacing            |
// |         section                |
// |       block_spacing            |
// |     table border/padding       |
// |     bottom caption fragments   |
// +--------------------------------+
scoped_refptr<const NGLayoutResult> NGTableLayoutAlgorithm::GenerateFragment(
    const LayoutUnit table_inline_size,
    const LayoutUnit minimal_table_grid_block_size,
    const NGTableGroupedChildren& grouped_children,
    const NGTableTypes::ColumnLocations& column_locations,
    const NGTableTypes::Rows& rows,
    const NGTableTypes::CellBlockConstraints& cell_block_constraints,
    const NGTableTypes::Sections& sections,
    const NGTableBorders& table_borders,
    const LogicalSize& border_spacing) {
  WritingDirectionMode table_writing_direction = Style().GetWritingDirection();
  scoped_refptr<NGTableConstraintSpaceData> constraint_space_data =
      CreateConstraintSpaceData(Style(), column_locations, sections, rows,
                                cell_block_constraints, table_inline_size,
                                border_spacing);

  const NGBoxStrut border_padding = container_builder_.BorderPadding();
  LayoutUnit table_block_end;

  // Top caption fragments.
  GenerateCaptionFragments(grouped_children, table_inline_size,
                           ECaptionSide::kTop, &table_block_end);

  // Section setup.
  const LogicalSize section_available_size{table_inline_size -
                                               border_padding.InlineSum() -
                                               border_spacing.inline_size * 2,
                                           kIndefiniteSize};
  DCHECK_GE(section_available_size.inline_size, LayoutUnit());
  auto CreateSectionConstraintSpace = [this, &table_writing_direction,
                                       &section_available_size,
                                       &constraint_space_data](
                                          wtf_size_t section_index) {
    NGConstraintSpaceBuilder section_space_builder(
        table_writing_direction.GetWritingMode(),
        table_writing_direction.GetWritingMode(),
        /* is_new_fc */ true);
    section_space_builder.SetTextDirection(Style().Direction());
    section_space_builder.SetAvailableSize(section_available_size);
    section_space_builder.SetIsFixedInlineSize(true);
    section_space_builder.SetPercentageResolutionSize(section_available_size);
    section_space_builder.SetNeedsBaseline(true);
    section_space_builder.SetTableSectionData(constraint_space_data,
                                              section_index);
    return section_space_builder.ToConstraintSpace();
  };

  // Generate section fragments.
  LogicalOffset section_offset;
  section_offset.inline_offset =
      border_padding.inline_start + border_spacing.inline_size;
  section_offset.block_offset = table_block_end + border_padding.block_start;

  base::Optional<LayoutUnit> table_baseline;
  wtf_size_t section_index = 0;
  bool has_section = false;
  for (NGBlockNode section : grouped_children) {
    scoped_refptr<const NGLayoutResult> section_result =
        section.Layout(CreateSectionConstraintSpace(section_index++));
    const NGPhysicalBoxFragment& physical_fragment =
        To<NGPhysicalBoxFragment>(section_result->PhysicalFragment());
    NGBoxFragment fragment(table_writing_direction, physical_fragment);
    if (fragment.BlockSize() != LayoutUnit() || !has_section)
      section_offset.block_offset += border_spacing.block_size;
    container_builder_.AddResult(*section_result, section_offset);
    if (!table_baseline) {
      if (const auto& section_baseline = fragment.Baseline())
        table_baseline = *section_baseline + section_offset.block_offset;
    }
    section_offset.block_offset += fragment.BlockSize();
    has_section = true;
  }
  if (has_section)
    section_offset.block_offset += border_spacing.block_size;
  LayoutUnit column_block_size =
      section_offset.block_offset - border_padding.block_start;
  if (has_section)
    column_block_size -= border_spacing.block_size * 2;
  LayoutUnit grid_block_size = std::max(
      section_offset.block_offset - table_block_end + border_padding.block_end,
      minimal_table_grid_block_size);
  LogicalRect table_grid_rect(LayoutUnit(), table_block_end,
                              container_builder_.InlineSize(), grid_block_size);
  table_block_end += grid_block_size;

  GenerateCaptionFragments(grouped_children, table_inline_size,
                           ECaptionSide::kBottom, &table_block_end);

  if (ConstraintSpace().IsFixedBlockSize()) {
    container_builder_.SetFragmentBlockSize(
        ConstraintSpace().AvailableSize().block_size);
  } else {
    container_builder_.SetFragmentBlockSize(table_block_end);
  }
  container_builder_.SetIntrinsicBlockSize(table_block_end);

  const WritingModeConverter grid_converter(
      Style().GetWritingDirection(),
      ToPhysicalSize(container_builder_.Size(),
                     table_writing_direction.GetWritingMode()));

  ComputeTableSpecificFragmentData(grouped_children, column_locations, rows,
                                   table_borders,
                                   grid_converter.ToPhysical(table_grid_rect),
                                   border_spacing, column_block_size);

  if (table_baseline)
    container_builder_.SetBaseline(*table_baseline);

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), &container_builder_).Run();
  return container_builder_.ToBoxFragment();
}

}  // namespace blink
