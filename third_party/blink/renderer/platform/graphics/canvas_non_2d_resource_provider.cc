// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_non_2d_resource_provider.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/function_ref.h"
#include "base/memory/ptr_util.h"
#include "base/memory/safety_checks.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
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
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_image_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/gpu/canvas_utils.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/instrumentation/canvas_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
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
    base::OnceClosure context_lost_callback) {
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

  // If we cannot use overlay, we have to remove the scanout flag.
  const auto& shared_image_caps = context_provider_wrapper->ContextProvider()
                                      .SharedImageInterface()
                                      ->GetCapabilities();
  bool is_overlay_supported = is_mappable_shared_image_allowed &&
                              shared_image_caps.supports_scanout_shared_images;

  if (!is_overlay_supported) {
    shared_image_usage_flags.RemoveAll(gpu::SHARED_IMAGE_USAGE_SCANOUT);
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
      context_provider_wrapper, shared_image_usage_flags,
      std::move(context_lost_callback)));

  return provider->IsValid() ? std::move(provider) : nullptr;
}

std::unique_ptr<CanvasNon2DResourceProvider>
CanvasNon2DResourceProvider::CreateForSoftwareCompositor(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider,
    base::OnceClosure context_lost_callback) {
  if (SharedGpuContext::IsGpuCompositingEnabled()) {
    return nullptr;
  }

  CHECK(format == viz::SharedImageFormat::N32Format() ||
        format == viz::SinglePlaneFormat::kRGBA_F16);

  auto provider = base::WrapUnique(new CanvasNon2DResourceProvider(
      size, format, alpha_type, color_space, hdr_metadata,
      shared_image_interface_provider, std::move(context_lost_callback)));
  return provider->IsValid() ? std::move(provider) : nullptr;
}

CanvasNon2DResourceProvider::CanvasNon2DResourceProvider(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    gpu::SharedImageUsageSet shared_image_usage_flags,
    base::OnceClosure context_lost_callback)
    : size_(size),
      format_(format),
      alpha_type_(alpha_type),
      color_space_(color_space),
      hdr_metadata_(hdr_metadata),
      context_lost_callback_(std::move(context_lost_callback)),
      is_software_(false),
      snapshot_paint_image_id_(cc::PaintImage::GetNextId()),
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

      gpu::ImageInfo image_info(size, format, shared_image_usage_flags,
                                color_space, kTopLeft_GrSurfaceOrigin,
                                alpha_type, /*buffer_usage=*/std::nullopt,
                                /*is_software=*/false);

      std::optional<base::TimeDelta> expiration_time =
          (base::FeatureList::IsEnabled(kCanvas2DReclaimUnusedResources))
              ? std::make_optional(kUnusedResourceExpirationTime)
              : std::nullopt;

      image_pool_ = gpu::SharedImagePool<CanvasResourceSharedImage>::Create(
          image_info, sii, "CanvasResourceRaster", kMaxRecycledCanvasResources,
          expiration_time);
    }
  }

  resource_ = NewOrRecycledResource();
  FlushForImageListener::Get()->AddObserver(this);
}

CanvasNon2DResourceProvider::CanvasNon2DResourceProvider(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider,
    base::OnceClosure context_lost_callback)
    : size_(size),
      format_(format),
      alpha_type_(alpha_type),
      color_space_(color_space),
      hdr_metadata_(hdr_metadata),
      context_lost_callback_(std::move(context_lost_callback)),
      is_software_(true),
      snapshot_paint_image_id_(cc::PaintImage::GetNextId()),
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

