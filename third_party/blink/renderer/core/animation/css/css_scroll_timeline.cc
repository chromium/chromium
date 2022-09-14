// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_scroll_timeline.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

CSSScrollTimeline::Options::Options(
    Document& document,
    ScrollTimeline::ReferenceType reference_type,
    absl::optional<Element*> reference_element,
    const AtomicString& name,
    TimelineAxis axis)
    : reference_type_(reference_type),
      reference_element_(reference_element),
      direction_(ComputeScrollDirection(axis)),
      name_(name) {}

ScrollTimeline::ScrollDirection
CSSScrollTimeline::Options::ComputeScrollDirection(TimelineAxis axis) {
  using ScrollDirection = ScrollTimeline::ScrollDirection;

  switch (axis) {
    case TimelineAxis::kBlock:
      return ScrollDirection::kBlock;
    case TimelineAxis::kInline:
      return ScrollDirection::kInline;
    case TimelineAxis::kVertical:
      return ScrollDirection::kVertical;
    case TimelineAxis::kHorizontal:
      return ScrollDirection::kHorizontal;
  }

  NOTREACHED();
  return ScrollDirection::kBlock;
}

CSSScrollTimeline::CSSScrollTimeline(Document* document, Options&& options)
    : ScrollTimeline(document,
                     options.reference_type_,
                     options.reference_element_.value_or(
                         document->ScrollingElementNoLayout()),
                     options.direction_),
      name_(options.name_) {
  SnapshotState();
}

bool CSSScrollTimeline::Matches(const Options& options) const {
  return (GetReferenceType() == options.reference_type_) &&
         (ReferenceElement() == options.reference_element_) &&
         (GetOrientation() == options.direction_) && (name_ == options.name_);
}

}  // namespace blink
