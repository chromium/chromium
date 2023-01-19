// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_TRACK_SIZING_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_TRACK_SIZING_ALGORITHM_H_

#include <memory>

#include "base/dcheck_is_on.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/layout/grid_baseline_alignment.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/style/grid_positions_resolver.h"
#include "third_party/blink/renderer/core/style/grid_track_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

static const int kInfinity = -1;

class Grid;
class GridTrackSizingAlgorithmStrategy;
class LayoutGrid;

enum TrackSizeComputationPhase {
  kResolveIntrinsicMinimums,
  kResolveContentBasedMinimums,
  kResolveMaxContentMinimums,
  kResolveIntrinsicMaximums,
  kResolveMaxContentMaximums,
  kMaximizeTracks,
};

class GridTrack {
  DISALLOW_NEW();

 public:
  LayoutUnit BaseSize() const;
  void SetBaseSize(LayoutUnit);

  LayoutUnit GrowthLimit() const;
  bool GrowthLimitIsInfinite() const { return growth_limit_ == kInfinity; }
  void SetGrowthLimit(LayoutUnit);

  bool InfiniteGrowthPotential() const;

  LayoutUnit PlannedSize() const { return planned_size_; }
  void SetPlannedSize(LayoutUnit);

  LayoutUnit SizeDuringDistribution() const {
    return size_during_distribution_;
  }
  void SetSizeDuringDistribution(LayoutUnit);

  void GrowSizeDuringDistribution(LayoutUnit);

  bool InfinitelyGrowable() const { return infinitely_growable_; }
  void SetInfinitelyGrowable(bool);

  absl::optional<LayoutUnit> GrowthLimitCap() const {
    return growth_limit_cap_;
  }
  void SetGrowthLimitCap(absl::optional<LayoutUnit>);

  const GridTrackSize& CachedTrackSize() const {
    DCHECK(cached_track_size_.has_value());
    return cached_track_size_.value();
  }
  void SetCachedTrackSize(const GridTrackSize&);

 private:
  bool IsGrowthLimitBiggerThanBaseSize() const;
  void EnsureGrowthLimitIsBiggerThanBaseSize();

  LayoutUnit base_size_;
  LayoutUnit growth_limit_;
  LayoutUnit planned_size_;
  LayoutUnit size_during_distribution_;
  absl::optional<LayoutUnit> growth_limit_cap_;
  bool infinitely_growable_ = false;
  absl::optional<GridTrackSize> cached_track_size_;
};

