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
  explicit NGGridPlacement(const wtf_size_t row_auto_repeat,
                           const wtf_size_t column_auto_repeat,
                           const wtf_size_t row_explicit_start,
                           const wtf_size_t column_explicit_start,
                           const PackingBehavior packing_behavior,
                           const GridTrackSizingDirection major_direction,
                           const ComputedStyle& grid_style,
                           wtf_size_t minor_max_end_line,
                           NGGridBlockTrackCollection& row_collection,
                           NGGridBlockTrackCollection& column_collection,
                           Vector<NGGridLayoutAlgorithm::GridItemData>& items);
  void RunAutoPlacementAlgorithm();

 private:
  // Place non auto-positioned items and determine what items need auto
  // placement, if any do, this returns true.
  bool PlaceNonAutoGridItems();
  // Place items that have a definite position on the major axis but need auto
  // placement on the minor axis.
  void PlaceGridItemsLockedToMajorAxis();
  // Place item that has a definite position on the minor axis but need auto
  // placement on the major axis.
  void PlaceAutoMajorAxisGridItem(
      NGGridLayoutAlgorithm::GridItemData& item_data);
  // Place items that need automatic placement on both the major and minor axis.
  void PlaceAutoBothAxisGridItem(
      NGGridLayoutAlgorithm::GridItemData& item_data);
  // Places a grid item if it has a definite position in the given direction,
  // returns true if item was able to be positioned, false if item needs auto
  // positioning in the given direction.
  bool PlaceGridItem(
      GridTrackSizingDirection grid_direction,
      NGGridLayoutAlgorithm::NGGridLayoutAlgorithm::GridItemData& item_data);

  void UpdatePlacementAndEnsureTrackCoverage(
      GridSpan span,
      GridTrackSizingDirection track_direction,
      NGGridLayoutAlgorithm::NGGridLayoutAlgorithm::GridItemData& item_data);

  // Returns true if the given placement would overlap with a placed item.
  bool DoesItemOverlap(wtf_size_t major_start,
                       wtf_size_t major_end,
                       wtf_size_t minor_start,
                       wtf_size_t minor_end) const;

  wtf_size_t AutoRepeat(GridTrackSizingDirection direction);
  wtf_size_t ExplicitStart(GridTrackSizingDirection direction);
  NGGridBlockTrackCollection& BlockCollection(
      GridTrackSizingDirection direction);

  const wtf_size_t row_auto_repeat_;
  const wtf_size_t column_auto_repeat_;
  const wtf_size_t row_explicit_start_;
  const wtf_size_t column_explicit_start_;
  const PackingBehavior packing_behavior_;
  const GridTrackSizingDirection major_direction_;
  const GridTrackSizingDirection minor_direction_;
  // Used to resolve positions using GridPositionsResolver.
  const ComputedStyle& grid_style_;

  // Keeps track of the biggest minor end line among items with an explicit
  // major line.
  wtf_size_t minor_max_end_line_ = 0;

  NGGridBlockTrackCollection& row_collection_;
  NGGridBlockTrackCollection& column_collection_;
  Vector<NGGridLayoutAlgorithm::GridItemData>& items_;

  wtf_size_t starting_minor_line_ = 0;
  wtf_size_t ending_minor_line_ = 0;
  wtf_size_t placement_cursor_major;
  wtf_size_t placement_cursor_minor;
  // Subset of |items_| containing items that have a definite position on the
  // major axis.
  Vector<NGGridLayoutAlgorithm::GridItemData*> items_locked_to_major_axis_;
  // Subset of |items_| containing items that do not have a definite position on
  // the major axis.
  Vector<NGGridLayoutAlgorithm::GridItemData*> items_not_locked_to_major_axis_;
};
}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_PLACEMENT_H_
