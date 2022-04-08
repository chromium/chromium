// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_layout_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_disable_side_effects_scope.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
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
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_child_iterator.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_constraint_space_data.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_fragment_data.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_helpers.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_utils.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_node.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/mathml/mathml_element.h"
#include "third_party/blink/renderer/core/mathml_names.h"

namespace blink {

namespace {

NGTableTypes::Caption ComputeCaptionConstraint(
    const NGConstraintSpace& table_space,
    const ComputedStyle& table_style,
    const NGTableGroupedChildren& grouped_children) {
  // Caption inline size constraints.
  NGTableTypes::Caption caption_min_max;
  for (const NGBlockNode& caption : grouped_children.captions) {
    // Caption %-block-sizes are treated as auto, as there isn't a reasonable
    // block-size to resolve against.
    NGMinMaxConstraintSpaceBuilder builder(table_space, table_style, caption,
                                           /* is_new_fc */ true);
    builder.SetAvailableBlockSize(kIndefiniteSize);
    const auto space = builder.ToConstraintSpace();

    MinMaxSizes min_max_sizes =
        ComputeMinAndMaxContentContribution(table_style, caption, space).sizes;
    min_max_sizes +=
        ComputeMarginsFor(space, caption.Style(), table_space).InlineSum();
    caption_min_max.Encompass(min_max_sizes);
  }
  return caption_min_max;
}

NGConstraintSpace CreateCaptionConstraintSpace(
    const NGConstraintSpace& table_constraint_space,
    const ComputedStyle& table_style,
    const NGBlockNode& caption,
    LogicalSize available_size,
    absl::optional<LayoutUnit> block_offset = absl::nullopt) {
  NGConstraintSpaceBuilder builder(table_constraint_space,
                                   caption.Style().GetWritingDirection(),
                                   /* is_new_fc */ true);
  SetOrthogonalFallbackInlineSizeIfNeeded(table_style, caption, &builder);
  builder.SetAvailableSize(available_size);
  builder.SetPercentageResolutionSize(available_size);
  builder.SetInlineAutoBehavior(NGAutoBehavior::kStretchImplicit);

  if (block_offset) {
    SetupSpaceBuilderForFragmentation(table_constraint_space, caption,
                                      *block_offset, &builder,
                                      /* is_new_fc */ true, false);
  }

  return builder.ToConstraintSpace();
}

NGTableLayoutAlgorithm::CaptionResult LayoutCaption(
    const NGConstraintSpace& table_constraint_space,
    const ComputedStyle& table_style,
    LayoutUnit table_inline_size,
    const NGConstraintSpace& caption_constraint_space,
    const NGBlockNode& caption,
    const NGBlockBreakToken* break_token = nullptr) {
  const NGLayoutResult* layout_result =
      caption.Layout(caption_constraint_space, break_token);
  NGFragment fragment(table_constraint_space.GetWritingDirection(),
                      layout_result->PhysicalFragment());
  NGBoxStrut margins = ComputeMarginsFor(
      caption_constraint_space, caption.Style(), table_constraint_space);
  ResolveInlineMargins(caption.Style(), table_style, table_inline_size,
                       fragment.InlineSize(), &margins);

  return {caption, layout_result, margins};
}

void ComputeCaptionFragments(
    const NGConstraintSpace& table_constraint_space,
    const ComputedStyle& table_style,
    const NGTableGroupedChildren& grouped_children,
    const LayoutUnit table_inline_size,
    HeapVector<NGTableLayoutAlgorithm::CaptionResult>* captions,
    LayoutUnit& captions_block_size) {
  const LogicalSize available_size = {table_inline_size, kIndefiniteSize};
  for (NGBlockNode caption : grouped_children.captions) {
    NGConstraintSpace caption_constraint_space = CreateCaptionConstraintSpace(
        table_constraint_space, table_style, caption, available_size);

    // If we are discarding the results (compute-only) and we are after layout
    // (|!NeedsLayout|), or if we are in block fragmentation, make sure not to
    // update the cached layout results. If we are block fragmented, a node may
    // generate multiple fragments, so make sure that we keep the fragments
    // generated and stored in the actual layout pass.
    //
    // TODO(mstensho): We can remove this if we only perform this operation once
    // per table node (and e.g. store the table data in the break tokens).
    absl::optional<NGDisableSideEffectsScope> disable_side_effects;
    if ((!captions && !caption.GetLayoutBox()->NeedsLayout()) ||
        table_constraint_space.HasBlockFragmentation())
      disable_side_effects.emplace();

    NGTableLayoutAlgorithm::CaptionResult caption_result =
        LayoutCaption(table_constraint_space, table_style, table_inline_size,
                      caption_constraint_space, caption);
    NGFragment fragment(table_constraint_space.GetWritingDirection(),
                        caption_result.layout_result->PhysicalFragment());
    captions_block_size +=
        fragment.BlockSize() + caption_result.margins.BlockSum();
    if (captions)
      captions->push_back(caption_result);
  }
}

LayoutUnit ComputeUndistributableTableSpace(
    const NGTableTypes::Columns& column_constraints,
    LayoutUnit inline_table_border_padding,
    LayoutUnit inline_border_spacing) {
  unsigned inline_space_count = 2;
  bool is_first_column = true;
  for (const NGTableTypes::Column& column : column_constraints.data) {
    if (!column.is_mergeable) {
      if (is_first_column)
        is_first_column = false;
      else
        inline_space_count++;
    }
  }

  return inline_table_border_padding +
         inline_space_count * inline_border_spacing;
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
    const bool has_collapsed_borders) {
  // If table has a css inline size, use that.
  if (space.IsFixedInlineSize() ||
      (space.IsInlineAutoBehaviorStretch() &&
       table_style.LogicalWidth().IsAuto()) ||
      !table_style.LogicalWidth().IsAuto() ||
      !table_style.LogicalMinWidth().IsAuto()) {
    return assignable_table_inline_size + undistributable_space;
  }
  // If there is a caption, it defines table wrapper inline size.
  if (caption_constraint.min_size) {
    return std::max(caption_constraint.min_size,
                    table_border_padding.InlineSum());
  }
  // Table is defined by its border/padding.
  if (has_collapsed_borders)
    return LayoutUnit();

  return assignable_table_inline_size + table_border_padding.InlineSum();
}

// standard: https://www.w3.org/TR/css-tables-3/#computing-the-table-width
LayoutUnit ComputeAssignableTableInlineSize(
    const NGTableNode& table,
    const NGConstraintSpace& space,
    const NGTableTypes::Columns& column_constraints,
    const NGTableTypes::Caption& caption_constraint,
    const LayoutUnit undistributable_space,
    const NGBoxStrut& table_border_padding,
    const bool is_fixed_layout) {
  if (space.IsFixedInlineSize()) {
    return (space.AvailableSize().inline_size - undistributable_space)
        .ClampNegativeToZero();
  }

  const MinMaxSizes grid_min_max =
      NGTableAlgorithmHelpers::ComputeGridInlineMinMax(
          table, column_constraints, undistributable_space, is_fixed_layout,
          /* is_layout_pass */ true);

  // Standard: "used width of the table".
  LayoutUnit used_table_inline_size = ComputeUsedInlineSizeForTableFragment(
      space, table, table_border_padding, grid_min_max);

  // |ComputeUsedInlineSizeForTableFragment| returns a value >= GRIDMIN because
  // of the |grid_min_max| parameter above.
  DCHECK_GE(used_table_inline_size, grid_min_max.min_size);

  // Don't allow the inline-size to go below the caption min-size.
  used_table_inline_size =
      std::max(used_table_inline_size, caption_constraint.min_size);

  // Standard: The assignable table width is the "used width of the table"
  // minus the total horizontal border spacing.
  const LayoutUnit assignable_table_inline_size =
      used_table_inline_size - undistributable_space;
  DCHECK_GE(assignable_table_inline_size, LayoutUnit());

  return assignable_table_inline_size;
}

// If |shrink_collapsed| is true, collapsed columns have zero width.
void ComputeLocationsFromColumns(
    const NGTableTypes::Columns& column_constraints,
    const Vector<LayoutUnit>& column_sizes,
    LayoutUnit inline_border_spacing,
    bool shrink_collapsed,
    Vector<NGTableColumnLocation>* column_locations,
    bool* has_collapsed_columns) {
  *has_collapsed_columns = false;
  column_locations->resize(column_constraints.data.size());
  if (column_locations->IsEmpty())
    return;
  bool is_first_non_collpased_column = true;
  LayoutUnit column_offset = inline_border_spacing;
  for (wtf_size_t i = 0; i < column_constraints.data.size(); ++i) {
    auto& column_location = (*column_locations)[i];
    auto& column_constraint = column_constraints.data[i];
    *has_collapsed_columns |= column_constraint.is_collapsed;
    if (column_constraints.data[i].is_mergeable &&
        (column_sizes[i] == kIndefiniteSize ||
         column_sizes[i] == LayoutUnit())) {
      // Empty mergeable columns are treated as collapsed.
      column_location.offset = column_offset;
      column_location.size = LayoutUnit();
      column_location.is_collapsed = true;
    } else if (shrink_collapsed && column_constraint.is_collapsed) {
      column_location.offset = column_offset;
      column_location.size = LayoutUnit();
      column_location.is_collapsed = true;
    } else {
      if (is_first_non_collpased_column)
        is_first_non_collpased_column = false;
      else
        column_offset += inline_border_spacing;
      column_location.offset = column_offset;
      column_location.size =
          column_sizes[i] != kIndefiniteSize ? column_sizes[i] : LayoutUnit();
      column_location.is_collapsed = false;
      column_offset += column_location.size;
    }
  }
}

scoped_refptr<const NGTableConstraintSpaceData> CreateConstraintSpaceData(
    const ComputedStyle& style,
    const Vector<NGTableColumnLocation>& column_locations,
    const NGTableTypes::Sections& sections,
    const NGTableTypes::Rows& rows,
    const NGTableTypes::CellBlockConstraints& cell_block_constraints,
    const LogicalSize& border_spacing) {
  bool is_table_block_size_specified = !style.LogicalHeight().IsAuto();
  scoped_refptr<NGTableConstraintSpaceData> data =
      base::MakeRefCounted<NGTableConstraintSpaceData>();
  data->table_writing_direction = style.GetWritingDirection();
  data->table_border_spacing = border_spacing;
  data->is_table_block_size_specified = is_table_block_size_specified;
  data->has_collapsed_borders =
      style.BorderCollapse() == EBorderCollapse::kCollapse;
  data->column_locations = column_locations;

  data->sections.ReserveCapacity(sections.size());
  for (const auto& section : sections)
    data->sections.emplace_back(section.start_row, section.row_count);
  data->rows.ReserveCapacity(rows.size());
  for (const auto& row : rows) {
    data->rows.emplace_back(row.block_size, row.start_cell_index,
                            row.cell_count, row.baseline, row.is_collapsed);
  }
  data->cells.ReserveCapacity(cell_block_constraints.size());
  // Traversing from section is necessary to limit cell's rowspan to the
  // section. The cell does not know what section it is in.
  for (const auto& section : sections) {
    for (wtf_size_t row_index = section.start_row;
         row_index < section.start_row + section.row_count; ++row_index) {
      const auto& row = rows[row_index];
      for (wtf_size_t cell_index = row.start_cell_index;
           cell_index < row.start_cell_index + row.cell_count; ++cell_index) {
        const auto& cell_block_constraint = cell_block_constraints[cell_index];
        const auto [cell_block_size, is_initial_block_size_indefinite] =
            NGTableAlgorithmUtils::ComputeCellBlockSize(
                cell_block_constraint, rows, row_index, border_spacing,
                is_table_block_size_specified);

        LayoutUnit rowspan_block_size =
            cell_block_constraint.effective_rowspan > 1 ? cell_block_size
                                                        : kIndefiniteSize;

        data->cells.emplace_back(
            cell_block_constraint.borders, rowspan_block_size,
            cell_block_constraint.column_index,
            is_initial_block_size_indefinite,
            cell_block_constraint
                .has_descendant_that_depends_on_percentage_block_size);
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
  STACK_ALLOCATED();

 public:
  void VisitCol(const NGLayoutInputNode& col,
                wtf_size_t start_column_index,
                wtf_size_t span) {
    wtf_size_t end_column_index = start_column_index + span - 1;
    DCHECK_LE(end_column_index, column_locations.size() - 1);
    LayoutUnit column_inline_size = column_locations[end_column_index].offset +
                                    column_locations[end_column_index].size -
                                    column_locations[start_column_index].offset;
    col.GetLayoutBox()->SetLogicalWidth(column_inline_size);
    col.GetLayoutBox()->SetLogicalHeight(table_grid_block_size);
    column_geometries.emplace_back(start_column_index, span,
                                   column_locations[start_column_index].offset -
                                       border_spacing.inline_size,
                                   column_inline_size, col);
  }

  void EnterColgroup(const NGLayoutInputNode& colgroup,
                     wtf_size_t start_column_index) {}

  void LeaveColgroup(const NGLayoutInputNode& colgroup,
                     wtf_size_t start_column_index,
                     wtf_size_t span,
                     bool has_children) {
    if (span == 0)
      return;
    wtf_size_t last_column_index = start_column_index + span - 1;
    LayoutUnit colgroup_size = column_locations[last_column_index].offset +
                               column_locations[last_column_index].size -
                               column_locations[start_column_index].offset;
    colgroup.GetLayoutBox()->SetLogicalWidth(colgroup_size);
    colgroup.GetLayoutBox()->SetLogicalHeight(table_grid_block_size);
    column_geometries.emplace_back(start_column_index, span,
                                   column_locations[start_column_index].offset -
                                       border_spacing.inline_size,
                                   colgroup_size, colgroup);
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
                  if (b.node.IsTableColgroup())
                    return a.start_column < b.start_column;
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

  ColumnGeometriesBuilder(const Vector<NGTableColumnLocation>& column_locations,
                          LayoutUnit table_grid_block_size,
                          const LogicalSize& border_spacing)
      : column_locations(column_locations),
        table_grid_block_size(table_grid_block_size),
        border_spacing(border_spacing) {}
  NGTableFragmentData::ColumnGeometries column_geometries;
  const Vector<NGTableColumnLocation>& column_locations;
  const LayoutUnit table_grid_block_size;
  const LogicalSize& border_spacing;
};

LayoutUnit ComputeTableSizeFromColumns(
    const Vector<NGTableColumnLocation>& column_locations,
    const NGBoxStrut& table_border_padding,
    const LogicalSize& border_spacing) {
  return column_locations.back().offset + column_locations.back().size +
         table_border_padding.InlineSum() + border_spacing.inline_size;
}

}  // namespace

LayoutUnit NGTableLayoutAlgorithm::ComputeTableInlineSize(
    const NGTableNode& table,
    const NGConstraintSpace& space,
    const NGBoxStrut& table_border_padding) {
  const bool is_fixed_layout = table.Style().IsFixedTableLayout();
  // Tables need autosizer.
  absl::optional<TextAutosizer::TableLayoutScope> text_autosizer;
  if (!is_fixed_layout)
    text_autosizer.emplace(To<LayoutNGTable>(table.GetLayoutBox()));

  const LogicalSize border_spacing = table.Style().TableBorderSpacing();
  NGTableGroupedChildren grouped_children(table);
  scoped_refptr<const NGTableBorders> table_borders = table.GetTableBorders();

  // Compute min/max inline constraints.
  const scoped_refptr<const NGTableTypes::Columns> column_constraints =
      table.GetColumnConstraints(grouped_children, table_border_padding);

  const NGTableTypes::Caption caption_constraint =
      ComputeCaptionConstraint(space, table.Style(), grouped_children);

  const LayoutUnit undistributable_space = ComputeUndistributableTableSpace(
      *column_constraints, table_border_padding.InlineSum(),
      border_spacing.inline_size);

  const LayoutUnit assignable_table_inline_size =
      ComputeAssignableTableInlineSize(
          table, space, *column_constraints, caption_constraint,
          undistributable_space, table_border_padding, is_fixed_layout);
  if (column_constraints->data.IsEmpty()) {
    return ComputeEmptyTableInlineSize(
        space, table.Style(), assignable_table_inline_size,
        undistributable_space, caption_constraint, table_border_padding,
        table_borders->IsCollapsed());
  }

  const Vector<LayoutUnit> column_sizes =
      NGTableAlgorithmHelpers::SynchronizeAssignableTableInlineSizeAndColumns(
          assignable_table_inline_size, is_fixed_layout, *column_constraints);

  // Final inline size must depend on column locations, because columns can be
  // hidden.
  Vector<NGTableColumnLocation> column_locations;
  bool has_collapsed_columns;
  ComputeLocationsFromColumns(
      *column_constraints, column_sizes, border_spacing.inline_size,
      /* collapse_columns */ true, &column_locations, &has_collapsed_columns);
  return std::max(ComputeTableSizeFromColumns(
                      column_locations, table_border_padding, border_spacing),
                  caption_constraint.min_size);
}

LayoutUnit NGTableLayoutAlgorithm::ComputeCaptionBlockSize(
    const NGTableNode& node,
    const NGConstraintSpace& space,
    const LayoutUnit table_inline_size) {
  NGTableGroupedChildren grouped_children(node);
  LayoutUnit captions_block_size;

  ComputeCaptionFragments(space, node.Style(), grouped_children,
                          table_inline_size, /* captions */ nullptr,
                          captions_block_size);
  return captions_block_size;
}

const NGLayoutResult* NGTableLayoutAlgorithm::Layout() {
  const bool is_fixed_layout = Style().IsFixedTableLayout();
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
      ComputeCaptionConstraint(ConstraintSpace(), Style(), grouped_children);
  // Compute assignable table inline size.
  // Standard: https://www.w3.org/TR/css-tables-3/#width-distribution
  const LayoutUnit undistributable_space = ComputeUndistributableTableSpace(
      *column_constraints, border_padding.InlineSum(),
      border_spacing.inline_size);

  const LayoutUnit assignable_table_inline_size =
      ComputeAssignableTableInlineSize(
          Node(), ConstraintSpace(), *column_constraints, caption_constraint,
          undistributable_space, border_padding, is_fixed_layout);

  // Distribute assignable table width.
  const Vector<LayoutUnit> column_sizes =
      NGTableAlgorithmHelpers::SynchronizeAssignableTableInlineSizeAndColumns(
          assignable_table_inline_size, is_fixed_layout, *column_constraints);

  Vector<NGTableColumnLocation> column_locations;
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

  // Before we can determine the block-size of the sections/rows, we need to
  // layout all of our captions.
  //
  // The block-size taken by the captions, *subtracts* from the available
  // block-size given to the table-grid.
  HeapVector<CaptionResult> captions;
  LayoutUnit captions_block_size;
  ComputeCaptionFragments(ConstraintSpace(), Style(), grouped_children,
                          container_builder_.InlineSize(), &captions,
                          captions_block_size);

  NGTableTypes::Rows rows;
  NGTableTypes::CellBlockConstraints cell_block_constraints;
  NGTableTypes::Sections sections;
  LayoutUnit minimal_table_grid_block_size;
  ComputeRows(table_inline_size_before_collapse - border_padding.InlineSum(),
              grouped_children, column_locations, *table_borders,
              border_spacing, border_padding, captions_block_size, &rows,
              &cell_block_constraints, &sections,
              &minimal_table_grid_block_size);

  if (has_collapsed_columns) {
    ComputeLocationsFromColumns(
        *column_constraints, column_sizes, border_spacing.inline_size,
        /* shrink_collapsed */ true, &column_locations, &has_collapsed_columns);
  }
#if DCHECK_IS_ON()
  // To avoid number rounding issues, instead of comparing sizes
  // equality, we check whether sizes differ in less than a pixel.
  if (!has_collapsed_columns) {
    // Columns define table whose inline size equals InitialFragmentGeometry.
    DCHECK_LT(
        (table_inline_size_before_collapse - container_builder_.InlineSize())
            .Abs(),
        LayoutUnit(1));
  } else if (ConstraintSpace().IsFixedInlineSize()) {
    // Collapsed columns + fixed inline size: columns define table whose
    // inline size is less or equal InitialFragmentGeometry.
    LayoutUnit table_inline_size =
        std::max(ComputeTableSizeFromColumns(column_locations, border_padding,
                                             border_spacing),
                 caption_constraint.min_size);
    DCHECK_LE(table_inline_size, container_builder_.InlineSize());
  } else {
    LayoutUnit table_inline_size =
        std::max(ComputeTableSizeFromColumns(column_locations, border_padding,
                                             border_spacing),
                 caption_constraint.min_size);
    DCHECK_LT((table_inline_size - container_builder_.InlineSize()).Abs(),
              LayoutUnit(1));
  }
#endif

  return GenerateFragment(container_builder_.InlineSize(),
                          minimal_table_grid_block_size, grouped_children,
                          column_locations, rows, cell_block_constraints,
                          sections, captions, *table_borders,
                          is_grid_empty ? LogicalSize() : border_spacing);
}

MinMaxSizesResult NGTableLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  const bool is_fixed_layout = Style().IsFixedTableLayout();
  // Tables need autosizer.
  absl::optional<TextAutosizer::TableLayoutScope> text_autosizer;
  if (!is_fixed_layout)
    text_autosizer.emplace(To<LayoutNGTable>(Node().GetLayoutBox()));

  const LogicalSize border_spacing = Style().TableBorderSpacing();
  NGTableGroupedChildren grouped_children(Node());
  const scoped_refptr<const NGTableBorders> table_borders =
      Node().GetTableBorders();
  const NGBoxStrut border_padding = container_builder_.BorderPadding();

  const scoped_refptr<const NGTableTypes::Columns> column_constraints =
      Node().GetColumnConstraints(grouped_children, border_padding);
  const NGTableTypes::Caption caption_constraint =
      ComputeCaptionConstraint(ConstraintSpace(), Style(), grouped_children);

  const LayoutUnit undistributable_space = ComputeUndistributableTableSpace(
      *column_constraints, border_padding.InlineSum(),
      border_spacing.inline_size);

  const MinMaxSizes grid_min_max =
      NGTableAlgorithmHelpers::ComputeGridInlineMinMax(
          Node(), *column_constraints, undistributable_space, is_fixed_layout,
          /* is_layout_pass */ false);

  MinMaxSizes min_max{
      std::max(grid_min_max.min_size, caption_constraint.min_size),
      std::max(grid_min_max.max_size, caption_constraint.min_size)};

  if (is_fixed_layout && Style().LogicalWidth().IsPercentOrCalc()) {
    min_max.max_size = NGTableTypes::kTableMaxInlineSize;
  }
  DCHECK_LE(min_max.min_size, min_max.max_size);
  return MinMaxSizesResult{min_max,
                           /* depends_on_block_constraints */ false};
}

void NGTableLayoutAlgorithm::ComputeRows(
    const LayoutUnit table_grid_inline_size,
    const NGTableGroupedChildren& grouped_children,
    const Vector<NGTableColumnLocation>& column_locations,
    const NGTableBorders& table_borders,
    const LogicalSize& border_spacing,
    const NGBoxStrut& table_border_padding,
    const LayoutUnit captions_block_size,
    NGTableTypes::Rows* rows,
    NGTableTypes::CellBlockConstraints* cell_block_constraints,
    NGTableTypes::Sections* sections,
    LayoutUnit* minimal_table_grid_block_size) {
  DCHECK_EQ(rows->size(), 0u);
  DCHECK_EQ(cell_block_constraints->size(), 0u);

  const bool is_table_block_size_specified = !Style().LogicalHeight().IsAuto();
  LayoutUnit total_table_block_size;
  wtf_size_t section_index = 0;
  for (auto it = grouped_children.begin(); it != grouped_children.end(); ++it) {
    NGTableAlgorithmUtils::ComputeSectionMinimumRowBlockSizes(
        *it, table_grid_inline_size, is_table_block_size_specified,
        column_locations, table_borders, border_spacing.block_size,
        section_index++, it.TreatAsTBody(), sections, rows,
        cell_block_constraints);
    total_table_block_size += sections->back().block_size;
  }

  LayoutUnit css_table_block_size;
  if (ConstraintSpace().IsInitialBlockSizeIndefinite() &&
      !ConstraintSpace().IsFixedBlockSize()) {
    // We get here when a flexbox wants to use the table's intrinsic height as
    // an input to the flex algorithm.
    css_table_block_size = kIndefiniteSize;
  } else {
    // If we can correctly resolve our min-block-size we want to distribute
    // sections/rows into this space. Pass a definite intrinsic block-size into
    // |ComputeBlockSizeForFragment| to force it to resolve.
    LayoutUnit intrinsic_block_size =
        BlockLengthUnresolvable(ConstraintSpace(), Style().LogicalMinHeight())
            ? kIndefiniteSize
            : table_border_padding.BlockSum();

    css_table_block_size = ComputeBlockSizeForFragment(
        ConstraintSpace(), Style(), table_border_padding, intrinsic_block_size,
        table_grid_inline_size,
        /* available_block_size_adjustment */ captions_block_size);
  }
  // In quirks mode, empty tables ignore any specified block-size.
  const bool is_empty_quirks_mode_table =
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

  for (auto& row : *rows) {
    if (!row.is_collapsed)
      continue;

    // Collapsed rows get zero block-size, and shrink the minimum table size.
    // TODO(ikilpatrick): As written |minimal_table_grid_block_size| can go
    // negative. Investigate.
    if (*minimal_table_grid_block_size != LayoutUnit()) {
      *minimal_table_grid_block_size -= row.block_size;
      if (rows->size() > 1)
        *minimal_table_grid_block_size -= border_spacing.block_size;
    }
    row.block_size = LayoutUnit();
  }
}

// Method also sets LogicalWidth/Height on columns.
void NGTableLayoutAlgorithm::ComputeTableSpecificFragmentData(
    const NGTableGroupedChildren& grouped_children,
    const Vector<NGTableColumnLocation>& column_locations,
    const NGTableTypes::Rows& rows,
    const NGTableBorders& table_borders,
    const PhysicalRect& table_grid_rect,
    const LogicalSize& border_spacing,
    const LayoutUnit table_grid_block_size) {
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
    container_builder_.SetTableColumnGeometries(
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
    DCHECK_NE(column_locations.size(), 0u);
    fragment_borders_geometry->columns.push_back(
        column_locations.back().offset + column_locations.back().size +
        grid_inline_start);
    LayoutUnit row_offset = table_borders.TableBorder().block_start;
    for (const auto& row : rows) {
      fragment_borders_geometry->rows.push_back(row_offset);
      row_offset += row.block_size;
    }
    fragment_borders_geometry->rows.push_back(row_offset);
    // crbug.com/1179369 make sure dimensions of table_borders and
    // fragment_borders_geometry are consistent.
    DCHECK_LE(table_borders.EdgesPerRow() / 2,
              fragment_borders_geometry->columns.size());
    container_builder_.SetTableCollapsedBorders(table_borders);
    container_builder_.SetTableCollapsedBordersGeometry(
        std::move(fragment_borders_geometry));
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
const NGLayoutResult* NGTableLayoutAlgorithm::GenerateFragment(
    const LayoutUnit table_inline_size,
    const LayoutUnit minimal_table_grid_block_size,
    const NGTableGroupedChildren& grouped_children,
    const Vector<NGTableColumnLocation>& column_locations,
    const NGTableTypes::Rows& rows,
    const NGTableTypes::CellBlockConstraints& cell_block_constraints,
    const NGTableTypes::Sections& sections,
    const HeapVector<CaptionResult>& captions,
    const NGTableBorders& table_borders,
    const LogicalSize& border_spacing) {
  const auto table_writing_direction = Style().GetWritingDirection();
  scoped_refptr<const NGTableConstraintSpaceData> constraint_space_data =
      CreateConstraintSpaceData(Style(), column_locations, sections, rows,
                                cell_block_constraints, border_spacing);

  const NGBoxStrut border_padding = container_builder_.BorderPadding();
  LayoutUnit child_block_offset;
  bool needs_end_border_spacing = false;
  bool has_processed_first_child = false;

  auto AddCaptionResult = [&](const CaptionResult& caption,
                              LayoutUnit* block_offset) -> void {
    NGBlockNode node = caption.node;
    node.StoreMargins(
        caption.margins.ConvertToPhysical(table_writing_direction));

    *block_offset += caption.margins.block_start;
    container_builder_.AddResult(
        *caption.layout_result,
        LogicalOffset(caption.margins.inline_start, *block_offset));

    *block_offset += NGFragment(table_writing_direction,
                                caption.layout_result->PhysicalFragment())
                         .BlockSize() +
                     caption.margins.block_end;
  };

  // We have already laid out the captions, in order to calculate the table grid
  // size. We can re-use these results now, unless we're in block fragmentation.
  // In that case we need to lay them out again now, so that they fragment and
  // resume properly.
  const bool relayout_captions = ConstraintSpace().HasBlockFragmentation();

  // Add all the top captions.
  if (!relayout_captions) {
    for (const auto& caption : captions) {
      if (caption.node.Style().CaptionSide() == ECaptionSide::kTop)
        AddCaptionResult(caption, &child_block_offset);
    }
  }

  // Section setup.
  const LayoutUnit section_available_inline_size =
      (table_inline_size - border_padding.InlineSum() -
       border_spacing.inline_size * 2)
          .ClampNegativeToZero();

  auto CreateSectionConstraintSpace = [&table_writing_direction,
                                       &section_available_inline_size,
                                       &constraint_space_data, &sections,
                                       this](const NGBlockNode& section,
                                             LayoutUnit block_offset,
                                             wtf_size_t section_index) {
    NGConstraintSpaceBuilder section_space_builder(
        table_writing_direction.GetWritingMode(), table_writing_direction,
        /* is_new_fc */ true);

    LogicalSize available_size = {section_available_inline_size,
                                  kIndefiniteSize};

    // Sections without rows can receive redistributed height from the table.
    if (constraint_space_data->sections[section_index].row_count == 0) {
      section_space_builder.SetIsFixedBlockSize(true);
      available_size.block_size = sections[section_index].block_size;
    }

    section_space_builder.SetAvailableSize(available_size);
    section_space_builder.SetIsFixedInlineSize(true);
    section_space_builder.SetPercentageResolutionSize(
        {section_available_inline_size, kIndefiniteSize});
    section_space_builder.SetTableSectionData(constraint_space_data,
                                              section_index);

    if (ConstraintSpace().HasBlockFragmentation()) {
      SetupSpaceBuilderForFragmentation(
          ConstraintSpace(), section, block_offset, &section_space_builder,
          /* is_new_fc */ true,
          container_builder_.RequiresContentBeforeBreaking());
    }

    return section_space_builder.ToConstraintSpace();
  };

  auto GridBlockSize = [&border_padding, &minimal_table_grid_block_size, this](
                           LayoutUnit start_offset,
                           LayoutUnit end_offset) -> LayoutUnit {
    DCHECK_GE(end_offset, start_offset);
    LayoutUnit grid_block_size = end_offset - start_offset;
    grid_block_size += border_padding.block_end;
    if (!IsResumingLayout(BreakToken())) {
      grid_block_size =
          std::max(grid_block_size, minimal_table_grid_block_size);
    }
    return grid_block_size;
  };

  // Generate section fragments, and also caption fragments, if we need to
  // regenerate them (block fragmentation).
  LogicalOffset section_offset;
  section_offset.inline_offset =
      border_padding.inline_start + border_spacing.inline_size;
  section_offset.block_offset = child_block_offset + border_padding.block_start;

  absl::optional<LayoutUnit> table_baseline;

  LayoutUnit first_section_block_offset = child_block_offset;
  LayoutUnit grid_block_size;
  bool broke_inside = false;
  bool is_past_first_section_start = false;
  bool is_past_last_section_end = false;
  NGTableChildIterator child_iterator(grouped_children, BreakToken());
  for (auto entry = child_iterator.NextChild();
       NGBlockNode child = entry.GetNode();
       entry = child_iterator.NextChild()) {
    const NGBlockBreakToken* child_break_token = entry.GetBreakToken();
    const NGLayoutResult* child_result;
    LayoutUnit child_inline_offset;
    if (child.IsTableCaption()) {
      if (!relayout_captions)
        continue;
      if (child.Style().CaptionSide() == ECaptionSide::kBottom &&
          !is_past_last_section_end) {
        // We found a bottom caption, which means that we're done with all the
        // sections. We need to calculate the grid size now, so that we set the
        // block-offset for the caption correctly.
        is_past_last_section_end = true;
        if (needs_end_border_spacing)
          section_offset.block_offset += border_spacing.block_size;
        grid_block_size = GridBlockSize(first_section_block_offset,
                                        section_offset.block_offset);
        child_block_offset = first_section_block_offset + grid_block_size;
      }

      LogicalSize available_size(container_builder_.InlineSize(),
                                 kIndefiniteSize);
      NGConstraintSpace child_space =
          CreateCaptionConstraintSpace(ConstraintSpace(), Style(), child,
                                       available_size, child_block_offset);
      CaptionResult caption = LayoutCaption(
          ConstraintSpace(), Style(), container_builder_.InlineSize(),
          child_space, child, child_break_token);
      child_result = caption.layout_result;
      child_block_offset += caption.margins.block_start;
      child_inline_offset = caption.margins.inline_start;
    } else {
      if (!is_past_first_section_start) {
        is_past_first_section_start = true;
        first_section_block_offset = child_block_offset;
        child_block_offset += border_padding.block_start;
      }

      if (ConstraintSpace().HasBlockFragmentation()) {
        // TODO(mstensho): Border-spacing should only be included if there are
        // table parts inside, but currently we need to lay out before we can
        // check that. Always assume that we need border spacing for now (as
        // long as we're not resuming inside the section).
        if (!IsResumingLayout(child_break_token))
          child_block_offset += border_spacing.block_size;
      }

      NGConstraintSpace child_space = CreateSectionConstraintSpace(
          child, child_block_offset, entry.GetSectionIndex());
      child_result = child.Layout(child_space, child_break_token);
      child_inline_offset = section_offset.inline_offset;
    }
    if (ConstraintSpace().HasBlockFragmentation()) {
      LayoutUnit fragmentainer_block_offset =
          ConstraintSpace().FragmentainerOffsetAtBfc() + child_block_offset;
      NGBreakStatus break_status = BreakBeforeChildIfNeeded(
          ConstraintSpace(), child, *child_result, fragmentainer_block_offset,
          has_processed_first_child, &container_builder_);
      if (break_status != NGBreakStatus::kContinue) {
        broke_inside = true;
        break;
      }
    }

    const auto& physical_fragment =
        To<NGPhysicalBoxFragment>(child_result->PhysicalFragment());
    NGBoxFragment fragment(table_writing_direction, physical_fragment);
    if (!child.IsTableCaption()) {
      if (fragment.HasDescendantsForTablePart()) {
        // Border-spacing has pre-emptively been added if we're participating in
        // block fragmentation. Otherwise add it now.
        if (!ConstraintSpace().HasBlockFragmentation())
          child_block_offset += border_spacing.block_size;
        // We want to add border-spacing after this section, but not if the
        // current fragment is past the block-end of the section. This might
        // happen if there are overflowing descendants, and this section should
        // just create an zero-sized fragment.
        needs_end_border_spacing =
            !child_break_token || !child_break_token->IsAtBlockEnd();
      }
      if (!table_baseline) {
        if (const auto& section_baseline = fragment.Baseline())
          table_baseline = *section_baseline + child_block_offset;
      }
    }

    container_builder_.AddResult(
        *child_result, LogicalOffset(child_inline_offset, child_block_offset));
    child_block_offset += fragment.BlockSize();

    if (!child.IsTableCaption())
      section_offset.block_offset = child_block_offset;

    if (ConstraintSpace().HasBlockFragmentation()) {
      has_processed_first_child = true;
      if (container_builder_.HasInflowChildBreakInside()) {
        broke_inside = true;
        break;
      }
    }
  }

  if (!child_iterator.NextChild())
    container_builder_.SetHasSeenAllChildren();
  if (broke_inside)
    needs_end_border_spacing = false;

  if (needs_end_border_spacing)
    section_offset.block_offset += border_spacing.block_size;
  LayoutUnit column_block_size =
      section_offset.block_offset - border_padding.block_start;
  if (needs_end_border_spacing)
    column_block_size -= border_spacing.block_size * 2;

  if (!is_past_last_section_end) {
    // If we haven't already calculated the grid size, do so now.
    grid_block_size =
        GridBlockSize(first_section_block_offset, section_offset.block_offset);
    child_block_offset = first_section_block_offset + grid_block_size;
  }

  const LogicalRect table_grid_rect(LayoutUnit(), first_section_block_offset,
                                    container_builder_.InlineSize(),
                                    grid_block_size);

  // Add all the bottom captions.
  if (!relayout_captions) {
    for (const auto& caption : captions) {
      if (caption.node.Style().CaptionSide() == ECaptionSide::kBottom)
        AddCaptionResult(caption, &child_block_offset);
    }
  }

  LayoutUnit previously_consumed_block_size;
  if (UNLIKELY(BreakToken()))
    previously_consumed_block_size = BreakToken()->ConsumedBlockSize();

  LayoutUnit block_size = std::max(
      grid_block_size, child_block_offset + previously_consumed_block_size);
  container_builder_.SetIntrinsicBlockSize(block_size);
  if (ConstraintSpace().IsFixedBlockSize())
    block_size = ConstraintSpace().AvailableSize().block_size;

  container_builder_.SetFragmentsTotalBlockSize(block_size);

  const WritingModeConverter grid_converter(
      Style().GetWritingDirection(),
      ToPhysicalSize(container_builder_.Size(),
                     table_writing_direction.GetWritingMode()));

  ComputeTableSpecificFragmentData(grouped_children, column_locations, rows,
                                   table_borders,
                                   grid_converter.ToPhysical(table_grid_rect),
                                   border_spacing, column_block_size);

  if (RuntimeEnabledFeatures::MathMLCoreEnabled() && Node().GetDOMNode() &&
      Node().GetDOMNode()->HasTagName(mathml_names::kMtableTag))
    table_baseline = MathTableBaseline(Style(), child_block_offset);
  if (table_baseline)
    container_builder_.SetBaseline(*table_baseline);

  container_builder_.SetIsTableNGPart();

  if (UNLIKELY(InvolvedInBlockFragmentation(container_builder_))) {
    NGBreakStatus status = FinishFragmentation(
        Node(), ConstraintSpace(), BorderPadding().block_end,
        FragmentainerSpaceAtBfcStart(ConstraintSpace()), &container_builder_);
    if (status == NGBreakStatus::kDisableFragmentation)
      return container_builder_.Abort(NGLayoutResult::kDisableFragmentation);
    // TODO(mstensho): Deal with early-breaks.
    DCHECK_EQ(status, NGBreakStatus::kContinue);
  }

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), &container_builder_).Run();
  return container_builder_.ToBoxFragment();
}

}  // namespace blink
