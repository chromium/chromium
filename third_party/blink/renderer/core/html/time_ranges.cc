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

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

TimeRanges::TimeRanges(double start, double end) {
  Add(start, end);
}

TimeRanges::TimeRanges(const blink::WebTimeRanges& web_ranges) {
  wtf_size_t size = base::checked_cast<wtf_size_t>(web_ranges.size());
  for (wtf_size_t i = 0; i < size; ++i)
    Add(web_ranges[i].start, web_ranges[i].end);
}

TimeRanges* TimeRanges::Copy() const {
  return MakeGarbageCollected<TimeRanges>(ranges_);
}

void TimeRanges::IntersectWith(const TimeRanges* other) {
  DCHECK(other);

  if (other == this)
    return;

  ranges_.IntersectWith(other->ranges_);
}

void TimeRanges::UnionWith(const TimeRanges* other) {
  DCHECK(other);
  ranges_.UnionWith(other->ranges_);
}

double TimeRanges::start(unsigned index,
                         ExceptionState& exception_state) const {
  if (index >= length()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMaximumBound("index", index, length()));
    return 0;
  }
  return ranges_[index].start;
}

double TimeRanges::end(unsigned index, ExceptionState& exception_state) const {
  if (index >= length()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMaximumBound("index", index, length()));
    return 0;
  }
  return ranges_[index].end;
}

void TimeRanges::Add(double start, double end) {
  ranges_.Add(start, end);
}

bool TimeRanges::Contain(double time) const {
  return ranges_.Contain(time);
}

double TimeRanges::Nearest(double new_playback_position,
                           double current_playback_position) const {
  return ranges_.Nearest(new_playback_position, current_playback_position);
}

}  // namespace blink
