// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_SCROLL_TIMELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_SCROLL_TIMELINE_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"

namespace blink {

class Document;
class Element;
class StyleRuleScrollTimeline;

// A CSSScrollTimeline is like a ScrollTimeline, except it originates from
// an @scroll-timeline rule.
class CORE_EXPORT CSSScrollTimeline : public ScrollTimeline {
 public:
  struct Options {
    STACK_ALLOCATED();

   public:
    Options(Element*, StyleRuleScrollTimeline&);

    // TODO(crbug.com/1097041): Support 'auto' value.
    bool IsValid() const { return time_range_.has_value(); }

   private:
    friend class CSSScrollTimeline;

    Element* source_;
    ScrollTimeline::ScrollDirection direction_;
    HeapVector<Member<ScrollTimelineOffset>>* offsets_;
    base::Optional<double> time_range_;
  };

  CSSScrollTimeline(Document*, const Options&);

  bool Matches(const Options&) const;

  // AnimationTimeline implementation.
  bool IsCSSScrollTimeline() const override { return true; }
};

template <>
struct DowncastTraits<CSSScrollTimeline> {
  static bool AllowFrom(const AnimationTimeline& value) {
    return value.IsCSSScrollTimeline();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_SCROLL_TIMELINE_H_
