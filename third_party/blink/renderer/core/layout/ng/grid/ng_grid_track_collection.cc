// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_track_collection.h"

#include "base/check.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_data.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

struct SameSizeAsNGGridLayoutTrackCollectionRange {
  wtf_size_t members[4];
  wtf_size_t bitfields;
};

ASSERT_SIZE(NGGridLayoutTrackCollection::Range,
            SameSizeAsNGGridLayoutTrackCollectionRange);

wtf_size_t NGGridTrackCollectionBase::RangeEndLine(
    wtf_size_t range_index) const {
  return RangeStartLine(range_index) + RangeTrackCount(range_index);
}

wtf_size_t NGGridTrackCollectionBase::RangeIndexFromGridLine(
    wtf_size_t grid_line) const {
  wtf_size_t upper = RangeCount();
  DCHECK_GT(upper, 0u);

  const wtf_size_t last_grid_line =
      RangeStartLine(upper - 1) + RangeTrackCount(upper - 1);
  DCHECK_LT(grid_line, last_grid_line);

  // Do a binary search on the ranges.
  wtf_size_t lower = 0;
  while (lower < upper) {
    const wtf_size_t center = (lower + upper) >> 1;
    const wtf_size_t start_line = RangeStartLine(center);

    if (grid_line < start_line)
      upper = center;
    else if (grid_line < start_line + RangeTrackCount(center))
      return center;
    else
      lower = center + 1;
  }
  return lower;
}

bool NGGridBlockTrackCollection::Range::IsCollapsed() const {
  return properties.HasProperty(TrackSpanProperties::kIsCollapsed);
}

bool NGGridBlockTrackCollection::Range::IsImplicit() const {
  return properties.HasProperty(TrackSpanProperties::kIsImplicit);
}

void NGGridBlockTrackCollection::Range::SetIsCollapsed() {
  properties.SetProperty(TrackSpanProperties::kIsCollapsed);
}

void NGGridBlockTrackCollection::Range::SetIsImplicit() {
  properties.SetProperty(TrackSpanProperties::kIsImplicit);
}

NGGridBlockTrackCollection::NGGridBlockTrackCollection(
    const ComputedStyle& grid_style,
    const NGGridPlacementData& placement_data,
    GridTrackSizingDirection track_direction)
    : NGGridTrackCollectionBase(track_direction),
      auto_repetitions_((track_direction == kForColumns)
                            ? placement_data.column_auto_repetitions
                            : placement_data.row_auto_repetitions),
      start_offset_((track_direction == kForColumns)
                        ? placement_data.column_start_offset
                        : placement_data.row_start_offset),
      track_indices_need_sort_(false),
      explicit_tracks_((track_direction == kForColumns)
                           ? grid_style.GridTemplateColumns().TrackList()
                           : grid_style.GridTemplateRows().TrackList()),
      implicit_tracks_((track_direction == kForColumns)
                           ? grid_style.GridAutoColumns().NGTrackList()
                           : grid_style.GridAutoRows().NGTrackList()) {
  // The implicit track list should have only one repeater, if any.
  DCHECK_LE(implicit_tracks_.RepeaterCount(), 1u);
  DCHECK_NE(kNotFound, auto_repetitions_);

  const wtf_size_t repeater_count = explicit_tracks_.RepeaterCount();

  // Add extra capacity for the extra lines needed for named grids.
  start_lines_.ReserveCapacity(repeater_count + 1);
  end_lines_.ReserveCapacity(repeater_count + 1);

  wtf_size_t current_repeater_start_line = start_offset_;
  for (wtf_size_t i = 0; i < repeater_count; ++i) {
    const wtf_size_t repeater_track_count =
        explicit_tracks_.RepeatCount(i, auto_repetitions_) *
        explicit_tracks_.RepeatSize(i);
    DCHECK_NE(repeater_track_count, 0u);

    start_lines_.emplace_back(current_repeater_start_line);
    current_repeater_start_line += repeater_track_count;
    end_lines_.emplace_back(current_repeater_start_line);
  }

  // There is a special scenario where named grid areas can be specified through
  // the "grid-template" property with no specified explicit grid; such case is
  // tricky because the computed value of "grid-template-columns" is expected to
  // return the computed size of columns from the named grid areas.
  //
  // In order to guarantee that such columns are included, if the last repeater
  // from the explicit grid ended before the end of the named grid area, add an
  // extra repeater to fulfill the named grid area's span.
  const wtf_size_t named_grid_area_end_line =
      start_offset_ + ((track_direction == kForColumns)
                           ? grid_style.NamedGridAreaColumnCount()
                           : grid_style.NamedGridAreaRowCount());

  if (current_repeater_start_line < named_grid_area_end_line) {
    start_lines_.emplace_back(current_repeater_start_line);
    end_lines_.emplace_back(named_grid_area_end_line);
  }
}

