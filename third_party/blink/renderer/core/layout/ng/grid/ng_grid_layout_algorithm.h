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
  enum class AxisEdge { kStart, kCenter, kEnd, kBaseline };

  // This enum corresponds to each step used to accommodate grid items across
  // intrinsic tracks according to their min and max track sizing functions, as
  // defined in https://drafts.csswg.org/css-grid-1/#algo-spanning-items.
  enum class GridItemContributionType {
    kForIntrinsicMinimums,
    kForContentBasedMinimums,
    kForMaxContentMinimums,
    kForIntrinsicMaximums,
    kForMaxContentMaximums
  };

  struct GridItemData {
    explicit GridItemData(const NGBlockNode node);

    AutoPlacementType AutoPlacement(
        GridTrackSizingDirection flow_direction) const;
    const GridSpan& Span(GridTrackSizingDirection track_direction) const;
    void SetSpan(const GridSpan& span,
                 GridTrackSizingDirection track_direction);

    wtf_size_t StartLine(GridTrackSizingDirection track_direction) const;
    wtf_size_t EndLine(GridTrackSizingDirection track_direction) const;
    wtf_size_t SpanSize(GridTrackSizingDirection track_direction) const;

    const NGBlockNode node;
    GridArea resolved_position;

    NGBoxStrut margins;
    LayoutUnit inline_size;
    MinMaxSizes min_max_sizes;

    // These fields are used to determine the sets this item spans in the
    // respective track collection; see |CacheItemSetIndices|.
    wtf_size_t columns_begin_set_index;
    wtf_size_t columns_end_set_index;
    wtf_size_t rows_begin_set_index;
    wtf_size_t rows_end_set_index;

    AxisEdge inline_axis_alignment;
    AxisEdge block_axis_alignment;

    bool is_inline_axis_stretched;
    bool is_block_axis_stretched;

    bool is_spanning_flex_track : 1;
    bool is_spanning_intrinsic_track : 1;
  };

  explicit NGGridLayoutAlgorithm(const NGLayoutAlgorithmParams& params);

  scoped_refptr<const NGLayoutResult> Layout() override;

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesInput&) const override;

 private:
  using NGGridSetVector = Vector<NGGridSet*, 16>;

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
               Vector<GridItemData>* grid_items);

      bool operator!=(const Iterator& other) const;
      GridItemData* operator->();
      GridItemData& operator*();
      Iterator& operator++();

     private:
      Vector<wtf_size_t>::const_iterator current_index_;
      Vector<GridItemData>* grid_items_;
    };

    ReorderedGridItems(const Vector<wtf_size_t>& reordered_item_indices,
                       Vector<GridItemData>& grid_items);
    Iterator begin();
    Iterator end();

   private:
    const Vector<wtf_size_t>& reordered_item_indices_;
    Vector<GridItemData>& grid_items_;
  };


  // Returns an iterator for every |NGGridSet| contained within an item's span
  // in the relevant track collection.
  static NGGridLayoutAlgorithmTrackCollection::SetIterator
  GetSetIteratorForItem(const GridItemData& item,
                        GridTrackSizingDirection track_direction,
                        NGGridLayoutAlgorithmTrackCollection& track_collection);

  // Returns the size that a grid item will distribute across the tracks with an
  // intrinsic sizing function it spans in the relevant track direction.
  LayoutUnit ContributionSizeForGridItem(
      const GridItemData& grid_item,
      GridTrackSizingDirection track_direction,
      GridItemContributionType contribution_type) const;

  void ConstructAndAppendGridItems(
      Vector<GridItemData>* grid_items,
      Vector<GridItemData>* out_of_flow_items) const;
  GridItemData MeasureGridItem(const NGBlockNode node) const;
  NGConstraintSpace BuildSpaceForGridItem(const NGBlockNode node) const;

  void BuildBlockTrackCollections(
      Vector<GridItemData>* grid_items,
      NGGridBlockTrackCollection* column_track_collection,
      NGGridBlockTrackCollection* row_track_collection) const;

  void BuildAlgorithmTrackCollections(
      Vector<GridItemData>* grid_items,
      NGGridLayoutAlgorithmTrackCollection* column_track_collection,
      NGGridLayoutAlgorithmTrackCollection* row_track_collection) const;

  // Sets specified track lists on |track_collection|.
  void SetSpecifiedTracks(GridTrackSizingDirection track_direction,
                          wtf_size_t automatic_repetitions,
                          NGGridBlockTrackCollection* track_collection) const;
  // Determines the explicit column and row track starts.
  void DetermineExplicitTrackStarts(wtf_size_t automatic_column_repetitions,
                                    wtf_size_t automatic_row_repetitions,
                                    wtf_size_t* explicit_column_start,
                                    wtf_size_t* explicit_row_start,
                                    wtf_size_t* column_count,
                                    wtf_size_t* row_count) const;

  // For every item and track direction, computes and stores the pair of indices
  // "begin" and "end" such that the item spans every set from the respective
  // collection's |sets_| with an index in the range [begin, end).
  void CacheItemSetIndices(
      GridTrackSizingDirection track_direction,
      const NGGridLayoutAlgorithmTrackCollection* track_collection,
      Vector<GridItemData>* grid_items) const;
  // For every grid item, determines if it spans a track with an intrinsic or
  // flexible sizing function and caches the answer in its |GridItemData|.
  void DetermineGridItemsSpanningIntrinsicOrFlexTracks(
      GridTrackSizingDirection track_direction,
      Vector<GridItemData>* grid_items,
      Vector<wtf_size_t>* reordered_item_indices,
      NGGridLayoutAlgorithmTrackCollection* track_collection) const;

  // Calculates from the min and max track sizing functions the used track size.
  void ComputeUsedTrackSizes(
      GridTrackSizingDirection track_direction,
      Vector<GridItemData>* grid_items,
      NGGridLayoutAlgorithmTrackCollection* track_collection) const;

  // These methods implement the steps of the algorithm for intrinsic track size
  // resolution defined in https://drafts.csswg.org/css-grid-1/#algo-content.
  void ResolveIntrinsicTrackSizes(
      GridTrackSizingDirection track_direction,
      Vector<GridItemData>* grid_items,
      Vector<wtf_size_t>* reordered_item_indices,
      NGGridLayoutAlgorithmTrackCollection* track_collection) const;
  void IncreaseTrackSizesToAccommodateGridItems(
      GridTrackSizingDirection track_direction,
      ReorderedGridItems::Iterator group_begin,
      ReorderedGridItems::Iterator group_end,
      GridItemContributionType contribution_type,
      NGGridLayoutAlgorithmTrackCollection* track_collection) const;

  static void DistributeExtraSpaceToSets(
      LayoutUnit extra_space,
      GridItemContributionType contribution_type,
      NGGridSetVector* sets_to_grow,
      NGGridSetVector* sets_to_grow_beyond_limit);

  // Lays out and computes inline and block offsets for grid items.
  void PlaceGridItems(
      const Vector<GridItemData>& grid_items,
      const Vector<GridItemData>& out_of_flow_items,
      NGGridLayoutAlgorithmTrackCollection& column_track_collection,
      NGGridLayoutAlgorithmTrackCollection& row_track_collection,
      LayoutUnit* intrinsic_block_size);

  // Lays out |grid_item| based on the offsets and sizes provided.
  void PlaceGridItem(const GridItemData& grid_item,
                     LogicalOffset offset,
                     LogicalSize size);

  // Gets the row or column gap of the grid.
  LayoutUnit GridGap(GridTrackSizingDirection track_direction,
                     LayoutUnit available_size = kIndefiniteSize) const;

  // Calculates inline and block offsets for all tracks.
  Vector<LayoutUnit> ComputeSetOffsets(
      GridTrackSizingDirection track_direction,
      LayoutUnit grid_gap,
      NGGridLayoutAlgorithmTrackCollection& track_collection) const;

  // Tests whether the row gap is unresolvable based on its type and the
  // available size.
  bool IsRowGridGapUnresolvable(LayoutUnit available_size) const;

  GridTrackSizingDirection AutoFlowDirection() const;

  LogicalSize border_box_size_;
  LogicalSize child_percentage_size_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_LAYOUT_ALGORITHM_H_
