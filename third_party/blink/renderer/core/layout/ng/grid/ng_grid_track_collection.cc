// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_track_collection.h"

#include "base/check.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

constexpr wtf_size_t NGGridTrackCollectionBase::kInvalidRangeIndex;

wtf_size_t NGGridTrackCollectionBase::RangeIndexFromTrackNumber(
    wtf_size_t track_number) const {
  wtf_size_t upper = RangeCount();
  wtf_size_t lower = 0u;

  // We can't look for a range in a collection with no ranges.
  DCHECK_NE(upper, 0u);
  // We don't expect a |track_number| outside of the bounds of the collection.
  DCHECK_LT(track_number,
            RangeTrackNumber(upper - 1) + RangeTrackCount(upper - 1));

  // Do a binary search on the tracks.
  wtf_size_t range = upper - lower;
  while (range > 1) {
    wtf_size_t center = lower + (range / 2u);

    wtf_size_t center_track_number = RangeTrackNumber(center);
    wtf_size_t center_track_count = RangeTrackCount(center);

    if (center_track_number <= track_number &&
        (track_number - center_track_number) < center_track_count) {
      // We found the track.
      return center;
    } else if (center_track_number > track_number) {
      // This track is too high.
      upper = center;
      range = upper - lower;
    } else {
      // This track is too low.
      lower = center + 1u;
      range = upper - lower;
    }
  }
  return lower;
}

NGGridTrackCollectionBase::RangeRepeatIterator
NGGridTrackCollectionBase::RangeIterator() const {
  return RangeRepeatIterator(this, 0u);
}

String NGGridTrackCollectionBase::ToString() const {
  if (RangeCount() == kInvalidRangeIndex)
    return "NGGridTrackCollection: Empty";

  StringBuilder builder;
  builder.Append("NGGridTrackCollection: [RangeCount: ");
  builder.AppendNumber<wtf_size_t>(RangeCount());
  builder.Append("], Ranges: ");
  for (wtf_size_t range_index = 0; range_index < RangeCount(); ++range_index) {
    builder.Append("[Start: ");
    builder.AppendNumber<wtf_size_t>(RangeTrackNumber(range_index));
    builder.Append(", Count: ");
    builder.AppendNumber<wtf_size_t>(RangeTrackCount(range_index));
    if (IsRangeCollapsed(range_index)) {
      builder.Append(", Collapsed ");
    }
    builder.Append("]");
    if (range_index + 1 < RangeCount())
      builder.Append(", ");
  }
  return builder.ToString();
}

NGGridTrackCollectionBase::RangeRepeatIterator::RangeRepeatIterator(
    const NGGridTrackCollectionBase* collection,
    wtf_size_t range_index)
    : collection_(collection) {
  DCHECK(collection_);
  range_count_ = collection_->RangeCount();
  SetRangeIndex(range_index);
}

bool NGGridTrackCollectionBase::RangeRepeatIterator::IsAtEnd() const {
  return range_index_ == kInvalidRangeIndex;
}

bool NGGridTrackCollectionBase::RangeRepeatIterator::MoveToNextRange() {
  return SetRangeIndex(range_index_ + 1);
}

wtf_size_t NGGridTrackCollectionBase::RangeRepeatIterator::RepeatCount() const {
  return range_track_count_;
}

wtf_size_t NGGridTrackCollectionBase::RangeRepeatIterator::RangeIndex() const {
  return range_index_;
}

wtf_size_t NGGridTrackCollectionBase::RangeRepeatIterator::RangeTrackStart()
    const {
  return range_track_start_;
}

wtf_size_t NGGridTrackCollectionBase::RangeRepeatIterator::RangeTrackEnd()
    const {
  if (range_index_ == kInvalidRangeIndex)
    return kInvalidRangeIndex;
  return range_track_start_ + range_track_count_ - 1;
}

