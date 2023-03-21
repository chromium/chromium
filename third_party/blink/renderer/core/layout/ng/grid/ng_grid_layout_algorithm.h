// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_break_token_data.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_node.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_placement.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_sizing_tree.h"
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

using GridItemDataPtrVector = Vector<GridItemData*, 16>;
using GridSetPtrVector = Vector<NGGridSet*, 16>;

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
      const NGGridPlacementData& placement_data,
      const NGGridLayoutData& layout_data,
      const ComputedStyle& grid_style,
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

  // Aggregate all direct out of flow children from the current grid container
  // to |oof_children|, unless |oof_children| is not provided.
  wtf_size_t BuildGridSizingSubtree(
      NGGridSizingTree* sizing_tree,
      HeapVector<Member<LayoutBox>>* oof_children = nullptr,
      const NGSubgriddedItemData& opt_subgrid_data = kNoSubgriddedItemData,
      const NGGridLineResolver* parent_line_resolver = nullptr,
      bool must_ignore_children = false) const;

  NGGridSizingTree BuildGridSizingTree(
      HeapVector<Member<LayoutBox>>* oof_children = nullptr) const;
  NGGridSizingTree BuildGridSizingTreeIgnoringChildren() const;

  const NGLayoutResult* LayoutInternal();

  LayoutUnit Baseline(const NGGridLayoutData& layout_data,
                      const GridItemData& grid_item,
                      GridTrackSizingDirection track_direction) const;

  void ComputeGridGeometry(NGGridSizingTree* grid_sizing_tree,
                           LayoutUnit* intrinsic_block_size);

  LayoutUnit ComputeIntrinsicBlockSizeIgnoringChildren() const;

  LayoutUnit GetLogicalBaseline(const NGBoxFragment&,
                                const bool is_last_baseline) const;
  LayoutUnit GetSynthesizedLogicalBaseline(const LayoutUnit block_size,
                                           const bool is_flipped_lines,
                                           const bool is_last_baseline) const;

  // Returns the size that a grid item will distribute across the tracks with an
  // intrinsic sizing function it spans in the relevant track direction.
  LayoutUnit ContributionSizeForGridItem(
      const NGGridLayoutData& layout_data,
      GridItemContributionType contribution_type,
      GridTrackSizingDirection track_direction,
      SizingConstraint sizing_constraint,
      GridItemData* grid_item) const;

  wtf_size_t ComputeAutomaticRepetitions(
      const absl::optional<GridArea>& subgrid_area,
      GridTrackSizingDirection track_direction) const;

  // Subgrids compute auto repetitions differently than standalone grids.
  wtf_size_t ComputeAutomaticRepetitionsForSubgrid(
      wtf_size_t subgrid_span_size,
      GridTrackSizingDirection track_direction) const;

  // Determines the major/minor alignment baselines for each row/column based on
  // each item in |grid_items|, and stores the results in |track_collection|.
  void CalculateAlignmentBaselines(
      const NGGridLayoutData& layout_data,
      SizingConstraint sizing_constraint,
      GridItems* grid_items,
      NGGridSizingTrackCollection* track_collection,
      bool* needs_additional_pass = nullptr) const;

  // Initialize the track collections of a given grid sizing data.
  void InitializeTrackCollection(const NGSubgriddedItemData& opt_subgrid_data,
                                 GridTrackSizingDirection track_direction,
                                 NGGridLayoutData* layout_data) const;

  // Initializes the track sizes of a given grid sizing subtree.
  void InitializeTrackSizes(
      wtf_size_t current_grid_index,
      const NGSubgriddedItemData& opt_subgrid_data,
      const absl::optional<GridTrackSizingDirection>& opt_track_direction,
      NGGridSizingTree* sizing_tree) const;

  // Helper that calls the method above for the entire grid sizing tree.
  void InitializeTrackSizes(NGGridSizingTree* sizing_tree,
                            const absl::optional<GridTrackSizingDirection>&
                                opt_track_direction = absl::nullopt) const;

  // Calculates from the min and max track sizing functions the used track size.
  void ComputeUsedTrackSizes(const NGGridLayoutData& layout_data,
                             SizingConstraint sizing_constraint,
                             GridItems* grid_items,
                             NGGridLayoutTrackCollection* track_collection,
                             bool* needs_additional_pass = nullptr) const;

  // These methods implement the steps of the algorithm for intrinsic track size
  // resolution defined in https://drafts.csswg.org/css-grid-2/#algo-content.
  void ResolveIntrinsicTrackSizes(const NGGridLayoutData& layout_data,
                                  SizingConstraint sizing_constraint,
                                  NGGridSizingTrackCollection* track_collection,
                                  GridItems* grid_items) const;

  void IncreaseTrackSizesToAccommodateGridItems(
      GridItemDataPtrVector::iterator group_begin,
      GridItemDataPtrVector::iterator group_end,
      const NGGridLayoutData& layout_data,
      const bool is_group_spanning_flex_track,
      SizingConstraint sizing_constraint,
      GridItemContributionType contribution_type,
      NGGridSizingTrackCollection* track_collection) const;

  void MaximizeTracks(SizingConstraint sizing_constraint,
                      NGGridSizingTrackCollection* track_collection) const;

  void StretchAutoTracks(SizingConstraint sizing_constraint,
                         NGGridSizingTrackCollection* track_collection) const;

  void ExpandFlexibleTracks(const NGGridLayoutData& layout_data,
                            SizingConstraint sizing_constraint,
                            NGGridSizingTrackCollection* track_collection,
                            GridItems* grid_items) const;

  // Gets the specified [column|row]-gap of the grid.
  LayoutUnit GutterSize(GridTrackSizingDirection track_direction) const;

  LayoutUnit DetermineFreeSpace(
      SizingConstraint sizing_constraint,
      const NGGridSizingTrackCollection& track_collection) const;

  NGConstraintSpace CreateConstraintSpace(
      NGCacheSlot cache_slot,
      const GridItemData& grid_item,
      const NGGridLayoutData& layout_data,
      const LogicalSize& containing_grid_area_size,
      absl::optional<LayoutUnit> opt_fixed_block_size,
      NGGridLayoutSubtree&& layout_subtree,
      bool min_block_size_should_encompass_intrinsic_size = false,
      absl::optional<LayoutUnit> opt_fragment_relative_block_offset =
          absl::nullopt) const;

  NGConstraintSpace CreateConstraintSpaceForLayout(
      const GridItemData& grid_item,
      const NGGridLayoutData& layout_data,
      LogicalRect* containing_grid_area,
      NGGridLayoutSubtree&& layout_subtree = NGGridLayoutSubtree(),
      bool min_block_size_should_encompass_intrinsic_size = false,
      absl::optional<LayoutUnit> opt_fragment_relative_block_offset =
          absl::nullopt) const;

  NGConstraintSpace CreateConstraintSpaceForMeasure(
      const GridItemData& grid_item,
      const NGGridLayoutData& layout_data,
      GridTrackSizingDirection track_direction,
      absl::optional<LayoutUnit> opt_fixed_block_size = absl::nullopt) const;

  NGGridLayoutAlgorithm CreateSubgridLayoutAlgorithm(
      const NGSubgriddedItemData& subgrid_data,
      NGConstraintSpace* constraint_space,
      NGFragmentGeometry* fragment_geometry) const;

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
      const NGGridLayoutSubtree& layout_subtree,
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
  // elements in the grid (as provided by `oof_children`).
  void PlaceOutOfFlowItems(const NGGridLayoutData& layout_data,
                           const LayoutUnit block_size,
                           HeapVector<Member<LayoutBox>>& oof_children);

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
