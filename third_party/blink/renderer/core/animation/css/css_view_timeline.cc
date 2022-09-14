// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_view_timeline.h"
#include "third_party/blink/renderer/core/animation/css/css_scroll_timeline.h"

#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

CSSViewTimeline::Options::Options(Element* subject, TimelineAxis axis)
    : subject_(subject),
      direction_(CSSScrollTimeline::Options::ComputeScrollDirection(axis)) {}

CSSViewTimeline::CSSViewTimeline(Document* document, Options&& options)
    : ViewTimeline(document, options.subject_, options.direction_) {
  SnapshotState();
}

bool CSSViewTimeline::Matches(const Options& options) const {
  return (subject() == options.subject_) &&
         (GetOrientation() == options.direction_);
}

}  // namespace blink
