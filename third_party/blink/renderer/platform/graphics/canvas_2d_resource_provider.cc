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
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "cc/paint/decode_stashing_image_provider.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/tiles/software_image_decode_cache.h"
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
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_feature_type.h"
#include "skia/buildflags.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_shared_image_interface_provider.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_deferred_paint_record.h"
#include "third_party/blink/renderer/platform/graphics/canvas_image_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/gpu/canvas_utils.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/instrumentation/canvas_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GpuTypes.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"
#include "ui/gfx/skia_span_util.h"

namespace blink {

BASE_FEATURE(kCanvas2DAutoFlushParams, base::FEATURE_DISABLED_BY_DEFAULT);

// The following parameters attempt to reach a compromise between not flushing
// too often, and not accumulating an unreasonable backlog. Flushing too
// often will hurt performance due to overhead costs. Accumulating large
// backlogs, in the case of OOPR-Canvas, results in poor parallelism and
// janky UI. With OOPR-Canvas disabled, it is still desirable to flush
// periodically to guard against run-away memory consumption caused by
// PaintOpBuffers that grow indefinitely. The OOPR-related jank is caused by
// long-running RasterCHROMIUM calls that monopolize the main thread
// of the GPU process. By flushing periodically, we allow the rasterization
// of canvas contents to be interleaved with other compositing and UI work.
//
// The default values for these parameters were initially determined
// empirically. They were selected to maximize the MotionMark score on
// desktop computers. Field trials may be used to tune these parameters
// further by using metrics data from the field.
const base::FeatureParam<int> kMaxRecordedOpKB(&kCanvas2DAutoFlushParams,
                                               "max_recorded_op_kb",
                                               2 * 1024);

const base::FeatureParam<int> kMaxPinnedImageKB(&kCanvas2DAutoFlushParams,
                                                "max_pinned_image_kb",
                                                32 * 1024);

// Graphite can generally handle more ops, increase the size accordingly.
const base::FeatureParam<int> kMaxRecordedOpGraphiteKB(
    &kCanvas2DAutoFlushParams,
    "max_recorded_op_graphite_kb",
    6 * 1024);

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
    if (image_pool_->GetImageInfo().is_software) {
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

std::unique_ptr<MemoryManagedPaintRecorder>
Canvas2DResourceProvider::ReleaseRecorder() {
  auto recorder = std::make_unique<MemoryManagedPaintRecorder>(Size(), this);
  recorder_->SetClient(nullptr);
  recorder_.swap(recorder);
  DisableLineDrawingAsPathsIfNecessary();
  return recorder;
}

void Canvas2DResourceProvider::SetRecorder(
    std::unique_ptr<MemoryManagedPaintRecorder> recorder) {
  recorder->SetClient(this);
  recorder_ = std::move(recorder);
  DisableLineDrawingAsPathsIfNecessary();
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

  // Its possible to have deferred work in skia which uses this resource. Try
  // flushing once to see if that releases the read refs. We can avoid a copy
  // by queuing this work before writing to this resource.
  if (is_accelerated_) {
    // Another context may have a read reference to this resource. Flush the
    // deferred queue in that context so that we don't need to copy.
    FlushForImageListener::Get()->NotifyFlushForImage(content_id);
  }

  return !resource_->HasOneRef();
}

std::unique_ptr<gpu::RasterScopedAccess>
Canvas2DResourceProvider::WillDrawInternal() {
  DCHECK(resource_);

  // Since the resource will be updated, the cached snapshot is no longer
  // valid. Note that it is important to release this reference here to not
  // trigger copy-on-write below from the resource ref in the snapshot.
  // Note that this is valid for single buffered mode also, since while the
  // resource/mailbox remains the same, the snapshot needs an updated sync
  // token for these writes.
  cached_snapshot_.reset();

  // Determine if a copy is needed for accelerated resources. This is required
  // if copy-on-write is required. Note that for unaccelerated resources, this
  // does not apply: writes to the SharedImage are deferred to
  // ProduceCanvasResource and hence copy-on-write is never needed here.
  std::unique_ptr<gpu::RasterScopedAccess> dst_access;
  if (is_accelerated_ && ShouldReplaceTargetBuffer(cached_content_id_)) {
    cached_content_id_ = PaintImage::kInvalidContentId;
    DCHECK(!current_resource_has_write_access_)
        << "Write access must be released before sharing the resource";

    auto old_resource = std::move(resource_);
    auto* old_resource_shared_image =
        static_cast<CanvasResourceSharedImage*>(old_resource.get());

    resource_ = NewOrRecycledResource();
    dst_access = resource_->BeginAccess(/*readonly=*/false);
    if (must_preserve_content_on_copy_on_write_) {
      auto old_mailbox = old_resource_shared_image->GetSharedImage()->mailbox();
      auto mailbox = resource()->GetSharedImage()->mailbox();
      auto src_access = old_resource->BeginAccess(/*readonly=*/true);
      RasterInterface()->CopySharedImage(old_mailbox, mailbox, 0, 0, 0, 0,
                                         Size().width(), Size().height());
      old_resource_shared_image->EndAccess(std::move(src_access));
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
  } else {
    dst_access = resource_->BeginAccess(/*readonly=*/false);
  }
  return dst_access;
}

void Canvas2DResourceProvider::WillDrawUnaccelerated() {
  CHECK(!IsAccelerated());

  if (IsSoftware()) {
    return;
  }
  cached_snapshot_.reset();
  EnsureWriteAccess();
}

void Canvas2DResourceProvider::DisableLineDrawingAsPathsIfNecessary() {
  if (context_provider_wrapper_ &&
      context_provider_wrapper_->ContextProvider()
              .GetGpuFeatureInfo()
              .status_values[gpu::GPU_FEATURE_TYPE_SKIA_GRAPHITE] ==
          gpu::kGpuFeatureStatusEnabled) {
    Recorder().DisableLineDrawingAsPaths();
  }
}

bool Canvas2DResourceProvider::WritePixels(const SkImageInfo& orig_info,
                                           const void* pixels,
                                           size_t row_bytes,
                                           int x,
                                           int y) {
  TRACE_EVENT0("blink", "Canvas2DResourceProvider::WritePixels");
  if (!is_accelerated_) {
    WillDrawUnaccelerated();
    DCHECK(IsValid());
    DCHECK(!Recorder().HasRecordedDrawOps());

    if (!skia_canvas_) {
      skia_canvas_ = std::make_unique<cc::SkiaPaintCanvas>(
          GetSkSurface()->getCanvas(), GetOrCreateSWCanvasImageProvider());
    }

    return GetSkSurface()->getCanvas()->writePixels(orig_info, pixels,
                                                    row_bytes, x, y);
  }
  if (IsGpuContextLost()) {
    return false;
  }

  auto access = WillDrawInternal();

  // The below  write to the resource's SharedImage will need to be preserved in
  // the case of a subsequent CopyOnWrite.
  // TODO(crbug.com/352263194): Logically this bool must already be true
  // (see discussion here:
  // https://chromium-review.googlesource.com/c/chromium/src/+/7557841/comment/bb38e497_ef1efdbc/).
  // Verify that this is the case and update the code here.
  must_preserve_content_on_copy_on_write_ = true;

  auto client_si = resource()->GetSharedImage();
  RasterInterface()->WritePixels(client_si->mailbox(), x, y,
                                 client_si->GetTextureTarget(),
                                 SkPixmap(orig_info, pixels, row_bytes));
  resource()->EndAccess(std::move(access));

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
    result += resource_->EstimatedSizeInBytes() * num_inflight_resources_;
  }
  return result;
}

void Canvas2DResourceProvider::OnContextDestroyed() {
  if (skia_canvas_) {
    skia_canvas_->reset_image_provider();
  }
  canvas_image_provider_.reset();
  if (image_pool_) {
    image_pool_->Clear();
  }
}

scoped_refptr<CanvasResource>
Canvas2DResourceProvider::ProduceCanvasResource() {
  TRACE_EVENT0("blink", "Canvas2DResourceProvider::ProduceCanvasResource");

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
  // backing SharedImage). Hence, we must make sure that we give up any write
  // access.
  EndWriteAccess();

  return resource_;
}

bool Canvas2DResourceProvider::IsValid() const {
  if (IsSoftware()) {
    // Software compositing (which always uses software raster).
    return shared_image_interface_provider_ &&
           shared_image_interface_provider_->SharedImageInterface() &&
           GetSkSurface();
  }

  if (is_accelerated_) {
    // GPU compositing and GPU raster.
    return !IsGpuContextLost();
  }

  // GPU compositing and software raster.
  return !IsGpuContextLost() && GetSkSurface();
}

void Canvas2DResourceProvider::TransferBackFromWebGPU(
    const gpu::SyncToken& webgpu_write_sync_token) {
  if (IsGpuContextLost()) {
    return;
  }

  resource()->EndExternalWrite(webgpu_write_sync_token);
}

gpu::SharedImageUsageSet Canvas2DResourceProvider::GetSharedImageUsageFlags()
    const {
  return image_pool_->GetImageInfo().usage;
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

void Canvas2DResourceProvider::EndWriteAccess() {
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

scoped_refptr<StaticBitmapImage> Canvas2DResourceProvider::Snapshot(
    ImageOrientation orientation) {
  TRACE_EVENT0("blink", "Canvas2DResourceProvider::Snapshot");
  if (!IsValid()) {
    return nullptr;
  }

  // We don't need to EndWriteAccess here since that's required to upload the
  // rendering results to the resource's SharedImage (e.g., for GPU compositing)
  // while in this case we are simply returning the rendered CPU-side results to
  // the client.
  if (!is_accelerated_) {
    return UnacceleratedSnapshot(orientation);
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

void Canvas2DResourceProvider::ReleaseImageProviderImages() {
  if (canvas_image_provider_) {
    canvas_image_provider_->ReleaseLockedImages();
    canvas_image_provider_->UnbindTextureBackedImages();
  }
}

scoped_refptr<UnacceleratedStaticBitmapImage>
Canvas2DResourceProvider::UnacceleratedSnapshot(ImageOrientation orientation) {
  if (!IsValid()) {
    return nullptr;
  }

  cc::PaintImage paint_image;

  auto sk_image = GetSkSurface()->makeImageSnapshot();
  if (sk_image) {
    auto last_snapshot_sk_image_id = snapshot_sk_image_id_;
    snapshot_sk_image_id_ = sk_image->uniqueID();

    // Ensure that a new PaintImage::ContentId is used only when the underlying
    // SkImage changes. This is necessary to ensure that the same image results
    // in a cache hit in cc's ImageDecodeCache.
    if (snapshot_paint_image_content_id_ == PaintImage::kInvalidContentId ||
        last_snapshot_sk_image_id != snapshot_sk_image_id_) {
      snapshot_paint_image_content_id_ = PaintImage::GetNextContentId();
    }

    paint_image =
        PaintImageBuilder::WithDefault()
            .set_id(snapshot_paint_image_id_)
            .set_image(std::move(sk_image), snapshot_paint_image_content_id_)
            .set_hdr_metadata(GetHdrMetadata())
            .TakePaintImage();
  }

  DCHECK(!paint_image.IsTextureBacked());
  return UnacceleratedStaticBitmapImage::Create(std::move(paint_image),
                                                orientation);
}

CanvasImageProvider*
Canvas2DResourceProvider::GetOrCreateCanvasImageProvider() {
  if (!IsAccelerated()) {
    return GetOrCreateSWCanvasImageProvider();
  }

  if (canvas_image_provider_) {
    return canvas_image_provider_.get();
  }

  // Callsites are responsible for checking this before invoking this
  // method.
  CHECK(context_provider_wrapper_);

  // Create an ImageDecodeCache for half float images only if the canvas is
  // using half float back storage.
  cc::ImageDecodeCache* cache_f16 = nullptr;
  if (GetSharedImageFormat() == viz::SinglePlaneFormat::kRGBA_F16) {
    cache_f16 = context_provider_wrapper_->ContextProvider().ImageDecodeCache(
        kRGBA_F16_SkColorType);
  }

  cc::ImageDecodeCache* cache_rgba8 =
      context_provider_wrapper_->ContextProvider().ImageDecodeCache(
          kN32_SkColorType);

  if (!context_provider_wrapper_) {
    context_provider_wrapper_ = SharedGpuContext::ContextProviderWrapper();
    if (context_provider_wrapper_) {
      context_provider_wrapper_->AddObserver(this);
    }
  }
  canvas_image_provider_ = std::make_unique<CanvasImageProvider>(
      cache_rgba8, cache_f16, GetColorSpace(), GetSharedImageFormat(),
      cc::PlaybackImageProvider::RasterMode::kGpu, context_provider_wrapper_);

  return canvas_image_provider_.get();
}

void Canvas2DResourceProvider::RasterRecord(cc::PaintRecord last_recording) {
  if (!is_accelerated_) {
    WillDrawUnaccelerated();
    if (!skia_canvas_) {
      skia_canvas_ = std::make_unique<cc::SkiaPaintCanvas>(
          GetSkSurface()->getCanvas(), GetOrCreateSWCanvasImageProvider());
    }
    cc::PlaybackCallbacks::CustomDataRasterCallback custom_callback;
    if (delegate_) {
      // base::Unretained(this) is safe here because the callback will only be
      // invoked during the scope of skia_canvas_->drawPicture().
      custom_callback = base::BindRepeating(
          &Canvas2DResourceProvider::ApplyAnimatedImageFrameIndexesForId,
          base::Unretained(this));
    }
    skia_canvas_->drawPicture(std::move(last_recording), custom_callback);
    return;
  }

  if (IsGpuContextLost()) {
    return;
  }

  auto access = WillDrawInternal();
  EnsureWriteAccess();

  cc::PlaybackCallbacks::CustomDataRasterCallback custom_callback;
  if (delegate_) {
    // base::Unretained(this) is safe here because the callback will only be
    // invoked during the scope of RasterCHROMIUM() below.
    custom_callback = base::BindRepeating(
        &Canvas2DResourceProvider::ApplyAnimatedImageFrameIndexesForId,
        base::Unretained(this));
  }

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
  size_t max_op_size_hint = gpu::raster::RasterInterface::kDefaultMaxOpSizeHint;
  gfx::Rect full_raster_rect(Size().width(), Size().height());
  gfx::Rect playback_rect(Size().width(), Size().height());
  gfx::Vector2dF post_translate(0.f, 0.f);
  gfx::Vector2dF post_scale(1.f, 1.f);

  const bool can_use_lcd_text = GetAlphaType() == kOpaque_SkAlphaType;
  const auto& caps =
      context_provider_wrapper_->ContextProvider().GetCapabilities();
  bool use_msaa = !caps.msaa_is_slow && !caps.avoid_stencil_buffers;
  ri->BeginRasterCHROMIUM(
      background_color, needs_clear,
      /*msaa_sample_count=*/use_msaa ? 1 : 0,
      use_msaa ? gpu::raster::MsaaMode::kDMSAA : gpu::raster::MsaaMode::kNoMSAA,
      can_use_lcd_text, /*visible=*/true, GetColorSpace(),
      /*hdr_headroom=*/0.f, resource()->GetSharedImage()->mailbox().name);

  ri->RasterCHROMIUM(list.get(), GetOrCreateCanvasImageProvider(), size,
                     full_raster_rect, playback_rect, post_translate,
                     post_scale, /*requires_clear=*/false,
                     /*raster_inducing_scroll_offsets=*/nullptr,
                     &max_op_size_hint, custom_callback);

  ri->EndRasterCHROMIUM();
  resource()->EndAccess(std::move(access));
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
  if (IsSoftware()) {
    if (surface_) {
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
    }
    return;
  }

  std::string path = base::StringPrintf("canvas/ResourceProvider_0x%" PRIXPTR,
                                        reinterpret_cast<uintptr_t>(this));

  resource()->OnMemoryDump(pmd, path);

  std::string cached_path = path + "/cached";
  image_pool_->OnMemoryDump(pmd, cached_path);
}

SkSurface* Canvas2DResourceProvider::GetSkSurface() const {
  if (!surface_) {
    surface_ = CreateSkSurface();
  }
  return surface_.get();
}

size_t Canvas2DResourceProvider::GetSize() const {
  if (!surface_) {
    return 0;
  }
  SkImageInfo info = surface_->imageInfo();
  return info.computeByteSize(info.minRowBytes());
}

std::unique_ptr<Canvas2DResourceProvider>
Canvas2DResourceProvider::CreateWithClear(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    RasterMode raster_mode,
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

  const bool is_accelerated = raster_mode == RasterMode::kGPU;

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

  if (is_accelerated && format != viz::SinglePlaneFormat::kRGBA_F16 &&
      should_force_bgra8_to_rgba) {
    format = viz::SinglePlaneFormat::kRGBA_8888;
  }

  const bool is_mappable_shared_image_allowed =
      is_gpu_compositing_enabled &&
      IsScanoutSupportedForCanvasWithFormat(format, capabilities);

  if (raster_mode == RasterMode::kCPU && !is_mappable_shared_image_allowed) {
    return nullptr;
  }

  // If we cannot use overlay, we have to remove the scanout flag and the
  // concurrent read write flag.
  const auto& shared_image_caps = context_provider_wrapper->ContextProvider()
                                      .SharedImageInterface()
                                      ->GetCapabilities();
  bool is_overlay_supported =
      is_mappable_shared_image_allowed &&
      (!is_accelerated || shared_image_caps.supports_scanout_shared_images);

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
      is_accelerated && format == viz::SinglePlaneFormat::kRGBA_8888) {
    // GPU-accelerated scannout usage on Mac uses IOSurface.  Must switch from
    // RGBA_8888 to BGRA_8888 in that case.
    format = viz::SinglePlaneFormat::kBGRA_8888;
  }
#endif

  auto provider = base::WrapUnique(new Canvas2DResourceProvider(
      size, format, alpha_type, color_space, hdr_metadata,
      context_provider_wrapper, is_accelerated, shared_image_usage_flags,
      delegate));
  if (!provider->IsValid()) {
    return nullptr;
  }

  provider->ClearAtCreation();

  // An error might have occurred while clearing.
  return provider->IsValid() ? std::move(provider) : nullptr;
}

std::unique_ptr<Canvas2DResourceProvider>
Canvas2DResourceProvider::CreateWithClear(
    gfx::Size size,
    const Canvas2DColorParams& color_params,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    RasterMode raster_mode,
    gpu::SharedImageUsageSet shared_image_usage_flags) {
  return CreateWithClear(
      size, color_params.GetSharedImageFormat(), color_params.GetAlphaType(),
      color_params.GetGfxColorSpace(), color_params.GetGfxHdrMetadata(),
      std::move(context_provider_wrapper), raster_mode,
      shared_image_usage_flags);
}

std::unique_ptr<Canvas2DResourceProvider>
Canvas2DResourceProvider::CreateWithClearForSoftwareCompositor(
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

  auto provider = base::WrapUnique(new Canvas2DResourceProvider(
      size, format, alpha_type, color_space, hdr_metadata,
      shared_image_interface_provider, delegate));
  if (provider->IsValid()) {
    provider->ClearAtCreation();
    // The ClearAtCreation() call cannot turn a SW CRPSI invalid.
    CHECK(provider->IsValid());
    return provider;
  }

  return nullptr;
}

void Canvas2DResourceProvider::NotifyGpuContextLostTask(
    base::WeakPtr<Canvas2DResourceProvider> provider) {
  if (provider && provider->delegate_) {
    // Move `provider` as hint that it shouldn't be reused after this point.
    // The `delegate` owns the provider and can delete it in
    // `NotifyGpuContextLost()`.
    std::move(provider)->delegate_->NotifyGpuContextLost();
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
    CanvasResourceProviderDelegate* delegate)
    : is_accelerated_(is_accelerated),
      is_software_(false),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      size_(size),
      format_(format),
      alpha_type_(alpha_type),
      color_space_(color_space),
      hdr_metadata_(hdr_metadata),
      delegate_(delegate),
      snapshot_paint_image_id_(cc::PaintImage::GetNextId()) {
  max_recorded_op_bytes_ = static_cast<size_t>(kMaxRecordedOpKB.Get()) * 1024;
  max_pinned_image_bytes_ = static_cast<size_t>(kMaxPinnedImageKB.Get()) * 1024;
  recorder_ = std::make_unique<MemoryManagedPaintRecorder>(Size(), this);
  if (context_provider_wrapper_) {
    context_provider_wrapper_->AddObserver(this);
    raster_context_provider_ = base::WrapRefCounted(
        context_provider_wrapper_->ContextProvider().RasterContextProvider());
    // Graphite can handle a large buffer size.
    if (context_provider_wrapper_->ContextProvider()
            .GetGpuFeatureInfo()
            .status_values[gpu::GPU_FEATURE_TYPE_SKIA_GRAPHITE] ==
        gpu::kGpuFeatureStatusEnabled) {
      max_recorded_op_bytes_ =
          static_cast<size_t>(kMaxRecordedOpGraphiteKB.Get()) * 1024;
      recorder_->DisableLineDrawingAsPaths();
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
          is_accelerated_ ? "CanvasResourceRaster" : "CanvasResourceRasterGmb",
          is_single_buffered ? 0 : kMaxRecycledCanvasResources,
          expiration_time);
    }
  }

  resource_ = NewOrRecycledResource();

  if (resource_) {
    EnsureWriteAccess();
  }
  CanvasMemoryDumpProvider::Instance()->RegisterClient(this);
}

void Canvas2DResourceProvider::InitializeForRecording(
    cc::PaintCanvas* canvas) const {
  if (delegate_) {
    delegate_->InitializeForRecording(canvas);
  }
}

void Canvas2DResourceProvider::RecordingCleared() {
  must_preserve_content_on_copy_on_write_ = false;
  clear_frame_ = true;
}

CanvasImageProvider*
Canvas2DResourceProvider::GetOrCreateSWCanvasImageProvider() {
  if (canvas_image_provider_) {
    return canvas_image_provider_.get();
  }

  cc::ImageDecodeCache* cache_f16 = nullptr;
  if (GetSharedImageFormat() == viz::SinglePlaneFormat::kRGBA_F16) {
    cache_f16 = &Image::SharedCCDecodeCache(kRGBA_F16_SkColorType);
  }

  cc::ImageDecodeCache* cache_rgba8 =
      &Image::SharedCCDecodeCache(kN32_SkColorType);

  if (!context_provider_wrapper_) {
    context_provider_wrapper_ = SharedGpuContext::ContextProviderWrapper();
    if (context_provider_wrapper_) {
      context_provider_wrapper_->AddObserver(this);
    }
  }
  canvas_image_provider_ = std::make_unique<CanvasImageProvider>(
      cache_rgba8, cache_f16, GetColorSpace(), GetSharedImageFormat(),
      cc::PlaybackImageProvider::RasterMode::kSoftware,
      context_provider_wrapper_);

  return canvas_image_provider_.get();
}

void Canvas2DResourceProvider::SetAnimatedImageFrameIndexes(
    scoped_refptr<const cc::AnimatedImageFrameIndexMap> map) {
  CHECK(canvas_image_provider_);
  canvas_image_provider_->SetAnimatedImageFrameIndexes(map);
}

Canvas2DResourceProvider::Canvas2DResourceProvider(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider,
    CanvasResourceProviderDelegate* delegate)
    : is_accelerated_(false),
      is_software_(true),
      shared_image_interface_provider_(
          shared_image_interface_provider
              ? shared_image_interface_provider->GetWeakPtr()
              : nullptr),
      size_(size),
      format_(format),
      alpha_type_(alpha_type),
      color_space_(color_space),
      hdr_metadata_(hdr_metadata),
      delegate_(delegate),
      snapshot_paint_image_id_(cc::PaintImage::GetNextId()) {
  max_recorded_op_bytes_ = static_cast<size_t>(kMaxRecordedOpKB.Get()) * 1024;
  max_pinned_image_bytes_ = static_cast<size_t>(kMaxPinnedImageKB.Get()) * 1024;
  recorder_ = std::make_unique<MemoryManagedPaintRecorder>(Size(), this);
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
  CanvasMemoryDumpProvider::Instance()->RegisterClient(this);
}

Canvas2DResourceProvider::~Canvas2DResourceProvider() {
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

  // Last chance for outstanding GPU timers to record metrics.
  if (RasterInterface()) {
    CheckGpuTimers(RasterInterface());
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

sk_sp<SkSurface> Canvas2DResourceProvider::CreateSkSurface() const {
  TRACE_EVENT0("blink", "Canvas2DResourceProvider::CreateSkSurface");

  CHECK(!IsAccelerated());

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

SkSurfaceProps Canvas2DResourceProvider::GetSkSurfaceProps() const {
  const bool can_use_lcd_text = GetAlphaType() == kOpaque_SkAlphaType;
  return skia::LegacyDisplayGlobals::ComputeSurfaceProps(can_use_lcd_text);
}

MemoryManagedPaintCanvas& Canvas2DResourceProvider::GetCanvasForTesting() {
  return Recorder().getRecordingCanvas();
}

void Canvas2DResourceProvider::RestoreBackBuffer(const cc::PaintImage& image) {
  DCHECK_EQ(image.height(), Size().height());
  DCHECK_EQ(image.width(), Size().width());

  auto sk_image = image.GetSwSkImage();
  DCHECK(sk_image);
  SkPixmap map;
  sk_image->peekPixels(&map);
  WritePixels(map.info(), map.addr(), map.rowBytes(), /*x=*/0, /*y=*/0);
}

void Canvas2DResourceProvider::ApplyAnimatedImageFrameIndexesForId(
    SkCanvas* canvas,
    uint32_t id) {
  CHECK(delegate_);
  SetAnimatedImageFrameIndexes(delegate_->GetAnimatedImageFrameIndexes(id));
}

void Canvas2DResourceProvider::ClearAtCreation() {
  DCHECK(IsValid());
  MemoryManagedPaintRecorder recorder(Size(), this);
  if (GetAlphaType() == kOpaque_SkAlphaType) {
    recorder.getRecordingCanvas().clear(SkColors::kBlack);
  } else {
    recorder.getRecordingCanvas().clear(SkColors::kTransparent);
  }

  RasterRecord(recorder.ReleaseMainRecording());
}

}  // namespace blink
