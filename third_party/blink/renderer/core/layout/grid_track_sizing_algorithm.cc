// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid_track_sizing_algorithm.h"

#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/grid.h"
#include "third_party/blink/renderer/core/layout/grid_layout_utils.h"
#include "third_party/blink/renderer/core/layout/layout_grid.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

class GridSizingData;

LayoutUnit GridTrack::BaseSize() const {
  DCHECK(IsGrowthLimitBiggerThanBaseSize());
  return base_size_;
}

LayoutUnit GridTrack::GrowthLimit() const {
  DCHECK(IsGrowthLimitBiggerThanBaseSize());
  DCHECK(!growth_limit_cap_ || growth_limit_cap_.value() >= growth_limit_ ||
         base_size_ >= growth_limit_cap_.value());
  return growth_limit_;
}

void GridTrack::SetBaseSize(LayoutUnit base_size) {
  base_size_ = base_size;
  EnsureGrowthLimitIsBiggerThanBaseSize();
}

void GridTrack::SetGrowthLimit(LayoutUnit growth_limit) {
  growth_limit_ =
      growth_limit == kInfinity
          ? growth_limit
          : std::min(growth_limit, growth_limit_cap_.value_or(growth_limit));
  EnsureGrowthLimitIsBiggerThanBaseSize();
}

bool GridTrack::InfiniteGrowthPotential() const {
  return GrowthLimitIsInfinite() || infinitely_growable_;
}

void GridTrack::SetPlannedSize(LayoutUnit planned_size) {
  DCHECK(planned_size >= 0 || planned_size == kInfinity);
  planned_size_ = planned_size;
}

void GridTrack::SetSizeDuringDistribution(LayoutUnit size_during_distribution) {
  DCHECK_GE(size_during_distribution, 0);
  DCHECK(GrowthLimitIsInfinite() || GrowthLimit() >= size_during_distribution);
  size_during_distribution_ = size_during_distribution;
}

void GridTrack::GrowSizeDuringDistribution(
    LayoutUnit size_during_distribution) {
  DCHECK_GE(size_during_distribution, 0);
  size_during_distribution_ += size_during_distribution;
}

void GridTrack::SetInfinitelyGrowable(bool infinitely_growable) {
  infinitely_growable_ = infinitely_growable;
}

void GridTrack::SetGrowthLimitCap(absl::optional<LayoutUnit> growth_limit_cap) {
  DCHECK(!growth_limit_cap || *growth_limit_cap >= 0);
  growth_limit_cap_ = growth_limit_cap;
}

void GridTrack::SetCachedTrackSize(const GridTrackSize& cached_track_size) {
  cached_track_size_ = cached_track_size;
}

bool GridTrack::IsGrowthLimitBiggerThanBaseSize() const {
  return GrowthLimitIsInfinite() || growth_limit_ >= base_size_;
}

void GridTrack::EnsureGrowthLimitIsBiggerThanBaseSize() {
  if (growth_limit_ != kInfinity && growth_limit_ < base_size_)
    growth_limit_ = base_size_;
}

static GridAxis GridAxisForDirection(GridTrackSizingDirection direction) {
  return direction == kForColumns ? kGridRowAxis : kGridColumnAxis;
}

static GridTrackSizingDirection GridDirectionForAxis(GridAxis axis) {
  return axis == kGridRowAxis ? kForColumns : kForRows;
}

template <typename F>
static void IterateGridItemsInTrackIndices(const Grid& grid,
                                           GridTrackSizingDirection direction,
                                           Vector<wtf_size_t>& track_indices,
                                           F callback) {
#if DCHECK_IS_ON()
  HeapHashSet<Member<LayoutBox>> items_set;
#endif
  for (wtf_size_t i = 0; i < track_indices.size(); ++i) {
    auto* iterator = grid.CreateIterator(direction, track_indices[i]);
    while (LayoutBox* grid_item = iterator->NextGridItem()) {
      const GridSpan& span = grid.GridItemSpan(*grid_item, direction);
      if (i > 0) {
        // Skip items already processed in an earlier track.
        DCHECK_LT(track_indices[i - 1], track_indices[i]);
        if (span.StartLine() <= track_indices[i - 1])
          continue;
      }
#if DCHECK_IS_ON()
      DCHECK(items_set.insert(grid_item).is_new_entry);
#endif
      callback(grid_item, span);
    }
  }
}

class IndefiniteSizeStrategy final : public GridTrackSizingAlgorithmStrategy {
 public:
  IndefiniteSizeStrategy(GridTrackSizingAlgorithm& algorithm)
      : GridTrackSizingAlgorithmStrategy(algorithm) {}

 private:
  void LayoutGridItemForMinSizeComputation(
      LayoutBox&,
      bool override_size_has_changed) const override;
  void MaximizeTracks(Vector<GridTrack>&,
                      absl::optional<LayoutUnit>& free_space) override;
  double FindUsedFlexFraction(
      Vector<wtf_size_t>& flexible_sized_tracks_index,
      GridTrackSizingDirection,
      absl::optional<LayoutUnit> free_space) const override;
  bool RecomputeUsedFlexFractionIfNeeded(
      double& flex_fraction,
      Vector<LayoutUnit>& increments,
      LayoutUnit& total_growth) const override;
  LayoutUnit FreeSpaceForStretchAutoTracksStep() const override;
  LayoutUnit MinContentForChild(LayoutBox&) const override;
  LayoutUnit MaxContentForChild(LayoutBox&) const override;
  bool IsComputingSizeContainment() const override;
};

class DefiniteSizeStrategy final : public GridTrackSizingAlgorithmStrategy {
 public:
  DefiniteSizeStrategy(GridTrackSizingAlgorithm& algorithm)
      : GridTrackSizingAlgorithmStrategy(algorithm) {}

 private:
  void LayoutGridItemForMinSizeComputation(
      LayoutBox&,
      bool override_size_has_changed) const override;
  void MaximizeTracks(Vector<GridTrack>&,
                      absl::optional<LayoutUnit>& free_space) override;
  double FindUsedFlexFraction(
      Vector<wtf_size_t>& flexible_sized_tracks_index,
      GridTrackSizingDirection,
      absl::optional<LayoutUnit> free_space) const override;
  bool RecomputeUsedFlexFractionIfNeeded(
      double& flex_fraction,
      Vector<LayoutUnit>& increments,
      LayoutUnit& total_growth) const override {
    return false;
  }
  LayoutUnit FreeSpaceForStretchAutoTracksStep() const override;
  LayoutUnit MinContentForChild(LayoutBox&) const override;
  LayoutUnit MinLogicalSizeForChild(LayoutBox&,
                                    const Length& child_min_size,
                                    LayoutUnit available_size) const override;
  bool IsComputingSizeContainment() const override { return false; }
};

GridTrackSizingAlgorithmStrategy::~GridTrackSizingAlgorithmStrategy() = default;

bool GridTrackSizingAlgorithmStrategy::HasRelativeMarginOrPaddingForChild(
    const LayoutGrid& grid,
    const LayoutBox& child,
    GridTrackSizingDirection direction) {
  GridTrackSizingDirection child_inline_direction =
      GridLayoutUtils::FlowAwareDirectionForChild(grid, child, kForColumns);
  if (direction == child_inline_direction) {
    return child.StyleRef().MarginStart().IsPercentOrCalc() ||
           child.StyleRef().MarginEnd().IsPercentOrCalc() ||
           child.StyleRef().PaddingStart().IsPercentOrCalc() ||
           child.StyleRef().PaddingEnd().IsPercentOrCalc();
  }
  return child.StyleRef().MarginBefore().IsPercentOrCalc() ||
         child.StyleRef().MarginAfter().IsPercentOrCalc() ||
         child.StyleRef().PaddingBefore().IsPercentOrCalc() ||
         child.StyleRef().PaddingAfter().IsPercentOrCalc();
}

bool GridTrackSizingAlgorithmStrategy::HasRelativeOrIntrinsicSizeForChild(
    const LayoutGrid& grid,
    const LayoutBox& child,
    GridTrackSizingDirection direction) {
  GridTrackSizingDirection child_inline_direction =
      GridLayoutUtils::FlowAwareDirectionForChild(grid, child, kForColumns);
  if (direction == child_inline_direction) {
    return child.HasRelativeLogicalWidth() ||
           !child.StyleRef().LogicalWidth().IsSpecified();
  }
  return child.HasRelativeLogicalHeight() ||
         !child.StyleRef().LogicalHeight().IsSpecified();
}

bool GridTrackSizingAlgorithmStrategy::
    ShouldClearOverrideContainingBlockContentSizeForChild(
        const LayoutGrid& grid,
        const LayoutBox& child,
        GridTrackSizingDirection direction) {
  return HasRelativeOrIntrinsicSizeForChild(grid, child, direction) ||
         HasRelativeMarginOrPaddingForChild(grid, child, direction);
}

void GridTrackSizingAlgorithmStrategy::
    SetOverrideContainingBlockContentSizeForChild(
        LayoutBox& child,
        GridTrackSizingDirection direction,
        LayoutUnit size) {
  if (direction == kForColumns)
    child.SetOverrideContainingBlockContentLogicalWidth(size);
  else
    child.SetOverrideContainingBlockContentLogicalHeight(size);
}

LayoutSize GridTrackSizingAlgorithm::EstimatedGridAreaBreadthForChild(
    const LayoutBox& child) const {
  return {EstimatedGridAreaBreadthForChild(child, kForColumns),
          EstimatedGridAreaBreadthForChild(child, kForRows)};
}

