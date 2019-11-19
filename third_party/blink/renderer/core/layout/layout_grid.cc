/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/layout_grid.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/grid_layout_utils.h"
#include "third_party/blink/renderer/core/layout/layout_state.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/paint/block_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/grid_area.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

LayoutGrid::LayoutGrid(Element* element)
    : LayoutBlock(element),
      grid_(Grid::Create(this)),
      track_sizing_algorithm_(this, *grid_) {
  DCHECK(!ChildrenInline());
}

LayoutGrid::~LayoutGrid() = default;

LayoutGrid* LayoutGrid::CreateAnonymous(Document* document) {
  LayoutGrid* layout_grid = new LayoutGrid(nullptr);
  layout_grid->SetDocumentForAnonymous(document);
  return layout_grid;
}

void LayoutGrid::AddChild(LayoutObject* new_child, LayoutObject* before_child) {
  LayoutBlock::AddChild(new_child, before_child);

  // Positioned grid items do not take up space or otherwise participate in the
  // layout of the grid, for that reason we don't need to mark the grid as dirty
  // when they are added.
  if (new_child->IsOutOfFlowPositioned())
    return;

  // The grid needs to be recomputed as it might contain auto-placed items that
  // will change their position.
  DirtyGrid();
}

void LayoutGrid::RemoveChild(LayoutObject* child) {
  LayoutBlock::RemoveChild(child);

  // Positioned grid items do not take up space or otherwise participate in the
  // layout of the grid, for that reason we don't need to mark the grid as dirty
  // when they are removed.
  if (child->IsOutOfFlowPositioned())
    return;

  // The grid needs to be recomputed as it might contain auto-placed items that
  // will change their position.
  DirtyGrid();
}

StyleSelfAlignmentData LayoutGrid::SelfAlignmentForChild(
    GridAxis axis,
    const LayoutBox& child,
    const ComputedStyle* style) const {
  return axis == kGridRowAxis ? JustifySelfForChild(child, style)
                              : AlignSelfForChild(child, style);
}

StyleSelfAlignmentData LayoutGrid::DefaultAlignment(
    GridAxis axis,
    const ComputedStyle& style) const {
  return axis == kGridRowAxis
             ? style.ResolvedJustifyItems(ItemPosition::kNormal)
             : style.ResolvedAlignItems(ItemPosition::kNormal);
}

bool LayoutGrid::DefaultAlignmentIsStretchOrNormal(
    GridAxis axis,
    const ComputedStyle& style) const {
  ItemPosition alignment = DefaultAlignment(axis, style).GetPosition();
  return alignment == ItemPosition::kStretch ||
         alignment == ItemPosition::kNormal;
}

bool LayoutGrid::SelfAlignmentChangedSize(GridAxis axis,
                                          const ComputedStyle& old_style,
                                          const ComputedStyle& new_style,
                                          const LayoutBox& child) const {
  return SelfAlignmentForChild(axis, child, &old_style).GetPosition() ==
                 ItemPosition::kStretch
             ? SelfAlignmentForChild(axis, child, &new_style).GetPosition() !=
                   ItemPosition::kStretch
             : SelfAlignmentForChild(axis, child, &new_style).GetPosition() ==
                   ItemPosition::kStretch;
}

bool LayoutGrid::DefaultAlignmentChangedSize(
    GridAxis axis,
    const ComputedStyle& old_style,
    const ComputedStyle& new_style) const {
  return DefaultAlignmentIsStretchOrNormal(axis, old_style)
             ? DefaultAlignment(axis, old_style).GetPosition() !=
                   DefaultAlignment(axis, new_style).GetPosition()
             : DefaultAlignmentIsStretchOrNormal(axis, new_style);
}

void LayoutGrid::StyleDidChange(StyleDifference diff,
                                const ComputedStyle* old_style) {
  LayoutBlock::StyleDidChange(diff, old_style);
  if (!old_style)
    return;

  const ComputedStyle& new_style = StyleRef();
  if (diff.NeedsFullLayout() &&
      (DefaultAlignmentChangedSize(kGridRowAxis, *old_style, new_style) ||
       DefaultAlignmentChangedSize(kGridColumnAxis, *old_style, new_style))) {
    // Style changes on the grid container implying stretching (to-stretch) or
    // shrinking (from-stretch) require the affected items to be laid out again.
    // These logic only applies to 'stretch' since the rest of the alignment
    // values don't change the size of the box.
    // In any case, the items' overrideSize will be cleared and recomputed (if
    // necessary)  as part of the Grid layout logic, triggered by this style
    // change.
    for (LayoutBox* child = FirstInFlowChildBox(); child;
         child = child->NextInFlowSiblingBox()) {
      if (SelfAlignmentChangedSize(kGridRowAxis, *old_style, new_style,
                                   *child) ||
          SelfAlignmentChangedSize(kGridColumnAxis, *old_style, new_style,
                                   *child)) {
        child->SetNeedsLayout(layout_invalidation_reason::kGridChanged);
      }
    }
  }

  // FIXME: The following checks could be narrowed down if we kept track of
  // which type of grid items we have:
  // - explicit grid size changes impact negative explicitely positioned and
  //   auto-placed grid items.
  // - named grid lines only impact grid items with named grid lines.
  // - auto-flow changes only impacts auto-placed children.

  if (ExplicitGridDidResize(*old_style) ||
      NamedGridLinesDefinitionDidChange(*old_style) ||
      old_style->GetGridAutoFlow() != StyleRef().GetGridAutoFlow() ||
      (diff.NeedsLayout() && (StyleRef().GridAutoRepeatColumns().size() ||
                              StyleRef().GridAutoRepeatRows().size())))
    DirtyGrid();
}

bool LayoutGrid::ExplicitGridDidResize(const ComputedStyle& old_style) const {
  return old_style.GridTemplateColumns().size() !=
             StyleRef().GridTemplateColumns().size() ||
         old_style.GridTemplateRows().size() !=
             StyleRef().GridTemplateRows().size() ||
         old_style.NamedGridAreaColumnCount() !=
             StyleRef().NamedGridAreaColumnCount() ||
         old_style.NamedGridAreaRowCount() !=
             StyleRef().NamedGridAreaRowCount() ||
         old_style.GridAutoRepeatColumns().size() !=
             StyleRef().GridAutoRepeatColumns().size() ||
         old_style.GridAutoRepeatRows().size() !=
             StyleRef().GridAutoRepeatRows().size();
}

bool LayoutGrid::NamedGridLinesDefinitionDidChange(
    const ComputedStyle& old_style) const {
  return old_style.NamedGridRowLines() != StyleRef().NamedGridRowLines() ||
         old_style.NamedGridColumnLines() != StyleRef().NamedGridColumnLines();
}

void LayoutGrid::ComputeTrackSizesForDefiniteSize(
    GridTrackSizingDirection direction,
    LayoutUnit available_space) {
  track_sizing_algorithm_.Setup(direction, NumTracks(direction, *grid_),
                                available_space);
  track_sizing_algorithm_.Run();

#if DCHECK_IS_ON()
  DCHECK(track_sizing_algorithm_.TracksAreWiderThanMinTrackBreadth());
#endif
}

void LayoutGrid::RepeatTracksSizingIfNeeded(
    LayoutUnit available_space_for_columns,
    LayoutUnit available_space_for_rows) {
  // In orthogonal flow cases column track's size is determined by using the
  // computed row track's size, which it was estimated during the first cycle of
  // the sizing algorithm.
  // TODO (lajava): these are just some of the cases which may require
  // a new cycle of the sizing algorithm; there may be more. In addition, not
  // all the cases with orthogonal flows require this extra cycle; we need a
  // more specific condition to detect whether child's min-content contribution
  // has changed or not.
  if (!has_any_orthogonal_item_ &&
      !track_sizing_algorithm_.HasAnyPercentSizedRowsIndefiniteHeight())
    return;

  // TODO (lajava): Whenever the min-content contribution of a grid item changes
  // we may need to update the grid container's intrinsic width. The new
  // intrinsic width may also affect the extra Track Sizing algorithm cycles we
  // are about to execute.
  // https://crbug.com/704713
  // https://github.com/w3c/csswg-drafts/issues/1039

  // Hence we need to repeat computeUsedBreadthOfGridTracks for both, columns
  // and rows, to determine the final values.
  ComputeTrackSizesForDefiniteSize(kForColumns, available_space_for_columns);
  ComputeContentPositionAndDistributionOffset(
      kForColumns, track_sizing_algorithm_.FreeSpace(kForColumns).value(),
      NonCollapsedTracks(kForColumns));
  ComputeTrackSizesForDefiniteSize(kForRows, available_space_for_rows);
  ComputeContentPositionAndDistributionOffset(
      kForRows, track_sizing_algorithm_.FreeSpace(kForRows).value(),
      NonCollapsedTracks(kForRows));
}

void LayoutGrid::UpdateBlockLayout(bool relayout_children) {
  DCHECK(NeedsLayout());

  // We cannot perform a simplifiedLayout() on a dirty grid that
  // has positioned items to be laid out.
  if (!relayout_children &&
      (!grid_->NeedsItemsPlacement() || !PosChildNeedsLayout()) &&
      SimplifiedLayout())
    return;

  SubtreeLayoutScope layout_scope(*this);

  PaintLayerScrollableArea::DelayScrollOffsetClampScope delay_clamp_scope;

  {
    // LayoutState needs this deliberate scope to pop before updating scroll
    // information (which may trigger relayout).
    LayoutState state(*this);

    LayoutSize previous_size = Size();
    has_definite_logical_height_ = HasDefiniteLogicalHeight();

    has_any_orthogonal_item_ = false;
    for (auto* child = FirstInFlowChildBox(); child;
         child = child->NextInFlowSiblingBox()) {
      // Grid's layout logic controls the grid item's override height, hence
      // we need to clear any override height set previously, so it doesn't
      // interfere in current layout execution.
      // Grid never uses the override width, that's why we don't need to clear
      // it.
      child->ClearOverrideLogicalHeight();

      // We may need to repeat the track sizing in case of any grid item was
      // orthogonal.
      if (GridLayoutUtils::IsOrthogonalChild(*this, *child))
        has_any_orthogonal_item_ = true;

      // We keep a cache of items with baseline as alignment values so
      // that we only compute the baseline shims for such items. This
      // cache is needed for performance related reasons due to the
      // cost of evaluating the item's participation in a baseline
      // context during the track sizing algorithm.
      if (IsBaselineAlignmentForChild(*child, kGridColumnAxis)) {
        track_sizing_algorithm_.CacheBaselineAlignedItem(*child,
                                                         kGridColumnAxis);
      }
      if (IsBaselineAlignmentForChild(*child, kGridRowAxis)) {
        track_sizing_algorithm_.CacheBaselineAlignedItem(*child, kGridRowAxis);
      }
    }
    baseline_items_cached_ = true;
    UpdateLogicalWidth();

    TextAutosizer::LayoutScope text_autosizer_layout_scope(this, &layout_scope);

    LayoutUnit available_space_for_columns = AvailableLogicalWidth();
    PlaceItemsOnGrid(track_sizing_algorithm_, available_space_for_columns);

    PerformGridItemsPreLayout(track_sizing_algorithm_);

    // 1- First, the track sizing algorithm is used to resolve the sizes of the
    // grid columns.
    // At this point the logical width is always definite as the above call to
    // updateLogicalWidth() properly resolves intrinsic sizes. We cannot do the
    // same for heights though because many code paths inside
    // updateLogicalHeight() require a previous call to setLogicalHeight() to
    // resolve heights properly (like for positioned items for example).
    ComputeTrackSizesForDefiniteSize(kForColumns, available_space_for_columns);

    // 1.5- Compute Content Distribution offsets for column tracks
    ComputeContentPositionAndDistributionOffset(
        kForColumns, track_sizing_algorithm_.FreeSpace(kForColumns).value(),
        NonCollapsedTracks(kForColumns));

    // 2- Next, the track sizing algorithm resolves the sizes of the grid rows,
    // using the grid column sizes calculated in the previous step.
    bool recompute_with_track_based_height = false;
    if (CachedHasDefiniteLogicalHeight()) {
      ComputeTrackSizesForDefiniteSize(
          kForRows, AvailableLogicalHeight(kExcludeMarginBorderPadding));
    } else if (HasOverrideIntrinsicContentLogicalHeight()) {
      ComputeTrackSizesForDefiniteSize(kForRows,
                                       OverrideIntrinsicContentLogicalHeight());
    } else {
      ComputeTrackSizesForIndefiniteSize(track_sizing_algorithm_, kForRows);
      if (ShouldApplySizeContainment())
        recompute_with_track_based_height = true;
    }
    LayoutUnit track_based_logical_height =
        track_sizing_algorithm_.ComputeTrackBasedSize() +
        BorderAndPaddingLogicalHeight();
    if (recompute_with_track_based_height)
      ComputeTrackSizesForDefiniteSize(kForRows, track_based_logical_height);

    // TODO(rego): We shouldn't need this once crbug.com/906530 is fixed.
    // Right now we need this because
    // LayoutBox::ComputeContentAndScrollbarLogicalHeightUsing() is adding the
    // ScrollbarLogicalHeight() for the intrinsic height cases. But that's
    // causing more problems as described in the bug linked before.
    if (!StyleRef().LogicalHeight().IsIntrinsic())
      track_based_logical_height += ScrollbarLogicalHeight();

    SetLogicalHeight(track_based_logical_height);
    UpdateLogicalHeight();

    // Once grid's indefinite height is resolved, we can compute the
    // available free space for Content Alignment.
    if (!CachedHasDefiniteLogicalHeight()) {
      track_sizing_algorithm_.SetFreeSpace(
          kForRows, LogicalHeight() - track_based_logical_height);
    }

    // 2.5- Compute Content Distribution offsets for rows tracks
    ComputeContentPositionAndDistributionOffset(
        kForRows, track_sizing_algorithm_.FreeSpace(kForRows).value(),
        NonCollapsedTracks(kForRows));

    // 3- If the min-content contribution of any grid items have changed based
    // on the row sizes calculated in step 2, steps 1 and 2 are repeated with
    // the new min-content contribution (once only).
    RepeatTracksSizingIfNeeded(available_space_for_columns,
                               ContentLogicalHeight());

    // Grid container should have the minimum height of a line if it's editable.
    // That doesn't affect track sizing though.
    if (HasLineIfEmpty())
      SetLogicalHeight(
          std::max(LogicalHeight(), MinimumLogicalHeightForEmptyLine()));

    LayoutGridItems();
    track_sizing_algorithm_.Reset();

    if (Size() != previous_size)
      relayout_children = true;

    LayoutPositionedObjects(relayout_children || IsDocumentElement());

    ComputeLayoutOverflow(ClientLogicalBottom());
  }

  UpdateAfterLayout();

  ClearNeedsLayout();

  track_sizing_algorithm_.ClearBaselineItemsCache();
  baseline_items_cached_ = false;
}

