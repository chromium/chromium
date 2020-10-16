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
  DCHECK_NE(track_number, kInvalidRangeIndex);
  DCHECK_LT(track_number,
            RangeTrackNumber(upper - 1u) + RangeTrackCount(upper - 1u));

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

void NGGridBlockTrackCollection::SetSpecifiedTracks(
    const NGGridTrackList* explicit_tracks,
    const NGGridTrackList* implicit_tracks,
    wtf_size_t auto_repeat_count) {
  DCHECK_NE(nullptr, explicit_tracks);
  DCHECK_NE(nullptr, implicit_tracks);
  // The implicit track list should have only one repeater, if any.
  DCHECK_LE(implicit_tracks->RepeaterCount(), 1u);
  DCHECK_NE(NGGridTrackCollectionBase::kInvalidRangeIndex, auto_repeat_count);
  explicit_tracks_ = explicit_tracks;
  implicit_tracks_ = implicit_tracks;
  auto_repeat_count_ = auto_repeat_count;

  const wtf_size_t repeater_count = explicit_tracks_->RepeaterCount();
  wtf_size_t total_track_count = 0;

  for (wtf_size_t i = 0; i < repeater_count; ++i) {
    wtf_size_t repeater_track_count =
        explicit_tracks_->RepeatCount(i, auto_repeat_count_) *
        explicit_tracks_->RepeatSize(i);
    if (repeater_track_count != 0) {
      starting_tracks_.push_back(total_track_count);
      ending_tracks_.push_back(total_track_count + repeater_track_count);
    }
    total_track_count += repeater_track_count;
  }
}

void NGGridBlockTrackCollection::EnsureTrackCoverage(wtf_size_t track_number,
                                                     wtf_size_t span_length) {
  DCHECK_NE(kInvalidRangeIndex, track_number);
  DCHECK_NE(kInvalidRangeIndex, span_length);
  track_indices_need_sort_ = true;
  starting_tracks_.push_back(track_number);
  ending_tracks_.push_back(track_number + span_length);
}

void NGGridBlockTrackCollection::FinalizeRanges() {
  ranges_.clear();

  // Sort start and ending tracks from low to high.
  if (track_indices_need_sort_) {
    std::stable_sort(starting_tracks_.begin(), starting_tracks_.end());
    std::stable_sort(ending_tracks_.begin(), ending_tracks_.end());
  }

  wtf_size_t current_range_track_start = 0u;

  // Indices into the starting and ending track vectors.
  wtf_size_t starting_tracks_index = 0u;
  wtf_size_t ending_tracks_index = 0u;

  wtf_size_t repeater_index = kInvalidRangeIndex;
  wtf_size_t repeater_track_start = kInvalidRangeIndex;
  wtf_size_t next_repeater_track_start = 0u;
  wtf_size_t current_repeater_track_count = 0u;

  wtf_size_t total_repeater_count = explicit_tracks_->RepeaterCount();
  wtf_size_t open_items_or_repeaters = 0u;
  bool is_in_auto_fit_range = false;

  while (true) {
    // Identify starting tracks index.
    while (starting_tracks_index < starting_tracks_.size() &&
           current_range_track_start >=
               starting_tracks_[starting_tracks_index]) {
      ++starting_tracks_index;
      ++open_items_or_repeaters;
    }

    // Identify ending tracks index.
    while (ending_tracks_index < ending_tracks_.size() &&
           current_range_track_start >= ending_tracks_[ending_tracks_index]) {
      ++ending_tracks_index;
      --open_items_or_repeaters;
      DCHECK_GE(open_items_or_repeaters, 0u);
    }

    // Identify ending tracks index.
    if (ending_tracks_index >= ending_tracks_.size()) {
      DCHECK_EQ(open_items_or_repeaters, 0u);
      break;
    }

    // Determine the next starting and ending track index.
    wtf_size_t next_starting_track = kInvalidRangeIndex;
    if (starting_tracks_index < starting_tracks_.size())
      next_starting_track = starting_tracks_[starting_tracks_index];
    wtf_size_t next_ending_track = ending_tracks_[ending_tracks_index];

    // Move |next_repeater_track_start| to the start of the next repeater, if
    // needed.
    while (current_range_track_start == next_repeater_track_start) {
      if (++repeater_index == total_repeater_count) {
        repeater_index = kInvalidRangeIndex;
        repeater_track_start = next_repeater_track_start;
        is_in_auto_fit_range = false;
        break;
      }

      is_in_auto_fit_range = explicit_tracks_->RepeatType(repeater_index) ==
                             NGGridTrackRepeater::RepeatType::kAutoFit;
      current_repeater_track_count =
          explicit_tracks_->RepeatCount(repeater_index, auto_repeat_count_) *
          explicit_tracks_->RepeatSize(repeater_index);
      repeater_track_start = next_repeater_track_start;
      next_repeater_track_start += current_repeater_track_count;
    }

    // Determine track number and count of the range.
    Range range;
    range.starting_track_number = current_range_track_start;
    DCHECK(next_starting_track != kInvalidRangeIndex ||
           next_ending_track < next_starting_track);
    range.track_count = std::min(next_ending_track, next_starting_track) -
                        current_range_track_start;

    // Compute repeater index and offset.
    if (repeater_index == kInvalidRangeIndex) {
      range.is_implicit_range = true;
      if (implicit_tracks_->RepeaterCount() == 0) {
        // No specified implicit tracks, use auto tracks.
        range.repeater_index = kInvalidRangeIndex;
        range.repeater_offset = 0;
      } else {
        // Use implicit tracks.
        range.repeater_index = 0;
        range.repeater_offset =
            current_range_track_start - repeater_track_start;
      }
    } else {
      range.is_implicit_range = false;
      range.repeater_index = repeater_index;
      range.repeater_offset = current_range_track_start - repeater_track_start;
    }
    range.is_collapsed = is_in_auto_fit_range && open_items_or_repeaters == 1u;

    current_range_track_start += range.track_count;
    ranges_.push_back(range);
  }

#if DCHECK_IS_ON()
  while (repeater_index != kInvalidRangeIndex &&
         repeater_index < total_repeater_count - 1u) {
    ++repeater_index;
    DCHECK_EQ(0u, explicit_tracks_->RepeatSize(repeater_index));
  }
#endif
  DCHECK_EQ(starting_tracks_index, starting_tracks_.size());
  DCHECK_EQ(ending_tracks_index, starting_tracks_.size());
  DCHECK(repeater_index == total_repeater_count - 1u ||
         repeater_index == kInvalidRangeIndex);
  starting_tracks_.clear();
  ending_tracks_.clear();
}

