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
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/paint/decode_stashing_image_provider.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_op_buffer.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/gpu/raster_context_provider.h"
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
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_image_provider.h"
#include "third_party/blink/renderer/platform/graphics/exported_canvas_resource.h"
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

void Canvas2DResourceProvider::OnContextLost() {
  if (notified_context_lost_) {
    return;
  }
  ClearUnusedResources();
  // Notify the owner of this resource provider that the GPU context was
  // lost. The call is done in a separate task, so that the owner can delete
  // this resource provider if needed.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&Canvas2DResourceProvider::NotifyGpuContextLostTask,
                     CreateWeakPtr()));
  notified_context_lost_ = true;
}

void Canvas2DResourceProvider::OnGpuChannelLost() {
  OnContextLost();
}

bool Canvas2DResourceProvider::ShouldReplaceTargetBuffer(
    PaintImage::ContentId content_id) {
  CHECK(is_accelerated_);

  // If the canvas is single buffered, concurrent read/writes to the resource
  // are allowed. Note that we ignore the resource lost case as well since
  // that only indicates that we did not get a sync token for read/write
  // synchronization which is not a requirement for single buffered canvas.
  if (IsSingleBuffered()) {
    return false;
  }

  // If the resource is missing or lost, we cannot use it for writes again.
  if (!resource() || resource()->IsLost()) {
    return true;
  }

  // We have the only ref to the resource which implies there are no active
  // readers.
  if (resource_->HasOneRef()) {
    return false;
  }

  // Another context may have a read reference to this resource. Flush the
  // deferred queue in that context so that we don't need to copy.
  FlushForImageListener::Get()->NotifyFlushForImage(content_id);

  return !resource_->HasOneRef();
}

void Canvas2DResourceProvider::EnsureResourceReadyForDraw() {
  CHECK(is_accelerated_);
  DCHECK(resource_);

  // Since the resource will be updated, the cached snapshot is no longer
  // valid. Note that it is important to release this reference here to not
  // trigger copy-on-write below from the resource ref in the snapshot.
  // Note that this is valid for single buffered mode also, since while the
  // resource/mailbox remains the same, the snapshot needs an updated sync
  // token for these writes.
  cached_snapshot_.reset();

  if (ShouldReplaceTargetBuffer(cached_content_id_)) {
    cached_content_id_ = PaintImage::kInvalidContentId;
    DCHECK(!current_resource_has_write_access_)
        << "Write access must be released before sharing the resource";

    auto old_resource = std::move(resource_);
    auto* old_resource_shared_image =
        static_cast<CanvasResourceSharedImage*>(old_resource.get());

    resource_ = NewOrRecycledResource();
    if (must_preserve_content_on_copy_on_write_) {
      auto [src_token, dst_token] = RasterInterface()->CopySharedImage(
          old_resource_shared_image->GetSharedImage(),
          old_resource_shared_image->acquire_sync_token(),
          resource()->GetSharedImage(), resource()->acquire_sync_token(),
          gfx::Rect(Size()), gfx::Point(0, 0));
      old_resource_shared_image->SetReleaseSyncToken(src_token);
      resource_->SetReleaseSyncToken(dst_token);
    } else {
      // If we're not copying over the previous contents, we need to ensure
      // that the image is cleared on the next BeginRasterCHROMIUM.
      is_cleared_ = false;
    }

    UMA_HISTOGRAM_BOOLEAN("Blink.Canvas.ContentChangeMode",
                          must_preserve_content_on_copy_on_write_);
    // By default, the contents of the new resource must be preserved on a
    // subsequent CopyOnWrite.
    must_preserve_content_on_copy_on_write_ = true;
  }
}

void Canvas2DResourceProvider::WillDrawUnaccelerated() {
  CHECK(!IsAccelerated());

  if (IsSoftware()) {
    return;
  }
  EnsureWriteAccess();
}

