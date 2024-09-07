// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/table/table_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/disable_layout_side_effects_scope.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/mathml/math_layout_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/space_utils.h"
#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_caption.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_column.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_column_visitor.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_section.h"
#include "third_party/blink/renderer/core/layout/table/table_borders.h"
#include "third_party/blink/renderer/core/layout/table/table_break_token_data.h"
#include "third_party/blink/renderer/core/layout/table/table_child_iterator.h"
#include "third_party/blink/renderer/core/layout/table/table_constraint_space_data.h"
#include "third_party/blink/renderer/core/layout/table/table_fragment_data.h"
#include "third_party/blink/renderer/core/layout/table/table_layout_utils.h"
#include "third_party/blink/renderer/core/layout/table/table_node.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/mathml/mathml_element.h"
#include "third_party/blink/renderer/core/mathml_names.h"

namespace blink {

namespace {

TableTypes::Caption ComputeCaptionConstraint(
    const ConstraintSpace& table_space,
    const ComputedStyle& table_style,
    const TableGroupedChildren& grouped_children) {
  // Caption inline size constraints.
  TableTypes::Caption caption_min_max;
  for (const BlockNode& caption : grouped_children.captions) {
    // Caption %-block-sizes are treated as auto, as there isn't a reasonable
    // block-size to resolve against.
    MinMaxConstraintSpaceBuilder builder(table_space, table_style, caption,
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

ConstraintSpace CreateCaptionConstraintSpace(
    const ConstraintSpace& table_constraint_space,
    const ComputedStyle& table_style,
    const BlockNode& caption,
    LogicalSize available_size,
    std::optional<LayoutUnit> block_offset = std::nullopt) {
  ConstraintSpaceBuilder builder(table_constraint_space,
                                 caption.Style().GetWritingDirection(),
                                 /* is_new_fc */ true);
  SetOrthogonalFallbackInlineSizeIfNeeded(table_style, caption, &builder);
  builder.SetAvailableSize(available_size);
  builder.SetPercentageResolutionSize(available_size);
  builder.SetInlineAutoBehavior(AutoSizeBehavior::kStretchImplicit);

  // If a block-offset is specified, it means that table captions are laid out
  // as part of normal table child layout (rather than in initial table
  // block-size calculation). That is normally only necessary if block
  // fragmentation is enabled, but may also occur if block fragmentation *was*
  // enabled for previous fragments, but is disabled for this fragment, because
  // of overflow clipping.
  if (block_offset && table_constraint_space.HasBlockFragmentation()) {
    SetupSpaceBuilderForFragmentation(
        table_constraint_space, caption,
        table_constraint_space.FragmentainerOffset() + *block_offset,
        table_constraint_space.FragmentainerBlockSize(),
        /*requires_content_before_breaking=*/false, &builder);
  }

  return builder.ToConstraintSpace();
}

TableLayoutAlgorithm::CaptionResult LayoutCaption(
    const ConstraintSpace& table_constraint_space,
    const ComputedStyle& table_style,
    LayoutUnit table_inline_size,
    const ConstraintSpace& caption_constraint_space,
    const BlockNode& caption,
    BoxStrut margins,
    const BlockBreakToken* break_token = nullptr,
    const EarlyBreak* early_break = nullptr) {
  const LayoutResult* layout_result =
      caption.Layout(caption_constraint_space, break_token, early_break);
  DCHECK_EQ(layout_result->Status(), LayoutResult::kSuccess);

  LogicalFragment fragment(table_constraint_space.GetWritingDirection(),
                           layout_result->GetPhysicalFragment());
  ResolveInlineAutoMargins(caption.Style(), table_style, table_inline_size,
                           fragment.InlineSize(), &margins);

  return {caption, layout_result, margins};
}

// Compute the margins of a caption as best as we can before layout (we need to
// lay out before we can resolve auto inline-margins). Remember that captions
// aren't actually inside the table, so its the *border-box* size of the table
// that matters here (not the content-box) when it comes to resolving
// percentages.
BoxStrut ComputeCaptionMargins(
    const ConstraintSpace& table_constraint_space,
    const BlockNode& caption,
    LayoutUnit table_border_box_inline_size,
    const BlockBreakToken* caption_break_token = nullptr) {
  BoxStrut margins =
      ComputeMarginsFor(caption.Style(), table_border_box_inline_size,
                        table_constraint_space.GetWritingDirection());
  AdjustMarginsForFragmentation(caption_break_token, &margins);
  return margins;
}

void ComputeCaptionFragments(
    const BoxFragmentBuilder& table_builder,
    const ComputedStyle& table_style,
    const TableGroupedChildren& grouped_children,
    HeapVector<TableLayoutAlgorithm::CaptionResult>* captions,
    LayoutUnit& captions_block_size) {
  const ConstraintSpace& table_constraint_space =
      table_builder.GetConstraintSpace();
  const LayoutUnit table_inline_size = table_builder.InlineSize();
  const LogicalSize available_size = {table_inline_size, kIndefiniteSize};
  for (BlockNode caption : grouped_children.captions) {
    BoxStrut margins = ComputeCaptionMargins(table_constraint_space, caption,
                                             table_inline_size);
    ConstraintSpace caption_constraint_space = CreateCaptionConstraintSpace(
        table_constraint_space, table_style, caption, available_size);

    // If we are discarding the results (compute-only) and we are after layout
    // (|!NeedsLayout|), or if we are in block fragmentation, make sure not to
    // update the cached layout results. If we are block fragmented, a node may
    // generate multiple fragments, so make sure that we keep the fragments
    // generated and stored in the actual layout pass.
    //
    // TODO(mstensho): We can remove this if we only perform this operation once
    // per table node (and e.g. store the table data in the break tokens).
    std::optional<DisableLayoutSideEffectsScope> disable_side_effects;
    if ((!captions && !caption.GetLayoutBox()->NeedsLayout()) ||
        InvolvedInBlockFragmentation(table_builder)) {
      disable_side_effects.emplace();
    }

    TableLayoutAlgorithm::CaptionResult caption_result =
        LayoutCaption(table_constraint_space, table_style, table_inline_size,
                      caption_constraint_space, caption, margins);
    LogicalFragment fragment(
        table_constraint_space.GetWritingDirection(),
        caption_result.layout_result->GetPhysicalFragment());
    captions_block_size +=
        fragment.BlockSize() + caption_result.margins.BlockSum();
    if (captions)
      captions->push_back(caption_result);
  }
}

LayoutUnit ComputeUndistributableTableSpace(
    const TableTypes::Columns& column_constraints,
    LayoutUnit inline_table_border_padding,
    LayoutUnit inline_border_spacing) {
  unsigned inline_space_count = 2;
  bool is_first_column = true;
  for (const TableTypes::Column& column : column_constraints.data) {
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
    const ConstraintSpace& space,
    const ComputedStyle& table_style,
    const LayoutUnit assignable_table_inline_size,
    const LayoutUnit undistributable_space,
    const TableTypes::Caption& caption_constraint,
    const BoxStrut& table_border_padding,
    const bool has_collapsed_borders) {
  // If table has a css inline size, use that.
  // TODO(https://crbug.com/313072): Should these IsAuto calls handle
  // intrinsic sizing keywords or calc-size() differently, e.g., by using
  // HasAutoOrContentOrIntrinsic rather than just HasAuto?
  if (space.IsFixedInlineSize() || space.IsInlineAutoBehaviorStretch() ||
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
    const TableNode& table,
    const ConstraintSpace& space,
    const TableTypes::Columns& column_constraints,
    const TableTypes::Caption& caption_constraint,
    const LayoutUnit undistributable_space,
    const BoxStrut& table_border_padding,
    const bool is_fixed_layout) {
  if (space.IsFixedInlineSize()) {
    return (space.AvailableSize().inline_size - undistributable_space)
        .ClampNegativeToZero();
  }

  const MinMaxSizes grid_min_max = ComputeGridInlineMinMax(
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
void ComputeLocationsFromColumns(const TableTypes::Columns& column_constraints,
                                 const Vector<LayoutUnit>& column_sizes,
                                 LayoutUnit inline_border_spacing,
                                 bool shrink_collapsed,
                                 Vector<TableColumnLocation>* column_locations,
                                 bool* has_collapsed_columns) {
  *has_collapsed_columns = false;
  column_locations->resize(column_constraints.data.size());
  if (column_locations->empty())
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

scoped_refptr<const TableConstraintSpaceData> CreateConstraintSpaceData(
    const ComputedStyle& style,
    const Vector<TableColumnLocation>& column_locations,
    const TableTypes::Sections& sections,
    const TableTypes::Rows& rows,
    const TableTypes::CellBlockConstraints& cell_block_constraints,
    const LogicalSize& border_spacing) {
  // TODO(https://crbug.com/313072): These should probably use
  // HasAutoOrContentOrIntrinsic rather than just HasAuto.
  bool is_table_block_size_specified = !style.LogicalHeight().HasAuto();
  scoped_refptr<TableConstraintSpaceData> data =
      base::MakeRefCounted<TableConstraintSpaceData>();
  data->table_writing_direction = style.GetWritingDirection();
  data->table_border_spacing = border_spacing;
  data->is_table_block_size_specified = is_table_block_size_specified;
  data->has_collapsed_borders =
      style.BorderCollapse() == EBorderCollapse::kCollapse;
  data->column_locations = column_locations;

  data->sections.reserve(sections.size());
  for (const auto& section : sections)
    data->sections.emplace_back(section.start_row, section.row_count);
  data->rows.reserve(rows.size());
  for (const auto& row : rows) {
    data->rows.emplace_back(row.block_size, row.start_cell_index,
                            row.cell_count, row.baseline, row.is_collapsed);
  }
  data->cells.reserve(cell_block_constraints.size());
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
            ComputeCellBlockSize(cell_block_constraint, rows, row_index,
                                 border_spacing, is_table_block_size_specified);

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
// in TableFragmentData. Geometry data is also copied
// back to LayoutObject.
class ColumnGeometriesBuilder {
  STACK_ALLOCATED();

 public:
  void VisitCol(const LayoutInputNode& col,
                wtf_size_t start_column_index,
                wtf_size_t span) {
    wtf_size_t end_column_index = start_column_index + span - 1;
    DCHECK_LE(end_column_index, column_locations.size() - 1);
    LayoutUnit column_inline_size = column_locations[end_column_index].offset +
                                    column_locations[end_column_index].size -
                                    column_locations[start_column_index].offset;

    column_geometries.emplace_back(start_column_index, span,
                                   column_locations[start_column_index].offset -
                                       column_locations[0].offset,
                                   column_inline_size, col);
  }

  void EnterColgroup(const LayoutInputNode& colgroup,
                     wtf_size_t start_column_index) {}

  void LeaveColgroup(const LayoutInputNode& colgroup,
                     wtf_size_t start_column_index,
                     wtf_size_t span,
                     bool has_children) {
    if (span == 0)
      return;
    wtf_size_t last_column_index = start_column_index + span - 1;
    LayoutUnit colgroup_size = column_locations[last_column_index].offset +
                               column_locations[last_column_index].size -
                               column_locations[start_column_index].offset;

    column_geometries.emplace_back(start_column_index, span,
                                   column_locations[start_column_index].offset -
                                       column_locations[0].offset,
                                   colgroup_size, colgroup);
  }

  void Sort() {
    // Geometries need to be sorted because this must be true:
    // - parent COLGROUP must come before child COLs.
    // - child COLs are in ascending order.
    std::sort(column_geometries.begin(), column_geometries.end(),
              [](const TableFragmentData::ColumnGeometry& a,
                 const TableFragmentData::ColumnGeometry& b) {
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

    wtf_size_t column_idx = 0;
    for (const auto& col : column_geometries) {
      To<LayoutTableColumn>(col.node.GetLayoutBox())
          ->SetColumnIndex(column_idx);
      column_idx++;
    }
  }

  ColumnGeometriesBuilder(const Vector<TableColumnLocation>& column_locations,
                          LayoutUnit table_column_block_size)
      : column_locations(column_locations),
        table_column_block_size(table_column_block_size) {}
  TableFragmentData::ColumnGeometries column_geometries;
  const Vector<TableColumnLocation>& column_locations;
  const LayoutUnit table_column_block_size;
};

LayoutUnit ComputeTableSizeFromColumns(
    const Vector<TableColumnLocation>& column_locations,
    const BoxStrut& table_border_padding,
    const LogicalSize& border_spacing) {
  return column_locations.back().offset + column_locations.back().size +
         table_border_padding.InlineSum() + border_spacing.inline_size;
}

// Border-box block extent of what CSS calls the "table box" [1]
// (i.e. everything except for captions).
//
// [1] https://www.w3.org/TR/CSS22/tables.html#model
struct TableBoxExtent {
  LayoutUnit start;
  LayoutUnit end;
};

// Call when beginning layout of the table box (typically right before laying
// out the first section).
TableBoxExtent BeginTableBoxLayout(
    LayoutUnit block_start_border_edge,
    LayoutUnit table_border_padding_block_start) {
  return {block_start_border_edge,
          block_start_border_edge + table_border_padding_block_start};
}

// Call when done with layout of the table box (typically right after having
// laid out the last table section).
LayoutUnit EndTableBoxLayout(LayoutUnit table_border_padding_block_end,
                             LayoutUnit border_spacing_after_last_section,
                             LayoutUnit minimal_table_grid_block_size,
                             TableBoxExtent* extent,
                             LayoutUnit* grid_block_size_inflation) {
  DCHECK_LE(extent->start, extent->end);
  extent->end +=
      border_spacing_after_last_section + table_border_padding_block_end;
  LayoutUnit sections_total_size = extent->end - extent->start;
  LayoutUnit grid_block_size =
      std::max(sections_total_size, minimal_table_grid_block_size);
  extent->end = extent->start + grid_block_size;

  // Record how much minimal_table_grid_block_size inflated the grid
  // block-size. This should be excluded from intrinsic block-size.
  *grid_block_size_inflation = grid_block_size - sections_total_size;

  return extent->end;
}

}  // namespace

LayoutUnit TableLayoutAlgorithm::ComputeTableInlineSize(
    const TableNode& table,
    const ConstraintSpace& space,
    const BoxStrut& table_border_padding) {
  const bool is_fixed_layout = table.Style().IsFixedTableLayout();
  // Tables need autosizer.
  std::optional<TextAutosizer::TableLayoutScope> text_autosizer;
  if (!is_fixed_layout)
    text_autosizer.emplace(To<LayoutTable>(table.GetLayoutBox()));

  const LogicalSize border_spacing = table.Style().TableBorderSpacing();
  TableGroupedChildren grouped_children(table);
  const TableBorders* table_borders = table.GetTableBorders();

  // Compute min/max inline constraints.
  const scoped_refptr<const TableTypes::Columns> column_constraints =
      table.GetColumnConstraints(grouped_children, table_border_padding);

  const TableTypes::Caption caption_constraint =
      ComputeCaptionConstraint(space, table.Style(), grouped_children);

  const LayoutUnit undistributable_space = ComputeUndistributableTableSpace(
      *column_constraints, table_border_padding.InlineSum(),
      border_spacing.inline_size);

  const LayoutUnit assignable_table_inline_size =
      ComputeAssignableTableInlineSize(
          table, space, *column_constraints, caption_constraint,
          undistributable_space, table_border_padding, is_fixed_layout);
  if (column_constraints->data.empty()) {
    return ComputeEmptyTableInlineSize(
        space, table.Style(), assignable_table_inline_size,
        undistributable_space, caption_constraint, table_border_padding,
        table_borders->IsCollapsed());
  }

  const Vector<LayoutUnit> column_sizes =
      SynchronizeAssignableTableInlineSizeAndColumns(
          assignable_table_inline_size, is_fixed_layout, *column_constraints);

  // Final inline size must depend on column locations, because columns can be
  // hidden.
  Vector<TableColumnLocation> column_locations;
  bool has_collapsed_columns;
  ComputeLocationsFromColumns(
      *column_constraints, column_sizes, border_spacing.inline_size,
      /* collapse_columns */ true, &column_locations, &has_collapsed_columns);
  return std::max(ComputeTableSizeFromColumns(
                      column_locations, table_border_padding, border_spacing),
                  caption_constraint.min_size);
}

LayoutUnit TableLayoutAlgorithm::ComputeCaptionBlockSize() {
  TableGroupedChildren grouped_children(Node());
  LayoutUnit captions_block_size;
  ComputeCaptionFragments(container_builder_, Node().Style(), grouped_children,
                          /* captions */ nullptr, captions_block_size);
  return captions_block_size;
}

const LayoutResult* TableLayoutAlgorithm::Layout() {
  if (is_known_to_be_last_table_box_) {
    // This is the last table box fragment. Shouldn't reserve space for cloned
    // block-end box decorations.
    container_builder_.SetShouldCloneBoxEndDecorations(false);
    // And since this is the last table box fragment, be sure *not* to break
    // before any trailing decorations (even if that would cause it to overflow
    // the fragmentainer).
    container_builder_.SetShouldPreventBreakBeforeBlockEndDecorations(true);
  }

  const bool is_fixed_layout = Style().IsFixedTableLayout();
  const LogicalSize border_spacing = Style().TableBorderSpacing();
  TableGroupedChildren grouped_children(Node());
  const TableBorders* table_borders = Node().GetTableBorders();
  DCHECK(table_borders);
  const BoxStrut border_padding = container_builder_.BorderPadding();

  // Algorithm:
  // - Compute inline constraints.
  // - Redistribute assignble table inline size to inline constraints.
  // - Compute column locations.
  // - Compute row block sizes.
  // - Generate fragment.
  const scoped_refptr<const TableTypes::Columns> column_constraints =
      Node().GetColumnConstraints(grouped_children, border_padding);
  const TableTypes::Caption caption_constraint =
      ComputeCaptionConstraint(GetConstraintSpace(), Style(), grouped_children);
  // Compute assignable table inline size.
  // Standard: https://www.w3.org/TR/css-tables-3/#width-distribution
  const LayoutUnit undistributable_space = ComputeUndistributableTableSpace(
      *column_constraints, border_padding.InlineSum(),
      border_spacing.inline_size);

  const LayoutUnit assignable_table_inline_size =
      ComputeAssignableTableInlineSize(
          Node(), GetConstraintSpace(), *column_constraints, caption_constraint,
          undistributable_space, border_padding, is_fixed_layout);

  // Distribute assignable table width.
  const Vector<LayoutUnit> column_sizes =
      SynchronizeAssignableTableInlineSizeAndColumns(
          assignable_table_inline_size, is_fixed_layout, *column_constraints);

  Vector<TableColumnLocation> column_locations;
  bool has_collapsed_columns;
  ComputeLocationsFromColumns(
      *column_constraints, column_sizes, border_spacing.inline_size,
      /* shrink_collapsed */ false, &column_locations, &has_collapsed_columns);

  LayoutUnit table_inline_size_before_collapse;
  const bool is_grid_empty = column_locations.empty();
  if (is_grid_empty) {
    table_inline_size_before_collapse = ComputeEmptyTableInlineSize(
        GetConstraintSpace(), Style(), assignable_table_inline_size,
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
  ComputeCaptionFragments(container_builder_, Style(), grouped_children,
                          &captions, captions_block_size);

  TableTypes::Rows rows;
  TableTypes::CellBlockConstraints cell_block_constraints;
  TableTypes::Sections sections;
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
  } else if (GetConstraintSpace().IsFixedInlineSize()) {
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

  const LayoutResult* result = GenerateFragment(
      container_builder_.InlineSize(), minimal_table_grid_block_size,
      grouped_children, column_locations, rows, cell_block_constraints,
      sections, captions, *table_borders,
      is_grid_empty ? LogicalSize() : border_spacing);

  if (result->Status() == LayoutResult::kNeedsRelayoutAsLastTableBox) {
    return RelayoutAsLastTableBox();
  }
  if (result->Status() == LayoutResult::kNeedsEarlierBreak) {
    // We shouldn't insert early-breaks when we're relaying out as the last
    // table-box fragment. That should take place *first*.
    DCHECK(!is_known_to_be_last_table_box_);

    return RelayoutAndBreakEarlier<TableLayoutAlgorithm>(
        *result->GetEarlyBreak());
  }

  return result;
}

MinMaxSizesResult TableLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  const bool is_fixed_layout = Style().IsFixedTableLayout();
  // Tables need autosizer.
  std::optional<TextAutosizer::TableLayoutScope> text_autosizer;
  if (!is_fixed_layout)
    text_autosizer.emplace(To<LayoutTable>(Node().GetLayoutBox()));

  const LogicalSize border_spacing = Style().TableBorderSpacing();
  const BoxStrut border_padding = container_builder_.BorderPadding();
  TableGroupedChildren grouped_children(Node());

  const scoped_refptr<const TableTypes::Columns> column_constraints =
      Node().GetColumnConstraints(grouped_children, border_padding);
  const TableTypes::Caption caption_constraint =
      ComputeCaptionConstraint(GetConstraintSpace(), Style(), grouped_children);

  const LayoutUnit undistributable_space = ComputeUndistributableTableSpace(
      *column_constraints, border_padding.InlineSum(),
      border_spacing.inline_size);

  const MinMaxSizes grid_min_max = ComputeGridInlineMinMax(
      Node(), *column_constraints, undistributable_space, is_fixed_layout,
      /* is_layout_pass */ false);

  MinMaxSizes min_max{
      std::max(grid_min_max.min_size, caption_constraint.min_size),
      std::max(grid_min_max.max_size, caption_constraint.min_size)};

  if (is_fixed_layout && Style().LogicalWidth().HasPercent()) {
    min_max.max_size = TableTypes::kTableMaxInlineSize;
  }
  DCHECK_LE(min_max.min_size, min_max.max_size);
  return MinMaxSizesResult{min_max,
                           /* depends_on_block_constraints */ false};
}

const LayoutResult* TableLayoutAlgorithm::RelayoutAsLastTableBox() {
  DCHECK(!is_known_to_be_last_table_box_);
  LayoutAlgorithmParams params(
      Node(), container_builder_.InitialFragmentGeometry(),
      GetConstraintSpace(), GetBreakToken(), /* early_break */ nullptr);
  TableLayoutAlgorithm algorithm(params);
  algorithm.is_known_to_be_last_table_box_ = true;

  // In case we were already re-laying out with a known early-break, we need to
  // re-propagate that piece of information as well, so that we don't end up
  // getting stuck in an infinite recursion with the early-break and
  // known-to-be-last-table-box mechanisms invoking each other.
  algorithm.early_break_ = early_break_;

  return algorithm.Layout();
}

void TableLayoutAlgorithm::ComputeRows(
    const LayoutUnit table_grid_inline_size,
    const TableGroupedChildren& grouped_children,
    const Vector<TableColumnLocation>& column_locations,
    const TableBorders& table_borders,
    const LogicalSize& border_spacing,
    const BoxStrut& table_border_padding,
    const LayoutUnit captions_block_size,
    TableTypes::Rows* rows,
    TableTypes::CellBlockConstraints* cell_block_constraints,
    TableTypes::Sections* sections,
    LayoutUnit* minimal_table_grid_block_size) {
  DCHECK_EQ(rows->size(), 0u);
  DCHECK_EQ(cell_block_constraints->size(), 0u);

  const TableBreakTokenData* table_break_data = nullptr;
  if (GetBreakToken()) {
    table_break_data =
        DynamicTo<TableBreakTokenData>(GetBreakToken()->TokenData());
  }
  if (table_break_data) {
    DCHECK(IsBreakInside(GetBreakToken()));
    *rows = table_break_data->rows;
    *cell_block_constraints = table_break_data->cell_block_constraints;
    *sections = table_break_data->sections;
    total_table_min_block_size_ = table_break_data->total_table_min_block_size;
  } else {
    DCHECK_EQ(total_table_min_block_size_, LayoutUnit());
    const bool is_table_block_size_specified =
        !Style().LogicalHeight().IsAuto();
    wtf_size_t section_index = 0;
    for (auto it = grouped_children.begin(); it != grouped_children.end();
         ++it) {
      ComputeSectionMinimumRowBlockSizes(
          *it, table_grid_inline_size, is_table_block_size_specified,
          column_locations, table_borders, border_spacing.block_size,
          section_index++, it.TreatAsTBody(), sections, rows,
          cell_block_constraints);
      total_table_min_block_size_ += sections->back().block_size;
    }
  }

  const ConstraintSpace& space = GetConstraintSpace();

  LayoutUnit css_table_block_size;
  if (space.IsInitialBlockSizeIndefinite() && !space.IsFixedBlockSize()) {
    // We get here when a flexbox wants to use the table's intrinsic height as
    // an input to the flex algorithm.
    css_table_block_size = kIndefiniteSize;
  } else {
    // If we can correctly resolve our min-block-size we want to distribute
    // sections/rows into this space. Pass a definite intrinsic block-size into
    // |ComputeBlockSizeForFragment| to force it to resolve.
    //
    // NOTE: We use `ResolveMainBlockLength` for resolving `min_length` so that
    // it will resolve to `kIndefiniteSize` if unresolvable.
    const Length& min_length = Style().LogicalMinHeight();
    const LayoutUnit intrinsic_block_size =
        min_length.HasAuto() ||
                ResolveMainBlockLength(space, Style(), table_border_padding,
                                       min_length, /* auto_length */ nullptr,
                                       kIndefiniteSize) == kIndefiniteSize
            ? kIndefiniteSize
            : table_border_padding.BlockSum();

    LayoutUnit override_available_block_size = kIndefiniteSize;
    if (space.AvailableSize().block_size != kIndefiniteSize) {
      override_available_block_size =
          (space.AvailableSize().block_size - captions_block_size)
              .ClampNegativeToZero();
    }

    css_table_block_size = ComputeBlockSizeForFragment(
        space, Node(), table_border_padding, intrinsic_block_size,
        table_grid_inline_size, override_available_block_size);
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
    if (distributable_block_size > total_table_min_block_size_) {
      DistributeTableBlockSizeToSections(
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
void TableLayoutAlgorithm::ComputeTableSpecificFragmentData(
    const TableGroupedChildren& grouped_children,
    const Vector<TableColumnLocation>& column_locations,
    const TableTypes::Rows& rows,
    const TableBorders& table_borders,
    const LogicalRect& table_grid_rect,
    const LayoutUnit table_grid_block_size) {
  container_builder_.SetTableGridRect(table_grid_rect);
  container_builder_.SetTableColumnCount(column_locations.size());
  container_builder_.SetHasCollapsedBorders(table_borders.IsCollapsed());
  // Column geometries.
  if (grouped_children.columns.size() > 0) {
    ColumnGeometriesBuilder geometry_builder(column_locations,
                                             table_grid_block_size);
    VisitLayoutTableColumn(grouped_children.columns, column_locations.size(),
                           &geometry_builder);
    geometry_builder.Sort();
    container_builder_.SetTableColumnGeometries(
        geometry_builder.column_geometries);
  }
  // Collapsed borders.
  if (!table_borders.IsEmpty()) {
    std::unique_ptr<TableFragmentData::CollapsedBordersGeometry>
        fragment_borders_geometry =
            std::make_unique<TableFragmentData::CollapsedBordersGeometry>();
    for (const auto& column : column_locations)
      fragment_borders_geometry->columns.push_back(column.offset);
    DCHECK_NE(column_locations.size(), 0u);
    fragment_borders_geometry->columns.push_back(
        column_locations.back().offset + column_locations.back().size);

    // Ensure the dimensions of table_borders and fragment_borders_geometry are
    // consistent.
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
const LayoutResult* TableLayoutAlgorithm::GenerateFragment(
    const LayoutUnit table_inline_size,
    LayoutUnit minimal_table_grid_block_size,
    const TableGroupedChildren& grouped_children,
    const Vector<TableColumnLocation>& column_locations,
    const TableTypes::Rows& rows,
    const TableTypes::CellBlockConstraints& cell_block_constraints,
    const TableTypes::Sections& sections,
    const HeapVector<CaptionResult>& captions,
    const TableBorders& table_borders,
    const LogicalSize& border_spacing) {
  const TableBreakTokenData* incoming_table_break_data = nullptr;
  LogicalBoxSides border_padding_sides_to_include;
  const auto& constraint_space = GetConstraintSpace();
  const LayoutUnit fragmentainer_space_at_start =
      FragmentainerSpaceLeftForChildren();
  LayoutUnit previously_consumed_block_size;
  LayoutUnit previously_consumed_table_box_block_size;

  // border-spacing that is to be added before the first table section (if any)
  // in this fragment. We will omit this when resuming table box layout in a
  // subsequent fragment.
  LayoutUnit border_spacing_before_first_section = border_spacing.block_size;

  LayoutUnit monolithic_overflow;
  bool is_past_table_box = false;
  if (GetBreakToken()) {
    previously_consumed_block_size = GetBreakToken()->ConsumedBlockSize();
    monolithic_overflow = GetBreakToken()->MonolithicOverflow();
    incoming_table_break_data =
        DynamicTo<TableBreakTokenData>(GetBreakToken()->TokenData());
    if (incoming_table_break_data) {
      previously_consumed_table_box_block_size =
          incoming_table_break_data->consumed_table_box_block_size;
      minimal_table_grid_block_size -=
          incoming_table_break_data->consumed_table_box_block_size;
      is_past_table_box = incoming_table_break_data->is_past_table_box;
      if (incoming_table_break_data->has_entered_table_box) {
        // The block-start border won't be in this fragment when resuming in the
        // slicing box decoration break model (and also not in the cloning
        // model, if we're already past the table box (and just dealing with
        // bottom captions, essentially)).
        if (Style().BoxDecorationBreak() == EBoxDecorationBreak::kSlice ||
            is_past_table_box) {
          border_padding_sides_to_include.block_start = false;
        }
        border_spacing_before_first_section = LayoutUnit();
      }
      if (is_past_table_box)
        border_padding_sides_to_include.block_end = false;
    }
  }

  const auto table_writing_direction = Style().GetWritingDirection();
  scoped_refptr<const TableConstraintSpaceData> constraint_space_data =
      CreateConstraintSpaceData(Style(), column_locations, sections, rows,
                                cell_block_constraints, border_spacing);

  const BoxStrut border_padding = container_builder_.BorderScrollbarPadding();
  const bool has_collapsed_borders = table_borders.IsCollapsed();

  // The current layout position.
  LayoutUnit child_block_offset;

  // border-spacing to add after the last table section in this fragment. We may
  // want to omit it in some cases, in which case it will be set to 0.
  LayoutUnit border_spacing_after_last_section;

  bool has_container_separation = false;

  auto AddCaptionResult = [&](const CaptionResult& caption,
                              LayoutUnit* block_offset) -> void {
    *block_offset += caption.margins.block_start;
    container_builder_.AddResult(
        *caption.layout_result,
        LogicalOffset(caption.margins.inline_start, *block_offset),
        caption.margins);

    *block_offset +=
        LogicalFragment(table_writing_direction,
                        caption.layout_result->GetPhysicalFragment())
            .BlockSize() +
        caption.margins.block_end;
  };

  // We have already laid out the captions, in order to calculate the table grid
  // size. We can re-use these results now, unless we're in block fragmentation.
  // In that case we need to lay them out again now, so that they fragment and
  // resume properly.
  const bool relayout_captions =
      InvolvedInBlockFragmentation(container_builder_);

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

  enum ESectionRepeatMode { kNotRepeated, kMayRepeatAgain, kRepeatedLast };

  auto CreateSectionConstraintSpace = [&table_writing_direction,
                                       &section_available_inline_size,
                                       &constraint_space_data, &sections, this](
                                          const BlockNode& section,
                                          LayoutUnit fragmentainer_block_offset,
                                          wtf_size_t section_index,
                                          LayoutUnit reserved_space,
                                          ESectionRepeatMode repeat_mode) {
    ConstraintSpaceBuilder section_space_builder(
        GetConstraintSpace(), table_writing_direction, /* is_new_fc */ true);

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

    if (repeat_mode != kNotRepeated) {
      section_space_builder.SetShouldRepeat(repeat_mode == kMayRepeatAgain);
      section_space_builder.SetIsInsideRepeatableContent(true);
      section_space_builder.SetShouldPropagateChildBreakValues(false);
    } else if (GetConstraintSpace().HasBlockFragmentation()) {
      // Note that, with fragmentainer_block_offset, we pretend that any
      // repeated table header isn't there (since it doesn't really participate
      // in block fragmentation anyway), so that the block-offset right after
      // such a header will be at 0, as far as block fragmentation is concerned.
      // This way the fragmentation engine will refuse to insert a break before
      // having made some content progress (even if the first piece of content
      // doesn't fit).
      SetupSpaceBuilderForFragmentation(container_builder_, section,
                                        fragmentainer_block_offset,
                                        &section_space_builder);

      // Reserve space for any repeated header / footer.
      if (GetConstraintSpace().HasKnownFragmentainerBlockSize()) {
        section_space_builder.ReserveSpaceInFragmentainer(reserved_space);
      }
    }

    return section_space_builder.ToConstraintSpace();
  };

  auto BlockStartBorderPadding = [&border_padding,
                                  &border_padding_sides_to_include]() {
    if (border_padding_sides_to_include.block_start)
      return border_padding.block_start;
    return LayoutUnit();
  };

  const LayoutUnit section_inline_offset =
      border_padding.inline_start + border_spacing.inline_size;

  std::optional<TableBoxExtent> table_box_extent;
  std::optional<LayoutUnit> first_baseline;
  std::optional<LayoutUnit> last_baseline;

  bool has_repeated_header = false;
  bool has_pending_repeated_footer = false;
  LayoutUnit repeated_footer_block_size;

  // Before fragmented layout we need to go through the table's children, to
  // look for repeatable headers and footers. This is especially important for
  // footers, since we need to reserve space for it after any preceding
  // non-repeated sections (typically tbody). We'll only repeat headers /
  // footers if we're not already inside repeatable content, though. See
  // crbug.com/1352931 for more details. Furthermore, we cannot repeat content
  // if side-effects are disabled, as that machinery depends on updating and
  // reading the physical fragments vector of the LayoutBox.
  if (!GetConstraintSpace().IsInsideRepeatableContent() &&
      !DisableLayoutSideEffectsScope::IsDisabled() &&
      (grouped_children.header || grouped_children.footer)) {
    LayoutUnit max_section_block_size =
        GetConstraintSpace().FragmentainerBlockSize() / 4;
    TableChildIterator child_iterator(grouped_children, GetBreakToken());
    for (auto entry = child_iterator.NextChild();
         BlockNode child = entry.GetNode();
         entry = child_iterator.NextChild()) {
      if (child != grouped_children.header && child != grouped_children.footer)
        continue;

      const BlockBreakToken* child_break_token = entry.GetBreakToken();
      // If we've already broken inside the section, it's not going to repeat,
      // but rather perform regular fragmentation.
      if (IsBreakInside(child_break_token))
        continue;

      LayoutUnit block_size = sections[entry.GetSectionIndex()].block_size;

      // Unless we have already decided to repeat in a previous fragment, check
      // if the block-size of the section is acceptable.
      if (!child_break_token || !child_break_token->IsRepeated()) {
        DCHECK(!child_break_token || child_break_token->IsBreakBefore());

        // If this isn't the first fragment for the table box, and the section
        // didn't repeat in the previous fragment, it doesn't make sense to
        // start repeating now. If this is a header, we may already have
        // finished (non-repeated) layout. If this is a footer, we have already
        // laid out at least one fragment without it.
        if (incoming_table_break_data &&
            incoming_table_break_data->has_entered_table_box)
          continue;

        // Headers and footers may be repeated if their block-size is one
        // quarter or less than that of the fragmentainer, AND 'break-inside'
        // has an applicable avoid* value. Being repeated means that the section
        // is monolithic, and nothing inside can break.
        //
        // See https://www.w3.org/TR/css-tables-3/#repeated-headers
        //
        // We will never make the decision to start repeating if we're in an
        // initial column balancing pass (we have no idea about the block-size
        // of the fragmentainer, so that would be impossible), but we will
        // continue repeating if we previously decided to do so in a previous
        // layout pass, for a previous fragment.
        if (!GetConstraintSpace().HasKnownFragmentainerBlockSize() ||
            block_size > max_section_block_size) {
          continue;
        }

        if (!IsAvoidBreakValue(GetConstraintSpace(),
                               child.Style().BreakInside())) {
          continue;
        }
      }

      if (child == grouped_children.header) {
        has_repeated_header = true;
      } else {
        DCHECK_EQ(child, grouped_children.footer);
        has_pending_repeated_footer = true;

        // We need to reserve space for the repeated footer at the end of the
        // fragmentainer.
        repeated_footer_block_size =
            block_size + border_spacing.block_size +
            (has_collapsed_borders ? border_padding.block_end : LayoutUnit());
      }
    }
  }

  bool has_entered_non_repeated_section = false;
  if (monolithic_overflow) {
    // If the page was overflowed by monolithic content (inside the table) on a
    // previous page, it has to mean that we've already entered a non-repeated
    // section, since those are the only ones that can cause fragmentation
    // overflow.
    has_entered_non_repeated_section = true;
  }

  LayoutUnit grid_block_size_inflation;
  LayoutUnit repeated_header_block_size;
  bool broke_inside = false;
  bool has_ended_table_box_layout = false;
  TableChildIterator child_iterator(grouped_children, GetBreakToken());
  // Generate section fragments; and also caption fragments, if we need to
  // regenerate them (block fragmentation).
  for (auto entry = child_iterator.NextChild();
       BlockNode child = entry.GetNode(); entry = child_iterator.NextChild()) {
    DCHECK(child.IsTableCaption() || child.IsTableSection());

    const EarlyBreak* early_break_in_child = nullptr;
    if (early_break_) [[unlikely]] {
      if (IsEarlyBreakTarget(*early_break_, container_builder_, child)) {
        container_builder_.AddBreakBeforeChild(child, kBreakAppealPerfect,
                                               /* is_forced_break */ false);
        broke_inside = true;

        if (child == grouped_children.footer)
          has_pending_repeated_footer = false;

        break;
      }
      early_break_in_child = EnterEarlyBreakInChild(child, *early_break_);
    }

    const BlockBreakToken* child_break_token = entry.GetBreakToken();
    const LayoutResult* child_result;
    std::optional<LayoutUnit> offset_before_repeated_header;
    LayoutUnit child_inline_offset;

    // Captions allow margins.
    LayoutUnit child_block_start_margin;
    LayoutUnit child_block_end_margin;

    std::optional<TableBoxExtent> new_table_box_extent;
    bool is_repeated_section = false;
    bool has_overlapping_repeated_header = false;

    if (child.IsTableCaption()) {
      if (!relayout_captions)
        continue;
      if (child.Style().CaptionSide() == ECaptionSide::kBottom &&
          !is_past_table_box) {
        DCHECK(!has_ended_table_box_layout);
        // We found the first bottom caption, which means that we're done with
        // all the sections (if any). We need to calculate the grid size now, so
        // that we set the block-offset for the caption correctly.
        if (!table_box_extent) {
          // There was no section to kick off "table box" extent
          // calculation. Do it now.
          table_box_extent = BeginTableBoxLayout(child_block_offset,
                                                 BlockStartBorderPadding());
        }

        child_block_offset = EndTableBoxLayout(
            border_padding.block_end, border_spacing_after_last_section,
            minimal_table_grid_block_size, &(*table_box_extent),
            &grid_block_size_inflation);
        has_ended_table_box_layout = true;

        // We're done with the table box if it fits inside the fragmentainer.
        is_past_table_box =
            !constraint_space.HasKnownFragmentainerBlockSize() ||
            table_box_extent->end <= fragmentainer_space_at_start;
      }

      LogicalSize available_size(container_builder_.InlineSize(),
                                 kIndefiniteSize);
      BoxStrut margins = ComputeCaptionMargins(constraint_space, child,
                                               container_builder_.InlineSize(),
                                               child_break_token);
      child_block_start_margin = margins.block_start;
      child_block_end_margin = margins.block_end;

      ConstraintSpace child_space = CreateCaptionConstraintSpace(
          constraint_space, Style(), child, available_size,
          child_block_offset + child_block_start_margin);
      CaptionResult caption = LayoutCaption(
          constraint_space, Style(), container_builder_.InlineSize(),
          child_space, child, margins, child_break_token, early_break_in_child);
      DCHECK_EQ(caption.layout_result->Status(), LayoutResult::kSuccess);
      child_result = caption.layout_result;
      child_inline_offset = caption.margins.inline_start;

      // Captions don't need to worry about repeated sections.
      repeated_header_block_size = LayoutUnit();
    } else {
      DCHECK(child.IsTableSection());
      LayoutUnit collapsible_border_spacing;
      if (table_box_extent) {
        // This is not the first section. Just add border-spacing.
        collapsible_border_spacing = border_spacing.block_size;
      } else {
        // Entering the first section in this fragment. This is where the "table
        // box" starts.
        new_table_box_extent =
            BeginTableBoxLayout(child_block_offset, BlockStartBorderPadding());
        child_block_offset += BlockStartBorderPadding();

        // We need to lay the section out before we can tell whether it should
        // be preceded by border-spacing (if there is nothing inside, it should
        // be omitted).
        collapsible_border_spacing = border_spacing_before_first_section;
      }

      LayoutUnit offset_for_childless_section = child_block_offset;

      bool may_repeat_again = false;
      if (child == grouped_children.header) {
        if (has_repeated_header) {
          is_repeated_section = true;
          // Unless we've already been at the end, we cannot tell whether this
          // is the last time the header will repeat. We will tentatively have
          // to make it repeatable. If this turns out to be wrong, because we
          // reach the end in this fragment, we need to abort and relayout.
          may_repeat_again = !is_known_to_be_last_table_box_;

          // We need to measure the block-size of the repeated header, because
          // this is something we have to subtract from available fragmentainer
          // block-size, AND fragmentainer block-offset, when laying out
          // non-repeated content (i.e. regular sections).
          offset_before_repeated_header.emplace(child_block_offset);

          if (monolithic_overflow) {
            // There's monolithic content from previous pages in the way, but we
            // still want to place the table header at the block-start. In
            // addition to this (probably) making sense, our implementation
            // requires it. Once we have decided to repeat a table section, we
            // need to be consistent about it. Take the header "out of flow",
            // and just restore the block-offset back to
            // offset_before_repeated_header afterwards.
            child_block_offset = -monolithic_overflow;
            has_overlapping_repeated_header = true;
          }

          // A header will share its collapsed border with the block-start of
          // the table. However when repeated it will draw the whole border
          // itself. We need to reserve additional space at the block-start for
          // this additional border space.
          if (has_collapsed_borders &&
              !border_padding_sides_to_include.block_start)
            child_block_offset += border_padding.block_start;
        }
      } else if (child == grouped_children.footer) {
        if (has_pending_repeated_footer) {
          is_repeated_section = true;
          // For footers it's easier, though. Since we got all the way to the
          // footer during layout, this means that this will be the last time
          // the footer is repeated. We can finish it right away, unless we have
          // a repeated header as well (which means that we're going to
          // relayout).
          has_pending_repeated_footer = false;
          may_repeat_again =
              !is_known_to_be_last_table_box_ && has_repeated_header;
        }
      }

      child_block_offset += collapsible_border_spacing;

      ESectionRepeatMode repeat_mode = kNotRepeated;
      if (is_repeated_section)
        repeat_mode = may_repeat_again ? kMayRepeatAgain : kRepeatedLast;

      LayoutUnit reserved_space;
      if (repeat_mode == kNotRepeated) {
        reserved_space =
            repeated_header_block_size + repeated_footer_block_size;
      }

      ConstraintSpace child_space = CreateSectionConstraintSpace(
          child, child_block_offset - repeated_header_block_size,
          entry.GetSectionIndex(), reserved_space, repeat_mode);
      if (is_repeated_section) {
        child_result =
            child.LayoutRepeatableRoot(child_space, child_break_token);
      } else {
        child_result =
            child.Layout(child_space, child_break_token, early_break_in_child);
      }
      child_inline_offset = section_inline_offset;

      border_spacing_after_last_section = border_spacing.block_size;
      if (To<PhysicalBoxFragment>(child_result->GetPhysicalFragment())
              .HasDescendantsForTablePart()) {
        // We want to add border-spacing after this section, but not if the
        // current fragment is past the block-end of the section. This might
        // happen if there are overflowing descendants, and this section should
        // just create an zero-sized fragment.
        if (child_break_token && child_break_token->IsAtBlockEnd())
          border_spacing_after_last_section = LayoutUnit();
      } else {
        // There were no children inside. Omit the border-spacing previously
        // added. Note that we should ideally re-lay out now if we're
        // block-fragmented and ran out of space (the section may have had a
        // non-zero block-size, for instance), since that would mean that we've
        // used less space than actually turned out to be available. However,
        // nobody will probably notice, and besides, our "empty section
        // handling" isn't identical to other engines anyway.
        child_block_offset = offset_for_childless_section;
      }
    }
    if (constraint_space.HasBlockFragmentation() &&
        (!child_break_token || !is_repeated_section)) {
      LayoutUnit fragmentainer_block_offset =
          FragmentainerOffsetForChildren() + child_block_start_margin +
          child_block_offset - repeated_header_block_size;
      BreakStatus break_status = BreakBeforeChildIfNeeded(
          child, *child_result, fragmentainer_block_offset,
          has_container_separation);
      if (break_status == BreakStatus::kNeedsEarlierBreak) {
        return container_builder_.Abort(LayoutResult::kNeedsEarlierBreak);
      }
      if (break_status == BreakStatus::kBrokeBefore) {
        broke_inside = true;
        break;
      }
      DCHECK_EQ(break_status, BreakStatus::kContinue);
    }

    const auto& physical_fragment =
        To<PhysicalBoxFragment>(child_result->GetPhysicalFragment());
    LogicalBoxFragment fragment(table_writing_direction, physical_fragment);
    if (child.IsTableSection()) {
      if (!is_repeated_section) {
        has_entered_non_repeated_section = true;
      }
      if (!first_baseline) {
        if (const auto& section_first_baseline = fragment.FirstBaseline())
          first_baseline = child_block_offset + *section_first_baseline;
      }
      if (const auto& section_last_baseline = fragment.LastBaseline())
        last_baseline = child_block_offset + *section_last_baseline;
    }

    child_block_offset += child_block_start_margin;
    container_builder_.AddResult(
        *child_result, LogicalOffset(child_inline_offset, child_block_offset));
    child_block_offset += fragment.BlockSize() + child_block_end_margin;

    if (child.IsTableSection()) {
      if (offset_before_repeated_header) {
        repeated_header_block_size = child_block_offset -
                                     *offset_before_repeated_header +
                                     border_spacing.block_size;

        if (has_overlapping_repeated_header) {
          // The header was taken "out of flow" and placed on top of monolithic
          // content. Now make sure that the offset is past the monolithic
          // overflow again (AND past the header).
          child_block_offset =
              std::max(child_block_offset, *offset_before_repeated_header);
        }
      }

      if (new_table_box_extent) {
        // The first section was added successfully. We're officially inside the
        // table box!
        DCHECK(!table_box_extent);
        table_box_extent = new_table_box_extent;
      }
      // Update the "table box" extent, now that we're past one section.
      table_box_extent->end = child_block_offset;
    } else if (child.Style().CaptionSide() == ECaptionSide::kBottom) {
      // We've successfully added bottom caption content, so we're past the
      // table box.
      is_past_table_box = true;
    }

    if (constraint_space.HasBlockFragmentation()) {
      if (!has_container_separation)
        has_container_separation = !is_repeated_section;
      if (container_builder_.HasInflowChildBreakInside()) {
        broke_inside = true;
        break;
      }
    }
  }

  if (!table_box_extent && !is_past_table_box && !broke_inside) {
    // We're not past the table box, we didn't break inside, but there was no
    // section to kick off "table box" extent calculation. Do it now.
    table_box_extent =
        BeginTableBoxLayout(child_block_offset, BlockStartBorderPadding());
  }

  bool table_box_will_continue =
      table_box_extent && !is_past_table_box && broke_inside;

  if (has_pending_repeated_footer && table_box_extent) {
    DCHECK(table_box_will_continue);
    // We broke before we got to the footer. Add it now. Before doing that,
    // though, also insert break tokens for the sections that we didn't get to
    // (if any), so that things will be resumed correctly when laying out the
    // next table fragment (inserting a break token for the repeated footer
    // alone would make the table child iterator skip any preceding sections).
    auto entry = child_iterator.NextChild();
    for (; BlockNode child = entry.GetNode();
         entry = child_iterator.NextChild()) {
      if (child == grouped_children.footer)
        break;

      auto* token = BlockBreakToken::CreateBreakBefore(
          child, /* is_forced_break */ false);
      container_builder_.AddBreakToken(token);
    }
    DCHECK_EQ(entry.GetNode(), grouped_children.footer);

    // The only case where we should allow a break before a repeatable section
    // is when we haven't processed a non-repeated section yet (a regular TBODY,
    // for instance). In all other cases we should make sure that the footer
    // fits, by forcefully making room for it, if necessary. This may be
    // necessary when there's monolithic content that overflows the current
    // page. We'll then place the repeated footer on top of any overflowing
    // monolithic content, at the bottom of the page. Our implementation
    // requires that once we've started repeating a section, it needs to be
    // present in *every* subsequent table fragment that contains parts of the
    // table box (i.e. non-captions).
    LayoutUnit adjusted_child_block_offset = child_block_offset;
    if (has_entered_non_repeated_section) {
      // We may be past the fragmentation line due to monolithic content from a
      // preceding page still taking up space in a resumed fragment -
      // potentially all space, and more. The fragmentainer offset will be right
      // after the end of the monolithic content, which might not even be on the
      // current page, but on a later one. We now want to calculate the offset
      // for the repeated table footer, relatively to that fragmentainer offset,
      // so that the footer ends up exactly at the bottom of this page (this may
      // be a negative offset, since the fragmentainer offset may be on a
      // subsequent page, after the monolithic content).
      LayoutUnit footer_offset_at_end_of_page =
          FragmentainerCapacityForChildren() -
          GetConstraintSpace().FragmentainerOffset() -
          repeated_footer_block_size;
      adjusted_child_block_offset =
          std::min(adjusted_child_block_offset, footer_offset_at_end_of_page);
    }

    LogicalOffset offset(section_inline_offset, adjusted_child_block_offset);
    ConstraintSpace child_space = CreateSectionConstraintSpace(
        grouped_children.footer, offset.block_offset, entry.GetSectionIndex(),
        /* reserved_space */ LayoutUnit(), kMayRepeatAgain);
    const LayoutResult* result = grouped_children.footer.LayoutRepeatableRoot(
        child_space, entry.GetBreakToken());

    BreakStatus break_status = BreakStatus::kContinue;
    if (!entry.GetBreakToken() || entry.GetBreakToken()->IsBreakBefore()) {
      // Although there are rules that make sure that a footer normally fits (it
      // should only be a quarter of the fragmentainer's block-size), if the
      // table box starts near the end of the fragmentainer, we may still run
      // out of space before a repeatable footer. So insert a break if
      // necessary.
      LayoutUnit fragmentainer_block_offset =
          FragmentainerOffsetForChildren() + offset.block_offset;
      break_status = BreakBeforeChildIfNeeded(grouped_children.footer, *result,
                                              fragmentainer_block_offset,
                                              has_container_separation);
    }
    if (break_status == BreakStatus::kContinue) {
      container_builder_.AddResult(*result, offset);
    } else {
      DCHECK_EQ(break_status, BreakStatus::kBrokeBefore);
    }
  }

  if (!child_iterator.NextChild())
    container_builder_.SetHasSeenAllChildren();

  if (table_box_extent && !is_past_table_box) {
    // If we had (any) break inside, we don't need end border-spacing, and
    // should be at-least the fragmentainer size (if definite).
    if (broke_inside) {
      if (constraint_space.HasKnownFragmentainerBlockSize()) {
        table_box_extent->end =
            std::max(table_box_extent->end, fragmentainer_space_at_start);
      }
      border_spacing_after_last_section = LayoutUnit();
    } else if (constraint_space.HasKnownFragmentainerBlockSize()) {
      // Truncate trailing border-spacing to fit within the fragmentainer.
      LayoutUnit new_border_spacing_after_last_section =
          std::min(border_spacing_after_last_section,
                   fragmentainer_space_at_start - child_block_offset -
                       border_padding.block_end);
      new_border_spacing_after_last_section =
          new_border_spacing_after_last_section.ClampNegativeToZero();
      if (border_spacing_after_last_section !=
          new_border_spacing_after_last_section) {
        container_builder_.SetIsTruncatedByFragmentationLine();
        border_spacing_after_last_section =
            new_border_spacing_after_last_section;
      }
    }

    if (!has_ended_table_box_layout) {
      child_block_offset = EndTableBoxLayout(
          border_padding.block_end, border_spacing_after_last_section,
          minimal_table_grid_block_size, &(*table_box_extent),
          &grid_block_size_inflation);

      has_ended_table_box_layout = true;

      if (!broke_inside) {
        // If the table box fits inside the fragmentainer, we're past it.
        is_past_table_box =
            !constraint_space.HasKnownFragmentainerBlockSize() ||
            table_box_extent->end <= fragmentainer_space_at_start;
      }
    }
  }

  if (!table_box_extent)
    border_padding_sides_to_include.block_start = false;
  if (!is_past_table_box &&
      (!container_builder_.ShouldCloneBoxEndDecorations() ||
       !table_box_extent)) {
    border_padding_sides_to_include.block_end = false;
  }

  // Add all the bottom captions.
  if (!relayout_captions) {
    for (const auto& caption : captions) {
      if (caption.node.Style().CaptionSide() == ECaptionSide::kBottom)
        AddCaptionResult(caption, &child_block_offset);
    }
  }

  LayoutUnit block_size = child_block_offset.ClampNegativeToZero();
  DCHECK_GE(block_size, grid_block_size_inflation);

  LayoutUnit intrinsic_block_size = block_size - grid_block_size_inflation;
  if (!has_ended_table_box_layout && !is_past_table_box) {
    // Include block-end border/padding/border-spacing when setting the
    // block-size. Even if we're positive at this point that we're going to
    // break inside, the fragmentation machinery expects this to be part of the
    // block-size. The reason is that the algorithms themselves don't really
    // know enough to tell for sure that we're *not* going to break inside. In
    // order for FinishFragmentation() to make that decision correctly, add
    // this, if it hasn't already been added.
    LayoutUnit table_block_end_fluff =
        border_padding.block_end + border_spacing_after_last_section;
    intrinsic_block_size += table_block_end_fluff;
    block_size += table_block_end_fluff;
  }
  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);

  block_size += previously_consumed_block_size;
  if (constraint_space.IsFixedBlockSize()) {
    block_size = constraint_space.AvailableSize().block_size;
    if (constraint_space.MinBlockSizeShouldEncompassIntrinsicSize()) {
      block_size = std::max(
          block_size, previously_consumed_block_size + intrinsic_block_size);
    }
  }
  container_builder_.SetFragmentsTotalBlockSize(block_size);

  if (Node().GetDOMNode() &&
      Node().GetDOMNode()->HasTagName(mathml_names::kMtableTag)) {
    container_builder_.SetBaselines(
        MathTableBaseline(Style(), child_block_offset));
  } else {
    if (first_baseline)
      container_builder_.SetFirstBaseline(*first_baseline);
    if (last_baseline)
      container_builder_.SetLastBaseline(*last_baseline);
  }

  container_builder_.SetIsTablePart();

  if (InvolvedInBlockFragmentation(container_builder_)) [[unlikely]] {
    BreakStatus status = FinishFragmentation(&container_builder_);
    if (status == BreakStatus::kNeedsEarlierBreak) {
      return container_builder_.Abort(LayoutResult::kNeedsEarlierBreak);
    }

    DCHECK_EQ(status, BreakStatus::kContinue);

    // Which side to include is normally handled by FinishFragmentation(), but
    // that function doesn't know about table weirdness (table captions flow
    // before and after table borders and padding).
    container_builder_.SetSidesToInclude(border_padding_sides_to_include);

    // Finishing fragmentation may have shrunk the fragment size. Reflect such
    // changes in the table box extent, so that we set the correct grid
    // block-size further below.
    if (table_box_extent) {
      table_box_extent->end = std::min(table_box_extent->end,
                                       container_builder_.FragmentBlockSize());
    }
  }

  LayoutUnit column_block_size = kIndefiniteSize;
  LogicalRect table_grid_rect;
  LayoutUnit grid_block_size;
  if (table_box_extent) {
    grid_block_size = table_box_extent->end - table_box_extent->start;
    if (!table_box_will_continue) {
      // We're at the last fragment for the "table box", and we can calculate
      // the stitched-together table column / column-group sizes. Columns and
      // column groups are special, in that they aren't actually laid out (and
      // get no fragments), so we need to do the LayoutBox block-size write-back
      // manually (all other nodes get this for free during layout).
      column_block_size =
          previously_consumed_table_box_block_size + grid_block_size;
      // Subtract first and last border-spacing, and table border/padding.
      column_block_size -=
          border_spacing.block_size * 2 + border_padding.BlockSum();
    }

    table_grid_rect =
        LogicalRect(LayoutUnit(), table_box_extent->start,
                    container_builder_.InlineSize(), grid_block_size);
  }

  bool has_entered_table_box = false;
  if (InvolvedInBlockFragmentation(container_builder_)) {
    LayoutUnit consumed_table_box_block_size =
        previously_consumed_table_box_block_size;
    if (incoming_table_break_data)
      has_entered_table_box = incoming_table_break_data->has_entered_table_box;
    consumed_table_box_block_size += grid_block_size;
    has_entered_table_box |= table_box_extent.has_value();

    container_builder_.SetBreakTokenData(
        MakeGarbageCollected<TableBreakTokenData>(
            container_builder_.GetBreakTokenData(), rows,
            cell_block_constraints, sections, total_table_min_block_size_,
            consumed_table_box_block_size, has_entered_table_box,
            is_past_table_box));
  }

  ComputeTableSpecificFragmentData(grouped_children, column_locations, rows,
                                   table_borders, table_grid_rect,
                                   column_block_size);

  container_builder_.HandleOofsAndSpecialDescendants();

  if ((has_repeated_header ||
       container_builder_.ShouldCloneBoxEndDecorations()) &&
      has_entered_table_box && !table_box_will_continue &&
      !is_known_to_be_last_table_box_) {
    // The table's border box ends in this fragment. We started off without
    // knowing this (unlike for all other box types, we cannot know this
    // up-front for tables). There are two things that may have become incorrect
    // because of this, so that we need to relayout with the new information in
    // mind:
    //
    // 1. Repeated table headers. We have already laid out the header in a
    // repeatable manner, with an outgoing "repeat" break token, but it's not
    // going to repeat anymore, so the break token needs to go away.
    //
    // 2. Cloned block-end box decorations. Cloned block-end box decorations
    // reduce available fragmentainer space available, to prevent child content
    // from overlapping with this area. Since the border box ends here, we
    // shouldn't have reserved space for this.
    return container_builder_.Abort(LayoutResult::kNeedsRelayoutAsLastTableBox);
  }

  return container_builder_.ToBoxFragment();
}

}  // namespace blink
