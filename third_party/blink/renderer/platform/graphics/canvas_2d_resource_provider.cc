// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_2d_resource_provider.h"

#include <inttypes.h>

#include <string>
#include <vector>

#include "base/byte_size.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/paint/decode_stashing_image_provider.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_op_buffer.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "skia/buildflags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_shared_image_interface_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_image_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/canvas_utils.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/gpu/GpuTypes.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"
#include "ui/gfx/skia_span_util.h"

namespace blink {

base::WeakPtr<Canvas2DResourceProvider>
Canvas2DResourceProvider::CreateWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void Canvas2DResourceProvider::OnResourceRefReturned(
    scoped_refptr<CanvasResourceSharedImage> resource) {
  if (!resource->IsLost() && resource->HasOneRef() &&
      resource_recycling_enabled_ && image_pool_) {
    image_pool_->ReleaseImage(std::move(resource));
  }
}

void Canvas2DResourceProvider::SetResourceRecyclingEnabled(bool value) {
  resource_recycling_enabled_ = value;
  if (!resource_recycling_enabled_) {
    ClearUnusedResources();
  }
}

Canvas2DResourceProvider::Canvas2DResourceProvider(
    std::unique_ptr<gpu::SharedImagePool<CanvasResourceSharedImage>> image_pool)
    : image_pool_(std::move(image_pool)) {
  CHECK(image_pool_);
}

Canvas2DResourceProvider::~Canvas2DResourceProvider() {
  UMA_HISTOGRAM_EXACT_LINEAR("Blink.Canvas.MaximumInflightResources",
                             max_inflight_resources_, 20);
}

void Canvas2DResourceProvider::ClearUnusedResources() {
  if (image_pool_) {
    image_pool_->Clear();
  }
}

}  // namespace blink
