// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/paint_timing_utils.h"

#include "base/feature_list.h"
#include "cc/trees/layer_tree_host.h"
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

cc::HeadsUpDisplayLayer* GetHUDLayer(LocalFrameView* frame_view) {
  if (!frame_view) {
    return nullptr;
  }

  if (auto* cc_layer = frame_view->RootCcLayer()) {
    if (auto* layer_tree_host = cc_layer->layer_tree_host()) {
      return layer_tree_host->hud_layer();
    }
  }
  return nullptr;
}

}  // namespace

cc::HeadsUpDisplayLayer* GetHUDLayerIfContentfulPaintRectsEnabled(
    LocalFrameView* frame_view) {
  cc::HeadsUpDisplayLayer* hud = GetHUDLayer(frame_view);
  if (hud && frame_view->RootCcLayer()
                 ->layer_tree_host()
                 ->GetDebugState()
                 .show_contentful_paint_rects) {
    return hud;
  }
  return nullptr;
}

cc::HeadsUpDisplayLayer* GetHUDLayerIfLayoutShiftRectsEnabled(
    LocalFrameView* frame_view) {
  cc::HeadsUpDisplayLayer* hud = GetHUDLayer(frame_view);
  if (hud && frame_view->RootCcLayer()
                 ->layer_tree_host()
                 ->GetDebugState()
                 .show_layout_shift_regions) {
    return hud;
  }
  return nullptr;
}

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

}  // namespace blink::paint_timing
