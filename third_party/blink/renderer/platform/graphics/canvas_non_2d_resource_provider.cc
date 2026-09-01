// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_non_2d_resource_provider.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/function_ref.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/skia_paint_canvas.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_graphics_shared_image_interface_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_image_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/gpu/canvas_utils.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/instrumentation/canvas_memory_dump_provider.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkSurface.h"

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

  auto provider = base::WrapUnique(new CanvasNon2DResourceProvider(
      size, format, alpha_type, color_space, hdr_metadata,
      context_provider_wrapper, shared_image_usage_flags, delegate));

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

  auto provider = base::WrapUnique(new CanvasNon2DResourceProvider(
      size, format, alpha_type, color_space, hdr_metadata,
      shared_image_interface_provider, delegate));
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

void CanvasNon2DResourceProvider::SetAnimatedImageFrameIndexes(
    scoped_refptr<const cc::AnimatedImageFrameIndexMap> indexes) {
  CHECK(canvas_image_provider_);
  canvas_image_provider_->SetAnimatedImageFrameIndexes(indexes);
}

bool CanvasNon2DResourceProvider::ShouldReplaceTargetBuffer(
    PaintImage::ContentId content_id) {
  // If the canvas is single buffered, concurrent read/writes to the resource
  // are allowed. Note that we ignore the resource lost case as well since
  // that only indicates that we did not get a sync token for read/write
  // synchronization which is not a requirement for single buffered canvas.
  if (IsSingleBuffered()) {
    return false;
  }

  // If the resource was lost, we can not use it for writes again.
  if (resource()->IsLost()) {
    return true;
  }

  // We have the only ref to the resource which implies there are no active
  // readers.
  if (resource_->HasOneRef()) {
    return false;
  }

  // Its possible to have deferred work in skia which uses this resource. Try
  // flushing once to see if that releases the read refs. We can avoid a copy
  // by queuing this work before writing to this resource.
  if (!is_software_) {
    // Another context may have a read reference to this resource. Flush the
    // deferred queue in that context so that we don't need to copy.
    FlushForImageListener::Get()->NotifyFlushForImage(content_id);
  }

  return !resource_->HasOneRef();
}

scoped_refptr<gpu::ClientSharedImage>
CanvasNon2DResourceProvider::BeginExternalOverwrite(
    gpu::SyncToken& internal_access_sync_token) {
  DCHECK(!is_software_);

  if (IsGpuContextLost()) {
    return nullptr;
  }

  // End the internal write access before calling WillDrawInternal(), which
  // has a precondition that there should be no current write access on the
  // resource.
  EndWriteAccess();

  // NOTE: Invoking WillDrawInternal() ensures that this invocation of
  // EndAccess() will generate a new sync token.
  auto access = WillDrawInternal();
  resource_->EndAccess(std::move(access));
  internal_access_sync_token = resource_->sync_token();
  return resource_->GetSharedImage();
}

void CanvasNon2DResourceProvider::EndExternalWrite(
    const gpu::SyncToken& external_write_sync_token) {
  if (IsGpuContextLost()) {
    return;
  }

  resource()->EndExternalWrite(external_write_sync_token);
}

scoped_refptr<CanvasResource>
CanvasNon2DResourceProvider::DoExternalOverdrawAndProduceResource(
    base::FunctionRef<void(cc::PaintCanvas&)> draw_callback) {
  cached_snapshot_.reset();

  if (!IsSoftware() && IsGpuContextLost()) {
    return nullptr;
  }

  scoped_refptr<CanvasResource> software_resource;
  if (IsSoftware()) {
    software_resource = NewOrRecycledResource();
    if (!software_resource) {
      return nullptr;
    }
  }

  draw_callback(recorder_for_external_draws_->getRecordingCanvas());
  if (recorder_for_external_draws_->HasReleasableDrawOps()) {
    FlushRecording(recorder_for_external_draws_->ReleaseMainRecording());
  }

  if (IsSoftware()) {
    // Note that the resource *must* be a CanvasResourceSharedImage as this
    // class creates CanvasResourceSharedImage instances exclusively.
    static_cast<CanvasResourceSharedImage*>(software_resource.get())
        ->UploadSoftwareRenderingResults(GetSkSurface());

    return software_resource;
  }

  // We are about to give the caller read access to this resource (and its
  // backing SharedImage). Hence, we must give up the current write access
  // (if any).
  EndWriteAccess();

  return resource_;
}

