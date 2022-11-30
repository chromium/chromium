// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_TIMELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_TIMELINE_H_

#include "base/check_op.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

// https://drafts.csswg.org/css-animations-2/#typedef-single-animation-timeline
class CORE_EXPORT StyleTimeline {
 public:
  // https://drafts.csswg.org/scroll-animations-1/#scroll-notation
  class ScrollData {
   public:
    // https://drafts.csswg.org/scroll-animations-1/#valdef-scroll-block
    static TimelineAxis DefaultAxis() { return TimelineAxis::kBlock; }
    // https://drafts.csswg.org/scroll-animations-1/#valdef-scroll-nearest
    static TimelineScroller DefaultScroller() {
      return TimelineScroller::kNearest;
    }

    ScrollData(TimelineAxis axis, TimelineScroller scroller)
        : axis_(axis), scroller_(scroller) {}
    bool operator==(const ScrollData& other) const {
      return axis_ == other.axis_ && scroller_ == other.scroller_;
    }
    bool operator!=(const ScrollData& other) const { return !(*this == other); }

    TimelineAxis GetAxis() const { return axis_; }
    const TimelineScroller& GetScroller() const { return scroller_; }

    bool HasDefaultAxis() const { return axis_ == DefaultAxis(); }
    bool HasDefaultScroller() const { return scroller_ == DefaultScroller(); }

   private:
    TimelineAxis axis_;
    TimelineScroller scroller_;
  };

  explicit StyleTimeline(CSSValueID keyword) : data_(keyword) {}
  explicit StyleTimeline(StyleName name) : data_(name) {}
  explicit StyleTimeline(const ScrollData& scroll_data) : data_(scroll_data) {}

  bool operator==(const StyleTimeline& other) const {
    return data_ == other.data_;
  }
  bool operator!=(const StyleTimeline& other) const {
    return !(*this == other);
  }

  bool IsKeyword() const { return absl::holds_alternative<CSSValueID>(data_); }
  bool IsName() const { return absl::holds_alternative<StyleName>(data_); }
  bool IsScroll() const { return absl::holds_alternative<ScrollData>(data_); }

  const CSSValueID& GetKeyword() const { return absl::get<CSSValueID>(data_); }
  const StyleName& GetName() const { return absl::get<StyleName>(data_); }
  const ScrollData& GetScroll() const { return absl::get<ScrollData>(data_); }

 private:
  absl::variant<CSSValueID, StyleName, ScrollData> data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_TIMELINE_H_
