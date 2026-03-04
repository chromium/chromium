// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_STACKING_BASELINE_ACCUMULATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_STACKING_BASELINE_ACCUMULATOR_H_

#include "third_party/blink/renderer/core/layout/grid/grid_layout_utils.h"

namespace blink {

class GridSizingTrackCollection;

struct GridSet;

// Handle container baselines in the stacking axis similar to multicolumn
// layout.
class StackingBaselineAccumulator : public BaselineAccumulator {
  STACK_ALLOCATED();

 public:
  explicit StackingBaselineAccumulator(
      GridSizingTrackCollection* track_collection)
      : track_collection_(track_collection),
        first_baseline_(std::nullopt),
        last_baseline_(std::nullopt) {
    DCHECK(track_collection_);
  }

  // `item_moved_to_earlier_opening` indicates that dense packing placed this
  // item at a position higher than its original position.
  void Accumulate(const GridItemData& item,
                  const LogicalBoxFragment& fragment,
                  const LayoutUnit block_offset,
                  LayoutUnit item_stacking_position,
                  bool item_moved_to_earlier_opening) override {
    auto first_baseline_value = fragment.FirstBaseline();
    auto last_baseline_value = fragment.LastBaseline();

    if (first_baseline_value.has_value()) {
      DCHECK(last_baseline_value.has_value());
      const auto& [begin_set_index, end_set_index] =
          item.SetIndices(track_collection_->Direction());

      // Use the track collection's GridSets to store baseline information.
      // Update all sets that this item spans.
      for (wtf_size_t set_index = begin_set_index; set_index < end_set_index;
           ++set_index) {
        GridSet& grid_set = track_collection_->GetSetAt(set_index);

        // First-baseline candidate for this set:
        // - Normal placement: the first item seen for this set wins,
        //   regardless of running position.
        // - Dense packing: if an item is moved to an earlier opening and has
        //   a smaller running position, it becomes the new first candidate.
        if (!grid_set.first_item_stacking_position ||
            (item_moved_to_earlier_opening &&
             item_stacking_position < *grid_set.first_item_stacking_position)) {
          grid_set.first_item_stacking_position = item_stacking_position;
          grid_set.grid_lanes_first_baseline =
              block_offset + *first_baseline_value;
        }

        // Last-baseline candidate for this set: an item with an equal-or-
        // larger running position is treated as later in placement order,
        // regardless of normal placement or dense packing.
        if (!grid_set.last_item_stacking_position ||
            item_stacking_position >= *grid_set.last_item_stacking_position) {
          grid_set.last_item_stacking_position = item_stacking_position;
          grid_set.grid_lanes_last_baseline =
              block_offset + *last_baseline_value;
        }
      }
    }
  }

  std::optional<LayoutUnit> FirstBaseline() const override {
    // Take the first item with a usable baseline from each track and use
    // them to calculate the first baseline of the grid-lanes container.
    if (!first_baseline_) {
      for (wtf_size_t set_index = 0;
           set_index < track_collection_->GetSetCount(); ++set_index) {
        const GridSet& grid_set = track_collection_->GetSetAt(set_index);
        if (auto first_baseline_value = grid_set.grid_lanes_first_baseline) {
          // Store the smallest value that has been processed so far.
          first_baseline_.emplace(
              std::min(first_baseline_.value_or(*first_baseline_value),
                       *first_baseline_value));
        }
      }
    }
    return first_baseline_;
  }

  std::optional<LayoutUnit> LastBaseline() const override {
    // Take the last item with a usable baseline from each track and use
    // them to calculate the last baseline of the grid-lanes container.
    if (!last_baseline_) {
      for (wtf_size_t set_index = 0;
           set_index < track_collection_->GetSetCount(); ++set_index) {
        const GridSet& grid_set = track_collection_->GetSetAt(set_index);
        if (auto last_baseline_value = grid_set.grid_lanes_last_baseline) {
          // Store the largest value that has been processed so far.
          last_baseline_.emplace(
              std::max(last_baseline_.value_or(*last_baseline_value),
                       *last_baseline_value));
        }
      }
    }
    return last_baseline_;
  }

 private:
  GridSizingTrackCollection* track_collection_;
  mutable std::optional<LayoutUnit> first_baseline_;
  mutable std::optional<LayoutUnit> last_baseline_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_STACKING_BASELINE_ACCUMULATOR_H_