LayoutUnit GridTrackSizingAlgorithm::EstimatedGridAreaBreadthForChild(
    const LayoutBox& child,
    GridTrackSizingDirection direction) const {
  const GridSpan& span = grid_->GridItemSpan(child, direction);
  LayoutUnit grid_area_size;
  bool grid_area_is_indefinite = false;
  absl::optional<LayoutUnit> available_size = AvailableSpace(direction);
  for (auto track_position : span) {
    // We may need to estimate the grid area size before running the track
    // sizing algorithm in order to perform the pre-layout of orthogonal
    // items.
    // We cannot use Tracks(direction)[track_position].CachedTrackSize()
    // because Tracks(direction) is empty, since we are either performing
    // pre-layout or are running the track sizing algorithm in the opposite
    // direction and haven't run it in the desired direction yet.
    const GridTrackSize& track_size =
        WasSetup() ? CalculateGridTrackSize(direction, track_position)
                   : RawGridTrackSize(direction, track_position);
    GridLength max_track_size = track_size.MaxTrackBreadth();
    if (max_track_size.IsContentSized() || max_track_size.IsFlex() ||
        IsRelativeGridLengthAsAuto(max_track_size, direction)) {
      grid_area_is_indefinite = true;
    } else {
      grid_area_size += ValueForLength(max_track_size.length(),
                                       available_size.value_or(LayoutUnit()));
    }
  }

  grid_area_size += layout_grid_->GuttersSize(
      *grid_, direction, span.StartLine(), span.IntegerSpan(), available_size);

  GridTrackSizingDirection child_inline_direction =
      GridLayoutUtils::FlowAwareDirectionForChild(*layout_grid_, child,
                                                  kForColumns);
  if (grid_area_is_indefinite) {
    return direction == child_inline_direction
               ? std::max(child.PreferredLogicalWidths().max_size,
                          grid_area_size)
               : LayoutUnit(-1);
  }
  return grid_area_size;
}

LayoutUnit GridTrackSizingAlgorithm::GridAreaBreadthForChild(
    const LayoutBox& child,
    GridTrackSizingDirection direction) const {
  bool add_content_alignment_offset =
      direction == kForColumns && sizing_state_ == kRowSizingFirstIteration;
  if (direction == kForRows &&
      (sizing_state_ == kColumnSizingFirstIteration ||
       sizing_state_ == kColumnSizingSecondIteration)) {
    DCHECK(GridLayoutUtils::IsOrthogonalChild(*layout_grid_, child));
    // TODO (jfernandez) Content Alignment should account for this heuristic
    // https://github.com/w3c/csswg-drafts/issues/2697
    if (sizing_state_ == kColumnSizingFirstIteration)
      return EstimatedGridAreaBreadthForChild(child, kForRows);
    add_content_alignment_offset = true;
  }

  const Vector<GridTrack>& all_tracks = Tracks(direction);
  const GridSpan& span = grid_->GridItemSpan(child, direction);
  LayoutUnit grid_area_breadth;
  for (auto track_position : span)
    grid_area_breadth += all_tracks[track_position].BaseSize();

  if (add_content_alignment_offset) {
    grid_area_breadth +=
        (span.IntegerSpan() - 1) * layout_grid_->GridItemOffset(direction);
  }

  grid_area_breadth +=
      layout_grid_->GuttersSize(*grid_, direction, span.StartLine(),
                                span.IntegerSpan(), AvailableSpace(direction));

  return grid_area_breadth;
}

bool GridTrackSizingAlgorithm::IsIntrinsicSizedGridArea(const LayoutBox& child,
                                                        GridAxis axis) const {
  DCHECK(WasSetup());
  GridTrackSizingDirection direction = GridDirectionForAxis(axis);
  const GridSpan& span = grid_->GridItemSpan(child, direction);
  for (auto track_position : span) {
    const GridTrackSize& track_size =
        RawGridTrackSize(direction, track_position);
    // We consider fr units as 'auto' for the min sizing function.
    // TODO(jfernandez): https://github.com/w3c/csswg-drafts/issues/2611
    //
    // The use of AvailableSize function may imply different results
    // for the same item when assuming indefinite or definite size
    // constraints depending on the phase we evaluate the item's
    // baseline participation.
    // TODO(jfernandez): https://github.com/w3c/csswg-drafts/issues/3046
    if (track_size.IsContentSized() || track_size.IsFitContent() ||
        track_size.MinTrackBreadth().IsFlex() ||
        (track_size.MaxTrackBreadth().IsFlex() && !AvailableSpace(direction)))
      return true;
  }
  return false;
}

bool GridTrackSizingAlgorithmStrategy::
    UpdateOverrideContainingBlockContentSizeForChild(
        LayoutBox& child,
        GridTrackSizingDirection direction,
        absl::optional<LayoutUnit> override_size) const {
  if (!override_size)
    override_size = algorithm_->GridAreaBreadthForChild(child, direction);
  if (GridLayoutUtils::OverrideContainingBlockContentSizeForChild(
          child, direction) == override_size.value())
    return false;

  SetOverrideContainingBlockContentSizeForChild(child, direction,
                                                override_size.value());
  return true;
}

LayoutUnit GridTrackSizingAlgorithmStrategy::LogicalHeightForChild(
    LayoutBox& child) const {
  GridTrackSizingDirection child_block_direction =
      GridLayoutUtils::FlowAwareDirectionForChild(*GetLayoutGrid(), child,
                                                  kForRows);
  // If |child| has a relative block-axis size, we shouldn't let it override its
  // intrinsic size, which is what we are interested in here. Thus we
  // need to set the block-axis OverrideContainingBlock size to -1 (no possible
  // resolution).
  if (ShouldClearOverrideContainingBlockContentSizeForChild(
          *GetLayoutGrid(), child, child_block_direction)) {
    SetOverrideContainingBlockContentSizeForChild(child, child_block_direction,
                                                  LayoutUnit(-1));
    child.SetSelfNeedsLayoutForAvailableSpace(true);
  }

  child.LayoutIfNeeded();

  return child.LogicalHeight() +
         GridLayoutUtils::MarginLogicalHeightForChild(*GetLayoutGrid(), child) +
         algorithm_->BaselineOffsetForChild(child,
                                            GridAxisForDirection(Direction()));
}

DISABLE_CFI_PERF
LayoutUnit GridTrackSizingAlgorithmStrategy::MinContentForChild(
    LayoutBox& child) const {
  GridTrackSizingDirection child_inline_direction =
      GridLayoutUtils::FlowAwareDirectionForChild(*GetLayoutGrid(), child,
                                                  kForColumns);
  if (Direction() == child_inline_direction) {
    // FIXME: It's unclear if we should return the intrinsic width or the
    // preferred width.
    // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
    if (child.NeedsPreferredWidthsRecalculation())
      child.SetIntrinsicLogicalWidthsDirty();
    return child.PreferredLogicalWidths().min_size +
           GridLayoutUtils::MarginLogicalWidthForChild(*GetLayoutGrid(),
                                                       child) +
           algorithm_->BaselineOffsetForChild(
               child, GridAxisForDirection(Direction()));
  }

  if (UpdateOverrideContainingBlockContentSizeForChild(
          child, child_inline_direction)) {
    child.SetSelfNeedsLayoutForAvailableSpace(true);
  }
  return LogicalHeightForChild(child);
}

DISABLE_CFI_PERF
LayoutUnit GridTrackSizingAlgorithmStrategy::MaxContentForChild(
    LayoutBox& child) const {
  GridTrackSizingDirection child_inline_direction =
      GridLayoutUtils::FlowAwareDirectionForChild(*GetLayoutGrid(), child,
                                                  kForColumns);
  if (Direction() == child_inline_direction) {
    // FIXME: It's unclear if we should return the intrinsic width or the
    // preferred width.
    // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
    if (child.NeedsPreferredWidthsRecalculation())
      child.SetIntrinsicLogicalWidthsDirty();
    return child.PreferredLogicalWidths().max_size +
           GridLayoutUtils::MarginLogicalWidthForChild(*GetLayoutGrid(),
                                                       child) +
           algorithm_->BaselineOffsetForChild(
               child, GridAxisForDirection(Direction()));
  }

  if (UpdateOverrideContainingBlockContentSizeForChild(
          child, child_inline_direction)) {
    child.SetSelfNeedsLayoutForAvailableSpace(true);
  }
  return LogicalHeightForChild(child);
}

LayoutUnit GridTrackSizingAlgorithmStrategy::MinSizeForChild(
    LayoutBox& child) const {
  GridTrackSizingDirection child_inline_direction =
      GridLayoutUtils::FlowAwareDirectionForChild(*GetLayoutGrid(), child,
                                                  kForColumns);
  bool is_row_axis = Direction() == child_inline_direction;
  const Length& child_size = is_row_axis ? child.StyleRef().LogicalWidth()
                                         : child.StyleRef().LogicalHeight();
  if (!child_size.IsAuto() && !child_size.IsPercentOrCalc())
    return MinContentForChild(child);

  const Length& child_min_size = is_row_axis
                                     ? child.StyleRef().LogicalMinWidth()
                                     : child.StyleRef().LogicalMinHeight();
  auto overflow = is_row_axis ? child.StyleRef().OverflowInlineDirection()
                              : child.StyleRef().OverflowBlockDirection();
  bool overflow_allows_auto =
      overflow == EOverflow::kVisible || overflow == EOverflow::kClip;
  LayoutUnit baseline_shim = algorithm_->BaselineOffsetForChild(
      child, GridAxisForDirection(Direction()));

  if (child_min_size.IsAuto() && overflow_allows_auto) {
    LayoutUnit min_size = MinContentForChild(child);
    const GridSpan& span =
        algorithm_->GetGrid().GridItemSpan(child, Direction());
    LayoutUnit max_breadth;
    const Vector<GridTrack>& all_tracks = algorithm_->Tracks(Direction());
    for (auto track_position : span) {
      const GridTrackSize& track_size =
          all_tracks[track_position].CachedTrackSize();
      if (!track_size.HasFixedMaxTrackBreadth())
        return min_size;
      max_breadth += ValueForLength(track_size.MaxTrackBreadth().length(),
                                    AvailableSpace().value_or(LayoutUnit()));
    }
    if (min_size > max_breadth) {
      LayoutUnit margin_and_border_and_padding =
          is_row_axis ? GridLayoutUtils::MarginLogicalWidthForChild(
                            *GetLayoutGrid(), child) +
                            child.BorderAndPaddingLogicalWidth()
                      : GridLayoutUtils::MarginLogicalHeightForChild(
                            *GetLayoutGrid(), child) +
                            child.BorderAndPaddingLogicalHeight();
      min_size =
          std::max(max_breadth, margin_and_border_and_padding + baseline_shim);
    }
    return min_size;
  }

  LayoutUnit grid_area_size =
      algorithm_->GridAreaBreadthForChild(child, child_inline_direction);
  return MinLogicalSizeForChild(child, child_min_size, grid_area_size) +
         baseline_shim;
}

