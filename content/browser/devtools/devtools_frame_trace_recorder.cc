// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_frame_trace_recorder.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/trace_event_impl.h"
#include "build/build_config.h"
#include "cc/trees/render_frame_metadata.h"
#include "content/browser/devtools/devtools_traceable_screenshot.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace content {

namespace {

static size_t kFrameAreaLimit = 256000;

void FrameCaptured(base::TimeTicks timestamp, const SkBitmap& bitmap) {
  if (bitmap.drawsNothing())
    return;
  if (DevToolsTraceableScreenshot::GetNumberOfInstances() >=
      DevToolsTraceableScreenshot::kMaximumNumberOfScreenshots) {
    return;
  }
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID_AND_TIMESTAMP(
      TRACE_DISABLED_BY_DEFAULT("devtools.screenshot"), "Screenshot", 1,
      timestamp,
      std::unique_ptr<base::trace_event::ConvertableToTraceFormat>(
          new DevToolsTraceableScreenshot(bitmap)));
}

void CaptureFrame(RenderFrameHostImpl* host,
                  const cc::RenderFrameMetadata& metadata) {
  RenderWidgetHostViewBase* view =
      static_cast<RenderWidgetHostViewBase*>(host->GetView());
  if (!view)
    return;
  if (DevToolsTraceableScreenshot::GetNumberOfInstances() >=
      DevToolsTraceableScreenshot::kMaximumNumberOfScreenshots) {
    return;
  }

  gfx::Size predicted_bitmap_size = gfx::ToCeiledSize(gfx::ScaleSize(
      metadata.scrollable_viewport_size, metadata.page_scale_factor));
  gfx::Size snapshot_size;
  float area = predicted_bitmap_size.GetArea();
  if (area <= kFrameAreaLimit) {
    snapshot_size = predicted_bitmap_size;
  } else {
    double scale = sqrt(kFrameAreaLimit / area);
    snapshot_size = gfx::ScaleToCeiledSize(predicted_bitmap_size, scale);
  }

  view->CopyFromSurface(gfx::Rect(), snapshot_size,
                        base::BindOnce(FrameCaptured, base::TimeTicks::Now()));
}

bool ScreenshotCategoryEnabled() {
  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("devtools.screenshot"), &enabled);
  return enabled;
}

}  // namespace

DevToolsFrameTraceRecorder::DevToolsFrameTraceRecorder() { }

DevToolsFrameTraceRecorder::~DevToolsFrameTraceRecorder() { }

void DevToolsFrameTraceRecorder::OnSynchronousSwapCompositorFrame(
    RenderFrameHostImpl* host,
    const cc::RenderFrameMetadata& metadata) {
  if (!host || !ScreenshotCategoryEnabled()) {
    return;
  }

  bool is_new_trace;
  TRACE_EVENT_IS_NEW_TRACE(&is_new_trace);
  if (!is_new_trace)
    CaptureFrame(host, metadata);
}

}  // namespace content
