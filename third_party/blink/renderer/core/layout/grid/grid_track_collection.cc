// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"

#include "base/check.h"
#include "third_party/blink/renderer/core/layout/grid/grid_line_resolver.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

struct SameSizeAsGridRange {
  wtf_size_t members[6];
  wtf_size_t bitfields;
};

ASSERT_SIZE(GridRange, SameSizeAsGridRange);

wtf_size_t GridTrackCollectionBase::RangeEndLine(wtf_size_t range_index) const {
  return RangeStartLine(range_index) + RangeTrackCount(range_index);
}

wtf_size_t GridTrackCollectionBase::RangeIndexFromGridLine(
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

bool GridRange::IsCollapsed() const {
  return properties.HasProperty(TrackSpanProperties::kIsCollapsed);
}

bool GridRange::IsImplicit() const {
  return properties.HasProperty(TrackSpanProperties::kIsImplicit);
}

void GridRange::SetIsCollapsed() {
  properties.SetProperty(TrackSpanProperties::kIsCollapsed);
}

void GridRange::SetIsImplicit() {
  properties.SetProperty(TrackSpanProperties::kIsImplicit);
}

GridRangeBuilder::GridRangeBuilder(const ComputedStyle& grid_style,
                                   const GridLineResolver& line_resolver,
                                   GridTrackSizingDirection track_direction,
                                   wtf_size_t start_offset)
    : auto_repetitions_(line_resolver.AutoRepetitions(track_direction)),
      start_offset_(start_offset),
      must_sort_grid_lines_(false),
      explicit_tracks_((track_direction == kForColumns)
                           ? grid_style.GridTemplateColumns().track_list
                           : grid_style.GridTemplateRows().track_list),
      implicit_tracks_((track_direction == kForColumns)
                           ? grid_style.GridAutoColumns()
                           : grid_style.GridAutoRows()) {
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

    // Subgrids can have zero auto repetitions.
    if (explicit_tracks_.IsSubgriddedAxis() && repeater_track_count == 0) {
      continue;
    }

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
  wtf_size_t named_grid_area_end_line = start_offset_;
  if (const auto& grid_template_areas = grid_style.GridTemplateAreas()) {
    named_grid_area_end_line += (track_direction == kForColumns)
                                    ? grid_template_areas->column_count
                                    : grid_template_areas->row_count;
  }

  if (current_repeater_start_line < named_grid_area_end_line) {
    start_lines_.emplace_back(current_repeater_start_line);
    end_lines_.emplace_back(named_grid_area_end_line);
  }
}

void GridRangeBuilder::EnsureTrackCoverage(
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

GridRangeVector GridRangeBuilder::FinalizeRanges() {
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

  GridRangeVector ranges;
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
    GridRange range;
    wtf_size_t current_repeater_size = 1;
    range.start_line = current_range_start_line;
    range.track_count =
        std::min(next_start_line, next_end_line) - current_range_start_line;
    DCHECK_GT(range.track_count, 0u);

    // Compute current repeater's index, size, and offset.
    // TODO(ethavar): Simplify this logic.
    range.begin_set_index = current_set_index;
    if (explicit_tracks_.IsSubgriddedAxis()) {
      // Subgridded axis specified on standalone grid, use 'auto'.
      range.repeater_index = kNotFound;
      range.repeater_offset = 0u;
    } else if (current_explicit_repeater_index != kNotFound) {
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

GridRangeBuilder::GridRangeBuilder(const NGGridTrackList& explicit_tracks,
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

GridSet::GridSet(wtf_size_t track_count,
                 const GridTrackSize& track_definition,
                 bool is_available_size_indefinite)
    : track_count(track_count),
      track_size(track_definition),
      fit_content_limit(kIndefiniteSize) {
  if (track_size.IsFitContent()) {
    // Argument for 'fit-content' is a <percentage> that couldn't be resolved to
    // a definite <length>, normalize to 'minmax(auto, max-content)'.
    if (is_available_size_indefinite &&
        track_size.FitContentTrackBreadth().HasPercent()) {
      track_size = GridTrackSize(Length::Auto(), Length::MaxContent());
    }
  } else {
    // Normalize |track_size| into a |kMinMaxTrackSizing| type; follow the
    // definitions from https://drafts.csswg.org/css-grid-2/#algo-terms.
    const auto normalized_min_track_sizing_function =
        ((is_available_size_indefinite &&
          track_size.MinTrackBreadth().HasPercent()) ||
         track_size.HasFlexMinTrackBreadth())
            ? Length::Auto()
            : track_size.MinTrackBreadth();

    const auto normalized_max_track_sizing_function =
        (is_available_size_indefinite &&
         track_size.MaxTrackBreadth().HasPercent())
            ? Length::Auto()
            : track_size.MaxTrackBreadth();

    track_size = GridTrackSize(normalized_min_track_sizing_function,
                               normalized_max_track_sizing_function);
  }
  DCHECK(track_size.GetType() == kFitContentTrackSizing ||
         track_size.GetType() == kMinMaxTrackSizing);
}

float GridSet::FlexFactor() const {
  DCHECK(track_size.HasFlexMaxTrackBreadth());
  return track_size.MaxTrackBreadth().GetFloatValue() * track_count;
}

LayoutUnit GridSet::BaseSize() const {
  DCHECK(!IsGrowthLimitLessThanBaseSize());
  return base_size;
}

LayoutUnit GridSet::GrowthLimit() const {
  DCHECK(!IsGrowthLimitLessThanBaseSize());
  return growth_limit;
}

void GridSet::InitBaseSize(LayoutUnit new_base_size) {
  DCHECK_NE(new_base_size, kIndefiniteSize);
  base_size = new_base_size;
  EnsureGrowthLimitIsNotLessThanBaseSize();
}

void GridSet::IncreaseBaseSize(LayoutUnit new_base_size) {
  // Expect base size to always grow monotonically.
  DCHECK_NE(new_base_size, kIndefiniteSize);
  DCHECK_LE(base_size, new_base_size);
  base_size = new_base_size;
  EnsureGrowthLimitIsNotLessThanBaseSize();
}

void GridSet::IncreaseGrowthLimit(LayoutUnit new_growth_limit) {
  // Growth limit is initialized as infinity; expect it to change from infinity
  // to a definite value and then to always grow monotonically.
  DCHECK_NE(new_growth_limit, kIndefiniteSize);
  DCHECK(!IsGrowthLimitLessThanBaseSize() &&
         (growth_limit == kIndefiniteSize || growth_limit <= new_growth_limit));
  growth_limit = new_growth_limit;
}

void GridSet::EnsureGrowthLimitIsNotLessThanBaseSize() {
  if (IsGrowthLimitLessThanBaseSize())
    growth_limit = base_size;
}

bool GridSet::IsGrowthLimitLessThanBaseSize() const {
  return growth_limit != kIndefiniteSize && growth_limit < base_size;
}

bool GridLayoutTrackCollection::operator==(
    const GridLayoutTrackCollection& other) const {
  return gutter_size_ == other.gutter_size_ &&
         track_direction_ == other.track_direction_ &&
         accumulated_gutter_size_delta_ ==
             other.accumulated_gutter_size_delta_ &&
         accumulated_start_extra_margin_ ==
             other.accumulated_start_extra_margin_ &&
         accumulated_end_extra_margin_ == other.accumulated_end_extra_margin_ &&
         baselines_.has_value() == other.baselines_.has_value() &&
         (!baselines_ || (baselines_->major == other.baselines_->major &&
                          baselines_->minor == other.baselines_->minor)) &&
         last_indefinite_index_ == other.last_indefinite_index_ &&
         ranges_ == other.ranges_ && sets_geometry_ == other.sets_geometry_;
}

wtf_size_t GridLayoutTrackCollection::RangeStartLine(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index].start_line;
}

wtf_size_t GridLayoutTrackCollection::RangeTrackCount(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index].track_count;
}

wtf_size_t GridLayoutTrackCollection::RangeSetCount(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index].set_count;
}

wtf_size_t GridLayoutTrackCollection::RangeBeginSetIndex(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index].begin_set_index;
}

TrackSpanProperties GridLayoutTrackCollection::RangeProperties(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index].properties;
}

