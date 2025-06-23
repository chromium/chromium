// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid/grid_layout_utils.h"

#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/style/grid_track_list.h"

namespace blink {

void ComputeAvailableSizes(const BoxStrut& border_scrollbar_padding,
                           const BlockNode& node,
                           const ConstraintSpace& constraint_space,
                           const BoxFragmentBuilder& container_builder,
                           LogicalSize& available_size,
                           LogicalSize& min_available_size,
                           LogicalSize& max_available_size) {
  // If our inline-size is indefinite, compute the min/max inline-sizes.
  if (available_size.inline_size == kIndefiniteSize) {
    const LayoutUnit border_scrollbar_padding_sum =
        border_scrollbar_padding.InlineSum();

    const MinMaxSizes sizes = ComputeMinMaxInlineSizes(
        constraint_space, node, container_builder.BorderPadding(),
        /*auto_min_length=*/nullptr, [](SizeType) -> MinMaxSizesResult {
          // If we've reached here we are inside the `ComputeMinMaxSizes` pass,
          // and also have something like "min-width: min-content". This is
          // cyclic. Just return indefinite.
          return {{kIndefiniteSize, kIndefiniteSize},
                  /* depends_on_block_constraints */ false};
        });

    min_available_size.inline_size =
        (sizes.min_size - border_scrollbar_padding_sum).ClampNegativeToZero();
    max_available_size.inline_size =
        (sizes.max_size == LayoutUnit::Max())
            ? sizes.max_size
            : (sizes.max_size - border_scrollbar_padding_sum)
                  .ClampNegativeToZero();
  }

  // And similar for the min/max block-sizes.
  if (available_size.block_size == kIndefiniteSize) {
    const LayoutUnit border_scrollbar_padding_sum =
        border_scrollbar_padding.BlockSum();
    const MinMaxSizes sizes = ComputeInitialMinMaxBlockSizes(
        constraint_space, node, container_builder.BorderPadding());

    min_available_size.block_size =
        (sizes.min_size - border_scrollbar_padding_sum).ClampNegativeToZero();
    max_available_size.block_size =
        (sizes.max_size == LayoutUnit::Max())
            ? sizes.max_size
            : (sizes.max_size - border_scrollbar_padding_sum)
                  .ClampNegativeToZero();
  }
}

wtf_size_t CalculateAutomaticRepetitions(const NGGridTrackList& track_list,
                                         const LayoutUnit gutter_size,
                                         LayoutUnit available_size,
                                         LayoutUnit min_available_size,
                                         LayoutUnit max_available_size) {
  DCHECK(track_list.HasAutoRepeater());

  if (available_size == kIndefiniteSize) {
    available_size = min_available_size;
  } else {
    max_available_size = available_size;
  }

  LayoutUnit auto_repeater_size;
  LayoutUnit non_auto_specified_size;
  for (wtf_size_t repeater_index = 0;
       repeater_index < track_list.RepeaterCount(); ++repeater_index) {
    const NGGridTrackRepeater::RepeatType repeat_type =
        track_list.RepeatType(repeater_index);
    const bool is_auto_repeater =
        repeat_type == NGGridTrackRepeater::kAutoFill ||
        repeat_type == NGGridTrackRepeater::kAutoFit;

    LayoutUnit repeater_size;
    const wtf_size_t repeater_track_count =
        track_list.RepeatSize(repeater_index);

    for (wtf_size_t i = 0; i < repeater_track_count; ++i) {
      const GridTrackSize& track_size =
          track_list.RepeatTrackSize(repeater_index, i);

      std::optional<LayoutUnit> fixed_min_track_breadth;
      if (track_size.HasFixedMinTrackBreadth()) {
        fixed_min_track_breadth.emplace(MinimumValueForLength(
            track_size.MinTrackBreadth(), available_size));
      }

      std::optional<LayoutUnit> fixed_max_track_breadth;
      if (track_size.HasFixedMaxTrackBreadth()) {
        fixed_max_track_breadth.emplace(MinimumValueForLength(
            track_size.MaxTrackBreadth(), available_size));
      }

      LayoutUnit track_contribution;
      if (fixed_max_track_breadth && fixed_min_track_breadth) {
        track_contribution =
            std::max(*fixed_max_track_breadth, *fixed_min_track_breadth);
      } else if (fixed_max_track_breadth) {
        track_contribution = *fixed_max_track_breadth;
      } else if (fixed_min_track_breadth) {
        track_contribution = *fixed_min_track_breadth;
      }

      // For the purpose of finding the number of auto-repeated tracks in a
      // standalone axis, the UA must floor the track size to a UA-specified
      // value to avoid division by zero. It is suggested that this floor be
      // 1px.
      if (is_auto_repeater) {
        track_contribution = std::max(LayoutUnit(1), track_contribution);
      }

      repeater_size += track_contribution + gutter_size;
    }

    if (!is_auto_repeater) {
      non_auto_specified_size +=
          repeater_size * track_list.RepeatCount(repeater_index, 0);
    } else {
      DCHECK_EQ(0, auto_repeater_size);
      auto_repeater_size = repeater_size;
    }
  }

  DCHECK_GT(auto_repeater_size, 0);

  // We can compute the number of repetitions by satisfying the expression
  // below. Notice that we subtract an extra |gutter_size| since it was included
  // in the contribution for the last set in the collection.
  //   available_size =
  //       (repetitions * auto_repeater_size) +
  //       non_auto_specified_size - gutter_size
  //
  // Solving for repetitions we have:
  //   repetitions =
  //       available_size - (non_auto_specified_size - gutter_size) /
  //       auto_repeater_size
  non_auto_specified_size -= gutter_size;

  // First we want to allow as many repetitions as possible, up to the max
  // available-size. Only do this if we have a definite max-size.
  // If a definite available-size was provided, |max_available_size| will be
  // set to that value.
  if (max_available_size != LayoutUnit::Max()) {
    // Use floor to ensure that the auto repeater sizes goes under the max
    // available-size.
    const int count = FloorToInt(
        (max_available_size - non_auto_specified_size) / auto_repeater_size);
    return (count <= 0) ? 1u : count;
  }

  // Next, consider the min available-size, which was already used to floor
  // |available_size|. Use ceil to ensure that the auto repeater size goes
  // above this min available-size.
  const int count = CeilToInt((available_size - non_auto_specified_size) /
                              auto_repeater_size);
  return (count <= 0) ? 1u : count;
}

}  // namespace blink
