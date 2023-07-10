// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"

#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/trees/render_frame_metadata_observer.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_flags.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"

namespace blink {

SimCompositor::SimCompositor() {
  last_frame_time_ = base::TimeTicks::Now();
}

SimCompositor::~SimCompositor() = default;

void SimCompositor::SetWebView(WebViewImpl& web_view) {
  web_view_ = &web_view;
}

SimCanvas::Commands SimCompositor::BeginFrame(double time_delta_in_seconds,
                                              bool raster) {
  DCHECK(web_view_);
  DCHECK(!LayerTreeHost()->defer_main_frame_update());
  // Verify that the need for a BeginMainFrame has been registered, and would
  // have caused the compositor to schedule one if we were using its scheduler.
  DCHECK(NeedsBeginFrame());
  DCHECK_GT(time_delta_in_seconds, 0);

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks start =
      last_frame_time_ + base::Seconds(time_delta_in_seconds);
  // Depending on the value of time_delta_in_seconds, `start` might be ahead of
  // the global clock, which can confuse LocalFrameUkmAggregator. So just sleep
  // until `start` is definitely in the past.
  base::PlatformThread::Sleep(start - now);
  last_frame_time_ = start;

  SimCanvas::Commands commands;
  paint_commands_ = &commands;

  LayerTreeHost()->CompositeForTest(last_frame_time_, raster,
                                    base::OnceClosure());

  paint_commands_ = nullptr;
  return commands;
}

SimCanvas::Commands SimCompositor::PaintFrame() {
  DCHECK(web_view_);

  if (!web_view_->MainFrameImpl())
    return SimCanvas::Commands();

  auto* frame = web_view_->MainFrameImpl()->GetFrame();
  auto* builder = MakeGarbageCollected<PaintRecordBuilder>();
  frame->View()->PaintOutsideOfLifecycleWithThrottlingAllowed(
      builder->Context(), PaintFlag::kOmitCompositingInfo);

  auto infinite_rect = InfiniteIntRect();
  SimCanvas canvas(infinite_rect.width(), infinite_rect.height());
  builder->EndRecording().Playback(&canvas);
  return canvas.GetCommands();
}

void SimCompositor::DidBeginMainFrame() {
  // Note that this will run *before* LocalFrameView::RunPostLifecycleSteps,
  // due to the calling conventions of PageWidgetDelegate::DidBeginFrame(). As a
  // result, frame throttling status has not been updated yet, and will be in
  // the same state as when the lifecycle steps ran.
  *paint_commands_ = PaintFrame();
}

void SimCompositor::SetLayerTreeHost(cc::LayerTreeHost* layer_tree_host) {
  layer_tree_host_ = layer_tree_host;
}

cc::LayerTreeHost* SimCompositor::LayerTreeHost() const {
  return layer_tree_host_;
}

}  // namespace blink
