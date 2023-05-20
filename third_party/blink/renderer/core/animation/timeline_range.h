// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_RANGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_RANGE_H_

#include "cc/animation/scroll_timeline.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_range.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

struct TimelineOffset;

// A TimelineRange represents a given scroll range within an associated
// scroller's minimum/maximum scroll. This is useful for ViewTimelines
// in particular, because they represent exactly that: a segment of a (non-view)
// ScrollTimeline.
//
// The primary job of TimelineRange is to convert offsets within an animation
// attachment range [1] (represented by TimelineOffset values) to fractional
// offsets within the TimelineRange. See TimelineRange::ToFractionalOffset.
//
// It may be helpful to think about a TimelineRange (which is timeline-specific)
// as a sub-range of the scroller's full range, and an animation attachment
// range (which is animation specific) as a sub-range of that TimelineRange.
//
// - For ViewTimelines, the start/end offsets will correspond to the scroll
//   range that would cause the scrollport to intersect with the subject
//   element's box.
// - For (non-view) ScrollTimelines, the start/end offset is always the
//   same as the minimum/maximum scroll.
// - For monotonic timelines, the TimelineRange is always empty.
//
// [1]
// https://drafts.csswg.org/scroll-animations-1/#named-range-animation-declaration
class CORE_EXPORT TimelineRange {
 public:
  using ScrollOffsets = cc::ScrollTimeline::ScrollOffsets;
  using NamedRange = V8TimelineRange::Enum;

  TimelineRange() = default;
  // The subject_size is the size of the subject element for ViewTimelines.
  // It should be zero for other timelines.
  explicit TimelineRange(ScrollOffsets offsets, double subject_size = 0)
      : offsets_(offsets), subject_size_(subject_size) {}

  bool operator==(const TimelineRange& other) const {
    return offsets_ == other.offsets_ && subject_size_ == other.subject_size_;
  }

  bool operator!=(const TimelineRange& other) const {
    return !(*this == other);
  }

  bool IsEmpty() const;

  // Converts an offset within some animation attachment range to a fractional
  // offset within this TimelineRange.
  double ToFractionalOffset(const TimelineOffset&) const;

 private:
  // https://drafts.csswg.org/scroll-animations-1/#view-timelines-ranges
  ScrollOffsets ConvertNamedRange(NamedRange) const;

  ScrollOffsets offsets_;
  double subject_size_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_RANGE_H_
