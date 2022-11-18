// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_view_timeline.h"
#include "third_party/blink/renderer/core/animation/css/css_scroll_timeline.h"

#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

CSSViewTimeline::Options::Options(Element* subject,
                                  TimelineAxis axis,
                                  TimelineInset inset)
    : subject_(subject),
      axis_(CSSScrollTimeline::Options::ComputeAxis(axis)),
      inset_(inset.GetStart(), inset.GetEnd()) {}

CSSViewTimeline::CSSViewTimeline(Document* document, Options&& options)
    : ViewTimeline(document, options.subject_, options.axis_, options.inset_) {}

bool CSSViewTimeline::Matches(const Options& options) const {
  return (subject() == options.subject_) && (GetAxis() == options.axis_) &&
         (GetInset() == options.inset_);
}

}  // namespace blink
