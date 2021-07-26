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
  using SetOffsetData = NGGridData::SetData;

  enum class AutoPlacementType { kNotNeeded, kMajor, kMinor, kBoth };
  enum class AxisEdge : uint8_t { kStart, kCenter, kEnd, kBaseline };
  enum class ItemType : uint8_t { kInGridFlow, kOutOfFlow };

  // This enum corresponds to each step used to accommodate grid items across
  // intrinsic tracks according to their min and max track sizing functions, as
  // defined in https://drafts.csswg.org/css-grid-2/#algo-spanning-items.
  enum class GridItemContributionType {
    kForIntrinsicMinimums,
    kForContentBasedMinimums,
    kForMaxContentMinimums,
    kForIntrinsicMaximums,
    kForMaxContentMaximums,
    kForFreeSpace,
  };

  enum class BaselineType : uint8_t {
    kMajor,
    kMinor,
  };

  struct GridItemIndices {
    wtf_size_t begin = kNotFound;
    wtf_size_t end = kNotFound;
  };

  struct OutOfFlowItemPlacement {
    GridItemIndices range_index;
    GridItemIndices offset_in_range;
  };

  struct CORE_EXPORT GridItemData {
    DISALLOW_NEW();

   public:
    explicit GridItemData(const NGBlockNode node) : node(node) {}

    AutoPlacementType AutoPlacement(
        const GridTrackSizingDirection major_direction) const;
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
    bool IsSpanningAutoMinimumTrack(
        GridTrackSizingDirection track_direction) const;

    bool IsBaselineAlignedForDirection(
        GridTrackSizingDirection track_direction) const;
    bool IsBaselineSpecifiedForDirection(
        GridTrackSizingDirection track_direction) const;
    void SetAlignmentFallback(const GridTrackSizingDirection track_direction,
                              const ComputedStyle& container_style,
                              const bool has_synthesized_baseline);

    // For this item and track direction, computes the pair of indices |begin|
    // and |end| such that the item spans every set from the respective
    // collection's |sets_| with an index in the range [begin, end).
    void ComputeSetIndices(
        const NGGridLayoutAlgorithmTrackCollection& track_collection);
    const GridItemIndices& SetIndices(
        GridTrackSizingDirection track_direction) const;
    GridItemIndices& RangeIndices(GridTrackSizingDirection track_direction);

    // For this out of flow item and track collection, computes and stores its
    // first and last spanned ranges, as well as the start and end track offset.
    // |grid_placement| is used to resolve the grid lines.
    void ComputeOutOfFlowItemPlacement(
        const NGGridLayoutAlgorithmTrackCollection& track_collection,
        const NGGridPlacement& grid_placement);

    NGBlockNode node;
    GridArea resolved_position;

    AxisEdge InlineAxisAlignment() const {
      return inline_axis_alignment_fallback.value_or(inline_axis_alignment);
    }

    AxisEdge BlockAxisAlignment() const {
      return block_axis_alignment_fallback.value_or(block_axis_alignment);
    }

    AxisEdge inline_axis_alignment;
    AxisEdge block_axis_alignment;
    absl::optional<AxisEdge> inline_axis_alignment_fallback;
    absl::optional<AxisEdge> block_axis_alignment_fallback;

    bool is_inline_axis_overflow_safe;
    bool is_block_axis_overflow_safe;

    ItemType item_type;
    bool is_grid_containing_block;

    NGAutoBehavior inline_auto_behavior;
    NGAutoBehavior block_auto_behavior;

    BaselineType row_baseline_type;
    BaselineType column_baseline_type;

    TrackSpanProperties column_span_properties;
    TrackSpanProperties row_span_properties;

    GridItemIndices column_set_indices;
    GridItemIndices row_set_indices;

    GridItemIndices column_range_indices;
    GridItemIndices row_range_indices;

    // These fields are only for out of flow items. They are used to store their
    // start/end range indices, and offsets in range in the respective track
    // collection; see |OutOfFlowItemPlacement|.
    OutOfFlowItemPlacement column_placement;
    OutOfFlowItemPlacement row_placement;
  };

  using GridItemVector = Vector<GridItemData*, 16>;
  using GridItemStorageVector = Vector<GridItemData, 4>;

  struct CORE_EXPORT GridItems {
    DISALLOW_NEW();

   public:
    class Iterator
        : public std::iterator<std::input_iterator_tag, GridItemData> {
      STACK_ALLOCATED();
     public:
      Iterator(GridItemStorageVector* item_data,
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
      GridItemStorageVector* item_data_;
      Vector<wtf_size_t>::const_iterator current_index_;
    };

    Iterator begin();
    Iterator end();

    void Append(const GridItemData& new_item_data);

    wtf_size_t Size() const { return item_data.size(); }
    bool IsEmpty() const { return item_data.IsEmpty(); }

    // Grid items are appended in document order, but we want to rearrange them
    // in order-modified document order since auto-placement and painting rely
    // on it later in the algorithm.
    GridItemStorageVector item_data;
    Vector<wtf_size_t> reordered_item_indices;
  };

  // Represents the offsets for the sets, and the gutter-size.
  //
  // Initially we only know some of the set sizes - others will be indefinite.
  // To represent this we store both the offset for the set, and a vector of all
  // last indefinite indices (or kNotFound if everything so far has been
  // definite). This allows us to get the appropriate size if a grid item spans
  // only fixed tracks, but will allow us to return an indefinite size if it
  // spans any indefinite set.
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
    SetGeometry() = default;
    SetGeometry(const Vector<SetOffsetData>& sets, LayoutUnit gutter_size)
        : sets(sets), gutter_size(gutter_size) {}

    LayoutUnit FinalGutterSize() const {
      DCHECK_GT(sets.size(), 0u);
      return (sets.size() == 1) ? LayoutUnit() : gutter_size;
    }

    Vector<wtf_size_t> last_indefinite_indices;
    Vector<SetOffsetData> sets;
    LayoutUnit gutter_size;
  };

  // Typically we pass around both the column, and row geometry together.
  struct GridGeometry {
    GridGeometry(SetGeometry&& column_geometry, SetGeometry&& row_geometry)
        : column_geometry(column_geometry),
          row_geometry(row_geometry),
          major_inline_baselines(column_geometry.sets.size(),
                                 LayoutUnit::Min()),
          minor_inline_baselines(column_geometry.sets.size(),
                                 LayoutUnit::Min()),
          major_block_baselines(row_geometry.sets.size(), LayoutUnit::Min()),
          minor_block_baselines(row_geometry.sets.size(), LayoutUnit::Min()) {}

    GridGeometry() = default;

    const SetGeometry& Geometry(GridTrackSizingDirection track_direction) const;

    // Updates stored major/minor baseline value.
    void UpdateBaseline(const GridItemData& grid_item,
                        LayoutUnit candidate_baseline,
                        GridTrackSizingDirection track_direction);

    // Retrieves major/minor baseline.
    LayoutUnit Baseline(const GridItemData& grid_item,
                        GridTrackSizingDirection track_direction) const;

    SetGeometry column_geometry;
    SetGeometry row_geometry;

    Vector<LayoutUnit> major_inline_baselines;
    Vector<LayoutUnit> minor_inline_baselines;
    Vector<LayoutUnit> major_block_baselines;
    Vector<LayoutUnit> minor_block_baselines;
  };

  explicit NGGridLayoutAlgorithm(const NGLayoutAlgorithmParams& params);

  scoped_refptr<const NGLayoutResult> Layout() override;
  MinMaxSizesResult ComputeMinMaxSizes(
      const MinMaxSizesFloatInput&) const override;

  // Computes the containing block rect of out of flow items from stored data
  // in |NGGridData|.
  static absl::optional<LogicalRect> ComputeContainingBlockRect(
      const NGBlockNode& node,
      const NGGridData& grid_data,
      const ComputedStyle& grid_style,
      const WritingMode container_writing_mode,
      const NGBoxStrut& borders,
      const LogicalSize& border_box_size,
      const LayoutUnit block_size);

  // Helper that computes tracks sizes in a given range.
  static Vector<std::div_t> ComputeTrackSizesInRange(
      const SetGeometry& set_geometry,
      wtf_size_t range_starting_set_index,
      wtf_size_t range_set_count);

 private:
  friend class NGGridLayoutAlgorithmTest;

  enum class SizingConstraint { kLayout, kMinContent, kMaxContent };

  LayoutUnit ComputeIntrinsicBlockSizeIgnoringChildren() const;

  // Returns the size that a grid item will distribute across the tracks with an
  // intrinsic sizing function it spans in the relevant track direction.
  LayoutUnit ContributionSizeForGridItem(
      SizingConstraint sizing_constraint,
      const GridGeometry& grid_geometry,
      const GridItemData& grid_item,
      GridTrackSizingDirection track_direction,
      GridItemContributionType contribution_type,
      bool* needs_additional_pass,
      bool* has_block_size_dependent_item) const;

  wtf_size_t ComputeAutomaticRepetitions(
      GridTrackSizingDirection track_direction) const;

  void ConstructAndAppendGridItems(
      GridItems* grid_items,
      GridItemStorageVector* out_of_flow_items = nullptr) const;

  static GridItemData MeasureGridItem(const NGBlockNode node,
                                      const ComputedStyle& container_style,
                                      const WritingMode container_writing_mode);

  void BuildBlockTrackCollections(
      GridItems* grid_items,
      NGGridBlockTrackCollection* column_track_collection,
      NGGridBlockTrackCollection* row_track_collection,
      NGGridPlacement* grid_placement) const;

  // Ensure coverage in block collection after grid items have been placed.
  void EnsureTrackCoverageForGridItems(
      GridItems* grid_items,
      NGGridBlockTrackCollection* track_collection) const;

  // For every grid item, caches properties of the track sizing functions it
  // spans (i.e. whether an item spans intrinsic or flexible tracks).
  void CacheGridItemsTrackSpanProperties(
      const NGGridLayoutAlgorithmTrackCollection& track_collection,
      GridItems* grid_items) const;

  // Returns 'true' if it's possible to layout a grid item.
  bool CanLayoutGridItem(const GridItemData& grid_item,
                         const NGConstraintSpace& space,
                         const GridTrackSizingDirection track_direction) const;

  // Determines the major/minor alignment baselines for each row/column based on
  // each item in |grid_items|, and stores the results in |grid_geometry|.
  void CalculateAlignmentBaselines(
      const GridTrackSizingDirection track_direction,
      GridGeometry* grid_geometry,
      GridItems* grid_items,
      bool* needs_additional_pass) const;

  // Initializes the given track collection, and returns the base set geometry.
  SetGeometry InitializeTrackSizes(
      NGGridLayoutAlgorithmTrackCollection* track_collection) const;

  // Calculates from the min and max track sizing functions the used track size.
  SetGeometry ComputeUsedTrackSizes(
      SizingConstraint sizing_constraint,
      const GridGeometry& grid_geometry,
      NGGridLayoutAlgorithmTrackCollection* track_collection,
      GridItems* grid_items,
      bool* needs_additional_pass,
      bool* has_block_size_dependent_item = nullptr) const;

  // These methods implement the steps of the algorithm for intrinsic track size
  // resolution defined in https://drafts.csswg.org/css-grid-2/#algo-content.
  void ResolveIntrinsicTrackSizes(
      SizingConstraint sizing_constraint,
      const GridGeometry& grid_geometry,
      NGGridLayoutAlgorithmTrackCollection* track_collection,
      GridItems* grid_items,
      bool* needs_additional_pass,
      bool* has_block_size_dependent_item) const;

  void IncreaseTrackSizesToAccommodateGridItems(
      SizingConstraint sizing_constraint,
      const GridGeometry& grid_geometry,
      GridItems::Iterator group_begin,
      GridItems::Iterator group_end,
      const bool is_group_spanning_flex_track,
      GridItemContributionType contribution_type,
      NGGridLayoutAlgorithmTrackCollection* track_collection,
      bool* needs_additional_pass,
      bool* has_block_size_dependent_item) const;

  void MaximizeTracks(
      SizingConstraint sizing_constraint,
      NGGridLayoutAlgorithmTrackCollection* track_collection) const;

  void StretchAutoTracks(
      SizingConstraint sizing_constraint,
      NGGridLayoutAlgorithmTrackCollection* track_collection) const;

  void ExpandFlexibleTracks(
      SizingConstraint sizing_constraint,
      const GridGeometry& grid_geometry,
      NGGridLayoutAlgorithmTrackCollection* track_collection,
      GridItems* grid_items,
      bool* needs_additional_pass,
      bool* has_block_size_dependent_item) const;

  SetGeometry ComputeSetGeometry(
      const NGGridLayoutAlgorithmTrackCollection& track_collection) const;

  // Gets the row or column gap of the grid.
  LayoutUnit GridGap(GridTrackSizingDirection track_direction) const;

  LayoutUnit DetermineFreeSpace(
      SizingConstraint sizing_constraint,
      const NGGridLayoutAlgorithmTrackCollection& track_collection) const;

  const NGConstraintSpace CreateConstraintSpace(
      const GridItemData& grid_item,
      const LogicalSize& containing_grid_area_size,
      absl::optional<LayoutUnit> opt_fixed_block_size,
      NGCacheSlot cache_slot) const;

  const NGConstraintSpace CreateConstraintSpaceForLayout(
      const GridGeometry& grid_geometry,
      const GridItemData& grid_item,
      LogicalRect* containing_grid_area) const;

  const NGConstraintSpace CreateConstraintSpaceForMeasure(
      const GridGeometry& grid_geometry,
      const GridItemData& grid_item,
      GridTrackSizingDirection track_direction,
      absl::optional<LayoutUnit> opt_fixed_block_size = absl::nullopt) const;

  // Layout the |grid_items| based on the offsets provided.
  void PlaceGridItems(const GridItems& grid_items,
                      const GridGeometry& grid_geometry,
                      LayoutUnit block_size);

  // Computes the static position, grid area and its offset of out of flow
  // elements in the grid.
  void PlaceOutOfFlowItems(
      const NGGridLayoutAlgorithmTrackCollection& column_track_collection,
      const NGGridLayoutAlgorithmTrackCollection& row_track_collection,
      const GridItemStorageVector& out_of_flow_items,
      const GridGeometry& grid_geometry,
      LayoutUnit block_size);

  // Helper method to compute the containing block rect for out of flow
  // elements.
  static LogicalRect ComputeContainingGridAreaRect(
      const NGGridLayoutAlgorithmTrackCollection& column_track_collection,
      const NGGridLayoutAlgorithmTrackCollection& row_track_collection,
      const GridGeometry& grid_geometry,
      const GridItemData& item,
      const NGBoxStrut& borders,
      const LogicalSize& border_box_size,
      LayoutUnit block_size);

  void ComputeGridItemOffsetAndSize(
      const GridItemData& grid_item,
      const SetGeometry& set_geometry,
      const GridTrackSizingDirection track_direction,
      LayoutUnit* start_offset,
      LayoutUnit* size) const;

  static void ComputeOutOfFlowOffsetAndSize(
      const GridItemData& out_of_flow_item,
      const SetGeometry& set_geometry,
      const NGGridLayoutAlgorithmTrackCollection& track_collection,
      const NGBoxStrut& borders,
      const LogicalSize& border_box_size,
      LayoutUnit block_size,
      LayoutUnit* start_offset,
      LayoutUnit* size);

  NGGridData::TrackCollectionGeometry ConvertSetGeometry(
      const SetGeometry& set_geometry,
      const NGGridLayoutAlgorithmTrackCollection& track_collection) const;

  LogicalSize border_box_size_;

  LogicalSize grid_available_size_;
  LogicalSize grid_min_available_size_;
  LogicalSize grid_max_available_size_;

  absl::optional<LayoutUnit> contain_intrinsic_block_size_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_LAYOUT_ALGORITHM_H_
