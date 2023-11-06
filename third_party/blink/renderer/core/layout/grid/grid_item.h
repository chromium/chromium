// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_ITEM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_ITEM_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"
#include "third_party/blink/renderer/core/layout/ng/ng_baseline_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

enum class AxisEdge { kStart, kCenter, kEnd, kFirstBaseline, kLastBaseline };
enum class SizingConstraint { kLayout, kMinContent, kMaxContent };

struct GridItemIndices {
  wtf_size_t begin{kNotFound};
  wtf_size_t end{kNotFound};
};

struct OutOfFlowItemPlacement {
  GridItemIndices range_index;
  GridItemIndices offset_in_range;
};

struct CORE_EXPORT GridItemData {
  USING_FAST_MALLOC(GridItemData);

 public:
  GridItemData() = delete;
  GridItemData(const GridItemData&) = default;
  GridItemData& operator=(const GridItemData&) = default;

  GridItemData(NGBlockNode node,
               const ComputedStyle& root_grid_style,
               FontBaseline parent_grid_font_baseline,
               bool parent_must_consider_grid_items_for_column_sizing = false,
               bool parent_must_consider_grid_items_for_row_sizing = false);

  void SetAlignmentFallback(GridTrackSizingDirection track_direction,
                            bool has_synthesized_baseline);

  AxisEdge InlineAxisAlignment() const {
    return inline_axis_alignment_fallback.value_or(inline_axis_alignment);
  }
  AxisEdge BlockAxisAlignment() const {
    return block_axis_alignment_fallback.value_or(block_axis_alignment);
  }

  bool IsInlineAxisOverflowSafe() const {
    return is_inline_axis_overflow_safe_fallback.value_or(
        is_inline_axis_overflow_safe);
  }
  bool IsBlockAxisOverflowSafe() const {
    return is_block_axis_overflow_safe_fallback.value_or(
        is_block_axis_overflow_safe);
  }

  bool IsBaselineAligned(GridTrackSizingDirection track_direction) const {
    const bool is_for_columns = track_direction == kForColumns;
    const bool has_subgridded_axis =
        is_for_columns ? has_subgridded_columns : has_subgridded_rows;

    if (has_subgridded_axis) {
      return false;
    }

    const auto axis_alignment =
        is_for_columns ? InlineAxisAlignment() : BlockAxisAlignment();
    return (axis_alignment == AxisEdge::kFirstBaseline ||
            axis_alignment == AxisEdge::kLastBaseline);
  }

  bool IsBaselineSpecified(GridTrackSizingDirection track_direction) const {
    const bool is_for_columns = track_direction == kForColumns;
    const bool has_subgridded_axis =
        is_for_columns ? has_subgridded_columns : has_subgridded_rows;

    if (has_subgridded_axis) {
      return false;
    }

    const auto axis_alignment =
        is_for_columns ? inline_axis_alignment : block_axis_alignment;
    return (axis_alignment == AxisEdge::kFirstBaseline ||
            axis_alignment == AxisEdge::kLastBaseline);
  }

  bool IsLastBaselineSpecified(GridTrackSizingDirection track_direction) const {
    return (track_direction == kForColumns)
               ? inline_axis_alignment == AxisEdge::kLastBaseline
               : block_axis_alignment == AxisEdge::kLastBaseline;
  }

  // For this item and track direction, computes the pair of indices |begin| and
  // |end| such that the item spans every set from the respective collection's
  // |sets_| with an index in the range [begin, end).
  void ComputeSetIndices(const GridLayoutTrackCollection& track_collection);

  // For this out of flow item and track collection, computes and stores its
  // first and last spanned ranges, as well as the start and end track offset.
  // |grid_placement| is used to resolve the grid lines.
  void ComputeOutOfFlowItemPlacement(
      const GridLayoutTrackCollection& track_collection,
      const GridPlacementData& placement_data,
      const ComputedStyle& grid_style);

  enum BaselineGroup BaselineGroup(
      GridTrackSizingDirection track_direction) const {
    return (track_direction == kForColumns) ? column_baseline_group
                                            : row_baseline_group;
  }

