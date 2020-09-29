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
  enum class AutoPlacementType { kNotNeeded, kMajor, kMinor, kBoth };

  struct GridItemData {
    explicit GridItemData(const NGBlockNode node);

    AutoPlacementType AutoPlacement(
        GridTrackSizingDirection flow_direction) const;
    wtf_size_t StartLine(GridTrackSizingDirection direction) const;
    wtf_size_t EndLine(GridTrackSizingDirection direction) const;
    const GridSpan& Span(GridTrackSizingDirection direction) const;
    void SetSpan(const GridSpan& span, GridTrackSizingDirection direction);
    const NGBlockNode node;
    GridArea resolved_position;

    NGBoxStrut margins;
    LayoutUnit inline_size;
    MinMaxSizes min_max_sizes;

    bool is_spanning_flex_track : 1;
    bool is_spanning_intrinsic_track : 1;
  };

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

  class ReorderedGridItems {
   public:
    class Iterator
        : public std::iterator<std::input_iterator_tag, GridItemData> {
     public:
      Iterator(Vector<wtf_size_t>::const_iterator current_index,
               Vector<GridItemData>* items);

      bool operator!=(const Iterator& other) const;
      GridItemData& operator*();
      Iterator& operator++();

     private:
      Vector<wtf_size_t>::const_iterator current_index_;
      Vector<GridItemData>* items_;
    };

    ReorderedGridItems(const Vector<wtf_size_t>& reordered_item_indices,
                       Vector<GridItemData>& items);
    Iterator begin();
    Iterator end();

   private:
    const Vector<wtf_size_t>& reordered_item_indices_;
    Vector<GridItemData>& items_;
  };

  ReorderedGridItems GetReorderedGridItems();
  NGGridLayoutAlgorithmTrackCollection& TrackCollection(
      GridTrackSizingDirection track_direction);

  void ConstructAndAppendGridItems();
  GridItemData MeasureGridItem(const NGBlockNode node);
  NGConstraintSpace BuildSpaceForGridItem(const NGBlockNode node) const;

  // Sets the specified tracks for row and column track lists.
  void SetSpecifiedTracks();
  // Ensures a range boundary will exist on the start and end of the grid item.
  void EnsureTrackCoverageForGridItem(GridTrackSizingDirection track_direction,
                                      GridItemData& grid_item);
  // Determines the explicit column and row track starts.
  void DetermineExplicitTrackStarts();

  // For every grid item, determines if it spans a track with an intrinsic or
  // flexible sizing function and caches the answer in its |GridItemData|.
  void DetermineGridItemsSpanningIntrinsicOrFlexTracks(
      GridTrackSizingDirection track_direction);

  // Calculates from the min and max track sizing functions the used track size.
  void ComputeUsedTrackSizes(GridTrackSizingDirection track_direction);

  // Allows a test to set the value for automatic track repetition.
  void SetAutomaticTrackRepetitionsForTesting(wtf_size_t auto_column,
                                              wtf_size_t auto_row);
  wtf_size_t AutoRepeatCountForDirection(
      GridTrackSizingDirection track_direction) const;

  // Lays out and computes inline and block offsets for grid items.
  void PlaceGridItems();
  // Gets the row or column gap of the grid.
  LayoutUnit GridGap(GridTrackSizingDirection track_direction);

  GridTrackSizingDirection AutoFlowDirection() const;

  GridLayoutAlgorithmState state_;
  LogicalSize border_box_size_;
  LogicalSize child_percentage_size_;

  Vector<GridItemData> items_;
  Vector<wtf_size_t> reordered_item_indices_;

  NGGridBlockTrackCollection block_column_track_collection_;
  NGGridBlockTrackCollection block_row_track_collection_;

  NGGridLayoutAlgorithmTrackCollection algorithm_column_track_collection_;
  NGGridLayoutAlgorithmTrackCollection algorithm_row_track_collection_;

  wtf_size_t explicit_column_start_ = 0;
  wtf_size_t explicit_row_start_ = 0;
  wtf_size_t column_count_ = 0;
  wtf_size_t row_count_ = 0;

  wtf_size_t automatic_column_repetitions_ =
      NGGridBlockTrackCollection::kInvalidRangeIndex;
  wtf_size_t automatic_row_repetitions_ =
      NGGridBlockTrackCollection::kInvalidRangeIndex;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_LAYOUT_ALGORITHM_H_
