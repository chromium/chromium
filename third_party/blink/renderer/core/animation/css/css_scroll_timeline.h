// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_SCROLL_TIMELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_SCROLL_TIMELINE_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

class Document;
class Element;

// A CSSScrollTimeline is like a ScrollTimeline, except it originates from
// the scroll-timeline-* properties.
class CORE_EXPORT CSSScrollTimeline : public ScrollTimeline {
 public:
  struct Options {
    STACK_ALLOCATED();

   public:
    Options(Document&,
            ScrollTimeline::ReferenceType reference_type,
            absl::optional<Element*> reference_element,
            const AtomicString& name,
            TimelineAxis);

    static ScrollTimeline::ScrollDirection ComputeScrollDirection(TimelineAxis);

   private:
    friend class CSSScrollTimeline;

    ScrollTimeline::ReferenceType reference_type_;
    absl::optional<Element*> reference_element_;
    ScrollTimeline::ScrollDirection direction_;
    AtomicString name_;
  };

  CSSScrollTimeline(Document*, Options&&);

  const AtomicString& Name() const { return name_; }

  bool Matches(const Options&) const;

  // AnimationTimeline implementation.
  bool IsCSSScrollTimeline() const override { return true; }

 private:
  AtomicString name_;
};

template <>
struct DowncastTraits<CSSScrollTimeline> {
  static bool AllowFrom(const AnimationTimeline& value) {
    return value.IsCSSScrollTimeline();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_SCROLL_TIMELINE_H_