const NGGridBlockTrackCollection::Range&
NGGridBlockTrackCollection::RangeAtRangeIndex(wtf_size_t range_index) const {
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index];
}

const NGGridBlockTrackCollection::Range&
NGGridBlockTrackCollection::RangeAtTrackNumber(wtf_size_t track_number) const {
  wtf_size_t range_index = RangeIndexFromTrackNumber(track_number);
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index];
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
    for (wtf_size_t i = 0; i < starting_tracks_.size(); ++i) {
      builder.AppendNumber<wtf_size_t>(starting_tracks_[i]);
      if (i + 1 != starting_tracks_.size())
        builder.Append(", ");
    }
    builder.Append("} ], [Ending: {");
    for (wtf_size_t i = 0; i < ending_tracks_.size(); ++i) {
      builder.AppendNumber<wtf_size_t>(ending_tracks_[i]);
      if (i + 1 != ending_tracks_.size())
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
  DCHECK_LT(range_index, RangeCount());
  return ranges_[range_index].starting_track_number;
}

wtf_size_t NGGridBlockTrackCollection::RangeTrackCount(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, RangeCount());
  return ranges_[range_index].track_count;
}

bool NGGridBlockTrackCollection::IsRangeCollapsed(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, RangeCount());
  return ranges_[range_index].is_collapsed;
}

wtf_size_t NGGridBlockTrackCollection::RangeCount() const {
  return ranges_.size();
}

// Default track size for a set with no specified track definition should be
// 'auto', but we will normalize it directly as 'minmax(auto, max-content)'.
NGGridSet::NGGridSet(wtf_size_t track_count, bool is_collapsed)
    : track_count_(track_count),
      track_size_(Length::Auto(), Length::MaxContent()),
      growth_limit_(kIndefiniteSize),
      fit_content_limit_(kIndefiniteSize),
      is_infinitely_growable_(false) {
  if (is_collapsed) {
    // From https://drafts.csswg.org/css-grid-1/#collapsed-track: "A collapsed
    // track is treated as having a fixed track sizing function of '0px'".
    track_size_ = GridTrackSize(Length::Fixed(), Length::Fixed());
  }
}