class GridTrackSizingAlgorithm final
    : public GarbageCollected<GridTrackSizingAlgorithm> {
  friend class GridTrackSizingAlgorithmStrategy;

 public:
  GridTrackSizingAlgorithm(const LayoutGrid* layout_grid, Grid& grid)
      : grid_(grid),
        layout_grid_(layout_grid),
        sizing_state_(kColumnSizingFirstIteration) {}

  // Setup() must be run before calling Run() as it configures the behaviour of
  // the algorithm.
  void Setup(GridTrackSizingDirection,
             wtf_size_t num_tracks,
             absl::optional<LayoutUnit> available_space);
  void Run();
  void Reset();

  // Required by LayoutGrid. Try to minimize the exposed surface.
  const Grid& GetGrid() const { return *grid_; }
  // TODO (jfernandez): We should remove any public getter for this attribute
  // and encapsulate any access in the algorithm class.
  Grid& GetMutableGrid() const { return *grid_; }
  LayoutUnit MinContentSize() const { return min_content_size_; }
  LayoutUnit MaxContentSize() const { return max_content_size_; }

  LayoutUnit BaselineOffsetForChild(const LayoutBox&, GridAxis) const;

  void CacheBaselineAlignedItem(const LayoutBox&, GridAxis);
  void CopyBaselineItemsCache(const GridTrackSizingAlgorithm*, GridAxis);
  void ClearBaselineItemsCache();

  LayoutSize EstimatedGridAreaBreadthForChild(const LayoutBox& child) const;

  Vector<GridTrack>& Tracks(GridTrackSizingDirection);
  const Vector<GridTrack>& Tracks(GridTrackSizingDirection) const;

  absl::optional<LayoutUnit> FreeSpace(GridTrackSizingDirection) const;
  void SetFreeSpace(GridTrackSizingDirection, absl::optional<LayoutUnit>);

  absl::optional<LayoutUnit> AvailableSpace(GridTrackSizingDirection) const;
  void SetAvailableSpace(GridTrackSizingDirection, absl::optional<LayoutUnit>);

#if DCHECK_IS_ON()
  bool TracksAreWiderThanMinTrackBreadth() const;
#endif

  LayoutUnit ComputeTrackBasedSize() const;

  bool HasAnyPercentSizedRowsIndefiniteHeight() const {
    return has_percent_sized_rows_indefinite_height_;
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(grid_);
    visitor->Trace(layout_grid_);
    visitor->Trace(strategy_);
    visitor->Trace(baseline_alignment_);
    visitor->Trace(column_baseline_items_map_);
    visitor->Trace(row_baseline_items_map_);
  }

 private:
  absl::optional<LayoutUnit> AvailableSpace() const;
  bool IsRelativeGridLengthAsAuto(const GridLength&,
                                  GridTrackSizingDirection) const;
  bool IsRelativeSizedTrackAsAuto(const GridTrackSize&,
                                  GridTrackSizingDirection) const;
  GridTrackSize CalculateGridTrackSize(GridTrackSizingDirection,
                                       wtf_size_t translated_index) const;
  const GridTrackSize& RawGridTrackSize(GridTrackSizingDirection,
                                        wtf_size_t translated_index) const;

  // Helper methods for step 1. initializeTrackSizes().
  LayoutUnit InitialBaseSize(const GridTrackSize&) const;
  LayoutUnit InitialGrowthLimit(const GridTrackSize&,
                                LayoutUnit base_size) const;

  // Helper methods for step 2. resolveIntrinsicTrackSizes().
  void SizeTrackToFitNonSpanningItem(const GridSpan&,
                                     LayoutBox& grid_item,
                                     GridTrack&);
  bool SpanningItemCrossesFlexibleSizedTracks(const GridSpan&) const;
  typedef class GridItemWithSpan GridItemWithSpan;
  template <TrackSizeComputationPhase phase>
  void IncreaseSizesToAccommodateSpanningItems(
      const HeapVector<GridItemWithSpan>::iterator& grid_items_with_span_begin,
      const HeapVector<GridItemWithSpan>::iterator& grid_items_with_span_end);
  LayoutUnit ItemSizeForTrackSizeComputationPhase(TrackSizeComputationPhase,
                                                  LayoutBox&) const;
  template <TrackSizeComputationPhase phase>
  void DistributeSpaceToTracks(
      Vector<GridTrack*>& tracks,
      Vector<GridTrack*>* grow_beyond_growth_limits_tracks,
      LayoutUnit& available_logical_space) const;
  LayoutUnit EstimatedGridAreaBreadthForChild(const LayoutBox&,
                                              GridTrackSizingDirection) const;
  LayoutUnit GridAreaBreadthForChild(const LayoutBox&,
                                     GridTrackSizingDirection) const;

  void ComputeBaselineAlignmentContext();
  void UpdateBaselineAlignmentContext(const LayoutBox&, GridAxis);
  bool CanParticipateInBaselineAlignment(const LayoutBox&, GridAxis) const;
  bool ParticipateInBaselineAlignment(const LayoutBox&, GridAxis) const;

  bool IsIntrinsicSizedGridArea(const LayoutBox&, GridAxis) const;
  void ComputeGridContainerIntrinsicSizes();

  // Helper methods for step 4. Stretch flexible tracks.
  typedef HashSet<size_t, IntWithZeroKeyHashTraits<size_t>> TrackIndexSet;
  double ComputeFlexFactorUnitSize(
      const Vector<GridTrack>& tracks,
      double flex_factor_sum,
      LayoutUnit& left_over_space,
      const Vector<wtf_size_t, 8>& flexible_tracks_indexes,
      std::unique_ptr<TrackIndexSet> tracks_to_treat_as_inflexible =
          nullptr) const;
  void ComputeFlexSizedTracksGrowth(double flex_fraction,
                                    Vector<LayoutUnit>& increments,
                                    LayoutUnit& total_growth) const;
  double FindFrUnitSize(const GridSpan& tracks_span,
                        LayoutUnit left_over_space) const;

  // Track sizing algorithm steps. Note that the "Maximize Tracks" step is done
  // entirely inside the strategies, that's why we don't need an additional
  // method at thise level.
  void InitializeTrackSizes();
  void ResolveIntrinsicTrackSizes();
  void StretchFlexibleTracks(absl::optional<LayoutUnit> free_space);
  void StretchAutoTracks();

  // State machine.
  void AdvanceNextState();
  bool IsValidTransition() const;

  // Data.
  bool WasSetup() const { return !!strategy_; }
  bool needs_setup_{true};
  bool has_percent_sized_rows_indefinite_height_{false};
  absl::optional<LayoutUnit> available_space_columns_;
  absl::optional<LayoutUnit> available_space_rows_;

  absl::optional<LayoutUnit> free_space_columns_;
  absl::optional<LayoutUnit> free_space_rows_;

  // We need to keep both alive in order to properly size grids with orthogonal
  // writing modes.
  Vector<GridTrack> columns_;
  Vector<GridTrack> rows_;
  Vector<wtf_size_t> content_sized_tracks_index_;
  Vector<wtf_size_t> flexible_sized_tracks_index_;
  Vector<wtf_size_t> auto_sized_tracks_for_stretch_index_;

  GridTrackSizingDirection direction_;

  Member<Grid> grid_;
  Member<const LayoutGrid> layout_grid_;
  Member<GridTrackSizingAlgorithmStrategy> strategy_;

  // The track sizing algorithm is used for both layout and intrinsic size
  // computation. We're normally just interested in intrinsic inline sizes
  // (a.k.a widths in most of the cases) for the computeIntrinsicLogicalWidths()
  // computations. That's why we don't need to keep around different values for
  // rows/columns.
  LayoutUnit min_content_size_;
  LayoutUnit max_content_size_;

  enum SizingState {
    kColumnSizingFirstIteration,
    kRowSizingFirstIteration,
    kColumnSizingSecondIteration,
    kRowSizingSecondIteration
  };
  SizingState sizing_state_;

  GridBaselineAlignment baseline_alignment_;

  using BaselineItemsCache = HeapHashMap<Member<const LayoutBox>, bool>;
  BaselineItemsCache column_baseline_items_map_;
  BaselineItemsCache row_baseline_items_map_;

  // This is a RAII class used to ensure that the track sizing algorithm is
  // executed as it is suppossed to be, i.e., first resolve columns and then
  // rows. Only if required a second iteration is run following the same order,
  // first columns and then rows.
  class StateMachine {
    STACK_ALLOCATED();

   public:
    explicit StateMachine(GridTrackSizingAlgorithm&);
    ~StateMachine();

   private:
    GridTrackSizingAlgorithm& algorithm_;
  };
};