bool NGGridTrackCollectionBase::RangeRepeatIterator::IsRangeCollapsed() const {
  DCHECK(collection_);
  DCHECK_NE(range_index_, kInvalidRangeIndex);
  return collection_->IsRangeCollapsed(range_index_);
}

bool NGGridTrackCollectionBase::RangeRepeatIterator::SetRangeIndex(
    wtf_size_t range_index) {
  if (range_index >= range_count_) {
    // Invalid index.
    range_index_ = kInvalidRangeIndex;
    range_track_start_ = kInvalidRangeIndex;
    range_track_count_ = 0;
    return false;
  }

  range_index_ = range_index;
  range_track_start_ = collection_->RangeTrackNumber(range_index_);
  range_track_count_ = collection_->RangeTrackCount(range_index_);
  return true;
}

bool NGGridBlockTrackCollection::Range::IsImplicit() const {
  return properties.HasProperty(TrackSpanProperties::kIsImplicit);
}

bool NGGridBlockTrackCollection::Range::IsCollapsed() const {
  return properties.HasProperty(TrackSpanProperties::kIsCollapsed);
}

void NGGridBlockTrackCollection::Range::SetIsImplicit() {
  properties.SetProperty(TrackSpanProperties::kIsImplicit);
}

void NGGridBlockTrackCollection::Range::SetIsCollapsed() {
  properties.SetProperty(TrackSpanProperties::kIsCollapsed);
}

