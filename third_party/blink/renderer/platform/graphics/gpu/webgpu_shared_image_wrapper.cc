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

namespace {

scoped_refptr<gpu::ClientSharedImage> CreateClientSharedImage(
    viz::SharedImageFormat format,
    gfx::Size size,
    const gfx::ColorSpace& color_space,
    SkAlphaType alpha_type,
    WebGraphicsContext3DProviderWrapper* context_provider_wrapper) {
  auto* sii =
      context_provider_wrapper->ContextProvider().SharedImageInterface();
  // The SharedImages created by this provider serve as a means of
  // import/export between VideoFrames/canvas and WebGPU, e.g.:
  // * Import from VideoFrames into WebGPU via CreateExternalTexture() (the
  //   WebGPU textures will then be read by clients)
  // * Export from WebGPU into a static bitmap image via
  //   GpuCanvasContext::{PaintRenderingResultsToSnapshot, GetImage}() (the
  //   export happens via the WebGPU interface)
  // Hence, both WEBGPU_READ and WEBGPU_WRITE usage are needed here.
  // Additionally, these SharedImages are both read and written by the
  // raster interface (both occur, for example, when copying canvas
  // resources between canvases) and can be put into
  // AcceleratedStaticBitmapImages (via Bitmap()) that are then copied into
  // GL textures by WebGL (via
  // AcceleratedStaticBitmapImage::CopyToTexture()).
  gpu::SharedImageUsageSet shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_WEBGPU_READ |
      gpu::SHARED_IMAGE_USAGE_WEBGPU_WRITE |
      gpu::SHARED_IMAGE_USAGE_RASTER_READ |
      gpu::SHARED_IMAGE_USAGE_RASTER_WRITE | gpu::SHARED_IMAGE_USAGE_GLES2_READ;

  return sii->CreateSharedImage(
      {format, size, color_space, kTopLeft_GrSurfaceOrigin, alpha_type,
       shared_image_usage_flags, "CanvasResourceRaster"},
      gpu::kNullSurfaceHandle);
}

}  // namespace

std::unique_ptr<WebGpuSharedImageWrapper> WebGpuSharedImageWrapper::Create(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata) {
  auto context_provider_wrapper = SharedGpuContext::ContextProviderWrapper();

  // IsGpuCompositingEnabled can re-create the context if it has been lost, do
  // this up front so that we can fail early and not expose ourselves to
  // use after free bugs (crbug.com/1126424)
  std::ignore = SharedGpuContext::IsGpuCompositingEnabled();

  // If the context is lost we don't want to re-create it here, the resulting
  // resource provider would be invalid anyway
  if (!context_provider_wrapper ||
      !context_provider_wrapper->ContextProvider().RasterInterface() ||
      context_provider_wrapper->ContextProvider().IsContextLost()) {
    return nullptr;
  }

  const auto& capabilities =
      context_provider_wrapper->ContextProvider().GetCapabilities();
  if ((size.width() < 1 || size.height() < 1 ||
       size.width() > capabilities.max_texture_size ||
       size.height() > capabilities.max_texture_size)) {
    return nullptr;
  }

#if BUILDFLAG(IS_LINUX)
  // WebGpu preferred canvas on linux is RGBA and interop (vk on gl) is
  // dependent on canvas copies being RGBA (not BGRA).
  if (format != viz::SinglePlaneFormat::kRGBA_F16) {
    format = viz::SinglePlaneFormat::kRGBA_8888;
  }
#endif

  auto provider = base::WrapUnique(
      new WebGpuSharedImageWrapper(size, format, alpha_type, color_space,
                                   hdr_metadata, context_provider_wrapper));

  if (provider->IsGpuContextLost()) {
    return nullptr;
  }
  return provider;
}

WebGpuSharedImageWrapper::WebGpuSharedImageWrapper(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper)
    : hdr_metadata_(hdr_metadata),
      recorder_for_external_draws_(
          std::make_unique<MemoryManagedPaintRecorder>(size,
                                                       /*client=*/nullptr)),
      shared_image_(CreateClientSharedImage(format,
                                            size,
                                            color_space,
                                            alpha_type,
                                            context_provider_wrapper.get())),
      context_provider_wrapper_(std::move(context_provider_wrapper)) {
  CanvasMemoryDumpProvider::Instance()->RegisterClient(this);
  // Graphite can handle a large buffer size.
  if (context_provider_wrapper_->ContextProvider()
          .GetGpuFeatureInfo()
          .status_values[gpu::GPU_FEATURE_TYPE_SKIA_GRAPHITE] ==
      gpu::kGpuFeatureStatusEnabled) {
    recorder_for_external_draws_->DisableLineDrawingAsPaths();
  }

  CHECK(shared_image_);
  WaitSyncToken(shared_image_->creation_sync_token());
  release_sync_token_ = shared_image_->creation_sync_token();
}

WebGpuSharedImageWrapper::~WebGpuSharedImageWrapper() {
  CanvasMemoryDumpProvider::Instance()->UnregisterClient(this);
}

void WebGpuSharedImageWrapper::WaitSyncToken(const gpu::SyncToken& sync_token) {
  if (sync_token.HasData()) {
    acquire_sync_token_ = sync_token;
    shared_image_->UpdateDestructionSyncToken(acquire_sync_token_);
  }
}

gpu::raster::RasterInterface* WebGpuSharedImageWrapper::RasterInterface()
    const {
  if (!context_provider_wrapper_) {
    return nullptr;
  }
  return context_provider_wrapper_->ContextProvider().RasterInterface();
}

bool WebGpuSharedImageWrapper::IsGpuContextLost() const {
  auto* raster_interface = RasterInterface();
  return !raster_interface ||
         raster_interface->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
}




scoped_refptr<gpu::ClientSharedImage> WebGpuSharedImageWrapper::GetSharedImage()
    const {
  if (IsGpuContextLost()) {
    return nullptr;
  }
  return shared_image_;
}

gpu::SyncToken WebGpuSharedImageWrapper::GetSyncToken() const {
  if (IsGpuContextLost()) {
    return gpu::SyncToken();
  }
  return release_sync_token_;
}


void WebGpuSharedImageWrapper::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd) {
  std::string path = base::StringPrintf("canvas/ResourceProvider_0x%" PRIXPTR,
                                        reinterpret_cast<uintptr_t>(this));

  std::string dump_name =
      base::StringPrintf("%s/CanvasResource_0x%" PRIXPTR, path.c_str(),
                         reinterpret_cast<uintptr_t>(this));
  auto* dump = pmd->CreateAllocatorDump(dump_name);
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  GetSize());

  shared_image_->OnMemoryDump(
      pmd, dump->guid(),
      static_cast<int>(gpu::TracingImportance::kClientOwner));
}

size_t WebGpuSharedImageWrapper::GetSize() const {
  return base::checked_cast<size_t>(
      shared_image_->EstimatedSizeInBytes().InBytes());
}

}  // namespace blink