scoped_refptr<StaticBitmapImage>
CanvasNon2DResourceProvider::DoExternalOverdrawAndSnapshot(
    base::FunctionRef<void(cc::PaintCanvas&)> draw_callback,
    ImageOrientation orientation) {
  cached_snapshot_.reset();

  if (!IsValid()) {
    return nullptr;
  }

  draw_callback(recorder_for_external_draws_->getRecordingCanvas());
  if (recorder_for_external_draws_->HasReleasableDrawOps()) {
    FlushRecording(recorder_for_external_draws_->ReleaseMainRecording());
  }
  return Snapshot(orientation);
}

std::unique_ptr<gpu::RasterScopedAccess>
CanvasNon2DResourceProvider::WillDrawInternal() {
  DCHECK(resource_);

  // Since the resource will be updated, the cached snapshot is no longer valid.
  // Note that this is valid for single buffered mode also, since while the
  // resource/mailbox remains the same, the snapshot needs an updated sync token
  // for these writes.
  cached_snapshot_.reset();

  // Determine if a new resource is needed for accelerated resources. Note that
  // for unaccelerated resources, writes to the SharedImage are deferred to
  // ProduceCanvasResource.
  if (is_software_ || !ShouldReplaceTargetBuffer(cached_content_id_)) {
    return resource_->BeginAccess(/*readonly=*/false);
  }

  std::unique_ptr<gpu::RasterScopedAccess> dst_access;
  cached_content_id_ = PaintImage::kInvalidContentId;
  DCHECK(!current_resource_has_write_access_)
      << "Write access must be released before sharing the resource";

  resource_ = NewOrRecycledResource();
  dst_access = resource_->BeginAccess(/*readonly=*/false);

  // As the image might have just been created, we need to ensure that it is
  // cleared on the next BeginRasterCHROMIUM to satisfy service-side security
  // requirements (note: as an optimization we could avoid doing this if the
  // resource was recycled as in that case there are no security implications).
  is_cleared_ = false;

  return dst_access;
}


scoped_refptr<CanvasResource>
CanvasNon2DResourceProvider::ProduceCanvasResource() {
  TRACE_EVENT0("blink", "CanvasNon2DResourceProvider::ProduceCanvasResource");
  if (IsSoftware()) {
    DCHECK(GetSkSurface());
    scoped_refptr<CanvasResource> output_resource = NewOrRecycledResource();
    if (!output_resource) {
      return nullptr;
    }

    // Note that the resource *must* be a CanvasResourceSharedImage as this
    // class creates CanvasResourceSharedImage instances exclusively.
    static_cast<CanvasResourceSharedImage*>(output_resource.get())
        ->UploadSoftwareRenderingResults(GetSkSurface());

    return output_resource;
  }

  if (IsGpuContextLost()) {
    return nullptr;
  }

  // We are about to give the caller read access to this resource (and its
  // backing SharedImage). Hence, we must give up any write access.
  EndWriteAccess();

  return resource_;
}

scoped_refptr<StaticBitmapImage> CanvasNon2DResourceProvider::Snapshot(
    ImageOrientation orientation) {
  TRACE_EVENT0("blink", "CanvasNon2DResourceProvider::Snapshot");
  if (!IsValid()) {
    return nullptr;
  }

  // We don't need to EndWriteAccess here since that's required to upload the
  // rendering results to the resource's SharedImage (e.g., for GPU compositing)
  // while in this case we are simply returning the rendered CPU-side results to
  // the client.
  if (is_software_) {
    cc::PaintImage paint_image;

    auto sk_image = GetSkSurface()->makeImageSnapshot();
    if (sk_image) {
      auto last_snapshot_sk_image_id = snapshot_sk_image_id_;
      snapshot_sk_image_id_ = sk_image->uniqueID();

      // Ensure that a new PaintImage::ContentId is used only when the
      // underlying SkImage changes. This is necessary to ensure that the same
      // image results in a cache hit in cc's ImageDecodeCache.
      if (snapshot_paint_image_content_id_ == PaintImage::kInvalidContentId ||
          last_snapshot_sk_image_id != snapshot_sk_image_id_) {
        snapshot_paint_image_content_id_ = PaintImage::GetNextContentId();
      }

      paint_image =
          PaintImageBuilder::WithDefault()
              .set_id(snapshot_paint_image_id_)
              .set_image(std::move(sk_image), snapshot_paint_image_content_id_)
              .set_hdr_metadata(hdr_metadata_)
              .TakePaintImage();
    }

    DCHECK(!paint_image.IsTextureBacked());
    return UnacceleratedStaticBitmapImage::Create(std::move(paint_image),
                                                  orientation);
  }

  if (!cached_snapshot_) {
    EndWriteAccess();
    cached_snapshot_ = resource_->Bitmap();

    // We'll record its content_id to be used by the FlushForImageListener.
    // This will be needed in WillDrawInternal, but we are doing it now, as we
    // don't know if later on we will be in the same thread the
    // cached_snapshot_ was created and we wouldn't be able to
    // PaintImageForCurrentFrame in AcceleratedStaticBitmapImage just to check
    // the content_id. ShouldReplaceTargetBuffer needs this ID in order to let
    // other contexts know to flush to avoid unnecessary copy-on-writes.
    if (cached_snapshot_) {
      cached_content_id_ =
          cached_snapshot_->PaintImageForCurrentFrame().GetContentIdForFrame(
              0u);
    }
  }

  DCHECK(cached_snapshot_);
  DCHECK(!current_resource_has_write_access_);
  return cached_snapshot_;
}