void NGGridBlockTrackCollection::EnsureTrackCoverage(
    wtf_size_t start_line,
    wtf_size_t span_length,
    wtf_size_t* grid_item_start_range_index,
    wtf_size_t* grid_item_end_range_index) {
  DCHECK_NE(kNotFound, start_line);
  DCHECK_NE(kNotFound, span_length);
  DCHECK(grid_item_start_range_index && grid_item_end_range_index);

  track_indices_need_sort_ = true;
  start_lines_.emplace_back(start_line, grid_item_start_range_index);
  end_lines_.emplace_back(start_line + span_length, grid_item_end_range_index);
}

void NGGridBlockTrackCollection::FinalizeRanges() {
  DCHECK(ranges_.IsEmpty());

  // Sort start and ending tracks from low to high.
  if (track_indices_need_sort_) {
    auto CompareTrackBoundaries = [](const TrackBoundaryToRangePair& a,
                                     const TrackBoundaryToRangePair& b) {
      return a.grid_line < b.grid_line;
    };
    std::sort(start_lines_.begin(), start_lines_.end(), CompareTrackBoundaries);
    std::sort(end_lines_.begin(), end_lines_.end(), CompareTrackBoundaries);
  }

  bool is_in_auto_fit_range = false;
  wtf_size_t current_range_start_line = 0u;
  wtf_size_t open_items_or_repeaters = 0u;
  wtf_size_t current_explicit_grid_line = start_offset_;
  wtf_size_t current_explicit_repeater_index = kNotFound;
  wtf_size_t explicit_repeater_count = explicit_tracks_.RepeaterCount();

  // If the explicit grid is not empty, |start_offset_| is the translated index
  // of the first track in |explicit_tracks_|; otherwise, the next repeater
  // does not exist, fallback to |kNotFound|.
  wtf_size_t next_explicit_repeater_start =
      explicit_repeater_count ? start_offset_ : kNotFound;

  // Index of the start/end line we are currently processing.
  wtf_size_t start_line_index = 0u;
  wtf_size_t end_line_index = 0u;

  while (true) {
    // Identify starting tracks index.
    while (start_line_index < start_lines_.size() &&
           current_range_start_line >=
               start_lines_[start_line_index].grid_line) {
      ++start_line_index;
      ++open_items_or_repeaters;
    }

    // Identify ending tracks index.
    while (end_line_index < end_lines_.size() &&
           current_range_start_line >= end_lines_[end_line_index].grid_line) {
      ++end_line_index;
      --open_items_or_repeaters;
      DCHECK_GE(open_items_or_repeaters, 0u);
    }

    // Identify ending tracks index.
    if (end_line_index >= end_lines_.size()) {
#if DCHECK_IS_ON()
      DCHECK_EQ(open_items_or_repeaters, 0u);
      // If we exhausted the end indices, then we must have already exhausted
      // the repeaters, or are located at the end of the last repeater.
      if (current_explicit_repeater_index != kNotFound) {
        DCHECK_EQ(current_explicit_repeater_index, explicit_repeater_count - 1);
        DCHECK_EQ(current_range_start_line, next_explicit_repeater_start);
      }
#endif
      break;
    }

    // Determine the next starting and ending track index.
    wtf_size_t next_start_line = (start_line_index < start_lines_.size())
                                     ? start_lines_[start_line_index].grid_line
                                     : kNotFound;
    wtf_size_t next_end_line = end_lines_[end_line_index].grid_line;

    // Move to the start of the next explicit repeater.
    while (current_range_start_line == next_explicit_repeater_start) {
      current_explicit_grid_line = next_explicit_repeater_start;

      // No next repeater, break and use implicit grid tracks.
      if (++current_explicit_repeater_index == explicit_repeater_count) {
        current_explicit_repeater_index = kNotFound;
        is_in_auto_fit_range = false;
        break;
      }

      is_in_auto_fit_range =
          explicit_tracks_.RepeatType(current_explicit_repeater_index) ==
          NGGridTrackRepeater::RepeatType::kAutoFit;
      next_explicit_repeater_start +=
          explicit_tracks_.RepeatSize(current_explicit_repeater_index) *
          explicit_tracks_.RepeatCount(current_explicit_repeater_index,
                                       auto_repetitions_);
    }

    // Determine track number and count of the range.
    Range range;
    range.start_line = current_range_start_line;
    DCHECK(next_start_line != kNotFound || next_end_line < next_start_line);
    range.track_count =
        std::min(next_start_line, next_end_line) - current_range_start_line;
    DCHECK_GT(range.track_count, 0u);

    // Compute repeater index and offset.
    if (current_explicit_repeater_index != kNotFound) {
      // This range is contained within a repeater of the explicit grid; at this
      // point, |current_explicit_grid_line| should be set to the start line of
      // such repeater.
      range.repeater_index = current_explicit_repeater_index;
      range.repeater_offset =
          (current_range_start_line - current_explicit_grid_line) %
          explicit_tracks_.RepeatSize(current_explicit_repeater_index);
    } else {
      range.SetIsImplicit();
      if (implicit_tracks_.RepeaterCount() == 0u) {
        // No specified implicit grid tracks, use 'auto'.
        range.repeater_index = kNotFound;
        range.repeater_offset = 0u;
      } else {
        // Otherwise, use the only repeater for implicit grid tracks.
        // There are 2 scenarios we want to cover here:
        //   1. At this point, we should not have reached any explicit repeater,
        //   since |current_explicit_grid_line| was initialized as the start
        //   line of the first explicit repeater (e.g. |start_offset_|), it can
        //   be used to determine the offset of ranges preceding the explicit
        //   grid; the last implicit grid track before the explicit grid
        //   receives the last specified size, and so on backwards.
        //
        //   2. This range is located after any repeater in |explicit_tracks_|,
        //   meaning it was defined with indices beyond the explicit grid.
        //   We should have set |current_explicit_grid_line| to the last line
        //   of the explicit grid at this point, use it to compute the offset of
        //   following implicit tracks; the first track after the explicit grid
        //   receives the first specified size, and so on forwards.
        //
        // Note that for both scenarios we can use the following formula:
        //   (current_range_start_line - current_explicit_grid_line) %
        //   implicit_repeater_size
        // The expression below is equivalent, but uses some modular arithmetic
        // properties to avoid |wtf_size_t| underflow in scenario 1.
        range.repeater_index = 0u;
        wtf_size_t implicit_repeater_size = implicit_tracks_.RepeatSize(0u);
        range.repeater_offset =
            (current_range_start_line + implicit_repeater_size -
             current_explicit_grid_line % implicit_repeater_size) %
            implicit_repeater_size;
      }
    }

    // Cache range-start indices to avoid having to recompute them later.
    // Loop backwards to find all other entries with the same track number. The
    // |start_line_index| will always land 1 position after duplicate entries.
    // Walk back to cache all duplicates until we are at the start of the vector
    // or we have gone over all duplicate entries.
    if (start_line_index != 0) {
      DCHECK_LE(start_line_index, start_lines_.size());
      for (wtf_size_t line_index = start_line_index - 1;
           start_lines_[line_index].grid_line == range.start_line;
           --line_index) {
        if (start_lines_[line_index].grid_item_range_index_to_cache) {
          *start_lines_[line_index].grid_item_range_index_to_cache =
              ranges_.size();
        }
        // This is needed here to avoid underflow.
        if (!line_index)
          break;
      }
    }

    // Cache range-end indices to avoid having to recompute them later. The
    // |end_line_index| will always land at the start of duplicate entries.
    // Cache all duplicate entries by walking forwards until we are at the end
    // of the vector or we have gone over all duplicate entries.
    const wtf_size_t end_line = range.start_line + range.track_count;
    for (wtf_size_t line_index = end_line_index;
         line_index < end_lines_.size() &&
         end_lines_[line_index].grid_line == end_line;
         ++line_index) {
      if (end_lines_[line_index].grid_item_range_index_to_cache)
        *end_lines_[line_index].grid_item_range_index_to_cache = ranges_.size();
    }

    if (is_in_auto_fit_range && open_items_or_repeaters == 1u)
      range.SetIsCollapsed();
    current_range_start_line += range.track_count;
    ranges_.emplace_back(std::move(range));
  }

  // We must have exhausted all start and end indices.
  DCHECK_EQ(start_line_index, start_lines_.size());
  DCHECK_EQ(end_line_index, end_lines_.size());
}