bool GridTrackSizingAlgorithm::CanParticipateInBaselineAlignment(
    const LayoutBox& child,
    GridAxis baseline_axis) const {
  DCHECK(baseline_axis == kGridColumnAxis
             ? column_baseline_items_map_.Contains(&child)
             : row_baseline_items_map_.Contains(&child));

  // Baseline cyclic dependencies only happen with synthesized
  // baselines. These cases include orthogonal or empty grid items
  // and replaced elements.
  bool is_parallel_to_baseline_axis =
      baseline_axis == kGridColumnAxis
          ? !GridLayoutUtils::IsOrthogonalChild(*layout_grid_, child)
          : GridLayoutUtils::IsOrthogonalChild(*layout_grid_, child);
  if (is_parallel_to_baseline_axis && child.FirstLineBoxBaseline() != -1)
    return true;

  // Baseline cyclic dependencies only happen in grid areas with
  // intrinsically-sized tracks.
  if (!IsIntrinsicSizedGridArea(child, baseline_axis))
    return true;

  return is_parallel_to_baseline_axis
             ? !child.HasRelativeLogicalHeight()
             : !child.HasRelativeLogicalWidth() &&
                   !child.StyleRef().LogicalWidth().IsAuto();
}

bool GridTrackSizingAlgorithm::ParticipateInBaselineAlignment(
    const LayoutBox& child,
    GridAxis baseline_axis) const {
  if (baseline_axis == kGridColumnAxis) {
    auto it = column_baseline_items_map_.find(&child);
    return it != column_baseline_items_map_.end() ? it->value : false;
  }
  auto it = row_baseline_items_map_.find(&child);
  return it != row_baseline_items_map_.end() ? it->value : false;
}

void GridTrackSizingAlgorithm::UpdateBaselineAlignmentContext(
    const LayoutBox& child,
    GridAxis baseline_axis) {
  DCHECK(WasSetup());
  DCHECK(CanParticipateInBaselineAlignment(child, baseline_axis));
  DCHECK(!child.NeedsLayout());

  ItemPosition align =
      layout_grid_->SelfAlignmentForChild(baseline_axis, child).GetPosition();
  const auto& span =
      grid_->GridItemSpan(child, GridDirectionForAxis(baseline_axis));
  baseline_alignment_.UpdateBaselineAlignmentContext(align, span.StartLine(),
                                                     child, baseline_axis);
}

LayoutUnit GridTrackSizingAlgorithm::BaselineOffsetForChild(
    const LayoutBox& child,
    GridAxis baseline_axis) const {
  if (!ParticipateInBaselineAlignment(child, baseline_axis))
    return LayoutUnit();

  ItemPosition align =
      layout_grid_->SelfAlignmentForChild(baseline_axis, child).GetPosition();
  const auto& span =
      grid_->GridItemSpan(child, GridDirectionForAxis(baseline_axis));
  return baseline_alignment_.BaselineOffsetForChild(align, span.StartLine(),
                                                    child, baseline_axis);
}

void GridTrackSizingAlgorithm::ClearBaselineItemsCache() {
  column_baseline_items_map_.clear();
  row_baseline_items_map_.clear();
}

void GridTrackSizingAlgorithm::CacheBaselineAlignedItem(const LayoutBox& item,
                                                        GridAxis axis) {
  DCHECK(layout_grid_->IsBaselineAlignmentForChild(item, axis));
  if (axis == kGridColumnAxis)
    column_baseline_items_map_.insert(&item, true);
  else
    row_baseline_items_map_.insert(&item, true);
}

void GridTrackSizingAlgorithm::CopyBaselineItemsCache(
    const GridTrackSizingAlgorithm* source,
    GridAxis axis) {
  if (axis == kGridColumnAxis)
    column_baseline_items_map_ = source->column_baseline_items_map_;
  else
    row_baseline_items_map_ = source->row_baseline_items_map_;
}

LayoutUnit GridTrackSizingAlgorithmStrategy::ComputeTrackBasedSize() const {
  return algorithm_->ComputeTrackBasedSize();
}

double GridTrackSizingAlgorithmStrategy::FindFrUnitSize(
    const GridSpan& tracks_span,
    LayoutUnit left_over_space) const {
  return algorithm_->FindFrUnitSize(tracks_span, left_over_space);
}

void GridTrackSizingAlgorithmStrategy::DistributeSpaceToTracks(
    Vector<GridTrack*>& tracks,
    LayoutUnit& available_logical_space) const {
  algorithm_->DistributeSpaceToTracks<kMaximizeTracks>(tracks, nullptr,
                                                       available_logical_space);
}

LayoutUnit GridTrackSizingAlgorithmStrategy::MinLogicalSizeForChild(
    LayoutBox& child,
    const Length& child_min_size,
    LayoutUnit available_size) const {
  GridTrackSizingDirection child_inline_direction =
      GridLayoutUtils::FlowAwareDirectionForChild(*GetLayoutGrid(), child,
                                                  kForColumns);
  bool is_row_axis = Direction() == child_inline_direction;

  if (is_row_axis) {
    return child.ComputeLogicalWidthUsing(kMinSize, child_min_size,
                                          available_size, GetLayoutGrid()) +
           GridLayoutUtils::MarginLogicalWidthForChild(*GetLayoutGrid(), child);
  }

  bool override_size_has_changed =
      UpdateOverrideContainingBlockContentSizeForChild(
          child, child_inline_direction, available_size);
  LayoutGridItemForMinSizeComputation(child, override_size_has_changed);

  return child.ComputeLogicalHeightUsing(kMinSize, child_min_size,
                                         child.IntrinsicLogicalHeight()) +
         GridLayoutUtils::MarginLogicalHeightForChild(*GetLayoutGrid(), child);
}

LayoutUnit DefiniteSizeStrategy::MinLogicalSizeForChild(
    LayoutBox& child,
    const Length& child_min_size,
    LayoutUnit available_size) const {
  GridTrackSizingDirection child_inline_direction =
      GridLayoutUtils::FlowAwareDirectionForChild(*GetLayoutGrid(), child,
                                                  kForColumns);
  LayoutUnit indefinite_size =
      Direction() == child_inline_direction ? LayoutUnit() : LayoutUnit(-1);
  if (HasRelativeMarginOrPaddingForChild(*GetLayoutGrid(), child,
                                         Direction()) ||
      (Direction() != child_inline_direction &&
       HasRelativeOrIntrinsicSizeForChild(*GetLayoutGrid(), child,
                                          Direction()))) {
    SetOverrideContainingBlockContentSizeForChild(child, Direction(),
                                                  indefinite_size);
  }
  return GridTrackSizingAlgorithmStrategy::MinLogicalSizeForChild(
      child, child_min_size, available_size);
}

void DefiniteSizeStrategy::LayoutGridItemForMinSizeComputation(
    LayoutBox& child,
    bool override_size_has_changed) const {
  if (override_size_has_changed) {
    child.SetSelfNeedsLayoutForAvailableSpace(true);
    child.LayoutIfNeeded();
  }
}

void DefiniteSizeStrategy::MaximizeTracks(
    Vector<GridTrack>& tracks,
    absl::optional<LayoutUnit>& free_space) {
  wtf_size_t tracks_size = tracks.size();
  Vector<GridTrack*> tracks_for_distribution(tracks_size);
  for (wtf_size_t i = 0; i < tracks_size; ++i) {
    tracks_for_distribution[i] = tracks.data() + i;
    tracks_for_distribution[i]->SetPlannedSize(
        tracks_for_distribution[i]->BaseSize());
  }

  DCHECK(free_space);
  DistributeSpaceToTracks(tracks_for_distribution, free_space.value());

  for (auto* track : tracks_for_distribution)
    track->SetBaseSize(track->PlannedSize());
}

double DefiniteSizeStrategy::FindUsedFlexFraction(
    Vector<wtf_size_t>& flexible_sized_tracks_index,
    GridTrackSizingDirection direction,
    absl::optional<LayoutUnit> free_space) const {
  GridSpan all_tracks_span = GridSpan::TranslatedDefiniteGridSpan(
      0, algorithm_->Tracks(direction).size());
  DCHECK(free_space);
  return FindFrUnitSize(all_tracks_span, free_space.value());
}

LayoutUnit DefiniteSizeStrategy::FreeSpaceForStretchAutoTracksStep() const {
  DCHECK(algorithm_->FreeSpace(Direction()));
  return algorithm_->FreeSpace(Direction()).value();
}

DISABLE_CFI_PERF
LayoutUnit DefiniteSizeStrategy::MinContentForChild(LayoutBox& child) const {
  GridTrackSizingDirection child_inline_direction =
      GridLayoutUtils::FlowAwareDirectionForChild(*GetLayoutGrid(), child,
                                                  kForColumns);
  if (Direction() == child_inline_direction && child.NeedsLayout() &&
      ShouldClearOverrideContainingBlockContentSizeForChild(
          *GetLayoutGrid(), child, child_inline_direction)) {
    SetOverrideContainingBlockContentSizeForChild(child, child_inline_direction,
                                                  LayoutUnit());
  }

  return GridTrackSizingAlgorithmStrategy::MinContentForChild(child);
}