LayoutUnit LayoutGrid::GridGap(
    GridTrackSizingDirection direction,
    base::Optional<LayoutUnit> available_size) const {
  const GapLength& gap =
      direction == kForColumns ? StyleRef().ColumnGap() : StyleRef().RowGap();
  if (gap.IsNormal())
    return LayoutUnit();

  return ValueForLength(gap.GetLength(), available_size.value_or(LayoutUnit()));
}

LayoutUnit LayoutGrid::GridGap(GridTrackSizingDirection direction) const {
  LayoutUnit available_size;
  bool is_row_axis = direction == kForColumns;

  const GapLength& gap =
      is_row_axis ? StyleRef().ColumnGap() : StyleRef().RowGap();
  if (gap.IsNormal())
    return LayoutUnit();

  if (gap.GetLength().IsPercentOrCalc()) {
    available_size =
        is_row_axis ? AvailableLogicalWidth() : ContentLogicalHeight();
  }

  // TODO(rego): Maybe we could cache the computed percentage as a performance
  // improvement.
  return ValueForLength(gap.GetLength(), available_size);
}

LayoutUnit LayoutGrid::GuttersSize(
    const Grid& grid,
    GridTrackSizingDirection direction,
    size_t start_line,
    size_t span,
    base::Optional<LayoutUnit> available_size) const {
  if (span <= 1)
    return LayoutUnit();

  LayoutUnit gap = GridGap(direction, available_size);

  // Fast path, no collapsing tracks.
  if (!grid.HasAutoRepeatEmptyTracks(direction))
    return gap * (span - 1);

  // If there are collapsing tracks we need to be sure that gutters are properly
  // collapsed. Apart from that, if we have a collapsed track in the edges of
  // the span we're considering, we need to move forward (or backwards) in order
  // to know whether the collapsed tracks reach the end of the grid (so the gap
  // becomes 0) or there is a non empty track before that.

  LayoutUnit gap_accumulator;
  size_t end_line = start_line + span;

  for (size_t line = start_line; line < end_line - 1; ++line) {
    if (!grid.IsEmptyAutoRepeatTrack(direction, line))
      gap_accumulator += gap;
  }

  // The above loop adds one extra gap for trailing collapsed tracks.
  if (gap_accumulator && grid.IsEmptyAutoRepeatTrack(direction, end_line - 1)) {
    DCHECK_GE(gap_accumulator, gap);
    gap_accumulator -= gap;
  }

  // If the startLine is the start line of a collapsed track we need to go
  // backwards till we reach a non collapsed track. If we find a non collapsed
  // track we need to add that gap.
  size_t non_empty_tracks_before_start_line = 0;
  if (start_line && grid.IsEmptyAutoRepeatTrack(direction, start_line)) {
    non_empty_tracks_before_start_line = start_line;
    auto begin = grid.AutoRepeatEmptyTracks(direction)->begin();
    for (auto it = begin; *it != start_line; ++it) {
      DCHECK(non_empty_tracks_before_start_line);
      --non_empty_tracks_before_start_line;
    }
    if (non_empty_tracks_before_start_line)
      gap_accumulator += gap;
  }

  // If the endLine is the end line of a collapsed track we need to go forward
  // till we reach a non collapsed track. If we find a non collapsed track we
  // need to add that gap.
  if (grid.IsEmptyAutoRepeatTrack(direction, end_line - 1)) {
    size_t non_empty_tracks_after_end_line =
        grid.NumTracks(direction) - end_line;
    auto current_empty_track =
        grid.AutoRepeatEmptyTracks(direction)->find(end_line - 1);
    auto end_empty_track = grid.AutoRepeatEmptyTracks(direction)->end();
    // HashSet iterators do not implement operator- so we have to manually
    // iterate to know the number of remaining empty tracks.
    for (auto it = ++current_empty_track; it != end_empty_track; ++it) {
      DCHECK(non_empty_tracks_after_end_line);
      --non_empty_tracks_after_end_line;
    }
    if (non_empty_tracks_after_end_line) {
      // We shouldn't count the gap twice if the span starts and ends
      // in a collapsed track bewtween two non-empty tracks.
      if (!non_empty_tracks_before_start_line)
        gap_accumulator += gap;
    } else if (non_empty_tracks_before_start_line) {
      // We shouldn't count the gap if the the span starts and ends in
      // a collapsed but there isn't non-empty tracks afterwards (it's
      // at the end of the grid).
      gap_accumulator -= gap;
    }
  }

  return gap_accumulator;
}

void LayoutGrid::ComputeIntrinsicLogicalWidths(
    LayoutUnit& min_logical_width,
    LayoutUnit& max_logical_width) const {
  LayoutUnit scrollbar_width = LayoutUnit(ScrollbarLogicalWidth());
  min_logical_width = scrollbar_width;
  max_logical_width = scrollbar_width;

  if (HasOverrideIntrinsicContentLogicalWidth()) {
    min_logical_width += OverrideIntrinsicContentLogicalWidth();
    max_logical_width = min_logical_width;
    return;
  }

  std::unique_ptr<Grid> grid = Grid::Create(this);
  GridTrackSizingAlgorithm algorithm(this, *grid);
  PlaceItemsOnGrid(algorithm, base::nullopt);

  PerformGridItemsPreLayout(algorithm);

  if (baseline_items_cached_) {
    algorithm.CopyBaselineItemsCache(track_sizing_algorithm_, kGridRowAxis);
  } else {
    for (auto* child = FirstInFlowChildBox(); child;
         child = child->NextInFlowSiblingBox()) {
      if (IsBaselineAlignmentForChild(*child, kGridRowAxis)) {
        algorithm.CacheBaselineAlignedItem(*child, kGridRowAxis);
      }
    }
  }

  ComputeTrackSizesForIndefiniteSize(algorithm, kForColumns);

  size_t number_of_tracks = algorithm.Tracks(kForColumns).size();
  LayoutUnit total_gutters_size = GuttersSize(
      algorithm.GetGrid(), kForColumns, 0, number_of_tracks, base::nullopt);

  min_logical_width += algorithm.MinContentSize() + total_gutters_size;
  max_logical_width += algorithm.MaxContentSize() + total_gutters_size;
}

void LayoutGrid::ComputeTrackSizesForIndefiniteSize(
    GridTrackSizingAlgorithm& algo,
    GridTrackSizingDirection direction) const {
  const Grid& grid = algo.GetGrid();
  algo.Setup(direction, NumTracks(direction, grid), base::nullopt);
  algo.Run();

#if DCHECK_IS_ON()
  DCHECK(algo.TracksAreWiderThanMinTrackBreadth());
#endif
}

base::Optional<LayoutUnit> LayoutGrid::OverrideIntrinsicContentLogicalSize(
    GridTrackSizingDirection direction) const {
  if (direction == kForColumns && HasOverrideIntrinsicContentLogicalWidth())
    return OverrideIntrinsicContentLogicalWidth();
  if (direction == kForRows && HasOverrideIntrinsicContentLogicalHeight())
    return OverrideIntrinsicContentLogicalHeight();
  return base::nullopt;
}

LayoutUnit LayoutGrid::OverrideContainingBlockContentSizeForChild(
    const LayoutBox& child,
    GridTrackSizingDirection direction) {
  return direction == kForColumns
             ? child.OverrideContainingBlockContentLogicalWidth()
             : child.OverrideContainingBlockContentLogicalHeight();
}

// Unfortunately there are still many layout methods that return -1 for
// non-resolvable sizes. We prefer to represent them with base::nullopt.
static base::Optional<LayoutUnit> ConvertLayoutUnitToOptional(LayoutUnit size) {
  if (size == -1)
    return base::nullopt;
  return size;
}

size_t LayoutGrid::ComputeAutoRepeatTracksCount(
    GridTrackSizingDirection direction,
    base::Optional<LayoutUnit> available_size) const {
  DCHECK(!available_size || available_size.value() != -1);
  bool is_row_axis = direction == kForColumns;
  // Since auto-fit collapses empty tracks, and contain: size dictates that
  // children should be ignored for the purposes of layout, we can conclude that
  // if these conditions hold we have 0 repetitions.
  if (ShouldApplySizeContainment() &&
      ((is_row_axis &&
        StyleRef().GridAutoRepeatColumnsType() == AutoRepeatType::kAutoFit) ||
       (!is_row_axis &&
        StyleRef().GridAutoRepeatRowsType() == AutoRepeatType::kAutoFit)))
    return 0;
  const auto& auto_repeat_tracks = is_row_axis
                                       ? StyleRef().GridAutoRepeatColumns()
                                       : StyleRef().GridAutoRepeatRows();
  size_t auto_repeat_track_list_length = auto_repeat_tracks.size();

  if (!auto_repeat_track_list_length)
    return 0;

  bool needs_to_fulfill_minimum_size = false;
  if (!available_size) {
    const Length& max_size = is_row_axis ? StyleRef().LogicalMaxWidth()
                                         : StyleRef().LogicalMaxHeight();
    base::Optional<LayoutUnit> containing_block_available_size;
    LayoutUnit available_max_size = LayoutUnit();
    if (max_size.IsSpecified()) {
      if (max_size.IsPercentOrCalc()) {
        containing_block_available_size =
            is_row_axis ? ContainingBlockLogicalWidthForContent()
                        : ContainingBlockLogicalHeightForContent(
                              kExcludeMarginBorderPadding);
      }
      LayoutUnit max_size_value = ValueForLength(
          max_size, containing_block_available_size.value_or(LayoutUnit()));
      available_max_size =
          is_row_axis
              ? AdjustContentBoxLogicalWidthForBoxSizing(max_size_value)
              : AdjustContentBoxLogicalHeightForBoxSizing(max_size_value);
    }

    base::Optional<LayoutUnit> intrinsic_size_override =
        OverrideIntrinsicContentLogicalSize(direction);

    const Length& min_size = is_row_axis ? StyleRef().LogicalMinWidth()
                                         : StyleRef().LogicalMinHeight();
    if (!available_max_size && !min_size.IsSpecified() &&
        !intrinsic_size_override) {
      return auto_repeat_track_list_length;
    }

    LayoutUnit available_min_size = LayoutUnit();
    if (min_size.IsSpecified()) {
      if (!containing_block_available_size && min_size.IsPercentOrCalc()) {
        containing_block_available_size =
            is_row_axis ? ContainingBlockLogicalWidthForContent()
                        : ContainingBlockLogicalHeightForContent(
                              kExcludeMarginBorderPadding);
      }
      LayoutUnit min_size_value = ValueForLength(
          min_size, containing_block_available_size.value_or(LayoutUnit()));
      available_min_size =
          is_row_axis
              ? AdjustContentBoxLogicalWidthForBoxSizing(min_size_value)
              : AdjustContentBoxLogicalHeightForBoxSizing(min_size_value);
    }

    // See https://drafts.csswg.org/css-grid/#auto-repeat for explanation of why
    // we use needs_to_fulfill_minimum_size. Note that we can treat the
    // intrinsic-size similar to min-size when filling the remainder of space.
    // That is, we should fill the intrinsic size fully.
    if (!max_size.IsSpecified() &&
        (min_size.IsSpecified() || intrinsic_size_override)) {
      needs_to_fulfill_minimum_size = true;
    }

    // Now we need to determine the available size.
    // We start with the maximum of all of the values. Then, we need to see if
    // max-size is breached. If it is, then we can shrink the size back up to
    // the max of min-size and max-size. This is because we can ignore
    // intrinsic-size in this situation since the min- and max- sizes take
    // priority.
    auto available_intrinsic_size =
        intrinsic_size_override.value_or(LayoutUnit());
    available_size =
        std::max(std::max(available_min_size, available_intrinsic_size),
                 available_max_size);
    if (max_size.IsSpecified() && available_max_size < available_size) {
      available_size = std::max(available_min_size, available_max_size);
    }
  }

  LayoutUnit auto_repeat_tracks_size;
  for (auto auto_track_size : auto_repeat_tracks) {
    DCHECK(auto_track_size.MinTrackBreadth().IsLength());
    DCHECK(!auto_track_size.MinTrackBreadth().IsFlex());
    bool has_definite_max_track_sizing_function =
        auto_track_size.MaxTrackBreadth().IsLength() &&
        !auto_track_size.MaxTrackBreadth().IsContentSized();
    const Length& track_length =
        has_definite_max_track_sizing_function
            ? auto_track_size.MaxTrackBreadth().length()
            : auto_track_size.MinTrackBreadth().length();
    auto_repeat_tracks_size +=
        ValueForLength(track_length, available_size.value());
  }
  // For the purpose of finding the number of auto-repeated tracks, the UA must
  // floor the track size to a UA-specified value to avoid division by zero. It
  // is suggested that this floor be 1px.
  auto_repeat_tracks_size =
      std::max<LayoutUnit>(LayoutUnit(1), auto_repeat_tracks_size);

  // There will be always at least 1 auto-repeat track, so take it already into
  // account when computing the total track size.
  LayoutUnit tracks_size = auto_repeat_tracks_size;
  const Vector<GridTrackSize>& track_sizes =
      is_row_axis ? StyleRef().GridTemplateColumns()
                  : StyleRef().GridTemplateRows();

  for (const auto& track : track_sizes) {
    bool has_definite_max_track_breadth =
        track.MaxTrackBreadth().IsLength() &&
        !track.MaxTrackBreadth().IsContentSized();
    DCHECK(has_definite_max_track_breadth ||
           (track.MinTrackBreadth().IsLength() &&
            !track.MinTrackBreadth().IsContentSized()));
    tracks_size += ValueForLength(has_definite_max_track_breadth
                                      ? track.MaxTrackBreadth().length()
                                      : track.MinTrackBreadth().length(),
                                  available_size.value());
  }

  // Add gutters as if there where only 1 auto repeat track. Gaps between auto
  // repeat tracks will be added later when computing the repetitions.
  LayoutUnit gap_size = GridGap(direction, available_size);
  tracks_size +=
      gap_size * (track_sizes.size() + auto_repeat_track_list_length - 1);

  LayoutUnit free_space = available_size.value() - tracks_size;
  if (free_space <= 0)
    return auto_repeat_track_list_length;

  LayoutUnit auto_repeat_size_with_gap =
      auto_repeat_tracks_size + gap_size * auto_repeat_track_list_length;

  size_t repetitions = 1 + (free_space / auto_repeat_size_with_gap).ToInt();
  free_space -= auto_repeat_size_with_gap * (repetitions - 1);

  // Provided the grid container does not have a definite size or max-size in
  // the relevant axis, if the min size is definite then the number of
  // repetitions is the smallest positive integer that fulfills that
  // minimum requirement. If after determining the repetitions, we still have
  // free space, then we need one more repetition to ensure we fill at least all
  // of the space.
  if (needs_to_fulfill_minimum_size && free_space)
    ++repetitions;

  return repetitions * auto_repeat_track_list_length;
}

