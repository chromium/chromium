// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_non_2d_resource_provider.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/function_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "cc/paint/skia_paint_canvas.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_feature_type.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_graphics_shared_image_interface_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_image_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/canvas_utils.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/instrumentation/canvas_memory_dump_provider.h"
#include "third_party/skia/include/core/SkAlphaType.h"

namespace blink {

base::WeakPtr<CanvasNon2DResourceProvider>
CanvasNon2DResourceProvider::CreateWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::unique_ptr<CanvasNon2DResourceProvider>
CanvasNon2DResourceProvider::Create(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    gpu::SharedImageUsageSet shared_image_usage_flags,
    CanvasResourceProviderDelegate* delegate) {
  // IsGpuCompositingEnabled can re-create the context if it has been lost, do
  // this up front so that we can fail early and not expose ourselves to
  // use after free bugs (crbug.com/1126424)
  const bool is_gpu_compositing_enabled =
      SharedGpuContext::IsGpuCompositingEnabled();

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

  // TODO(crbug.com/40767377): Pass in info as is for all cases.
  // Overriding the info to use RGBA instead of N32 is needed because code
  // elsewhere assumes RGBA. OTOH the software path seems to be assuming N32
  // somewhere in the later pipeline but for offscreen canvas only.
  bool should_force_bgra8_to_rgba =
      !shared_image_usage_flags.HasAny(gpu::SHARED_IMAGE_USAGE_WEBGPU_READ |
                                       gpu::SHARED_IMAGE_USAGE_WEBGPU_WRITE);
#if BUILDFLAG(IS_WIN)
  // Concurrent read/write on Windows results in a swapchain backing, which
  // supports BGRA; hence there is no need to force to RGBA in this case.
  should_force_bgra8_to_rgba =
      should_force_bgra8_to_rgba &&
      !shared_image_usage_flags.Has(
          gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE);
#endif

#if BUILDFLAG(IS_LINUX)
  // WebGpu preferred canvas on linux is RGBA and interop (vk on gl) is
  // dependent on canvas copies being RGBA (not BGRA).
  should_force_bgra8_to_rgba = true;
#endif

  if (format != viz::SinglePlaneFormat::kRGBA_F16 &&
      should_force_bgra8_to_rgba) {
    format = viz::SinglePlaneFormat::kRGBA_8888;
  }

  const bool is_mappable_shared_image_allowed =
      is_gpu_compositing_enabled &&
      IsScanoutSupportedForCanvasWithFormat(format, capabilities);

  // If we cannot use overlay, we have to remove the scanout flag and the
  // concurrent read write flag.
  const auto& shared_image_caps = context_provider_wrapper->ContextProvider()
                                      .SharedImageInterface()
                                      ->GetCapabilities();
  bool is_overlay_supported = is_mappable_shared_image_allowed &&
                              shared_image_caps.supports_scanout_shared_images;

#if BUILDFLAG(IS_WIN)
  // On Windows, SCANOUT usage is additionally supported in the special case
  // of the swapchain being used on the service side to implement concurrent
  // read/write.
  is_overlay_supported = is_overlay_supported ||
                         (shared_image_usage_flags.Has(
                              gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE) &&
                          shared_image_caps.shared_image_swap_chain);
#endif

  if (!is_overlay_supported) {
    shared_image_usage_flags.RemoveAll(
        gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE |
        gpu::SHARED_IMAGE_USAGE_SCANOUT);
  }

#if BUILDFLAG(IS_MAC)
  if (shared_image_usage_flags.Has(gpu::SHARED_IMAGE_USAGE_SCANOUT) &&
      format == viz::SinglePlaneFormat::kRGBA_8888) {
    // GPU-accelerated scannout usage on Mac uses IOSurface.  Must switch from
    // RGBA_8888 to BGRA_8888 in that case.
    format = viz::SinglePlaneFormat::kBGRA_8888;
  }
#endif

  auto provider = std::make_unique<CanvasNon2DResourceProvider>(
      size, format, alpha_type, color_space, hdr_metadata,
      context_provider_wrapper, shared_image_usage_flags, delegate);

  return provider->IsValid() ? std::move(provider) : nullptr;
}

std::unique_ptr<CanvasNon2DResourceProvider>
CanvasNon2DResourceProvider::Create(
    gfx::Size size,
    const Canvas2DColorParams& color_params,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    gpu::SharedImageUsageSet shared_image_usage_flags) {
  return Create(size, color_params.GetSharedImageFormat(),
                color_params.GetAlphaType(), color_params.GetGfxColorSpace(),
                color_params.GetGfxHdrMetadata(),
                std::move(context_provider_wrapper), shared_image_usage_flags);
}

std::unique_ptr<CanvasNon2DResourceProvider>
CanvasNon2DResourceProvider::CreateForWebGPU(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    gpu::SharedImageUsageSet shared_image_usage_flags,
    CanvasResourceProviderDelegate* delegate) {
  auto context_provider_wrapper = SharedGpuContext::ContextProviderWrapper();
  // The SharedImages created by this provider serve as a means of import/export
  // between VideoFrames/canvas and WebGPU, e.g.:
  // * Import from VideoFrames into WebGPU via CreateExternalTexture() (the
  //   WebGPU textures will then be read by clients)
  // * Export from WebGPU into a static bitmap image via
  //   GpuCanvasContext::{PaintRenderingResultsToSnapshot, GetImage}() (the
  //   export happens via the WebGPU interface)
  // Hence, both WEBGPU_READ and WEBGPU_WRITE usage are needed here.
  return CanvasNon2DResourceProvider::Create(
      size, format, alpha_type, color_space, hdr_metadata,
      std::move(context_provider_wrapper),
      shared_image_usage_flags | gpu::SHARED_IMAGE_USAGE_WEBGPU_READ |
          gpu::SHARED_IMAGE_USAGE_WEBGPU_WRITE,
      delegate);
}

std::unique_ptr<CanvasNon2DResourceProvider>
CanvasNon2DResourceProvider::CreateForSoftwareCompositor(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider,
    CanvasResourceProviderDelegate* delegate) {
  if (SharedGpuContext::IsGpuCompositingEnabled()) {
    return nullptr;
  }

  CHECK(format == viz::SharedImageFormat::N32Format() ||
        format == viz::SinglePlaneFormat::kRGBA_F16);

  auto provider = std::make_unique<CanvasNon2DResourceProvider>(
      size, format, alpha_type, color_space, hdr_metadata,
      shared_image_interface_provider, delegate);
  return provider->IsValid() ? std::move(provider) : nullptr;
}

std::unique_ptr<CanvasNon2DResourceProvider>
CanvasNon2DResourceProvider::CreateForSoftwareCompositor(
    gfx::Size size,
    const Canvas2DColorParams& color_params,
    WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider) {
  return CreateForSoftwareCompositor(
      size, color_params.GetSharedImageFormat(), color_params.GetAlphaType(),
      color_params.GetGfxColorSpace(), color_params.GetGfxHdrMetadata(),
      shared_image_interface_provider);
}

CanvasNon2DResourceProvider::CanvasNon2DResourceProvider(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    gpu::SharedImageUsageSet shared_image_usage_flags,
    CanvasResourceProviderDelegate* delegate)
    : size_(size),
      format_(format),
      alpha_type_(alpha_type),
      color_space_(color_space),
      hdr_metadata_(hdr_metadata),
      delegate_(delegate),
      is_software_(false),
      snapshot_paint_image_id_(cc::PaintImage::GetNextId()),
      recorder_for_external_draws_(
          std::make_unique<MemoryManagedPaintRecorder>(Size(),
                                                       /*client=*/nullptr)),
      context_provider_wrapper_(std::move(context_provider_wrapper)) {
  CanvasMemoryDumpProvider::Instance()->RegisterClient(this);
  if (context_provider_wrapper_) {
    context_provider_wrapper_->AddObserver(this);
    raster_context_provider_ = base::WrapRefCounted(
        context_provider_wrapper_->ContextProvider().RasterContextProvider());
    // Graphite can handle a large buffer size.
    if (context_provider_wrapper_->ContextProvider()
            .GetGpuFeatureInfo()
            .status_values[gpu::GPU_FEATURE_TYPE_SKIA_GRAPHITE] ==
        gpu::kGpuFeatureStatusEnabled) {
      recorder_for_external_draws_->DisableLineDrawingAsPaths();
    }
  }

  if (raster_context_provider_) {
    raster_context_provider_->AddObserver(this);
  }

  if (context_provider_wrapper_) {
    if (auto* sii = context_provider_wrapper_->ContextProvider()
                        .SharedImageInterface()) {
      // These SharedImages are both read and written by the raster interface
      // (both occur, for example, when copying canvas resources between
      // canvases). Additionally, these SharedImages can be put into
      // AcceleratedStaticBitmapImages (via Bitmap()) that are then copied into
      // GL textures by WebGL (via
      // AcceleratedStaticBitmapImage::CopyToTexture()).
      shared_image_usage_flags = shared_image_usage_flags |
                                 gpu::SHARED_IMAGE_USAGE_RASTER_READ |
                                 gpu::SHARED_IMAGE_USAGE_RASTER_WRITE |
                                 gpu::SHARED_IMAGE_USAGE_GLES2_READ;
      // Add WEBGPU_READ usage to allow importing into WebGPU without a copy.
      if (base::FeatureList::IsEnabled(kCanvasResourceIsWebGPUCompatible)) {
        shared_image_usage_flags |= gpu::SHARED_IMAGE_USAGE_WEBGPU_READ;
      }

      std::optional<gfx::BufferUsage> buffer_usage = std::nullopt;
      if (is_software_) {
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

      gpu::ImageInfo image_info(size, format, shared_image_usage_flags,
                                color_space, kTopLeft_GrSurfaceOrigin,
                                alpha_type, buffer_usage,
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
          !is_software_ ? "CanvasResourceRaster" : "CanvasResourceRasterGmb",
          is_single_buffered ? 0 : kMaxRecycledCanvasResources,
          expiration_time);
    }
  }

  resource_ = NewOrRecycledResource();
  FlushForImageListener::Get()->AddObserver(this);

  if (resource_) {
    EnsureWriteAccess();
  }
}

CanvasNon2DResourceProvider::CanvasNon2DResourceProvider(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider,
    CanvasResourceProviderDelegate* delegate)
    : size_(size),
      format_(format),
      alpha_type_(alpha_type),
      color_space_(color_space),
      hdr_metadata_(hdr_metadata),
      delegate_(delegate),
      is_software_(true),
      snapshot_paint_image_id_(cc::PaintImage::GetNextId()),
      recorder_for_external_draws_(
          std::make_unique<MemoryManagedPaintRecorder>(Size(),
                                                       /*client=*/nullptr)),
      shared_image_interface_provider_(
          shared_image_interface_provider
              ? shared_image_interface_provider->GetWeakPtr()
              : nullptr) {
  CanvasMemoryDumpProvider::Instance()->RegisterClient(this);
  if (shared_image_interface_provider_) {
    shared_image_interface_provider_->AddGpuChannelLostObserver(this);
    if (auto* sii = shared_image_interface_provider_->SharedImageInterface()) {
      gpu::ImageInfo image_info(
          size, format, gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY, color_space,
          kTopLeft_GrSurfaceOrigin, alpha_type, /*buffer_usage=*/std::nullopt,
          /*is_software=*/true);
      image_pool_ = gpu::SharedImagePool<CanvasResourceSharedImage>::Create(
          image_info, sii, "CanvasResourceSharedImage",
          kMaxRecycledCanvasResources);
    }
  }
}

CanvasNon2DResourceProvider::~CanvasNon2DResourceProvider() {
  CanvasMemoryDumpProvider::Instance()->UnregisterClient(this);
  if (context_provider_wrapper_) {
    context_provider_wrapper_->RemoveObserver(this);
  }
  if (raster_context_provider_) {
    raster_context_provider_->RemoveObserver(this);
  }
  if (shared_image_interface_provider_) {
    shared_image_interface_provider_->RemoveGpuChannelLostObserver(this);
  }

  if (!is_software_) {
    FlushForImageListener::Get()->RemoveObserver(this);
  }

  // Last chance for outstanding GPU timers to record metrics.
  if (RasterInterface()) {
    CheckGpuTimers(RasterInterface());
  }

  UMA_HISTOGRAM_EXACT_LINEAR("Blink.Canvas.MaximumInflightResources",
                             max_inflight_resources_, 20);
}

scoped_refptr<CanvasResourceSharedImage>
CanvasNon2DResourceProvider::NewOrRecycledResource() {
  if (!image_pool_) {
    return nullptr;
  }

  auto resource = image_pool_->GetImage();
  if (!resource) {
    return nullptr;
  }

  CHECK(!IsSingleBuffered() || !resource->IsInitialized());

  if (!resource->IsInitialized()) {
    if (image_pool_->GetImageInfo().is_software) {
      resource->InitializeSoftware(
          CreateWeakPtr(), shared_image_interface_provider_, hdr_metadata_);
    } else {
      resource->Initialize(CreateWeakPtr(), context_provider_wrapper_,
                           hdr_metadata_, !is_software_);
    }
    ++num_inflight_resources_;
    if (num_inflight_resources_ > max_inflight_resources_) {
      max_inflight_resources_ = num_inflight_resources_;
    }
  }
  DCHECK(resource->HasOneRef());
  return resource;
}

bool CanvasNon2DResourceProvider::IsValid() const {
  if (IsSoftware()) {
    return shared_image_interface_provider_ &&
           shared_image_interface_provider_->SharedImageInterface() &&
           GetSkSurface();
  }

  return !IsGpuContextLost();
}

gpu::SharedImageUsageSet CanvasNon2DResourceProvider::GetSharedImageUsageFlags()
    const {
  return image_pool_->GetImageInfo().usage;
}

void CanvasNon2DResourceProvider::EnsureWriteAccess() {
  DCHECK(resource_);
  // In software mode, we don't need write access to the resource during
  // drawing since it is executed on CPU memory managed by Skia.
  DCHECK(resource_->HasOneRef() || IsSingleBuffered() || is_software_)
      << "Write access requires exclusive access to the resource";
  DCHECK(!resource()->is_cross_thread())
      << "Write access is only allowed on the owning thread";

  if (current_resource_has_write_access_ || IsGpuContextLost()) {
    return;
  }
  current_resource_has_write_access_ = true;
}

void CanvasNon2DResourceProvider::EndWriteAccess() {
  DCHECK(!resource()->is_cross_thread());

  if (!current_resource_has_write_access_ || IsGpuContextLost()) {
    return;
  }

  if (is_software_) {
    if (ShouldReplaceTargetBuffer()) {
      resource_ = NewOrRecycledResource();
    }
    if (!resource() || !GetSkSurface()) {
      return;
    }
    resource()->UploadSoftwareRenderingResults(GetSkSurface());
  }

  current_resource_has_write_access_ = false;
}

void CanvasNon2DResourceProvider::OnContextLost() {
  if (notified_context_lost_) {
    return;
  }
  ClearUnusedResources();
  // Notify the owner of this resource provider that the GPU context was
  // lost. The call is done in a separate task, so that the owner can delete
  // this resource provider if needed.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&CanvasNon2DResourceProvider::NotifyGpuContextLostTask,
                     CreateWeakPtr()));
  notified_context_lost_ = true;
}