void IndefiniteSizeStrategy::LayoutGridItemForMinSizeComputation(
    LayoutBox& child,
    bool override_size_has_changed) const {
  if (override_size_has_changed && Direction() != kForColumns) {
    child.SetSelfNeedsLayoutForAvailableSpace(true);
    child.LayoutIfNeeded();
  }
}

void IndefiniteSizeStrategy::MaximizeTracks(Vector<GridTrack>& tracks,
                                            absl::optional<LayoutUnit>&) {
  for (auto& track : tracks)
    track.SetBaseSize(track.GrowthLimit());
}

static inline double NormalizedFlexFraction(const GridTrack& track) {
  double flex_factor = track.CachedTrackSize().MaxTrackBreadth().Flex();
  return track.BaseSize() / std::max<double>(1, flex_factor);
}

double IndefiniteSizeStrategy::FindUsedFlexFraction(
    Vector<wtf_size_t>& flexible_sized_tracks_index,
    GridTrackSizingDirection direction,
    absl::optional<LayoutUnit>) const {
  auto all_tracks = algorithm_->Tracks(direction);

  double flex_fraction = 0;
  for (const auto& track_index : flexible_sized_tracks_index) {
    flex_fraction = std::max(flex_fraction,
                             NormalizedFlexFraction(all_tracks[track_index]));
  }

  const Grid& grid = algorithm_->GetGrid();
  if (!grid.HasGridItems())
    return flex_fraction;

  IterateGridItemsInTrackIndices(
      grid, direction, flexible_sized_tracks_index,
      [&](LayoutBox* grid_item, const GridSpan& span) {
        // Removing gutters from the max-content contribution of the item,
        // so they are not taken into account in FindFrUnitSize().
        LayoutUnit left_over_space =
            MaxContentForChild(*grid_item) -
            GetLayoutGrid()->GuttersSize(algorithm_->GetGrid(), direction,
                                         span.StartLine(), span.IntegerSpan(),
                                         AvailableSpace());
        flex_fraction =
            std::max(flex_fraction, FindFrUnitSize(span, left_over_space));
      });

  return flex_fraction;
}

bool IndefiniteSizeStrategy::RecomputeUsedFlexFractionIfNeeded(
    double& flex_fraction,
    Vector<LayoutUnit>& increments,
    LayoutUnit& total_growth) const {
  if (Direction() == kForColumns)
    return false;

  const LayoutGrid* layout_grid = GetLayoutGrid();
  LayoutUnit min_size = layout_grid->ComputeContentLogicalHeight(
      kMinSize, layout_grid->StyleRef().LogicalMinHeight(), LayoutUnit(-1));
  LayoutUnit max_size = layout_grid->ComputeContentLogicalHeight(
      kMaxSize, layout_grid->StyleRef().LogicalMaxHeight(), LayoutUnit(-1));

  // Redo the flex fraction computation using min|max-height as definite
  // available space in case the total height is smaller than min-height or
  // larger than max-height.
  LayoutUnit rows_size = total_growth + ComputeTrackBasedSize();
  bool check_min_size = min_size && rows_size < min_size;
  bool check_max_size = max_size != -1 && rows_size > max_size;
  if (!check_min_size && !check_max_size)
    return false;

  LayoutUnit free_space = check_max_size ? max_size : LayoutUnit(-1);
  const Grid& grid = algorithm_->GetGrid();
  free_space =
      std::max(free_space, min_size) -
      layout_grid->GuttersSize(grid, kForRows, 0, grid.NumTracks(kForRows),
                               AvailableSpace());

  wtf_size_t number_of_tracks = algorithm_->Tracks(Direction()).size();
  flex_fraction = FindFrUnitSize(
      GridSpan::TranslatedDefiniteGridSpan(0, number_of_tracks), free_space);
  return true;
}

LayoutUnit IndefiniteSizeStrategy::FreeSpaceForStretchAutoTracksStep() const {
  DCHECK(!algorithm_->FreeSpace(Direction()));
  if (Direction() == kForColumns)
    return LayoutUnit();

  LayoutUnit min_size = GetLayoutGrid()->ComputeContentLogicalHeight(
      kMinSize, GetLayoutGrid()->StyleRef().LogicalMinHeight(), LayoutUnit(-1));
  return min_size - ComputeTrackBasedSize();
}

DISABLE_CFI_PERF
LayoutUnit IndefiniteSizeStrategy::MinContentForChild(LayoutBox& child) const {
  GridTrackSizingDirection child_inline_direction =
      GridLayoutUtils::FlowAwareDirectionForChild(*GetLayoutGrid(), child,
                                                  kForColumns);
  if (Direction() == child_inline_direction || Direction() == kForRows)
    return GridTrackSizingAlgorithmStrategy::MinContentForChild(child);

  // This code is executed only when computing the grid's intrinsic
  // width based on an orthogonal child. We rely on the pre-layout
  // performed in LayoutGrid::LayoutOrthogonalWritingModeRoots.
  DCHECK(GridLayoutUtils::IsOrthogonalChild(*GetLayoutGrid(), child));

  return child.LogicalHeight() +
         GridLayoutUtils::MarginLogicalHeightForChild(*GetLayoutGrid(), child) +
         algorithm_->BaselineOffsetForChild(child,
                                            GridAxisForDirection(Direction()));
}

DISABLE_CFI_PERF
LayoutUnit IndefiniteSizeStrategy::MaxContentForChild(LayoutBox& child) const {
  GridTrackSizingDirection child_inline_direction =
      GridLayoutUtils::FlowAwareDirectionForChild(*GetLayoutGrid(), child,
                                                  kForColumns);
  if (Direction() == child_inline_direction || Direction() == kForRows)
    return GridTrackSizingAlgorithmStrategy::MaxContentForChild(child);

  // This code is executed only when computing the grid's intrinsic
  // width based on an orthogonal child. We rely on the pre-layout
  // performed in LayoutGrid::LayoutOrthogonalWritingModeRoots.
  DCHECK(GridLayoutUtils::IsOrthogonalChild(*GetLayoutGrid(), child));

  return child.LogicalHeight() +
         GridLayoutUtils::MarginLogicalHeightForChild(*GetLayoutGrid(), child) +
         algorithm_->BaselineOffsetForChild(child,
                                            GridAxisForDirection(Direction()));
}

bool IndefiniteSizeStrategy::IsComputingSizeContainment() const {
  return GetLayoutGrid()->ShouldApplySizeContainment();
}

absl::optional<LayoutUnit> GridTrackSizingAlgorithm::FreeSpace(
    GridTrackSizingDirection direction) const {
  return direction == kForRows ? free_space_rows_ : free_space_columns_;
}

absl::optional<LayoutUnit> GridTrackSizingAlgorithm::AvailableSpace(
    GridTrackSizingDirection direction) const {
  return direction == kForRows ? available_space_rows_
                               : available_space_columns_;
}

absl::optional<LayoutUnit> GridTrackSizingAlgorithm::AvailableSpace() const {
  DCHECK(WasSetup());
  return AvailableSpace(direction_);
}

void GridTrackSizingAlgorithm::SetAvailableSpace(
    GridTrackSizingDirection direction,
    absl::optional<LayoutUnit> available_space) {
  if (direction == kForColumns)
    available_space_columns_ = available_space;
  else
    available_space_rows_ = available_space;
}

Vector<GridTrack>& GridTrackSizingAlgorithm::Tracks(
    GridTrackSizingDirection direction) {
  return direction == kForColumns ? columns_ : rows_;
}

const Vector<GridTrack>& GridTrackSizingAlgorithm::Tracks(
    GridTrackSizingDirection direction) const {
  return direction == kForColumns ? columns_ : rows_;
}

void GridTrackSizingAlgorithm::SetFreeSpace(
    GridTrackSizingDirection direction,
    absl::optional<LayoutUnit> free_space) {
  if (direction == kForColumns)
    free_space_columns_ = free_space;
  else
    free_space_rows_ = free_space;
}

const GridTrackSize& GridTrackSizingAlgorithm::RawGridTrackSize(
    GridTrackSizingDirection direction,
    wtf_size_t translated_index) const {
  bool is_row_axis = direction == kForColumns;
  const ComputedStyle& grid_container_style = layout_grid_->StyleRef();
  const ComputedGridTrackList& computed_grid_track_list =
      is_row_axis ? grid_container_style.GridTemplateColumns()
                  : grid_container_style.GridTemplateRows();
  const Vector<GridTrackSize, 1>& track_list_sizes =
      computed_grid_track_list.track_sizes.LegacyTrackList();
  const Vector<GridTrackSize, 1>& auto_repeat_track_sizes =
      computed_grid_track_list.auto_repeat_track_sizes;
  const Vector<GridTrackSize, 1>& auto_track_styles =
      is_row_axis ? grid_container_style.GridAutoColumns().LegacyTrackList()
                  : grid_container_style.GridAutoRows().LegacyTrackList();
  wtf_size_t insertion_point =
      computed_grid_track_list.auto_repeat_insertion_point;
  wtf_size_t auto_repeat_tracks_count = grid_->AutoRepeatTracks(direction);

  // We should not use GridPositionsResolver::explicitGridXXXCount() for this
  // because the explicit grid might be larger than the number of tracks in
  // grid-template-rows|columns (if grid-template-areas is specified for
  // example).
  wtf_size_t explicit_tracks_count =
      track_list_sizes.size() + auto_repeat_tracks_count;

  int untranslated_index_as_int =
      static_cast<int>(translated_index - grid_->ExplicitGridStart(direction));
  wtf_size_t auto_track_styles_size = auto_track_styles.size();
  if (untranslated_index_as_int < 0) {
    int index =
        untranslated_index_as_int % static_cast<int>(auto_track_styles_size);
    // We need to traspose the index because the first negative implicit line
    // will get the last defined auto track and so on.
    index += index ? auto_track_styles_size : 0;
    return auto_track_styles[index];
  }

  wtf_size_t untranslated_index =
      static_cast<wtf_size_t>(untranslated_index_as_int);
  if (untranslated_index >= explicit_tracks_count) {
    return auto_track_styles[(untranslated_index - explicit_tracks_count) %
                             auto_track_styles_size];
  }

  if (LIKELY(!auto_repeat_tracks_count) || untranslated_index < insertion_point)
    return track_list_sizes[untranslated_index];

  if (untranslated_index < (insertion_point + auto_repeat_tracks_count)) {
    wtf_size_t auto_repeat_local_index = untranslated_index - insertion_point;
    return auto_repeat_track_sizes[auto_repeat_local_index %
                                   auto_repeat_track_sizes.size()];
  }

  return track_list_sizes[untranslated_index - auto_repeat_tracks_count];
}