wtf_size_t GridLayoutTrackCollection::EndLineOfImplicitGrid() const {
  if (ranges_.empty())
    return 0;
  const auto& last_range = ranges_.back();
  return last_range.start_line + last_range.track_count;
}

bool GridLayoutTrackCollection::IsGridLineWithinImplicitGrid(
    wtf_size_t grid_line) const {
  DCHECK_NE(grid_line, kNotFound);
  return grid_line <= EndLineOfImplicitGrid();
}

wtf_size_t GridLayoutTrackCollection::GetSetCount() const {
  if (ranges_.empty())
    return 0;
  const auto& last_range = ranges_.back();
  return last_range.begin_set_index + last_range.set_count;
}

LayoutUnit GridLayoutTrackCollection::GetSetOffset(wtf_size_t set_index) const {
  DCHECK_LT(set_index, sets_geometry_.size());
  return sets_geometry_[set_index].offset;
}

wtf_size_t GridLayoutTrackCollection::GetSetTrackCount(
    wtf_size_t set_index) const {
  DCHECK_LT(set_index + 1, sets_geometry_.size());
  return sets_geometry_[set_index + 1].track_count;
}

LayoutUnit GridLayoutTrackCollection::StartExtraMargin(
    wtf_size_t set_index) const {
  return set_index ? accumulated_gutter_size_delta_ / 2
                   : accumulated_start_extra_margin_;
}

