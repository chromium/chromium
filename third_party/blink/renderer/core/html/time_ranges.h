/*
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_TIME_RANGES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_TIME_RANGES_H_

#include "third_party/blink/public/platform/web_time_range.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#include <algorithm>

namespace blink {

class ExceptionState;

class CORE_EXPORT TimeRanges final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // We consider all the Ranges to be semi-bounded as follow: [start, end[
  struct Range {
    DISALLOW_NEW();

   public:
    Range() = default;
    Range(double start, double end) {
      start_ = start;
      end_ = end;
    }
    double start_;
    double end_;

    inline bool isPointInRange(double point) const {
      return start_ <= point && point < end_;
    }

    inline bool IsOverlappingRange(const Range& range) const {
      return isPointInRange(range.start_) || isPointInRange(range.end_) ||
             range.isPointInRange(start_);
    }

    inline bool IsContiguousWithRange(const Range& range) const {
      return range.start_ == end_ || range.end_ == start_;
    }

    inline Range UnionWithOverlappingOrContiguousRange(
        const Range& range) const {
      Range ret;

      ret.start_ = std::min(start_, range.start_);
      ret.end_ = std::max(end_, range.end_);

      return ret;
    }

    inline bool IsBeforeRange(const Range& range) const {
      return range.start_ >= end_;
    }
  };

  static TimeRanges* Create() { return new TimeRanges; }
  static TimeRanges* Create(double start, double end) {
    return new TimeRanges(start, end);
  }
  static TimeRanges* Create(const WebTimeRanges&);

  TimeRanges* Copy() const;
  void IntersectWith(const TimeRanges*);
  void UnionWith(const TimeRanges*);

  unsigned length() const { return ranges_.size(); }
  double start(unsigned index, ExceptionState&) const;
  double end(unsigned index, ExceptionState&) const;

  void Add(double start, double end);

  bool Contain(double time) const;

  double Nearest(double new_playback_position,
                 double current_playback_position) const;

 private:
  TimeRanges() = default;

  TimeRanges(double start, double end);

  void Invert();

  Vector<Range> ranges_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_TIME_RANGES_H_
