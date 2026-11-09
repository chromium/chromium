// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lanes_gap_accumulator.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/gap/grid_lanes_gap_geometry.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/grid/grid_layout_utils.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

namespace {

// Maps a forward stacking-axis offset, measured from the start of placement, to
// its final offset after fill-reverse and content alignment.
LayoutUnit FinalStackingOffset(LayoutUnit forward_offset,
                               const GridLanesGapGeometryState& state) {
  return state.is_fill_reverse
             ? state.border_scrollbar_padding_start +
                   state.effective_stacking_size - forward_offset +
                   state.content_alignment_translation
             : state.border_scrollbar_padding_start + forward_offset +
                   state.content_alignment_translation;
}

// Returns the final bounds of an opening that is empty for gap decorations.
//
// The first opening in a lane, which has no item above it, and the last
// opening both reach a content edge. That side is left open
// (`LayoutUnit::Min()` or `LayoutUnit::Max()`), so it does not depend on the
// container's final size.
TrackOpeningCoordinates EmptyOpeningCoordinates(
    const TrackOpening& opening,
    bool is_last,
    const GridLanesGapGeometryState& state) {
  LayoutUnit start = LayoutUnit::Min();
  LayoutUnit end = LayoutUnit::Max();

  // Fill-reverse flips the stacking axis, so the forward start becomes the
  // final end.
  LayoutUnit& forward_start = state.is_fill_reverse ? end : start;
  LayoutUnit& forward_end = state.is_fill_reverse ? start : end;

  // Without a candidate, this is the lane's first opening and it starts at the
  // content edge.
  if (opening.alignment_candidate.item) {
    forward_start = FinalStackingOffset(
        is_last ? opening.coordinates.start_position
                : opening.coordinates.start_position - state.stacking_gap,
        state);
  }
  if (!is_last) {
    forward_end = FinalStackingOffset(
        opening.coordinates.end_position - state.stacking_gap, state);
  }
  return {start, end};
}

// Empty openings in a lane arrive in increasing order and never overlap: the
// space between two of them is the stacking contribution of the item between
// them, which is never negative. Openings that touch are kept separate, since
// there is still an item between them.
void AppendEmptyOpening(const TrackOpeningCoordinates& opening,
                        Vector<TrackOpeningCoordinates>& lane_openings) {
  if (opening.end_position <= opening.start_position) {
    return;
  }
  CHECK(lane_openings.empty() ||
        opening.start_position >= lane_openings.back().end_position);
  lane_openings.push_back(opening);
}

// Adds cross gaps at an item's final border-box start and end. Items with
// available alignment space use kItemStart/kItemEnd; all others use the
// occupancy-only types.
void AppendOccupancyCrossGaps(const TrackOpeningCoordinates& bounds,
                             bool has_alignment_space,
                             GridTrackSizingDirection direction,
                             LayoutUnit grid_axis_offset,
                             Vector<CrossGap>& cross_gaps) {
  const LogicalOffset start =
      direction == kForColumns
          ? LogicalOffset(grid_axis_offset, bounds.start_position)
          : LogicalOffset(bounds.start_position, grid_axis_offset);
  const LogicalOffset end =
      direction == kForColumns
          ? LogicalOffset(grid_axis_offset, bounds.end_position)
          : LogicalOffset(bounds.end_position, grid_axis_offset);
  cross_gaps.emplace_back(
      start, has_alignment_space ? CrossGap::Type::kItemStart
                                 : CrossGap::Type::kOccupiedStart);
  cross_gaps.emplace_back(
      end, has_alignment_space ? CrossGap::Type::kItemEnd
                               : CrossGap::Type::kOccupiedEnd);
}

auto CrossGapLess(GridTrackSizingDirection direction) {
  return [direction](const CrossGap& a, const CrossGap& b) {
    return std::pair{a.GetGapOffset(direction), a.GetType()} <
           std::pair{b.GetGapOffset(direction), b.GetType()};
  };
}

// The lane graph orders placement openings, including dense-packed children
// and fill-reverse. Check final offsets too: box edges need not retain that
// order. Entries with equal offsets and types are interchangeable within a
// lane.
void EnsureCrossGapsOrdered(Vector<CrossGap>& gaps,
                            GridTrackSizingDirection direction) {
  const auto less = CrossGapLess(direction);
  if (!std::ranges::is_sorted(gaps, less)) {
    std::ranges::sort(gaps, less);
  }
}

// Stably merges two ordered cross-gap streams.
Vector<CrossGap> MergeCrossGaps(const Vector<CrossGap>& first,
                                const Vector<CrossGap>& second,
                                GridTrackSizingDirection direction) {
  Vector<CrossGap> merged;
  merged.ReserveInitialCapacity(first.size() + second.size());
  std::ranges::merge(first, second, std::back_inserter(merged),
                     CrossGapLess(direction));
  return merged;
}

// Checks whether real gutters cover a cross-gap offset. Advances through
// ordered gutters as offsets increase to avoid rescanning the lane. A missing
// neighboring lane is treated as covered because no main gap exists on that
// side.
class LaneGutterCursor {
  STACK_ALLOCATED();

