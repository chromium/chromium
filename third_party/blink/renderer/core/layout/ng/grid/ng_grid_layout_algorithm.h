// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_track_collection.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CORE_EXPORT NGGridLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  explicit NGGridLayoutAlgorithm(const NGLayoutAlgorithmParams& params);

  scoped_refptr<const NGLayoutResult> Layout() override;

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesInput&) const override;

  const NGGridLayoutAlgorithmTrackCollection& ColumnTrackCollection() const;
  const NGGridLayoutAlgorithmTrackCollection& RowTrackCollection() const;

 private:
  friend class NGGridLayoutAlgorithmTest;

  enum class GridLayoutAlgorithmState {
    kMeasuringItems,
    kResolvingInlineSize,
    kResolvingBlockSize,
    kPlacingGridItems,
    kCompletedLayout
  };

  struct GridItemData {
    LayoutUnit inline_size;
    MinMaxSizes min_max_sizes;
    NGBoxStrut margins;
  };

  NGGridLayoutAlgorithmTrackCollection& TrackCollection(
      GridTrackSizingDirection track_direction);

  void ConstructAndAppendGridItems();
  void ConstructAndAppendGridItem(const NGBlockNode& node);
  GridItemData MeasureGridItem(const NGBlockNode& node);
  NGConstraintSpace BuildSpaceForGridItem(const NGBlockNode& node) const;

  // Sets the specified tracks for row and column track lists.
  void SetSpecifiedTracks();
  // Ensures a range boundary will exist on the start and end of the grid item.
  void EnsureTrackCoverageForGridItem(const NGBlockNode& grid_item,
                                      GridTrackSizingDirection grid_direction);
  // Determines the explicit column and row track starts.
  void DetermineExplicitTrackStarts();

  // Calculates from the min and max track sizing functions the used track size.
  void ComputeUsedTrackSizes(GridTrackSizingDirection track_direction);

  // Allows a test to set the value for automatic track repetition.
  void SetAutomaticTrackRepetitionsForTesting(wtf_size_t auto_column,
                                              wtf_size_t auto_row);
  wtf_size_t AutoRepeatCountForDirection(
      GridTrackSizingDirection direction) const;

  // Lays out and computes inline and block offsets for grid items.
  void PlaceGridItems();

  Vector<GridItemData> items_;
  GridLayoutAlgorithmState state_;
  LogicalSize child_percentage_size_;

  NGGridBlockTrackCollection block_column_track_collection_;
  NGGridBlockTrackCollection block_row_track_collection_;

  NGGridLayoutAlgorithmTrackCollection algorithm_column_track_collection_;
  NGGridLayoutAlgorithmTrackCollection algorithm_row_track_collection_;

  wtf_size_t explicit_column_start_ = 0;
  wtf_size_t explicit_row_start_ = 0;

  wtf_size_t automatic_column_repetitions_ =
      NGGridBlockTrackCollection::kInvalidRangeIndex;
  wtf_size_t automatic_row_repetitions_ =
      NGGridBlockTrackCollection::kInvalidRangeIndex;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_LAYOUT_ALGORITHM_H_
