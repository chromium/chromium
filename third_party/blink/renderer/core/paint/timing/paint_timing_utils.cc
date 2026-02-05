// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/paint_timing_utils.h"

#include "cc/trees/layer_tree_host.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"

namespace blink::paint_timing {

namespace {

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

}  // namespace blink::paint_timing
