// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_ITEM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_ITEM_H_

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_track_collection.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/style/grid_positions_resolver.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class NGGridPlacement;

enum class AxisEdge { kStart, kCenter, kEnd, kBaseline };
enum class BaselineType { kMajor, kMinor };
enum class SizingConstraint { kLayout, kMinContent, kMaxContent };

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

  explicit GridItemData(const NGBlockNode node)
      : node(node), is_sizing_dependent_on_block_size(false) {}

  void SetAlignmentFallback(const GridTrackSizingDirection track_direction,
                            const ComputedStyle& container_style,
                            const bool has_synthesized_baseline);

  AxisEdge InlineAxisAlignment() const {
    return inline_axis_alignment_fallback.value_or(inline_axis_alignment);
  }
  AxisEdge BlockAxisAlignment() const {
    return block_axis_alignment_fallback.value_or(block_axis_alignment);
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
  void ComputeSetIndices(
      const NGGridLayoutAlgorithmTrackCollection& track_collection);

  // For this out of flow item and track collection, computes and stores its
  // first and last spanned ranges, as well as the start and end track offset.
  // |grid_placement| is used to resolve the grid lines.
  void ComputeOutOfFlowItemPlacement(
      const NGGridLayoutAlgorithmTrackCollection& track_collection,
      const NGGridPlacement& grid_placement);

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

  NGBlockNode node;
  GridArea resolved_position;

  bool is_block_axis_overflow_safe : 1;
  bool is_inline_axis_overflow_safe : 1;
  bool is_sizing_dependent_on_block_size : 1;

  AxisEdge inline_axis_alignment;
  AxisEdge block_axis_alignment;

  absl::optional<AxisEdge> inline_axis_alignment_fallback;
  absl::optional<AxisEdge> block_axis_alignment_fallback;

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

using GridItemStorageVector = Vector<GridItemData, 4>;

struct CORE_EXPORT GridItems {
  DISALLOW_NEW();

 public:
  class Iterator : public std::iterator<std::input_iterator_tag, GridItemData> {
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

    GridItemData& operator*() const {
      DCHECK(current_index_ && *current_index_ < item_data_->size());
      return item_data_->at(*current_index_);
    }

    GridItemData* operator->() const { return &operator*(); }

   private:
    GridItemStorageVector* item_data_;
    Vector<wtf_size_t>::const_iterator current_index_;
  };

  Iterator begin() {
    return Iterator(&item_data, reordered_item_indices.begin());
  }
  Iterator end() { return Iterator(&item_data, reordered_item_indices.end()); }

  void Append(const GridItemData& new_item_data) {
    reordered_item_indices.push_back(item_data.size());
    item_data.emplace_back(new_item_data);
  }
  void ReserveCapacity(const wtf_size_t capacity) {
    reordered_item_indices.ReserveCapacity(capacity);
    item_data.ReserveCapacity(capacity);
  }

  wtf_size_t Size() const { return item_data.size(); }
  bool IsEmpty() const { return item_data.IsEmpty(); }

  // Grid items are appended in document order, but we want to rearrange them in
  // order-modified document order since auto-placement and painting rely on it
  // later in the algorithm.
  GridItemStorageVector item_data;
  Vector<wtf_size_t> reordered_item_indices;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_ITEM_H_