gpu::SharedImageUsageSet
CanvasNon2DResourceProvider::GetSharedImageUsageFlagsForTesting() const {
  return image_pool_->GetImageInfo().usage;
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
  if (provider && provider->context_lost_callback_) {
    // Move `provider` as hint that it shouldn't be reused after this point.
    // The owner of the provider can delete it in `context_lost_callback_`.
    std::move(std::move(provider)->context_lost_callback_).Run();
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

bool CanvasNon2DResourceProvider::ShouldReplaceTargetBuffer(
    PaintImage::ContentId content_id) {
  CHECK(!is_software_);

  // If the resource was lost, we can not use it for writes again.
  if (resource_->IsLost()) {
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
  FlushForImageListener::Get()->NotifyFlushForImage(content_id);

  return !resource_->HasOneRef();
}

scoped_refptr<gpu::ClientSharedImage>
CanvasNon2DResourceProvider::BeginExternalOverwrite(
    gpu::SyncToken& internal_access_sync_token) {
  CHECK(!is_software_);

  if (IsGpuContextLost()) {
    return nullptr;
  }

  EnsureResourceReadyForDraw();

  internal_access_sync_token = resource_->acquire_sync_token();
  return resource_->GetSharedImage();
}

void CanvasNon2DResourceProvider::EndExternalWrite(
    const gpu::SyncToken& external_write_sync_token) {
  if (IsGpuContextLost()) {
    return;
  }

  resource_->WaitSyncToken(external_write_sync_token);
  resource_->SetReleaseSyncToken(external_write_sync_token);
}

scoped_refptr<ExportedCanvasResource>
CanvasNon2DResourceProvider::DoExternalOverdrawAndProduceResource(
    base::FunctionRef<void(cc::PaintCanvas&)> draw_callback) {
  cached_snapshot_.reset();

  if (!IsSoftware() && IsGpuContextLost()) {
    return nullptr;
  }

  scoped_refptr<CanvasResourceSharedImage> software_resource;
  if (IsSoftware()) {
    software_resource = NewOrRecycledResource();
    if (!software_resource) {
      return nullptr;
    }
  }

  MemoryManagedPaintRecorder recorder(Size(), /*client=*/nullptr);
  draw_callback(recorder.getRecordingCanvas());
  if (recorder.HasReleasableDrawOps()) {
    FlushRecording(recorder.ReleaseMainRecording());
  }

  if (IsSoftware()) {
    software_resource->UploadSoftwareRenderingResults(GetSkSurface());

    return CreateExportedResource(std::move(software_resource));
  }

  return resource_ ? CreateExportedResource(resource_) : nullptr;
}

scoped_refptr<StaticBitmapImage>
CanvasNon2DResourceProvider::DoExternalOverdrawAndSnapshot(
    base::FunctionRef<void(cc::PaintCanvas&)> draw_callback) {
  cached_snapshot_.reset();

  if (!IsValid()) {
    return nullptr;
  }

  MemoryManagedPaintRecorder recorder(Size(), /*client=*/nullptr);
  draw_callback(recorder.getRecordingCanvas());
  if (recorder.HasReleasableDrawOps()) {
    FlushRecording(recorder.ReleaseMainRecording());
  }
  return Snapshot();
}

void CanvasNon2DResourceProvider::EnsureResourceReadyForDraw() {
  CHECK(!is_software_);
  DCHECK(resource_);

  // Since the resource will be updated, the cached snapshot is no longer valid.
  cached_snapshot_.reset();

  // Determine if a new resource is needed.
  if (ShouldReplaceTargetBuffer(cached_content_id_)) {
    cached_content_id_ = PaintImage::kInvalidContentId;

    resource_ = NewOrRecycledResource();

    // As the image might have just been created, we need to ensure that it is
    // cleared on the next BeginRasterCHROMIUM to satisfy service-side security
    // requirements (note: as an optimization we could avoid doing this if the
    // resource was recycled as in that case there are no security implications).
    is_cleared_ = false;
  }
}

scoped_refptr<ExportedCanvasResource>
CanvasNon2DResourceProvider::CreateExportedResource(
    scoped_refptr<CanvasResourceSharedImage> resource) {
  return base::MakeRefCounted<ExportedCanvasResource>(
      resource, resource->GetSharedImage(), resource->sync_token(),
      viz::TransferableResource::ResourceSource::kCanvas, hdr_metadata_,
      context_provider_wrapper_, !IsSoftware());
}

scoped_refptr<ExportedCanvasResource>
CanvasNon2DResourceProvider::ProduceCanvasResource() {
  TRACE_EVENT0("blink", "CanvasNon2DResourceProvider::ProduceCanvasResource");
  if (IsSoftware()) {
    DCHECK(GetSkSurface());
    scoped_refptr<CanvasResourceSharedImage> output_resource =
        NewOrRecycledResource();
    if (!output_resource) {
      return nullptr;
    }

    output_resource->UploadSoftwareRenderingResults(GetSkSurface());

    return CreateExportedResource(std::move(output_resource));
  }

  if (IsGpuContextLost()) {
    return nullptr;
  }

  return resource_ ? CreateExportedResource(resource_) : nullptr;
}

scoped_refptr<StaticBitmapImage> CanvasNon2DResourceProvider::Snapshot() {
  TRACE_EVENT0("blink", "CanvasNon2DResourceProvider::Snapshot");
  if (!IsValid()) {
    return nullptr;
  }

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
    return UnacceleratedStaticBitmapImage::Create(std::move(paint_image));
  }

  if (!cached_snapshot_) {
    // The |release_callback| keeps a ref on this resource to ensure the backing
    // shared image is kept alive until the lifetime of the image.
    // Note that the code in CanvasResourceProvider::RecycleResource also uses
    // the ref-count on the resource as a proxy for a read lock to allow
    // recycling the resource once all refs have been released.
    auto release_callback = base::BindOnce(
        [](scoped_refptr<CanvasResourceSharedImage>&& resource,
           const gpu::SyncToken& sync_token, bool lost_resource) {
          CHECK(resource);
          CHECK(!lost_resource);

          resource->WaitSyncToken(sync_token);
          CanvasResource::DropRefOnOwningThread(std::move(resource));
        },
        resource_);

    cached_snapshot_ =
        AcceleratedStaticBitmapImage::CreateFromCanvasSharedImage(
            resource_->GetSharedImage(), resource_->sync_token(), alpha_type_,
            hdr_metadata_, context_provider_wrapper_,
            base::PlatformThread::CurrentRef(),
            ThreadScheduler::Current()->CleanupTaskRunner(),
            std::move(release_callback));

    // We'll record its content_id to be used by the FlushForImageListener.
    // This will be needed in EnsureResourceReadyForDraw(), but we are doing it
    // now, as we don't know if later on we will be in the same thread the
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
  return cached_snapshot_;
}

CanvasImageProvider* CanvasNon2DResourceProvider::GetOrCreateImageProvider() {
  if (!canvas_image_provider_) {
    if (!is_software_) {
      if (!IsGpuContextLost()) {
        // Create an ImageDecodeCache for half float images only if the canvas
        // is using half float back storage.
        cc::ImageDecodeCache* cache_f16 = nullptr;
        if (format_ == viz::SinglePlaneFormat::kRGBA_F16) {
          cache_f16 =
              context_provider_wrapper_->ContextProvider().ImageDecodeCache(
                  kRGBA_F16_SkColorType);
        }

        cc::ImageDecodeCache* cache_rgba8 =
            context_provider_wrapper_->ContextProvider().ImageDecodeCache(
                kN32_SkColorType);

        canvas_image_provider_ = std::make_unique<CanvasImageProvider>(
            cache_rgba8, cache_f16, color_space_, format_,
            cc::PlaybackImageProvider::RasterMode::kGpu,
            context_provider_wrapper_);
      }
    } else {
      // TODO(crbug.com/352263194): CanvasImageProvider only uses
      // `context_provider_wrapper_` in `RasterMode::kSoftware` for
      // `PaintImage::BindTextureBacking()`, which is only non-no-op for
      // `VideoTextureBacking` (from HTML-in-Canvas `<video>` PaintRecords).
      // Investigate whether software `CanvasNon2DResourceProvider` can ever
      // encounter that case and, if not, pass nullptr here and make
      // `context_provider_wrapper_` GPU-only.
      if (!context_provider_wrapper_) {
        context_provider_wrapper_ = SharedGpuContext::ContextProviderWrapper();
        if (context_provider_wrapper_) {
          context_provider_wrapper_->AddObserver(this);
        }
      }

      // Create an ImageDecodeCache for half float images only if the canvas
      // is using half float back storage.
      cc::ImageDecodeCache* cache_f16 = nullptr;
      if (format_ == viz::SinglePlaneFormat::kRGBA_F16) {
        cache_f16 = &Image::SharedCCDecodeCache(kRGBA_F16_SkColorType);
      }

      cc::ImageDecodeCache* cache_rgba8 =
          &Image::SharedCCDecodeCache(kN32_SkColorType);

      canvas_image_provider_ = std::make_unique<CanvasImageProvider>(
          cache_rgba8, cache_f16, color_space_, format_,
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
    // Software PaintRecord playback allocates short-lived Skia rasterization
    // scratch buffers that churn PartitionAlloc's quarantine. Keep safety
    // checks active when discardable images may trigger image decoding.
    // TODO(crbug.com/582017429): Optimize and remove if possible.
    std::optional<base::ScopedSafetyChecksExclusion> excluded;
    if (!last_recording.has_discardable_images()) {
      excluded.emplace();
    }
    skia_canvas_->drawPicture(std::move(last_recording));
  } else if (!IsGpuContextLost()) {
    EnsureResourceReadyForDraw();

    const bool needs_clear = !is_cleared_;
    is_cleared_ = true;
    auto* image_provider = GetOrCreateImageProvider();

    gpu::SyncToken sync_token = RasterInterface()->RasterSharedImage(
        resource_->GetSharedImage(), resource_->acquire_sync_token(),
        std::move(last_recording), image_provider, needs_clear);
    resource_->SetReleaseSyncToken(sync_token);
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
    result += resource_->GetSharedImage()->EstimatedSizeInBytes() *
              num_inflight_resources_;
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

  resource_->OnMemoryDump(pmd, path);

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


SkSurfaceProps CanvasNon2DResourceProvider::GetSkSurfaceProps() const {
  const bool can_use_lcd_text = alpha_type_ == kOpaque_SkAlphaType;
  return skia::LegacyDisplayGlobals::ComputeSurfaceProps(can_use_lcd_text);
}

sk_sp<SkSurface> CanvasNon2DResourceProvider::CreateSkSurface() const {
  TRACE_EVENT0("blink", "CanvasNon2DResourceProvider::CreateSkSurface");

  CHECK(is_software_);

  const auto props = GetSkSurfaceProps();
  const auto info = SkImageInfo::Make(
      size_.width(), size_.height(), viz::ToClosestSkColorType(format_),
      alpha_type_, color_space_.ToSkColorSpace());
  return SkSurfaces::Raster(info, &props);
}

}  // namespace blink