bool GridTrackSizingAlgorithm::IsRelativeGridLengthAsAuto(
    const GridLength& length,
    GridTrackSizingDirection direction) const {
  return length.HasPercentage() && !AvailableSpace(direction);
}

bool GridTrackSizingAlgorithm::IsRelativeSizedTrackAsAuto(
    const GridTrackSize& track_size,
    GridTrackSizingDirection direction) const {
  if (track_size.MinTrackBreadth().HasPercentage())
    return IsRelativeGridLengthAsAuto(track_size.MinTrackBreadth(), direction);
  if (track_size.MaxTrackBreadth().HasPercentage())
    return IsRelativeGridLengthAsAuto(track_size.MaxTrackBreadth(), direction);
  return false;
}

GridTrackSize GridTrackSizingAlgorithm::CalculateGridTrackSize(
    GridTrackSizingDirection direction,
    wtf_size_t translated_index) const {
  DCHECK(WasSetup());
  // Collapse empty auto repeat tracks if auto-fit.
  if (grid_->HasAutoRepeatEmptyTracks(direction) &&
      grid_->IsEmptyAutoRepeatTrack(direction, translated_index))
    return {Length::Fixed(), kLengthTrackSizing};

  const GridTrackSize& track_size =
      RawGridTrackSize(direction, translated_index);
  if (track_size.IsFitContent()) {
    return IsRelativeGridLengthAsAuto(track_size.FitContentTrackBreadth(),
                                      direction)
               ? GridTrackSize(Length::Auto(), Length::MaxContent())
               : track_size;
  }

  GridLength min_track_breadth = track_size.MinTrackBreadth();
  GridLength max_track_breadth = track_size.MaxTrackBreadth();

  // If the logical width/height of the grid container is indefinite, percentage
  // values are treated as <auto>.
  if (IsRelativeSizedTrackAsAuto(track_size, direction)) {
    if (direction == kForRows) {
      // We avoid counting the cases in which it doesn't matter if we resolve
      // the percentages row tracks against the intrinsic height of the grid
      // container or we treat them as auto. Basically if we have just one row,
      // it has 100% size and the max-block-size is none.
      if ((grid_->NumTracks(direction) != 1) || !min_track_breadth.IsLength() ||
          !min_track_breadth.length().IsPercent() ||
          (min_track_breadth.length().Percent() != 100.0f) ||
          !max_track_breadth.IsLength() ||
          !max_track_breadth.length().IsPercent() ||
          (max_track_breadth.length().Percent() != 100.0f) ||
          !layout_grid_->StyleRef().LogicalMaxHeight().IsNone()) {
        UseCounter::Count(layout_grid_->GetDocument(),
                          WebFeature::kGridRowTrackPercentIndefiniteHeight);
      }
    }
    if (min_track_breadth.HasPercentage())
      min_track_breadth = Length::Auto();
    if (max_track_breadth.HasPercentage())
      max_track_breadth = Length::Auto();
  }

  // Flex sizes are invalid as a min sizing function. However we still can have
  // a flexible |minTrackBreadth| if the track had a flex size directly (e.g.
  // "1fr"), the spec says that in this case it implies an automatic minimum.
  // TODO(jfernandez): https://github.com/w3c/csswg-drafts/issues/2611
  // TODO(jfernandez): We may have to change IsIntrinsicSizedGridArea too.
  if (min_track_breadth.IsFlex())
    min_track_breadth = Length::Auto();

  return GridTrackSize(min_track_breadth, max_track_breadth);
}

LayoutUnit GridTrackSizingAlgorithm::InitialBaseSize(
    const GridTrackSize& track_size) const {
  const GridLength& grid_length = track_size.MinTrackBreadth();

  // TODO(obrufau): https://github.com/w3c/csswg-drafts/issues/2611 may allow
  // flexible lengths to be used as min track sizing functions.
  DCHECK(!grid_length.IsFlex());

  const Length& track_length = grid_length.length();
  if (track_length.IsSpecified()) {
    DCHECK(!grid_length.HasPercentage() || AvailableSpace());
    return ValueForLength(track_length,
                          AvailableSpace().value_or(LayoutUnit()));
  }

  DCHECK(track_length.IsMinContent() || track_length.IsAuto() ||
         track_length.IsMaxContent());
  return LayoutUnit();
}

LayoutUnit GridTrackSizingAlgorithm::InitialGrowthLimit(
    const GridTrackSize& track_size,
    LayoutUnit base_size) const {
  const GridLength& grid_length = track_size.MaxTrackBreadth();
  if (grid_length.IsFlex())
    return base_size;

  const Length& track_length = grid_length.length();
  if (track_length.IsSpecified()) {
    DCHECK(!grid_length.HasPercentage() || AvailableSpace());
    return ValueForLength(track_length,
                          AvailableSpace().value_or(LayoutUnit()));
  }

  DCHECK(track_length.IsMinContent() || track_length.IsAuto() ||
         track_length.IsMaxContent());
  return LayoutUnit(kInfinity);
}

void GridTrackSizingAlgorithm::InitializeTrackSizes() {
  DCHECK(content_sized_tracks_index_.empty());
  DCHECK(flexible_sized_tracks_index_.empty());
  DCHECK(auto_sized_tracks_for_stretch_index_.empty());
  DCHECK(!has_percent_sized_rows_indefinite_height_);
  Vector<GridTrack>& track_list = Tracks(direction_);
  bool indefinite_height =
      direction_ == kForRows && !layout_grid_->CachedHasDefiniteLogicalHeight();
  wtf_size_t num_tracks = track_list.size();
  for (wtf_size_t i = 0; i < num_tracks; ++i) {
    const GridTrackSize& track_size = CalculateGridTrackSize(direction_, i);
    GridTrack& track = track_list[i];
    track.SetCachedTrackSize(track_size);
    track.SetBaseSize(InitialBaseSize(track_size));
    track.SetGrowthLimit(InitialGrowthLimit(track_size, track.BaseSize()));
    track.SetInfinitelyGrowable(false);

    if (track_size.IsFitContent()) {
      track.SetGrowthLimitCap(
          ValueForLength(track_size.FitContentTrackBreadth().length(),
                         AvailableSpace().value_or(LayoutUnit())));
    }

    if (track_size.IsContentSized())
      content_sized_tracks_index_.push_back(i);
    if (track_size.MaxTrackBreadth().IsFlex())
      flexible_sized_tracks_index_.push_back(i);
    if (track_size.HasAutoMaxTrackBreadth() && !track_size.IsFitContent())
      auto_sized_tracks_for_stretch_index_.push_back(i);

    if (!has_percent_sized_rows_indefinite_height_ && indefinite_height) {
      const GridTrackSize& raw_track_size = RawGridTrackSize(direction_, i);
      if (raw_track_size.MinTrackBreadth().HasPercentage() ||
          raw_track_size.MaxTrackBreadth().HasPercentage())
        has_percent_sized_rows_indefinite_height_ = true;
    }
  }
}

void GridTrackSizingAlgorithm::SizeTrackToFitNonSpanningItem(
    const GridSpan& span,
    LayoutBox& grid_item,
    GridTrack& track) {
  const wtf_size_t track_position = span.StartLine();
  const GridTrackSize& track_size =
      Tracks(direction_)[track_position].CachedTrackSize();

  if (track_size.HasMinContentMinTrackBreadth()) {
    track.SetBaseSize(
        std::max(track.BaseSize(), strategy_->MinContentForChild(grid_item)));
  } else if (track_size.HasMaxContentMinTrackBreadth()) {
    track.SetBaseSize(
        std::max(track.BaseSize(), strategy_->MaxContentForChild(grid_item)));
  } else if (track_size.HasAutoMinTrackBreadth()) {
    track.SetBaseSize(
        std::max(track.BaseSize(), strategy_->MinSizeForChild(grid_item)));
  }

  if (track_size.HasMinContentMaxTrackBreadth()) {
    track.SetGrowthLimit(std::max(track.GrowthLimit(),
                                  strategy_->MinContentForChild(grid_item)));
  } else if (track_size.HasMaxContentOrAutoMaxTrackBreadth()) {
    LayoutUnit growth_limit = strategy_->MaxContentForChild(grid_item);
    if (track_size.IsFitContent()) {
      growth_limit =
          std::min(growth_limit,
                   ValueForLength(track_size.FitContentTrackBreadth().length(),
                                  AvailableSpace().value_or(LayoutUnit())));
    }
    track.SetGrowthLimit(std::max(track.GrowthLimit(), growth_limit));
  }
}

