// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/paint_timing_utils.h"

#include "base/feature_list.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
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
  if (!media_timing || media_timing->IsVideo() ||
      !IsA<HTMLVideoElement>(object.GetNode())) {
    return false;
  }
  const Settings* settings = object.GetDocument().GetSettings();
  if (!settings) {
    return false;
  }
  StringView default_poster =
      StripLeadingAndTrailingHtmlSpaces(settings->GetDefaultVideoPosterURL());
  if (default_poster.empty()) {
    return false;
  }
  return object.GetDocument().CompleteURL(default_poster) ==
         media_timing->Url();
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
         element->GetLayoutObject()->StyleRef().Opacity() == 0.0f;
}

}  // namespace blink::paint_timing
