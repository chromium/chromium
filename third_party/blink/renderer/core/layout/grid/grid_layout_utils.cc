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
#include "third_party/blink/renderer/core/layout/grid/grid_line_resolver.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_sizing_algorithm.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/style/grid_track_list.h"

namespace blink {

LayoutUnit GetTrackBaseline(const GridItemData& grid_item,
                            const GridLayoutTrackCollection& track_collection) {
  const auto track_direction = track_collection.Direction();

  // "If a box spans multiple shared alignment contexts, then it participates
  //  in first/last baseline alignment within its start-most/end-most shared
  //  alignment context along that axis"
  // https://www.w3.org/TR/css-align-3/#baseline-sharing-group
  const auto& [begin_set_index, end_set_index] =
      grid_item.SetIndices(track_direction);

  return (grid_item.BaselineGroup(track_direction) == BaselineGroup::kMajor)
             ? track_collection.MajorBaseline(begin_set_index)
             : track_collection.MinorBaseline(end_set_index - 1);
}

LayoutUnit GetLogicalBaseline(const LogicalBoxFragment& baseline_fragment,
                              FontBaseline font_baseline,
                              bool is_last_baseline) {
  return is_last_baseline
             ? baseline_fragment.BlockSize() -
                   baseline_fragment.LastBaselineOrSynthesize(font_baseline)
             : baseline_fragment.FirstBaselineOrSynthesize(font_baseline);
}

void StoreItemBaseline(const LogicalBoxFragment& baseline_fragment,
                       GridTrackSizingDirection track_direction,
                       FontBaseline font_baseline,
                       LayoutUnit extra_margin,
                       GridSizingTrackCollection& track_collection,
                       GridItemData& item) {
  const bool has_synthesized_baseline =
      !baseline_fragment.FirstBaseline().has_value();
  item.SetAlignmentFallback(track_direction, has_synthesized_baseline);

  const LayoutUnit item_baseline =
      GetLogicalBaseline(baseline_fragment, font_baseline,
                         item.IsLastBaselineSpecified(track_direction));
  const LayoutUnit total_baseline = extra_margin + item_baseline;

  // "If a box spans multiple shared alignment contexts, then it participates
  //  in first/last baseline alignment within its start-most/end-most shared
  //  alignment context along that axis"
  // https://www.w3.org/TR/css-align-3/#baseline-sharing-group
  const auto& [begin_set_index, end_set_index] =
      item.SetIndices(track_direction);

  if (item.BaselineGroup(track_direction) == BaselineGroup::kMajor) {
    track_collection.SetMajorBaseline(begin_set_index, total_baseline);
  } else {
    track_collection.SetMinorBaseline(end_set_index - 1, total_baseline);
  }
}

LayoutUnit ComputeBaselineOffset(
    const GridItemData& grid_item,
    const GridLayoutTrackCollection& track_collection,
    const LogicalBoxFragment& baseline_fragment,
    const LogicalBoxFragment& fragment,
    FontBaseline font_baseline,
    GridTrackSizingDirection track_direction,
    LayoutUnit available_size) {
  if (!grid_item.IsBaselineAligned(track_direction)) {
    return LayoutUnit();
  }

  // The baseline offset is the difference between the grid item's baseline
  // and its track baseline.
  const LayoutUnit baseline_delta =
      GetTrackBaseline(grid_item, track_collection) -
      GetLogicalBaseline(baseline_fragment, font_baseline,
                         grid_item.IsLastBaselineSpecified(track_direction));

  if (grid_item.BaselineGroup(track_direction) == BaselineGroup::kMajor) {
    return baseline_delta;
  }

  // BaselineGroup::kMinor
  const LayoutUnit item_size = (track_direction == kForColumns)
                                   ? fragment.InlineSize()
                                   : fragment.BlockSize();
  return available_size - baseline_delta - item_size;
}

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
    const HashMap<GridTrackSize, LayoutUnit>* intrinsic_repeat_track_sizes) {
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
        auto it = intrinsic_repeat_track_sizes->find(track_size);
        CHECK_NE(it, intrinsic_repeat_track_sizes->end());
        track_contribution = it->value;
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

namespace {

GridArea SubgriddedAreaInParent(const SubgriddedItemData& opt_subgrid_data) {
  if (!opt_subgrid_data.IsSubgrid()) {
    return GridArea();
  }

  auto subgridded_area_in_parent = opt_subgrid_data->resolved_position;

  if (!opt_subgrid_data->has_subgridded_columns) {
    subgridded_area_in_parent.columns = GridSpan::IndefiniteGridSpan();
  }
  if (!opt_subgrid_data->has_subgridded_rows) {
    subgridded_area_in_parent.rows = GridSpan::IndefiniteGridSpan();
  }

  if (!opt_subgrid_data->is_parallel_with_root_grid) {
    std::swap(subgridded_area_in_parent.columns,
              subgridded_area_in_parent.rows);
  }
  return subgridded_area_in_parent;
}

}  // namespace

template <typename LayoutAlgorithmType>
void BuildGridSizingSubtree(const LayoutAlgorithmType& algorithm,
                            GridSizingTree* sizing_tree,
                            HeapVector<Member<LayoutBox>>* opt_oof_children,
                            const SubgriddedItemData& opt_subgrid_data,
                            const GridLineResolver* opt_parent_line_resolver,
                            bool must_invalidate_placement_cache,
                            bool must_ignore_children) {
  DCHECK(sizing_tree);

  const auto& node = algorithm.Node();
  const auto& style = node.Style();

  sizing_tree->AddToPreorderTraversal(node);

  const auto subgrid_area = SubgriddedAreaInParent(opt_subgrid_data);
  const auto column_auto_repetitions =
      algorithm.ComputeAutomaticRepetitions(subgrid_area.columns, kForColumns);
  const auto row_auto_repetitions =
      algorithm.ComputeAutomaticRepetitions(subgrid_area.rows, kForRows);
  const auto constraint_space = algorithm.GetConstraintSpace();
  const auto writing_mode = constraint_space.GetWritingMode();

  // Initialize this grid's line resolver.
  const auto line_resolver =
      opt_parent_line_resolver
          ? GridLineResolver(style, *opt_parent_line_resolver, subgrid_area,
                             column_auto_repetitions, row_auto_repetitions)
          : GridLineResolver(style, column_auto_repetitions,
                             row_auto_repetitions);

  GridItems grid_items;
  GridLayoutData layout_data;
  bool has_nested_subgrid = false;
  wtf_size_t column_start_offset = 0;
  wtf_size_t row_start_offset = 0;

  if (!must_ignore_children) {
    // Construct grid items that are not subgridded.
    grid_items =
        node.ConstructGridItems(line_resolver, &must_invalidate_placement_cache,
                                opt_oof_children, &has_nested_subgrid);

    const auto& placement_data = node.CachedPlacementData();
    column_start_offset = placement_data.column_start_offset;
    row_start_offset = placement_data.row_start_offset;
  }

  auto BuildSizingCollection = [&](GridTrackSizingDirection track_direction) {
    GridRangeBuilder range_builder(
        style, track_direction, line_resolver.AutoRepetitions(track_direction),
        (track_direction == kForColumns) ? column_start_offset
                                         : row_start_offset);

    bool must_create_baselines = false;
    for (auto& grid_item : grid_items.IncludeSubgriddedItems()) {
      if (grid_item.IsConsideredForSizing(track_direction)) {
        must_create_baselines |= grid_item.IsBaselineSpecified(track_direction);
      }

      if (grid_item.MustCachePlacementIndices(track_direction)) {
        auto& range_indices = grid_item.RangeIndices(track_direction);
        range_builder.EnsureTrackCoverage(grid_item.StartLine(track_direction),
                                          grid_item.SpanSize(track_direction),
                                          &range_indices.begin,
                                          &range_indices.end);
      }
    }

    layout_data.SetTrackCollection(std::make_unique<GridSizingTrackCollection>(
        range_builder.FinalizeRanges(), track_direction,
        must_create_baselines));
  };

  const bool has_standalone_columns = subgrid_area.columns.IsIndefinite();
  const bool has_standalone_rows = subgrid_area.rows.IsIndefinite();

  if (has_standalone_columns) {
    BuildSizingCollection(kForColumns);
  }
  if (has_standalone_rows) {
    BuildSizingCollection(kForRows);
  }

  if (!has_nested_subgrid) {
    sizing_tree->SetSizingNodeData(node, std::move(grid_items),
                                   std::move(layout_data));
    return;
  }

  InitializeTrackCollection(opt_subgrid_data, style, constraint_space,
                            algorithm.BorderScrollbarPadding(),
                            algorithm.GetGridAvailableSize(), kForColumns,
                            &layout_data);
  InitializeTrackCollection(opt_subgrid_data, style, constraint_space,
                            algorithm.BorderScrollbarPadding(),
                            algorithm.GetGridAvailableSize(), kForRows,
                            &layout_data);

  if (has_standalone_columns) {
    layout_data.SizingCollection(kForColumns).CacheDefiniteSetsGeometry();
  }
  if (has_standalone_rows) {
    layout_data.SizingCollection(kForRows).CacheDefiniteSetsGeometry();
  }

  // `AppendSubgriddedItems` rely on the cached placement data of a subgrid to
  // construct its grid items, so we need to build their subtrees beforehand.
  for (auto& grid_item : grid_items) {
    if (!grid_item.IsSubgrid()) {
      continue;
    }

    // TODO(ethavar): Currently we have an issue where we can't correctly cache
    // the set indices of this grid item to determine its available space. This
    // happens because subgridded items are not considered by the range builder
    // since they can't be placed before we recurse into subgrids.
    grid_item.ComputeSetIndices(layout_data.Columns());
    grid_item.ComputeSetIndices(layout_data.Rows());

    const auto space =
        algorithm.CreateConstraintSpaceForLayout(grid_item, layout_data);
    const auto fragment_geometry =
        CalculateInitialFragmentGeometryForSubgrid(grid_item, space);

    // TODO(almaher): Use the grid lanes algorithm if the subgrid requires it.
    const GridLayoutAlgorithm subgrid_algorithm(
        {grid_item.node, fragment_geometry, space});

    // TODO(almaher): Use the grid lanes algorithm if the subgrid requires it.
    BuildGridSizingSubtree<GridLayoutAlgorithm>(
        subgrid_algorithm, sizing_tree, /*opt_oof_children=*/nullptr,
        SubgriddedItemData(grid_item, layout_data, writing_mode),
        &line_resolver, must_invalidate_placement_cache);

    // After we accommodate subgridded items in their respective sizing track
    // collections, their placement indices might be incorrect, so we want to
    // recompute them when we call `InitializeTrackSizes`.
    grid_item.ResetPlacementIndices();
  }

  node.AppendSubgriddedItems(&grid_items);

  // We need to recreate the track builder collections to ensure track coverage
  // for subgridded items; it would be ideal to have them accounted for already,
  // but we might need the track collections to compute a subgrid's automatic
  // repetitions, so we do this process twice to avoid a cyclic dependency.
  if (has_standalone_columns) {
    BuildSizingCollection(kForColumns);
  }
  if (has_standalone_rows) {
    BuildSizingCollection(kForRows);
  }

  sizing_tree->SetSizingNodeData(node, std::move(grid_items),
                                 std::move(layout_data));
}

// TODO(almaher): Need to add an instance for GridLanesLayoutAlgorithm.
template void BuildGridSizingSubtree(const GridLayoutAlgorithm&,
                                     GridSizingTree*,
                                     HeapVector<Member<LayoutBox>>*,
                                     const SubgriddedItemData&,
                                     const GridLineResolver*,
                                     bool,
                                     bool);

template <typename LayoutAlgorithmType>
GridSizingTree BuildGridSizingTree(
    const LayoutAlgorithmType& algorithm,
    HeapVector<Member<LayoutBox>>* opt_oof_children) {
  DCHECK(!algorithm.GetConstraintSpace().GetGridLayoutSubtree());

  GridSizingTree sizing_tree;
  BuildGridSizingSubtree<LayoutAlgorithmType>(algorithm, &sizing_tree,
                                              opt_oof_children);
  return sizing_tree;
}

// TODO(almaher): Need to add an instance for GridLanesLayoutAlgorithm.
template CORE_EXPORT GridSizingTree
BuildGridSizingTree(const GridLayoutAlgorithm&, HeapVector<Member<LayoutBox>>*);

template <typename LayoutAlgorithmType>
GridSizingTree BuildGridSizingTreeIgnoringChildren(
    const LayoutAlgorithmType& algorithm) {
  DCHECK(!algorithm.GetConstraintSpace().GetGridLayoutSubtree());

  GridSizingTree sizing_tree;
  BuildGridSizingSubtree<LayoutAlgorithmType>(
      algorithm, &sizing_tree, /*opt_oof_children=*/nullptr,
      /*opt_subgrid_data=*/kNoSubgriddedItemData,
      /*opt_parent_line_resolver=*/nullptr,
      /*must_invalidate_placement_cache=*/false,
      /*must_ignore_children=*/true);
  return sizing_tree;
}

// TODO(almaher): Need to add an instance for GridLanesLayoutAlgorithm.
template GridSizingTree BuildGridSizingTreeIgnoringChildren(
    const GridLayoutAlgorithm&);

FragmentGeometry CalculateInitialFragmentGeometryForSubgrid(
    const GridItemData& subgrid_data,
    const ConstraintSpace& space,
    const GridSizingSubtree& sizing_subtree) {
  DCHECK(subgrid_data.IsSubgrid());

  const auto& node = To<GridNode>(subgrid_data.node);
  {
    const bool subgrid_has_standalone_columns =
        subgrid_data.is_parallel_with_root_grid
            ? !subgrid_data.has_subgridded_columns
            : !subgrid_data.has_subgridded_rows;

    // We won't be able to resolve the intrinsic sizes of a subgrid if its
    // tracks are subgridded, i.e., their sizes can't be resolved by the subgrid
    // itself, or if `sizing_subtree` is not provided, i.e., the grid sizing
    // tree it's not completed at this step of the sizing algorithm.
    if (subgrid_has_standalone_columns && sizing_subtree) {
      return CalculateInitialFragmentGeometry(
          space, node, /* break_token */ nullptr,
          [&](SizeType) -> MinMaxSizesResult {
            return node.ComputeSubgridMinMaxSizes(sizing_subtree, space);
          });
    }
  }

  bool needs_to_compute_min_max_sizes = false;

  const auto fragment_geometry = CalculateInitialFragmentGeometry(
      space, node, /* break_token */ nullptr,
      [&needs_to_compute_min_max_sizes](SizeType) -> MinMaxSizesResult {
        // We can't call `ComputeMinMaxSizes` for a subgrid with an incomplete
        // grid sizing tree, as its intrinsic size relies on its subtree. If we
        // end up in this function, we need to use an intrinsic fragment
        // geometry instead to avoid a cyclic dependency.
        needs_to_compute_min_max_sizes = true;
        return MinMaxSizesResult();
      });

  if (needs_to_compute_min_max_sizes) {
    return CalculateInitialFragmentGeometry(space, node,
                                            /* break_token */ nullptr,
                                            /* is_intrinsic */ true);
  }
  return fragment_geometry;
}

std::unique_ptr<GridLayoutTrackCollection> CreateSubgridTrackCollection(
    const SubgriddedItemData& subgrid_data,
    const ComputedStyle& style,
    const ConstraintSpace& space,
    const BoxStrut& border_scrollbar_padding,
    const LogicalSize grid_available_size,
    GridTrackSizingDirection track_direction) {
  DCHECK(subgrid_data.IsSubgrid());

  const bool is_for_columns_in_parent = subgrid_data->is_parallel_with_root_grid
                                            ? track_direction == kForColumns
                                            : track_direction == kForRows;

  const auto& parent_track_collection =
      is_for_columns_in_parent ? subgrid_data.Columns() : subgrid_data.Rows();
  const auto& range_indices = is_for_columns_in_parent
                                  ? subgrid_data->column_range_indices
                                  : subgrid_data->row_range_indices;

  return std::make_unique<GridLayoutTrackCollection>(
      parent_track_collection.CreateSubgridTrackCollection(
          range_indices.begin, range_indices.end,
          GridTrackSizingAlgorithm::CalculateGutterSize(
              style, grid_available_size, track_direction,
              parent_track_collection.GutterSize()),
          ComputeMarginsForSelf(space, style), border_scrollbar_padding,
          track_direction,
          is_for_columns_in_parent
              ? subgrid_data->is_opposite_direction_in_root_grid_columns
              : subgrid_data->is_opposite_direction_in_root_grid_rows));
}

void InitializeTrackCollection(const SubgriddedItemData& opt_subgrid_data,
                               const ComputedStyle& style,
                               const ConstraintSpace& space,
                               const BoxStrut& border_scrollbar_padding,
                               const LogicalSize grid_available_size,
                               GridTrackSizingDirection track_direction,
                               GridLayoutData* layout_data) {
  if (layout_data->HasSubgriddedAxis(track_direction)) {
    // If we don't have a sizing collection for this axis, then we're in a
    // subgrid that must inherit the track collection of its parent grid.
    DCHECK(opt_subgrid_data.IsSubgrid());

    layout_data->SetTrackCollection(CreateSubgridTrackCollection(
        opt_subgrid_data, style, space, border_scrollbar_padding,
        grid_available_size, track_direction));
    return;
  }

  auto& track_collection = layout_data->SizingCollection(track_direction);
  track_collection.BuildSets(style, grid_available_size);
}

}  // namespace blink