void CanvasNon2DResourceProvider::OnGpuChannelLost() {
  OnContextLost();
}

void CanvasNon2DResourceProvider::OnContextDestroyed() {
  if (skia_canvas_) {
    skia_canvas_->reset_image_provider();
  }
  canvas_image_provider_.reset();
  if (image_pool_) {
    image_pool_->Clear();
  }
}

void CanvasNon2DResourceProvider::NotifyGpuContextLostTask(
    base::WeakPtr<CanvasNon2DResourceProvider> provider) {
  if (provider && provider->delegate_) {
    // Move `provider` as hint that it shouldn't be reused after this point.
    // The `delegate` owns the provider and can delete it in
    // `NotifyGpuContextLost()`.
    std::move(provider)->delegate_->NotifyGpuContextLost();
  }
}

// For WebGpu RecyclableCanvasResource.
void CanvasNon2DResourceProvider::OnAcquireRecyclableCanvasResource() {
  EnsureWriteAccess();
}

void CanvasNon2DResourceProvider::OnDestroyRecyclableCanvasResource(
    const gpu::SyncToken& sync_token) {
  // RecyclableCanvasResource should be the only one that holds onto
  // |resource_|.
  DCHECK(resource()->HasOneRef());
  resource()->WaitSyncToken(sync_token);
}