NGGridBlockTrackCollection::NGGridBlockTrackCollection(
    const NGGridTrackList& explicit_tracks,
    const NGGridTrackList& implicit_tracks,
    wtf_size_t auto_repetitions)
    : NGGridTrackCollectionBase(kForColumns),
      auto_repetitions_(auto_repetitions),
      start_offset_(0),
      track_indices_need_sort_(false),
      explicit_tracks_(explicit_tracks),
      implicit_tracks_(implicit_tracks) {
  const wtf_size_t repeater_count = explicit_tracks_.RepeaterCount();

  wtf_size_t current_repeater_start_line = 0;
  for (wtf_size_t i = 0; i < repeater_count; ++i) {
    const wtf_size_t repeater_track_count =
        explicit_tracks_.RepeatCount(i, auto_repetitions_) *
        explicit_tracks_.RepeatSize(i);
    DCHECK_NE(repeater_track_count, 0u);

    start_lines_.emplace_back(current_repeater_start_line);
    current_repeater_start_line += repeater_track_count;
    end_lines_.emplace_back(current_repeater_start_line);
  }
}

wtf_size_t NGGridBlockTrackCollection::RangeStartLine(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index].start_line;
}

wtf_size_t NGGridBlockTrackCollection::RangeTrackCount(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index].track_count;
}