CanvasImageProvider* CanvasNon2DResourceProvider::GetOrCreateImageProvider() {
  if (!canvas_image_provider_) {
    if (!context_provider_wrapper_) {
      context_provider_wrapper_ = SharedGpuContext::ContextProviderWrapper();
      if (context_provider_wrapper_) {
        context_provider_wrapper_->AddObserver(this);
      }
    }
    if (!is_software_) {
      if (!IsGpuContextLost()) {
        // Create an ImageDecodeCache for half float images only if the canvas
        // is using half float back storage.
        cc::ImageDecodeCache* cache_f16 = nullptr;
        if (GetSharedImageFormat() == viz::SinglePlaneFormat::kRGBA_F16) {
          cache_f16 =
              context_provider_wrapper_->ContextProvider().ImageDecodeCache(
                  kRGBA_F16_SkColorType);
        }

        cc::ImageDecodeCache* cache_rgba8 =
            context_provider_wrapper_->ContextProvider().ImageDecodeCache(
                kN32_SkColorType);

        canvas_image_provider_ = std::make_unique<CanvasImageProvider>(
            cache_rgba8, cache_f16, GetColorSpace(), GetSharedImageFormat(),
            cc::PlaybackImageProvider::RasterMode::kGpu,
            context_provider_wrapper_);
      }
    } else {
      // Create an ImageDecodeCache for half float images only if the canvas
      // is using half float back storage.
      cc::ImageDecodeCache* cache_f16 = nullptr;
      if (GetSharedImageFormat() == viz::SinglePlaneFormat::kRGBA_F16) {
        cache_f16 = &Image::SharedCCDecodeCache(kRGBA_F16_SkColorType);
      }

      cc::ImageDecodeCache* cache_rgba8 =
          &Image::SharedCCDecodeCache(kN32_SkColorType);

      canvas_image_provider_ = std::make_unique<CanvasImageProvider>(
          cache_rgba8, cache_f16, GetColorSpace(), GetSharedImageFormat(),
          cc::PlaybackImageProvider::RasterMode::kSoftware,
          context_provider_wrapper_);
    }
  }
  return canvas_image_provider_.get();
}

void CanvasNon2DResourceProvider::FlushRecording(
    cc::PaintRecord last_recording) {
  if (is_software_) {
    if (!skia_canvas_) {
      auto* image_provider = GetOrCreateImageProvider();
      skia_canvas_ = std::make_unique<cc::SkiaPaintCanvas>(
          GetSkSurface()->getCanvas(), image_provider);
    }
    skia_canvas_->drawPicture(std::move(last_recording));
  } else if (!IsGpuContextLost()) {
    auto access = WillDrawInternal();
    EnsureWriteAccess();

    const bool needs_clear = !is_cleared_;
    is_cleared_ = true;

    gpu::raster::RasterInterface* ri = RasterInterface();
    SkColor4f background_color = GetAlphaType() == kOpaque_SkAlphaType
                                     ? SkColors::kBlack
                                     : SkColors::kTransparent;

    auto list = base::MakeRefCounted<cc::DisplayItemList>();
    list->StartPaint();
    list->push<cc::DrawRecordOp>(std::move(last_recording));
    list->EndPaintOfUnpaired(gfx::Rect(Size().width(), Size().height()));
    list->Finalize();

    gfx::Size size(Size().width(), Size().height());
    size_t max_op_size_hint =
        gpu::raster::RasterInterface::kDefaultMaxOpSizeHint;
    gfx::Rect full_raster_rect(Size().width(), Size().height());
    gfx::Rect playback_rect(Size().width(), Size().height());
    gfx::Vector2dF post_translate(0.f, 0.f);
    gfx::Vector2dF post_scale(1.f, 1.f);

    const bool can_use_lcd_text = GetAlphaType() == kOpaque_SkAlphaType;
    const auto& caps =
        context_provider_wrapper_->ContextProvider().GetCapabilities();
    bool use_msaa = !caps.msaa_is_slow && !caps.avoid_stencil_buffers;
    ri->BeginRasterCHROMIUM(background_color, needs_clear,
                            /*msaa_sample_count=*/use_msaa ? 1 : 0,
                            use_msaa ? gpu::raster::MsaaMode::kDMSAA
                                     : gpu::raster::MsaaMode::kNoMSAA,
                            can_use_lcd_text, /*visible=*/true, GetColorSpace(),
                            /*hdr_headroom=*/0.f,
                            resource()->GetSharedImage()->mailbox().name);

    auto* image_provider = GetOrCreateImageProvider();
    ri->RasterCHROMIUM(
        list.get(), image_provider, size, full_raster_rect, playback_rect,
        post_translate, post_scale, /*requires_clear=*/false,
        /*raster_inducing_scroll_offsets=*/nullptr, &max_op_size_hint,
        base::RepeatingCallback<void(SkCanvas*, uint32_t)>());

    ri->EndRasterCHROMIUM();
    resource()->EndAccess(std::move(access));
  }

  // Images are locked for the duration of the rasterization, in case they get
  // used multiple times. We can unlock them once the rasterization is complete.
  if (canvas_image_provider_) {
    canvas_image_provider_->ReleaseLockedImages();
    canvas_image_provider_->UnbindTextureBackedImages();
  }
}