std::unique_ptr<OrderedTrackIndexSet>
LayoutGrid::ComputeEmptyTracksForAutoRepeat(
    Grid& grid,
    GridTrackSizingDirection direction) const {
  bool is_row_axis = direction == kForColumns;
  if ((is_row_axis &&
       StyleRef().GridAutoRepeatColumnsType() != AutoRepeatType::kAutoFit) ||
      (!is_row_axis &&
       StyleRef().GridAutoRepeatRowsType() != AutoRepeatType::kAutoFit))
    return nullptr;

  std::unique_ptr<OrderedTrackIndexSet> empty_track_indexes;
  size_t insertion_point =
      is_row_axis ? StyleRef().GridAutoRepeatColumnsInsertionPoint()
                  : StyleRef().GridAutoRepeatRowsInsertionPoint();
  size_t first_auto_repeat_track =
      insertion_point + std::abs(grid.SmallestTrackStart(direction));
  size_t last_auto_repeat_track =
      first_auto_repeat_track + grid.AutoRepeatTracks(direction);

  if (!grid.HasGridItems()) {
    empty_track_indexes = std::make_unique<OrderedTrackIndexSet>();
    for (size_t track_index = first_auto_repeat_track;
         track_index < last_auto_repeat_track; ++track_index)
      empty_track_indexes->insert(track_index);
  } else {
    for (size_t track_index = first_auto_repeat_track;
         track_index < last_auto_repeat_track; ++track_index) {
      auto iterator = grid.CreateIterator(direction, track_index);
      if (!iterator->NextGridItem()) {
        if (!empty_track_indexes)
          empty_track_indexes = std::make_unique<OrderedTrackIndexSet>();
        empty_track_indexes->insert(track_index);
      }
    }
  }
  return empty_track_indexes;
}

size_t LayoutGrid::ClampAutoRepeatTracks(GridTrackSizingDirection direction,
                                         size_t auto_repeat_tracks) const {
  if (!auto_repeat_tracks)
    return 0;

  size_t insertion_point =
      direction == kForColumns
          ? StyleRef().GridAutoRepeatColumnsInsertionPoint()
          : StyleRef().GridAutoRepeatRowsInsertionPoint();

  if (insertion_point == 0)
    return std::min<size_t>(auto_repeat_tracks, kGridMaxTracks);

  if (insertion_point >= kGridMaxTracks)
    return 0;

  return std::min(auto_repeat_tracks,
                  static_cast<size_t>(kGridMaxTracks) - insertion_point);
}

// TODO(svillar): we shouldn't have to pass the available logical width as
// argument. The problem is that availableLogicalWidth() does always return a
// value even if we cannot resolve it like when computing the intrinsic size
// (preferred widths). That's why we pass the responsibility to the caller who
// does know whether the available logical width is indefinite or not.
void LayoutGrid::PlaceItemsOnGrid(
    GridTrackSizingAlgorithm& algorithm,
    base::Optional<LayoutUnit> available_logical_width) const {
  Grid& grid = algorithm.GetMutableGrid();
  size_t auto_repeat_rows = ComputeAutoRepeatTracksCount(
      kForRows, ConvertLayoutUnitToOptional(
                    AvailableLogicalHeightForPercentageComputation()));
  size_t auto_repeat_columns =
      ComputeAutoRepeatTracksCount(kForColumns, available_logical_width);

  auto_repeat_rows = ClampAutoRepeatTracks(kForRows, auto_repeat_rows);
  auto_repeat_columns = ClampAutoRepeatTracks(kForColumns, auto_repeat_columns);

  if (auto_repeat_rows != grid.AutoRepeatTracks(kForRows) ||
      auto_repeat_columns != grid.AutoRepeatTracks(kForColumns)) {
    grid.SetNeedsItemsPlacement(true);
    grid.SetAutoRepeatTracks(auto_repeat_rows, auto_repeat_columns);
  }

  if (!grid.NeedsItemsPlacement())
    return;

  DCHECK(!grid.HasGridItems());
  PopulateExplicitGridAndOrderIterator(grid);

  Vector<LayoutBox*> auto_major_axis_auto_grid_items;
  Vector<LayoutBox*> specified_major_axis_auto_grid_items;
#if DCHECK_IS_ON()
  DCHECK(!grid.HasAnyGridItemPaintOrder());
#endif
  size_t child_index = 0;
  for (LayoutBox* child = grid.GetOrderIterator().First(); child;
       child = grid.GetOrderIterator().Next()) {
    if (child->IsOutOfFlowPositioned())
      continue;

    // Grid items should use the grid area sizes instead of the containing block
    // (grid container) sizes, we initialize the overrides here if needed to
    // ensure it.
    if (!child->HasOverrideContainingBlockContentLogicalWidth())
      child->SetOverrideContainingBlockContentLogicalWidth(LayoutUnit());
    if (!child->HasOverrideContainingBlockContentLogicalHeight())
      child->SetOverrideContainingBlockContentLogicalHeight(LayoutUnit(-1));

    grid.SetGridItemPaintOrder(*child, child_index++);

    GridArea area = grid.GridItemArea(*child);
    if (!area.rows.IsIndefinite())
      area.rows.Translate(abs(grid.SmallestTrackStart(kForRows)));
    if (!area.columns.IsIndefinite())
      area.columns.Translate(abs(grid.SmallestTrackStart(kForColumns)));

    if (area.rows.IsIndefinite() || area.columns.IsIndefinite()) {
      grid.SetGridItemArea(*child, area);
      GridSpan major_axis_positions =
          (AutoPlacementMajorAxisDirection() == kForColumns) ? area.columns
                                                             : area.rows;
      if (major_axis_positions.IsIndefinite())
        auto_major_axis_auto_grid_items.push_back(child);
      else
        specified_major_axis_auto_grid_items.push_back(child);
      continue;
    }
    grid.Insert(*child, area);
  }

#if DCHECK_IS_ON()
  if (grid.HasGridItems()) {
    DCHECK_GE(grid.NumTracks(kForRows),
              GridPositionsResolver::ExplicitGridRowCount(
                  *Style(), grid.AutoRepeatTracks(kForRows)));
    DCHECK_GE(grid.NumTracks(kForColumns),
              GridPositionsResolver::ExplicitGridColumnCount(
                  *Style(), grid.AutoRepeatTracks(kForColumns)));
  }
#endif

  PlaceSpecifiedMajorAxisItemsOnGrid(grid,
                                     specified_major_axis_auto_grid_items);
  PlaceAutoMajorAxisItemsOnGrid(grid, auto_major_axis_auto_grid_items);

  // Compute collapsable tracks for auto-fit.
  grid.SetAutoRepeatEmptyColumns(
      ComputeEmptyTracksForAutoRepeat(grid, kForColumns));
  grid.SetAutoRepeatEmptyRows(ComputeEmptyTracksForAutoRepeat(grid, kForRows));

  grid.SetNeedsItemsPlacement(false);

#if DCHECK_IS_ON()
  for (LayoutBox* child = grid.GetOrderIterator().First(); child;
       child = grid.GetOrderIterator().Next()) {
    if (child->IsOutOfFlowPositioned())
      continue;

    GridArea area = grid.GridItemArea(*child);
    DCHECK(area.rows.IsTranslatedDefinite());
    DCHECK(area.columns.IsTranslatedDefinite());
  }
#endif
}

// TODO(lajava): Consider rafactoring this code with
// LocalFrameView::PrepareOrthogonalWritingModeRootForLayout
static bool PrepareOrthogonalWritingModeRootForLayout(LayoutObject& root) {
  DCHECK(root.IsBox() && ToLayoutBox(root).IsOrthogonalWritingModeRoot());
  if (!root.NeedsLayout() || root.IsOutOfFlowPositioned() ||
      root.IsColumnSpanAll() || root.IsTablePart())
    return false;

  return true;
}

void LayoutGrid::PerformGridItemsPreLayout(
    const GridTrackSizingAlgorithm& algorithm) const {
  DCHECK(!algorithm.GetGrid().NeedsItemsPlacement());
  if (!GetDocument().View()->IsInPerformLayout())
    return;
  for (auto* child = FirstInFlowChildBox(); child;
       child = child->NextInFlowSiblingBox()) {
    // Blink does a pre-layout of all the orthogonal boxes in the layout
    // tree (see how LocalFrameView::PerformLayout calls its
    // LayoutOrthogonalWritingModeRoots function). However, grid items
    // don't participate in this process (see the function
    // PrepareOrthogonalWritingModeRootForLayout) because it's useless
    // and even wrong if they don't have their corresponding Grid Area.
    // TODO(jfernandez): Consider rafactoring this code with
    // LocalFrameView::LayoutOrthogonalWritingModeRoots
    if (GridLayoutUtils::IsOrthogonalChild(*this, *child)) {
      if (PrepareOrthogonalWritingModeRootForLayout(*child)) {
        UpdateGridAreaLogicalSize(
            *child, algorithm.EstimatedGridAreaBreadthForChild(*child));
        child->LayoutIfNeeded();
        continue;
      }
    }
    // We need to layout the item to know whether it must synthesize its
    // baseline or not, which may imply a cyclic sizing dependency.
    // TODO (jfernandez): Can we avoid it ?
    if (IsBaselineAlignmentForChild(*child)) {
      if (child->HasRelativeLogicalWidth() ||
          child->HasRelativeLogicalHeight()) {
        UpdateGridAreaLogicalSize(
            *child, algorithm.EstimatedGridAreaBreadthForChild(*child));
      }
      child->LayoutIfNeeded();
    }
  }
}

