// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_MASONRY_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_MASONRY_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/masonry/masonry_node.h"

namespace blink {

class GridItems;
class GridLineResolver;
class GridSizingTrackCollection;
class MasonryRunningPositions;
enum class SizingConstraint;
struct GridItemData;

class CORE_EXPORT MasonryLayoutAlgorithm
    : public LayoutAlgorithm<MasonryNode, BoxFragmentBuilder, BlockBreakToken> {
 public:
  explicit MasonryLayoutAlgorithm(const LayoutAlgorithmParams& params);

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesFloatInput&);
  const LayoutResult* Layout();

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

  GridSizingTrackCollection BuildGridAxisTracks(
      const GridLineResolver& line_resolver,
      const GridItems& masonry_items,
      SizingConstraint sizing_constraint,
      wtf_size_t& start_offset) const;

  wtf_size_t ComputeAutomaticRepetitions() const;

  // From https://drafts.csswg.org/css-grid-3/#track-sizing-performance:
  //   "... synthesize a virtual masonry item that has the maximum of every
  //   intrinsic size contribution among the items in that group."
  // Returns a collection of items that reflect the intrinsic contributions from
  // the item groups, which will be used to resolve the grid axis' track sizes.
  GridItems BuildVirtualMasonryItems(const GridLineResolver& line_resolver,
                                     const GridItems& masonry_items,
                                     SizingConstraint sizing_constraint,
                                     wtf_size_t& start_offset) const;

  LayoutUnit ComputeMasonryItemBlockContribution(
      GridTrackSizingDirection track_direction,
      SizingConstraint sizing_constraint,
      const ConstraintSpace space_for_measure,
      const GridItemData* virtual_item) const;

  ConstraintSpace CreateConstraintSpace(
      const GridItemData& masonry_item,
      const LogicalSize& containing_size,
      const LogicalSize& fixed_available_size,
      LayoutResultCacheSlot result_cache_slot) const;

  // If `containing_rect` is provided, it will store the available size for the
  // item and its offset within the container. These values will be used to
  // adjust the item's final position using its alignment properties.
  ConstraintSpace CreateConstraintSpaceForLayout(
      const GridItemData& masonry_item,
      const GridLayoutTrackCollection& track_collection,
      LogicalRect* containing_rect = nullptr) const;

  ConstraintSpace CreateConstraintSpaceForMeasure(
      const GridItemData& masonry_item,
      std::optional<LayoutUnit> opt_fixed_inline_size = std::nullopt,
      bool is_for_min_max_sizing = false) const;

  LayoutUnit intrinsic_block_size_;

  LogicalSize masonry_available_size_;
  LogicalSize masonry_min_available_size_;
  LogicalSize masonry_max_available_size_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_MASONRY_LAYOUT_ALGORITHM_H_