bool GridTrackSizingAlgorithm::SpanningItemCrossesFlexibleSizedTracks(
    const GridSpan& span) const {
  const Vector<GridTrack>& track_list = Tracks(direction_);
  for (auto track_position : span) {
    const GridTrackSize& track_size =
        track_list[track_position].CachedTrackSize();
    if (track_size.MinTrackBreadth().IsFlex() ||
        track_size.MaxTrackBreadth().IsFlex())
      return true;
  }

  return false;
}

// We're basically using a class instead of a std::pair because of accessing
// gridItem() or getGridSpan() is much more self-explanatory that using .first
// or .second members in the pair. Having a std::pair<LayoutBox*, size_t>
// does not work either because we still need the GridSpan so we'd have to add
// an extra hash lookup for each item.
class GridItemWithSpan {
  DISALLOW_NEW();

 public:
  GridItemWithSpan(LayoutBox& grid_item, const GridSpan& grid_span)
      : grid_item_(&grid_item), grid_span_(grid_span) {}

  LayoutBox& GridItem() const { return *grid_item_; }
  GridSpan GetGridSpan() const { return grid_span_; }

  bool operator<(const GridItemWithSpan other) const {
    return grid_span_.IntegerSpan() < other.grid_span_.IntegerSpan();
  }

  void Trace(Visitor* visitor) const { visitor->Trace(grid_item_); }

 private:
  Member<LayoutBox> grid_item_;
  GridSpan grid_span_;
};

enum TrackSizeRestriction {
  kAllowInfinity,
  kForbidInfinity,
};

static LayoutUnit TrackSizeForTrackSizeComputationPhase(
    TrackSizeComputationPhase phase,
    const GridTrack& track,
    TrackSizeRestriction restriction) {
  switch (phase) {
    case kResolveIntrinsicMinimums:
    case kResolveContentBasedMinimums:
    case kResolveMaxContentMinimums:
    case kMaximizeTracks:
      return track.BaseSize();
    case kResolveIntrinsicMaximums:
    case kResolveMaxContentMaximums:
      const LayoutUnit& growth_limit = track.GrowthLimit();
      if (restriction == kAllowInfinity)
        return growth_limit;
      return growth_limit == kInfinity ? track.BaseSize() : growth_limit;
  }

  NOTREACHED();
  return track.BaseSize();
}

static bool ShouldProcessTrackForTrackSizeComputationPhase(
    TrackSizeComputationPhase phase,
    const GridTrackSize& track_size) {
  switch (phase) {
    case kResolveIntrinsicMinimums:
      return track_size.HasIntrinsicMinTrackBreadth();
    case kResolveContentBasedMinimums:
      return track_size.HasMinOrMaxContentMinTrackBreadth();
    case kResolveMaxContentMinimums:
      return track_size.HasMaxContentMinTrackBreadth();
    case kResolveIntrinsicMaximums:
      return track_size.HasIntrinsicMaxTrackBreadth();
    case kResolveMaxContentMaximums:
      return track_size.HasMaxContentOrAutoMaxTrackBreadth();
    case kMaximizeTracks:
      NOTREACHED();
      return false;
  }

  NOTREACHED();
  return false;
}

static bool TrackShouldGrowBeyondGrowthLimitsForTrackSizeComputationPhase(
    TrackSizeComputationPhase phase,
    const GridTrackSize& track_size) {
  switch (phase) {
    case kResolveIntrinsicMinimums:
    case kResolveContentBasedMinimums:
      return track_size
          .HasAutoOrMinContentMinTrackBreadthAndIntrinsicMaxTrackBreadth();
    case kResolveMaxContentMinimums:
      return track_size
          .HasMaxContentMinTrackBreadthAndMaxContentMaxTrackBreadth();
    case kResolveIntrinsicMaximums:
    case kResolveMaxContentMaximums:
      return true;
    case kMaximizeTracks:
      NOTREACHED();
      return false;
  }

  NOTREACHED();
  return false;
}

static void MarkAsInfinitelyGrowableForTrackSizeComputationPhase(
    TrackSizeComputationPhase phase,
    GridTrack& track) {
  switch (phase) {
    case kResolveIntrinsicMinimums:
    case kResolveContentBasedMinimums:
    case kResolveMaxContentMinimums:
      return;
    case kResolveIntrinsicMaximums:
      if (TrackSizeForTrackSizeComputationPhase(phase, track, kAllowInfinity) ==
              kInfinity &&
          track.PlannedSize() != kInfinity)
        track.SetInfinitelyGrowable(true);
      return;
    case kResolveMaxContentMaximums:
      if (track.InfinitelyGrowable())
        track.SetInfinitelyGrowable(false);
      return;
    case kMaximizeTracks:
      NOTREACHED();
      return;
  }

  NOTREACHED();
}

static void UpdateTrackSizeForTrackSizeComputationPhase(
    TrackSizeComputationPhase phase,
    GridTrack& track) {
  switch (phase) {
    case kResolveIntrinsicMinimums:
    case kResolveContentBasedMinimums:
    case kResolveMaxContentMinimums:
      track.SetBaseSize(track.PlannedSize());
      return;
    case kResolveIntrinsicMaximums:
    case kResolveMaxContentMaximums:
      track.SetGrowthLimit(track.PlannedSize());
      return;
    case kMaximizeTracks:
      NOTREACHED();
      return;
  }

  NOTREACHED();
}

LayoutUnit GridTrackSizingAlgorithm::ItemSizeForTrackSizeComputationPhase(
    TrackSizeComputationPhase phase,
    LayoutBox& grid_item) const {
  switch (phase) {
    case kResolveIntrinsicMinimums:
      return strategy_->MinSizeForChild(grid_item);
    case kResolveContentBasedMinimums:
    case kResolveIntrinsicMaximums:
      return strategy_->MinContentForChild(grid_item);
    case kResolveMaxContentMinimums:
    case kResolveMaxContentMaximums:
      return strategy_->MaxContentForChild(grid_item);
    case kMaximizeTracks:
      NOTREACHED();
      return LayoutUnit();
  }

  NOTREACHED();
  return LayoutUnit();
}

static bool SortByGridTrackGrowthPotential(const GridTrack* track1,
                                           const GridTrack* track2) {
  // This check ensures that we respect the irreflexivity property of the strict
  // weak ordering required by std::sort(forall x: NOT x < x).
  bool track1_has_infinite_growth_potential_without_cap =
      track1->InfiniteGrowthPotential() && !track1->GrowthLimitCap();
  bool track2_has_infinite_growth_potential_without_cap =
      track2->InfiniteGrowthPotential() && !track2->GrowthLimitCap();

  if (track1_has_infinite_growth_potential_without_cap &&
      track2_has_infinite_growth_potential_without_cap)
    return false;

  if (track1_has_infinite_growth_potential_without_cap ||
      track2_has_infinite_growth_potential_without_cap)
    return track2_has_infinite_growth_potential_without_cap;

  LayoutUnit track1_limit =
      track1->GrowthLimitCap().value_or(track1->GrowthLimit());
  LayoutUnit track2_limit =
      track2->GrowthLimitCap().value_or(track2->GrowthLimit());
  return (track1_limit - track1->BaseSize()) <
         (track2_limit - track2->BaseSize());
}

static void ClampGrowthShareIfNeeded(TrackSizeComputationPhase phase,
                                     const GridTrack& track,
                                     LayoutUnit& growth_share) {
  if (phase != kResolveMaxContentMaximums || !track.GrowthLimitCap())
    return;

  LayoutUnit distance_to_cap =
      track.GrowthLimitCap().value() - track.SizeDuringDistribution();
  if (distance_to_cap <= 0)
    return;

  growth_share = std::min(growth_share, distance_to_cap);
}

template <TrackSizeComputationPhase phase>
void GridTrackSizingAlgorithm::DistributeSpaceToTracks(
    Vector<GridTrack*>& tracks,
    Vector<GridTrack*>* grow_beyond_growth_limits_tracks,
    LayoutUnit& available_logical_space) const {
  DCHECK_GE(available_logical_space, 0);

  for (auto* track : tracks) {
    track->SetSizeDuringDistribution(
        TrackSizeForTrackSizeComputationPhase(phase, *track, kForbidInfinity));
  }

  if (available_logical_space > 0) {
    std::sort(tracks.begin(), tracks.end(), SortByGridTrackGrowthPotential);

    wtf_size_t tracks_size = tracks.size();
    for (wtf_size_t i = 0; i < tracks_size; ++i) {
      GridTrack& track = *tracks[i];
      LayoutUnit available_logical_space_share =
          available_logical_space / (tracks_size - i);
      const LayoutUnit& track_breadth =
          TrackSizeForTrackSizeComputationPhase(phase, track, kForbidInfinity);
      LayoutUnit growth_share =
          track.InfiniteGrowthPotential()
              ? available_logical_space_share
              : std::min(available_logical_space_share,
                         track.GrowthLimit() - track_breadth);
      ClampGrowthShareIfNeeded(phase, track, growth_share);
      DCHECK_GE(growth_share, 0) << "We must never shrink any grid track or "
                                    "else we can't guarantee we abide by our "
                                    "min-sizing function.";
      track.GrowSizeDuringDistribution(growth_share);
      available_logical_space -= growth_share;
    }
  }

  if (available_logical_space > 0 && grow_beyond_growth_limits_tracks) {
    // We need to sort them because there might be tracks with growth limit caps
    // (like the ones with fit-content()) which cannot indefinitely grow over
    // the limits.
    if (phase == kResolveMaxContentMaximums) {
      std::sort(grow_beyond_growth_limits_tracks->begin(),
                grow_beyond_growth_limits_tracks->end(),
                SortByGridTrackGrowthPotential);
    }

    wtf_size_t tracks_growing_above_max_breadth_size =
        grow_beyond_growth_limits_tracks->size();
    for (wtf_size_t i = 0; i < tracks_growing_above_max_breadth_size; ++i) {
      GridTrack* track = grow_beyond_growth_limits_tracks->at(i);
      LayoutUnit growth_share =
          available_logical_space / (tracks_growing_above_max_breadth_size - i);
      ClampGrowthShareIfNeeded(phase, *track, growth_share);
      DCHECK_GE(growth_share, 0) << "We must never shrink any grid track or "
                                    "else we can't guarantee we abide by our "
                                    "min-sizing function.";
      track->GrowSizeDuringDistribution(growth_share);
      available_logical_space -= growth_share;
    }
  }

  for (auto* track : tracks) {
    track->SetPlannedSize(
        track->PlannedSize() == kInfinity
            ? track->SizeDuringDistribution()
            : std::max(track->PlannedSize(), track->SizeDuringDistribution()));
  }
}