base::ByteSize CanvasNon2DResourceProvider::EstimatedSizeInBytes() const {
  base::ByteSize result;
  if (resource_) {
    result += resource_->EstimatedSizeInBytes() * num_inflight_resources_;
  }
  return result;
}

void CanvasNon2DResourceProvider::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd) {
  if (IsSoftware()) {
    if (!surface_) {
      return;
    }

    std::string dump_name =
        base::StringPrintf("canvas/ResourceProvider/SkSurface/0x%" PRIXPTR,
                           reinterpret_cast<uintptr_t>(surface_.get()));
    auto* dump = pmd->CreateAllocatorDump(dump_name);

    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    GetSize());
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameObjectCount,
                    base::trace_event::MemoryAllocatorDump::kUnitsObjects, 1);

    if (const char* system_allocator_name =
            base::trace_event::MemoryDumpManager::GetInstance()
                ->system_allocator_pool_name()) {
      pmd->AddSuballocation(dump->guid(), system_allocator_name);
    }
    return;
  }

  std::string path = base::StringPrintf("canvas/ResourceProvider_0x%" PRIXPTR,
                                        reinterpret_cast<uintptr_t>(this));

  resource()->OnMemoryDump(pmd, path);

  std::string cached_path = path + "/cached";
  image_pool_->OnMemoryDump(pmd, cached_path);
}

size_t CanvasNon2DResourceProvider::GetSize() const {
  if (!surface_) {
    return 0;
  }
  SkImageInfo info = surface_->imageInfo();
  return info.computeByteSize(info.minRowBytes());
}

SkSurface* CanvasNon2DResourceProvider::GetSkSurface() const {
  if (!surface_) {
    surface_ = CreateSkSurface();
  }
  return surface_.get();
}

void CanvasNon2DResourceProvider::RecordingCleared() {}

void CanvasNon2DResourceProvider::InitializeForRecording(
    cc::PaintCanvas* canvas) const {
  if (delegate_) {
    delegate_->InitializeForRecording(canvas);
  }
}

SkSurfaceProps CanvasNon2DResourceProvider::GetSkSurfaceProps() const {
  const bool can_use_lcd_text = GetAlphaType() == kOpaque_SkAlphaType;
  return skia::LegacyDisplayGlobals::ComputeSurfaceProps(can_use_lcd_text);
}

sk_sp<SkSurface> CanvasNon2DResourceProvider::CreateSkSurface() const {
  TRACE_EVENT0("blink", "CanvasNon2DResourceProvider::CreateSkSurface");

  CHECK(is_software_);

  if (is_software_) {
    const auto props = GetSkSurfaceProps();
    const auto info = SkImageInfo::Make(
        size_.width(), size_.height(), viz::ToClosestSkColorType(format_),
        alpha_type_, color_space_.ToSkColorSpace());
    return SkSurfaces::Raster(info, &props);
  }

  if (IsGpuContextLost() || !resource_) {
    return nullptr;
  }

  const auto props = GetSkSurfaceProps();

  // When using software raster with GPU compositing, we render into CPU memory
  // managed internally by SkSurface and copy the rendered results to the
  // current resource's backing SharedImage before dispatching that SharedImage
  // to the display compositor.
  return SkSurfaces::Raster(resource_->CreateSkImageInfo(), &props);
}

}  // namespace blink