NGGridSet::NGGridSet(wtf_size_t track_count)
    : track_count(track_count),
      track_size(Length::Auto(), Length::Auto()),
      fit_content_limit(kIndefiniteSize) {}

NGGridSet::NGGridSet(wtf_size_t track_count,
                     const GridTrackSize& track_definition,
                     bool is_available_size_indefinite)
    : track_count(track_count),
      track_size(track_definition),
      fit_content_limit(kIndefiniteSize) {
  if (track_size.IsFitContent()) {
    DCHECK(track_size.FitContentTrackBreadth().IsLength());

    // Argument for 'fit-content' is a <percentage> that couldn't be resolved to
    // a definite <length>, normalize to 'minmax(auto, max-content)'.
    if (is_available_size_indefinite &&
        track_size.FitContentTrackBreadth().length().IsPercent()) {
      track_size = GridTrackSize(Length::Auto(), Length::MaxContent());
    }
  } else {
    // Normalize |track_size| into a |kMinMaxTrackSizing| type; follow the
    // definitions from https://drafts.csswg.org/css-grid-2/#algo-terms.
    bool is_unresolvable_percentage_min_function =
        is_available_size_indefinite &&
        track_size.MinTrackBreadth().HasPercentage();

    GridLength normalized_min_track_sizing_function =
        (is_unresolvable_percentage_min_function ||
         track_size.HasFlexMinTrackBreadth())
            ? Length::Auto()
            : track_size.MinTrackBreadth();

    bool is_unresolvable_percentage_max_function =
        is_available_size_indefinite &&
        track_size.MaxTrackBreadth().HasPercentage();

    GridLength normalized_max_track_sizing_function =
        (is_unresolvable_percentage_max_function ||
         track_size.HasAutoMaxTrackBreadth())
            ? Length::Auto()
            : track_size.MaxTrackBreadth();

    track_size = GridTrackSize(normalized_min_track_sizing_function,
                               normalized_max_track_sizing_function);
  }
  DCHECK(track_size.GetType() == kFitContentTrackSizing ||
         track_size.GetType() == kMinMaxTrackSizing);
}

double NGGridSet::FlexFactor() const {
  DCHECK(track_size.HasFlexMaxTrackBreadth());
  return track_size.MaxTrackBreadth().Flex() * track_count;
}

LayoutUnit NGGridSet::BaseSize() const {
  DCHECK(!IsGrowthLimitLessThanBaseSize());
  return base_size;
}

LayoutUnit NGGridSet::GrowthLimit() const {
  DCHECK(!IsGrowthLimitLessThanBaseSize());
  return growth_limit;
}

void NGGridSet::InitBaseSize(LayoutUnit new_base_size) {
  DCHECK_NE(new_base_size, kIndefiniteSize);
  base_size = new_base_size;
  EnsureGrowthLimitIsNotLessThanBaseSize();
}

void NGGridSet::IncreaseBaseSize(LayoutUnit new_base_size) {
  // Expect base size to always grow monotonically.
  DCHECK_NE(new_base_size, kIndefiniteSize);
  DCHECK_LE(base_size, new_base_size);
  base_size = new_base_size;
  EnsureGrowthLimitIsNotLessThanBaseSize();
}

void NGGridSet::IncreaseGrowthLimit(LayoutUnit new_growth_limit) {
  // Growth limit is initialized as infinity; expect it to change from infinity
  // to a definite value and then to always grow monotonically.
  DCHECK_NE(new_growth_limit, kIndefiniteSize);
  DCHECK(!IsGrowthLimitLessThanBaseSize() &&
         (growth_limit == kIndefiniteSize || growth_limit <= new_growth_limit));
  growth_limit = new_growth_limit;
}

void NGGridSet::EnsureGrowthLimitIsNotLessThanBaseSize() {
  if (IsGrowthLimitLessThanBaseSize())
    growth_limit = base_size;
}

bool NGGridSet::IsGrowthLimitLessThanBaseSize() const {
  return growth_limit != kIndefiniteSize && growth_limit < base_size;
}

bool NGGridLayoutTrackCollection::Range::IsCollapsed() const {
  return properties.HasProperty(TrackSpanProperties::kIsCollapsed);
}

