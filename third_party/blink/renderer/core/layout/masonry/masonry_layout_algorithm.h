// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_MASONRY_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_MASONRY_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/masonry/grid_lanes_node.h"

namespace blink {

class ComputedStyle;
class GridItems;
class GridLayoutData;
class GridLineResolver;
class GridSizingTrackCollection;
class MasonryRunningPositions;
enum class GridItemContributionType;
enum class SizingConstraint;
struct BoxStrut;
struct GridItemData;
struct GridPlacementData;

class CORE_EXPORT MasonryLayoutAlgorithm
    : public LayoutAlgorithm<GridLanesNode,
                             BoxFragmentBuilder,
                             BlockBreakToken> {
 public:
  explicit MasonryLayoutAlgorithm(const LayoutAlgorithmParams& params);

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesFloatInput&);
  const LayoutResult* Layout();

  // Computes the containing block rect for out-of-flow items placed
  // within the masonry.
  static LogicalRect ComputeOutOfFlowItemContainingRect(
      const GridPlacementData& placement_data,
      const GridLayoutData& layout_data,
      const ComputedStyle& masonry_style,
      const BoxStrut& borders,
      const LogicalSize& border_box_size,
      GridItemData* out_of_flow_item);

 private:
  friend class MasonryLayoutAlgorithmTest;

  // This places all the items stored in `masonry_items` and adjusts
  // `intrinsic_block_size_` based on the placement of the items. Each item's
  // resolved position is translated based on `start_offset`. Placement of
  // the items is finalized within this method. `running_positions` is an output
  // parameter that can be used to find the intrinsic inline size when the
  // stacking axis is the inline axis.
  void PlaceMasonryItems(
      const GridLayoutTrackCollection& track_collection,
      GridItems& masonry_items,
      wtf_size_t start_offset,
      MasonryRunningPositions& running_positions,
      std::optional<SizingConstraint> sizing_constraint = std::nullopt);

  // Places all out-of-flow (OOF) masonry items. For each item, this method
  // computes the size and location of the containing block rectangle within the
  // masonry container, calculates alignment offsets using item alignment
  // properties, and adds the item as an out-of-flow candidate via
  // `AddOutOfFlowChildCandidate`. `oof_children` is a required input vector
  // containing the layout boxes of OOF masonry items.
  void PlaceOutOfFlowItems(const GridLayoutData& layout_data,
                           LayoutUnit block_size,
                           HeapVector<Member<LayoutBox>>& oof_children);

  // Returns the track collection given the provided `sizing_constraint`.
  // If `intrinsic_repeat_track_sizes` is non-null, this contains the track
  // size(s) to use for intrinsic sized track(s) inside a repeat() track
  // definition. The `masonry_items` and `start_offset` associated with the
  // returned track collection are returned via the corresponding output params.
  // If we hit an intrinsic sized track within a repeat() definition and don't
  // provide `intrinsic_repeat_track_sizes`, then `needs_intrinsic_track_size`
  // will be set to true, indicating that another track sizing pass will be
  // required once we've computed the intrinsic track size. `opt_oof_children`
  // is an optional vector of out-of-flow direct children of the masonry
  // container that this method will populate. `collapsed_track_indexes` will be
  // populated with all the grid track indexes that were collapsed as a result
  // of auto-fit.
  GridSizingTrackCollection ComputeGridAxisTracks(
      const SizingConstraint sizing_constraint,
      const Vector<LayoutUnit>* intrinsic_repeat_track_sizes,
      GridItems& masonry_items,
      Vector<wtf_size_t>& collapsed_track_indexes,
      wtf_size_t& start_offset,
      bool& needs_intrinsic_track_size,
      HeapVector<Member<LayoutBox>>* opt_oof_children = nullptr) const;

  GridSizingTrackCollection BuildGridAxisTracks(
      const GridLineResolver& line_resolver,
      const GridItems& masonry_items,
      SizingConstraint sizing_constraint,
      bool& needs_intrinsic_track_size,
      Vector<wtf_size_t>& collapsed_track_indexes,
      wtf_size_t& start_offset) const;

  // Given a `track_collection`, return all the track sizes of an auto repeat
  // that has intrinsic track size(s). This method assumes that such an auto
  // repeat exists in `track_collection`. `has_items` indicates whether there
  // are any masonry items in the masonry container.
  Vector<LayoutUnit> GetIntrinsicRepeaterTrackSizes(
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
      const Vector<LayoutUnit>* intrinsic_repeat_track_sizes,
      bool& needs_intrinsic_track_size) const;

  // From https://drafts.csswg.org/css-grid-3/#track-sizing-performance:
  //   "... synthesize a virtual masonry item that has the maximum of every
  //   intrinsic size contribution among the items in that group."
  // Returns a collection of items that reflect the intrinsic contributions from
  // the item groups, which will be used to resolve the grid axis' track sizes.
  // If `needs_intrinsic_track_size` is true, that means that we are in the
  // first track size pass required to compute intrinsic track sizes within a
  // repeat definition, which requires adjustments to virtual item creation and
  // track sizing per
  // https://www.w3.org/TR/css-grid-3/#masonry-intrinsic-repeat.
  GridItems BuildVirtualMasonryItems(const GridLineResolver& line_resolver,
                                     const GridItems& masonry_items,
                                     const bool needs_intrinsic_track_size,
                                     SizingConstraint sizing_constraint,
                                     const wtf_size_t auto_repetition_count,
                                     wtf_size_t& start_offset) const;

  LayoutUnit ComputeMasonryItemBlockContribution(
      GridTrackSizingDirection track_direction,
      SizingConstraint sizing_constraint,
      const ConstraintSpace space_for_measure,
      const GridItemData* virtual_item,
      const bool needs_intrinsic_track_size) const;

  ConstraintSpace CreateConstraintSpace(
      const GridItemData& masonry_item,
      const LogicalSize& containing_size,
      const LogicalSize& fixed_available_size,
      LayoutResultCacheSlot result_cache_slot) const;

  // Return the inline contribution of `masonry_item` calculated to either the
  // min-width or the max-width based on `sizing_constraint`.
  LayoutUnit CalculateItemInlineContribution(
      const GridItemData& masonry_item,
      const GridLayoutTrackCollection& track_collection,
      SizingConstraint sizing_constraint);

  // If `containing_rect` is provided, it will store the available size for the
  // item and its offset within the container. These values will be used to
  // adjust the item's final position using its alignment properties.
  ConstraintSpace CreateConstraintSpaceForLayout(
      const GridItemData& masonry_item,
      const GridLayoutTrackCollection& track_collection,
      std::optional<LayoutUnit> opt_fixed_inline_size = std::nullopt,
      LogicalRect* containing_rect = nullptr) const;

  ConstraintSpace CreateConstraintSpaceForMeasure(
      const GridItemData& masonry_item,
      std::optional<LayoutUnit> opt_fixed_inline_size = std::nullopt,
      const GridLayoutTrackCollection* track_collection = nullptr,
      bool is_for_min_max_sizing = false) const;

  LayoutUnit ContributionSizeForVirtualItem(
      const GridLayoutTrackCollection& track_collection,
      GridItemContributionType contribution_type,
      GridItemData* virtual_item) const;

  LayoutUnit intrinsic_block_size_;

  LogicalSize masonry_available_size_;
  LogicalSize masonry_min_available_size_;
  LogicalSize masonry_max_available_size_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_MASONRY_LAYOUT_ALGORITHM_H_
