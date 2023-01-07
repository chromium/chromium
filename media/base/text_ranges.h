// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_TEXT_RANGES_H_
#define MEDIA_BASE_TEXT_RANGES_H_

#include <stddef.h>

#include <map>

#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {

// Helper class used by the TextRenderer to filter out text cues that
// have already been passed downstream.
class MEDIA_EXPORT TextRanges {
 public:
  TextRanges();

  TextRanges(const TextRanges&) = delete;
  TextRanges& operator=(const TextRanges&) = delete;

  ~TextRanges();

  // Reset the current range pointer, such that we bind to a new range
  // (either one that exists already, or one that is freshly-created)
  // during the next AddCue().
  void Reset();

  // Given a cue with starting timestamp |start_time|, add its start
  // time to the time ranges. (Note that following a Reset(), cue
  // times are assumed to be monotonically increasing.) If this time
  // has already been added to the time ranges, then AddCue() returns
  // false and clients should not push the cue downstream. Otherwise,
  // the time is added to the time ranges and AddCue() returns true,
  // meaning that the cue should be pushed downstream.
  bool AddCue(base::TimeDelta start_time);

  // Returns a count of the number of time ranges, intended for use by
  // the unit test module to vet proper time range merge behavior.
  size_t RangeCountForTesting() const;

 private:
  // Describes a range of times for cues that have already been
  // pushed downstream.
  class Range {
   public:
    // Initialize last_time count.
    void ResetCount(base::TimeDelta start_time);

    // Set last_time and associated counts.
    void SetLastTime(base::TimeDelta last_time);

    // Adjust time range state to mark the cue as having been seen,
    // returning true if we have not seen |start_time| already and
    // false otherwise.
    bool AddCue(base::TimeDelta start_time);

    // Returns the value of the last time in the range.
    base::TimeDelta last_time() const;

   private:
    // The last timestamp of this range.
    base::TimeDelta last_time_;

    // The number of cues we have detected so far, for this range,
    // whose timestamp matches last_time.
    int max_count_;

    // The number of cues we have seen since the most recent Reset(),
    // whose timestamp matches last_time.
    int count_;
  };

  typedef std::map<base::TimeDelta, Range> RangeMap;

  // NewRange() is used to create a new time range when AddCue() is
  // called immediately following a Reset(), and no existing time
  // range contains the indicated |start_time| of the cue.
  void NewRange(base::TimeDelta start_time);

  // Coalesce curr_range with the range that immediately follows.
  void Merge(Range& curr_range, const RangeMap::iterator& next_range_itr);

  // The collection of time ranges, each of which is bounded
  // (inclusive) by the key and Range::last_time.
  RangeMap range_map_;

  // The time range to which we bind following a Reset().
  RangeMap::iterator curr_range_itr_;
};

}  // namespace media

#endif  // MEDIA_BASE_TEXT_RANGES_H_