NGGridSet::NGGridSet(wtf_size_t track_count,
                     const GridTrackSize& track_size,
                     bool is_content_box_size_indefinite)
    : track_count_(track_count),
      track_size_(track_size),
      growth_limit_(kIndefiniteSize),
      fit_content_limit_(kIndefiniteSize),
      is_infinitely_growable_(false) {
  if (track_size_.IsFitContent()) {
    DCHECK(track_size_.FitContentTrackBreadth().IsLength());

    // Argument for 'fit-content' is a <percentage> that couldn't be resolved to
    // a definite <length>, normalize to 'minmax(auto, max-content)'.
    if (is_content_box_size_indefinite &&
        track_size_.FitContentTrackBreadth().length().IsPercent()) {
      track_size_ = GridTrackSize(Length::Auto(), Length::MaxContent());
    }
  } else {
    // Normalize |track_size_| into a |kMinMaxTrackSizing| type; follow the
    // definitions from https://drafts.csswg.org/css-grid-1/#algo-terms.
    bool is_unresolvable_percentage_min_function =
        is_content_box_size_indefinite &&
        track_size_.MinTrackBreadth().HasPercentage();

    GridLength normalized_min_track_sizing_function =
        (is_unresolvable_percentage_min_function ||
         track_size_.HasFlexMinTrackBreadth())
            ? Length::Auto()
            : track_size_.MinTrackBreadth();

    bool is_unresolvable_percentage_max_function =
        is_content_box_size_indefinite &&
        track_size_.MaxTrackBreadth().HasPercentage();

    GridLength normalized_max_track_sizing_function =
        (is_unresolvable_percentage_max_function ||
         track_size_.HasAutoMaxTrackBreadth())
            ? Length::MaxContent()
            : track_size_.MaxTrackBreadth();

    track_size_ = GridTrackSize(normalized_min_track_sizing_function,
                                normalized_max_track_sizing_function);
  }
  DCHECK(track_size_.GetType() == kFitContentTrackSizing ||
         track_size_.GetType() == kMinMaxTrackSizing);
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
    : starting_track_number(block_track_range.starting_track_number),
      track_count(block_track_range.track_count),
      starting_set_index(starting_set_index),
      is_collapsed(block_track_range.is_collapsed) {}

NGGridLayoutAlgorithmTrackCollection::SetIterator::SetIterator(
    NGGridLayoutAlgorithmTrackCollection* collection,
    wtf_size_t begin_set_index,
    wtf_size_t end_set_index)
    : collection_(collection),
      current_set_index_(begin_set_index),
      end_set_index_(end_set_index) {
  DCHECK(collection_);
  DCHECK_LE(current_set_index_, end_set_index_);
}

bool NGGridLayoutAlgorithmTrackCollection::SetIterator::IsAtEnd() const {
  DCHECK_LE(current_set_index_, end_set_index_);
  return current_set_index_ == end_set_index_;
}

bool NGGridLayoutAlgorithmTrackCollection::SetIterator::MoveToNextSet() {
  current_set_index_ = std::min(current_set_index_ + 1, end_set_index_);
  return current_set_index_ < end_set_index_;
}

NGGridSet& NGGridLayoutAlgorithmTrackCollection::SetIterator::CurrentSet()
    const {
  DCHECK_LT(current_set_index_, end_set_index_);
  return collection_->SetAt(current_set_index_);
}

NGGridLayoutAlgorithmTrackCollection::NGGridLayoutAlgorithmTrackCollection(
    const NGGridBlockTrackCollection& block_track_collection,
    bool is_content_box_size_indefinite) {
  for (auto range_iterator = block_track_collection.RangeIterator();
       !range_iterator.IsAtEnd(); range_iterator.MoveToNextRange()) {
    const NGGridBlockTrackCollection::Range& block_track_range =
        block_track_collection.RangeAtRangeIndex(range_iterator.RangeIndex());
    AppendTrackRange(block_track_range,
                     block_track_range.is_implicit_range
                         ? block_track_collection.ImplicitTracks()
                         : block_track_collection.ExplicitTracks(),
                     is_content_box_size_indefinite);
  }
}

void NGGridLayoutAlgorithmTrackCollection::AppendTrackRange(
    const NGGridBlockTrackCollection::Range& block_track_range,
    const NGGridTrackList& specified_track_list,
    bool is_content_box_size_indefinite) {
  Range new_range(block_track_range, /* starting_set_index */ sets_.size());

  if (block_track_range.is_collapsed ||
      block_track_range.repeater_index == kInvalidRangeIndex) {
#if DCHECK_IS_ON()
    // If there are no specified repeaters for this range, it must be implicit.
    if (block_track_range.repeater_index == kInvalidRangeIndex)
      DCHECK(block_track_range.is_implicit_range);
#endif

    // Append a single element for the entire range's set.
    new_range.set_count = 1;
    sets_.emplace_back(block_track_range.track_count,
                       block_track_range.is_collapsed);
  } else {
    wtf_size_t repeater_size =
        specified_track_list.RepeatSize(block_track_range.repeater_index);

    // The number of different set elements in this range is the number of track
    // definitions from |NGGridBlockTrackCollection| range's repeater clamped by
    // the range's total track count if it's less than the repeater's size.
    new_range.set_count =
        std::min(repeater_size, block_track_range.track_count);
    DCHECK_GT(new_range.set_count, 0u);

    // The following two variables help compute how many tracks a set element
    // compresses; suppose we want to print this range, we would circle through
    // the repeater's track list, starting at the range's repeater offset,
    // printing every definition until the track count for the range is covered:
    //
    // 1. |floor_set_track_count| is the number of times we would return to the
    // range's repeater offset, meaning that every definition in the repeater's
    // track list appears at least that many times within the range.
    wtf_size_t floor_set_track_count = new_range.track_count / repeater_size;
    // 2. The remaining track count would not complete another iteration over
    // the entire repeater; this means that the first |remaining_track_count|
    // definitions appear one more time in the range.
    wtf_size_t remaining_track_count = new_range.track_count % repeater_size;

    for (wtf_size_t i = 0; i < new_range.set_count; ++i) {
      wtf_size_t set_track_count =
          floor_set_track_count + ((i < remaining_track_count) ? 1 : 0);
      wtf_size_t set_repeater_offset =
          (block_track_range.repeater_offset + i) % repeater_size;
      const GridTrackSize& set_track_size =
          specified_track_list.RepeatTrackSize(block_track_range.repeater_index,
                                               set_repeater_offset);
      sets_.emplace_back(set_track_count, set_track_size,
                         is_content_box_size_indefinite);
    }
  }

  // Cache if this range contains an intrinsic or flexible track.
  new_range.is_spanning_flex_track = false;
  new_range.is_spanning_intrinsic_track = false;
  for (wtf_size_t i = 0; i < new_range.set_count; ++i) {
    const NGGridSet& set = sets_[new_range.starting_set_index + i];

    // From https://drafts.csswg.org/css-grid-1/#algo-terms, a <flex> minimum
    // sizing function shouldn't happen as it would be normalized to 'auto'.
    DCHECK(!set.TrackSize().HasFlexMinTrackBreadth());
    new_range.is_spanning_flex_track |=
        set.TrackSize().HasFlexMaxTrackBreadth();
    new_range.is_spanning_intrinsic_track |=
        set.TrackSize().HasIntrinsicMinTrackBreadth() ||
        set.TrackSize().HasIntrinsicMaxTrackBreadth();
  }
  ranges_.push_back(new_range);
}

NGGridSet& NGGridLayoutAlgorithmTrackCollection::SetAt(wtf_size_t set_index) {
  DCHECK_LT(set_index, SetCount());
  return sets_[set_index];
}

NGGridLayoutAlgorithmTrackCollection::SetIterator
NGGridLayoutAlgorithmTrackCollection::GetSetIterator() {
  return SetIterator(this, 0u, SetCount());
}

NGGridLayoutAlgorithmTrackCollection::SetIterator
NGGridLayoutAlgorithmTrackCollection::GetSetIterator(wtf_size_t begin_set_index,
                                                     wtf_size_t end_set_index) {
  DCHECK_LE(end_set_index, SetCount());
  DCHECK_LE(begin_set_index, end_set_index);
  return SetIterator(this, begin_set_index, end_set_index);
}

wtf_size_t NGGridLayoutAlgorithmTrackCollection::RangeSetCount(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, RangeCount());
  return ranges_[range_index].set_count;
}

