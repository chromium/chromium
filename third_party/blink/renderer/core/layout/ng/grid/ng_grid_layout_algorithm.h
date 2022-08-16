// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_break_token_data.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_data.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_item.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_node.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_placement.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_properties.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_track_collection.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class NGBoxFragment;

// This enum corresponds to each step used to accommodate grid items across
// intrinsic tracks according to their min and max track sizing functions, as
// defined in https://drafts.csswg.org/css-grid-2/#algo-spanning-items.
enum class GridItemContributionType {
  kForIntrinsicMinimums,
  kForContentBasedMinimums,
  kForMaxContentMinimums,
  kForIntrinsicMaximums,
  kForMaxContentMaximums,
  kForFreeSpace,
};

class CORE_EXPORT NGGridLayoutAlgorithm
    : public NGLayoutAlgorithm<NGGridNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  explicit NGGridLayoutAlgorithm(const NGLayoutAlgorithmParams& params);

  const NGLayoutResult* Layout() override;
  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesFloatInput&) override;

  // Computes the containing block rect of out of flow items from stored data in
  // |NGGridLayoutData|.
  static LogicalRect ComputeOutOfFlowItemContainingRect(
      const NGGridPlacement& grid_placement,
      const NGGridLayoutData& layout_data,
      const NGBoxStrut& borders,
      const LogicalSize& border_box_size,
      GridItemData* out_of_flow_item);

  // Helper that computes tracks sizes in a given range.
  static Vector<std::div_t> ComputeTrackSizesInRange(
      const NGGridLayoutTrackCollection& track_collection,
      wtf_size_t range_starting_set_index,
      wtf_size_t range_set_count);

 private:
  friend class NGGridLayoutAlgorithmTest;

  const NGLayoutResult* LayoutInternal();

  LayoutUnit Baseline(const NGGridLayoutData& layout_data,
                      const GridItemData& grid_item,
                      const GridTrackSizingDirection track_direction) const;

  void ComputeGridGeometry(const NGGridPlacementData& placement_data,
                           GridItems* grid_items,
                           NGGridLayoutData* layout_data,
                           LayoutUnit* intrinsic_block_size);

  LayoutUnit ComputeIntrinsicBlockSizeIgnoringChildren() const;

  LayoutUnit GetLogicalBaseline(const NGBoxFragment&, BaselineGroup) const;

  // Returns the size that a grid item will distribute across the tracks with an
  // intrinsic sizing function it spans in the relevant track direction.
  LayoutUnit ContributionSizeForGridItem(
      const NGGridLayoutData& layout_data,
      const SizingConstraint sizing_constraint,
      const GridTrackSizingDirection track_direction,
      const GridItemContributionType contribution_type,
      GridItemData* grid_item) const;

  NGGridPlacementData PlacementData() const;

  std::unique_ptr<NGGridLayoutTrackCollection> LayoutTrackCollection(
      const NGGridPlacementData& placement_data,
      const GridTrackSizingDirection track_direction,
      GridItems* grid_items) const;

  wtf_size_t ComputeAutomaticRepetitions(
      const GridTrackSizingDirection track_direction) const;

  void BuildBlockTrackCollection(
      GridItems* grid_items,
      NGGridBlockTrackCollection* track_collection) const;

  // Determines the major/minor alignment baselines for each row/column based on
  // each item in |grid_items|, and stores the results in |track_collection|.
  void CalculateAlignmentBaselines(
      const NGGridLayoutData& layout_data,
      const SizingConstraint sizing_constraint,
      GridItems* grid_items,
      NGGridSizingTrackCollection* track_collection,
      bool* needs_additional_pass = nullptr) const;

  // Initializes the given track collection, and returns the base set geometry.
  void InitializeTrackSizes(
      const NGGridProperties& grid_properties,
      NGGridLayoutTrackCollection* track_collection) const;

  // Calculates from the min and max track sizing functions the used track size.
  void ComputeUsedTrackSizes(const NGGridLayoutData& layout_data,
                             const NGGridProperties& grid_properties,
                             const SizingConstraint sizing_constraint,
                             GridItems* grid_items,
                             NGGridLayoutTrackCollection* track_collection,
                             bool* needs_additional_pass = nullptr) const;

  // These methods implement the steps of the algorithm for intrinsic track size
  // resolution defined in https://drafts.csswg.org/css-grid-2/#algo-content.
  void ResolveIntrinsicTrackSizes(const NGGridLayoutData& layout_data,
                                  const SizingConstraint sizing_constraint,
                                  NGGridSizingTrackCollection* track_collection,
                                  GridItems* grid_items) const;

  void IncreaseTrackSizesToAccommodateGridItems(
      GridItems::Iterator group_begin,
      GridItems::Iterator group_end,
      const NGGridLayoutData& layout_data,
      const bool is_group_spanning_flex_track,
      const SizingConstraint sizing_constraint,
      const GridItemContributionType contribution_type,
      NGGridSizingTrackCollection* track_collection) const;

  void MaximizeTracks(const SizingConstraint sizing_constraint,
                      NGGridSizingTrackCollection* track_collection) const;

  void StretchAutoTracks(const SizingConstraint sizing_constraint,
                         NGGridSizingTrackCollection* track_collection) const;

  void ExpandFlexibleTracks(const NGGridLayoutData& layout_data,
                            const SizingConstraint sizing_constraint,
                            NGGridSizingTrackCollection* track_collection,
                            GridItems* grid_items) const;

  // Gets the specified [column|row]-gap of the grid.
  LayoutUnit GutterSize(const GridTrackSizingDirection track_direction) const;

  LayoutUnit DetermineFreeSpace(
      SizingConstraint sizing_constraint,
      const NGGridSizingTrackCollection& track_collection) const;

  const NGConstraintSpace CreateConstraintSpace(
      const GridItemData& grid_item,
      const NGGridLayoutData& layout_data,
      const LogicalSize& containing_grid_area_size,
      NGCacheSlot cache_slot,
      absl::optional<LayoutUnit> opt_fixed_block_size,
      absl::optional<LayoutUnit> opt_fragment_relative_block_offset =
          absl::nullopt,
      bool opt_min_block_size_should_encompass_intrinsic_size = false) const;

  const NGConstraintSpace CreateConstraintSpaceForLayout(
      const GridItemData& grid_item,
      const NGGridLayoutData& layout_data,
      LogicalRect* containing_grid_area,
      absl::optional<LayoutUnit> opt_fragment_relative_block_offset =
          absl::nullopt,
      bool opt_min_block_size_should_encompass_intrinsic_size = false) const;

  const NGConstraintSpace CreateConstraintSpaceForMeasure(
      const GridItemData& grid_item,
      const NGGridLayoutData& layout_data,
      const GridTrackSizingDirection track_direction,
      absl::optional<LayoutUnit> opt_fixed_block_size = absl::nullopt) const;

  // Layout the |grid_items|, and add them to the builder.
  //
  // If |out_grid_items_placement_data| is present determine the offset for
  // each of the |grid_items| but *don't* add the resulting fragment to the
  // builder.
  //
  // This is used for fragmentation which requires us to know the final offset
  // of each item before fragmentation occurs.
  void PlaceGridItems(
      const GridItems& grid_items,
      const NGGridLayoutData& layout_data,
      Vector<EBreakBetween>* out_row_break_between,
      Vector<GridItemPlacementData>* out_grid_items_placement_data = nullptr);

  // Layout the |grid_items| for fragmentation (when there is a known
  // fragmentainer size).
  //
  // This will go through all the grid_items and place fragments which belong
  // within this fragmentainer.
  void PlaceGridItemsForFragmentation(
      const GridItems& grid_items,
      const Vector<EBreakBetween>& row_break_between,
      NGGridLayoutData* layout_data,
      Vector<GridItemPlacementData>* grid_item_placement_data,
      Vector<LayoutUnit>* row_offset_adjustments,
      LayoutUnit* intrinsic_block_size,
      LayoutUnit* consumed_grid_block_size);

  // Computes the static position, grid area and its offset of out of flow
  // elements in the grid.
  void PlaceOutOfFlowItems(const NGGridLayoutData& layout_data,
                           const LayoutUnit block_size);

  void ComputeGridItemOffsetAndSize(
      const GridItemData& grid_item,
      const NGGridLayoutTrackCollection& track_collection,
      LayoutUnit* start_offset,
      LayoutUnit* size) const;

  LogicalSize grid_available_size_;
  LogicalSize grid_min_available_size_;
  LogicalSize grid_max_available_size_;

  absl::optional<LayoutUnit> contain_intrinsic_block_size_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_LAYOUT_ALGORITHM_H_