  WritingDirectionMode BaselineWritingDirection(
      GridTrackSizingDirection track_direction) const {
    // NOTE: For reading the baseline from a fragment the direction doesn't
    // matter - just use the default.
    return {(track_direction == kForColumns) ? column_baseline_writing_mode
                                             : row_baseline_writing_mode,
            TextDirection::kLtr};
  }

  const GridItemIndices& SetIndices(
      GridTrackSizingDirection track_direction) const {
    return (track_direction == kForColumns) ? column_set_indices
                                            : row_set_indices;
  }

  GridItemIndices& RangeIndices(GridTrackSizingDirection track_direction) {
    return (track_direction == kForColumns) ? column_range_indices
                                            : row_range_indices;
  }

  void ResetPlacementIndices() {
    column_range_indices = row_range_indices = GridItemIndices();
    column_set_indices = row_set_indices = GridItemIndices();
  }

  const GridSpan& Span(GridTrackSizingDirection track_direction) const {
    return resolved_position.Span(track_direction);
  }
  wtf_size_t StartLine(GridTrackSizingDirection track_direction) const {
    return resolved_position.StartLine(track_direction);
  }
  wtf_size_t EndLine(GridTrackSizingDirection track_direction) const {
    return resolved_position.EndLine(track_direction);
  }
  wtf_size_t SpanSize(GridTrackSizingDirection track_direction) const {
    return resolved_position.SpanSize(track_direction);
  }

  bool IsSubgrid() const {
    return has_subgridded_columns || has_subgridded_rows;
  }

  bool IsConsideredForSizing(GridTrackSizingDirection track_direction) const {
    return (track_direction == kForColumns) ? is_considered_for_column_sizing
                                            : is_considered_for_row_sizing;
  }

  bool IsOppositeDirectionInRootGrid(
      GridTrackSizingDirection track_direction) const {
    return (track_direction == kForColumns)
               ? is_opposite_direction_in_root_grid_columns
               : is_opposite_direction_in_root_grid_rows;
  }

  bool MustCachePlacementIndices(
      GridTrackSizingDirection track_direction) const {
    return !is_subgridded_to_parent_grid ||
           IsConsideredForSizing(track_direction) ||
           MustConsiderGridItemsForSizing(track_direction);
  }

  bool MustConsiderGridItemsForSizing(
      GridTrackSizingDirection track_direction) const {
    return (track_direction == kForColumns)
               ? must_consider_grid_items_for_column_sizing
               : must_consider_grid_items_for_row_sizing;
  }

  bool IsOutOfFlow() const { return node.IsOutOfFlowPositioned(); }

  const TrackSpanProperties& GetTrackSpanProperties(
      GridTrackSizingDirection track_direction) const {
    return (track_direction == kForColumns) ? column_span_properties
                                            : row_span_properties;
  }

  void SetTrackSpanProperty(const TrackSpanProperties::PropertyId property,
                            GridTrackSizingDirection track_direction) {
    if (track_direction == kForColumns)
      column_span_properties.SetProperty(property);
    else
      row_span_properties.SetProperty(property);
  }

  bool IsSpanningFlexibleTrack(GridTrackSizingDirection track_direction) const {
    return GetTrackSpanProperties(track_direction)
        .HasProperty(TrackSpanProperties::kHasFlexibleTrack);
  }
  bool IsSpanningIntrinsicTrack(
      GridTrackSizingDirection track_direction) const {
    return GetTrackSpanProperties(track_direction)
        .HasProperty(TrackSpanProperties::kHasIntrinsicTrack);
  }
  bool IsSpanningAutoMinimumTrack(
      GridTrackSizingDirection track_direction) const {
    return GetTrackSpanProperties(track_direction)
        .HasProperty(TrackSpanProperties::kHasAutoMinimumTrack);
  }
  bool IsSpanningFixedMinimumTrack(
      GridTrackSizingDirection track_direction) const {
    return GetTrackSpanProperties(track_direction)
        .HasProperty(TrackSpanProperties::kHasFixedMinimumTrack);
  }
  bool IsSpanningFixedMaximumTrack(
      GridTrackSizingDirection track_direction) const {
    return GetTrackSpanProperties(track_direction)
        .HasProperty(TrackSpanProperties::kHasFixedMaximumTrack);
  }

  void Trace(Visitor* visitor) const { visitor->Trace(node); }

  NGBlockNode node;
  GridArea resolved_position;