wtf_size_t NGGridLayoutAlgorithmTrackCollection::RangeStartingSetIndex(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, RangeCount());
  return ranges_[range_index].starting_set_index;
}

bool NGGridLayoutAlgorithmTrackCollection::IsRangeSpanningIntrinsicTrack(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, RangeCount());
  return ranges_[range_index].is_spanning_intrinsic_track;
}

bool NGGridLayoutAlgorithmTrackCollection::IsRangeSpanningFlexTrack(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, RangeCount());
  return ranges_[range_index].is_spanning_flex_track;
}

wtf_size_t NGGridLayoutAlgorithmTrackCollection::RangeTrackNumber(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, RangeCount());
  return ranges_[range_index].starting_track_number;
}

wtf_size_t NGGridLayoutAlgorithmTrackCollection::RangeTrackCount(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, RangeCount());
  return ranges_[range_index].track_count;
}

bool NGGridLayoutAlgorithmTrackCollection::IsRangeCollapsed(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, RangeCount());
  return ranges_[range_index].is_collapsed;
}

wtf_size_t NGGridLayoutAlgorithmTrackCollection::RangeCount() const {
  return ranges_.size();
}

wtf_size_t NGGridLayoutAlgorithmTrackCollection::SetCount() const {
  return sets_.size();
}

}  // namespace blink
