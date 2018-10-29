/*
 * Copyright (C) 2007, 2009, 2010 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/time_ranges.h"

#include <math.h>

#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

TimeRanges::TimeRanges(double start, double end) {
  Add(start, end);
}

TimeRanges* TimeRanges::Create(const blink::WebTimeRanges& web_ranges) {
  TimeRanges* ranges = TimeRanges::Create();

  wtf_size_t size = SafeCast<wtf_size_t>(web_ranges.size());
  for (wtf_size_t i = 0; i < size; ++i)
    ranges->Add(web_ranges[i].start, web_ranges[i].end);

  return ranges;
}

TimeRanges* TimeRanges::Copy() const {
  TimeRanges* new_session = TimeRanges::Create();

  wtf_size_t size = ranges_.size();
  for (wtf_size_t i = 0; i < size; i++)
    new_session->Add(ranges_[i].start_, ranges_[i].end_);

  return new_session;
}

void TimeRanges::Invert() {
  TimeRanges* inverted = TimeRanges::Create();
  double pos_inf = std::numeric_limits<double>::infinity();
  double neg_inf = -std::numeric_limits<double>::infinity();

  if (!ranges_.size()) {
    inverted->Add(neg_inf, pos_inf);
  } else {
    double start = ranges_.front().start_;
    if (start != neg_inf)
      inverted->Add(neg_inf, start);

    for (wtf_size_t index = 0; index + 1 < ranges_.size(); ++index)
      inverted->Add(ranges_[index].end_, ranges_[index + 1].start_);

    double end = ranges_.back().end_;
    if (end != pos_inf)
      inverted->Add(end, pos_inf);
  }

  ranges_.swap(inverted->ranges_);
}

void TimeRanges::IntersectWith(const TimeRanges* other) {
  DCHECK(other);

  if (other == this)
    return;

  TimeRanges* inverted_other = other->Copy();
  inverted_other->Invert();

  Invert();
  UnionWith(inverted_other);
  Invert();
}

void TimeRanges::UnionWith(const TimeRanges* other) {
  DCHECK(other);
  TimeRanges* unioned = Copy();
  for (wtf_size_t index = 0; index < other->ranges_.size(); ++index) {
    const Range& range = other->ranges_[index];
    unioned->Add(range.start_, range.end_);
  }

  ranges_.swap(unioned->ranges_);
}

double TimeRanges::start(unsigned index,
                         ExceptionState& exception_state) const {
  if (index >= length()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMaximumBound("index", index, length()));
    return 0;
  }
  return ranges_[index].start_;
}

double TimeRanges::end(unsigned index, ExceptionState& exception_state) const {
  if (index >= length()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMaximumBound("index", index, length()));
    return 0;
  }
  return ranges_[index].end_;
}

void TimeRanges::Add(double start, double end) {
  DCHECK_LE(start, end);
  unsigned overlapping_arc_index;
  Range added_range(start, end);

  // For each present range check if we need to:
  // - merge with the added range, in case we are overlapping or contiguous
  // - Need to insert in place, we we are completely, not overlapping and not
  //   contiguous in between two ranges.
  //
  // TODO: Given that we assume that ranges are correctly ordered, this could be
  // optimized.

  for (overlapping_arc_index = 0; overlapping_arc_index < ranges_.size();
       overlapping_arc_index++) {
    if (added_range.IsOverlappingRange(ranges_[overlapping_arc_index]) ||
        added_range.IsContiguousWithRange(ranges_[overlapping_arc_index])) {
      // We need to merge the addedRange and that range.
      added_range = added_range.UnionWithOverlappingOrContiguousRange(
          ranges_[overlapping_arc_index]);
      ranges_.EraseAt(overlapping_arc_index);
      overlapping_arc_index--;
    } else {
      // Check the case for which there is no more to do
      if (!overlapping_arc_index) {
        if (added_range.IsBeforeRange(ranges_[0])) {
          // First index, and we are completely before that range (and not
          // contiguous, nor overlapping).  We just need to be inserted here.
          break;
        }
      } else {
        if (ranges_[overlapping_arc_index - 1].IsBeforeRange(added_range) &&
            added_range.IsBeforeRange(ranges_[overlapping_arc_index])) {
          // We are exactly after the current previous range, and before the
          // current range, while not overlapping with none of them. Insert
          // here.
          break;
        }
      }
    }
  }

  // Now that we are sure we don't overlap with any range, just add it.
  ranges_.insert(overlapping_arc_index, added_range);
}

bool TimeRanges::Contain(double time) const {
  for (unsigned n = 0; n < length(); n++) {
    if (time >= start(n, IGNORE_EXCEPTION_FOR_TESTING) &&
        time <= end(n, IGNORE_EXCEPTION_FOR_TESTING))
      return true;
  }
  return false;
}

double TimeRanges::Nearest(double new_playback_position,
                           double current_playback_position) const {
  unsigned count = length();
  double best_match = 0;
  double best_delta = std::numeric_limits<double>::infinity();
  for (unsigned ndx = 0; ndx < count; ndx++) {
    double start_time = start(ndx, IGNORE_EXCEPTION_FOR_TESTING);
    double end_time = end(ndx, IGNORE_EXCEPTION_FOR_TESTING);
    if (new_playback_position >= start_time &&
        new_playback_position <= end_time)
      return new_playback_position;

    double delta, match;
    if (new_playback_position < start_time) {
      delta = start_time - new_playback_position;
      match = start_time;
    } else {
      delta = new_playback_position - end_time;
      match = end_time;
    }

    if (delta < best_delta ||
        (delta == best_delta &&
         std::abs(current_playback_position - match) <
             std::abs(current_playback_position - best_match))) {
      best_delta = delta;
      best_match = match;
    }
  }
  return best_match;
}

}  // namespace blink