void NGGridBlockTrackCollection::SetSpecifiedTracks(
    const NGGridTrackList* explicit_tracks,
    const NGGridTrackList* implicit_tracks,
    const wtf_size_t start_offset,
    const wtf_size_t auto_repetitions,
    const wtf_size_t named_grid_area_track_count) {
  DCHECK(explicit_tracks && implicit_tracks);

  // The implicit track list should have only one repeater, if any.
  DCHECK_LE(implicit_tracks->RepeaterCount(), 1u);
  DCHECK_NE(kInvalidRangeIndex, auto_repetitions);

  explicit_tracks_ = explicit_tracks;
  implicit_tracks_ = implicit_tracks;
  auto_repetitions_ = auto_repetitions;

  const wtf_size_t repeater_count = explicit_tracks_->RepeaterCount();
  wtf_size_t current_repeater_start_line = start_offset;
  // Add extra capacity for the extra lines needed for named grids.
  start_lines_.ReserveCapacity(repeater_count + 1);
  end_lines_.ReserveCapacity(repeater_count + 1);
  for (wtf_size_t i = 0; i < repeater_count; ++i) {
    wtf_size_t repeater_track_count =
        explicit_tracks_->RepeatCount(i, auto_repetitions_) *
        explicit_tracks_->RepeatSize(i);
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
      start_offset + named_grid_area_track_count;
  if (current_repeater_start_line < named_grid_area_end_line) {
    start_lines_.emplace_back(current_repeater_start_line);
    end_lines_.emplace_back(named_grid_area_end_line);
  }
}

void NGGridBlockTrackCollection::EnsureTrackCoverage(
    wtf_size_t track_number,
    wtf_size_t span_length,
    wtf_size_t* grid_item_start_range_index,
    wtf_size_t* grid_item_end_range_index) {
  DCHECK_NE(kInvalidRangeIndex, track_number);
  DCHECK_NE(kInvalidRangeIndex, span_length);
  DCHECK(grid_item_start_range_index && grid_item_end_range_index);

  track_indices_need_sort_ = true;
  start_lines_.emplace_back(track_number, grid_item_start_range_index);
  end_lines_.emplace_back(track_number + span_length,
                          grid_item_end_range_index);
}

void NGGridBlockTrackCollection::FinalizeRanges(wtf_size_t start_offset) {
  // Sort start and ending tracks from low to high.
  if (track_indices_need_sort_) {
    auto CompareTrackBoundaries = [](const TrackBoundaryToRangePair& a,
                                     const TrackBoundaryToRangePair& b) {
      return a.grid_line < b.grid_line;
    };
    std::sort(start_lines_.begin(), start_lines_.end(), CompareTrackBoundaries);
    std::sort(end_lines_.begin(), end_lines_.end(), CompareTrackBoundaries);
  }
  ranges_.clear();

  bool is_in_auto_fit_range = false;
  wtf_size_t current_range_start_line = 0u;
  wtf_size_t open_items_or_repeaters = 0u;
  wtf_size_t current_explicit_grid_line = start_offset;
  wtf_size_t current_explicit_repeater_index = kInvalidRangeIndex;
  wtf_size_t explicit_repeater_count = explicit_tracks_->RepeaterCount();

  // If the explicit grid is not empty, |start_offset| is the translated index
  // of the first track in |explicit_tracks_|; otherwise, the next repeater
  // does not exist, fallback to |kInvalidRangeIndex|.
  wtf_size_t next_explicit_repeater_start =
      explicit_repeater_count ? start_offset : kInvalidRangeIndex;

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
      if (current_explicit_repeater_index != kInvalidRangeIndex) {
        DCHECK_EQ(current_explicit_repeater_index, explicit_repeater_count - 1);
        DCHECK_EQ(current_range_start_line, next_explicit_repeater_start);
      }
#endif
      break;
    }

    // Determine the next starting and ending track index.
    wtf_size_t next_start_line = (start_line_index < start_lines_.size())
                                     ? start_lines_[start_line_index].grid_line
                                     : kInvalidRangeIndex;
    wtf_size_t next_end_line = end_lines_[end_line_index].grid_line;

    // Move to the start of the next explicit repeater.
    while (current_range_start_line == next_explicit_repeater_start) {
      current_explicit_grid_line = next_explicit_repeater_start;

      // No next repeater, break and use implicit grid tracks.
      if (++current_explicit_repeater_index == explicit_repeater_count) {
        current_explicit_repeater_index = kInvalidRangeIndex;
        is_in_auto_fit_range = false;
        break;
      }

      is_in_auto_fit_range =
          explicit_tracks_->RepeatType(current_explicit_repeater_index) ==
          NGGridTrackRepeater::RepeatType::kAutoFit;
      next_explicit_repeater_start +=
          explicit_tracks_->RepeatSize(current_explicit_repeater_index) *
          explicit_tracks_->RepeatCount(current_explicit_repeater_index,
                                        auto_repetitions_);
    }

    // Determine track number and count of the range.
    Range range;
    range.start_line = current_range_start_line;
    DCHECK(next_start_line != kInvalidRangeIndex ||
           next_end_line < next_start_line);
    range.track_count =
        std::min(next_start_line, next_end_line) - current_range_start_line;
    DCHECK_GT(range.track_count, 0u);

    // Compute repeater index and offset.
    if (current_explicit_repeater_index != kInvalidRangeIndex) {
      // This range is contained within a repeater of the explicit grid; at this
      // point, |current_explicit_grid_line| should be set to the start line of
      // such repeater.
      range.repeater_index = current_explicit_repeater_index;
      range.repeater_offset =
          (current_range_start_line - current_explicit_grid_line) %
          explicit_tracks_->RepeatSize(current_explicit_repeater_index);
    } else {
      range.SetIsImplicit();
      if (implicit_tracks_->RepeaterCount() == 0u) {
        // No specified implicit grid tracks, use 'auto'.
        range.repeater_index = kInvalidRangeIndex;
        range.repeater_offset = 0u;
      } else {
        // Otherwise, use the only repeater for implicit grid tracks.
        // There are 2 scenarios we want to cover here:
        //   1. At this point, we should not have reached any explicit repeater,
        //   since |current_explicit_grid_line| was initialized as the start
        //   line of the first explicit repeater (e.g. |start_offset|), it can
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
        wtf_size_t implicit_repeater_size = implicit_tracks_->RepeatSize(0u);
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
    const wtf_size_t track_number = range.start_line + range.track_count;
    for (wtf_size_t line_index = end_line_index;
         line_index < end_lines_.size() &&
         end_lines_[line_index].grid_line == track_number;
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
  DCHECK_EQ(end_line_index, start_lines_.size());

  start_lines_.clear();
  end_lines_.clear();
}

const NGGridTrackList& NGGridBlockTrackCollection::ExplicitTracks() const {
  DCHECK_NE(nullptr, explicit_tracks_);
  return *explicit_tracks_;
}

const NGGridTrackList& NGGridBlockTrackCollection::ImplicitTracks() const {
  DCHECK_NE(nullptr, implicit_tracks_);
  return *implicit_tracks_;
}

String NGGridBlockTrackCollection::ToString() const {
  if (ranges_.IsEmpty()) {
    StringBuilder builder;
    builder.Append("NGGridTrackCollection: [SpecifiedTracks: ");
    builder.Append(explicit_tracks_->ToString());
    if (HasImplicitTracks()) {
      builder.Append("], [ImplicitTracks: ");
      builder.Append(implicit_tracks_->ToString());
    }

    builder.Append("], [Starting: {");
    for (wtf_size_t i = 0; i < start_lines_.size(); ++i) {
      builder.AppendNumber<wtf_size_t>(start_lines_[i].grid_line);
      if (i + 1 != start_lines_.size())
        builder.Append(", ");
    }
    builder.Append("} ], [Ending: {");
    for (wtf_size_t i = 0; i < end_lines_.size(); ++i) {
      builder.AppendNumber<wtf_size_t>(end_lines_[i].grid_line);
      if (i + 1 != end_lines_.size())
        builder.Append(", ");
    }
    builder.Append("} ] ");
    return builder.ToString();
  } else {
    return NGGridTrackCollectionBase::ToString();
  }
}

bool NGGridBlockTrackCollection::HasImplicitTracks() const {
  return implicit_tracks_->RepeaterCount() != 0;
}

wtf_size_t NGGridBlockTrackCollection::ImplicitRepeatSize() const {
  DCHECK(HasImplicitTracks());
  return implicit_tracks_->RepeatSize(0);
}

wtf_size_t NGGridBlockTrackCollection::RangeTrackNumber(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index].start_line;
}

wtf_size_t NGGridBlockTrackCollection::RangeTrackCount(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index].track_count;
}

bool NGGridBlockTrackCollection::IsRangeCollapsed(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index].IsCollapsed();
}