NGGridLayoutTrackCollection::NGGridLayoutTrackCollection(
    const NGGridLayoutTrackCollection& other,
    const NGBoxStrut& subgrid_border_scrollbar_padding,
    const NGBoxStrut& subgrid_margins)
    : NGGridLayoutTrackCollection(other) {
  const bool is_for_columns = Direction() == kForColumns;

  sets_geometry_start_offset_ += is_for_columns ? subgrid_margins.inline_start
                                                : subgrid_margins.block_start;
  start_extra_margin_ =
      sets_geometry_start_offset_ +
      (is_for_columns ? subgrid_border_scrollbar_padding.inline_start
                      : subgrid_border_scrollbar_padding.block_start);

  end_extra_margin_ += is_for_columns
                           ? subgrid_margins.inline_end +
                                 subgrid_border_scrollbar_padding.inline_end
                           : subgrid_margins.block_end +
                                 subgrid_border_scrollbar_padding.block_end;
}

bool NGGridLayoutTrackCollection::operator==(
    const NGGridLayoutTrackCollection& other) const {
  return gutter_size_ == other.gutter_size_ &&
         sets_geometry_start_offset_ == other.sets_geometry_start_offset_ &&
         start_extra_margin_ == other.start_extra_margin_ &&
         end_extra_margin_ == other.end_extra_margin_ &&
         ranges_ == other.ranges_ &&
         major_baselines_ == other.major_baselines_ &&
         minor_baselines_ == other.minor_baselines_ &&
         sets_geometry_ == other.sets_geometry_;
}

wtf_size_t NGGridLayoutTrackCollection::RangeStartLine(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index].start_line;
}

wtf_size_t NGGridLayoutTrackCollection::RangeTrackCount(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index].track_count;
}

wtf_size_t NGGridLayoutTrackCollection::RangeSetCount(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index].set_count;
}

wtf_size_t NGGridLayoutTrackCollection::RangeBeginSetIndex(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index].begin_set_index;
}

bool NGGridLayoutTrackCollection::RangeHasTrackSpanProperty(
    wtf_size_t range_index,
    TrackSpanProperties::PropertyId property_id) const {
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index].properties.HasProperty(property_id);
}

wtf_size_t NGGridLayoutTrackCollection::EndLineOfImplicitGrid() const {
  if (ranges_.IsEmpty())
    return 0;
  const auto& last_range = ranges_.back();
  return last_range.start_line + last_range.track_count;
}

bool NGGridLayoutTrackCollection::IsGridLineWithinImplicitGrid(
    wtf_size_t grid_line) const {
  DCHECK_NE(grid_line, kNotFound);
  return grid_line <= EndLineOfImplicitGrid();
}

wtf_size_t NGGridLayoutTrackCollection::GetSetCount() const {
  if (ranges_.IsEmpty())
    return 0;
  const auto& last_range = ranges_.back();
  return last_range.begin_set_index + last_range.set_count;
}

LayoutUnit NGGridLayoutTrackCollection::GetSetOffset(
    wtf_size_t set_index) const {
  DCHECK_LT(set_index, sets_geometry_.size());

  // This extra margin is added to set offsets within a subgrid to account for
  // its accumulated margin, border, scrollbar, padding, and gutter size.
  LayoutUnit extra_margin;

  if (!set_index)
    extra_margin = start_extra_margin_;
  else if (set_index == sets_geometry_.size() - 1)
    extra_margin = -end_extra_margin_;

  return sets_geometry_[set_index].offset + extra_margin -
         sets_geometry_start_offset_;
}

wtf_size_t NGGridLayoutTrackCollection::GetSetTrackCount(
    wtf_size_t set_index) const {
  DCHECK_LT(set_index + 1, sets_geometry_.size());
  return sets_geometry_[set_index + 1].track_count;
}

LayoutUnit NGGridLayoutTrackCollection::MajorBaseline(
    wtf_size_t set_index) const {
  DCHECK_LT(set_index, major_baselines_.size());
  return major_baselines_[set_index];
}

LayoutUnit NGGridLayoutTrackCollection::MinorBaseline(
    wtf_size_t set_index) const {
  DCHECK_LT(set_index, minor_baselines_.size());
  return minor_baselines_[set_index];
}

void NGGridLayoutTrackCollection::AdjustSetOffsets(wtf_size_t set_index,
                                                   LayoutUnit delta) {
  DCHECK_LT(set_index, sets_geometry_.size());
  for (wtf_size_t i = set_index; i < sets_geometry_.size(); ++i)
    sets_geometry_[i].offset += delta;
}

LayoutUnit NGGridLayoutTrackCollection::ComputeSetSpanSize() const {
  return ComputeSetSpanSize(0, GetSetCount());
}