LayoutUnit GridLayoutTrackCollection::EndExtraMargin(
    wtf_size_t set_index) const {
  return (set_index < sets_geometry_.size() - 1)
             ? accumulated_gutter_size_delta_ / 2
             : accumulated_end_extra_margin_;
}

LayoutUnit GridLayoutTrackCollection::MajorBaseline(
    wtf_size_t set_index) const {
  if (!baselines_) {
    return LayoutUnit::Min();
  }

  DCHECK_LT(set_index, baselines_->major.size());
  return baselines_->major[set_index];
}

LayoutUnit GridLayoutTrackCollection::MinorBaseline(
    wtf_size_t set_index) const {
  if (!baselines_) {
    return LayoutUnit::Min();
  }

  DCHECK_LT(set_index, baselines_->minor.size());
  return baselines_->minor[set_index];
}

void GridLayoutTrackCollection::AdjustSetOffsets(wtf_size_t set_index,
                                                 LayoutUnit delta) {
  DCHECK_LT(set_index, sets_geometry_.size());
  for (wtf_size_t i = set_index; i < sets_geometry_.size(); ++i)
    sets_geometry_[i].offset += delta;
}

LayoutUnit GridLayoutTrackCollection::ComputeSetSpanSize() const {
  return ComputeSetSpanSize(0, GetSetCount());
}

