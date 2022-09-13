// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_ITEM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_ITEM_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_track_collection.h"
#include "third_party/blink/renderer/core/layout/ng/ng_baseline_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class NGGridPlacement;

enum class AxisEdge { kStart, kCenter, kEnd, kBaseline };
enum class SizingConstraint { kLayout, kMinContent, kMaxContent };

struct GridItemIndices {
  wtf_size_t begin = kNotFound;
  wtf_size_t end = kNotFound;
};

struct OutOfFlowItemPlacement {
  GridItemIndices range_index;
  GridItemIndices offset_in_range;
};

struct CORE_EXPORT GridItemData : public GarbageCollected<GridItemData> {
  GridItemData(const NGBlockNode node, const ComputedStyle& container_style);

  void SetAlignmentFallback(const GridTrackSizingDirection track_direction,
                            const ComputedStyle& container_style,
                            const bool has_synthesized_baseline);

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

  bool IsBaselineAlignedForDirection(
      const GridTrackSizingDirection track_direction) const {
    return (track_direction == kForColumns)
               ? InlineAxisAlignment() == AxisEdge::kBaseline
               : BlockAxisAlignment() == AxisEdge::kBaseline;
  }
  bool IsBaselineSpecifiedForDirection(
      const GridTrackSizingDirection track_direction) const {
    return (track_direction == kForColumns)
               ? inline_axis_alignment == AxisEdge::kBaseline
               : block_axis_alignment == AxisEdge::kBaseline;
  }

  // For this item and track direction, computes the pair of indices |begin| and
  // |end| such that the item spans every set from the respective collection's
  // |sets_| with an index in the range [begin, end).
  void ComputeSetIndices(const NGGridLayoutTrackCollection& track_collection);

  // For this out of flow item and track collection, computes and stores its
  // first and last spanned ranges, as well as the start and end track offset.
  // |grid_placement| is used to resolve the grid lines.
  void ComputeOutOfFlowItemPlacement(
      const NGGridLayoutTrackCollection& track_collection,
      const NGGridPlacement& grid_placement);

  enum BaselineGroup BaselineGroup(
      const GridTrackSizingDirection track_direction) const {
    return (track_direction == kForColumns) ? column_baseline_group
                                            : row_baseline_group;
  }

  WritingDirectionMode BaselineWritingDirection(
      const GridTrackSizingDirection track_direction) const {
    // NOTE: For reading the baseline from a fragment the direction doesn't
    // matter - just use the default.
    return {(track_direction == kForColumns) ? column_baseline_writing_mode
                                             : row_baseline_writing_mode,
            TextDirection::kLtr};
  }

  const GridItemIndices& SetIndices(
      const GridTrackSizingDirection track_direction) const {
    return (track_direction == kForColumns) ? column_set_indices
                                            : row_set_indices;
  }
  GridItemIndices& RangeIndices(
      const GridTrackSizingDirection track_direction) {
    return (track_direction == kForColumns) ? column_range_indices
                                            : row_range_indices;
  }

  const GridSpan& Span(const GridTrackSizingDirection track_direction) const {
    return resolved_position.Span(track_direction);
  }
  wtf_size_t StartLine(const GridTrackSizingDirection track_direction) const {
    return resolved_position.StartLine(track_direction);
  }
  wtf_size_t EndLine(const GridTrackSizingDirection track_direction) const {
    return resolved_position.EndLine(track_direction);
  }
  wtf_size_t SpanSize(const GridTrackSizingDirection track_direction) const {
    return resolved_position.SpanSize(track_direction);
  }

  bool HasSubgriddedAxis(const GridTrackSizingDirection track_direction) const {
    if (node.IsGrid()) {
      return (track_direction == kForColumns)
                 ? node.Style().GridTemplateColumns().IsSubgriddedAxis()
                 : node.Style().GridTemplateRows().IsSubgriddedAxis();
    }
    return false;
  }

  GridItemData* ParentGrid() const { return parent_grid.Get(); }

  bool IsConsideredForSizing(
      const GridTrackSizingDirection track_direction) const {
    return (track_direction == kForColumns) ? is_considered_for_column_sizing
                                            : is_considered_for_row_sizing;
  }