LayoutUnit NGGridLayoutTrackCollection::ComputeSetSpanSize(
    wtf_size_t begin_set_index,
    wtf_size_t end_set_index) const {
  DCHECK_LE(begin_set_index, end_set_index);
  DCHECK_LT(end_set_index, sets_geometry_.size());

  if (begin_set_index == end_set_index)
    return LayoutUnit();

  if (IsSpanningIndefiniteSet(begin_set_index, end_set_index))
    return kIndefiniteSize;

  // While the set offsets are guaranteed to be in non-decreasing order, if an
  // extra margin is larger than any of the offsets or the gutter size saturates
  // the end offset, the following difference may become negative.
  return (GetSetOffset(end_set_index) - gutter_size_ -
          GetSetOffset(begin_set_index))
      .ClampNegativeToZero();
}

NGGridLayoutTrackCollection
NGGridLayoutTrackCollection::CreateSubgridCollection(
    wtf_size_t begin_range_index,
    wtf_size_t end_range_index,
    GridTrackSizingDirection subgrid_track_direction) const {
  DCHECK_LE(begin_range_index, end_range_index);
  DCHECK_LT(end_range_index, ranges_.size());

  NGGridLayoutTrackCollection subgrid_collection(subgrid_track_direction);
  subgrid_collection.ranges_.ReserveInitialCapacity(end_range_index + 1 -
                                                    begin_range_index);

  const wtf_size_t start_line_offset = ranges_[begin_range_index].start_line;
  const wtf_size_t begin_set_index = ranges_[begin_range_index].begin_set_index;

  for (wtf_size_t i = begin_range_index; i <= end_range_index; ++i) {
    Range translated_range = ranges_[i];
    translated_range.start_line -= start_line_offset;
    translated_range.begin_set_index -= begin_set_index;
    subgrid_collection.ranges_.emplace_back(std::move(translated_range));
  }

  const wtf_size_t end_set_index = ranges_[end_range_index].begin_set_index +
                                   ranges_[end_range_index].set_count;

  DCHECK_LT(begin_set_index, end_set_index);
  DCHECK_LT(end_set_index, sets_geometry_.size());

  const wtf_size_t set_span_size = end_set_index - begin_set_index;
  const auto first_set_offset = sets_geometry_[begin_set_index].offset;

  subgrid_collection.sets_geometry_.ReserveInitialCapacity(set_span_size + 1);
  subgrid_collection.sets_geometry_.emplace_back(/* offset */ LayoutUnit(),
                                                 /* track_count */ 0);

  for (wtf_size_t i = begin_set_index + 1; i <= end_set_index; ++i) {
    subgrid_collection.sets_geometry_.emplace_back(
        sets_geometry_[i].offset - first_set_offset,
        sets_geometry_[i].track_count);
  }

  if (!major_baselines_.IsEmpty()) {
    DCHECK_LE(end_set_index, major_baselines_.size());
    DCHECK_LE(end_set_index, minor_baselines_.size());

    subgrid_collection.major_baselines_.ReserveInitialCapacity(set_span_size);
    subgrid_collection.minor_baselines_.ReserveInitialCapacity(set_span_size);

    for (wtf_size_t i = begin_set_index; i < end_set_index; ++i) {
      subgrid_collection.major_baselines_.emplace_back(major_baselines_[i]);
      subgrid_collection.minor_baselines_.emplace_back(minor_baselines_[i]);
    }
  }

  subgrid_collection.gutter_size_ = gutter_size_;
  subgrid_collection.sets_geometry_start_offset_ =
      subgrid_collection.start_extra_margin_ = start_extra_margin_;
  subgrid_collection.end_extra_margin_ = end_extra_margin_;

  return subgrid_collection;
}

NGGridSizingTrackCollection::NGGridSizingTrackCollection(
    const NGGridBlockTrackCollection& block_track_collection,
    bool is_available_size_indefinite)
    : NGGridLayoutTrackCollection(block_track_collection.Direction()),
      non_collapsed_track_count_(0) {
  for (const auto& block_track_range : block_track_collection.Ranges()) {
    AppendTrackRange(block_track_range,
                     block_track_range.IsImplicit()
                         ? block_track_collection.ImplicitTracks()
                         : block_track_collection.ExplicitTracks(),
                     is_available_size_indefinite);
  }

  const wtf_size_t set_count = sets_.size() + 1;
  last_indefinite_indices_.ReserveInitialCapacity(set_count);
  sets_geometry_.ReserveInitialCapacity(set_count);
}

NGGridSet& NGGridSizingTrackCollection::GetSetAt(wtf_size_t set_index) {
  DCHECK_LT(set_index, sets_.size());
  return sets_[set_index];
}

const NGGridSet& NGGridSizingTrackCollection::GetSetAt(
    wtf_size_t set_index) const {
  DCHECK_LT(set_index, sets_.size());
  return sets_[set_index];
}