 public:
  LaneGutterCursor(const Vector<CrossGap>* gutters,
                   GridTrackSizingDirection direction,
                   LayoutUnit half_width,
                   LayoutUnit content_start,
                   LayoutUnit content_end)
      : gutters_(gutters),
        direction_(direction),
        half_width_(half_width),
        content_start_(content_start),
        content_end_(content_end) {}

  bool Covers(LayoutUnit offset) {
    if (!gutters_) {
      return true;
    }
    while (index_ < gutters_->size()) {
      const LayoutUnit center = (*gutters_)[index_].GetGapOffset(direction_);
      if (center <= content_start_ || center + half_width_ < offset) {
        ++index_;
      } else {
        return center < content_end_ && center - half_width_ <= offset;
      }
    }
    return false;
  }

 private:
  const Vector<CrossGap>* gutters_;
  GridTrackSizingDirection direction_;
  LayoutUnit half_width_;
  LayoutUnit content_start_;
  LayoutUnit content_end_;
  wtf_size_t index_ = 0;
};

}  // namespace

GridLanesGapAccumulator::GridLanesGapAccumulator(const ComputedStyle& style)
    : gap_geometry_(MakeGarbageCollected<GridLanesGapGeometry>()) {
  const bool is_reverse_track_direction =
      style.IsReverseGridLanesTrackDirection();
  const bool is_reverse_fill_direction =
      style.IsReverseGridLanesFillDirection();
  if (is_reverse_track_direction || is_reverse_fill_direction) {
    gap_geometry_->SetGapPlacementReversal(
        {is_reverse_track_direction, is_reverse_fill_direction});
  }
}

void GridLanesGapAccumulator::BuildMainGaps(
    const GridLayoutTrackCollection& grid_axis_tracks) {
  collapsed_track_indexes_ = &grid_axis_tracks.CollapsedTrackIndexes();
  raw_track_count_ = grid_axis_tracks.EndLineOfImplicitGrid();
  const GridTrackGapData gap_data = BuildGridTrackGapData(
      grid_axis_tracks, GridTrackGapType::kMain, *gap_geometry_);
  const wtf_size_t track_count = UncollapsedTrackCount();
  if (track_count > 0) {
    CHECK_EQ(gap_geometry_->MainGapCount(), track_count - 1);
  }

  const LayoutUnit gutter_size = grid_axis_tracks.GutterSize();

  const GridTrackSizingDirection grid_axis_direction =
      grid_axis_tracks.Direction();
  gap_geometry_->SetMainDirection(grid_axis_direction);
  if (grid_axis_direction == kForColumns) {
    gap_geometry_->SetInlineGapSize(gutter_size);
    gap_geometry_->SetContentInlineOffsets(gap_data.content_start,
                                           gap_data.content_end);
  } else {
    gap_geometry_->SetBlockGapSize(gutter_size);
    gap_geometry_->SetContentBlockOffsets(gap_data.content_start,
                                          gap_data.content_end);
  }
}

