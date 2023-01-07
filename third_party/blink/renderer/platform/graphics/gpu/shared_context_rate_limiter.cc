// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/shared_context_rate_limiter.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/extensions_3d_util.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace blink {

SharedContextRateLimiter::SharedContextRateLimiter(unsigned max_pending_ticks)
    : max_pending_ticks_(max_pending_ticks), can_use_sync_queries_(false) {
  context_provider_ =
      Platform::Current()->CreateSharedOffscreenGraphicsContext3DProvider();
  if (!context_provider_)
    return;

  gpu::raster::RasterInterface* raster_interface =
      context_provider_->RasterInterface();
  if (raster_interface &&
      raster_interface->GetGraphicsResetStatusKHR() == GL_NO_ERROR) {
    const auto& gpu_capabilities = context_provider_->GetCapabilities();
    // TODO(junov): when the GLES 3.0 command buffer is ready, we could use
    // fenceSync instead.
    can_use_sync_queries_ = gpu_capabilities.sync_query;
  }
}

void SharedContextRateLimiter::Tick() {
  TRACE_EVENT0("blink", "SharedContextRateLimiter::Tick");
  if (!context_provider_)
    return;

  gpu::raster::RasterInterface* raster_interface =
      context_provider_->RasterInterface();
  if (!raster_interface ||
      raster_interface->GetGraphicsResetStatusKHR() != GL_NO_ERROR)
    return;

  queries_.push_back(0);
  if (can_use_sync_queries_) {
    raster_interface->GenQueriesEXT(1, &queries_.back());
    raster_interface->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM,
                                    queries_.back());
    raster_interface->EndQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM);
  }
  if (queries_.size() > max_pending_ticks_) {
    if (can_use_sync_queries_) {
      TRACE_EVENT0("blink",
                   "GPU backpressure via GL_COMMANDS_COMPLETED_CHROMIUM");
      GLuint result;
      raster_interface->GetQueryObjectuivEXT(queries_.front(),
                                             GL_QUERY_RESULT_EXT, &result);
      raster_interface->DeleteQueriesEXT(1, &queries_.front());
      queries_.pop_front();
    } else {
      TRACE_EVENT0("blink", "GPU backpressure via RasterInterface::Finish");
      raster_interface->Finish();
      Reset();
    }
  }
}

void SharedContextRateLimiter::Reset() {
  if (!context_provider_)
    return;

  gpu::raster::RasterInterface* raster_interface =
      context_provider_->RasterInterface();
  if (can_use_sync_queries_ && raster_interface &&
      raster_interface->GetGraphicsResetStatusKHR() == GL_NO_ERROR) {
    while (!queries_.empty()) {
      raster_interface->DeleteQueriesEXT(1, &queries_.front());
      queries_.pop_front();
    }
  } else {
    queries_.clear();
  }
}

}  // namespace blink
