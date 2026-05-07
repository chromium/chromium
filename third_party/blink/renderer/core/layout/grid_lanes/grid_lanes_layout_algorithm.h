// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_sizing_algorithm.h"
#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lanes_item_group.h"
#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lanes_node.h"
#include "third_party/blink/renderer/core/layout/layout_algorithm.h"

namespace blink {

class BaselineAccumulator;
class ComputedStyle;
class GridItems;
class GridLayoutData;
class GridLayoutTree;
class GridLineResolver;
class GridSizingTrackCollection;
class GridSizingSubtree;
class GridSizingTree;
class GridLanesRunningPositions;
class SubgriddedItemData;
enum class GridItemContributionType;
struct BoxStrut;
struct GridItemData;
struct GridPlacementData;

class CORE_EXPORT GridLanesLayoutAlgorithm
    : public LayoutAlgorithm<GridLanesNode,
                             BoxFragmentBuilder,
                             BlockBreakToken> {
 public:
  explicit GridLanesLayoutAlgorithm(const LayoutAlgorithmParams& params);

  // Expose base class accessors needed by functions in grid_layout_utils.
  using LayoutAlgorithm::BorderScrollbarPadding;
  using LayoutAlgorithm::GetConstraintSpace;
  using LayoutAlgorithm::Node;
  using LayoutAlgorithm::Style;

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesFloatInput&);
  const LayoutResult* Layout();

  // Computes the containing block rect for out-of-flow items placed
  // within the grid-lanes.
  static LogicalRect ComputeOutOfFlowItemContainingRect(
      const GridPlacementData& placement_data,
      const GridLayoutData& layout_data,
      const ComputedStyle& grid_lanes_style,
      const BoxStrut& borders,
      const LogicalSize& border_box_size,
      GridItemData* out_of_flow_item);

  // Builds the sizing collection for the given `track_direction`. For the
  // stacking axis this is a no-op. For the grid axis, this builds virtual items
  // and creates a track collection from virtual item contributions, setting the
  // track collection on `layout_data`. If `needs_intrinsic_track_size` is true,
  // that means that we are in the first track size pass required to compute
  // intrinsic track sizes within a repeat definition.
  void BuildSizingCollection(
      GridTrackSizingDirection track_direction,
      const GridLineResolver& line_resolver,
      GridItems& grid_items,
      GridLayoutData& layout_data,
      SizingConstraint sizing_constraint = SizingConstraint::kLayout,
      bool needs_intrinsic_track_size = false,
      VirtualItems** opt_virtual_items = nullptr) const;

  // `containing_grid_area` is an optional out parameter that holds the computed
  // grid area (offset and size) of the specified grid item.
  ConstraintSpace CreateConstraintSpaceForLayout(
      const SubgriddedItemData& subgridded_item,
      const GridLayoutSubtree* opt_layout_subtree = nullptr,
      LogicalRect* containing_grid_area = nullptr,
      LayoutUnit unavailable_block_size = LayoutUnit(),
      bool min_block_size_should_encompass_intrinsic_size = false,
      std::optional<LayoutUnit> opt_child_block_offset = std::nullopt,
      std::optional<LayoutUnit> opt_fixed_inline_size = std::nullopt) const;

  LogicalSize GetGridAvailableSize() const {
    return grid_lanes_available_size_;
  }

 private:
  friend class GridLanesLayoutAlgorithmTest;

  enum class PlacementPhase {
    kCalculateBaselines,
    kFinalPlacement,
  };

  // Builds the grid-lanes sizing tree, runs track sizing (including any
  // intrinsic repeat passes), and baseline alignment. Grid items are moved out
  // via the `grid_items` parameter. `opt_oof_children` is an optional vector of
  // out-of-flow direct children of the grid-lanes container.
  GridSizingTree ComputeGridLanesSizingTree(
      SizingConstraint sizing_constraint,
      bool should_apply_inline_size_containment,
      GridItems** grid_items,
      HeapVector<Member<LayoutBox>>* opt_oof_children = nullptr);