void LayoutGrid::PopulateExplicitGridAndOrderIterator(Grid& grid) const {
  OrderIteratorPopulator populator(grid.GetOrderIterator());
  int smallest_row_start = 0;
  int smallest_column_start = 0;

  size_t auto_repeat_rows = grid.AutoRepeatTracks(kForRows);
  size_t auto_repeat_columns = grid.AutoRepeatTracks(kForColumns);
  size_t maximum_row_index =
      GridPositionsResolver::ExplicitGridRowCount(*Style(), auto_repeat_rows);
  size_t maximum_column_index = GridPositionsResolver::ExplicitGridColumnCount(
      *Style(), auto_repeat_columns);

  for (LayoutBox* child = FirstInFlowChildBox(); child;
       child = child->NextInFlowSiblingBox()) {
    populator.CollectChild(child);

    // This function bypasses the cache (gridItemArea()) as it is used to
    // build it.
    GridSpan row_positions =
        GridPositionsResolver::ResolveGridPositionsFromStyle(
            *Style(), *child, kForRows, auto_repeat_rows);
    GridSpan column_positions =
        GridPositionsResolver::ResolveGridPositionsFromStyle(
            *Style(), *child, kForColumns, auto_repeat_columns);
    grid.SetGridItemArea(*child, GridArea(row_positions, column_positions));

    // |positions| is 0 if we need to run the auto-placement algorithm.
    if (!row_positions.IsIndefinite()) {
      smallest_row_start =
          std::min(smallest_row_start, row_positions.UntranslatedStartLine());
      maximum_row_index =
          std::max<int>(maximum_row_index, row_positions.UntranslatedEndLine());
    } else {
      // Grow the grid for items with a definite row span, getting the largest
      // such span.
      size_t span_size =
          GridPositionsResolver::SpanSizeForAutoPlacedItem(*child, kForRows);
      maximum_row_index = std::max(maximum_row_index, span_size);
    }

    if (!column_positions.IsIndefinite()) {
      smallest_column_start = std::min(
          smallest_column_start, column_positions.UntranslatedStartLine());
      maximum_column_index = std::max<int>(
          maximum_column_index, column_positions.UntranslatedEndLine());
    } else {
      // Grow the grid for items with a definite column span, getting the
      // largest such span.
      size_t span_size =
          GridPositionsResolver::SpanSizeForAutoPlacedItem(*child, kForColumns);
      maximum_column_index = std::max(maximum_column_index, span_size);
    }
  }

  grid.SetSmallestTracksStart(smallest_row_start, smallest_column_start);
  grid.EnsureGridSize(maximum_row_index + abs(smallest_row_start),
                      maximum_column_index + abs(smallest_column_start));
}

std::unique_ptr<GridArea>
LayoutGrid::CreateEmptyGridAreaAtSpecifiedPositionsOutsideGrid(
    const Grid& grid,
    const LayoutBox& grid_item,
    GridTrackSizingDirection specified_direction,
    const GridSpan& specified_positions) const {
  GridTrackSizingDirection cross_direction =
      specified_direction == kForColumns ? kForRows : kForColumns;
  const size_t end_of_cross_direction = grid.NumTracks(cross_direction);
  size_t cross_direction_span_size =
      GridPositionsResolver::SpanSizeForAutoPlacedItem(grid_item,
                                                       cross_direction);
  GridSpan cross_direction_positions = GridSpan::TranslatedDefiniteGridSpan(
      end_of_cross_direction,
      end_of_cross_direction + cross_direction_span_size);
  return std::make_unique<GridArea>(
      specified_direction == kForColumns ? cross_direction_positions
                                         : specified_positions,
      specified_direction == kForColumns ? specified_positions
                                         : cross_direction_positions);
}

void LayoutGrid::PlaceSpecifiedMajorAxisItemsOnGrid(
    Grid& grid,
    const Vector<LayoutBox*>& auto_grid_items) const {
  bool is_for_columns = AutoPlacementMajorAxisDirection() == kForColumns;
  bool is_grid_auto_flow_dense = StyleRef().IsGridAutoFlowAlgorithmDense();

  // Mapping between the major axis tracks (rows or columns) and the last
  // auto-placed item's position inserted on that track. This is needed to
  // implement "sparse" packing for items locked to a given track.
  // See https://drafts.csswg.org/css-grid/#auto-placement-algo
  HashMap<unsigned, unsigned, DefaultHash<unsigned>::Hash,
          WTF::UnsignedWithZeroKeyHashTraits<unsigned>>
      minor_axis_cursors;

  for (auto* const auto_grid_item : auto_grid_items) {
    GridSpan major_axis_positions =
        grid.GridItemSpan(*auto_grid_item, AutoPlacementMajorAxisDirection());
    DCHECK(major_axis_positions.IsTranslatedDefinite());
    DCHECK(
        !grid.GridItemSpan(*auto_grid_item, AutoPlacementMinorAxisDirection())
             .IsTranslatedDefinite());
    size_t minor_axis_span_size =
        GridPositionsResolver::SpanSizeForAutoPlacedItem(
            *auto_grid_item, AutoPlacementMinorAxisDirection());
    unsigned major_axis_initial_position = major_axis_positions.StartLine();

    auto iterator = grid.CreateIterator(
        AutoPlacementMajorAxisDirection(), major_axis_positions.StartLine(),
        is_grid_auto_flow_dense
            ? 0
            : minor_axis_cursors.at(major_axis_initial_position));
    std::unique_ptr<GridArea> empty_grid_area = iterator->NextEmptyGridArea(
        major_axis_positions.IntegerSpan(), minor_axis_span_size);
    DCHECK(empty_grid_area);

    grid.Insert(*auto_grid_item, *empty_grid_area);

    if (!is_grid_auto_flow_dense)
      minor_axis_cursors.Set(major_axis_initial_position,
                             is_for_columns
                                 ? empty_grid_area->rows.StartLine()
                                 : empty_grid_area->columns.StartLine());
  }
}

void LayoutGrid::PlaceAutoMajorAxisItemsOnGrid(
    Grid& grid,
    const Vector<LayoutBox*>& auto_grid_items) const {
  std::pair<size_t, size_t> auto_placement_cursor = std::make_pair(0, 0);
  bool is_grid_auto_flow_dense = StyleRef().IsGridAutoFlowAlgorithmDense();

  for (auto* const auto_grid_item : auto_grid_items) {
    PlaceAutoMajorAxisItemOnGrid(grid, *auto_grid_item, auto_placement_cursor);

    // If grid-auto-flow is dense, reset auto-placement cursor.
    if (is_grid_auto_flow_dense) {
      auto_placement_cursor.first = 0;
      auto_placement_cursor.second = 0;
    }
  }
}

void LayoutGrid::PlaceAutoMajorAxisItemOnGrid(
    Grid& grid,
    LayoutBox& grid_item,
    std::pair<size_t, size_t>& auto_placement_cursor) const {
  GridSpan minor_axis_positions =
      grid.GridItemSpan(grid_item, AutoPlacementMinorAxisDirection());
  DCHECK(!grid.GridItemSpan(grid_item, AutoPlacementMajorAxisDirection())
              .IsTranslatedDefinite());
  size_t major_axis_span_size =
      GridPositionsResolver::SpanSizeForAutoPlacedItem(
          grid_item, AutoPlacementMajorAxisDirection());

  const size_t end_of_major_axis =
      grid.NumTracks(AutoPlacementMajorAxisDirection());
  size_t major_axis_auto_placement_cursor =
      AutoPlacementMajorAxisDirection() == kForColumns
          ? auto_placement_cursor.second
          : auto_placement_cursor.first;
  size_t minor_axis_auto_placement_cursor =
      AutoPlacementMajorAxisDirection() == kForColumns
          ? auto_placement_cursor.first
          : auto_placement_cursor.second;

  std::unique_ptr<GridArea> empty_grid_area;
  if (minor_axis_positions.IsTranslatedDefinite()) {
    // Move to the next track in major axis if initial position in minor axis is
    // before auto-placement cursor.
    if (minor_axis_positions.StartLine() < minor_axis_auto_placement_cursor)
      major_axis_auto_placement_cursor++;

    if (major_axis_auto_placement_cursor < end_of_major_axis) {
      auto iterator = grid.CreateIterator(AutoPlacementMinorAxisDirection(),
                                          minor_axis_positions.StartLine(),
                                          major_axis_auto_placement_cursor);
      empty_grid_area = iterator->NextEmptyGridArea(
          minor_axis_positions.IntegerSpan(), major_axis_span_size);
    }

    if (!empty_grid_area) {
      empty_grid_area = CreateEmptyGridAreaAtSpecifiedPositionsOutsideGrid(
          grid, grid_item, AutoPlacementMinorAxisDirection(),
          minor_axis_positions);
    }
  } else {
    size_t minor_axis_span_size =
        GridPositionsResolver::SpanSizeForAutoPlacedItem(
            grid_item, AutoPlacementMinorAxisDirection());

    for (size_t major_axis_index = major_axis_auto_placement_cursor;
         major_axis_index < end_of_major_axis; ++major_axis_index) {
      auto iterator = grid.CreateIterator(AutoPlacementMajorAxisDirection(),
                                          major_axis_index,
                                          minor_axis_auto_placement_cursor);
      empty_grid_area = iterator->NextEmptyGridArea(major_axis_span_size,
                                                    minor_axis_span_size);
      DCHECK(empty_grid_area);

      // Check that it fits in the minor axis direction, as we shouldn't grow
      // in that direction here (it was already managed in
      // populateExplicitGridAndOrderIterator()).
      size_t minor_axis_final_position_index =
          AutoPlacementMinorAxisDirection() == kForColumns
              ? empty_grid_area->columns.EndLine()
              : empty_grid_area->rows.EndLine();
      const size_t end_of_minor_axis =
          grid.NumTracks(AutoPlacementMinorAxisDirection());
      if (minor_axis_final_position_index <= end_of_minor_axis)
        break;

      // Discard empty grid area as it does not fit in the minor axis
      // direction. We don't need to create a new empty grid area yet as we
      // might find a valid one in the next iteration.
      empty_grid_area.reset();

      // As we're moving to the next track in the major axis we should reset the
      // auto-placement cursor in the minor axis.
      minor_axis_auto_placement_cursor = 0;
    }

    if (!empty_grid_area)
      empty_grid_area = CreateEmptyGridAreaAtSpecifiedPositionsOutsideGrid(
          grid, grid_item, AutoPlacementMinorAxisDirection(),
          GridSpan::TranslatedDefiniteGridSpan(0, minor_axis_span_size));
  }

  grid.Insert(grid_item, *empty_grid_area);
  // Move auto-placement cursor to the new position.
  auto_placement_cursor.first = empty_grid_area->rows.StartLine();
  auto_placement_cursor.second = empty_grid_area->columns.StartLine();
}

GridTrackSizingDirection LayoutGrid::AutoPlacementMajorAxisDirection() const {
  return StyleRef().IsGridAutoFlowDirectionColumn() ? kForColumns : kForRows;
}

GridTrackSizingDirection LayoutGrid::AutoPlacementMinorAxisDirection() const {
  return StyleRef().IsGridAutoFlowDirectionColumn() ? kForRows : kForColumns;
}

void LayoutGrid::DirtyGrid() {
  if (grid_->NeedsItemsPlacement())
    return;

  grid_->SetNeedsItemsPlacement(true);
}

Vector<LayoutUnit> LayoutGrid::TrackSizesForComputedStyle(
    GridTrackSizingDirection direction) const {
  bool is_row_axis = direction == kForColumns;
  auto& positions = is_row_axis ? column_positions_ : row_positions_;
  size_t num_positions = positions.size();
  LayoutUnit offset_between_tracks =
      is_row_axis ? offset_between_columns_.distribution_offset
                  : offset_between_rows_.distribution_offset;

  Vector<LayoutUnit> tracks;
  if (num_positions < 2)
    return tracks;

  DCHECK(!grid_->NeedsItemsPlacement());
  bool has_collapsed_tracks = grid_->HasAutoRepeatEmptyTracks(direction);
  LayoutUnit gap = !has_collapsed_tracks ? GridGap(direction) : LayoutUnit();
  tracks.ReserveCapacity(num_positions - 1);
  for (size_t i = 0; i < num_positions - 2; ++i)
    tracks.push_back(positions[i + 1] - positions[i] - offset_between_tracks -
                     gap);
  tracks.push_back(positions[num_positions - 1] - positions[num_positions - 2]);

  if (!has_collapsed_tracks)
    return tracks;

  size_t remaining_empty_tracks =
      grid_->AutoRepeatEmptyTracks(direction)->size();
  size_t last_line = tracks.size();
  gap = GridGap(direction);
  for (size_t i = 1; i < last_line; ++i) {
    if (grid_->IsEmptyAutoRepeatTrack(direction, i - 1)) {
      --remaining_empty_tracks;
    } else {
      // Remove the gap between consecutive non empty tracks. Remove it also
      // just once for an arbitrary number of empty tracks between two non empty
      // ones.
      bool all_remaining_tracks_are_empty =
          remaining_empty_tracks == (last_line - i);
      if (!all_remaining_tracks_are_empty ||
          !grid_->IsEmptyAutoRepeatTrack(direction, i))
        tracks[i - 1] -= gap;
    }
  }

  return tracks;
}