LayoutUnit GridLanesGapAccumulator::FinalGutterCenter(
    const GridLanesItemData& item,
    const GridLanesGapGeometryState& state) const {
  const LayoutUnit half_stacking_gap = state.stacking_gap / 2;
  return FinalStackingOffset(item.ForwardStackingStart() - half_stacking_gap,
                             state);
}

wtf_size_t GridLanesGapAccumulator::UncollapsedTrackCount() const {
  CHECK(collapsed_track_indexes_);
  CHECK_LE(collapsed_track_indexes_->size(), raw_track_count_);
  return raw_track_count_ - collapsed_track_indexes_->size();
}

bool GridLanesGapAccumulator::HasMainGaps() const {
  return gap_geometry_->MainGapCount() > 0;
}

void GridLanesGapAccumulator::CaptureEmptyOpenings(
    const GridLanesRunningPositions& running_positions,
    const GridLanesGapGeometryState& state) {
  CHECK(HasMainGaps());
  CHECK_EQ(running_positions.TrackCount(), raw_track_count_);
  CHECK(running_positions.IsTrackingAlignmentCandidates());

  GridLanesEmptyOpenings empty_openings;
  empty_openings.ReserveInitialCapacity(UncollapsedTrackCount());
  items_with_alignment_space_.ReserveInitialCapacity(UncollapsedTrackCount());

  wtf_size_t collapsed_track_index = 0;
  for (wtf_size_t raw_track_index = 0; raw_track_index < raw_track_count_;
       ++raw_track_index) {
    if (collapsed_track_index < collapsed_track_indexes_->size() &&
        collapsed_track_indexes_->at(collapsed_track_index) ==
            raw_track_index) {
      ++collapsed_track_index;
      continue;
    }

    Vector<TrackOpeningCoordinates>& lane_empty_openings =
        empty_openings.emplace_back();
    auto& alignment_items = items_with_alignment_space_.emplace_back();
    // Openings are ordered along the stacking axis, ending with the last
    // opening. Fill-reverse visits them backwards so final offsets increase.
    const HeapVector<TrackOpening>& openings =
        running_positions.GetTrackOpenings(raw_track_index);
    const wtf_size_t opening_count = openings.size();

    for (wtf_size_t i = 0; i < opening_count; ++i) {
      const wtf_size_t opening_index =
          state.is_fill_reverse ? opening_count - i - 1 : i;
      const auto& opening = openings[opening_index];
      CHECK(opening_index == 0 || opening.alignment_candidate.item);
      if (opening.IsEmptyForGapDecorations()) {
        AppendEmptyOpening(
            EmptyOpeningCoordinates(
                opening, /*is_last=*/opening_index == opening_count - 1, state),
            lane_empty_openings);
      } else {
        alignment_items.push_back(opening.alignment_candidate.item.Get());
      }
    }
  }
  gap_geometry_->SetEmptyOpenings(std::move(empty_openings));
}

const GapGeometry* GridLanesGapAccumulator::FinalizeGapGeometry(
    const GridLanesDataVector& grid_lanes,
    const GridLanesGapGeometryState& state,
    LayoutUnit stacking_content_start,
    LayoutUnit stacking_content_end,
    const BoxFragmentBuilder& builder) {
  const bool is_for_columns = gap_geometry_->GetMainDirection() == kForColumns;

  if (is_for_columns) {
    gap_geometry_->SetContentBlockOffsets(stacking_content_start,
                                          stacking_content_end);
    gap_geometry_->SetBlockGapSize(state.stacking_gap);
  } else {
    gap_geometry_->SetContentInlineOffsets(stacking_content_start,
                                           stacking_content_end);
    gap_geometry_->SetInlineGapSize(state.stacking_gap);
  }

  // Placement is skipped when the container has no in-flow items, so every
  // lane is empty.
  if (grid_lanes.empty() && HasMainGaps()) {
    GridLanesEmptyOpenings empty_openings(
        UncollapsedTrackCount(), {{/*start_position=*/LayoutUnit::Min(),
                                   /*end_position=*/LayoutUnit::Max()}});
    gap_geometry_->SetEmptyOpenings(std::move(empty_openings));
  }

  BuildCrossGaps(grid_lanes, state, builder);

  if (gap_geometry_->MainGapCount() == 0 &&
      gap_geometry_->CrossGapCount() == 0) {
    return nullptr;
  }

  return gap_geometry_;
}