bool Canvas2DResourceProvider::WritePixels(const SkImageInfo& orig_info,
                                           const void* pixels,
                                           size_t row_bytes,
                                           int x,
                                           int y) {
  TRACE_EVENT0("blink", "Canvas2DResourceProvider::WritePixels");
  CHECK(is_accelerated_);
  if (IsGpuContextLost()) {
    return false;
  }

  EnsureResourceReadyForDraw();

  // The below  write to the resource's SharedImage will need to be preserved in
  // the case of a subsequent CopyOnWrite.
  // TODO(crbug.com/352263194): Logically this bool must already be true
  // (see discussion here:
  // https://chromium-review.googlesource.com/c/chromium/src/+/7557841/comment/bb38e497_ef1efdbc/).
  // Verify that this is the case and update the code here.
  must_preserve_content_on_copy_on_write_ = true;

  gpu::SyncToken sync_token = RasterInterface()->WritePixels(
      resource()->GetSharedImage(), resource()->acquire_sync_token(), x, y,
      SkPixmap(orig_info, pixels, row_bytes));
  resource()->SetReleaseSyncToken(sync_token);

  // If the overdraw optimization kicked in, we need to indicate that the
  // pixels do not need to be cleared, otherwise the subsequent
  // rasterizations will clobber canvas contents.
  if (x <= 0 && y <= 0 && orig_info.width() >= Size().width() &&
      orig_info.height() >= Size().height()) {
    is_cleared_ = true;
  }

  return true;
}

base::ByteSize Canvas2DResourceProvider::EstimatedSizeInBytes() const {
  base::ByteSize result;
  if (resource_) {
    result += resource_->GetSharedImage()->EstimatedSizeInBytes() *
              num_inflight_resources_;
  }
  return result;
}

void Canvas2DResourceProvider::OnContextDestroyed() {
  if (image_pool_) {
    image_pool_->Clear();
  }
}

scoped_refptr<ExportedCanvasResource>
Canvas2DResourceProvider::ProduceCanvasResource(SkSurface* surface) {
  TRACE_EVENT0("blink", "Canvas2DResourceProvider::ProduceCanvasResource");

  if (IsSoftware()) {
    DCHECK(surface);
    scoped_refptr<CanvasResourceSharedImage> output_resource =
        NewOrRecycledResource();
    if (!output_resource) {
      return nullptr;
    }

    output_resource->UploadSoftwareRenderingResults(surface);

    return base::MakeRefCounted<ExportedCanvasResource>(
        std::move(output_resource));
  }

  if (IsGpuContextLost()) {
    return nullptr;
  }

  // We are about to give the caller read access to this resource (and its
  // backing SharedImage). Hence, we must make sure that we give up any write
  // access.
  EndWriteAccess(surface);

  return resource_ ? base::MakeRefCounted<ExportedCanvasResource>(resource_)
                   : nullptr;
}

bool Canvas2DResourceProvider::IsValid() const {
  if (IsSoftware()) {
    // Software compositing (which always uses software raster).
    return shared_image_interface_provider_ &&
           shared_image_interface_provider_->SharedImageInterface();
  }

  return !IsGpuContextLost();
}

void Canvas2DResourceProvider::EnsureWriteAccess() {
  DCHECK(resource_);
  // In software mode, we don't need write access to the resource during
  // drawing since it is executed on CPU memory managed by Skia.
  DCHECK(resource_->HasOneRef() || IsSingleBuffered() || !is_accelerated_)
      << "Write access requires exclusive access to the resource";
  DCHECK(!resource()->is_cross_thread())
      << "Write access is only allowed on the owning thread";

  if (current_resource_has_write_access_ || IsGpuContextLost()) {
    return;
  }
  current_resource_has_write_access_ = true;
}

void Canvas2DResourceProvider::EndWriteAccess(SkSurface* surface) {
  DCHECK(!resource()->is_cross_thread());

  if (!current_resource_has_write_access_ || IsGpuContextLost()) {
    return;
  }

  if (is_accelerated_) {
    // As a write operation has just completed on the current resource, it is
    // now necessary to preserve that resource's contents on a subsequent
    // CopyOnWrite.
    must_preserve_content_on_copy_on_write_ = true;
  } else {
    if (!resource() || resource()->IsLost() || !resource()->HasOneRef()) {
      resource_ = NewOrRecycledResource();
    }
    if (!resource() || !surface) {
      return;
    }
    resource()->UploadSoftwareRenderingResults(surface);
  }

  current_resource_has_write_access_ = false;
}