template <TrackSizeComputationPhase phase>
void GridTrackSizingAlgorithm::IncreaseSizesToAccommodateSpanningItems(
    const HeapVector<GridItemWithSpan>::iterator& grid_items_with_span_begin,
    const HeapVector<GridItemWithSpan>::iterator& grid_items_with_span_end) {
  Vector<GridTrack>& all_tracks = Tracks(direction_);
  for (const auto& track_index : content_sized_tracks_index_) {
    GridTrack& track = all_tracks[track_index];
    track.SetPlannedSize(
        TrackSizeForTrackSizeComputationPhase(phase, track, kAllowInfinity));
  }

  Vector<GridTrack*> grow_beyond_growth_limits_tracks;
  Vector<GridTrack*> filtered_tracks;
  for (auto* it = grid_items_with_span_begin; it != grid_items_with_span_end;
       ++it) {
    const GridItemWithSpan& grid_item_with_span = *it;
    DCHECK_GT(grid_item_with_span.GetGridSpan().IntegerSpan(), 1u);
    const GridSpan& item_span = grid_item_with_span.GetGridSpan();

    grow_beyond_growth_limits_tracks.Shrink(0);
    filtered_tracks.Shrink(0);
    LayoutUnit spanning_tracks_size;
    for (auto track_position : item_span) {
      GridTrack& track = all_tracks[track_position];
      const GridTrackSize& track_size = track.CachedTrackSize();
      spanning_tracks_size +=
          TrackSizeForTrackSizeComputationPhase(phase, track, kForbidInfinity);
      if (!ShouldProcessTrackForTrackSizeComputationPhase(phase, track_size))
        continue;

      filtered_tracks.push_back(&track);

      if (TrackShouldGrowBeyondGrowthLimitsForTrackSizeComputationPhase(
              phase, track_size))
        grow_beyond_growth_limits_tracks.push_back(&track);
    }

    if (filtered_tracks.empty())
      continue;

    spanning_tracks_size +=
        layout_grid_->GuttersSize(*grid_, direction_, item_span.StartLine(),
                                  item_span.IntegerSpan(), AvailableSpace());

    LayoutUnit extra_space = ItemSizeForTrackSizeComputationPhase(
                                 phase, grid_item_with_span.GridItem()) -
                             spanning_tracks_size;
    extra_space = extra_space.ClampNegativeToZero();
    auto& tracks_to_grow_beyond_growth_limits =
        grow_beyond_growth_limits_tracks.empty()
            ? filtered_tracks
            : grow_beyond_growth_limits_tracks;
    DistributeSpaceToTracks<phase>(
        filtered_tracks, &tracks_to_grow_beyond_growth_limits, extra_space);
  }

  for (const auto& track_index : content_sized_tracks_index_) {
    GridTrack& track = all_tracks[track_index];
    MarkAsInfinitelyGrowableForTrackSizeComputationPhase(phase, track);
    UpdateTrackSizeForTrackSizeComputationPhase(phase, track);
  }
}

void GridTrackSizingAlgorithm::ResolveIntrinsicTrackSizes() {
  Vector<GridTrack>& all_tracks = Tracks(direction_);
  HeapVector<GridItemWithSpan> items_sorted_by_increasing_span;
  if (grid_->HasGridItems()) {
    IterateGridItemsInTrackIndices(
        *grid_, direction_, content_sized_tracks_index_,
        [&](LayoutBox* grid_item, const GridSpan& span) {
          if (span.IntegerSpan() == 1) {
            SizeTrackToFitNonSpanningItem(span, *grid_item,
                                          all_tracks[span.StartLine()]);
          } else if (!SpanningItemCrossesFlexibleSizedTracks(span)) {
            items_sorted_by_increasing_span.push_back(
                GridItemWithSpan(*grid_item, span));
          }
        });
    std::sort(items_sorted_by_increasing_span.begin(),
              items_sorted_by_increasing_span.end());
  }

  auto* it = items_sorted_by_increasing_span.begin();
  auto* end = items_sorted_by_increasing_span.end();
  while (it != end) {
    auto* range_end = std::upper_bound(it, end, *it);
    IncreaseSizesToAccommodateSpanningItems<kResolveIntrinsicMinimums>(
        it, range_end);
    IncreaseSizesToAccommodateSpanningItems<kResolveContentBasedMinimums>(
        it, range_end);
    IncreaseSizesToAccommodateSpanningItems<kResolveMaxContentMinimums>(
        it, range_end);
    IncreaseSizesToAccommodateSpanningItems<kResolveIntrinsicMaximums>(
        it, range_end);
    IncreaseSizesToAccommodateSpanningItems<kResolveMaxContentMaximums>(
        it, range_end);
    it = range_end;
  }

  for (const auto& track_index : content_sized_tracks_index_) {
    GridTrack& track = all_tracks[track_index];
    if (track.GrowthLimit() == kInfinity)
      track.SetGrowthLimit(track.BaseSize());
  }
}

void GridTrackSizingAlgorithm::ComputeGridContainerIntrinsicSizes() {
  min_content_size_ = max_content_size_ = LayoutUnit();

  Vector<GridTrack>& all_tracks = Tracks(direction_);
  for (auto& track : all_tracks) {
    DCHECK(strategy_->IsComputingSizeContainment() ||
           !track.InfiniteGrowthPotential());
    min_content_size_ += track.BaseSize();
    max_content_size_ +=
        track.GrowthLimitIsInfinite() ? track.BaseSize() : track.GrowthLimit();
    // The growth limit caps must be cleared now in order to properly sort
    // tracks by growth potential on an eventual "Maximize Tracks".
    track.SetGrowthLimitCap(absl::nullopt);
  }
}

LayoutUnit GridTrackSizingAlgorithm::ComputeTrackBasedSize() const {
  LayoutUnit size;

  const Vector<GridTrack>& all_tracks = Tracks(direction_);
  for (auto& track : all_tracks) {
    size +=
        track.GrowthLimitIsInfinite() ? track.BaseSize() : track.GrowthLimit();
  }

  size += layout_grid_->GuttersSize(*grid_, direction_, 0, all_tracks.size(),
                                    AvailableSpace());

  return size;
}

double GridTrackSizingAlgorithm::FindFrUnitSize(
    const GridSpan& tracks_span,
    LayoutUnit left_over_space) const {
  if (left_over_space <= 0)
    return 0;

  const Vector<GridTrack>& all_tracks = Tracks(direction_);
  double flex_factor_sum = 0;
  Vector<wtf_size_t, 8> flexible_tracks_indexes;
  for (auto track_index : tracks_span) {
    const GridTrackSize& track_size = all_tracks[track_index].CachedTrackSize();
    if (!track_size.MaxTrackBreadth().IsFlex()) {
      left_over_space -= all_tracks[track_index].BaseSize();
    } else {
      flexible_tracks_indexes.push_back(track_index);
      flex_factor_sum += track_size.MaxTrackBreadth().Flex();
    }
  }
  // We don't remove the gutters from left_over_space here, because that was
  // already done before.

  // The function is not called if we don't have <flex> grid tracks.
  DCHECK(!flexible_tracks_indexes.empty());

  return ComputeFlexFactorUnitSize(all_tracks, flex_factor_sum, left_over_space,
                                   flexible_tracks_indexes);
}

double GridTrackSizingAlgorithm::ComputeFlexFactorUnitSize(
    const Vector<GridTrack>& tracks,
    double flex_factor_sum,
    LayoutUnit& left_over_space,
    const Vector<wtf_size_t, 8>& flexible_tracks_indexes,
    std::unique_ptr<TrackIndexSet> tracks_to_treat_as_inflexible) const {
  // We want to avoid the effect of flex factors sum below 1 making the factor
  // unit size to grow exponentially.
  double hypothetical_factor_unit_size =
      left_over_space / std::max<double>(1, flex_factor_sum);

  // product of the hypothetical "flex factor unit" and any flexible track's
  // "flex factor" must be grater than such track's "base size".
  std::unique_ptr<TrackIndexSet> additional_tracks_to_treat_as_inflexible =
      std::move(tracks_to_treat_as_inflexible);
  bool valid_flex_factor_unit = true;
  for (auto index : flexible_tracks_indexes) {
    if (additional_tracks_to_treat_as_inflexible &&
        additional_tracks_to_treat_as_inflexible->Contains(index))
      continue;
    LayoutUnit base_size = tracks[index].BaseSize();
    double flex_factor =
        tracks[index].CachedTrackSize().MaxTrackBreadth().Flex();
    // treating all such tracks as inflexible.
    if (base_size > hypothetical_factor_unit_size * flex_factor) {
      left_over_space -= base_size;
      flex_factor_sum -= flex_factor;
      if (!additional_tracks_to_treat_as_inflexible) {
        additional_tracks_to_treat_as_inflexible =
            std::make_unique<TrackIndexSet>();
      }
      additional_tracks_to_treat_as_inflexible->insert(index);
      valid_flex_factor_unit = false;
    }
  }
  if (!valid_flex_factor_unit) {
    return ComputeFlexFactorUnitSize(
        tracks, flex_factor_sum, left_over_space, flexible_tracks_indexes,
        std::move(additional_tracks_to_treat_as_inflexible));
  }
  return hypothetical_factor_unit_size;
}

