// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/text_ranges.h"

#include "base/check_op.h"

namespace media {

TextRanges::TextRanges() {
  Reset();
}

TextRanges::~TextRanges() = default;

void TextRanges::Reset() {
  curr_range_itr_ = range_map_.end();
}

bool TextRanges::AddCue(base::TimeDelta start_time) {
  typedef RangeMap::iterator Itr;

  if (curr_range_itr_ == range_map_.end()) {
    // There is no active time range, so this is the first AddCue()
    // attempt that follows a Reset().

    if (range_map_.empty()) {
      NewRange(start_time);
      return true;
    }

    if (start_time < range_map_.begin()->first) {
      NewRange(start_time);
      return true;
    }

    const Itr itr = --Itr(range_map_.upper_bound(start_time));
    DCHECK(start_time >= itr->first);

    Range& range = itr->second;

    if (start_time > range.last_time()) {
      NewRange(start_time);
      return true;
    }

    range.ResetCount(start_time);
    curr_range_itr_ = itr;
    return false;
  }

  DCHECK(start_time >= curr_range_itr_->first);

  Range& curr_range = curr_range_itr_->second;

  if (start_time <= curr_range.last_time())
    return curr_range.AddCue(start_time);

  const Itr next_range_itr = ++Itr(curr_range_itr_);

  if (next_range_itr != range_map_.end()) {
    DCHECK(next_range_itr->first > curr_range.last_time());
    DCHECK(start_time <= next_range_itr->first);

    if (start_time == next_range_itr->first) {
      // We have walked off the current range, and onto the next one.
      // There is now no ambiguity about where the current time range
      // ends, and so we coalesce the current and next ranges.

      Merge(curr_range, next_range_itr);
      return false;
    }
  }

  // Either |curr_range| is the last range in the map, or there is a
  // next range beyond |curr_range|, but its start time is ahead of
  // this cue's start time.  In either case, this cue becomes the new
  // last_time for |curr_range|.  Eventually we will see a cue whose
  // time matches the start time of the next range, in which case we
  // coalesce the current and next ranges.

  curr_range.SetLastTime(start_time);
  return true;
}

size_t TextRanges::RangeCountForTesting() const {
  return range_map_.size();
}

void TextRanges::NewRange(base::TimeDelta start_time) {
  Range range;
  range.SetLastTime(start_time);

  std::pair<RangeMap::iterator, bool> result =
      range_map_.insert(std::make_pair(start_time, range));
  DCHECK(result.second);

  curr_range_itr_ = result.first;
}

void TextRanges::Merge(
    Range& curr_range,
    const RangeMap::iterator& next_range_itr) {
  curr_range = next_range_itr->second;
  curr_range.ResetCount(next_range_itr->first);
  range_map_.erase(next_range_itr);
}

void TextRanges::Range::ResetCount(base::TimeDelta start_time) {
  count_ = (start_time < last_time_) ? 0 : 1;
}

void TextRanges::Range::SetLastTime(base::TimeDelta last_time) {
  last_time_ = last_time;
  count_ = 1;
  max_count_ = 1;
}

bool TextRanges::Range::AddCue(base::TimeDelta start_time) {
  if (start_time < last_time_) {
    DCHECK_EQ(count_, 0);
    return false;
  }

  DCHECK(start_time == last_time_);

  ++count_;
  if (count_ <= max_count_)
    return false;

  ++max_count_;
  return true;
}

base::TimeDelta TextRanges::Range::last_time() const {
  return last_time_;
}

}  // namespace media