LogicalRect GridLanesGapAccumulator::FinalItemBounds(
    const GridLanesItemData& item,
    const BoxFragmentBuilder& builder) const {
  const wtf_size_t child_index =
      item.grid_lanes_placement_data->builder_child_index;
  CHECK_LT(child_index, builder.Children().size());
  const auto& child = builder.Children()[child_index];
  const LogicalBoxFragment fragment(builder.GetWritingDirection(),
                                    To<PhysicalBoxFragment>(*child.fragment));
  return {child.offset, fragment.Size()};
}

Vector<CrossGap> GridLanesGapAccumulator::BuildArtificialCrossGaps(
    const LaneItems& lane_items,
    wtf_size_t compact_track_index,
    const GridLanesGapGeometryState& state,
    const BoxFragmentBuilder& builder) {
  const auto direction = gap_geometry_->GetMainDirection();
  const auto cross_direction = OppositeDirection(direction);
  const bool is_for_columns = direction == kForColumns;
  const LayoutUnit grid_axis_offset =
      gap_geometry_->GridAxisOffsetForLaneBoundary(compact_track_index);
  const LayoutUnit lane_end =
      gap_geometry_->GridAxisOffsetForLaneBoundary(compact_track_index + 1);
  const auto make_boundary = [&](LayoutUnit offset, CrossGap::Type type) {
    return CrossGap(direction == kForColumns
                        ? LogicalOffset(grid_axis_offset, offset)
                        : LogicalOffset(offset, grid_axis_offset),
                    type);
  };
  Vector<CrossGap> occupied_edges;
  occupied_edges.ReserveInitialCapacity(2 * lane_items.size());
  Vector<CrossGap> spanner_starts;
  Vector<CrossGap> spanner_ends;
  const auto& alignment_items =
      items_with_alignment_space_[compact_track_index];
  wtf_size_t alignment_item_index = 0;
  for (const auto& item : lane_items) {
    // Finalized opening candidates are a subset of the lane graph's placement
    // order, including dense packing and fill-reverse. Match them before
    // sorting final box geometry, which may have a different order.
    const bool has_alignment_space =
        alignment_item_index < alignment_items.size() &&
        alignment_items[alignment_item_index].Get() == item->item.Get();
    if (has_alignment_space) {
      ++alignment_item_index;
    }

    // Grid-axis alignment can leave a spanned lane outside the item's actual
    // border box. Its lane-graph entry does not make that space occupied.
    const auto item_bounds = FinalItemBounds(*item, builder);
    const LayoutUnit grid_start = is_for_columns
                                      ? item_bounds.InlineStartOffset()
                                      : item_bounds.BlockStartOffset();
    const LayoutUnit grid_end = is_for_columns ? item_bounds.InlineEndOffset()
                                               : item_bounds.BlockEndOffset();
    const bool occupies_lane =
        grid_end > grid_axis_offset && grid_start < lane_end;
    const bool is_spanner = item->item->SpanSize(direction) > 1;
    if (!occupies_lane && !is_spanner) {
      continue;
    }
    const TrackOpeningCoordinates bounds{
        is_for_columns ? item_bounds.BlockStartOffset()
                       : item_bounds.InlineStartOffset(),
        is_for_columns ? item_bounds.BlockEndOffset()
                       : item_bounds.InlineEndOffset()};
    if (is_spanner) {
      // Record the stacking-axis clearance: one stacking gap before and after
      // the spanner's final border box.
      spanner_starts.push_back(
          make_boundary(bounds.start_position - state.stacking_gap,
                        CrossGap::Type::kSpannerBoundary));
      spanner_ends.push_back(
          make_boundary(bounds.end_position + state.stacking_gap,
                        CrossGap::Type::kSpannerBoundary));
    }

    if (!occupies_lane) {
      continue;
    }
    AppendOccupancyCrossGaps(bounds, has_alignment_space, direction,
                             grid_axis_offset, occupied_edges);
  }
  CHECK_EQ(alignment_item_index, alignment_items.size());
  // Start and end streams are checked separately: overlapping boxes can have
  // ordered starts but disordered ends. Applying the same stacking gap to
  // every offset in a stream cannot change its order.
  EnsureCrossGapsOrdered(spanner_starts, cross_direction);
  EnsureCrossGapsOrdered(spanner_ends, cross_direction);
  // Final alignment, negative margins, and overlap can reorder individual
  // item edges. Sort only when the final cross-gap order is inverted.
  EnsureCrossGapsOrdered(occupied_edges, cross_direction);
  return MergeCrossGaps(
      occupied_edges,
      MergeCrossGaps(spanner_starts, spanner_ends, cross_direction),
      cross_direction);
}