void GridTrackSizingAlgorithm::ComputeFlexSizedTracksGrowth(
    double flex_fraction,
    Vector<LayoutUnit>& increments,
    LayoutUnit& total_growth) const {
  wtf_size_t num_flex_tracks = flexible_sized_tracks_index_.size();
  DCHECK_EQ(increments.size(), num_flex_tracks);
  const Vector<GridTrack>& all_tracks = Tracks(direction_);
  for (wtf_size_t i = 0; i < num_flex_tracks; ++i) {
    wtf_size_t track_index = flexible_sized_tracks_index_[i];
    const GridTrackSize& track_size = all_tracks[track_index].CachedTrackSize();
    DCHECK(track_size.MaxTrackBreadth().IsFlex());
    LayoutUnit old_base_size = all_tracks[track_index].BaseSize();
    LayoutUnit new_base_size = std::max(
        old_base_size,
        LayoutUnit(flex_fraction * track_size.MaxTrackBreadth().Flex()));
    increments[i] = new_base_size - old_base_size;
    total_growth += increments[i];
  }
}

void GridTrackSizingAlgorithm::StretchFlexibleTracks(
    absl::optional<LayoutUnit> free_space) {
  if (flexible_sized_tracks_index_.empty())
    return;

  double flex_fraction = strategy_->FindUsedFlexFraction(
      flexible_sized_tracks_index_, direction_, free_space);

  LayoutUnit total_growth;
  Vector<LayoutUnit> increments;
  increments.Grow(flexible_sized_tracks_index_.size());
  ComputeFlexSizedTracksGrowth(flex_fraction, increments, total_growth);

  if (strategy_->RecomputeUsedFlexFractionIfNeeded(flex_fraction, increments,
                                                   total_growth)) {
    total_growth = LayoutUnit(0);
    ComputeFlexSizedTracksGrowth(flex_fraction, increments, total_growth);
  }

  wtf_size_t i = 0;
  Vector<GridTrack>& all_tracks = Tracks(direction_);
  for (auto track_index : flexible_sized_tracks_index_) {
    auto& track = all_tracks[track_index];
    if (LayoutUnit increment = increments[i++])
      track.SetBaseSize(track.BaseSize() + increment);
  }
  if (FreeSpace(direction_)) {
    SetFreeSpace(direction_, FreeSpace(direction_).value() - total_growth);
  }
  max_content_size_ += total_growth;
}

void GridTrackSizingAlgorithm::StretchAutoTracks() {
  LayoutUnit free_space = strategy_->FreeSpaceForStretchAutoTracksStep();
  if (auto_sized_tracks_for_stretch_index_.empty() || (free_space <= 0) ||
      (layout_grid_->ContentAlignment(direction_).Distribution() !=
       ContentDistributionType::kStretch))
    return;

  unsigned number_of_auto_sized_tracks =
      auto_sized_tracks_for_stretch_index_.size();
  LayoutUnit size_to_increase = free_space / number_of_auto_sized_tracks;
  Vector<GridTrack>& all_tracks = Tracks(direction_);
  for (const auto& track_index : auto_sized_tracks_for_stretch_index_) {
    auto& track = all_tracks[track_index];
    LayoutUnit base_size = track.BaseSize() + size_to_increase;
    track.SetBaseSize(base_size);
  }
  SetFreeSpace(direction_, LayoutUnit());
}

void GridTrackSizingAlgorithm::AdvanceNextState() {
  switch (sizing_state_) {
    case kColumnSizingFirstIteration:
      sizing_state_ = kRowSizingFirstIteration;
      return;
    case kRowSizingFirstIteration:
      if (!strategy_->IsComputingSizeContainment())
        sizing_state_ = kColumnSizingSecondIteration;
      return;
    case kColumnSizingSecondIteration:
      sizing_state_ = kRowSizingSecondIteration;
      return;
    case kRowSizingSecondIteration:
      sizing_state_ = kColumnSizingFirstIteration;
      return;
  }
  NOTREACHED();
  sizing_state_ = kColumnSizingFirstIteration;
}

bool GridTrackSizingAlgorithm::IsValidTransition() const {
  switch (sizing_state_) {
    case kColumnSizingFirstIteration:
    case kColumnSizingSecondIteration:
      return direction_ == kForColumns;
    case kRowSizingFirstIteration:
    case kRowSizingSecondIteration:
      return direction_ == kForRows;
  }
  NOTREACHED();
  return false;
}

void GridTrackSizingAlgorithm::Setup(
    GridTrackSizingDirection direction,
    wtf_size_t num_tracks,
    absl::optional<LayoutUnit> available_space) {
  DCHECK(needs_setup_);
  direction_ = direction;
  SetAvailableSpace(
      direction, available_space ? available_space.value().ClampNegativeToZero()
                                 : available_space);

  if (available_space)
    strategy_ = MakeGarbageCollected<DefiniteSizeStrategy>(*this);
  else
    strategy_ = MakeGarbageCollected<IndefiniteSizeStrategy>(*this);

  content_sized_tracks_index_.Shrink(0);
  flexible_sized_tracks_index_.Shrink(0);
  auto_sized_tracks_for_stretch_index_.Shrink(0);
  has_percent_sized_rows_indefinite_height_ = false;

  if (available_space) {
    LayoutUnit gutters_size = layout_grid_->GuttersSize(
        *grid_, direction, 0, grid_->NumTracks(direction), available_space);
    SetFreeSpace(direction, available_space.value() - gutters_size);
  } else {
    SetFreeSpace(direction, absl::nullopt);
  }
  Tracks(direction).resize(num_tracks);

  ComputeBaselineAlignmentContext();

  needs_setup_ = false;
}

void GridTrackSizingAlgorithm::ComputeBaselineAlignmentContext() {
  GridAxis axis = GridAxisForDirection(direction_);
  baseline_alignment_.Clear(axis);
  baseline_alignment_.SetBlockFlow(layout_grid_->StyleRef().GetWritingMode());
  BaselineItemsCache& baseline_items_cache = axis == kGridColumnAxis
                                                 ? column_baseline_items_map_
                                                 : row_baseline_items_map_;
  for (auto& child : baseline_items_cache.Keys()) {
    // TODO (jfernandez): We may have to get rid of the baseline participation
    // flag (hence just using a HashSet) depending on the CSS WG resolution on
    // https://github.com/w3c/csswg-drafts/issues/3046
    if (CanParticipateInBaselineAlignment(*child, axis)) {
      UpdateBaselineAlignmentContext(*child, axis);
      baseline_items_cache.Set(child, true);
    } else {
      baseline_items_cache.Set(child, false);
    }
  }
}

// Described in https://drafts.csswg.org/css-grid/#algo-track-sizing
void GridTrackSizingAlgorithm::Run() {
  DCHECK(WasSetup());
  StateMachine state_machine(*this);

  // Step 1.
  absl::optional<LayoutUnit> initial_free_space = FreeSpace(direction_);
  InitializeTrackSizes();

  if (strategy_->IsComputingSizeContainment()) {
    ComputeGridContainerIntrinsicSizes();
    return;
  }

  // Step 2.
  if (!content_sized_tracks_index_.empty())
    ResolveIntrinsicTrackSizes();

  // This is not exactly a step of the track sizing algorithm, but we use the
  // track sizes computed
  // up to this moment (before maximization) to calculate the grid container
  // intrinsic sizes.
  ComputeGridContainerIntrinsicSizes();

  if (FreeSpace(direction_)) {
    LayoutUnit updated_free_space =
        FreeSpace(direction_).value() - min_content_size_;
    SetFreeSpace(direction_, updated_free_space);
    if (updated_free_space <= 0)
      return;
  }

  // Step 3.
  strategy_->MaximizeTracks(Tracks(direction_), direction_ == kForColumns
                                                    ? free_space_columns_
                                                    : free_space_rows_);

  // Step 4.
  StretchFlexibleTracks(initial_free_space);

  // Step 5.
  StretchAutoTracks();
}

void GridTrackSizingAlgorithm::Reset() {
  DCHECK(WasSetup());
  sizing_state_ = kColumnSizingFirstIteration;
  columns_.Shrink(0);
  rows_.Shrink(0);
  content_sized_tracks_index_.Shrink(0);
  flexible_sized_tracks_index_.Shrink(0);
  auto_sized_tracks_for_stretch_index_.Shrink(0);
  has_percent_sized_rows_indefinite_height_ = false;
  SetAvailableSpace(kForRows, absl::nullopt);
  SetAvailableSpace(kForColumns, absl::nullopt);
}

#if DCHECK_IS_ON()
bool GridTrackSizingAlgorithm::TracksAreWiderThanMinTrackBreadth() const {
  const Vector<GridTrack>& all_tracks = Tracks(direction_);
  for (const auto& all_track : all_tracks) {
    const GridTrackSize& track_size = all_track.CachedTrackSize();
    if (InitialBaseSize(track_size) > all_track.BaseSize())
      return false;
  }
  return true;
}
#endif

GridTrackSizingAlgorithm::StateMachine::StateMachine(
    GridTrackSizingAlgorithm& algorithm)
    : algorithm_(algorithm) {
  DCHECK(algorithm_.IsValidTransition());
  DCHECK(!algorithm_.needs_setup_);
}

GridTrackSizingAlgorithm::StateMachine::~StateMachine() {
  algorithm_.AdvanceNextState();
  algorithm_.needs_setup_ = true;
}

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::GridItemWithSpan)
