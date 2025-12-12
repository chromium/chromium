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

  void Accumulate(const GridItemData& item,
                  const LogicalBoxFragment& fragment,
                  const LayoutUnit block_offset,
                  LayoutUnit item_stacking_position) override {
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

        // Calculate first baseline for the stacking axis from the highest
        // baseline among the first items with usable baselines across all
        // tracks.

        // TODO(yanlingwang): Update negative margin handling if needed once we
        // resolve on https://github.com/w3c/csswg-drafts/issues/13165.
        if (!grid_set.first_item_stacking_position ||
            item_stacking_position < *grid_set.first_item_stacking_position) {
          grid_set.first_item_stacking_position = item_stacking_position;
          LayoutUnit item_first_baseline = block_offset + *first_baseline_value;
          // Store the smallest value that has been processed so far.
          first_baseline_.emplace(
              std::min(first_baseline_.value_or(item_first_baseline),
                       item_first_baseline));
        }

        // Update last baseline if this item has a later stacking position
        // than the current last item for this set.

        // TODO(yanlingwang): Update negative margin handling if needed once we
        // resolve on https://github.com/w3c/csswg-drafts/issues/13165.
        if (!grid_set.last_item_stacking_position ||
            item_stacking_position > *grid_set.last_item_stacking_position) {
          grid_set.last_item_stacking_position = item_stacking_position;
          grid_set.grid_lanes_last_baseline =
              block_offset + *last_baseline_value;
        }
      }
    }
  }

  std::optional<LayoutUnit> FirstBaseline() const override {
    return first_baseline_;
  }

  std::optional<LayoutUnit> LastBaseline() const override {
    // Calculate last baseline for the stacking axis from the lowest baseline
    // among the last items with usable baselines across all the tracks.
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
  std::optional<LayoutUnit> first_baseline_;
  mutable std::optional<LayoutUnit> last_baseline_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_STACKING_BASELINE_ACCUMULATOR_H_