void CanvasNon2DResourceProvider::OnFlushForImage(
    cc::PaintImage::ContentId content_id) {
  if (cached_snapshot_ &&
      cached_snapshot_->PaintImageForCurrentFrame().GetContentIdForFrame(0) ==
          content_id) {
    // This handles the case where the cached snapshot is referenced by an
    // ImageBitmap that is being transferred to a worker.
    cached_snapshot_.reset();
  }
}

void CanvasNon2DResourceProvider::ClearUnusedResources() {
  if (image_pool_) {
    image_pool_->Clear();
  }
}

bool CanvasNon2DResourceProvider::IsSingleBuffered() const {
  return image_pool_ && image_pool_->GetImageInfo().usage.Has(
                            gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE);
}

void CanvasNon2DResourceProvider::OnResourceRefReturned(
    scoped_refptr<CanvasResourceSharedImage>&& resource) {
  if (!resource->IsLost() && resource->HasOneRef() && image_pool_) {
    image_pool_->ReleaseImage(std::move(resource));
  }
}

gpu::raster::RasterInterface* CanvasNon2DResourceProvider::RasterInterface()
    const {
  if (!ContextProviderWrapper()) {
    return nullptr;
  }
  return ContextProviderWrapper()->ContextProvider().RasterInterface();
}

bool CanvasNon2DResourceProvider::IsGpuContextLost() const {
  auto* raster_interface = RasterInterface();
  return !raster_interface ||
         raster_interface->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
}

}  // namespace blink