const StyleContentAlignmentData& LayoutGrid::ContentAlignmentNormalBehavior() {
  static const StyleContentAlignmentData kNormalBehavior = {
      ContentPosition::kNormal, ContentDistributionType::kStretch};
  return kNormalBehavior;
}

static bool OverrideSizeChanged(const LayoutBox& child,
                                GridTrackSizingDirection direction,
                                LayoutSize size) {
  if (direction == kForColumns) {
    return !child.HasOverrideContainingBlockContentLogicalWidth() ||
           child.OverrideContainingBlockContentLogicalWidth() != size.Width();
  }
  return !child.HasOverrideContainingBlockContentLogicalHeight() ||
         child.OverrideContainingBlockContentLogicalHeight() != size.Height();
}

static bool HasRelativeBlockAxisSize(const LayoutGrid& grid,
                                     const LayoutBox& child) {
  return GridLayoutUtils::IsOrthogonalChild(grid, child)
             ? child.HasRelativeLogicalWidth() ||
                   child.StyleRef().LogicalWidth().IsAuto()
             : child.HasRelativeLogicalHeight();
}

void LayoutGrid::UpdateGridAreaLogicalSize(
    LayoutBox& child,
    LayoutSize grid_area_logical_size) const {
  // Because the grid area cannot be styled, we don't need to adjust
  // the grid breadth to account for 'box-sizing'.
  bool grid_area_width_changed =
      OverrideSizeChanged(child, kForColumns, grid_area_logical_size);
  bool grid_area_height_changed =
      OverrideSizeChanged(child, kForRows, grid_area_logical_size);
  if (grid_area_width_changed ||
      (grid_area_height_changed && HasRelativeBlockAxisSize(*this, child))) {
    child.SetSelfNeedsLayoutForAvailableSpace(true);
  }

  child.SetOverrideContainingBlockContentLogicalWidth(
      grid_area_logical_size.Width());
  child.SetOverrideContainingBlockContentLogicalHeight(
      grid_area_logical_size.Height());
}

void LayoutGrid::LayoutGridItems() {
  if (LayoutBlockedByDisplayLock(DisplayLockLifecycleTarget::kChildren))
    return;

  PopulateGridPositionsForDirection(kForColumns);
  PopulateGridPositionsForDirection(kForRows);

  for (LayoutBox* child = FirstChildBox(); child;
       child = child->NextSiblingBox()) {
    if (child->IsOutOfFlowPositioned()) {
      PrepareChildForPositionedLayout(*child);
      continue;
    }

    // Setting the definite grid area's sizes. It may imply that the
    // item must perform a layout if its area differs from the one
    // used during the track sizing algorithm.
    UpdateGridAreaLogicalSize(
        *child, LayoutSize(GridAreaBreadthForChildIncludingAlignmentOffsets(
                               *child, kForColumns),
                           GridAreaBreadthForChildIncludingAlignmentOffsets(
                               *child, kForRows)));

    // Stretching logic might force a child layout, so we need to run it before
    // the layoutIfNeeded call to avoid unnecessary relayouts. This might imply
    // that child margins, needed to correctly determine the available space
    // before stretching, are not set yet.
    ApplyStretchAlignmentToChildIfNeeded(*child);

    child->LayoutIfNeeded();

    // We need pending layouts to be done in order to compute auto-margins
    // properly.
    UpdateAutoMarginsInColumnAxisIfNeeded(*child);
    UpdateAutoMarginsInRowAxisIfNeeded(*child);

    const GridArea& area = grid_->GridItemArea(*child);
#if DCHECK_IS_ON()
    DCHECK_LT(area.columns.StartLine(),
              track_sizing_algorithm_.Tracks(kForColumns).size());
    DCHECK_LT(area.rows.StartLine(),
              track_sizing_algorithm_.Tracks(kForRows).size());
#endif
    SetLogicalPositionForChild(*child);

    // Keep track of children overflowing their grid area as we might need to
    // paint them even if the grid-area is not visible. Using physical
    // dimensions for simplicity, so we can forget about orthogonalty.
    LayoutUnit child_grid_area_height =
        child->OverrideContainingBlockContentHeight();
    LayoutUnit child_grid_area_width =
        child->OverrideContainingBlockContentWidth();
    LayoutRect grid_area_rect(
        GridAreaLogicalPosition(area),
        LayoutSize(child_grid_area_width, child_grid_area_height));
    LayoutRect child_overflow_rect = child->FrameRect();
    child_overflow_rect.SetSize(child->VisualOverflowRect().Size());
  }
}

void LayoutGrid::PrepareChildForPositionedLayout(LayoutBox& child) {
  DCHECK(child.IsOutOfFlowPositioned());
  child.ContainingBlock()->InsertPositionedObject(&child);

  PaintLayer* child_layer = child.Layer();
  // Static position of a positioned child should use the content-box
  // (https://drafts.csswg.org/css-grid/#static-position).
  child_layer->SetStaticInlinePosition(BorderAndPaddingStart());
  child_layer->SetStaticBlockPosition(BorderAndPaddingBefore());
}

bool LayoutGrid::HasStaticPositionForChild(
    const LayoutBox& child,
    GridTrackSizingDirection direction) const {
  return direction == kForColumns ? child.StyleRef().HasStaticInlinePosition(
                                        IsHorizontalWritingMode())
                                  : child.StyleRef().HasStaticBlockPosition(
                                        IsHorizontalWritingMode());
}

void LayoutGrid::LayoutPositionedObjects(bool relayout_children,
                                         PositionedLayoutBehavior info) {
  if (LayoutBlockedByDisplayLock(DisplayLockLifecycleTarget::kChildren))
    return;

  column_of_positioned_item_.clear();
  row_of_positioned_item_.clear();

  TrackedLayoutBoxListHashSet* positioned_descendants = PositionedObjects();
  if (!positioned_descendants)
    return;

  for (auto* child : *positioned_descendants) {
    LayoutUnit column_breadth =
        GridAreaBreadthForOutOfFlowChild(*child, kForColumns);
    LayoutUnit row_breadth = GridAreaBreadthForOutOfFlowChild(*child, kForRows);

    child->SetOverrideContainingBlockContentLogicalWidth(column_breadth);
    child->SetOverrideContainingBlockContentLogicalHeight(row_breadth);

    // Mark for layout as we're resetting the position before and we relay in
    // generic layout logic for positioned items in order to get the offsets
    // properly resolved.
    child->SetNeedsLayout(layout_invalidation_reason::kGridChanged,
                          kMarkOnlyThis);

    LayoutPositionedObject(child, relayout_children, info);

    SetLogicalOffsetForChild(*child, kForColumns);
    SetLogicalOffsetForChild(*child, kForRows);
  }
}

LayoutUnit LayoutGrid::GridAreaBreadthForChildIncludingAlignmentOffsets(
    const LayoutBox& child,
    GridTrackSizingDirection direction) const {
  // We need the cached value when available because Content Distribution
  // alignment properties may have some influence in the final grid area
  // breadth.
  const Vector<GridTrack>& tracks = track_sizing_algorithm_.Tracks(direction);
  const GridSpan& span =
      track_sizing_algorithm_.GetGrid().GridItemSpan(child, direction);
  const Vector<LayoutUnit>& line_positions =
      (direction == kForColumns) ? column_positions_ : row_positions_;
  LayoutUnit initial_track_position = line_positions[span.StartLine()];
  LayoutUnit final_track_position = line_positions[span.EndLine() - 1];
  // Track Positions vector stores the 'start' grid line of each track, so we
  // have to add last track's baseSize.
  return final_track_position - initial_track_position +
         tracks[span.EndLine() - 1].BaseSize();
}

void LayoutGrid::PopulateGridPositionsForDirection(
    GridTrackSizingDirection direction) {
  // Since we add alignment offsets and track gutters, grid lines are not always
  // adjacent. Hence we will have to assume from now on that we just store
  // positions of the initial grid lines of each track, except the last one,
  // which is the only one considered as a final grid line of a track.

  // The grid container's frame elements (border, padding and <content-position>
  // offset) are sensible to the inline-axis flow direction. However, column
  // lines positions are 'direction' unaware. This simplification allows us to
  // use the same indexes to identify the columns independently on the
  // inline-axis direction.
  bool is_row_axis = direction == kForColumns;
  auto& tracks = track_sizing_algorithm_.Tracks(direction);
  size_t number_of_tracks = tracks.size();
  size_t number_of_lines = number_of_tracks + 1;
  size_t last_line = number_of_lines - 1;
  bool has_collapsed_tracks = grid_->HasAutoRepeatEmptyTracks(direction);
  size_t number_of_collapsed_tracks =
      has_collapsed_tracks ? grid_->AutoRepeatEmptyTracks(direction)->size()
                           : 0;
  const auto& offset =
      direction == kForColumns ? offset_between_columns_ : offset_between_rows_;
  auto& positions = is_row_axis ? column_positions_ : row_positions_;
  positions.resize(number_of_lines);

  auto border_and_padding =
      is_row_axis ? BorderAndPaddingLogicalLeft() : BorderAndPaddingBefore();
  if (is_row_axis) {
    if (StyleRef().IsHorizontalWritingMode() &&
        !StyleRef().IsLeftToRightDirection())
      border_and_padding += ScrollbarLogicalWidth();
  } else {
    if (StyleRef().GetWritingMode() == WritingMode::kVerticalRl)
      border_and_padding += ScrollbarLogicalHeight();
  }

  positions[0] = border_and_padding + offset.position_offset;
  if (number_of_lines > 1) {
    // If we have collapsed tracks we just ignore gaps here and add them later
    // as we might not compute the gap between two consecutive tracks without
    // examining the surrounding ones.
    LayoutUnit gap = !has_collapsed_tracks ? GridGap(direction) : LayoutUnit();
    size_t next_to_last_line = number_of_lines - 2;
    for (size_t i = 0; i < next_to_last_line; ++i)
      positions[i + 1] = positions[i] + offset.distribution_offset +
                         tracks[i].BaseSize() + gap;
    positions[last_line] =
        positions[next_to_last_line] + tracks[next_to_last_line].BaseSize();

    // Adjust collapsed gaps. Collapsed tracks cause the surrounding gutters to
    // collapse (they coincide exactly) except on the edges of the grid where
    // they become 0.
    if (has_collapsed_tracks) {
      gap = GridGap(direction);
      size_t remaining_empty_tracks = number_of_collapsed_tracks;
      LayoutUnit offset_accumulator;
      LayoutUnit gap_accumulator;
      for (size_t i = 1; i < last_line; ++i) {
        if (grid_->IsEmptyAutoRepeatTrack(direction, i - 1)) {
          --remaining_empty_tracks;
          offset_accumulator += offset.distribution_offset;
        } else {
          // Add gap between consecutive non empty tracks. Add it also just once
          // for an arbitrary number of empty tracks between two non empty ones.
          bool all_remaining_tracks_are_empty =
              remaining_empty_tracks == (last_line - i);
          if (!all_remaining_tracks_are_empty ||
              !grid_->IsEmptyAutoRepeatTrack(direction, i))
            gap_accumulator += gap;
        }
        positions[i] += gap_accumulator - offset_accumulator;
      }
      positions[last_line] += gap_accumulator - offset_accumulator;
    }
  }
}

static LayoutUnit ComputeOverflowAlignmentOffset(OverflowAlignment overflow,
                                                 LayoutUnit track_size,
                                                 LayoutUnit child_size) {
  LayoutUnit offset = track_size - child_size;
  switch (overflow) {
    case OverflowAlignment::kSafe:
      // If overflow is 'safe', we have to make sure we don't overflow the
      // 'start' edge (potentially cause some data loss as the overflow is
      // unreachable).
      return offset.ClampNegativeToZero();
    case OverflowAlignment::kUnsafe:
    case OverflowAlignment::kDefault:
      // If we overflow our alignment container and overflow is 'true'
      // (default), we ignore the overflow and just return the value regardless
      // (which may cause data loss as we overflow the 'start' edge).
      return offset;
  }

  NOTREACHED();
  return LayoutUnit();
}

LayoutUnit LayoutGrid::AvailableAlignmentSpaceForChildBeforeStretching(
    LayoutUnit grid_area_breadth_for_child,
    const LayoutBox& child) const {
  // Because we want to avoid multiple layouts, stretching logic might be
  // performed before children are laid out, so we can't use the child cached
  // values. Hence, we may need to compute margins in order to determine the
  // available height before stretching.
  return grid_area_breadth_for_child -
         GridLayoutUtils::MarginLogicalHeightForChild(*this, child);
}

StyleSelfAlignmentData LayoutGrid::AlignSelfForChild(
    const LayoutBox& child,
    const ComputedStyle* style) const {
  if (!style)
    style = Style();
  return child.StyleRef().ResolvedAlignSelf(SelfAlignmentNormalBehavior(&child),
                                            style);
}

StyleSelfAlignmentData LayoutGrid::JustifySelfForChild(
    const LayoutBox& child,
    const ComputedStyle* style) const {
  if (!style)
    style = Style();
  return child.StyleRef().ResolvedJustifySelf(
      SelfAlignmentNormalBehavior(&child), style);
}

