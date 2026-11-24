// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/gpu/GpuTypes.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"
#include "ui/gfx/color_space.h"

namespace blink {

// When enabled, unused resources (ready to be recycled) are reclaimed after a
// delay.
BASE_FEATURE(kCanvas2DReclaimUnusedResources,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether we add SHARED_IMAGE_USAGE_WEBGPU_READ by default to shared
// image backed CanvasResources so that they can be imported into WebGPU without
// an intermediate copy. This could cause a different shared image backing type
// to be used in the GPU process based on the OS platform.
BASE_FEATURE(kCanvasResourceIsWebGPUCompatible,
#if BUILDFLAG(IS_APPLE)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

CanvasResource::CanvasResource(
    scoped_refptr<gpu::ClientSharedImage> shared_image)
    : gpu::ClientImage(std::move(shared_image)) {
  subclass_manages_destruction_sync_token_ = true;
  if (!GetSharedImage()->is_software()) {
    // Wait for the mailbox to be ready to be used.
    WaitSyncToken(GetSharedImage()->creation_sync_token());
  }
}

CanvasResource::~CanvasResource() = default;

void CanvasResource::WaitSyncToken(const gpu::SyncToken& sync_token) {
  if (sync_token.HasData()) {
    acquire_sync_token_ = sync_token;
    GetSharedImage()->UpdateDestructionSyncToken(acquire_sync_token_);
  }
}

void CanvasResource::NotifyResourceLost() {
  resource_is_lost_ = true;
}

}  // namespace blink
