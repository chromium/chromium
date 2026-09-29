// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/web_vitals_hud_helper.h"

#include "cc/layers/heads_up_display_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

namespace {

cc::HeadsUpDisplayLayer* GetHUDLayer(LocalFrameView* frame_view) {
  if (auto* cc_layer = frame_view->RootCcLayer()) {
    if (auto* layer_tree_host = cc_layer->layer_tree_host()) {
      return layer_tree_host->hud_layer();
    }
  }
  return nullptr;
}

bool IsMetricEnabled(cc::WebVitalMetricType metric_type,
                     LocalFrameView* frame_view) {
  auto* cc_layer = frame_view->RootCcLayer();
  if (!cc_layer) {
    return false;
  }
  auto* layer_tree_host = cc_layer->layer_tree_host();
  if (!layer_tree_host) {
    return false;
  }
  const auto& debug_state = layer_tree_host->GetDebugState();
  switch (metric_type) {
    case cc::WebVitalMetricType::kLayoutShift:
      return debug_state.show_layout_shift_regions;
    case cc::WebVitalMetricType::kNavigationContentfulPaint:
    case cc::WebVitalMetricType::kInteractionContentfulPaint:
      return debug_state.show_contentful_paint_rects;
  }
}

}  // namespace

WebVitalsHudHelper::WebVitalsHudHelper(cc::WebVitalMetricType metric_type,
                                       LocalFrameView* frame_view)
    : metric_type_(metric_type) {
  CHECK(frame_view);
  if (!IsMetricEnabled(metric_type, frame_view)) {
    return;
  }
  hud_layer_ = GetHUDLayer(frame_view);
  if (!hud_layer_) {
    return;
  }
  widget_ = frame_view->GetFrame().GetWidgetForLocalRoot();
}

void WebVitalsHudHelper::AddWebVitalsDebugRect(
    const PaintTimingRecord& record) {
  if (!IsEnabled()) {
    return;
  }
  CHECK(widget_);
  hud_layer_->AddWebVitalsDebugRect(
      {metric_type_, gfx::ToEnclosedRect(
                         widget_->DIPsToBlinkSpace(record.RootVisualRect()))});
}

void WebVitalsHudHelper::AddWebVitalsDebugRect(const gfx::Rect& rect) {
  if (!IsEnabled()) {
    return;
  }
  hud_layer_->AddWebVitalsDebugRect({metric_type_, rect});
}

}  // namespace blink
