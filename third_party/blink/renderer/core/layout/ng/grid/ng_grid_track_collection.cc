// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_track_collection.h"

#include "base/check.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_data.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

struct SameSizeAsNGGridRange {
  wtf_size_t members[6];
  wtf_size_t bitfields;
};

ASSERT_SIZE(NGGridRange, SameSizeAsNGGridRange);

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

bool NGGridRange::IsCollapsed() const {
  return properties.HasProperty(TrackSpanProperties::kIsCollapsed);
}

bool NGGridRange::IsImplicit() const {
  return properties.HasProperty(TrackSpanProperties::kIsImplicit);
}

void NGGridRange::SetIsCollapsed() {
  properties.SetProperty(TrackSpanProperties::kIsCollapsed);
}

void NGGridRange::SetIsImplicit() {
  properties.SetProperty(TrackSpanProperties::kIsImplicit);
}

NGGridRangeBuilder::NGGridRangeBuilder(
    const ComputedStyle& grid_style,
    const NGGridPlacementData& placement_data,
    GridTrackSizingDirection track_direction)
    : auto_repetitions_(placement_data.AutoRepetitions(track_direction)),
      start_offset_((track_direction == kForColumns)
                        ? placement_data.column_start_offset
                        : placement_data.row_start_offset),
      must_sort_grid_lines_(false),
      explicit_tracks_((track_direction == kForColumns)
                           ? grid_style.GridTemplateColumns().TrackList()
                           : grid_style.GridTemplateRows().TrackList()),
      implicit_tracks_((track_direction == kForColumns)
                           ? grid_style.GridAutoColumns().NGTrackList()
                           : grid_style.GridAutoRows().NGTrackList()) {
  // The implicit track list should have only one repeater, if any.
  DCHECK_LE(implicit_tracks_.RepeaterCount(), 1u);
  DCHECK_NE(auto_repetitions_, kNotFound);

  const wtf_size_t repeater_count = explicit_tracks_.RepeaterCount();

  // Add extra capacity for the extra lines needed for named grids.
  start_lines_.ReserveInitialCapacity(repeater_count + 1);
  end_lines_.ReserveInitialCapacity(repeater_count + 1);

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

void NGGridRangeBuilder::EnsureTrackCoverage(
    wtf_size_t start_line,
    wtf_size_t span_length,
    wtf_size_t* grid_item_start_range_index,
    wtf_size_t* grid_item_end_range_index) {
  DCHECK_NE(start_line, kNotFound);
  DCHECK_NE(span_length, kNotFound);
  DCHECK(grid_item_start_range_index && grid_item_end_range_index);

  must_sort_grid_lines_ = true;
  start_lines_.emplace_back(start_line, grid_item_start_range_index);
  end_lines_.emplace_back(start_line + span_length, grid_item_end_range_index);
}

NGGridRangeVector NGGridRangeBuilder::FinalizeRanges() {
  DCHECK_EQ(start_lines_.size(), end_lines_.size());

  // Sort start and ending tracks from low to high.
  if (must_sort_grid_lines_) {
    auto CompareTrackBoundaries = [](const TrackBoundaryToRangePair& a,
                                     const TrackBoundaryToRangePair& b) {
      return a.grid_line < b.grid_line;
    };
    std::sort(start_lines_.begin(), start_lines_.end(), CompareTrackBoundaries);
    std::sort(end_lines_.begin(), end_lines_.end(), CompareTrackBoundaries);
    must_sort_grid_lines_ = false;
  }

  const wtf_size_t explicit_repeater_count = explicit_tracks_.RepeaterCount();
  const wtf_size_t grid_line_count = start_lines_.size();

  NGGridRangeVector ranges;
  bool is_in_auto_fit_range = false;

  wtf_size_t current_explicit_grid_line = start_offset_;
  wtf_size_t current_explicit_repeater_index = kNotFound;
  wtf_size_t current_range_start_line = 0;
  wtf_size_t current_set_index = 0;
  wtf_size_t open_items_or_repeaters = 0;

  // If the explicit grid is not empty, |start_offset_| is the translated index
  // of the first track in |explicit_tracks_|; otherwise, the next repeater
  // does not exist, fallback to |kNotFound|.
  wtf_size_t next_explicit_repeater_start =
      explicit_repeater_count ? start_offset_ : kNotFound;

  // Index of the start/end line we are currently processing.
  wtf_size_t start_line_index = 0;
  wtf_size_t end_line_index = 0;

  while (true) {
    // Identify starting tracks index.
    while (start_line_index < grid_line_count &&
           current_range_start_line >=
               start_lines_[start_line_index].grid_line) {
      ++start_line_index;
      ++open_items_or_repeaters;
    }

    // Identify ending tracks index.
    while (end_line_index < grid_line_count &&
           current_range_start_line >= end_lines_[end_line_index].grid_line) {
      ++end_line_index;
      --open_items_or_repeaters;
      DCHECK_GE(open_items_or_repeaters, 0u);
    }

    if (end_line_index >= grid_line_count)
      break;

    // Determine the next starting and ending track index.
    const wtf_size_t next_start_line =
        (start_line_index < grid_line_count)
            ? start_lines_[start_line_index].grid_line
            : kNotFound;
    const wtf_size_t next_end_line = end_lines_[end_line_index].grid_line;
    DCHECK(next_start_line != kNotFound || next_end_line < next_start_line);

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

    // Compute this range's begin set index, start line, and track count.
    NGGridRange range;
    wtf_size_t current_repeater_size = 1;
    range.start_line = current_range_start_line;
    range.track_count =
        std::min(next_start_line, next_end_line) - current_range_start_line;
    DCHECK_GT(range.track_count, 0u);

    // Compute current repeater's index, size, and offset.
    range.begin_set_index = current_set_index;
    if (current_explicit_repeater_index != kNotFound) {
      current_repeater_size =
          explicit_tracks_.RepeatSize(current_explicit_repeater_index);

      // This range is contained within a repeater of the explicit grid; at this
      // point, |current_explicit_grid_line| should be set to the start line of
      // such repeater.
      range.repeater_index = current_explicit_repeater_index;
      range.repeater_offset =
          (current_range_start_line - current_explicit_grid_line) %
          current_repeater_size;
    } else {
      range.SetIsImplicit();
      if (!implicit_tracks_.RepeaterCount()) {
        // No specified implicit grid tracks, use 'auto'.
        range.repeater_index = kNotFound;
        range.repeater_offset = 0u;
      } else {
        current_repeater_size = implicit_tracks_.RepeatSize(0);

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
        //   current_repeater_size
        // The expression below is equivalent, but uses some modular arithmetic
        // properties to avoid |wtf_size_t| underflow in scenario 1.
        range.repeater_index = 0;
        range.repeater_offset =
            (current_range_start_line + current_repeater_size -
             current_explicit_grid_line % current_repeater_size) %
            current_repeater_size;
      }
    }

    // Cache range-start indices to avoid having to recompute them later.
    // Loop backwards to find all other entries with the same track number. The
    // |start_line_index| will always land 1 position after duplicate entries.
    // Walk back to cache all duplicates until we are at the start of the vector
    // or we have gone over all duplicate entries.
    if (start_line_index != 0) {
      DCHECK_LE(start_line_index, grid_line_count);
      for (wtf_size_t line_index = start_line_index - 1;
           start_lines_[line_index].grid_line == range.start_line;
           --line_index) {
        if (start_lines_[line_index].grid_item_range_index_to_cache) {
          *start_lines_[line_index].grid_item_range_index_to_cache =
              ranges.size();
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
         line_index < grid_line_count &&
         end_lines_[line_index].grid_line == end_line;
         ++line_index) {
      if (end_lines_[line_index].grid_item_range_index_to_cache)
        *end_lines_[line_index].grid_item_range_index_to_cache = ranges.size();
    }

    if (is_in_auto_fit_range && open_items_or_repeaters == 1) {
      range.SetIsCollapsed();
      range.set_count = 0;
    } else {
      // If this is a non-collapsed range, the number of sets in this range is
      // the number of track definitions in the current repeater clamped by the
      // track count if it's less than the repeater's size.
      range.set_count = std::min(current_repeater_size, range.track_count);
      DCHECK_GT(range.set_count, 0u);
    }

    current_range_start_line += range.track_count;
    current_set_index += range.set_count;
    ranges.emplace_back(std::move(range));
  }

#if DCHECK_IS_ON()
  // We must have exhausted all start and end indices.
  DCHECK_EQ(start_line_index, grid_line_count);
  DCHECK_EQ(end_line_index, grid_line_count);
  DCHECK_EQ(open_items_or_repeaters, 0u);

  // If we exhausted the end indices, then we must have already exhausted the
  // repeaters, or are located at the end of the last repeater.
  if (current_explicit_repeater_index != kNotFound) {
    DCHECK_EQ(current_explicit_repeater_index, explicit_repeater_count - 1);
    DCHECK_EQ(current_range_start_line, next_explicit_repeater_start);
  }
#endif
  return ranges;
}

NGGridRangeBuilder::NGGridRangeBuilder(const NGGridTrackList& explicit_tracks,
                                       const NGGridTrackList& implicit_tracks,
                                       wtf_size_t auto_repetitions)
    : auto_repetitions_(auto_repetitions),
      start_offset_(0),
      must_sort_grid_lines_(false),
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

NGGridLayoutTrackCollection::NGGridLayoutTrackCollection(
    const NGGridLayoutTrackCollection& other,
    const NGBoxStrut& subgrid_border_scrollbar_padding,
    const NGBoxStrut& subgrid_margins)
    : NGGridLayoutTrackCollection(other) {
  const bool is_for_columns = track_direction_ == kForColumns;

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
         baselines_.has_value() == other.baselines_.has_value() &&
         (!baselines_ || (baselines_->major == other.baselines_->major &&
                          baselines_->minor == other.baselines_->minor)) &&
         ranges_ == other.ranges_ && sets_geometry_ == other.sets_geometry_;
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

TrackSpanProperties NGGridLayoutTrackCollection::RangeProperties(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index].properties;
}

wtf_size_t NGGridLayoutTrackCollection::EndLineOfImplicitGrid() const {
  if (ranges_.empty())
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
  if (ranges_.empty())
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
  DCHECK(baselines_ && set_index < baselines_->major.size());
  return baselines_->major[set_index];
}

LayoutUnit NGGridLayoutTrackCollection::MinorBaseline(
    wtf_size_t set_index) const {
  DCHECK(baselines_ && set_index < baselines_->minor.size());
  return baselines_->minor[set_index];
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
    NGGridRange translated_range = ranges_[i];
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

  if (baselines_ && !baselines_->major.empty()) {
    DCHECK_LE(end_set_index, baselines_->major.size());
    DCHECK_LE(end_set_index, baselines_->minor.size());

    Baselines subgrid_baselines;
    subgrid_baselines.major.ReserveInitialCapacity(set_span_size);
    subgrid_baselines.minor.ReserveInitialCapacity(set_span_size);

    for (wtf_size_t i = begin_set_index; i < end_set_index; ++i) {
      subgrid_baselines.major.emplace_back(baselines_->major[i]);
      subgrid_baselines.minor.emplace_back(baselines_->minor[i]);
    }
    subgrid_collection.baselines_ = std::move(subgrid_baselines);
  }

  subgrid_collection.gutter_size_ = gutter_size_;
  subgrid_collection.sets_geometry_start_offset_ =
      subgrid_collection.start_extra_margin_ = start_extra_margin_;
  subgrid_collection.end_extra_margin_ = end_extra_margin_;

  return subgrid_collection;
}

bool NGGridLayoutTrackCollection::HasFlexibleTrack() const {
  return properties_.HasProperty(TrackSpanProperties::kHasFlexibleTrack);
}

bool NGGridLayoutTrackCollection::HasIntrinsicTrack() const {
  return properties_.HasProperty(TrackSpanProperties::kHasIntrinsicTrack);
}

bool NGGridLayoutTrackCollection::IsDependentOnAvailableSize() const {
  return properties_.HasProperty(
      TrackSpanProperties::kIsDependentOnAvailableSize);
}

bool NGGridLayoutTrackCollection::IsSpanningOnlyDefiniteTracks() const {
  return !properties_.HasProperty(TrackSpanProperties::kHasNonDefiniteTrack);
}

NGGridSizingTrackCollection::NGGridSizingTrackCollection(
    NGGridRangeVector&& ranges,
    bool must_create_baselines,
    GridTrackSizingDirection track_direction)
    : NGGridLayoutTrackCollection(track_direction) {
  ranges_ = std::move(ranges);

  if (must_create_baselines)
    baselines_ = Baselines();

  wtf_size_t set_count = 0;
  for (const auto& range : ranges_) {
    if (!range.IsCollapsed()) {
      non_collapsed_track_count_ += range.track_count;
      set_count += range.set_count;
    }
  }

  last_indefinite_indices_.ReserveInitialCapacity(set_count + 1);
  sets_geometry_.ReserveInitialCapacity(set_count + 1);
  sets_.ReserveInitialCapacity(set_count);
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
  if (last_indefinite_indices_.empty())
    return false;

  DCHECK_LT(begin_set_index, end_set_index);
  DCHECK_LT(end_set_index, last_indefinite_indices_.size());
  const wtf_size_t last_indefinite_index =
      last_indefinite_indices_[end_set_index];

  return last_indefinite_index != kNotFound &&
         begin_set_index <= last_indefinite_index;
}

LayoutUnit NGGridSizingTrackCollection::TotalTrackSize() const {
  if (sets_.empty())
    return LayoutUnit();

  LayoutUnit total_track_size;
  for (const auto& set : sets_)
    total_track_size += set.BaseSize() + set.track_count * gutter_size_;
  return total_track_size - gutter_size_;
}

void NGGridSizingTrackCollection::CacheDefiniteSetsGeometry() {
  DCHECK(sets_geometry_.empty() && last_indefinite_indices_.empty());

  LayoutUnit first_set_offset;
  last_indefinite_indices_.push_back(kNotFound);
  sets_geometry_.emplace_back(first_set_offset, /* track_count */ 0);

  for (const auto& set : sets_) {
    if (set.track_size.IsDefinite()) {
      first_set_offset += set.base_size + gutter_size_ * set.track_count;
      last_indefinite_indices_.push_back(last_indefinite_indices_.back());
    } else {
      last_indefinite_indices_.push_back(last_indefinite_indices_.size() - 1);
    }

    DCHECK_LE(sets_geometry_.back().offset, first_set_offset);
    sets_geometry_.emplace_back(first_set_offset, set.track_count);
  }
}

void NGGridSizingTrackCollection::CacheInitializedSetsGeometry(
    LayoutUnit first_set_offset) {
  last_indefinite_indices_.Shrink(0);
  sets_geometry_.Shrink(0);

  last_indefinite_indices_.push_back(kNotFound);
  sets_geometry_.emplace_back(first_set_offset, /* track_count */ 0);

  for (const auto& set : sets_) {
    if (set.growth_limit == kIndefiniteSize) {
      last_indefinite_indices_.push_back(last_indefinite_indices_.size() - 1);
    } else {
      first_set_offset += set.growth_limit + gutter_size_ * set.track_count;
      last_indefinite_indices_.push_back(last_indefinite_indices_.back());
    }

    DCHECK_LE(sets_geometry_.back().offset, first_set_offset);
    sets_geometry_.emplace_back(first_set_offset, set.track_count);
  }
}

void NGGridSizingTrackCollection::FinalizeSetsGeometry(
    LayoutUnit first_set_offset,
    LayoutUnit override_gutter_size) {
  gutter_size_ = override_gutter_size;

  last_indefinite_indices_.Shrink(0);
  sets_geometry_.Shrink(0);

  sets_geometry_.emplace_back(first_set_offset, /* track_count */ 0);

  for (const auto& set : sets_) {
    first_set_offset += set.BaseSize() + gutter_size_ * set.track_count;
    DCHECK_LE(sets_geometry_.back().offset, first_set_offset);
    sets_geometry_.emplace_back(first_set_offset, set.track_count);
  }
}

void NGGridSizingTrackCollection::SetIndefiniteGrowthLimitsToBaseSize() {
  for (auto& set : sets_) {
    if (set.GrowthLimit() == kIndefiniteSize)
      set.growth_limit = set.base_size;
  }
}

void NGGridSizingTrackCollection::ResetBaselines() {
  DCHECK(baselines_);

  const wtf_size_t set_count = sets_.size();
  baselines_->major = Vector<LayoutUnit, 16>(set_count, LayoutUnit::Min());
  baselines_->minor = Vector<LayoutUnit, 16>(set_count, LayoutUnit::Min());
}

void NGGridSizingTrackCollection::SetMajorBaseline(
    wtf_size_t set_index,
    LayoutUnit candidate_baseline) {
  DCHECK(baselines_ && set_index < baselines_->major.size());
  if (candidate_baseline > baselines_->major[set_index])
    baselines_->major[set_index] = candidate_baseline;
}

void NGGridSizingTrackCollection::SetMinorBaseline(
    wtf_size_t set_index,
    LayoutUnit candidate_baseline) {
  DCHECK(baselines_ && set_index < baselines_->minor.size());
  if (candidate_baseline > baselines_->minor[set_index])
    baselines_->minor[set_index] = candidate_baseline;
}

void NGGridSizingTrackCollection::BuildSets(const ComputedStyle& grid_style,
                                            LayoutUnit grid_available_size) {
  const bool is_for_columns = track_direction_ == kForColumns;

  BuildSets(is_for_columns ? grid_style.GridTemplateColumns().TrackList()
                           : grid_style.GridTemplateRows().TrackList(),
            is_for_columns ? grid_style.GridAutoColumns().NGTrackList()
                           : grid_style.GridAutoRows().NGTrackList(),
            grid_available_size == kIndefiniteSize);
}

void NGGridSizingTrackCollection::BuildSets(
    const NGGridTrackList& explicit_track_list,
    const NGGridTrackList& implicit_track_list,
    bool is_available_size_indefinite) {
  properties_.Reset();
  sets_.Shrink(0);

  for (auto& range : ranges_) {
    // Notice that |NGGridRange::Reset| does not reset the |kIsCollapsed| or
    // |kIsImplicit| flags as they're not affected by the set definitions.
    range.properties.Reset();

    // Collapsed ranges don't produce sets as they will be sized to zero anyway.
    if (range.IsCollapsed())
      continue;

    auto CacheSetProperties = [&range](const NGGridSet& set) {
      const auto& set_track_size = set.track_size;

      // From https://drafts.csswg.org/css-grid-2/#algo-terms, a <flex> minimum
      // sizing function shouldn't happen as it would be normalized to 'auto'.
      DCHECK(!set_track_size.HasFlexMinTrackBreadth());

      if (set_track_size.HasAutoMinTrackBreadth())
        range.properties.SetProperty(TrackSpanProperties::kHasAutoMinimumTrack);

      if (set_track_size.HasFixedMinTrackBreadth()) {
        range.properties.SetProperty(
            TrackSpanProperties::kHasFixedMinimumTrack);
      }

      if (set_track_size.HasFixedMaxTrackBreadth()) {
        range.properties.SetProperty(
            TrackSpanProperties::kHasFixedMaximumTrack);
      }

      if (set_track_size.HasFlexMaxTrackBreadth()) {
        range.properties.SetProperty(TrackSpanProperties::kHasFlexibleTrack);
        range.properties.SetProperty(
            TrackSpanProperties::kIsDependentOnAvailableSize);
      }

      if (set_track_size.HasIntrinsicMinTrackBreadth() ||
          set_track_size.HasIntrinsicMaxTrackBreadth()) {
        range.properties.SetProperty(TrackSpanProperties::kHasIntrinsicTrack);
      }

      if (!set_track_size.IsDefinite())
        range.properties.SetProperty(TrackSpanProperties::kHasNonDefiniteTrack);
    };

    if (range.repeater_index == kNotFound) {
      // The only case where a range doesn't have a repeater index is when the
      // range is in the implicit grid and there are no auto track definitions;
      // fill the entire range with a single set of 'auto' tracks.
      DCHECK(range.IsImplicit());
      CacheSetProperties(sets_.emplace_back(range.track_count));
    } else {
      const auto& specified_track_list =
          range.IsImplicit() ? implicit_track_list : explicit_track_list;

      const wtf_size_t current_repeater_size =
          specified_track_list.RepeatSize(range.repeater_index);
      DCHECK_LT(range.repeater_offset, current_repeater_size);

      // The following two variables help compute how many tracks a set element
      // compresses; suppose we want to print the range, we would circle through
      // the repeater's track list, starting at the range's repeater offset,
      // printing every definition until we cover its track count.
      //
      // 1. |floor_set_track_count| is the number of times we would return to
      // the range's repeater offset, meaning that every definition in the
      // repeater's track list appears at least that many times.
      const wtf_size_t floor_set_track_count =
          range.track_count / current_repeater_size;

      // 2. The remaining track count would not complete another iteration over
      // the entire repeater; this means that the first |remaining_track_count|
      // definitions appear one more time in the range.
      const wtf_size_t remaining_track_count =
          range.track_count % current_repeater_size;

      for (wtf_size_t i = 0; i < range.set_count; ++i) {
        const wtf_size_t set_track_count =
            floor_set_track_count + ((i < remaining_track_count) ? 1 : 0);
        const wtf_size_t set_repeater_offset =
            (range.repeater_offset + i) % current_repeater_size;
        const auto& set_track_size = specified_track_list.RepeatTrackSize(
            range.repeater_index, set_repeater_offset);

        // Record if any of the track sizes depend on the available size; we
        // need to record any percentage tracks *before* normalization as they
        // will change to 'auto' if the available size is indefinite.
        if (set_track_size.HasPercentage()) {
          range.properties.SetProperty(
              TrackSpanProperties::kIsDependentOnAvailableSize);
        }

        CacheSetProperties(sets_.emplace_back(set_track_count, set_track_size,
                                              is_available_size_indefinite));
      }
    }
    properties_ |= range.properties;
  }
}

// https://drafts.csswg.org/css-grid-2/#algo-init
void NGGridSizingTrackCollection::InitializeSets(LayoutUnit grid_available_size,
                                                 LayoutUnit gutter_size) {
  gutter_size_ = gutter_size;
  for (auto& set : sets_) {
    const auto& track_size = set.track_size;

    if (track_size.IsFitContent()) {
      // Indefinite lengths cannot occur, as they must be normalized to 'auto'.
      DCHECK(!track_size.FitContentTrackBreadth().HasPercentage() ||
             grid_available_size != kIndefiniteSize);

      LayoutUnit fit_content_argument = MinimumValueForLength(
          track_size.FitContentTrackBreadth().length(), grid_available_size);
      set.fit_content_limit = fit_content_argument * set.track_count;
    }

    if (track_size.HasFixedMaxTrackBreadth()) {
      DCHECK(!track_size.MaxTrackBreadth().HasPercentage() ||
             grid_available_size != kIndefiniteSize);

      // A fixed sizing function: Resolve to an absolute length and use that
      // size as the track’s initial growth limit; if the growth limit is less
      // than the base size, increase the growth limit to match the base size.
      LayoutUnit fixed_max_breadth = MinimumValueForLength(
          track_size.MaxTrackBreadth().length(), grid_available_size);
      set.growth_limit = fixed_max_breadth * set.track_count;
    } else {
      // An intrinsic or flexible sizing function: Use an initial growth limit
      // of infinity.
      set.growth_limit = kIndefiniteSize;
    }

    if (track_size.HasFixedMinTrackBreadth()) {
      DCHECK(!track_size.MinTrackBreadth().HasPercentage() ||
             grid_available_size != kIndefiniteSize);

      // A fixed sizing function: Resolve to an absolute length and use that
      // size as the track’s initial base size.
      LayoutUnit fixed_min_breadth = MinimumValueForLength(
          track_size.MinTrackBreadth().length(), grid_available_size);
      set.InitBaseSize(fixed_min_breadth * set.track_count);
    } else {
      // An intrinsic sizing function: Use an initial base size of zero.
      DCHECK(track_size.HasIntrinsicMinTrackBreadth());
      set.InitBaseSize(LayoutUnit());
    }
  }
}

}  // namespace blink