NGGridSizingTrackCollection::SetIterator
NGGridSizingTrackCollection::GetSetIterator() {
  return SetIterator(this, 0, sets_.size());
}

NGGridSizingTrackCollection::ConstSetIterator
NGGridSizingTrackCollection::GetConstSetIterator() const {
  return ConstSetIterator(this, 0, sets_.size());
}

NGGridSizingTrackCollection::SetIterator
NGGridSizingTrackCollection::GetSetIterator(wtf_size_t begin_set_index,
                                            wtf_size_t end_set_index) {
  return SetIterator(this, begin_set_index, end_set_index);
}

bool NGGridSizingTrackCollection::IsSpanningIndefiniteSet(
    wtf_size_t begin_set_index,
    wtf_size_t end_set_index) const {
  if (last_indefinite_indices_.IsEmpty())
    return false;

  DCHECK_LT(begin_set_index, end_set_index);
  DCHECK_LT(end_set_index, last_indefinite_indices_.size());
  const wtf_size_t last_indefinite_index =
      last_indefinite_indices_[end_set_index];

  return last_indefinite_index != kNotFound &&
         begin_set_index <= last_indefinite_index;
}

LayoutUnit NGGridSizingTrackCollection::TotalTrackSize() const {
  if (sets_.IsEmpty())
    return LayoutUnit();

  LayoutUnit total_track_size;
  for (const auto& set : sets_)
    total_track_size += set.BaseSize() + set.track_count * gutter_size_;
  return total_track_size - gutter_size_;
}

void NGGridSizingTrackCollection::InitializeSetsGeometry(
    LayoutUnit first_set_offset,
    LayoutUnit gutter_size) {
  last_indefinite_indices_.Shrink(0);
  sets_geometry_.Shrink(0);

  last_indefinite_indices_.push_back(kNotFound);
  sets_geometry_.emplace_back(first_set_offset, /* track_count */ 0);

  for (const auto& set : sets_) {
    if (set.GrowthLimit() == kIndefiniteSize) {
      last_indefinite_indices_.push_back(last_indefinite_indices_.size() - 1);
    } else {
      first_set_offset += set.GrowthLimit() + set.track_count * gutter_size;
      last_indefinite_indices_.push_back(last_indefinite_indices_.back());
    }

    DCHECK_LE(sets_geometry_.back().offset, first_set_offset);
    sets_geometry_.emplace_back(first_set_offset, set.track_count);
  }
  gutter_size_ = gutter_size;
}

void NGGridSizingTrackCollection::CacheSetsGeometry(LayoutUnit first_set_offset,
                                                    LayoutUnit gutter_size) {
  last_indefinite_indices_.clear();
  sets_geometry_.Shrink(0);

  sets_geometry_.emplace_back(first_set_offset, /* track_count */ 0);
  for (const auto& set : sets_) {
    first_set_offset += set.BaseSize() + set.track_count * gutter_size;
    DCHECK_LE(sets_geometry_.back().offset, first_set_offset);
    sets_geometry_.emplace_back(first_set_offset, set.track_count);
  }
  gutter_size_ = gutter_size;
}

void NGGridSizingTrackCollection::SetIndefiniteGrowthLimitsToBaseSize() {
  for (auto& set : sets_) {
    if (set.GrowthLimit() == kIndefiniteSize)
      set.growth_limit = set.base_size;
  }
}

void NGGridSizingTrackCollection::ResetBaselines() {
  const wtf_size_t set_count = sets_.size();
  major_baselines_ = Vector<LayoutUnit>(set_count);
  minor_baselines_ = Vector<LayoutUnit>(set_count);
}

void NGGridSizingTrackCollection::SetMajorBaseline(
    wtf_size_t set_index,
    LayoutUnit candidate_baseline) {
  DCHECK_LT(set_index, major_baselines_.size());
  if (candidate_baseline > major_baselines_[set_index])
    major_baselines_[set_index] = candidate_baseline;
}

void NGGridSizingTrackCollection::SetMinorBaseline(
    wtf_size_t set_index,
    LayoutUnit candidate_baseline) {
  DCHECK_LT(set_index, minor_baselines_.size());
  if (candidate_baseline > minor_baselines_[set_index])
    minor_baselines_[set_index] = candidate_baseline;
}