  // Computes the grid-lanes geometry by running track sizing (including any
  // intrinsic repeat passes), baseline alignment, and finalization. Returns
  // the finalized layout subtree. Grid items are moved out via the
  // `grid_items` parameter. `opt_oof_children` is an optional vector of
  // out-of-flow direct children of the grid-lanes container.
  GridLayoutSubtree* ComputeGridLanesGeometry(
      SizingConstraint sizing_constraint,
      bool should_apply_inline_size_containment,
      GridItems** grid_items,
      HeapVector<Member<LayoutBox>>* opt_oof_children = nullptr);

  // This places all the items in the sizing tree and adjusts
  // `intrinsic_block_size_` based on the placement of the items. Each item's
  // resolved position is translated based on the cached start offset.
  // Placement of the items is finalized within this method. `running_positions`
  // is an output parameter that can be used to find the intrinsic inline size
  // when the stacking axis is the inline axis. `opt_sizing_subtree` is required
  // when `sizing_constraint` is for measure so that subgridded item data can be
  // accessed for proper sizing.
  void PlaceGridLanesItems(
      GridItems& grid_items,
      const GridLayoutSubtree* layout_subtree,
      GridLayoutData& layout_data,
      GridLanesRunningPositions& running_positions,
      std::optional<SizingConstraint> sizing_constraint = std::nullopt,
      const GridSizingSubtree* opt_sizing_subtree = nullptr);

  // Iterates through and lays out each item in `grid_lanes_items`. If
  // `placement_phase` is kCalculateBaselines, this method measures items and
  // stores their baseline contributions to compute track baselines, but does
  // not add item layout results to the container. If `placement_phase` is
  // kFinalPlacement, this method performs final placement and alignment using
  // the previously computed track baselines, and adds item layout results to
  // the container. This ensures baseline information is available before items
  // are positioned. The `running_positions` output parameter tracks the
  // cumulative positions along the stacking axis for each track. The
  // `baseline_accumulator` output parameter accumulates container-level
  // baselines from the items. `opt_sizing_subtree` is required
  // when `sizing_constraint` is for measure so that subgridded item data can be
  // accessed for proper sizing.
  void RunGridLanesPlacementPhase(
      GridItems& grid_items,
      const GridLayoutSubtree* layout_subtree,
      GridLayoutData& layout_data,
      std::optional<SizingConstraint> sizing_constraint,
      LayoutUnit stacking_axis_gap,
      PlacementPhase placement_phase,
      BaselineAccumulator* baseline_accumulator,
      GridLanesRunningPositions& running_positions,
      const GridSizingSubtree* opt_sizing_subtree = nullptr);

  // Places all out-of-flow (OOF) grid-lanes items. For each item, this method
  // computes the size and location of the containing block rectangle within the
  // grid-lanes container, calculates alignment offsets using item alignment
  // properties, and adds the item as an out-of-flow candidate via
  // `AddOutOfFlowChildCandidate`. `oof_children` is a required input vector
  // containing the layout boxes of OOF grid-lanes items.
  void PlaceOutOfFlowItems(const GridLayoutData& layout_data,
                           LayoutUnit block_size,
                           HeapVector<Member<LayoutBox>>& oof_children);

  // Initializes the track sizes of a grid-lanes sizing subtree.
  void InitializeTrackSizes(const GridSizingSubtree& sizing_subtree,
                            const SubgriddedItemData& opt_subgrid_data) const;

  // Helper that calls the method above for the entire grid sizing tree.
  void InitializeTrackSizes(GridSizingTree* sizing_tree) const;

  // Creates a sizing tree based on the given `sizing_constraint` and
  // populates `sizing_tree` with the result. If
  // `should_apply_inline_size_containment` is true, build tracks without using
  // any items. `opt_oof_children` is an optional vector of out-of-flow direct
  // children of the grid-lanes container that this method will populate.
  // `needs_intrinsic_track_size` is an out parameter set to true if we hit an
  // intrinsic sized track within a repeat() definition and need to run an
  // additional track sizing pass to calculate the intrinsic track size.
  void ComputeSizingTreeInGridAxis(
      SizingConstraint sizing_constraint,
      const bool should_apply_inline_size_containment,
      GridSizingTree* sizing_tree,
      bool& needs_intrinsic_track_size,
      HeapVector<Member<LayoutBox>>* opt_oof_children = nullptr);

