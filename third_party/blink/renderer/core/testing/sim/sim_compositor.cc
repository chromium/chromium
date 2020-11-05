// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"

#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/trees/render_frame_metadata_observer.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"

namespace blink {

SimCompositor::SimCompositor() {
  last_frame_time_ = base::TimeTicks::Now();
}

SimCompositor::~SimCompositor() = default;

void SimCompositor::SetWebView(
    WebViewImpl& web_view,
    frame_test_helpers::TestWebViewClient& view_client) {
  web_view_ = &web_view;
  test_web_view_client_ = &view_client;
}

SimCanvas::Commands SimCompositor::BeginFrame(double time_delta_in_seconds,
                                              bool raster) {
  DCHECK(web_view_);
  DCHECK(!layer_tree_host()->defer_main_frame_update());
  // Verify that the need for a BeginMainFrame has been registered, and would
  // have caused the compositor to schedule one if we were using its scheduler.
  DCHECK(NeedsBeginFrame());
  DCHECK_GT(time_delta_in_seconds, 0);

  ClearAnimationScheduled();

  last_frame_time_ += base::TimeDelta::FromSecondsD(time_delta_in_seconds);

  SimCanvas::Commands commands;
  paint_commands_ = &commands;

  layer_tree_host()->Composite(last_frame_time_, raster);

  paint_commands_ = nullptr;
  return commands;
}

SimCanvas::Commands SimCompositor::PaintFrame() {
  DCHECK(web_view_);

  if (!web_view_->MainFrameImpl())
    return SimCanvas::Commands();

  auto* frame = web_view_->MainFrameImpl()->GetFrame();
  PaintRecordBuilder builder;
  frame->View()->PaintOutsideOfLifecycle(builder.Context(),
                                         kGlobalPaintFlattenCompositingLayers);

  auto infinite_rect = LayoutRect::InfiniteIntRect();
  SimCanvas canvas(infinite_rect.Width(), infinite_rect.Height());
  builder.EndRecording()->Playback(&canvas);
  return canvas.GetCommands();
}

void SimCompositor::DidBeginMainFrame() {
  // Note that this will run *before* LocalFrameView::RunPostLifecycleSteps,
  // due to the calling conventions of PageWidgetDelegate::DidBeginFrame(). As a
  // result, frame throttling status has not been updated yet, and will be in
  // the same state as when the lifecycle steps ran.
  *paint_commands_ = PaintFrame();
}

}  // namespace blink