void GridLanesGapAccumulator::FilterArtificialCrossGaps(
    Vector<CrossGap>& boundaries,
    wtf_size_t compact_track_index,
    const Vector<Vector<CrossGap>>& lane_gutters,
    LayoutUnit stacking_gap) const {
  const auto direction = OppositeDirection(gap_geometry_->GetMainDirection());
  const LayoutUnit content_start = direction == kForRows
                                       ? gap_geometry_->GetContentBlockStart()
                                       : gap_geometry_->GetContentInlineStart();
  const LayoutUnit content_end = direction == kForRows
                                     ? gap_geometry_->GetContentBlockEnd()
                                     : gap_geometry_->GetContentInlineEnd();
  const LayoutUnit half_width = stacking_gap / 2;
  CHECK_LT(compact_track_index, lane_gutters.size());
  LaneGutterCursor own(&lane_gutters[compact_track_index], direction,
                       half_width, content_start, content_end);
  LaneGutterCursor before(compact_track_index > 0
                              ? &lane_gutters[compact_track_index - 1]
                              : nullptr,
                          direction, half_width, content_start, content_end);
  LaneGutterCursor after(compact_track_index + 1 < lane_gutters.size()
                             ? &lane_gutters[compact_track_index + 1]
                             : nullptr,
                         direction, half_width, content_start, content_end);

  wtf_size_t kept_count = 0;
  for (wtf_size_t i = 0; i < boundaries.size(); ++i) {
    auto& boundary = boundaries[i];
    const bool needs_gutter_check =
        boundary.GetType() != CrossGap::Type::kOccupiedStart &&
        boundary.GetType() != CrossGap::Type::kOccupiedEnd;
    // Occupancy-only cross gaps are kept regardless of gutter coverage.
    if (needs_gutter_check) {
      const LayoutUnit offset = boundary.GetGapOffset(direction);
      // A boundary in this lane's gutter is redundant on both sides. A
      // neighboring gutter covers only that neighbor's side, so keep the
      // boundary if the other adjacent main gap still needs it.
      const bool is_covered =
          own.Covers(offset) || (before.Covers(offset) && after.Covers(offset));
      if (offset <= content_start || offset >= content_end || is_covered) {
        if (boundary.GetType() == CrossGap::Type::kItemStart) {
          boundary.SetType(CrossGap::Type::kOccupiedStart);
        } else if (boundary.GetType() == CrossGap::Type::kItemEnd) {
          boundary.SetType(CrossGap::Type::kOccupiedEnd);
        } else {
          // A redundant spanner boundary has no occupancy to retain.
          continue;
        }
      }
    }
    if (kept_count != i) {
      boundaries[kept_count] = boundary;
    }
    ++kept_count;
  }
  boundaries.Shrink(kept_count);
  EnsureCrossGapsOrdered(boundaries, direction);
  DCHECK(std::ranges::is_sorted(boundaries, CrossGapLess(direction)));
}