wtf_size_t NGGridBlockTrackCollection::RangeCount() const {
  return ranges_.size();
}

NGGridSet::NGGridSet(wtf_size_t track_count)
    : track_count_(track_count),
      track_size_(Length::Auto(), Length::Auto()),
      fit_content_limit_(kIndefiniteSize) {}

NGGridSet::NGGridSet(wtf_size_t track_count,
                     const GridTrackSize& track_size,
                     bool is_available_size_indefinite)
    : track_count_(track_count),
      track_size_(track_size),
      fit_content_limit_(kIndefiniteSize) {
  if (track_size_.IsFitContent()) {
    DCHECK(track_size_.FitContentTrackBreadth().IsLength());

    // Argument for 'fit-content' is a <percentage> that couldn't be resolved to
    // a definite <length>, normalize to 'minmax(auto, max-content)'.
    if (is_available_size_indefinite &&
        track_size_.FitContentTrackBreadth().length().IsPercent()) {
      track_size_ = GridTrackSize(Length::Auto(), Length::MaxContent());
    }
  } else {
    // Normalize |track_size_| into a |kMinMaxTrackSizing| type; follow the
    // definitions from https://drafts.csswg.org/css-grid-2/#algo-terms.
    bool is_unresolvable_percentage_min_function =
        is_available_size_indefinite &&
        track_size_.MinTrackBreadth().HasPercentage();

    GridLength normalized_min_track_sizing_function =
        (is_unresolvable_percentage_min_function ||
         track_size_.HasFlexMinTrackBreadth())
            ? Length::Auto()
            : track_size_.MinTrackBreadth();

    bool is_unresolvable_percentage_max_function =
        is_available_size_indefinite &&
        track_size_.MaxTrackBreadth().HasPercentage();

    GridLength normalized_max_track_sizing_function =
        (is_unresolvable_percentage_max_function ||
         track_size_.HasAutoMaxTrackBreadth())
            ? Length::Auto()
            : track_size_.MaxTrackBreadth();

    track_size_ = GridTrackSize(normalized_min_track_sizing_function,
                                normalized_max_track_sizing_function);
  }
  DCHECK(track_size_.GetType() == kFitContentTrackSizing ||
         track_size_.GetType() == kMinMaxTrackSizing);
}