  bool has_subgridded_columns : 1;
  bool has_subgridded_rows : 1;
  bool is_block_axis_overflow_safe : 1;
  bool is_considered_for_column_sizing : 1;
  bool is_considered_for_row_sizing : 1;
  bool is_inline_axis_overflow_safe : 1;
  bool is_parallel_with_root_grid : 1;
  bool is_sizing_dependent_on_block_size : 1;
  bool is_subgridded_to_parent_grid : 1;
  bool is_opposite_direction_in_root_grid_columns : 1;
  bool is_opposite_direction_in_root_grid_rows : 1;
  bool must_consider_grid_items_for_column_sizing : 1;
  bool must_consider_grid_items_for_row_sizing : 1;

  FontBaseline parent_grid_font_baseline;

  AxisEdge inline_axis_alignment;
  AxisEdge block_axis_alignment;

  absl::optional<AxisEdge> inline_axis_alignment_fallback;
  absl::optional<AxisEdge> block_axis_alignment_fallback;

  absl::optional<bool> is_inline_axis_overflow_safe_fallback;
  absl::optional<bool> is_block_axis_overflow_safe_fallback;

  NGAutoBehavior inline_auto_behavior;
  NGAutoBehavior block_auto_behavior;

  enum BaselineGroup column_baseline_group;
  enum BaselineGroup row_baseline_group;

  WritingMode column_baseline_writing_mode;
  WritingMode row_baseline_writing_mode;

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

class CORE_EXPORT GridItems {
  DISALLOW_NEW();

 public:
  using GridItemDataVector = Vector<std::unique_ptr<GridItemData>, 16>;

  template <bool is_const>
  class IteratorBase {
    STACK_ALLOCATED();

   public:
    using value_type = typename std::
        conditional<is_const, const GridItemData, GridItemData>::type;

    using GridItemDataVectorPtr =
        typename std::conditional<is_const,
                                  const GridItemDataVector*,
                                  GridItemDataVector*>::type;

    IteratorBase(GridItemDataVectorPtr item_data, wtf_size_t current_index)
        : current_index_(current_index), item_data_(item_data) {
      DCHECK(item_data_);
      DCHECK_LE(current_index_, item_data_->size());
    }

    bool operator!=(const IteratorBase& other) {
      return current_index_ != other.current_index_ ||
             item_data_ != other.item_data_;
    }

    IteratorBase& operator++() {
      ++current_index_;
      return *this;
    }

    IteratorBase operator++(int) {
      auto current_iterator = *this;
      ++current_index_;
      return current_iterator;
    }

    value_type* operator->() const {
      DCHECK_LT(current_index_, item_data_->size());
      return item_data_->at(current_index_).get();
    }

    value_type& operator*() const { return *operator->(); }

   private:
    wtf_size_t current_index_;
    GridItemDataVectorPtr item_data_;
  };

  typedef IteratorBase<false> Iterator;
  typedef IteratorBase<true> ConstIterator;

  GridItems() = default;
  GridItems(GridItems&&) = default;
  GridItems& operator=(GridItems&&) = default;

  GridItems(const GridItems& other);

  GridItems& operator=(const GridItems& other) {
    return *this = GridItems(other);
  }

  Iterator begin() { return {&item_data_, 0}; }
  Iterator end() { return {&item_data_, item_data_.size()}; }

  ConstIterator begin() const { return {&item_data_, 0}; }
  ConstIterator end() const { return {&item_data_, item_data_.size()}; }

  bool IsEmpty() const { return item_data_.empty(); }
  wtf_size_t Size() const { return item_data_.size(); }

  void Append(GridItems* other);
  void RemoveSubgriddedItems();
  void SortByOrderProperty();

  void Append(std::unique_ptr<GridItemData>&& new_item_data) {
    item_data_.emplace_back(std::move(new_item_data));
  }

  GridItemData& At(wtf_size_t index) {
    DCHECK(item_data_[index]);
    return *item_data_[index];
  }

  void ReserveInitialCapacity(wtf_size_t initial_capacity) {
    item_data_.ReserveInitialCapacity(initial_capacity);
  }

 private:
  // Grid items are rearranged in order-modified document order since
  // auto-placement and painting rely on it later in the algorithm.
  GridItemDataVector item_data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_ITEM_H_
