/*
 * Copyright (C) 2019 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/platform/web_time_range.h"

#include <cmath>
#include <limits>

namespace blink {

void WebTimeRanges::Add(double start, double end) {
  DCHECK_LE(start, end);
  unsigned overlapping_arc_index;
  WebTimeRange added_range(start, end);

  // For each present range check if we need to:
  // - merge with the added range, in case we are overlapping or contiguous
  // - Need to insert in place, we we are completely, not overlapping and not
  //   contiguous in between two ranges.
  //
  // TODO: Given that we assume that ranges are correctly ordered, this could be
  // optimized.

  for (overlapping_arc_index = 0; overlapping_arc_index < size();
       overlapping_arc_index++) {
    if (added_range.IsOverlappingRange((*this)[overlapping_arc_index]) ||
        added_range.IsContiguousWithRange((*this)[overlapping_arc_index])) {
      // We need to merge the addedRange and that range.
      added_range = added_range.UnionWithOverlappingOrContiguousRange(
          (*this)[overlapping_arc_index]);
      EraseAt(overlapping_arc_index);
      overlapping_arc_index--;
    } else {
      // Check the case for which there is no more to do
      if (!overlapping_arc_index) {
        if (added_range.IsBeforeRange((*this)[0])) {
          // First index, and we are completely before that range (and not
          // contiguous, nor overlapping).  We just need to be inserted here.
          break;
        }
      } else {
        if ((*this)[overlapping_arc_index - 1].IsBeforeRange(added_range) &&
            added_range.IsBeforeRange((*this)[overlapping_arc_index])) {
          // We are exactly after the current previous range, and before the
          // current range, while not overlapping with none of them. Insert
          // here.
          break;
        }
      }
    }
  }

  // Now that we are sure we don't overlap with any range, just add it.
  Insert(overlapping_arc_index, added_range);
}

bool WebTimeRanges::Contain(double time) const {
  for (const WebTimeRange& range : *this) {
    if (time >= range.start && time <= range.end)
      return true;
  }
  return false;
}

void WebTimeRanges::Invert() {
  WebTimeRanges inverted;
  double pos_inf = std::numeric_limits<double>::infinity();
  double neg_inf = -std::numeric_limits<double>::infinity();

  if (!size()) {
    inverted.Add(neg_inf, pos_inf);
  } else {
    double start = front().start;
    if (start != neg_inf)
      inverted.Add(neg_inf, start);

    for (size_t index = 0; index + 1 < size(); ++index)
      inverted.Add((*this)[index].end, (*this)[index + 1].start);

    double end = back().end;
    if (end != pos_inf)
      inverted.Add(end, pos_inf);
  }

  swap(inverted);
}

void WebTimeRanges::IntersectWith(const WebTimeRanges& other) {
  WebTimeRanges inverted_other = other;
  inverted_other.Invert();

  Invert();
  UnionWith(inverted_other);
  Invert();
}

void WebTimeRanges::UnionWith(const WebTimeRanges& other) {
  for (const WebTimeRange& range : other) {
    Add(range.start, range.end);
  }
}

double WebTimeRanges::Nearest(double new_playback_position,
                              double current_playback_position) const {
  double best_match = 0;
  double best_delta = std::numeric_limits<double>::infinity();
  for (const WebTimeRange& range : *this) {
    double start_time = range.start;
    double end_time = range.end;
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
