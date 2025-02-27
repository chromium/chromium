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

  GridSizingTrackCollection BuildGridAxisTracks(
      const GridLineResolver& line_resolver,
      SizingConstraint sizing_constraint,
      wtf_size_t* start_offset) const;

  wtf_size_t ComputeAutomaticRepetitions() const;

  // From https://drafts.csswg.org/css-grid-3/#track-sizing-performance:
  //   "... synthesize a virtual masonry item that has the maximum of every
  //   intrinsic size contribution among the items in that group."
  // Returns a collection of items that reflect the intrinsic contributions from
  // the item groups, which will be used to resolve the grid axis' track sizes.
  GridItems* VirtualMasonryItems(const GridLineResolver& line_resolver,
                                 wtf_size_t* start_offset) const;

  ConstraintSpace CreateConstraintSpace(
      const GridItemData& masonry_item,
      const LogicalSize& containing_size,
      LayoutResultCacheSlot result_cache_slot) const;

  // If `containing_rect` is provided, it will store the available size for the
  // item and its offset within the container. These values will be used to
  // adjust the item's final position using its alignment properties.
  ConstraintSpace CreateConstraintSpaceForLayout(
      const GridItemData& masonry_item,
      const GridSizingTrackCollection& track_collection,
      LogicalRect* containing_rect = nullptr) const;

  ConstraintSpace CreateConstraintSpaceForMeasure(
      const GridItemData& masonry_item) const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_MASONRY_LAYOUT_ALGORITHM_H_