LayoutUnit GridLayoutTrackCollection::ComputeSetSpanSize(
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

bool GridLayoutTrackCollection::IsSpanningIndefiniteSet(
    wtf_size_t begin_set_index,
    wtf_size_t end_set_index) const {
  if (last_indefinite_index_.empty()) {
    return false;
  }

  DCHECK_LT(begin_set_index, end_set_index);
  DCHECK_LT(end_set_index, last_indefinite_index_.size());
  const wtf_size_t last_indefinite_index =
      last_indefinite_index_[end_set_index];

  return last_indefinite_index != kNotFound &&
         begin_set_index <= last_indefinite_index;
}

GridLayoutTrackCollection
GridLayoutTrackCollection::CreateSubgridTrackCollection(
    wtf_size_t begin_range_index,
    wtf_size_t end_range_index,
    LayoutUnit subgrid_gutter_size,
    const BoxStrut& subgrid_margin,
    const BoxStrut& subgrid_border_scrollbar_padding,
    GridTrackSizingDirection subgrid_track_direction,
    bool is_opposite_direction_in_root_grid) const {
  DCHECK_LE(begin_range_index, end_range_index);
  DCHECK_LT(end_range_index, ranges_.size());

  GridLayoutTrackCollection subgrid_track_collection(subgrid_track_direction);

  const wtf_size_t begin_set_index = ranges_[begin_range_index].begin_set_index;
  const wtf_size_t end_set_index = ranges_[end_range_index].begin_set_index +
                                   ranges_[end_range_index].set_count;

  DCHECK_LT(end_set_index, sets_geometry_.size());
  DCHECK_LT(begin_set_index, end_set_index);

  // Copy and translate the ranges in the subgrid's span.
  {
    auto& subgrid_properties = subgrid_track_collection.properties_;
    auto& subgrid_ranges = subgrid_track_collection.ranges_;

    const wtf_size_t range_count = end_range_index - begin_range_index;
    wtf_size_t current_begin_set_index = 0;
    wtf_size_t current_start_line = 0;

    subgrid_ranges.ReserveInitialCapacity(range_count + 1);

    for (wtf_size_t i = 0; i <= range_count; ++i) {
      // Opposite direction subgrids need to iterate backwards.
      const wtf_size_t current_index = is_opposite_direction_in_root_grid
                                           ? end_range_index - i
                                           : begin_range_index + i;

      auto& subgrid_translated_range =
          subgrid_ranges.emplace_back(ranges_[current_index]);
      subgrid_translated_range.begin_set_index = current_begin_set_index;
      current_begin_set_index += subgrid_translated_range.set_count;

      subgrid_translated_range.start_line = current_start_line;
      current_start_line += subgrid_translated_range.track_count;

      subgrid_properties |= subgrid_translated_range.properties;
    }
  }

  const wtf_size_t set_span_size = end_set_index - begin_set_index;

  // Copy the sets geometry and adjust its offsets to accommodate the subgrid's
  // margin, border, scrollbar, padding, and gutter size.
  const auto subgrid_gutter_size_delta = subgrid_gutter_size - gutter_size_;

  const bool is_for_columns = subgrid_track_direction == kForColumns;
  const auto subgrid_margin_start =
      is_for_columns ? subgrid_margin.inline_start : subgrid_margin.block_start;

  const auto subgrid_border_scrollbar_padding_start =
      is_for_columns ? subgrid_border_scrollbar_padding.inline_start
                     : subgrid_border_scrollbar_padding.block_start;

  const auto subgrid_margin_border_scrollbar_padding_start =
      subgrid_margin_start + subgrid_border_scrollbar_padding_start;
  const auto subgrid_margin_border_scrollbar_padding_end =
      is_for_columns ? subgrid_margin.inline_end +
                           subgrid_border_scrollbar_padding.inline_end
                     : subgrid_margin.block_end +
                           subgrid_border_scrollbar_padding.block_end;

  // Accumulate the extra margin from the spanned sets in the parent track
  // collection and this subgrid's margins and gutter size delta.
  {
    subgrid_track_collection.accumulated_gutter_size_delta_ =
        subgrid_gutter_size_delta + accumulated_gutter_size_delta_;

    auto& subgrid_sets_geometry = subgrid_track_collection.sets_geometry_;
    subgrid_sets_geometry.ReserveInitialCapacity(set_span_size + 1);
    subgrid_sets_geometry.emplace_back(
        /* offset */ subgrid_border_scrollbar_padding_start);

    // Opposite direction subgrids adjust extra margin from the opposite side.
    subgrid_track_collection.accumulated_start_extra_margin_ =
        subgrid_margin_border_scrollbar_padding_start +
        (is_opposite_direction_in_root_grid
             ? EndExtraMargin(end_set_index)
             : StartExtraMargin(begin_set_index));

    subgrid_track_collection.accumulated_end_extra_margin_ =
        subgrid_margin_border_scrollbar_padding_end +
        (is_opposite_direction_in_root_grid ? StartExtraMargin(begin_set_index)
                                            : EndExtraMargin(end_set_index));

    // Opposite direction subgrids iterate backwards.
    const wtf_size_t first_set_index =
        is_opposite_direction_in_root_grid ? end_set_index : begin_set_index;
    LayoutUnit first_set_offset = sets_geometry_[first_set_index].offset;

    if (is_opposite_direction_in_root_grid) {
      first_set_offset -= subgrid_margin_start;
    } else {
      first_set_offset += subgrid_margin_start;
    }

    for (wtf_size_t i = 1; i < set_span_size; ++i) {
      // Opposite direction subgrids need to iterate backwards.
      const wtf_size_t current_index = is_opposite_direction_in_root_grid
                                           ? end_set_index - i
                                           : begin_set_index + i;
      auto& set =
          subgrid_sets_geometry.emplace_back(sets_geometry_[current_index]);
      if (is_opposite_direction_in_root_grid) {
        set.offset = first_set_offset - set.offset;

        // Opposite direction subgrids take their offset from the current index,
        // but their track counts from the subsequent index.
        const wtf_size_t next_index = current_index + 1;
        DCHECK_LT(next_index, sets_geometry_.size());
        set.track_count = sets_geometry_[next_index].track_count;
      } else {
        set.offset -= first_set_offset;
      }
      DCHECK_GT(set.track_count, 0U);
      set.offset += subgrid_gutter_size_delta / 2;
    }
    const wtf_size_t last_set_index =
        is_opposite_direction_in_root_grid ? begin_set_index : end_set_index;
    auto& last_set =
        subgrid_sets_geometry.emplace_back(sets_geometry_[last_set_index]);

    if (is_opposite_direction_in_root_grid) {
      last_set.offset = first_set_offset - last_set.offset;
      // Opposite direction subgrids take their offset from the current index,
      // but their track counts from the subsequent index.
      const wtf_size_t next_index = last_set_index + 1;
      DCHECK_LT(next_index, sets_geometry_.size());
      last_set.track_count = sets_geometry_[next_index].track_count;
    } else {
      last_set.offset -= first_set_offset;
    }
    last_set.offset +=
        subgrid_gutter_size_delta - subgrid_margin_border_scrollbar_padding_end;
    DCHECK_GT(last_set.track_count, 0U);
  }

  // Copy the last indefinite indices in the subgrid's span.
  if (!last_indefinite_index_.empty()) {
    auto& subgrid_last_indefinite_index =
        subgrid_track_collection.last_indefinite_index_;

    subgrid_last_indefinite_index.ReserveInitialCapacity(set_span_size + 1);
    subgrid_last_indefinite_index.push_back(kNotFound);

    wtf_size_t last_indefinite_index = kNotFound;
    for (wtf_size_t i = 0; i < set_span_size; ++i) {
      // Opposite direction subgrids need to iterate backwards.
      const wtf_size_t current_index = is_opposite_direction_in_root_grid
                                           ? end_set_index - i - 1
                                           : begin_set_index + i;

      DCHECK_LT(current_index + 1, last_indefinite_index_.size());

      // Map the last indefinite index from the parent track collection by
      // looking for a change in subsequent entries.
      if (last_indefinite_index_[current_index + 1] !=
          last_indefinite_index_[current_index]) {
        last_indefinite_index = i;
      }
      subgrid_last_indefinite_index.push_back(last_indefinite_index);
    }
  }

  // Copy the major and minor baselines in the subgrid's span.
  if (baselines_ && !baselines_->major.empty()) {
    DCHECK_LE(end_set_index, baselines_->major.size());
    DCHECK_LE(end_set_index, baselines_->minor.size());

    Baselines subgrid_baselines;
    subgrid_baselines.major.ReserveInitialCapacity(set_span_size);
    subgrid_baselines.minor.ReserveInitialCapacity(set_span_size);

    // Adjust the baselines to accommodate the subgrid extra margins.
    for (wtf_size_t i = 0; i < set_span_size; ++i) {
      LayoutUnit major_adjust =
          (i == 0) ? subgrid_margin_border_scrollbar_padding_start
                   : subgrid_gutter_size_delta / 2;
      LayoutUnit minor_adjust =
          (i == set_span_size - 1) ? subgrid_margin_border_scrollbar_padding_end
                                   : subgrid_gutter_size_delta / 2;
      if (is_opposite_direction_in_root_grid) {
        std::swap(major_adjust, minor_adjust);
      }
      const wtf_size_t current_index = is_opposite_direction_in_root_grid
                                           ? end_set_index - i - 1
                                           : begin_set_index + i;
      subgrid_baselines.major.emplace_back(baselines_->major[current_index] -
                                           major_adjust);
      subgrid_baselines.minor.emplace_back(baselines_->minor[current_index] -
                                           minor_adjust);
    }

    if (is_opposite_direction_in_root_grid) {
      std::swap(subgrid_baselines.major, subgrid_baselines.minor);
    }

    subgrid_track_collection.baselines_.emplace(std::move(subgrid_baselines));
  }

  subgrid_track_collection.gutter_size_ = subgrid_gutter_size;
  return subgrid_track_collection;
}

bool GridLayoutTrackCollection::HasFlexibleTrack() const {
  return properties_.HasProperty(TrackSpanProperties::kHasFlexibleTrack);
}

bool GridLayoutTrackCollection::HasIntrinsicTrack() const {
  return properties_.HasProperty(TrackSpanProperties::kHasIntrinsicTrack);
}

bool GridLayoutTrackCollection::HasNonDefiniteTrack() const {
  return properties_.HasProperty(TrackSpanProperties::kHasNonDefiniteTrack);
}

bool GridLayoutTrackCollection::IsDependentOnAvailableSize() const {
  return properties_.HasProperty(
      TrackSpanProperties::kIsDependentOnAvailableSize);
}

bool GridLayoutTrackCollection::HasIndefiniteSet() const {
  return !last_indefinite_index_.empty() &&
         last_indefinite_index_.back() != kNotFound;
}

GridSizingTrackCollection::GridSizingTrackCollection(
    GridRangeVector&& ranges,
    bool must_create_baselines,
    GridTrackSizingDirection track_direction)
    : GridLayoutTrackCollection(track_direction) {
  ranges_ = std::move(ranges);

  if (must_create_baselines) {
    baselines_.emplace();
  }

  wtf_size_t set_count = 0;
  for (const auto& range : ranges_) {
    if (!range.IsCollapsed()) {
      non_collapsed_track_count_ += range.track_count;
      set_count += range.set_count;
    }
  }

  last_indefinite_index_.ReserveInitialCapacity(set_count + 1);
  sets_geometry_.ReserveInitialCapacity(set_count + 1);
  sets_.ReserveInitialCapacity(set_count);
}

GridSet& GridSizingTrackCollection::GetSetAt(wtf_size_t set_index) {
  DCHECK_LT(set_index, sets_.size());
  return sets_[set_index];
}

const GridSet& GridSizingTrackCollection::GetSetAt(wtf_size_t set_index) const {
  DCHECK_LT(set_index, sets_.size());
  return sets_[set_index];
}

GridSizingTrackCollection::SetIterator
GridSizingTrackCollection::GetSetIterator() {
  return SetIterator(this, 0, sets_.size());
}

GridSizingTrackCollection::ConstSetIterator
GridSizingTrackCollection::GetConstSetIterator() const {
  return ConstSetIterator(this, 0, sets_.size());
}

GridSizingTrackCollection::SetIterator
GridSizingTrackCollection::GetSetIterator(wtf_size_t begin_set_index,
                                          wtf_size_t end_set_index) {
  return SetIterator(this, begin_set_index, end_set_index);
}

LayoutUnit GridSizingTrackCollection::TotalTrackSize() const {
  if (sets_.empty())
    return LayoutUnit();

  LayoutUnit total_track_size;
  for (const auto& set : sets_)
    total_track_size += set.BaseSize() + set.track_count * gutter_size_;
  return total_track_size - gutter_size_;
}

void GridSizingTrackCollection::CacheDefiniteSetsGeometry() {
  DCHECK(sets_geometry_.empty() && last_indefinite_index_.empty());

  LayoutUnit first_set_offset;
  last_indefinite_index_.push_back(kNotFound);
  sets_geometry_.emplace_back(first_set_offset);

  for (const auto& set : sets_) {
    if (set.track_size.IsDefinite()) {
      first_set_offset += set.base_size + gutter_size_ * set.track_count;
      last_indefinite_index_.push_back(last_indefinite_index_.back());
    } else {
      last_indefinite_index_.push_back(last_indefinite_index_.size() - 1);
    }

    DCHECK_LE(sets_geometry_.back().offset, first_set_offset);
    sets_geometry_.emplace_back(first_set_offset, set.track_count);
  }
}

void GridSizingTrackCollection::CacheInitializedSetsGeometry(
    LayoutUnit first_set_offset) {
  last_indefinite_index_.Shrink(0);
  sets_geometry_.Shrink(0);

  last_indefinite_index_.push_back(kNotFound);
  sets_geometry_.emplace_back(first_set_offset);

  for (const auto& set : sets_) {
    if (set.growth_limit == kIndefiniteSize) {
      last_indefinite_index_.push_back(last_indefinite_index_.size() - 1);
    } else {
      first_set_offset += set.growth_limit + gutter_size_ * set.track_count;
      last_indefinite_index_.push_back(last_indefinite_index_.back());
    }

    DCHECK_LE(sets_geometry_.back().offset, first_set_offset);
    sets_geometry_.emplace_back(first_set_offset, set.track_count);
  }
}

void GridSizingTrackCollection::FinalizeSetsGeometry(
    LayoutUnit first_set_offset,
    LayoutUnit override_gutter_size) {
  gutter_size_ = override_gutter_size;

  last_indefinite_index_.Shrink(0);
  sets_geometry_.Shrink(0);

  sets_geometry_.emplace_back(first_set_offset);

  for (const auto& set : sets_) {
    first_set_offset += set.BaseSize() + gutter_size_ * set.track_count;
    DCHECK_LE(sets_geometry_.back().offset, first_set_offset);
    sets_geometry_.emplace_back(first_set_offset, set.track_count);
  }
}

void GridSizingTrackCollection::SetIndefiniteGrowthLimitsToBaseSize() {
  for (auto& set : sets_) {
    if (set.GrowthLimit() == kIndefiniteSize)
      set.growth_limit = set.base_size;
  }
}

void GridSizingTrackCollection::ResetBaselines() {
  DCHECK(baselines_);

  const wtf_size_t set_count = sets_.size();
  baselines_->major = Vector<LayoutUnit, 16>(set_count, LayoutUnit::Min());
  baselines_->minor = Vector<LayoutUnit, 16>(set_count, LayoutUnit::Min());
}

void GridSizingTrackCollection::SetMajorBaseline(
    wtf_size_t set_index,
    LayoutUnit candidate_baseline) {
  DCHECK(baselines_ && set_index < baselines_->major.size());
  if (candidate_baseline > baselines_->major[set_index])
    baselines_->major[set_index] = candidate_baseline;
}

void GridSizingTrackCollection::SetMinorBaseline(
    wtf_size_t set_index,
    LayoutUnit candidate_baseline) {
  DCHECK(baselines_ && set_index < baselines_->minor.size());
  if (candidate_baseline > baselines_->minor[set_index])
    baselines_->minor[set_index] = candidate_baseline;
}

void GridSizingTrackCollection::BuildSets(const ComputedStyle& grid_style,
                                          LayoutUnit grid_available_size,
                                          LayoutUnit gutter_size) {
  const bool is_for_columns = track_direction_ == kForColumns;
  gutter_size_ = gutter_size;

  BuildSets(
      is_for_columns ? grid_style.GridTemplateColumns().track_list
                     : grid_style.GridTemplateRows().track_list,
      is_for_columns ? grid_style.GridAutoColumns() : grid_style.GridAutoRows(),
      grid_available_size == kIndefiniteSize);
  InitializeSets(grid_available_size);
}

void GridSizingTrackCollection::BuildSets(
    const NGGridTrackList& explicit_track_list,
    const NGGridTrackList& implicit_track_list,
    bool is_available_size_indefinite) {
  properties_.Reset();
  sets_.Shrink(0);

  for (auto& range : ranges_) {
    // Notice that |GridRange::Reset| does not reset the |kIsCollapsed| or
    // |kIsImplicit| flags as they're not affected by the set definitions.
    range.properties.Reset();

    // Collapsed ranges don't produce sets as they will be sized to zero anyway.
    if (range.IsCollapsed())
      continue;

    auto CacheSetProperties = [&range](const GridSet& set) {
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
      // The only cases where a range doesn't have a repeater index are when the
      // range is in the implicit grid and there are no auto track definitions,
      // or when 'subgrid' is specified on a track definition but it's not a
      // child of a grid (and thus not a subgrid); in both cases, fill the
      // entire range with a single set of 'auto' tracks.
      DCHECK(range.IsImplicit() || explicit_track_list.IsSubgriddedAxis());
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
void GridSizingTrackCollection::InitializeSets(LayoutUnit grid_available_size) {
  for (auto& set : sets_) {
    const auto& track_size = set.track_size;

    if (track_size.IsFitContent()) {
      // Indefinite lengths cannot occur, as they must be normalized to 'auto'.
      DCHECK(!track_size.FitContentTrackBreadth().HasPercent() ||
             grid_available_size != kIndefiniteSize);

      LayoutUnit fit_content_argument = MinimumValueForLength(
          track_size.FitContentTrackBreadth(), grid_available_size);
      set.fit_content_limit = fit_content_argument * set.track_count;
    }

    if (track_size.HasFixedMaxTrackBreadth()) {
      DCHECK(!track_size.MaxTrackBreadth().HasPercent() ||
             grid_available_size != kIndefiniteSize);

      // A fixed sizing function: Resolve to an absolute length and use that
      // size as the track’s initial growth limit; if the growth limit is less
      // than the base size, increase the growth limit to match the base size.
      LayoutUnit fixed_max_breadth = MinimumValueForLength(
          track_size.MaxTrackBreadth(), grid_available_size);
      set.growth_limit = fixed_max_breadth * set.track_count;
    } else {
      // An intrinsic or flexible sizing function: Use an initial growth limit
      // of infinity.
      set.growth_limit = kIndefiniteSize;
    }

    if (track_size.HasFixedMinTrackBreadth()) {
      DCHECK(!track_size.MinTrackBreadth().HasPercent() ||
             grid_available_size != kIndefiniteSize);

      // A fixed sizing function: Resolve to an absolute length and use that
      // size as the track’s initial base size.
      LayoutUnit fixed_min_breadth = MinimumValueForLength(
          track_size.MinTrackBreadth(), grid_available_size);
      set.InitBaseSize(fixed_min_breadth * set.track_count);
    } else {
      // An intrinsic sizing function: Use an initial base size of zero.
      DCHECK(track_size.HasIntrinsicMinTrackBreadth());
      set.InitBaseSize(LayoutUnit());
    }
  }
}

}  // namespace blink