  // Completes the track sizing algorithm for non-definite tracks of a
  // grid-lanes sizing subtree.
  void CompleteTrackSizingAlgorithm(const GridSizingSubtree& sizing_subtree,
                                    SizingConstraint sizing_constraint,
                                    bool needs_intrinsic_track_size) const;

  // Helper that calls the method above for the entire grid sizing tree.
  void CompleteTrackSizingAlgorithm(SizingConstraint sizing_constraint,
                                    GridSizingTree* sizing_tree,
                                    bool needs_intrinsic_track_size) const;

  // Resolves non-definite track sizes for the grid axis.
  void ComputeUsedTrackSizes(const GridSizingSubtree& sizing_subtree,
                             SizingConstraint sizing_constraint,
                             bool needs_intrinsic_track_size) const;

  // Performs the final baseline alignment pass of a sizing subtree in the grid
  // axis.
  void ComputeBaselineAlignment(const GridLayoutTree* layout_tree,
                                const GridSizingSubtree& sizing_subtree);

  // Helper that calls the method above for the entire grid sizing tree.
  void CompleteFinalBaselineAlignment(GridSizingTree* sizing_tree);

  // Populate `sizing_tree` with the track sizes of an auto repeat that has
  // intrinsic track size(s). This method assumes that such an auto repeat
  // exists in the sizing tree's track collection.
  void CalculateIntrinsicTrackSizes(GridSizingTree& sizing_tree) const;

  // Given a `track_collection`, return all the track sizes of an auto repeat
  // that has intrinsic track size(s). This method assumes that such an auto
  // repeat exists in `track_collection`. `has_items` indicates whether there
  // are any grid-lanes items in the grid-lanes container.
  HashMap<GridTrackSize, LayoutUnit> GetIntrinsicRepeaterTrackSizes(
      bool has_items,
      const GridSizingTrackCollection& track_collection) const;

  // If `intrinsic_repeat_track_sizes` is non-null, this indicates the track
  // size(s) to use for intrinsic sized track(s) inside a repeat() track
  // definition. If we hit an intrinsic sized track within a repeat() definition
  // and don't provide `intrinsic_repeat_track_sizes`, then
  // `needs_intrinsic_track_size` will be set to true, indicating that another
  // track sizing pass will be required once we've computed the intrinsic track
  // size.
  wtf_size_t ComputeAutomaticRepetitions(
      const HashMap<GridTrackSize, LayoutUnit>* intrinsic_repeat_track_sizes,
      bool& needs_intrinsic_track_size) const;

  // From https://drafts.csswg.org/css-grid-3/#track-sizing-performance:
  //   "... synthesize a virtual masonry item that has the maximum of every
  //   intrinsic size contribution among the items in that group."
  // This method returns a collection of virtual items, with empty intrinsic
  // contribution sizes, as these get calculated after track initialization has
  // occurred. If `needs_intrinsic_track_size` is true, that means that we are
  // in the first track size pass required to compute intrinsic track sizes
  // within a repeat definition, which requires adjustments to virtual item
  // creation and track sizing per
  // https://www.w3.org/TR/css-grid-3/#masonry-intrinsic-repeat.
  VirtualItems* BuildVirtualGridLanesItems(
      const GridLineResolver& line_resolver,
      const GridItems& grid_lanes_items,
      const bool needs_intrinsic_track_size,
      const wtf_size_t auto_repetition_count,
      wtf_size_t& start_offset) const;