void GridLanesGapAccumulator::AppendLaneCrossGaps(
    const Vector<CrossGap>& lane_cross_gaps,
    wtf_size_t compact_track_index) {
  for (const auto& gap : lane_cross_gaps) {
    const wtf_size_t cross_gap_index = gap_geometry_->CrossGapCount();
    gap_geometry_->AddCrossGap(gap.GetGapOffset(), gap.GetType());
    if (compact_track_index < gap_geometry_->MainGapCount()) {
      gap_geometry_->MainGapAt(compact_track_index)
          .IncrementRangeOfCrossGapsBefore(cross_gap_index);
    }
    if (compact_track_index > 0) {
      gap_geometry_->MainGapAt(compact_track_index - 1)
          .IncrementRangeOfCrossGapsAfter(cross_gap_index);
    }
  }
}

void GridLanesGapAccumulator::RecordLaneEntry(
    const GridLanesItemData& item,
    bool has_preceding_gap,
    wtf_size_t compact_track_index,
    const GridLanesGapGeometryState& state,
    LaneItems& lane_items,
    Vector<CrossGap>& lane_cross_gaps) {
  lane_items.push_back(&item);
  if (!has_preceding_gap) {
    return;
  }

  const LayoutUnit center = FinalGutterCenter(item, state);
  const LayoutUnit grid_axis_offset =
      gap_geometry_->GridAxisOffsetForLaneBoundary(compact_track_index);
  const bool is_for_columns = gap_geometry_->GetMainDirection() == kForColumns;
  const LogicalOffset offset = is_for_columns
                                   ? LogicalOffset(grid_axis_offset, center)
                                   : LogicalOffset(center, grid_axis_offset);
  lane_cross_gaps.emplace_back(offset);
}

void GridLanesGapAccumulator::RecordLaneItemTree(
    GridLanesItemData& item,
    bool contains_last_item_in_lane,
    wtf_size_t compact_track_index,
    const GridLanesGapGeometryState& state,
    LaneItems& lane_items,
    Vector<CrossGap>& lane_cross_gaps) {
  if (state.is_fill_reverse) {
    const bool has_preceding_gap =
        !contains_last_item_in_lane || !item.items_densely_packed_above.empty();
    RecordLaneEntry(item, has_preceding_gap, compact_track_index, state,
                    lane_items, lane_cross_gaps);
  }

  const auto& densely_packed_items = item.items_densely_packed_above;
  for (wtf_size_t child_index = 0; child_index < densely_packed_items.size();
       ++child_index) {
    bool contains_last_item = contains_last_item_in_lane;
    contains_last_item &= child_index == densely_packed_items.size() - 1;
    RecordLaneItemTree(*densely_packed_items[child_index], contains_last_item,
                       compact_track_index, state, lane_items, lane_cross_gaps);
  }

  if (!state.is_fill_reverse) {
    RecordLaneEntry(item,
                    /*has_preceding_gap=*/!lane_items.empty(),
                    compact_track_index, state, lane_items, lane_cross_gaps);
  }
}

