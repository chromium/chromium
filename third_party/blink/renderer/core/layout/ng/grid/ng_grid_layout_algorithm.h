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

class NGGridPlacement;

class CORE_EXPORT NGGridLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  enum class AutoPlacementType { kNotNeeded, kMajor, kMinor, kBoth };
  enum class AxisEdge { kStart, kCenter, kEnd, kBaseline };
  enum class ItemType { kInGridFlow, kOutOfFlow };

  // This enum corresponds to each step used to accommodate grid items across
  // intrinsic tracks according to their min and max track sizing functions, as
  // defined in https://drafts.csswg.org/css-grid-2/#algo-spanning-items.
  enum class GridItemContributionType {
    kForIntrinsicMinimums,
    kForContentBasedMinimums,
    kForMaxContentMinimums,
    kForIntrinsicMaximums,
    kForMaxContentMaximums,
    kForFreeSpace
  };

  struct ItemSetIndices {
    wtf_size_t begin = kNotFound;
    wtf_size_t end = kNotFound;
  };

  struct GridItemData {
    explicit GridItemData(const NGBlockNode node) : node(node) {}

    AutoPlacementType AutoPlacement(
        GridTrackSizingDirection flow_direction) const;
    const GridSpan& Span(GridTrackSizingDirection track_direction) const;
    void SetSpan(const GridSpan& span,
                 GridTrackSizingDirection track_direction);

    wtf_size_t StartLine(GridTrackSizingDirection track_direction) const;
    wtf_size_t EndLine(GridTrackSizingDirection track_direction) const;
    wtf_size_t SpanSize(GridTrackSizingDirection track_direction) const;

    const TrackSpanProperties& GetTrackSpanProperties(
        GridTrackSizingDirection track_direction) const;
    void SetTrackSpanProperty(TrackSpanProperties::PropertyId property,
                              GridTrackSizingDirection track_direction);

    bool IsSpanningFlexibleTrack(
        GridTrackSizingDirection track_direction) const;
    bool IsSpanningIntrinsicTrack(
        GridTrackSizingDirection track_direction) const;

    // For this item and track direction, computes and stores the pair of
    // indices "begin" and "end" such that the item spans every set from the
    // respective collection's |sets_| with an index in the range [begin, end).
    // |grid_placement| is used to resolve the grid lines of out of flow items
    // and it has a default nullptr value for grid items.
    ItemSetIndices SetIndices(
        const NGGridLayoutAlgorithmTrackCollection& track_collection,
        const NGGridPlacement* grid_placement = nullptr);

    const NGBlockNode node;
    GridArea resolved_position;

    NGBoxStrut margins;

    AxisEdge inline_axis_alignment;
    AxisEdge block_axis_alignment;

    ItemType item_type;

    bool is_inline_axis_stretched;
    bool is_block_axis_stretched;

    TrackSpanProperties column_span_properties;
    TrackSpanProperties row_span_properties;

    // These fields are used to determine the sets this item spans in the
    // respective track collection; see |SetIndices|. We use optional since some
    // scenarios don't require to compute the indices at all.
    base::Optional<ItemSetIndices> column_set_indices;
    base::Optional<ItemSetIndices> row_set_indices;
  };

  explicit NGGridLayoutAlgorithm(const NGLayoutAlgorithmParams& params);

  scoped_refptr<const NGLayoutResult> Layout() override;
  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesInput&) const override;

 private:
  friend class NGGridLayoutAlgorithmTest;

  enum class SizingConstraint { kLayout, kMinContent, kMaxContent };

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

  // Returns the size that a grid item will distribute across the tracks with an
  // intrinsic sizing function it spans in the relevant track direction.
  LayoutUnit ContributionSizeForGridItem(
      const GridItemData& grid_item,
      GridTrackSizingDirection track_direction,
      GridItemContributionType contribution_type) const;

  wtf_size_t ComputeAutomaticRepetitions(
      GridTrackSizingDirection track_direction) const;

  void ConstructAndAppendGridItems(
      Vector<GridItemData>* grid_items,
      Vector<GridItemData>* out_of_flow_items = nullptr) const;
  GridItemData MeasureGridItem(const NGBlockNode node) const;

  void BuildBlockTrackCollections(
      Vector<GridItemData>* grid_items,
      NGGridBlockTrackCollection* column_track_collection,
      NGGridBlockTrackCollection* row_track_collection,
      NGGridPlacement* grid_placement) const;

  void BuildAlgorithmTrackCollections(
      Vector<GridItemData>* grid_items,
      NGGridLayoutAlgorithmTrackCollection* column_track_collection,
      NGGridLayoutAlgorithmTrackCollection* row_track_collection,
      NGGridPlacement* grid_placement) const;

  // Sets specified track lists on |track_collection|.
  void SetSpecifiedTracks(wtf_size_t auto_repetitions,
                          NGGridBlockTrackCollection* track_collection) const;
  // Ensure coverage in block collection after grid items have been placed.
  void EnsureTrackCoverageForGridItems(
      const Vector<GridItemData>& grid_items,
      NGGridBlockTrackCollection* track_collection) const;

  // For every grid item, caches properties of the track sizing functions it
  // spans (i.e. whether an item spans intrinsic or flexible tracks).
  void CacheGridItemsTrackSpanProperties(
      const NGGridLayoutAlgorithmTrackCollection& track_collection,
      Vector<GridItemData>* grid_items,
      Vector<wtf_size_t>* reordered_item_indices) const;

  // Calculates from the min and max track sizing functions the used track size.
  void ComputeUsedTrackSizes(
      SizingConstraint sizing_constraint,
      NGGridLayoutAlgorithmTrackCollection* track_collection,
      Vector<GridItemData>* grid_items,
      Vector<wtf_size_t>* reordered_item_indices) const;

  // These methods implement the steps of the algorithm for intrinsic track size
  // resolution defined in https://drafts.csswg.org/css-grid-2/#algo-content.
  void ResolveIntrinsicTrackSizes(
      NGGridLayoutAlgorithmTrackCollection* track_collection,
      Vector<GridItemData>* grid_items,
      Vector<wtf_size_t>* reordered_item_indices) const;

  void IncreaseTrackSizesToAccommodateGridItems(
      ReorderedGridItems::Iterator group_begin,
      ReorderedGridItems::Iterator group_end,
      GridItemContributionType contribution_type,
      NGGridLayoutAlgorithmTrackCollection* track_collection) const;

  void MaximizeTracks(
      SizingConstraint sizing_constraint,
      NGGridLayoutAlgorithmTrackCollection* track_collection) const;

  void StretchAutoTracks(
      SizingConstraint sizing_constraint,
      NGGridLayoutAlgorithmTrackCollection* track_collection) const;

  // Lays out and computes inline and block offsets for grid items.
  void PlaceItems(
      const NGGridLayoutAlgorithmTrackCollection& column_track_collection,
      const NGGridLayoutAlgorithmTrackCollection& row_track_collection,
      Vector<GridItemData>* grid_items,
      Vector<GridItemData>* out_of_flow_items,
      NGGridPlacement* grid_placement,
      LayoutUnit* intrinsic_block_size,
      LayoutUnit* block_size);

  // Gets the row or column gap of the grid.
  LayoutUnit GridGap(GridTrackSizingDirection track_direction,
                     LayoutUnit available_size = kIndefiniteSize) const;

  LayoutUnit DetermineFreeSpace(
      SizingConstraint sizing_constraint,
      const NGGridLayoutAlgorithmTrackCollection& track_collection) const;

  // Layout the |grid_items| based on the offsets provided.
  void PlaceGridItems(const Vector<GridItemData>& grid_items,
                      const Vector<LayoutUnit>& column_set_offsets,
                      const Vector<LayoutUnit>& row_set_offsets,
                      LayoutUnit block_size,
                      LayoutUnit column_gutter_size,
                      LayoutUnit row_gutter_size);

  // Computes the static position, grid area and its offset of out of flow
  // elements in the grid.
  void PlaceOutOfFlowItems(const Vector<GridItemData>& out_of_flow_items,
                           const Vector<LayoutUnit>& column_set_offsets,
                           const Vector<LayoutUnit>& row_set_offsets,
                           LayoutUnit block_size,
                           LayoutUnit column_gutter_size,
                           LayoutUnit row_gutter_size);

  // Gets the out of flow descendants from the container builder and computes
  // their containing block rect.
  void PlaceOutOfFlowDescendants(
      const NGGridLayoutAlgorithmTrackCollection& column_track_collection,
      const NGGridLayoutAlgorithmTrackCollection& row_track_collection,
      const Vector<LayoutUnit>& column_set_offsets,
      const Vector<LayoutUnit>& row_set_offsets,
      const NGGridPlacement& grid_placement,
      LayoutUnit block_size,
      LayoutUnit column_gutter_size,
      LayoutUnit row_gutter_size);

  // Helper method to compute the containing grid area for grid items or the
  // containing block rect for out of flow elements.
  LogicalRect ComputeContainingGridAreaRect(
      const GridItemData& item,
      const Vector<LayoutUnit>& column_set_offsets,
      const Vector<LayoutUnit>& row_set_offsets,
      LayoutUnit block_size,
      LayoutUnit column_gutter_size,
      LayoutUnit row_gutter_size);

  // Helper method that computes the offset and size of an item.
  void ComputeOffsetAndSize(const GridItemData& item,
                            const Vector<LayoutUnit>& set_offsets,
                            const GridTrackSizingDirection track_direction,
                            LayoutUnit block_size,
                            LayoutUnit gutter_size,
                            LayoutUnit* start_offset,
                            LayoutUnit* size) const;

  // Determines the position of the out of flow item's container.
  void DeterminePositionOfOutOfFlowContainer(
      Vector<GridItemData>* out_of_flow_items,
      const GridTrackSizingDirection track_direction) const;

  GridTrackSizingDirection AutoFlowDirection() const;

  LogicalSize border_box_size_;
  LogicalSize child_percentage_size_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_LAYOUT_ALGORITHM_H_
