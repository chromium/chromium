// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_STACKING_BASELINE_ACCUMULATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_STACKING_BASELINE_ACCUMULATOR_H_

#include "third_party/blink/renderer/core/layout/grid/grid_layout_utils.h"
#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lanes_running_positions.h"

namespace blink {

// Handle container baselines in the stacking axis similar to multicolumn
// layout.
class StackingBaselineAccumulator : public BaselineAccumulator {
  STACK_ALLOCATED();

 public:
  explicit StackingBaselineAccumulator(
      GridLanesRunningPositions& running_positions,
      GridTrackSizingDirection grid_axis_direction)
      : running_positions_(running_positions),
        grid_axis_direction_(grid_axis_direction),
        first_baseline_(std::nullopt),
        last_baseline_(std::nullopt) {}

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
      const auto& span = item.resolved_position.Span(grid_axis_direction_);

      for (wtf_size_t track_idx = span.StartLine(); track_idx < span.EndLine();
           ++track_idx) {
        const auto& track_data = running_positions_.GetTrackDataAt(track_idx);

        if (!track_data.first_item_stacking_position ||
            (item_moved_to_earlier_opening &&
             item_stacking_position <
                 *track_data.first_item_stacking_position)) {
          running_positions_.SetFirstBaseline(
              track_idx, item_stacking_position,
              block_offset + *first_baseline_value);
        }

        if (!track_data.last_item_stacking_position ||
            item_stacking_position >= *track_data.last_item_stacking_position) {
          running_positions_.SetLastBaseline(
              track_idx, item_stacking_position,
              block_offset + *last_baseline_value);
        }
      }
    }
  }

  std::optional<LayoutUnit> FirstBaseline() const override {
    if (!first_baseline_) {
      for (wtf_size_t i = 0; i < running_positions_.TrackCount(); ++i) {
        const auto& track_data = running_positions_.GetTrackDataAt(i);
        if (track_data.first_baseline) {
          first_baseline_.emplace(
              std::min(first_baseline_.value_or(*track_data.first_baseline),
                       *track_data.first_baseline));
        }
      }
    }
    return first_baseline_;
  }

  std::optional<LayoutUnit> LastBaseline() const override {
    // Take the last item with a usable baseline from each track and use
    // them to calculate the last baseline of the grid-lanes container.
    if (!last_baseline_) {
      for (wtf_size_t i = 0; i < running_positions_.TrackCount(); ++i) {
        const auto& track_data = running_positions_.GetTrackDataAt(i);
        if (track_data.last_baseline) {
          last_baseline_.emplace(
              std::max(last_baseline_.value_or(*track_data.last_baseline),
                       *track_data.last_baseline));
        }
      }
    }
    return last_baseline_;
  }

 private:
  GridLanesRunningPositions& running_positions_;
  GridTrackSizingDirection grid_axis_direction_;
  mutable std::optional<LayoutUnit> first_baseline_;
  mutable std::optional<LayoutUnit> last_baseline_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_STACKING_BASELINE_ACCUMULATOR_H_
