// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_shared_image_wrapper.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/function_ref.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/paint/display_item_list.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_feature_type.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_image_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/canvas_utils.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/instrumentation/canvas_memory_dump_provider.h"
#include "third_party/skia/include/core/SkAlphaType.h"

namespace blink {

WebGpuSharedImageWrapper::WebGpuSharedImageWrapper(
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper)
    : recorder_for_external_draws_(
          std::make_unique<MemoryManagedPaintRecorder>(shared_image->size(),
                                                       /*client=*/nullptr)),
      shared_image_(std::move(shared_image)),
      context_provider_wrapper_(std::move(context_provider_wrapper)) {
  // Graphite can handle a large buffer size.
  if (context_provider_wrapper_->ContextProvider()
          .GetGpuFeatureInfo()
          .status_values[gpu::GPU_FEATURE_TYPE_SKIA_GRAPHITE] ==
      gpu::kGpuFeatureStatusEnabled) {
    recorder_for_external_draws_->DisableLineDrawingAsPaths();
  }

  CHECK(shared_image_);
  WaitSyncToken(shared_image_->creation_sync_token());
}

WebGpuSharedImageWrapper::~WebGpuSharedImageWrapper() = default;

void WebGpuSharedImageWrapper::WaitSyncToken(const gpu::SyncToken& sync_token) {
  if (sync_token.HasData()) {
    sync_token_ = sync_token;
    shared_image_->UpdateDestructionSyncToken(sync_token_);
  }
}

}  // namespace blink