// FIXME: This logic is shared by LayoutFlexibleBox, so it should be moved to
// LayoutBox.
void LayoutGrid::ApplyStretchAlignmentToChildIfNeeded(LayoutBox& child) {
  GridTrackSizingDirection child_block_direction =
      GridLayoutUtils::FlowAwareDirectionForChild(*this, child, kForRows);
  bool block_flow_is_column_axis = child_block_direction == kForRows;
  bool allowed_to_stretch_child_block_size =
      block_flow_is_column_axis ? AllowedToStretchChildAlongColumnAxis(child)
                                : AllowedToStretchChildAlongRowAxis(child);
  if (allowed_to_stretch_child_block_size) {
    LayoutUnit stretched_logical_height =
        AvailableAlignmentSpaceForChildBeforeStretching(
            OverrideContainingBlockContentSizeForChild(child,
                                                       child_block_direction),
            child);
    LayoutUnit desired_logical_height = child.ConstrainLogicalHeightByMinMax(
        stretched_logical_height, LayoutUnit(-1));
    child.SetOverrideLogicalHeight(desired_logical_height);
    if (desired_logical_height != child.LogicalHeight()) {
      // TODO (lajava): Can avoid laying out here in some cases. See
      // https://webkit.org/b/87905.
      child.SetLogicalHeight(LayoutUnit());
      child.SetSelfNeedsLayoutForAvailableSpace(true);
    }
  }
}

// TODO(lajava): This logic is shared by LayoutFlexibleBox, so it should be
// moved to LayoutBox.
bool LayoutGrid::HasAutoMarginsInColumnAxis(const LayoutBox& child) const {
  if (IsHorizontalWritingMode())
    return child.StyleRef().MarginTop().IsAuto() ||
           child.StyleRef().MarginBottom().IsAuto();
  return child.StyleRef().MarginLeft().IsAuto() ||
         child.StyleRef().MarginRight().IsAuto();
}

// TODO(lajava): This logic is shared by LayoutFlexibleBox, so it should be
// moved to LayoutBox.
bool LayoutGrid::HasAutoMarginsInRowAxis(const LayoutBox& child) const {
  if (IsHorizontalWritingMode())
    return child.StyleRef().MarginLeft().IsAuto() ||
           child.StyleRef().MarginRight().IsAuto();
  return child.StyleRef().MarginTop().IsAuto() ||
         child.StyleRef().MarginBottom().IsAuto();
}

// TODO(lajava): This logic is shared by LayoutFlexibleBox, so it should be
// moved to LayoutBox.
DISABLE_CFI_PERF
void LayoutGrid::UpdateAutoMarginsInRowAxisIfNeeded(LayoutBox& child) {
  DCHECK(!child.IsOutOfFlowPositioned());

  const Length& margin_start = child.StyleRef().MarginStartUsing(StyleRef());
  const Length& margin_end = child.StyleRef().MarginEndUsing(StyleRef());
  LayoutUnit margin_logical_width;
  // We should only consider computed margins if their specified value isn't
  // 'auto', since such computed value may come from a previous layout and may
  // be incorrect now.
  if (!margin_start.IsAuto())
    margin_logical_width += child.MarginStart();
  if (!margin_end.IsAuto())
    margin_logical_width += child.MarginEnd();
  LayoutUnit available_alignment_space =
      child.OverrideContainingBlockContentLogicalWidth() -
      child.LogicalWidth() - margin_logical_width;
  if (available_alignment_space <= 0)
    return;

  if (margin_start.IsAuto() && margin_end.IsAuto()) {
    child.SetMarginStart(available_alignment_space / 2, Style());
    child.SetMarginEnd(available_alignment_space / 2, Style());
  } else if (margin_start.IsAuto()) {
    child.SetMarginStart(available_alignment_space, Style());
  } else if (margin_end.IsAuto()) {
    child.SetMarginEnd(available_alignment_space, Style());
  }
}

// TODO(lajava): This logic is shared by LayoutFlexibleBox, so it should be
// moved to LayoutBox.
DISABLE_CFI_PERF
void LayoutGrid::UpdateAutoMarginsInColumnAxisIfNeeded(LayoutBox& child) {
  DCHECK(!child.IsOutOfFlowPositioned());

  const Length& margin_before = child.StyleRef().MarginBeforeUsing(StyleRef());
  const Length& margin_after = child.StyleRef().MarginAfterUsing(StyleRef());
  LayoutUnit margin_logical_height;
  // We should only consider computed margins if their specified value isn't
  // 'auto', since such computed value may come from a previous layout and may
  // be incorrect now.
  if (!margin_before.IsAuto())
    margin_logical_height += child.MarginBefore();
  if (!margin_after.IsAuto())
    margin_logical_height += child.MarginAfter();
  LayoutUnit available_alignment_space =
      child.OverrideContainingBlockContentLogicalHeight() -
      child.LogicalHeight() - margin_logical_height;
  if (available_alignment_space <= 0)
    return;

  if (margin_before.IsAuto() && margin_after.IsAuto()) {
    child.SetMarginBefore(available_alignment_space / 2, Style());
    child.SetMarginAfter(available_alignment_space / 2, Style());
  } else if (margin_before.IsAuto()) {
    child.SetMarginBefore(available_alignment_space, Style());
  } else if (margin_after.IsAuto()) {
    child.SetMarginAfter(available_alignment_space, Style());
  }
}

// TODO(lajava): This logic is shared by LayoutFlexibleBox, so it might be
// refactored somehow.
LayoutUnit LayoutGrid::SynthesizedBaselineFromBorderBox(
    const LayoutBox& box,
    LineDirectionMode direction) {
  return direction == kHorizontalLine ? box.Size().Height()
                                      : box.Size().Width();
}

LayoutUnit LayoutGrid::BaselinePosition(FontBaseline,
                                        bool,
                                        LineDirectionMode direction,
                                        LinePositionMode mode) const {
  DCHECK_EQ(mode, kPositionOnContainingLine);
  LayoutUnit baseline = FirstLineBoxBaseline();
  // We take border-box's bottom if no valid baseline.
  if (baseline == -1) {
    return SynthesizedBaselineFromBorderBox(*this, direction) +
           MarginLogicalHeight();
  }

  return baseline + BeforeMarginInLineDirection(direction);
}

LayoutUnit LayoutGrid::FirstLineBoxBaseline() const {
  if (IsWritingModeRoot() || !grid_->HasGridItems() ||
      ShouldApplyLayoutContainment())
    return LayoutUnit(-1);
  const LayoutBox* baseline_child = nullptr;
  const LayoutBox* first_child = nullptr;
  bool is_baseline_aligned = false;
  // Finding the first grid item in grid order.
  for (size_t column = 0;
       !is_baseline_aligned && column < grid_->NumTracks(kForColumns);
       column++) {
    const GridItemList& cell = grid_->Cell(0, column);
    for (size_t index = 0; index < cell.size(); index++) {
      const LayoutBox* child = cell[index];
      DCHECK(!child->IsOutOfFlowPositioned());
      // If an item participates in baseline alignment, we select such item.
      if (IsBaselineAlignmentForChild(*child, kGridColumnAxis)) {
        // TODO (lajava): self-baseline and content-baseline alignment
        // still not implemented.
        baseline_child = child;
        is_baseline_aligned = true;
        break;
      }
      if (!baseline_child) {
        // Use dom order for items in the same cell.
        if (!first_child || (grid_->GridItemPaintOrder(*child) <
                             grid_->GridItemPaintOrder(*first_child)))
          first_child = child;
      }
    }
    if (!baseline_child && first_child)
      baseline_child = first_child;
  }

  if (!baseline_child)
    return LayoutUnit(-1);

  LayoutUnit baseline =
      GridLayoutUtils::IsOrthogonalChild(*this, *baseline_child)
          ? LayoutUnit(-1)
          : baseline_child->FirstLineBoxBaseline();
  // We take border-box's bottom if no valid baseline.
  if (baseline == -1) {
    // TODO (lajava): We should pass |direction| into
    // firstLineBoxBaseline and stop bailing out if we're a writing
    // mode root.  This would also fix some cases where the grid is
    // orthogonal to its container.
    LineDirectionMode direction =
        IsHorizontalWritingMode() ? kHorizontalLine : kVerticalLine;
    return SynthesizedBaselineFromBorderBox(*baseline_child, direction) +
           LogicalTopForChild(*baseline_child);
  }

  return baseline + baseline_child->LogicalTop();
}

LayoutUnit LayoutGrid::InlineBlockBaseline(LineDirectionMode direction) const {
  return FirstLineBoxBaseline();
}

bool LayoutGrid::IsBaselineAlignmentForChild(const LayoutBox& child) const {
  return IsBaselineAlignmentForChild(child, kGridRowAxis) ||
         IsBaselineAlignmentForChild(child, kGridColumnAxis);
}

bool LayoutGrid::IsBaselineAlignmentForChild(const LayoutBox& child,
                                             GridAxis baseline_axis) const {
  if (child.IsOutOfFlowPositioned())
    return false;
  ItemPosition align =
      SelfAlignmentForChild(baseline_axis, child).GetPosition();
  bool has_auto_margins = baseline_axis == kGridColumnAxis
                              ? HasAutoMarginsInColumnAxis(child)
                              : HasAutoMarginsInRowAxis(child);
  return IsBaselinePosition(align) && !has_auto_margins;
}

LayoutUnit LayoutGrid::ColumnAxisBaselineOffsetForChild(
    const LayoutBox& child) const {
  return track_sizing_algorithm_.BaselineOffsetForChild(child, kGridColumnAxis);
}

LayoutUnit LayoutGrid::RowAxisBaselineOffsetForChild(
    const LayoutBox& child) const {
  return track_sizing_algorithm_.BaselineOffsetForChild(child, kGridRowAxis);
}

GridAxisPosition LayoutGrid::ColumnAxisPositionForChild(
    const LayoutBox& child) const {
  bool has_same_writing_mode =
      child.StyleRef().GetWritingMode() == StyleRef().GetWritingMode();
  bool child_is_ltr = child.StyleRef().IsLeftToRightDirection();
  if (child.IsOutOfFlowPositioned() &&
      !HasStaticPositionForChild(child, kForRows))
    return kGridAxisStart;

  switch (AlignSelfForChild(child).GetPosition()) {
    case ItemPosition::kSelfStart:
      // TODO (lajava): Should we implement this logic in a generic utility
      // function?
      // Aligns the alignment subject to be flush with the edge of the alignment
      // container corresponding to the alignment subject's 'start' side in the
      // column axis.
      if (GridLayoutUtils::IsOrthogonalChild(*this, child)) {
        // If orthogonal writing-modes, self-start will be based on the child's
        // inline-axis direction (inline-start), because it's the one parallel
        // to the column axis.
        if (StyleRef().IsFlippedBlocksWritingMode())
          return child_is_ltr ? kGridAxisEnd : kGridAxisStart;
        return child_is_ltr ? kGridAxisStart : kGridAxisEnd;
      }
      // self-start is based on the child's block-flow direction. That's why we
      // need to check against the grid container's block-flow direction.
      return has_same_writing_mode ? kGridAxisStart : kGridAxisEnd;
    case ItemPosition::kSelfEnd:
      // TODO (lajava): Should we implement this logic in a generic utility
      // function?
      // Aligns the alignment subject to be flush with the edge of the alignment
      // container corresponding to the alignment subject's 'end' side in the
      // column axis.
      if (GridLayoutUtils::IsOrthogonalChild(*this, child)) {
        // If orthogonal writing-modes, self-end will be based on the child's
        // inline-axis direction, (inline-end) because it's the one parallel to
        // the column axis.
        if (StyleRef().IsFlippedBlocksWritingMode())
          return child_is_ltr ? kGridAxisStart : kGridAxisEnd;
        return child_is_ltr ? kGridAxisEnd : kGridAxisStart;
      }
      // self-end is based on the child's block-flow direction. That's why we
      // need to check against the grid container's block-flow direction.
      return has_same_writing_mode ? kGridAxisEnd : kGridAxisStart;
    case ItemPosition::kCenter:
      return kGridAxisCenter;
    // Only used in flex layout, otherwise equivalent to 'start'.
    case ItemPosition::kFlexStart:
    // Aligns the alignment subject to be flush with the alignment container's
    // 'start' edge (block-start) in the column axis.
    case ItemPosition::kStart:
      return kGridAxisStart;
    // Only used in flex layout, otherwise equivalent to 'end'.
    case ItemPosition::kFlexEnd:
    // Aligns the alignment subject to be flush with the alignment container's
    // 'end' edge (block-end) in the column axis.
    case ItemPosition::kEnd:
      return kGridAxisEnd;
    case ItemPosition::kStretch:
      return kGridAxisStart;
    case ItemPosition::kBaseline:
    case ItemPosition::kLastBaseline:
      return kGridAxisStart;
    case ItemPosition::kLegacy:
    case ItemPosition::kAuto:
    case ItemPosition::kNormal:
    case ItemPosition::kLeft:
    case ItemPosition::kRight:
      break;
  }

  NOTREACHED();
  return kGridAxisStart;
}

