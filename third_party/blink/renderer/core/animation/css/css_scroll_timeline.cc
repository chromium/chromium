// Copyright 2020 The Chromium Authors
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
      axis_(ComputeAxis(axis)),
      name_(name) {}

ScrollTimeline::ScrollAxis CSSScrollTimeline::Options::ComputeAxis(
    TimelineAxis axis) {
  switch (axis) {
    case TimelineAxis::kBlock:
      return ScrollAxis::kBlock;
    case TimelineAxis::kInline:
      return ScrollAxis::kInline;
    case TimelineAxis::kVertical:
      return ScrollAxis::kVertical;
    case TimelineAxis::kHorizontal:
      return ScrollAxis::kHorizontal;
  }

  NOTREACHED();
  return ScrollAxis::kBlock;
}

CSSScrollTimeline::CSSScrollTimeline(Document* document, Options&& options)
    : ScrollTimeline(document,
                     options.reference_type_,
                     options.reference_element_.value_or(
                         document->ScrollingElementNoLayout()),
                     options.axis_),
      name_(options.name_) {}

bool CSSScrollTimeline::Matches(const Options& options) const {
  return (GetReferenceType() == options.reference_type_) &&
         (ReferenceElement() == options.reference_element_) &&
         (GetAxis() == options.axis_) && (name_ == options.name_);
}

}  // namespace blink
