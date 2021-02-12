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

  struct CORE_EXPORT GridItemData {
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

  struct CORE_EXPORT GridItems {
    class Iterator
        : public std::iterator<std::input_iterator_tag, GridItemData> {
     public:
      Iterator(Vector<GridItemData>* item_data,
               Vector<wtf_size_t>::const_iterator current_index)
          : item_data_(item_data), current_index_(current_index) {
        DCHECK(item_data_);
      }

      bool operator!=(const Iterator& other) const {
        return current_index_ != other.current_index_ ||
               item_data_ != other.item_data_;
      }

      Iterator& operator++() {
        ++current_index_;
        return *this;
      }

      GridItemData* operator->() {
        DCHECK(current_index_ && *current_index_ < item_data_->size());
        return &(item_data_->at(*current_index_));
      }

      GridItemData& operator*() {
        DCHECK(current_index_ && *current_index_ < item_data_->size());
        return item_data_->at(*current_index_);
      }

     private:
      Vector<GridItemData>* item_data_;
      Vector<wtf_size_t>::const_iterator current_index_;
    };

    Iterator begin();
    Iterator end();

    void Append(const GridItemData& new_item_data);

    bool IsEmpty() const;

    // Grid items are appended to |item_data_| in the same order provided by
    // |NGGridChildIterator|, which iterates over its children in order-modified
    // document order; we want to keep such order since auto-placement and
    // painting order rely on it later in the algorithm.
    Vector<GridItemData> item_data;
    Vector<wtf_size_t> reordered_item_indices;
  };

  // See |SetGeometry|.
  struct SetOffsetData {
    SetOffsetData(LayoutUnit offset, wtf_size_t last_indefinite_index)
        : offset(offset), last_indefinite_index(last_indefinite_index) {}
    LayoutUnit offset;
    wtf_size_t last_indefinite_index;
  };

  // Represents the offsets for the sets, and the gutter-size.
  //
  // Initially we only know some of the set sizes - others will be indefinite.
  // To represent this we store both the offset for the set, and the last index
  // where there was an indefinite set (or kNotFound if everything so far has
  // been definite). This allows us to get the appropriate size if a grid item
  // spans only fixed tracks, but will allow us to return an indefinite size if
  // it spans any indefinite set.
  //
  // As an example:
  //   grid-template-rows: auto auto 100px 100px auto 100px;
  //
  // Results in:
  //                  |  auto |  auto |   100   |   100   |   auto  |   100   |
  //   [{0, kNotFound}, {0, 0}, {0, 1}, {100, 1}, {200, 1}, {200, 4}, {300, 4}]
  //
  // Various queries (start/end refer to the grid lines):
  //  start: 0, end: 1 -> indefinite as:
  //    "start <= sets[end].last_indefinite_index"
  //  start: 1, end: 3 -> indefinite as:
  //    "start <= sets[end].last_indefinite_index"
  //  start: 2, end: 4 -> 200px
  //  start: 5, end: 6 -> 100px
  //  start: 3, end: 5 -> indefinite as:
  //    "start <= sets[end].last_indefinite_index"
  struct SetGeometry {
    Vector<SetOffsetData> sets;
    LayoutUnit gutter_size;
  };

  // Typically we pass around both the column, and row geometry together.
  struct GridGeometry {
    SetGeometry column_geometry;
    SetGeometry row_geometry;
  };

  explicit NGGridLayoutAlgorithm(const NGLayoutAlgorithmParams& params);

  scoped_refptr<const NGLayoutResult> Layout() override;
  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesInput&) const override;

 private:
  friend class NGGridLayoutAlgorithmTest;

  enum class SizingConstraint { kLayout, kMinContent, kMaxContent };

  // Returns the size that a grid item will distribute across the tracks with an
  // intrinsic sizing function it spans in the relevant track direction.
  LayoutUnit ContributionSizeForGridItem(
      const GridGeometry& grid_geometry,
      const GridItemData& grid_item,
      GridTrackSizingDirection track_direction,
      GridItemContributionType contribution_type) const;

  wtf_size_t ComputeAutomaticRepetitions(
      GridTrackSizingDirection track_direction) const;

  void ConstructAndAppendGridItems(
      GridItems* grid_items,
      Vector<GridItemData>* out_of_flow_items = nullptr) const;
  GridItemData MeasureGridItem(const NGBlockNode node) const;

  void BuildBlockTrackCollections(
      GridItems* grid_items,
      NGGridBlockTrackCollection* column_track_collection,
      NGGridBlockTrackCollection* row_track_collection,
      NGGridPlacement* grid_placement) const;

  void BuildAlgorithmTrackCollections(
      GridItems* grid_items,
      NGGridLayoutAlgorithmTrackCollection* column_track_collection,
      NGGridLayoutAlgorithmTrackCollection* row_track_collection,
      NGGridPlacement* grid_placement) const;

  // Sets specified track lists on |track_collection|.
  void SetSpecifiedTracks(wtf_size_t auto_repetitions,
                          NGGridBlockTrackCollection* track_collection) const;
  // Ensure coverage in block collection after grid items have been placed.
  void EnsureTrackCoverageForGridItems(
      const GridItems& grid_items,
      NGGridBlockTrackCollection* track_collection) const;

  // For every grid item, caches properties of the track sizing functions it
  // spans (i.e. whether an item spans intrinsic or flexible tracks).
  void CacheGridItemsTrackSpanProperties(
      const NGGridLayoutAlgorithmTrackCollection& track_collection,
      GridItems* grid_items) const;

  // Initializes the given track collection, and returns the base set geometry.
  SetGeometry InitializeTrackSizes(
      NGGridLayoutAlgorithmTrackCollection* track_collection) const;

  // Calculates from the min and max track sizing functions the used track size.
  void ComputeUsedTrackSizes(
      SizingConstraint sizing_constraint,
      const GridGeometry& grid_geometry,
      NGGridLayoutAlgorithmTrackCollection* track_collection,
      GridItems* grid_items) const;

  // These methods implement the steps of the algorithm for intrinsic track size
  // resolution defined in https://drafts.csswg.org/css-grid-2/#algo-content.
  void ResolveIntrinsicTrackSizes(
      const GridGeometry& grid_geometry,
      NGGridLayoutAlgorithmTrackCollection* track_collection,
      GridItems* grid_items) const;

  void IncreaseTrackSizesToAccommodateGridItems(
      const GridGeometry& grid_geometry,
      GridItems::Iterator group_begin,
      GridItems::Iterator group_end,
      const bool is_group_spanning_flex_track,
      GridItemContributionType contribution_type,
      NGGridLayoutAlgorithmTrackCollection* track_collection) const;

  void MaximizeTracks(
      SizingConstraint sizing_constraint,
      NGGridLayoutAlgorithmTrackCollection* track_collection) const;

  void StretchAutoTracks(
      SizingConstraint sizing_constraint,
      NGGridLayoutAlgorithmTrackCollection* track_collection) const;

  SetGeometry ComputeSetGeometry(
      const NGGridLayoutAlgorithmTrackCollection& track_collection,
      const LayoutUnit available_size) const;

  // Gets the row or column gap of the grid.
  LayoutUnit GridGap(GridTrackSizingDirection track_direction,
                     LayoutUnit available_size = kIndefiniteSize) const;

  LayoutUnit DetermineFreeSpace(
      SizingConstraint sizing_constraint,
      const NGGridLayoutAlgorithmTrackCollection& track_collection) const;

  const NGConstraintSpace CreateConstraintSpace(
      const GridGeometry& grid_geometry,
      const GridItemData& grid_item,
      NGCacheSlot cache_slot,
      LogicalRect* rect) const;

  // Layout the |grid_items| based on the offsets provided.
  void PlaceGridItems(const GridItems& grid_items,
                      const GridGeometry& grid_geometry,
                      LayoutUnit block_size);

  // Computes the static position, grid area and its offset of out of flow
  // elements in the grid.
  void PlaceOutOfFlowItems(const Vector<GridItemData>& out_of_flow_items,
                           const GridGeometry& grid_geometry,
                           LayoutUnit block_size);

  // Gets the out of flow descendants from the container builder and computes
  // their containing block rect.
  void PlaceOutOfFlowDescendants(
      const NGGridLayoutAlgorithmTrackCollection& column_track_collection,
      const NGGridLayoutAlgorithmTrackCollection& row_track_collection,
      const GridGeometry& grid_geometry,
      const NGGridPlacement& grid_placement,
      LayoutUnit block_size);

  // Helper method to compute the containing grid area for grid items or the
  // containing block rect for out of flow elements.
  LogicalRect ComputeContainingGridAreaRect(const GridGeometry& grid_geometry,
                                            const GridItemData& item,
                                            LayoutUnit block_size);

  // Helper method that computes the offset and size of an item.
  void ComputeOffsetAndSize(const GridItemData& item,
                            const SetGeometry& set_geometry,
                            const GridTrackSizingDirection track_direction,
                            LayoutUnit block_size,
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