double NGGridSet::FlexFactor() const {
  DCHECK(track_size_.HasFlexMaxTrackBreadth());
  return track_size_.MaxTrackBreadth().Flex() * TrackCount();
}

bool NGGridSet::IsGrowthLimitLessThanBaseSize() const {
  return growth_limit_ != kIndefiniteSize && growth_limit_ < base_size_;
}

void NGGridSet::EnsureGrowthLimitIsNotLessThanBaseSize() {
  if (IsGrowthLimitLessThanBaseSize())
    growth_limit_ = base_size_;
}

LayoutUnit NGGridSet::BaseSize() const {
  DCHECK(!IsGrowthLimitLessThanBaseSize());
  return base_size_;
}

void NGGridSet::SetBaseSize(LayoutUnit base_size) {
  // Expect base size to always grow monotonically.
  DCHECK_NE(base_size, kIndefiniteSize);
  DCHECK_LE(base_size_, base_size);
  base_size_ = base_size;
  EnsureGrowthLimitIsNotLessThanBaseSize();
}

void NGGridSet::InitBaseSize(LayoutUnit base_size) {
  DCHECK_NE(base_size, kIndefiniteSize);
  base_size_ = base_size;
  EnsureGrowthLimitIsNotLessThanBaseSize();
}

LayoutUnit NGGridSet::GrowthLimit() const {
  DCHECK(!IsGrowthLimitLessThanBaseSize());
  return growth_limit_;
}

void NGGridSet::SetGrowthLimit(LayoutUnit growth_limit) {
  // Growth limit is initialized as infinity; expect it to change from infinity
  // to a definite value and then to always grow monotonically.
  DCHECK_NE(growth_limit, kIndefiniteSize);
  DCHECK(!IsGrowthLimitLessThanBaseSize());
  DCHECK(growth_limit_ == kIndefiniteSize || growth_limit_ <= growth_limit);
  growth_limit_ = growth_limit;
}

NGGridLayoutAlgorithmTrackCollection::Range::Range(
    const NGGridBlockTrackCollection::Range& block_track_range,
    wtf_size_t starting_set_index)
    : start_line(block_track_range.start_line),
      starting_set_index(starting_set_index),
      track_count(block_track_range.track_count),
      properties(block_track_range.properties) {}

bool NGGridLayoutAlgorithmTrackCollection::Range::IsCollapsed() const {
  return properties.HasProperty(TrackSpanProperties::kIsCollapsed);
}

NGGridLayoutAlgorithmTrackCollection::NGGridLayoutAlgorithmTrackCollection(
    const NGGridBlockTrackCollection& block_track_collection,
    bool is_available_size_indefinite)
    : track_direction_(block_track_collection.Direction()),
      non_collapsed_track_count_(0) {
  for (const auto& block_track_range : block_track_collection.Ranges()) {
    AppendTrackRange(block_track_range,
                     block_track_range.IsImplicit()
                         ? block_track_collection.ImplicitTracks()
                         : block_track_collection.ExplicitTracks(),
                     is_available_size_indefinite);
  }
}

