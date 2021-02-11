// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_PLACEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_PLACEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_track_collection.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// This class encapsulates the Grid Item Placement Algorithm described by
// https://drafts.csswg.org/css-grid/#auto-placement-algo
class CORE_EXPORT NGGridPlacement {
  STACK_ALLOCATED();

 public:
  enum class PackingBehavior { kSparse, kDense };

  using GridItems = NGGridLayoutAlgorithm::GridItems;
  using GridItemData = NGGridLayoutAlgorithm::GridItemData;
  using AutoPlacementType = NGGridLayoutAlgorithm::AutoPlacementType;

  NGGridPlacement(const ComputedStyle& grid_style,
                  const wtf_size_t column_auto_repetitions,
                  const wtf_size_t row_auto_repetitions);

  void RunAutoPlacementAlgorithm(GridItems* grid_items);
  // Helper function to resolve start and end lines of out of flow items.
  void ResolveOutOfFlowItemGridLines(
      const NGGridLayoutAlgorithmTrackCollection& track_collection,
      const ComputedStyle& out_of_flow_item_style,
      wtf_size_t* start_line,
      wtf_size_t* end_line) const;

  wtf_size_t AutoRepetitions(GridTrackSizingDirection track_direction) const;

 private:
  struct AutoPlacementCursor {
    wtf_size_t major_position{0};
    wtf_size_t minor_position{0};
  };

  // Compute the track start offset from the grid items positioned at negative
  // indices.
  wtf_size_t DetermineTrackStartOffset(
      const GridItems& grid_items,
      GridTrackSizingDirection track_direction) const;

  // Place non auto-positioned elements from |grid_items|; returns true if any
  // item needs to resolve an automatic position. Otherwise, false.
  bool PlaceNonAutoGridItems(GridItems* grid_items);
  // Place elements from |grid_items| that have a definite position on the major
  // axis but need auto-placement on the minor axis.
  void PlaceGridItemsLockedToMajorAxis(GridItems* grid_items);
  // Place an item that has a definite position on the minor axis but need
  // auto-placement on the major axis.
  void PlaceAutoMajorAxisGridItem(GridItemData* grid_item,
                                  AutoPlacementCursor* placement_cursor,
                                  const GridItems& grid_items);
  // Place an item that needs auto-placement on both the major and minor axis.
  void PlaceAutoBothAxisGridItem(GridItemData* grid_item,
                                 AutoPlacementCursor* placement_cursor,
                                 const GridItems& grid_items);

  // Places a grid item; returns true if it has a definite position in the given
  // direction, false if the item needs auto-placement.
  bool PlaceGridItem(GridItemData* grid_item,
                     GridTrackSizingDirection track_direction) const;

  // Returns true if the given placement would overlap with a placed item.
  bool DoesItemOverlap(wtf_size_t major_start,
                       wtf_size_t major_end,
                       wtf_size_t minor_start,
                       wtf_size_t minor_end,
                       const GridItems& grid_items) const;

  wtf_size_t StartOffset(GridTrackSizingDirection track_direction) const;
  wtf_size_t AutoRepeatTrackCount(
      GridTrackSizingDirection track_direction) const;
  bool HasSparsePacking() const;

  // Used to resolve positions using |GridPositionsResolver|.
  const ComputedStyle& grid_style_;

  const PackingBehavior packing_behavior_;
  const GridTrackSizingDirection major_direction_;
  const GridTrackSizingDirection minor_direction_;
  const wtf_size_t column_auto_repeat_track_count_;
  const wtf_size_t row_auto_repeat_track_count_;
  const wtf_size_t column_auto_repetitions_;
  const wtf_size_t row_auto_repetitions_;

  wtf_size_t minor_max_end_line_;
  wtf_size_t column_start_offset_;
  wtf_size_t row_start_offset_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_PLACEMENT_H_
