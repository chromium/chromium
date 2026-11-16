// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/paint_timing_utils.h"

#include "base/feature_list.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/media_timing.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink::paint_timing {

namespace {

BASE_FEATURE(kIgnoreDefaultVideoPosterForPaintTiming,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

bool ShouldIgnoreImageContentForPaintTiming(const LayoutObject& object,
                                            const MediaTiming* media_timing) {
  if (!base::FeatureList::IsEnabled(kIgnoreDefaultVideoPosterForPaintTiming)) {
    return false;
  }
  const auto* video = DynamicTo<HTMLVideoElement>(object.GetNode());
  if (!media_timing || media_timing->IsVideo() || !video) {
    return false;
  }
  KURL default_poster_url = video->DefaultPosterImageURL();
  return !default_poster_url.IsEmpty() &&
         default_poster_url == media_timing->Url();
}

void NotifyLoaderPerformanceTimingChanged(LocalDOMWindow* window) {
  if (!window) {
    return;
  }
  NotifyLoaderPerformanceTimingChanged(window->document());
}

void NotifyLoaderPerformanceTimingChanged(Document* document) {
  if (!document || !document->Loader()) {
    return;
  }
  document->Loader()->DidChangePerformanceTiming();
}

bool IsDocumentElementInvisible(const Document& document) {
  const Element* element = document.documentElement();
  return element && element->GetLayoutObject() &&
         element->GetLayoutObject()->Style().Opacity() == 0.0f;
}

}  // namespace blink::paint_timing