  // Computes the block-axis contribution of a virtual grid-lanes item for track
  // sizing. Also computes a baseline shim for the item and sets `baseline_shim`
  // to that value, which accounts for extra space needed to align the item's
  // baseline with the shared baseline of its track.
  LayoutUnit ComputeGridLanesItemBlockContribution(
      const GridSizingSubtree& sizing_subtree,
      GridTrackSizingDirection track_direction,
      SizingConstraint sizing_constraint,
      const ConstraintSpace space_for_measure,
      GridItemData* virtual_item,
      const bool needs_intrinsic_track_size,
      const BoxStrut& margins,
      LayoutUnit shared_baseline,
      LayoutUnit& baseline_shim) const;

  ConstraintSpace CreateConstraintSpace(
      const GridItemData& grid_lanes_item,
      const LogicalSize& containing_size,
      const LogicalSize& fixed_available_size,
      LayoutResultCacheSlot result_cache_slot,
      const GridLayoutSubtree* opt_layout_subtree = nullptr) const;

  // Return the inline contribution of `grid_lanes_item` calculated to either
  // the min-width or the max-width based on `sizing_constraint`.
  LayoutUnit CalculateItemInlineContribution(
      const GridSizingSubtree& sizing_subtree,
      const GridItemData& grid_lanes_item,
      const GridLayoutTrackCollection& track_collection,
      SizingConstraint sizing_constraint);

  ConstraintSpace CreateConstraintSpaceForMeasure(
      const SubgriddedItemData& subgridded_item,
      std::optional<LayoutUnit> opt_fixed_inline_size = std::nullopt,
      const GridLayoutTrackCollection* track_collection = nullptr,
      bool is_for_min_max_sizing = false) const;

  // Computes the shared baseline for items within a single virtual item group
  // (i.e., items that share the same span and baseline alignment). Returns the
  // maximum baseline among all items in the group.
  LayoutUnit ComputeSharedBaselineForGroup(
      const GridSizingSubtree& sizing_subtree,
      const GridItems::GridItemDataVector& group_items,
      GridTrackSizingDirection grid_axis_direction,
      SizingConstraint sizing_constraint) const;

  // From https://drafts.csswg.org/css-grid-3/#track-sizing-performance:
  //   "... synthesize a virtual masonry item that has the maximum of every
  //   intrinsic size contribution among the items in that group."
  // This method calculates the intrinsic contribution sizes and per-track
  // shared baselines for each virtual item group and updates the corresponding
  // virtual item(s) associated with each group accordingly, which will be used
  // to resolve the grid axis' track sizes. If `needs_intrinsic_track_size` is
  // true, that means that we are in the first track size pass required to
  // compute intrinsic track sizes within a repeat definition, which requires
  // adjustments to virtual item creation and track sizing per
  // https://www.w3.org/TR/css-grid-3/#masonry-intrinsic-repeat.
  void MeasureVirtualGridLanesItems(const GridSizingSubtree& sizing_subtree,
                                    SizingConstraint sizing_constraint,
                                    bool needs_intrinsic_track_size) const;

  // Lays out `grid_lanes_item` for measurement using `space_for_measure`. If
  // the available inline size is indefinite (e.g., for an orthogonal virtual
  // item), falls back to using the item's max-content contribution as its
  // inline size.
  const LayoutResult* LayoutItemForMeasureWithFallback(
      const GridSizingSubtree& sizing_subtree,
      GridItemData* grid_lanes_item,
      const ConstraintSpace& space_for_measure,
      SizingConstraint sizing_constraint) const;

  // `track_baseline` is the shared baseline for the track that `virtual_item`
  // participates in, or `LayoutUnit::Min()` if the item is not
  // baseline-aligned.
  LayoutUnit ContributionSizeForVirtualItem(
      const GridLayoutTrackCollection& track_collection,
      LayoutUnit track_baseline,
      GridItemContributionType contribution_type,
      GridItemData* virtual_item) const;

  LayoutUnit ComputeIntrinsicBlockSizeIgnoringChildren();

  std::optional<LayoutUnit> contain_intrinsic_block_size_;
  LayoutUnit intrinsic_block_size_;

  LogicalSize grid_lanes_available_size_;
  LogicalSize grid_lanes_min_available_size_;
  LogicalSize grid_lanes_max_available_size_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_LAYOUT_ALGORITHM_H_
