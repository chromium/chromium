// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid/grid_layout_utils.h"

#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/geometry/static_position.h"
#include "third_party/blink/renderer/core/layout/grid/grid_item.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"
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

wtf_size_t CalculateAutomaticRepetitions(
    const GridTrackList& track_list,
    const LayoutUnit gutter_size,
    LayoutUnit available_size,
    LayoutUnit min_available_size,
    LayoutUnit max_available_size,
    const Vector<LayoutUnit>* intrinsic_repeat_track_sizes) {
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
    const GridTrackRepeater::RepeatType repeat_type =
        track_list.RepeatType(repeater_index);
    const bool is_auto_repeater = repeat_type == GridTrackRepeater::kAutoFill ||
                                  repeat_type == GridTrackRepeater::kAutoFit;

    LayoutUnit repeater_size;
    const wtf_size_t repeater_track_count =
        track_list.RepeatSize(repeater_index);

    for (wtf_size_t i = 0; i < repeater_track_count; ++i) {
      const GridTrackSize& track_size =
          track_list.RepeatTrackSize(repeater_index, i);

      const bool is_track_size_intrinsic =
          track_size.IsTrackDefinitionIntrinsic();
      if (is_track_size_intrinsic && !intrinsic_repeat_track_sizes) {
        return 0;
      }

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
      if (is_track_size_intrinsic) {
        CHECK_EQ(intrinsic_repeat_track_sizes->size(), repeater_track_count);
        track_contribution = (*intrinsic_repeat_track_sizes)[i];
      } else if (fixed_max_track_breadth && fixed_min_track_breadth) {
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

namespace {

// TODO(yanlingwang): Update this method to use direct individual track size
// queries once GridLayoutTrackCollection supports storing and querying
// individual track sizes, rather than computing them from set offsets.
Vector<std::div_t> ComputeTrackSizesInRange(
    const GridLayoutTrackCollection& track_collection,
    const wtf_size_t range_begin_set_index,
    const wtf_size_t range_set_count) {
  Vector<std::div_t> track_sizes;
  track_sizes.ReserveInitialCapacity(range_set_count);

  const wtf_size_t ending_set_index = range_begin_set_index + range_set_count;
  for (wtf_size_t i = range_begin_set_index; i < ending_set_index; ++i) {
    // Set information is stored as offsets. To determine the size of a single
    // track in a given set, first determine the total size the set takes up
    // by finding the difference between the offsets and subtracting the gutter
    // size for each track in the set.
    LayoutUnit set_size =
        track_collection.GetSetOffset(i + 1) - track_collection.GetSetOffset(i);
    const wtf_size_t set_track_count = track_collection.GetSetTrackCount(i);

    DCHECK_GE(set_size, 0);
    set_size = (set_size - track_collection.GutterSize() * set_track_count)
                   .ClampNegativeToZero();

    // Once we have determined the size of the set, we can find the size of a
    // given track by dividing the `set_size` by the `set_track_count`.
    DCHECK_GT(set_track_count, 0u);
    track_sizes.emplace_back(std::div(set_size.RawValue(), set_track_count));
  }
  return track_sizes;
}

// For out of flow items that are located in the middle of a range, computes
// the extra offset relative to the start of its containing range.
LayoutUnit ComputeTrackOffsetInRange(
    const GridLayoutTrackCollection& track_collection,
    const wtf_size_t range_begin_set_index,
    const wtf_size_t range_set_count,
    const wtf_size_t offset_in_range) {
  if (!range_set_count || !offset_in_range) {
    return LayoutUnit();
  }

  // To compute the index offset, we have to determine the size of the
  // tracks within the grid item's span.
  Vector<std::div_t> track_sizes = ComputeTrackSizesInRange(
      track_collection, range_begin_set_index, range_set_count);

  // Calculate how many sets there are from the start of the range to the
  // `offset_in_range`. This division can produce a remainder, which would
  // mean that not all of the sets are repeated the same amount of times from
  // the start to the `offset_in_range`.
  const wtf_size_t floor_set_track_count = offset_in_range / range_set_count;
  const wtf_size_t remaining_track_count = offset_in_range % range_set_count;

  // Iterate over the sets and add the sizes of the tracks to `index_offset`.
  LayoutUnit index_offset = track_collection.GutterSize() * offset_in_range;
  for (wtf_size_t i = 0; i < track_sizes.size(); ++i) {
    // If we have a remainder from the `floor_set_track_count`, we have to
    // consider it to get the correct offset.
    const wtf_size_t set_count =
        floor_set_track_count + ((remaining_track_count > i) ? 1 : 0);
    index_offset +=
        LayoutUnit::FromRawValue(std::min<int>(set_count, track_sizes[i].rem) +
                                 (set_count * track_sizes[i].quot));
  }
  return index_offset;
}

template <bool snap_to_end_of_track>
LayoutUnit TrackOffset(const GridLayoutTrackCollection& track_collection,
                       const wtf_size_t range_index,
                       const wtf_size_t offset_in_range) {
  const wtf_size_t range_begin_set_index =
      track_collection.RangeBeginSetIndex(range_index);
  const wtf_size_t range_track_count =
      track_collection.RangeTrackCount(range_index);
  const wtf_size_t range_set_count =
      track_collection.RangeSetCount(range_index);

  LayoutUnit track_offset;
  if (offset_in_range == range_track_count) {
    DCHECK(snap_to_end_of_track);
    track_offset =
        track_collection.GetSetOffset(range_begin_set_index + range_set_count);
  } else {
    DCHECK(offset_in_range || !snap_to_end_of_track);
    DCHECK_LT(offset_in_range, range_track_count);

    // If an out of flow item starts/ends in the middle of a range, compute and
    // add the extra offset to the start offset of the range.
    track_offset =
        track_collection.GetSetOffset(range_begin_set_index) +
        ComputeTrackOffsetInRange(track_collection, range_begin_set_index,
                                  range_set_count, offset_in_range);
  }

  // `track_offset` includes the gutter size at the end of the last track,
  // when we snap to the end of last track such gutter size should be removed.
  // However, only snap if this range is not collapsed or if it can snap to the
  // end of the last track in the previous range of the collection.
  if (snap_to_end_of_track && (range_set_count || range_index)) {
    track_offset -= track_collection.GutterSize();
  }
  return track_offset;
}

LayoutUnit TrackStartOffset(const GridLayoutTrackCollection& track_collection,
                            const wtf_size_t range_index,
                            const wtf_size_t offset_in_range) {
  if (!track_collection.RangeCount()) {
    // If the start line of an out of flow item is not 'auto' in an empty and
    // undefined grid, start offset is the start border scrollbar padding.
    DCHECK_EQ(range_index, 0u);
    DCHECK_EQ(offset_in_range, 0u);
    return track_collection.GetSetOffset(0);
  }

  const wtf_size_t range_track_count =
      track_collection.RangeTrackCount(range_index);

  if (offset_in_range == range_track_count &&
      range_index == track_collection.RangeCount() - 1) {
    // The only case where we allow the offset to be equal to the number of
    // tracks in the range is for the last range in the collection, which should
    // match the end line of the implicit grid; snap to the track end instead.
    return TrackOffset</* snap_to_end_of_track */ true>(
        track_collection, range_index, offset_in_range);
  }

  DCHECK_LT(offset_in_range, range_track_count);
  return TrackOffset</* snap_to_end_of_track */ false>(
      track_collection, range_index, offset_in_range);
}

LayoutUnit TrackEndOffset(const GridLayoutTrackCollection& track_collection,
                          const wtf_size_t range_index,
                          const wtf_size_t offset_in_range) {
  if (!track_collection.RangeCount()) {
    // If the end line of an out of flow item is not 'auto' in an empty and
    // undefined grid, end offset is the start border scrollbar padding.
    DCHECK_EQ(range_index, 0u);
    DCHECK_EQ(offset_in_range, 0u);
    return track_collection.GetSetOffset(0);
  }

  if (!offset_in_range && !range_index) {
    // Only allow the offset to be 0 for the first range in the collection,
    // which is the start line of the implicit grid; don't snap to the end.
    return TrackOffset</* snap_to_end_of_track */ false>(
        track_collection, range_index, offset_in_range);
  }

  DCHECK_GT(offset_in_range, 0u);
  return TrackOffset</* snap_to_end_of_track */ true>(
      track_collection, range_index, offset_in_range);
}

}  // namespace

void ComputeOutOfFlowOffsetAndSize(
    const GridItemData& out_of_flow_item,
    const GridLayoutTrackCollection& track_collection,
    const BoxStrut& borders,
    const LogicalSize& border_box_size,
    LayoutUnit* start_offset,
    LayoutUnit* size,
    bool is_grid_lanes_axis) {
  DCHECK(start_offset && size && out_of_flow_item.IsOutOfFlow());
  OutOfFlowItemPlacement item_placement;
  LayoutUnit end_offset;

  // For the normal grid axis, determine axis from track collection direction.
  // For the grid-lanes stacking axis, invert the direction to get the stacking
  // axis.
  const bool is_for_columns = is_grid_lanes_axis
                                  ? track_collection.Direction() == kForRows
                                  : track_collection.Direction() == kForColumns;

  // The default padding box value for `size` is used for out of flow items in
  // which both the start line and end line are defined as 'auto'.
  if (is_for_columns) {
    item_placement = out_of_flow_item.column_placement;
    *start_offset = borders.inline_start;
    end_offset = border_box_size.inline_size - borders.inline_end;
  } else {
    item_placement = out_of_flow_item.row_placement;
    *start_offset = borders.block_start;
    end_offset = border_box_size.block_size - borders.block_end;
  }

  // For the grid-lanes stacking axis, ignore grid placement and use border
  // edges. If the start line is defined, the size will be calculated by
  // subtracting the offset at `start_index`; otherwise, use the computed border
  // start.
  if (!is_grid_lanes_axis && item_placement.range_index.begin != kNotFound) {
    DCHECK_NE(item_placement.offset_in_range.begin, kNotFound);

    *start_offset =
        TrackStartOffset(track_collection, item_placement.range_index.begin,
                         item_placement.offset_in_range.begin);
  }

  // If the end line is defined, the offset (which can be the offset at the
  // start index or the start border) and the added grid gap after the spanned
  // tracks are subtracted from the offset at the end index.
  if (!is_grid_lanes_axis && item_placement.range_index.end != kNotFound) {
    DCHECK_NE(item_placement.offset_in_range.end, kNotFound);

    end_offset =
        TrackEndOffset(track_collection, item_placement.range_index.end,
                       item_placement.offset_in_range.end);
  }

  // `start_offset` can be greater than `end_offset` if the used track sizes or
  // gutter size saturated the set offsets of the track collection.
  *size = (end_offset - *start_offset).ClampNegativeToZero();
}

void AlignmentOffsetForOutOfFlow(AxisEdge inline_axis_edge,
                                 AxisEdge block_axis_edge,
                                 LogicalSize container_size,
                                 LogicalStaticPosition* static_pos) {
  using InlineEdge = LogicalStaticPosition::InlineEdge;
  using BlockEdge = LogicalStaticPosition::BlockEdge;

  switch (inline_axis_edge) {
    case AxisEdge::kStart:
    case AxisEdge::kFirstBaseline:
      static_pos->inline_edge = InlineEdge::kInlineStart;
      break;
    case AxisEdge::kCenter:
      static_pos->inline_edge = InlineEdge::kInlineCenter;
      static_pos->offset.inline_offset += container_size.inline_size / 2;
      break;
    case AxisEdge::kEnd:
    case AxisEdge::kLastBaseline:
      static_pos->inline_edge = InlineEdge::kInlineEnd;
      static_pos->offset.inline_offset += container_size.inline_size;
      break;
  }

  switch (block_axis_edge) {
    case AxisEdge::kStart:
    case AxisEdge::kFirstBaseline:
      static_pos->block_edge = BlockEdge::kBlockStart;
      break;
    case AxisEdge::kCenter:
      static_pos->block_edge = BlockEdge::kBlockCenter;
      static_pos->offset.block_offset += container_size.block_size / 2;
      break;
    case AxisEdge::kEnd:
    case AxisEdge::kLastBaseline:
      static_pos->block_edge = BlockEdge::kBlockEnd;
      static_pos->offset.block_offset += container_size.block_size;
      break;
  }
}

LayoutUnit CalculateIntrinsicMinimumContribution(
    bool is_parallel_with_track_direction,
    bool special_spanning_criteria,
    const LayoutUnit min_content_contribution,
    const LayoutUnit max_content_contribution,
    const ConstraintSpace& space,
    const MinMaxSizesResult& subgrid_minmax_sizes,
    const GridItemData* grid_item,
    bool& maybe_clamp) {
  CHECK(grid_item);
  const auto& node = grid_item->node;
  const ComputedStyle& item_style = node.Style();
  maybe_clamp = false;

  // TODO(ikilpatrick): All of the below is incorrect for replaced elements.
  const auto& main_length = is_parallel_with_track_direction
                                ? item_style.LogicalWidth()
                                : item_style.LogicalHeight();
  const auto& min_length = is_parallel_with_track_direction
                               ? item_style.LogicalMinWidth()
                               : item_style.LogicalMinHeight();

  // We could be clever and make this an if-stmt, but each type has
  // subtle consequences. This forces us in the future when we add a new
  // length type to consider what the best thing is for grid.
  switch (main_length.GetType()) {
    case Length::kAuto:
    case Length::kFitContent:
    case Length::kStretch:
    case Length::kPercent:
    case Length::kCalculated: {
      const auto border_padding =
          ComputeBorders(space, node) + ComputePadding(space, item_style);

      // All of the above lengths are considered 'auto' if we are querying a
      // minimum contribution. They all require definite track sizes to
      // determine their final size.
      //
      // From https://drafts.csswg.org/css-grid/#min-size-auto:
      //   To provide a more reasonable default minimum size for grid items,
      //   the used value of its automatic minimum size in a given axis is
      //   the content-based minimum size if all of the following are true:
      //     - it is not a scroll container
      //     - it spans at least one track in that axis whose min track
      //     sizing function is 'auto'
      //     - if it spans more than one track in that axis, none of those
      //     tracks are flexible
      //   Otherwise, the automatic minimum size is zero, as usual.
      //
      // Start by resolving the cases where |min_length| is non-auto or its
      // automatic minimum size should be zero.
      if (!min_length.HasAuto() || item_style.IsScrollContainer() ||
          special_spanning_criteria) {
        // TODO(ikilpatrick): This block needs to respect the aspect-ratio,
        // and apply the transferred min/max sizes when appropriate. We do
        // this sometimes elsewhere so should unify and simplify this code.
        if (is_parallel_with_track_direction) {
          auto MinMaxSizesFunc = [&](SizeType type) -> MinMaxSizesResult {
            if (grid_item->IsSubgrid()) {
              return subgrid_minmax_sizes;
            }
            return node.ComputeMinMaxSizes(item_style.GetWritingMode(), type,
                                           space);
          };
          return ResolveMinInlineLength(space, item_style, border_padding,
                                        MinMaxSizesFunc, min_length);
        } else {
          return ResolveInitialMinBlockLength(space, item_style, border_padding,
                                              min_length);
        }
      }

      maybe_clamp = true;
      return min_content_contribution;
    }
    case Length::kMinContent:
    case Length::kMaxContent:
    case Length::kFixed: {
      // All of the above lengths are "definite" (non-auto), and don't need
      // the special min-size treatment above. (They will all end up being
      // the specified size).
      return main_length.IsMaxContent() ? max_content_contribution
                                        : min_content_contribution;
    }
    case Length::kMinIntrinsic:
    case Length::kFlex:
    case Length::kExtendToZoom:
    case Length::kDeviceWidth:
    case Length::kDeviceHeight:
    case Length::kNone:
    case Length::kContent:
      NOTREACHED();
  }
}

LayoutUnit ClampIntrinsicMinSize(LayoutUnit min_content_contribution,
                                 LayoutUnit min_clamp_size,
                                 LayoutUnit spanned_tracks_definite_max_size) {
  CHECK_NE(spanned_tracks_definite_max_size, kIndefiniteSize);
  DCHECK_GE(min_content_contribution, min_clamp_size);

  // Don't clamp beyond `min_clamp_size`, which usually represents
  // the sum of border/padding, margins, and the baseline shim for
  // the associated item.
  spanned_tracks_definite_max_size =
      std::max(spanned_tracks_definite_max_size, min_clamp_size);

  return std::min(min_content_contribution, spanned_tracks_definite_max_size);
}

}  // namespace blink