GridAxisPosition LayoutGrid::RowAxisPositionForChild(
    const LayoutBox& child) const {
  bool has_same_direction =
      child.StyleRef().Direction() == StyleRef().Direction();
  bool grid_is_ltr = StyleRef().IsLeftToRightDirection();
  if (child.IsOutOfFlowPositioned() &&
      !HasStaticPositionForChild(child, kForColumns))
    return kGridAxisStart;

  switch (JustifySelfForChild(child).GetPosition()) {
    case ItemPosition::kSelfStart:
      // TODO (lajava): Should we implement this logic in a generic utility
      // function?
      // Aligns the alignment subject to be flush with the edge of the alignment
      // container corresponding to the alignment subject's 'start' side in the
      // row axis.
      if (GridLayoutUtils::IsOrthogonalChild(*this, child)) {
        // If orthogonal writing-modes, self-start will be based on the child's
        // block-axis direction, because it's the one parallel to the row axis.
        if (child.StyleRef().IsFlippedBlocksWritingMode())
          return grid_is_ltr ? kGridAxisEnd : kGridAxisStart;
        return grid_is_ltr ? kGridAxisStart : kGridAxisEnd;
      }
      // self-start is based on the child's inline-flow direction. That's why we
      // need to check against the grid container's direction.
      return has_same_direction ? kGridAxisStart : kGridAxisEnd;
    case ItemPosition::kSelfEnd:
      // TODO (lajava): Should we implement this logic in a generic utility
      // function?
      // Aligns the alignment subject to be flush with the edge of the alignment
      // container corresponding to the alignment subject's 'end' side in the
      // row axis.
      if (GridLayoutUtils::IsOrthogonalChild(*this, child)) {
        // If orthogonal writing-modes, self-end will be based on the child's
        // block-axis direction, because it's the one parallel to the row axis.
        if (child.StyleRef().IsFlippedBlocksWritingMode())
          return grid_is_ltr ? kGridAxisStart : kGridAxisEnd;
        return grid_is_ltr ? kGridAxisEnd : kGridAxisStart;
      }
      // self-end is based on the child's inline-flow direction. That's why we
      // need to check against the grid container's direction.
      return has_same_direction ? kGridAxisEnd : kGridAxisStart;
    case ItemPosition::kLeft:
      // Aligns the alignment subject to be flush with the alignment container's
      // 'line-left' edge. We want the physical 'left' side, so we have to take
      // account, container's inline-flow direction.
      return grid_is_ltr ? kGridAxisStart : kGridAxisEnd;
    case ItemPosition::kRight:
      // Aligns the alignment subject to be flush with the alignment container's
      // 'line-right' edge. We want the physical 'right' side, so we have to
      // take account, container's inline-flow direction.
      return grid_is_ltr ? kGridAxisEnd : kGridAxisStart;
    case ItemPosition::kCenter:
      return kGridAxisCenter;
    // Only used in flex layout, otherwise equivalent to 'start'.
    case ItemPosition::kFlexStart:
    // Aligns the alignment subject to be flush with the alignment container's
    // 'start' edge (inline-start) in the row axis.
    case ItemPosition::kStart:
      return kGridAxisStart;
    // Only used in flex layout, otherwise equivalent to 'end'.
    case ItemPosition::kFlexEnd:
    // Aligns the alignment subject to be flush with the alignment container's
    // 'end' edge (inline-end) in the row axis.
    case ItemPosition::kEnd:
      return kGridAxisEnd;
    case ItemPosition::kStretch:
      return kGridAxisStart;
    case ItemPosition::kBaseline:
    case ItemPosition::kLastBaseline:
      return kGridAxisStart;
    case ItemPosition::kLegacy:
    case ItemPosition::kAuto:
    case ItemPosition::kNormal:
      break;
  }

  NOTREACHED();
  return kGridAxisStart;
}

LayoutUnit LayoutGrid::ColumnAxisOffsetForChild(const LayoutBox& child) const {
  LayoutUnit start_of_row;
  LayoutUnit end_of_row;
  GridAreaPositionForChild(child, kForRows, start_of_row, end_of_row);
  LayoutUnit start_position = start_of_row + MarginBeforeForChild(child);
  if (HasAutoMarginsInColumnAxis(child))
    return start_position;
  GridAxisPosition axis_position = ColumnAxisPositionForChild(child);
  switch (axis_position) {
    case kGridAxisStart:
      return start_position + ColumnAxisBaselineOffsetForChild(child);
    case kGridAxisEnd:
    case kGridAxisCenter: {
      LayoutUnit column_axis_child_size =
          GridLayoutUtils::IsOrthogonalChild(*this, child)
              ? child.LogicalWidth() + child.MarginLogicalWidth()
              : child.LogicalHeight() + child.MarginLogicalHeight();
      OverflowAlignment overflow = AlignSelfForChild(child).Overflow();
      LayoutUnit offset_from_start_position = ComputeOverflowAlignmentOffset(
          overflow, end_of_row - start_of_row, column_axis_child_size);
      return start_position + (axis_position == kGridAxisEnd
                                   ? offset_from_start_position
                                   : offset_from_start_position / 2);
    }
  }

  NOTREACHED();
  return LayoutUnit();
}

LayoutUnit LayoutGrid::RowAxisOffsetForChild(const LayoutBox& child) const {
  LayoutUnit start_of_column;
  LayoutUnit end_of_column;
  GridAreaPositionForChild(child, kForColumns, start_of_column, end_of_column);
  LayoutUnit start_position = start_of_column + MarginStartForChild(child);
  if (HasAutoMarginsInRowAxis(child))
    return start_position;
  GridAxisPosition axis_position = RowAxisPositionForChild(child);
  switch (axis_position) {
    case kGridAxisStart:
      return start_position + RowAxisBaselineOffsetForChild(child);
    case kGridAxisEnd:
    case kGridAxisCenter: {
      LayoutUnit row_axis_child_size =
          GridLayoutUtils::IsOrthogonalChild(*this, child)
              ? child.LogicalHeight() + child.MarginLogicalHeight()
              : child.LogicalWidth() + child.MarginLogicalWidth();
      OverflowAlignment overflow = JustifySelfForChild(child).Overflow();
      LayoutUnit offset_from_start_position = ComputeOverflowAlignmentOffset(
          overflow, end_of_column - start_of_column, row_axis_child_size);
      return start_position + (axis_position == kGridAxisEnd
                                   ? offset_from_start_position
                                   : offset_from_start_position / 2);
    }
  }

  NOTREACHED();
  return LayoutUnit();
}

LayoutUnit LayoutGrid::ResolveAutoStartGridPosition(
    GridTrackSizingDirection direction) const {
  if (direction == kForRows || StyleRef().IsLeftToRightDirection())
    return LayoutUnit();

  int last_line = NumTracks(kForColumns, *grid_);
  ContentPosition position = StyleRef().ResolvedJustifyContentPosition(
      ContentAlignmentNormalBehavior());
  if (position == ContentPosition::kEnd)
    return column_positions_[last_line] - ClientLogicalWidth();
  if (position == ContentPosition::kStart ||
      StyleRef().ResolvedJustifyContentDistribution(
          ContentAlignmentNormalBehavior()) ==
          ContentDistributionType::kStretch)
    return column_positions_[0] - BorderAndPaddingLogicalLeft();
  return LayoutUnit();
}

LayoutUnit LayoutGrid::ResolveAutoEndGridPosition(
    GridTrackSizingDirection direction) const {
  if (direction == kForRows)
    return ClientLogicalHeight();
  if (StyleRef().IsLeftToRightDirection())
    return ClientLogicalWidth();

  int last_line = NumTracks(kForColumns, *grid_);
  ContentPosition position = StyleRef().ResolvedJustifyContentPosition(
      ContentAlignmentNormalBehavior());
  if (position == ContentPosition::kEnd)
    return column_positions_[last_line];
  if (position == ContentPosition::kStart ||
      StyleRef().ResolvedJustifyContentDistribution(
          ContentAlignmentNormalBehavior()) ==
          ContentDistributionType::kStretch) {
    return column_positions_[0] - BorderAndPaddingLogicalLeft() +
           ClientLogicalWidth();
  }
  return ClientLogicalWidth();
}

LayoutUnit LayoutGrid::GridAreaBreadthForOutOfFlowChild(
    const LayoutBox& child,
    GridTrackSizingDirection direction) {
  DCHECK(child.IsOutOfFlowPositioned());
  bool is_row_axis = direction == kForColumns;
  GridSpan span = GridPositionsResolver::ResolveGridPositionsFromStyle(
      *Style(), child, direction, AutoRepeatCountForDirection(direction));
  if (span.IsIndefinite())
    return is_row_axis ? ClientLogicalWidth() : ClientLogicalHeight();

  int smallest_start = abs(grid_->SmallestTrackStart(direction));
  int start_line = span.UntranslatedStartLine() + smallest_start;
  int end_line = span.UntranslatedEndLine() + smallest_start;
  int last_line = NumTracks(direction, *grid_);
  GridPosition start_position = direction == kForColumns
                                    ? child.StyleRef().GridColumnStart()
                                    : child.StyleRef().GridRowStart();
  GridPosition end_position = direction == kForColumns
                                  ? child.StyleRef().GridColumnEnd()
                                  : child.StyleRef().GridRowEnd();

  bool start_is_auto =
      start_position.IsAuto() || start_line < 0 || start_line > last_line;
  bool end_is_auto =
      end_position.IsAuto() || end_line < 0 || end_line > last_line;

  if (start_is_auto && end_is_auto)
    return is_row_axis ? ClientLogicalWidth() : ClientLogicalHeight();

  LayoutUnit start;
  LayoutUnit end;
  auto& positions = is_row_axis ? column_positions_ : row_positions_;
  auto& line_of_positioned_item =
      is_row_axis ? column_of_positioned_item_ : row_of_positioned_item_;
  LayoutUnit border_edge = is_row_axis ? BorderLogicalLeft() : BorderBefore();
  if (start_is_auto) {
    start = ResolveAutoStartGridPosition(direction) + border_edge;
  } else {
    line_of_positioned_item.Set(&child, start_line);
    start = positions[start_line];
  }
  if (end_is_auto) {
    end = ResolveAutoEndGridPosition(direction) + border_edge;
  } else {
    end = positions[end_line];
    // These vectors store line positions including gaps, but we shouldn't
    // consider them for the edges of the grid.
    if (end_line > 0 && end_line < last_line) {
      DCHECK(!grid_->NeedsItemsPlacement());
      // TODO(rego): It would be more efficient to call GridGap(direction) and
      // pass that value to GuttersSize(), so we could avoid the call to
      // available size if the gutter doesn't use percentages.
      end -= GuttersSize(
          *grid_, direction, end_line - 1, 2,
          is_row_axis ? AvailableLogicalWidth() : ContentLogicalHeight());
      end -= is_row_axis ? offset_between_columns_.distribution_offset
                         : offset_between_rows_.distribution_offset;
    }
  }
  // TODO (lajava): Is expectable that in some cases 'end' is smaller than
  // 'start' ?
  return std::max(end - start, LayoutUnit());
}

LayoutUnit LayoutGrid::LogicalOffsetForOutOfFlowChild(
    const LayoutBox& child,
    GridTrackSizingDirection direction,
    LayoutUnit track_breadth) const {
  DCHECK(child.IsOutOfFlowPositioned());
  if (HasStaticPositionForChild(child, direction))
    return LayoutUnit();

  bool is_row_axis = direction == kForColumns;
  bool is_flowaware_row_axis = GridLayoutUtils::FlowAwareDirectionForChild(
                                   *this, child, direction) == kForColumns;
  LayoutUnit child_position =
      is_flowaware_row_axis ? child.LogicalLeft() : child.LogicalTop();
  LayoutUnit grid_border = is_row_axis ? BorderLogicalLeft() : BorderBefore();
  LayoutUnit child_margin =
      is_flowaware_row_axis ? child.MarginLineLeft() : child.MarginBefore();
  LayoutUnit offset = child_position - grid_border - child_margin;
  if (!is_row_axis || StyleRef().IsLeftToRightDirection())
    return offset;

  LayoutUnit child_breadth =
      is_flowaware_row_axis
          ? child.LogicalWidth() + child.MarginLogicalWidth()
          : child.LogicalHeight() + child.MarginLogicalHeight();
  return track_breadth - offset - child_breadth;
}

void LayoutGrid::GridAreaPositionForOutOfFlowChild(
    const LayoutBox& child,
    GridTrackSizingDirection direction,
    LayoutUnit& start,
    LayoutUnit& end) const {
  DCHECK(child.IsOutOfFlowPositioned());
  DCHECK(GridLayoutUtils::HasOverrideContainingBlockContentSizeForChild(
      child, direction));
  LayoutUnit track_breadth =
      GridLayoutUtils::OverrideContainingBlockContentSizeForChild(child,
                                                                  direction);
  bool is_row_axis = direction == kForColumns;
  auto& line_of_positioned_item =
      is_row_axis ? column_of_positioned_item_ : row_of_positioned_item_;
  start = is_row_axis ? BorderLogicalLeft() : BorderBefore();
  if (base::Optional<size_t> line = line_of_positioned_item.at(&child)) {
    auto& positions = is_row_axis ? column_positions_ : row_positions_;
    start = positions[line.value()];
  }
  start += LogicalOffsetForOutOfFlowChild(child, direction, track_breadth);
  end = start + track_breadth;
}

