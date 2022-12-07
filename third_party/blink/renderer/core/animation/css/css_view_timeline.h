// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_VIEW_TIMELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_VIEW_TIMELINE_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/animation/view_timeline.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/scoped_css_name.h"
#include "third_party/blink/renderer/core/style/timeline_inset.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"

namespace blink {

class Document;
class Element;

// A CSSViewTimeline is like a ViewTimeline, except it originates from
// the view-timeline-* properties.
class CORE_EXPORT CSSViewTimeline : public ViewTimeline {
 public:
  struct Options {
    STACK_ALLOCATED();

   public:
    Options(Element* subject, TimelineAxis, TimelineInset);

   private:
    friend class CSSViewTimeline;

    Element* subject_;
    ScrollAxis axis_;
    ViewTimeline::Inset inset_;
  };

  CSSViewTimeline(Document*, Options&&);

  bool Matches(const Options&) const;
};

using CSSViewTimelineMap =
    HeapHashMap<Member<const ScopedCSSName>, Member<CSSViewTimeline>>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_VIEW_TIMELINE_H_