class GridTrackSizingAlgorithmStrategy
    : public GarbageCollected<GridTrackSizingAlgorithmStrategy> {
 public:
  GridTrackSizingAlgorithmStrategy(const GridTrackSizingAlgorithmStrategy&) =
      delete;
  GridTrackSizingAlgorithmStrategy& operator=(
      const GridTrackSizingAlgorithmStrategy&) = delete;
  virtual ~GridTrackSizingAlgorithmStrategy();

  virtual LayoutUnit MinContentForChild(LayoutBox&) const;
  virtual LayoutUnit MaxContentForChild(LayoutBox&) const;
  LayoutUnit MinSizeForChild(LayoutBox&) const;

  virtual void MaximizeTracks(Vector<GridTrack>&,
                              absl::optional<LayoutUnit>& free_space) = 0;
  virtual double FindUsedFlexFraction(
      Vector<wtf_size_t>& flexible_sized_tracks_index,
      GridTrackSizingDirection,
      absl::optional<LayoutUnit> initial_free_space) const = 0;
  virtual bool RecomputeUsedFlexFractionIfNeeded(
      double& flex_fraction,
      Vector<LayoutUnit>& increments,
      LayoutUnit& total_growth) const = 0;
  virtual LayoutUnit FreeSpaceForStretchAutoTracksStep() const = 0;
  virtual bool IsComputingSizeContainment() const = 0;
  virtual void Trace(Visitor* visitor) const { visitor->Trace(algorithm_); }

 protected:
  explicit GridTrackSizingAlgorithmStrategy(GridTrackSizingAlgorithm& algorithm)
      : algorithm_(algorithm) {}

  virtual LayoutUnit MinLogicalSizeForChild(LayoutBox&,
                                            const Length& child_min_size,
                                            LayoutUnit available_size) const;
  virtual void LayoutGridItemForMinSizeComputation(
      LayoutBox&,
      bool override_size_has_changed) const = 0;

  LayoutUnit LogicalHeightForChild(LayoutBox&) const;

  bool UpdateOverrideContainingBlockContentSizeForChild(
      LayoutBox&,
      GridTrackSizingDirection,
      absl::optional<LayoutUnit> = absl::nullopt) const;
  LayoutUnit ComputeTrackBasedSize() const;

  GridTrackSizingDirection Direction() const { return algorithm_->direction_; }
  double FindFrUnitSize(const GridSpan& tracks_span,
                        LayoutUnit left_over_space) const;
  void DistributeSpaceToTracks(Vector<GridTrack*>& tracks,
                               LayoutUnit& available_logical_space) const;
  const LayoutGrid* GetLayoutGrid() const { return algorithm_->layout_grid_; }
  absl::optional<LayoutUnit> AvailableSpace() const {
    return algorithm_->AvailableSpace();
  }

  // Helper functions
  static bool HasRelativeMarginOrPaddingForChild(const LayoutGrid&,
                                                 const LayoutBox& child,
                                                 GridTrackSizingDirection);
  static bool HasRelativeOrIntrinsicSizeForChild(const LayoutGrid&,
                                                 const LayoutBox& child,
                                                 GridTrackSizingDirection);
  static bool ShouldClearOverrideContainingBlockContentSizeForChild(
      const LayoutGrid&,
      const LayoutBox& child,
      GridTrackSizingDirection);
  static void SetOverrideContainingBlockContentSizeForChild(
      LayoutBox& child,
      GridTrackSizingDirection,
      LayoutUnit size);

  Member<GridTrackSizingAlgorithm> algorithm_;
};
}

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_TRACK_SIZING_ALGORITHM_H_