  bool IsGridContainingBlock() const { return node.IsContainingBlockNGGrid(); }
  bool IsOutOfFlow() const { return node.IsOutOfFlowPositioned(); }

  const TrackSpanProperties& GetTrackSpanProperties(
      const GridTrackSizingDirection track_direction) const {
    return (track_direction == kForColumns) ? column_span_properties
                                            : row_span_properties;
  }
  void SetTrackSpanProperty(const TrackSpanProperties::PropertyId property,
                            const GridTrackSizingDirection track_direction) {
    if (track_direction == kForColumns)
      column_span_properties.SetProperty(property);
    else
      row_span_properties.SetProperty(property);
  }

  bool IsSpanningFlexibleTrack(
      const GridTrackSizingDirection track_direction) const {
    return GetTrackSpanProperties(track_direction)
        .HasProperty(TrackSpanProperties::kHasFlexibleTrack);
  }
  bool IsSpanningIntrinsicTrack(
      const GridTrackSizingDirection track_direction) const {
    return GetTrackSpanProperties(track_direction)
        .HasProperty(TrackSpanProperties::kHasIntrinsicTrack);
  }
  bool IsSpanningAutoMinimumTrack(
      const GridTrackSizingDirection track_direction) const {
    return GetTrackSpanProperties(track_direction)
        .HasProperty(TrackSpanProperties::kHasAutoMinimumTrack);
  }
  bool IsSpanningFixedMinimumTrack(
      const GridTrackSizingDirection track_direction) const {
    return GetTrackSpanProperties(track_direction)
        .HasProperty(TrackSpanProperties::kHasFixedMinimumTrack);
  }
  bool IsSpanningFixedMaximumTrack(
      const GridTrackSizingDirection track_direction) const {
    return GetTrackSpanProperties(track_direction)
        .HasProperty(TrackSpanProperties::kHasFixedMaximumTrack);
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(node);
    visitor->Trace(parent_grid);
  }

  NGBlockNode node;
  GridArea resolved_position;
  Member<GridItemData> parent_grid;

  bool is_block_axis_overflow_safe : 1;
  bool is_inline_axis_overflow_safe : 1;
  bool is_sizing_dependent_on_block_size : 1;
  bool is_considered_for_column_sizing : 1;
  bool is_considered_for_row_sizing : 1;
  bool can_subgrid_items_in_column_direction : 1;
  bool can_subgrid_items_in_row_direction : 1;

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

struct CORE_EXPORT GridItems {
  STACK_ALLOCATED();

 public:
  using GridItemDataVector = HeapVector<Member<GridItemData>, 16>;

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
      return item_data_->at(current_index_).Get();
    }

    value_type& operator*() const { return *operator->(); }

   private:
    wtf_size_t current_index_;
    GridItemDataVectorPtr item_data_;
  };

  typedef IteratorBase<false> Iterator;
  typedef IteratorBase<true> ConstIterator;

  Iterator begin() { return {&item_data, 0}; }
  Iterator end() { return {&item_data, item_data.size()}; }

  ConstIterator begin() const { return {&item_data, 0}; }
  ConstIterator end() const { return {&item_data, item_data.size()}; }

  wtf_size_t Size() const { return item_data.size(); }
  bool IsEmpty() const { return item_data.IsEmpty(); }

  GridItemData& operator[](wtf_size_t index) { return *item_data[index]; }

  void Append(GridItemData* new_item_data) {
    DCHECK(new_item_data);
    item_data.emplace_back(new_item_data);
  }

  void RemoveSubgriddedItems();

  void ReserveInitialCapacity(wtf_size_t initial_capacity) {
    item_data.ReserveInitialCapacity(initial_capacity);
  }
  void ReserveCapacity(wtf_size_t new_capacity) {
    item_data.ReserveCapacity(new_capacity);
  }

  // Grid items are rearranged in order-modified document order since
  // auto-placement and painting rely on it later in the algorithm.
  GridItemDataVector item_data;
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::GridItemData)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_ITEM_H_