void NGGridLayoutAlgorithmTrackCollection::SetAllGrowthLimitsToBaseSize() {
  for (auto set_iterator = GetSetIterator(); !set_iterator.IsAtEnd();
       set_iterator.MoveToNextSet()) {
    auto& set = set_iterator.CurrentSet();
    if (set.GrowthLimit() == kIndefiniteSize)
      set.SetGrowthLimit(set.BaseSize());
  }
}

void NGGridLayoutAlgorithmTrackCollection::AppendTrackRange(
    const NGGridBlockTrackCollection::Range& block_track_range,
    const NGGridTrackList& specified_track_list,
    bool is_available_size_indefinite) {
  Range new_range(block_track_range, /* starting_set_index */ sets_.size());

  if (block_track_range.repeater_index == kInvalidRangeIndex) {
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
        sets_[new_range.starting_set_index + i].TrackSize();

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

wtf_size_t NGGridLayoutAlgorithmTrackCollection::EndLineOfImplicitGrid() const {
  if (ranges_.IsEmpty())
    return 0;

  wtf_size_t last_range_index = ranges_.size() - 1;
  return RangeTrackNumber(last_range_index) + RangeTrackCount(last_range_index);
}

bool NGGridLayoutAlgorithmTrackCollection::IsGridLineWithinImplicitGrid(
    wtf_size_t grid_line) const {
  DCHECK_NE(grid_line, kInvalidRangeIndex);
  return grid_line <= EndLineOfImplicitGrid();
}

NGGridSet& NGGridLayoutAlgorithmTrackCollection::SetAt(wtf_size_t set_index) {
  DCHECK_LT(set_index, SetCount());
  return sets_[set_index];
}

const NGGridSet& NGGridLayoutAlgorithmTrackCollection::SetAt(
    wtf_size_t set_index) const {
  DCHECK_LT(set_index, SetCount());
  return sets_[set_index];
}

NGGridLayoutAlgorithmTrackCollection::SetIterator
NGGridLayoutAlgorithmTrackCollection::GetSetIterator() {
  return SetIterator(this, 0u, SetCount());
}

NGGridLayoutAlgorithmTrackCollection::ConstSetIterator
NGGridLayoutAlgorithmTrackCollection::GetConstSetIterator() const {
  return ConstSetIterator(this, 0u, SetCount());
}

NGGridLayoutAlgorithmTrackCollection::SetIterator
NGGridLayoutAlgorithmTrackCollection::GetSetIterator(wtf_size_t begin_set_index,
                                                     wtf_size_t end_set_index) {
  return SetIterator(this, begin_set_index, end_set_index);
}

wtf_size_t NGGridLayoutAlgorithmTrackCollection::RangeSetCount(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index].set_count;
}

wtf_size_t NGGridLayoutAlgorithmTrackCollection::RangeStartingSetIndex(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index].starting_set_index;
}

bool NGGridLayoutAlgorithmTrackCollection::RangeHasTrackSpanProperty(
    wtf_size_t range_index,
    TrackSpanProperties::PropertyId property_id) const {
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index].properties.HasProperty(property_id);
}

wtf_size_t NGGridLayoutAlgorithmTrackCollection::RangeTrackNumber(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index].start_line;
}

wtf_size_t NGGridLayoutAlgorithmTrackCollection::RangeTrackCount(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index].track_count;
}

bool NGGridLayoutAlgorithmTrackCollection::IsRangeCollapsed(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index].IsCollapsed();
}

wtf_size_t NGGridLayoutAlgorithmTrackCollection::RangeCount() const {
  return ranges_.size();
}

wtf_size_t NGGridLayoutAlgorithmTrackCollection::SetCount() const {
  return sets_.size();
}

}  // namespace blink
