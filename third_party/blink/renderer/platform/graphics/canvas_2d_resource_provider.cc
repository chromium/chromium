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

BASE_FEATURE(kAppendCpuUsages, base::FEATURE_ENABLED_BY_DEFAULT);

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

base::WeakPtr<Canvas2DResourceProvider>
Canvas2DResourceProvider::CreateWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

scoped_refptr<CanvasResourceSharedImage>
Canvas2DResourceProvider::NewOrRecycledResource() {
  if (!image_pool_) {
    return nullptr;
  }

  auto resource = image_pool_->GetImage();
  if (!resource) {
    return nullptr;
  }

  CHECK(!IsSingleBuffered() || !resource->IsInitialized());

  if (!resource->IsInitialized()) {
    if (IsSoftware()) {
      resource->InitializeSoftware(
          CreateWeakPtr(), shared_image_interface_provider_, hdr_metadata_);
    } else {
      resource->Initialize(CreateWeakPtr(), context_provider_wrapper_,
                           hdr_metadata_, is_accelerated_);
    }
    ++num_inflight_resources_;
    if (num_inflight_resources_ > max_inflight_resources_) {
      max_inflight_resources_ = num_inflight_resources_;
    }
  }
  DCHECK(resource->HasOneRef());
  return resource;
}

void Canvas2DResourceProvider::OnResourceRefReturned(
    scoped_refptr<CanvasResourceSharedImage>&& resource) {
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
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    bool is_accelerated,
    gpu::SharedImageUsageSet shared_image_usage_flags)
    : is_accelerated_(is_accelerated),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      hdr_metadata_(hdr_metadata) {
  CHECK(context_provider_wrapper_);

  auto* sii =
      context_provider_wrapper_->ContextProvider().SharedImageInterface();
  CHECK(sii);

  // These SharedImages are both read and written by the raster interface
  // (both occur, for example, when copying canvas resources between
  // canvases). Additionally, these SharedImages can be put into
  // AcceleratedStaticBitmapImages (via Bitmap()) that are then copied into
  // GL textures by WebGL (via
  // AcceleratedStaticBitmapImage::CopyToTexture()).
  shared_image_usage_flags =
      shared_image_usage_flags | gpu::SHARED_IMAGE_USAGE_RASTER_READ |
      gpu::SHARED_IMAGE_USAGE_RASTER_WRITE | gpu::SHARED_IMAGE_USAGE_GLES2_READ;
  // Add WEBGPU_READ usage to allow importing into WebGPU without a copy.
  if (base::FeatureList::IsEnabled(kCanvasResourceIsWebGPUCompatible)) {
    shared_image_usage_flags |= gpu::SHARED_IMAGE_USAGE_WEBGPU_READ;
  }

  std::optional<gfx::BufferUsage> buffer_usage = std::nullopt;
  if (!is_accelerated_) {
    // Ideally we should add SHARED_IMAGE_USAGE_CPU_WRITE_ONLY to the shared
    // image usage flag here since mailbox will be used for CPU writes by
    // the client. But doing that stops us from using CompoundImagebacking
    // as many backings do not support SHARED_IMAGE_USAGE_CPU_WRITE_ONLY.
    // TODO(https://crbug.com/40280504): Add that usage flag back here once
    // the issue is resolved.
    buffer_usage = gfx::BufferUsage::SCANOUT_CPU_READ_WRITE;
    if (base::FeatureList::IsEnabled(kAppendCpuUsages)) {
      shared_image_usage_flags |= gpu::SHARED_IMAGE_USAGE_CPU_READ |
                                  gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY;
    }
  }

  gpu::ImageInfo image_info(size, format, shared_image_usage_flags, color_space,
                            kTopLeft_GrSurfaceOrigin, alpha_type, buffer_usage,
                            /*is_software=*/false);

  std::optional<base::TimeDelta> expiration_time =
      (base::FeatureList::IsEnabled(kCanvas2DReclaimUnusedResources))
          ? std::make_optional(
                Canvas2DResourceProvider::kUnusedResourceExpirationTime)
          : std::nullopt;
  bool is_single_buffered = shared_image_usage_flags.Has(
      gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE);

  image_pool_ = gpu::SharedImagePool<CanvasResourceSharedImage>::Create(
      image_info, sii,
      is_accelerated_ ? "CanvasResourceRaster" : "CanvasResourceRasterGmb",
      is_single_buffered ? 0 : kMaxRecycledCanvasResources, expiration_time);

  resource_ = NewOrRecycledResource();

  // Single buffered mode supported only for accelerated canvas.
  CHECK(!IsSingleBuffered() || is_accelerated_);
}

Canvas2DResourceProvider::Canvas2DResourceProvider(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider)
    : is_accelerated_(false),
      shared_image_interface_provider_(
          shared_image_interface_provider->GetWeakPtr()),
      hdr_metadata_(hdr_metadata) {
  CHECK(shared_image_interface_provider_);
  auto* sii = shared_image_interface_provider_->SharedImageInterface();
  CHECK(sii);
  gpu::ImageInfo image_info(
      size, format, gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY, color_space,
      kTopLeft_GrSurfaceOrigin, alpha_type, /*buffer_usage=*/std::nullopt,
      /*is_software=*/true);
  image_pool_ = gpu::SharedImagePool<CanvasResourceSharedImage>::Create(
      image_info, sii, "CanvasResourceSharedImage",
      kMaxRecycledCanvasResources);
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

bool Canvas2DResourceProvider::
    unused_resources_reclaim_timer_is_running_for_testing() const {
  return image_pool_ ? image_pool_->IsReclaimTimerRunningForTesting() : false;
}

bool Canvas2DResourceProvider::IsSingleBuffered() const {
  return image_pool_ && image_pool_->GetImageInfo().usage.Has(
                            gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE);
}

bool Canvas2DResourceProvider::HasUnusedResourcesForTesting() const {
  return image_pool_ && image_pool_->GetPoolSizeForTesting() > 0;
}

}  // namespace blink