void NGGridSizingTrackCollection::AppendTrackRange(
    const NGGridBlockTrackCollection::Range& block_track_range,
    const NGGridTrackList& specified_track_list,
    bool is_available_size_indefinite) {
  Range new_range;

  new_range.begin_set_index = sets_.size();
  new_range.properties = block_track_range.properties;
  new_range.start_line = block_track_range.start_line;
  new_range.track_count = block_track_range.track_count;

  if (block_track_range.repeater_index == kNotFound) {
    // The only case where a range doesn't have a repeater index is when the
    // range is in the implicit grid and there are no auto track definitions;
    // fill the entire range with a single set of 'auto' tracks.
    DCHECK(block_track_range.IsImplicit());

    non_collapsed_track_count_ += new_range.track_count;
    new_range.set_count = 1;
    sets_.emplace_back(new_range.track_count);
  } else if (block_track_range.IsCollapsed()) {
    // Append a range that contains the collapsed tracks, but do not append new
    // sets so that its tracks do not participate in the track sizing algorithm.
    new_range.set_count = 0;
  } else {
    non_collapsed_track_count_ += new_range.track_count;
    wtf_size_t current_repeater_size =
        specified_track_list.RepeatSize(block_track_range.repeater_index);
    DCHECK_LT(block_track_range.repeater_offset, current_repeater_size);

    // The number of different set elements in this range is the number of track
    // definitions from |NGGridBlockTrackCollection| range's repeater clamped by
    // the range's total track count if it's less than the repeater's size.
    new_range.set_count =
        std::min(current_repeater_size, new_range.track_count);
    DCHECK_GT(new_range.set_count, 0u);

    // The following two variables help compute how many tracks a set element
    // compresses; suppose we want to print this range, we would circle through
    // the repeater's track list, starting at the range's repeater offset,
    // printing every definition until the track count for the range is covered:
    //
    // 1. |floor_set_track_count| is the number of times we would return to the
    // range's repeater offset, meaning that every definition in the repeater's
    // track list appears at least that many times within the range.
    wtf_size_t floor_set_track_count =
        new_range.track_count / current_repeater_size;
    // 2. The remaining track count would not complete another iteration over
    // the entire repeater; this means that the first |remaining_track_count|
    // definitions appear one more time in the range.
    wtf_size_t remaining_track_count =
        new_range.track_count % current_repeater_size;

    for (wtf_size_t i = 0; i < new_range.set_count; ++i) {
      wtf_size_t set_track_count =
          floor_set_track_count + ((i < remaining_track_count) ? 1 : 0);
      wtf_size_t set_repeater_offset =
          (block_track_range.repeater_offset + i) % current_repeater_size;
      const GridTrackSize& set_track_size =
          specified_track_list.RepeatTrackSize(block_track_range.repeater_index,
                                               set_repeater_offset);
      sets_.emplace_back(set_track_count, set_track_size,
                         is_available_size_indefinite);

      // Record if any of the tracks depend on the available-size. We need to
      // record any percentage tracks *before* normalization as they will
      // change once the available-size becomes definite.
      if (set_track_size.HasPercentage()) {
        new_range.properties.SetProperty(
            TrackSpanProperties::kIsDependentOnAvailableSize);
      }
    }
  }

  // Cache this range's track span properties.
  for (wtf_size_t i = 0; i < new_range.set_count; ++i) {
    const auto& set_track_size =
        sets_[new_range.begin_set_index + i].track_size;

    // From https://drafts.csswg.org/css-grid-2/#algo-terms, a <flex> minimum
    // sizing function shouldn't happen as it would be normalized to 'auto'.
    DCHECK(!set_track_size.HasFlexMinTrackBreadth());

    if (set_track_size.HasAutoMinTrackBreadth()) {
      new_range.properties.SetProperty(
          TrackSpanProperties::kHasAutoMinimumTrack);
    }
    if (set_track_size.HasFixedMinTrackBreadth()) {
      new_range.properties.SetProperty(
          TrackSpanProperties::kHasFixedMinimumTrack);
    }
    if (set_track_size.HasFixedMaxTrackBreadth()) {
      new_range.properties.SetProperty(
          TrackSpanProperties::kHasFixedMaximumTrack);
    }
    if (set_track_size.HasFlexMaxTrackBreadth()) {
      new_range.properties.SetProperty(TrackSpanProperties::kHasFlexibleTrack);
      new_range.properties.SetProperty(
          TrackSpanProperties::kIsDependentOnAvailableSize);
    }
    if (set_track_size.HasIntrinsicMinTrackBreadth() ||
        set_track_size.HasIntrinsicMaxTrackBreadth()) {
      new_range.properties.SetProperty(TrackSpanProperties::kHasIntrinsicTrack);
    }
    if (!set_track_size.HasFixedMinTrackBreadth() ||
        !set_track_size.HasFixedMaxTrackBreadth() ||
        (set_track_size.MinTrackBreadth().length() !=
         set_track_size.MaxTrackBreadth().length())) {
      new_range.properties.SetProperty(
          TrackSpanProperties::kHasNonDefiniteTrack);
    }
  }
  ranges_.push_back(new_range);
}

}  // namespace blink