scoped_refptr<StaticBitmapImage> Canvas2DResourceProvider::Snapshot() {
  TRACE_EVENT0("blink", "Canvas2DResourceProvider::Snapshot");
  CHECK(is_accelerated_);
  if (!IsValid()) {
    return nullptr;
  }

  if (!cached_snapshot_) {
    EndWriteAccess(/*surface=*/nullptr);

    // The |release_callback| keeps a ref on this resource to ensure the backing
    // shared image is kept alive until the lifetime of the image.
    // Note that the code in CanvasResourceProvider::RecycleResource also uses
    // the ref-count on the resource as a proxy for a read lock to allow
    // recycling the resource once all refs have been released.
    auto release_callback = base::BindOnce(
        [](scoped_refptr<CanvasResourceSharedImage>&& resource,
           const gpu::SyncToken& sync_token, bool lost_resource) {
          CHECK(resource);

          resource->WaitSyncToken(sync_token);
          if (lost_resource) {
            resource->NotifyResourceLost();
          }

          CanvasResource::DropRefOnOwningThread(std::move(resource));
        },
        resource_);

    const auto& client_shared_image = resource_->GetSharedImage();

    cached_snapshot_ =
        AcceleratedStaticBitmapImage::CreateFromCanvasSharedImage(
            client_shared_image, resource_->sync_token(),
            client_shared_image->alpha_type(), resource_->hdr_metadata_,
            resource_->context_provider_wrapper_, resource_->owning_thread_ref_,
            resource_->owning_thread_task_runner_, std::move(release_callback));

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
  DCHECK(!current_resource_has_write_access_);
  return cached_snapshot_;
}

void Canvas2DResourceProvider::OnFlushForImage(
    cc::PaintImage::ContentId content_id) {
  if (cached_snapshot_ &&
      cached_snapshot_->PaintImageForCurrentFrame().GetContentIdForFrame(0) ==
          content_id) {
    // This handles the case where the cached snapshot is referenced by an
    // ImageBitmap that is being transferred to a worker.
    cached_snapshot_.reset();
  }
}

void Canvas2DResourceProvider::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd) {
  std::string path = base::StringPrintf("canvas/ResourceProvider_0x%" PRIXPTR,
                                        reinterpret_cast<uintptr_t>(this));

  if (resource()) {
    resource()->OnMemoryDump(pmd, path);
  }

  std::string cached_path = path + "/cached";
  image_pool_->OnMemoryDump(pmd, cached_path);
}

void Canvas2DResourceProvider::NotifyGpuContextLostTask(
    base::WeakPtr<Canvas2DResourceProvider> provider) {
  if (provider && provider->context_lost_callback_) {
    // Move `provider` as hint that it shouldn't be reused after this point.
    // The owner of the provider can delete it in `context_lost_callback_`.
    std::move(std::move(provider)->context_lost_callback_).Run();
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
    gpu::SharedImageUsageSet shared_image_usage_flags,
    base::OnceClosure context_lost_callback)
    : is_accelerated_(is_accelerated),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      hdr_metadata_(hdr_metadata),
      context_lost_callback_(std::move(context_lost_callback)) {
  CHECK(context_provider_wrapper_);
  context_provider_wrapper_->AddObserver(this);
  raster_context_provider_ = base::WrapRefCounted(
      context_provider_wrapper_->ContextProvider().RasterContextProvider());
  if (raster_context_provider_) {
    raster_context_provider_->AddObserver(this);
  }

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

  if (resource_) {
    EnsureWriteAccess();
  }

  // Single buffered mode supported only for accelerated canvas.
  CHECK(!IsSingleBuffered() || is_accelerated_);
}

void Canvas2DResourceProvider::RecordingCleared() {
  must_preserve_content_on_copy_on_write_ = false;
}

Canvas2DResourceProvider::Canvas2DResourceProvider(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider,
    base::OnceClosure context_lost_callback)
    : is_accelerated_(false),
      shared_image_interface_provider_(
          shared_image_interface_provider->GetWeakPtr()),
      hdr_metadata_(hdr_metadata),
      context_lost_callback_(std::move(context_lost_callback)) {
  CHECK(shared_image_interface_provider_);
  shared_image_interface_provider_->AddGpuChannelLostObserver(this);
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
  if (context_provider_wrapper_) {
    context_provider_wrapper_->RemoveObserver(this);
  }
  if (raster_context_provider_) {
    raster_context_provider_->RemoveObserver(this);
  }
  if (shared_image_interface_provider_) {
    shared_image_interface_provider_->RemoveGpuChannelLostObserver(this);
  }

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

gpu::raster::RasterInterface* Canvas2DResourceProvider::RasterInterface()
    const {
  if (!ContextProviderWrapper()) {
    return nullptr;
  }
  return ContextProviderWrapper()->ContextProvider().RasterInterface();
}

bool Canvas2DResourceProvider::IsGpuContextLost() const {
  auto* raster_interface = RasterInterface();
  return !raster_interface ||
         raster_interface->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
}

}  // namespace blink