void GridLanesGapAccumulator::RecordMainGapSegmentStates(
    wtf_size_t main_gap_index,
    const LaneItems& before_items,
    const LaneItems& after_items) {
  if (before_items.empty() || after_items.empty()) {
    return;
  }

  MainGap& main_gap = gap_geometry_->MainGapAt(main_gap_index);
  GridLanesMainGapSegmentWalker walker(*gap_geometry_, main_gap_index);
  wtf_size_t segment_index = 0;
  std::optional<wtf_size_t> blocked_run_start;

  // Walk through the gap decoration segments for this `MainGap` to mark the
  // segments that are blocked by a spanner.
  while (const auto segment = walker.Next()) {
    CHECK_LT(segment->before_occupant_index, before_items.size());
    CHECK_LT(segment->after_occupant_index, after_items.size());
    // Entries in adjacent lanes share placement data only when one item spans
    // both lanes, so the segment is blocked.
    const bool is_blocked =
        before_items[segment->before_occupant_index]
            ->grid_lanes_placement_data ==
        after_items[segment->after_occupant_index]->grid_lanes_placement_data;

    if (is_blocked) {
      if (!blocked_run_start) {
        blocked_run_start = segment_index;
      }
    } else if (blocked_run_start) {
      main_gap.AddGapSegmentStateRange(
          {*blocked_run_start, segment_index,
           GapSegmentState(GapSegmentState::kBlocked)});
      blocked_run_start.reset();
    }

    ++segment_index;
  }

  if (blocked_run_start) {
    // Close a blocked run that extends through the final segment.
    main_gap.AddGapSegmentStateRange(
        {*blocked_run_start, segment_index,
         GapSegmentState(GapSegmentState::kBlocked)});
  }
}

void GridLanesGapAccumulator::CollectLaneEntries(
    const GridLanesDataVector& grid_lanes,
    const GridLanesGapGeometryState& state,
    HeapVector<LaneItems>& lane_items,
    Vector<Vector<CrossGap>>& lane_gutters) {
  CHECK_EQ(grid_lanes.size(), raw_track_count_);
  CHECK(collapsed_track_indexes_);
  wtf_size_t collapsed_track_index = 0;
  for (wtf_size_t raw_track_index = 0; raw_track_index < grid_lanes.size();
       ++raw_track_index) {
    if (collapsed_track_index < collapsed_track_indexes_->size() &&
        collapsed_track_indexes_->at(collapsed_track_index) ==
            raw_track_index) {
      ++collapsed_track_index;
      continue;
    }
    const GridLaneData& lane_data = grid_lanes[raw_track_index];
    const wtf_size_t compact_track_index = lane_gutters.size();
    auto& items_in_lane = lane_items.emplace_back();
    auto& gutters_in_lane = lane_gutters.emplace_back();
    const auto& items = lane_data.item_data;
    for (wtf_size_t item_index = 0; item_index < items.size(); ++item_index) {
      RecordLaneItemTree(
          *items[item_index],
          /*contains_last_item_in_lane=*/item_index == items.size() - 1,
          compact_track_index, state, items_in_lane, gutters_in_lane);
    }

    DCHECK_EQ(gutters_in_lane.size(),
              items_in_lane.empty() ? 0u : items_in_lane.size() - 1);
    EnsureCrossGapsOrdered(
        gutters_in_lane, OppositeDirection(gap_geometry_->GetMainDirection()));
  }
  CHECK_EQ(lane_gutters.size(), UncollapsedTrackCount());
  CHECK_EQ(collapsed_track_index, collapsed_track_indexes_->size());
}

void GridLanesGapAccumulator::BuildCrossGaps(
    const GridLanesDataVector& grid_lanes,
    const GridLanesGapGeometryState& state,
    const BoxFragmentBuilder&) {
  // Placement is skipped when the container has no in-flow items. Main gaps
  // still exist, but there is no lane graph from which to build cross gaps.
  if (grid_lanes.empty()) {
    return;
  }

  HeapVector<LaneItems> lane_items;
  Vector<Vector<CrossGap>> lane_gutters;
  CollectLaneEntries(grid_lanes, state, lane_items, lane_gutters);

  for (wtf_size_t compact_track_index = 0;
       compact_track_index < lane_items.size(); ++compact_track_index) {
    AppendLaneCrossGaps(lane_gutters[compact_track_index], compact_track_index);
    if (compact_track_index > 0) {
      RecordMainGapSegmentStates(compact_track_index - 1,
                                 lane_items[compact_track_index - 1],
                                 lane_items[compact_track_index]);
    }
  }
}

}  // namespace blink