void LayoutGrid::GridAreaPositionForInFlowChild(
    const LayoutBox& child,
    GridTrackSizingDirection direction,
    LayoutUnit& start,
    LayoutUnit& end) const {
  DCHECK(!child.IsOutOfFlowPositioned());
  const Grid& grid = track_sizing_algorithm_.GetGrid();
  const GridSpan& span = grid.GridItemSpan(child, direction);
  // TODO (lajava): This is a common pattern, why not defining a function like
  // positions(direction) ?
  auto& positions =
      direction == kForColumns ? column_positions_ : row_positions_;
  start = positions[span.StartLine()];
  end = positions[span.EndLine()];
  // The 'positions' vector includes distribution offset (because of content
  // alignment) and gutters so we need to subtract them to get the actual
  // end position for a given track (this does not have to be done for the
  // last track as there are no more positions's elements after it, nor for
  // collapsed tracks).
  if (span.EndLine() < positions.size() - 1 &&
      !(grid.HasAutoRepeatEmptyTracks(direction) &&
        grid.IsEmptyAutoRepeatTrack(direction, span.EndLine())))
    end -= GridGap(direction) + GridItemOffset(direction);
}

void LayoutGrid::GridAreaPositionForChild(const LayoutBox& child,
                                          GridTrackSizingDirection direction,
                                          LayoutUnit& start,
                                          LayoutUnit& end) const {
  if (child.IsOutOfFlowPositioned())
    GridAreaPositionForOutOfFlowChild(child, direction, start, end);
  else
    GridAreaPositionForInFlowChild(child, direction, start, end);
}

ContentPosition static ResolveContentDistributionFallback(
    ContentDistributionType distribution) {
  switch (distribution) {
    case ContentDistributionType::kSpaceBetween:
      return ContentPosition::kStart;
    case ContentDistributionType::kSpaceAround:
      return ContentPosition::kCenter;
    case ContentDistributionType::kSpaceEvenly:
      return ContentPosition::kCenter;
    case ContentDistributionType::kStretch:
      return ContentPosition::kStart;
    case ContentDistributionType::kDefault:
      return ContentPosition::kNormal;
  }

  NOTREACHED();
  return ContentPosition::kNormal;
}

static void ComputeContentDistributionOffset(
    ContentAlignmentData& offset,
    const LayoutUnit& available_free_space,
    ContentPosition& fallback_position,
    ContentDistributionType distribution,
    unsigned number_of_grid_tracks) {
  if (distribution != ContentDistributionType::kDefault &&
      fallback_position == ContentPosition::kNormal)
    fallback_position = ResolveContentDistributionFallback(distribution);

  // Initialize to an invalid offset.
  offset.position_offset = LayoutUnit(-1);
  offset.distribution_offset = LayoutUnit(-1);
  if (available_free_space <= 0)
    return;

  LayoutUnit position_offset;
  LayoutUnit distribution_offset;
  switch (distribution) {
    case ContentDistributionType::kSpaceBetween:
      if (number_of_grid_tracks < 2)
        return;
      distribution_offset = available_free_space / (number_of_grid_tracks - 1);
      position_offset = LayoutUnit();
      break;
    case ContentDistributionType::kSpaceAround:
      if (number_of_grid_tracks < 1)
        return;
      distribution_offset = available_free_space / number_of_grid_tracks;
      position_offset = distribution_offset / 2;
      break;
    case ContentDistributionType::kSpaceEvenly:
      distribution_offset = available_free_space / (number_of_grid_tracks + 1);
      position_offset = distribution_offset;
      break;
    case ContentDistributionType::kStretch:
    case ContentDistributionType::kDefault:
      return;
    default:
      NOTREACHED();
      return;
  }

  offset.position_offset = position_offset;
  offset.distribution_offset = distribution_offset;
}

StyleContentAlignmentData LayoutGrid::ContentAlignment(
    GridTrackSizingDirection direction) const {
  return direction == kForColumns ? StyleRef().ResolvedJustifyContent(
                                        ContentAlignmentNormalBehavior())
                                  : StyleRef().ResolvedAlignContent(
                                        ContentAlignmentNormalBehavior());
}

void LayoutGrid::ComputeContentPositionAndDistributionOffset(
    GridTrackSizingDirection direction,
    const LayoutUnit& available_free_space,
    unsigned number_of_grid_tracks) {
  auto& offset =
      direction == kForColumns ? offset_between_columns_ : offset_between_rows_;
  StyleContentAlignmentData content_alignment_data =
      ContentAlignment(direction);
  ContentPosition position = content_alignment_data.GetPosition();
  // If <content-distribution> value can't be applied, 'position' will become
  // the associated <content-position> fallback value.
  ComputeContentDistributionOffset(offset, available_free_space, position,
                                   content_alignment_data.Distribution(),
                                   number_of_grid_tracks);
  if (offset.IsValid())
    return;

  // TODO (lajava): Default value for overflow isn't exaclty as 'unsafe'.
  // https://drafts.csswg.org/css-align/#overflow-values
  if (available_free_space == 0 ||
      (available_free_space < 0 &&
       content_alignment_data.Overflow() == OverflowAlignment::kSafe)) {
    offset.position_offset = LayoutUnit();
    offset.distribution_offset = LayoutUnit();
    return;
  }

  LayoutUnit position_offset;
  bool is_row_axis = direction == kForColumns;
  switch (position) {
    case ContentPosition::kLeft:
      DCHECK(is_row_axis);
      position_offset = LayoutUnit();
      break;
    case ContentPosition::kRight:
      DCHECK(is_row_axis);
      position_offset = available_free_space;
      break;
    case ContentPosition::kCenter:
      position_offset = available_free_space / 2;
      break;
    // Only used in flex layout, for other layout, it's equivalent to 'End'.
    case ContentPosition::kFlexEnd:
      U_FALLTHROUGH;
    case ContentPosition::kEnd:
      if (is_row_axis) {
        position_offset = StyleRef().IsLeftToRightDirection()
                              ? available_free_space
                              : LayoutUnit();
      } else {
        position_offset = available_free_space;
      }
      break;
    // Only used in flex layout, for other layout, it's equivalent to 'Start'.
    case ContentPosition::kFlexStart:
      U_FALLTHROUGH;
    case ContentPosition::kStart:
      if (is_row_axis) {
        position_offset = StyleRef().IsLeftToRightDirection()
                              ? LayoutUnit()
                              : available_free_space;
      } else {
        position_offset = LayoutUnit();
      }
      break;
    case ContentPosition::kBaseline:
      U_FALLTHROUGH;
    case ContentPosition::kLastBaseline:
      // FIXME: These two require implementing Baseline Alignment. For now, we
      // always 'start' align the child. crbug.com/234191
      if (is_row_axis) {
        position_offset = StyleRef().IsLeftToRightDirection()
                              ? LayoutUnit()
                              : available_free_space;
      } else {
        position_offset = LayoutUnit();
      }
      break;
    case ContentPosition::kNormal:
      U_FALLTHROUGH;
    default:
      NOTREACHED();
      return;
  }

  offset.position_offset = position_offset;
  offset.distribution_offset = LayoutUnit();
}

LayoutUnit LayoutGrid::TranslateOutOfFlowRTLCoordinate(
    const LayoutBox& child,
    LayoutUnit coordinate) const {
  DCHECK(child.IsOutOfFlowPositioned());
  DCHECK(!StyleRef().IsLeftToRightDirection());

  if (column_of_positioned_item_.at(&child))
    return TranslateRTLCoordinate(coordinate);

  return BorderLogicalLeft() + BorderLogicalRight() + ClientLogicalWidth() -
         coordinate;
}

LayoutUnit LayoutGrid::TranslateRTLCoordinate(LayoutUnit coordinate) const {
  DCHECK(!StyleRef().IsLeftToRightDirection());

  LayoutUnit alignment_offset = column_positions_[0];
  LayoutUnit right_grid_edge_position =
      column_positions_[column_positions_.size() - 1];
  return right_grid_edge_position + alignment_offset - coordinate;
}

// TODO: SetLogicalPositionForChild has only one caller, consider its
// refactoring in the future.
void LayoutGrid::SetLogicalPositionForChild(LayoutBox& child) const {
  // "In the positioning phase [...] calculations are performed according to the
  // writing mode of the containing block of the box establishing the orthogonal
  // flow." However, 'setLogicalPosition' will only take into account the
  // child's writing-mode, so the position may need to be transposed.
  LayoutPoint child_location(LogicalOffsetForChild(child, kForColumns),
                             LogicalOffsetForChild(child, kForRows));
  child.SetLogicalLocation(GridLayoutUtils::IsOrthogonalChild(*this, child)
                               ? child_location.TransposedPoint()
                               : child_location);
}

void LayoutGrid::SetLogicalOffsetForChild(
    LayoutBox& child,
    GridTrackSizingDirection direction) const {
  if (!child.IsGridItem() && HasStaticPositionForChild(child, direction))
    return;
  // 'SetLogicalLeft' and 'SetLogicalTop' only take into account the child's
  // writing-mode, that's why 'FlowAwareDirectionForChild' is needed.
  if (GridLayoutUtils::FlowAwareDirectionForChild(*this, child, direction) ==
      kForColumns)
    child.SetLogicalLeft(LogicalOffsetForChild(child, direction));
  else
    child.SetLogicalTop(LogicalOffsetForChild(child, direction));
}

LayoutUnit LayoutGrid::LogicalOffsetForChild(
    const LayoutBox& child,
    GridTrackSizingDirection direction) const {
  if (direction == kForRows) {
    return ColumnAxisOffsetForChild(child);
  }
  LayoutUnit row_axis_offset = RowAxisOffsetForChild(child);
  // We stored column_position_'s data ignoring the direction, hence we might
  // need now to translate positions from RTL to LTR, as it's more convenient
  // for painting.
  if (!StyleRef().IsLeftToRightDirection()) {
    row_axis_offset =
        (child.IsOutOfFlowPositioned()
             ? TranslateOutOfFlowRTLCoordinate(child, row_axis_offset)
             : TranslateRTLCoordinate(row_axis_offset)) -
        (GridLayoutUtils::IsOrthogonalChild(*this, child)
             ? child.LogicalHeight()
             : child.LogicalWidth());
  }
  return row_axis_offset;
}

LayoutPoint LayoutGrid::GridAreaLogicalPosition(const GridArea& area) const {
  LayoutUnit column_axis_offset = row_positions_[area.rows.StartLine()];
  LayoutUnit row_axis_offset = column_positions_[area.columns.StartLine()];

  // See comment in findChildLogicalPosition() about why we need sometimes to
  // translate from RTL to LTR the rowAxisOffset coordinate.
  return LayoutPoint(StyleRef().IsLeftToRightDirection()
                         ? row_axis_offset
                         : TranslateRTLCoordinate(row_axis_offset),
                     column_axis_offset);
}

void LayoutGrid::PaintChildren(const PaintInfo& paint_info,
                               const PhysicalOffset& paint_offset) const {
  DCHECK(!grid_->NeedsItemsPlacement());
  if (grid_->HasGridItems()) {
    BlockPainter(*this).PaintChildrenAtomically(grid_->GetOrderIterator(),
                                                paint_info);
  }
}

bool LayoutGrid::CachedHasDefiniteLogicalHeight() const {
  SECURITY_DCHECK(has_definite_logical_height_);
  return has_definite_logical_height_.value();
}

size_t LayoutGrid::NonCollapsedTracks(
    GridTrackSizingDirection direction) const {
  auto& tracks = track_sizing_algorithm_.Tracks(direction);
  size_t number_of_tracks = tracks.size();
  bool has_collapsed_tracks = grid_->HasAutoRepeatEmptyTracks(direction);
  size_t number_of_collapsed_tracks =
      has_collapsed_tracks ? grid_->AutoRepeatEmptyTracks(direction)->size()
                           : 0;
  return number_of_tracks - number_of_collapsed_tracks;
}

size_t LayoutGrid::NumTracks(GridTrackSizingDirection direction,
                             const Grid& grid) const {
  // Due to limitations in our internal representation, we cannot know the
  // number of columns from m_grid *if* there is no row (because m_grid would be
  // empty). That's why in that case we need to get it from the style. Note that
  // we know for sure that there are't any implicit tracks, because not having
  // rows implies that there are no "normal" children (out-of-flow children are
  // not stored in m_grid).
  DCHECK(!grid.NeedsItemsPlacement());
  if (direction == kForRows)
    return grid.NumTracks(kForRows);

  return grid.NumTracks(kForRows)
             ? grid.NumTracks(kForColumns)
             : GridPositionsResolver::ExplicitGridColumnCount(
                   StyleRef(), grid.AutoRepeatTracks(kForColumns));
}

LayoutUnit LayoutGrid::GridItemOffset(
    GridTrackSizingDirection direction) const {
  return direction == kForRows ? offset_between_rows_.distribution_offset
                               : offset_between_columns_.distribution_offset;
}

}  // namespace blink
