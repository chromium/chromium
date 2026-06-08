// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"

#include <inttypes.h>

#include <string>
#include <vector>

#include "base/byte_size.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
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

class FlushForImageListener {
  // With deferred rendering it's possible for a drawImage operation on a canvas
  // to trigger a copy-on-write if another canvas has a read reference to it.
  // This can cause serious regressions due to extra allocations:
  // crbug.com/1030108. FlushForImageListener keeps a list of all active 2d
  // contexts on a thread and notifies them when one is attempting copy-on
  // write. If the notified context has a read reference to the canvas
  // attempting a copy-on-write it then flushes so as to make the copy-on-write
  // unnecessary.
 public:
  static FlushForImageListener* GetFlushForImageListener();
  void AddObserver(FlushForImageObserver* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(FlushForImageObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  void NotifyFlushForImage(cc::PaintImage::ContentId content_id) {
    for (FlushForImageObserver& obs : observers_) {
      obs.OnFlushForImage(content_id);
    }
  }

 private:
  friend class ThreadSpecific<FlushForImageListener>;
  base::ReentrantObserverList<FlushForImageObserver> observers_;
};

static FlushForImageListener* GetFlushForImageListener() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<FlushForImageListener>,
                                  flush_for_image_listener, ());
  return flush_for_image_listener;
}

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

class CanvasImageProvider : public cc::ImageProvider {
 public:
  CanvasImageProvider(cc::ImageDecodeCache* cache_n32,
                      cc::ImageDecodeCache* cache_f16,
                      const gfx::ColorSpace& target_color_space,
                      viz::SharedImageFormat canvas_format,
                      cc::PlaybackImageProvider::RasterMode raster_mode,
                      scoped_refptr<const cc::AnimatedImageFrameIndexMap>
                          animated_image_frame_indexes);
  CanvasImageProvider(const CanvasImageProvider&) = delete;
  CanvasImageProvider& operator=(const CanvasImageProvider&) = delete;
  ~CanvasImageProvider() override = default;

  // cc::ImageProvider implementation.
  cc::ImageProvider::ScopedResult GetRasterContent(
      const cc::DrawImage&) override;

  void ReleaseLockedImages() { locked_images_.clear(); }
  void SetAnimatedImageFrameIndexes(
      scoped_refptr<const cc::AnimatedImageFrameIndexMap> indexes);

 private:
  void CanUnlockImage(ScopedResult);
  void CleanupLockedImages();
  bool IsHardwareDecodeCache() const;

  cc::PlaybackImageProvider::RasterMode raster_mode_;
  bool cleanup_task_pending_ = false;
  Vector<ScopedResult> locked_images_;
  std::optional<cc::PlaybackImageProvider> playback_image_provider_n32_;
  std::optional<cc::PlaybackImageProvider> playback_image_provider_f16_;

  base::WeakPtrFactory<CanvasImageProvider> weak_factory_{this};
};

Canvas2DResourceProviderBitmap::Canvas2DResourceProviderBitmap(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    CanvasResourceProvider::Delegate* delegate)
    : CanvasResourceProvider(kBitmap),
      size_(size),
      format_(format),
      alpha_type_(alpha_type),
      color_space_(color_space),
      hdr_metadata_(hdr_metadata),
      delegate_(delegate) {
  max_recorded_op_bytes_ = static_cast<size_t>(kMaxRecordedOpKB.Get()) * 1024;
  max_pinned_image_bytes_ = static_cast<size_t>(kMaxPinnedImageKB.Get()) * 1024;
  recorder_ = std::make_unique<MemoryManagedPaintRecorder>(Size(), this);
}

Canvas2DResourceProviderBitmap::~Canvas2DResourceProviderBitmap() = default;

SkSurface* Canvas2DResourceProviderBitmap::GetSkSurface() const {
  if (!surface_) {
    surface_ = CreateSkSurface();
  }
  return surface_.get();
}

void Canvas2DResourceProviderBitmap::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd) {
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
}

size_t Canvas2DResourceProviderBitmap::GetSize() const {
  if (!surface_) {
    return 0;
  }
  SkImageInfo info = surface_->imageInfo();
  return info.computeByteSize(info.minRowBytes());
}

void Canvas2DResourceProviderBitmap::InitializeForRecording(
    cc::PaintCanvas* canvas) const {
  if (delegate_) {
    delegate_->InitializeForRecording(canvas);
  }
}

std::unique_ptr<MemoryManagedPaintRecorder>
Canvas2DResourceProviderBitmap::ReleaseRecorder() {
  auto recorder = std::make_unique<MemoryManagedPaintRecorder>(Size(), this);
  recorder_->SetClient(nullptr);
  recorder_.swap(recorder);
  return recorder;
}

void Canvas2DResourceProviderBitmap::SetRecorder(
    std::unique_ptr<MemoryManagedPaintRecorder> recorder) {
  recorder->SetClient(this);
  recorder_ = std::move(recorder);
}

void Canvas2DResourceProviderBitmap::FlushIfRecordingLimitExceeded() {
  if (IsPrinting() && clear_frame_) {
    return;
  }
  if (Recorder().ReleasableOpBytesUsed() > max_recorded_op_bytes_ ||
      Recorder().ReleasableImageBytesUsed() > max_pinned_image_bytes_)
      [[unlikely]] {
    Flush(FlushReason::kOther);
  }
}

scoped_refptr<StaticBitmapImage> Canvas2DResourceProviderBitmap::Snapshot(
    ImageOrientation orientation) {
  TRACE_EVENT0("blink", "Canvas2DResourceProviderBitmap::Snapshot");
  if (!IsValid()) {
    return nullptr;
  }

  Flush();

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

sk_sp<SkSurface> Canvas2DResourceProviderBitmap::CreateSkSurface() const {
  TRACE_EVENT0("blink", "Canvas2DResourceProviderBitmap::CreateSkSurface");

  const auto info = SkImageInfo::Make(
      size_.width(), size_.height(), viz::ToClosestSkColorType(format_),
      kPremul_SkAlphaType, color_space_.ToSkColorSpace());
  const auto props = GetSkSurfaceProps();
  return SkSurfaces::Raster(info, &props);
}

void Canvas2DResourceProviderBitmap::RasterRecord(
    cc::PaintRecord last_recording) {
  if (!skia_canvas_) {
    skia_canvas_ = std::make_unique<cc::SkiaPaintCanvas>(
        GetSkSurface()->getCanvas(), GetOrCreateSWCanvasImageProvider());
  }
  skia_canvas_->drawPicture(std::move(last_recording));
}

bool Canvas2DResourceProviderBitmap::WritePixels(const SkImageInfo& orig_info,
                                                 const void* pixels,
                                                 size_t row_bytes,
                                                 int x,
                                                 int y) {
  TRACE_EVENT0("blink", "Canvas2DResourceProviderBitmap::WritePixels");
  DCHECK(IsValid());
  DCHECK(!Recorder().HasRecordedDrawOps());

  if (!skia_canvas_) {
    skia_canvas_ = std::make_unique<cc::SkiaPaintCanvas>(
        GetSkSurface()->getCanvas(), GetOrCreateSWCanvasImageProvider());
  }

  bool wrote_pixels = GetSkSurface()->getCanvas()->writePixels(
      orig_info, pixels, row_bytes, x, y);

  if (wrote_pixels) {
    // WritePixels content is not saved in recording. Calling WritePixels
    // therefore invalidates `last_recording_` because it's now
    // missing that information.
    last_recording_ = std::nullopt;
  }
  return wrote_pixels;
}

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


base::WeakPtr<Canvas2DResourceProviderSharedImage>
Canvas2DResourceProviderSharedImage::CreateWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

scoped_refptr<CanvasResourceSharedImage>
Canvas2DResourceProviderSharedImage::NewOrRecycledResource() {
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

void Canvas2DResourceProviderSharedImage::OnResourceRefReturned(
    scoped_refptr<CanvasResourceSharedImage>&& resource) {
  if (!resource->IsLost() && resource->HasOneRef() &&
      resource_recycling_enabled_ && image_pool_) {
    image_pool_->ReleaseImage(std::move(resource));
  }
}

base::WeakPtr<CanvasNon2DResourceProviderSharedImage>
CanvasNon2DResourceProviderSharedImage::CreateWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

scoped_refptr<CanvasResourceSharedImage>
CanvasNon2DResourceProviderSharedImage::NewOrRecycledResource() {
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

std::unique_ptr<MemoryManagedPaintRecorder>
Canvas2DResourceProviderSharedImage::ReleaseRecorder() {
  auto recorder = std::make_unique<MemoryManagedPaintRecorder>(Size(), this);
  recorder_->SetClient(nullptr);
  recorder_.swap(recorder);
  DisableLineDrawingAsPathsIfNecessary();
  return recorder;
}

void Canvas2DResourceProviderSharedImage::SetRecorder(
    std::unique_ptr<MemoryManagedPaintRecorder> recorder) {
  recorder->SetClient(this);
  recorder_ = std::move(recorder);
  DisableLineDrawingAsPathsIfNecessary();
}

void Canvas2DResourceProviderSharedImage::FlushIfRecordingLimitExceeded() {
  if (IsPrinting() && clear_frame_) {
    return;
  }
  if (Recorder().ReleasableOpBytesUsed() > max_recorded_op_bytes_ ||
      Recorder().ReleasableImageBytesUsed() > max_pinned_image_bytes_)
      [[unlikely]] {
    Flush(FlushReason::kOther);
  }
}

void Canvas2DResourceProviderSharedImage::SetResourceRecyclingEnabled(
    bool value) {
  resource_recycling_enabled_ = value;
  if (!resource_recycling_enabled_) {
    ClearUnusedResources();
  }
}

void Canvas2DResourceProviderSharedImage::OnContextLost() {
  if (notified_context_lost_) {
    return;
  }
  ClearUnusedResources();
  // Notify the owner of this resource provider that the GPU context was
  // lost. The call is done in a separate task, so that the owner can delete
  // this resource provider if needed.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &Canvas2DResourceProviderSharedImage::NotifyGpuContextLostTask,
          CreateWeakPtr()));
  notified_context_lost_ = true;
}

void Canvas2DResourceProviderSharedImage::OnGpuChannelLost() {
  OnContextLost();
}

bool Canvas2DResourceProviderSharedImage::ShouldReplaceTargetBuffer(
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
  if (is_accelerated_) {
    // Another context may have a read reference to this resource. Flush the
    // deferred queue in that context so that we don't need to copy.
    GetFlushForImageListener()->NotifyFlushForImage(content_id);
  }

  return !resource_->HasOneRef();
}

std::unique_ptr<gpu::RasterScopedAccess>
Canvas2DResourceProviderSharedImage::WillDrawInternal() {
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

bool CanvasNon2DResourceProviderSharedImage::ShouldReplaceTargetBuffer(
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
  if (is_accelerated_) {
    // Another context may have a read reference to this resource. Flush the
    // deferred queue in that context so that we don't need to copy.
    GetFlushForImageListener()->NotifyFlushForImage(content_id);
  }

  return !resource_->HasOneRef();
}

std::unique_ptr<gpu::RasterScopedAccess>
CanvasNon2DResourceProviderSharedImage::WillDrawInternal() {
  DCHECK(resource_);

  // Since the resource will be updated, the cached snapshot is no longer valid.
  // Note that this is valid for single buffered mode also, since while the
  // resource/mailbox remains the same, the snapshot needs an updated sync token
  // for these writes.
  cached_snapshot_.reset();

  // Determine if a new resource is needed for accelerated resources. Note that
  // for unaccelerated resources, writes to the SharedImage are deferred to
  // ProduceCanvasResource.
  if (!is_accelerated_ || !ShouldReplaceTargetBuffer(cached_content_id_)) {
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

void Canvas2DResourceProviderSharedImage::WillDrawUnaccelerated() {
  CHECK(!IsAccelerated());

  if (IsSoftware()) {
    return;
  }
  cached_snapshot_.reset();
  EnsureWriteAccess();
}

ScopedRasterTimer
Canvas2DResourceProviderSharedImage::CreateScopedRasterTimer() {
  return ScopedRasterTimer(IsAccelerated() ? RasterInterface() : nullptr, *this,
                           always_enable_raster_timers_for_testing_);
}

void Canvas2DResourceProviderSharedImage::
    DisableLineDrawingAsPathsIfNecessary() {
  if (context_provider_wrapper_ &&
      context_provider_wrapper_->ContextProvider()
              .GetGpuFeatureInfo()
              .status_values[gpu::GPU_FEATURE_TYPE_SKIA_GRAPHITE] ==
          gpu::kGpuFeatureStatusEnabled) {
    Recorder().DisableLineDrawingAsPaths();
  }
}

void CanvasNon2DResourceProviderSharedImage::PrepareForWebGPUDummyMailbox() {
  if (resource()) {
    resource()->PrepareForWebGPUDummyMailbox();
  }
}

bool Canvas2DResourceProviderSharedImage::WritePixels(
    const SkImageInfo& orig_info,
    const void* pixels,
    size_t row_bytes,
    int x,
    int y) {
  TRACE_EVENT0("blink", "Canvas2DResourceProviderSharedImage::WritePixels");
  if (!is_accelerated_) {
    WillDrawUnaccelerated();
    DCHECK(IsValid());
    DCHECK(!Recorder().HasRecordedDrawOps());

    if (!skia_canvas_) {
      skia_canvas_ = std::make_unique<cc::SkiaPaintCanvas>(
          GetSkSurface()->getCanvas(), GetOrCreateSWCanvasImageProvider());
    }

    bool wrote_pixels = GetSkSurface()->getCanvas()->writePixels(
        orig_info, pixels, row_bytes, x, y);

    if (wrote_pixels) {
      // WritePixels content is not saved in recording. Calling WritePixels
      // therefore invalidates `last_recording_` because it's now
      // missing that information.
      last_recording_ = std::nullopt;
    }
    return wrote_pixels;
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

bool CanvasNon2DResourceProviderSharedImage::UploadToBackingSharedImage(
    const SkPixmap& pixmap,
    uint32_t src_x,
    uint32_t src_y) {
  CHECK(is_accelerated_);

  const int dest_width = Size().width();
  const int dest_height = Size().height();

  SkPixmap subset;
  if (!pixmap.extractSubset(
          &subset,
          SkIRect::MakeXYWH(static_cast<int>(src_x), static_cast<int>(src_y),
                            dest_width, dest_height))) {
    return false;
  }

  TRACE_EVENT0("blink",
               "CanvasNon2DResourceProviderSharedImage::"
               "UploadToBackingSharedImage");
  if (IsGpuContextLost()) {
    return false;
  }

  auto access = WillDrawInternal();

  auto client_si = resource()->GetSharedImage();
  RasterInterface()->WritePixels(client_si->mailbox(), /*dst_x_offset=*/0,
                                 /*dst_y_offset=*/0,
                                 client_si->GetTextureTarget(), subset);
  resource()->EndAccess(std::move(access));

  is_cleared_ = true;

  return true;
}

void CanvasNon2DResourceProviderSharedImage::OnContextLost() {
  if (notified_context_lost_) {
    return;
  }
  ClearUnusedResources();
  // Notify the owner of this resource provider that the GPU context was
  // lost. The call is done in a separate task, so that the owner can delete
  // this resource provider if needed.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CanvasNon2DResourceProviderSharedImage::NotifyGpuContextLostTask,
          CreateWeakPtr()));
  notified_context_lost_ = true;
}

void CanvasNon2DResourceProviderSharedImage::OnGpuChannelLost() {
  OnContextLost();
}

bool CanvasNon2DResourceProviderSharedImage::CopyToBackingSharedImage(
    const scoped_refptr<gpu::ClientSharedImage>& shared_image,
    uint32_t src_x,
    uint32_t src_y,
    const gpu::SyncToken& ready_sync_token,
    gpu::SyncToken& completion_sync_token) {
  CHECK(is_accelerated_);
  gpu::raster::RasterInterface* raster = RasterInterface();
  if (!raster) {
    return false;
  }

  if (IsGpuContextLost()) {
    return false;
  }

  gfx::Rect copy_rect(src_x, src_y, Size().width(), Size().height());

  EndWriteAccess();
  auto dst_access = WillDrawInternal();

  auto dst_client_si = resource()->GetSharedImage();
  if (!dst_client_si) {
    resource()->EndAccess(std::move(dst_access));
    return false;
  }

  std::unique_ptr<gpu::RasterScopedAccess> src_access =
      shared_image->BeginRasterAccess(raster, ready_sync_token,
                                      /*readonly=*/true);
  raster->CopySharedImage(shared_image->mailbox(), dst_client_si->mailbox(),
                          /*xoffset=*/0,
                          /*yoffset=*/0, copy_rect.x(), copy_rect.y(),
                          copy_rect.width(), copy_rect.height());
  completion_sync_token =
      gpu::RasterScopedAccess::EndAccess(std::move(src_access));
  resource()->EndAccess(std::move(dst_access));
  is_cleared_ = true;
  return true;
}

scoped_refptr<gpu::ClientSharedImage>
CanvasNon2DResourceProviderSharedImage::BeginExternalOverwrite(
    gpu::SyncToken& internal_access_sync_token) {
  DCHECK(is_accelerated_);

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

base::ByteSize Canvas2DResourceProviderSharedImage::EstimatedSizeInBytes()
    const {
  base::ByteSize result;
  if (resource_) {
    result += resource_->EstimatedSizeInBytes() * num_inflight_resources_;
  }
  return result;
}

void Canvas2DResourceProviderSharedImage::OnContextDestroyed() {
  if (skia_canvas_) {
    skia_canvas_->reset_image_provider();
  }
  canvas_image_provider_.reset();
  if (image_pool_) {
    image_pool_->Clear();
  }
}

scoped_refptr<CanvasResource>
Canvas2DResourceProviderSharedImage::ProduceCanvasResource(FlushReason reason) {
  TRACE_EVENT0("blink",
               "Canvas2DResourceProviderSharedImage::ProduceCanvasResource");
  if (IsSoftware()) {
    DCHECK(GetSkSurface());
    scoped_refptr<CanvasResource> output_resource = NewOrRecycledResource();
    if (!output_resource) {
      return nullptr;
    }

    Flush(reason);

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
  // backing SharedImage). Hence, we must make sure that the SI is updated to
  // reflect the ops made in the current write access (if any) and give up any
  // such write access.
  Flush(reason);
  EndWriteAccess();

  return resource_;
}


void CanvasNon2DResourceProviderSharedImage::OnContextDestroyed() {
  if (skia_canvas_) {
    skia_canvas_->reset_image_provider();
  }
  canvas_image_provider_.reset();
  if (image_pool_) {
    image_pool_->Clear();
  }
}

scoped_refptr<CanvasResource>
CanvasNon2DResourceProviderSharedImage::ProduceCanvasResource() {
  TRACE_EVENT0("blink",
               "CanvasNon2DResourceProviderSharedImage::ProduceCanvasResource");
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

bool Canvas2DResourceProviderSharedImage::IsValid() const {
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

bool CanvasNon2DResourceProviderSharedImage::IsValid() const {
  if (IsSoftware()) {
    return shared_image_interface_provider_ &&
           shared_image_interface_provider_->SharedImageInterface() &&
           GetSkSurface();
  }

  return !IsGpuContextLost();
}

void Canvas2DResourceProviderSharedImage::TransferBackFromWebGPU(
    const gpu::SyncToken& webgpu_write_sync_token) {
  if (IsGpuContextLost()) {
    return;
  }

  resource()->EndExternalWrite(webgpu_write_sync_token);
}

void CanvasNon2DResourceProviderSharedImage::EndExternalWrite(
    const gpu::SyncToken& external_write_sync_token) {
  if (IsGpuContextLost()) {
    return;
  }

  resource()->EndExternalWrite(external_write_sync_token);
}

gpu::SharedImageUsageSet
Canvas2DResourceProviderSharedImage::GetSharedImageUsageFlags() const {
  return image_pool_->GetImageInfo().usage;
}

gpu::SharedImageUsageSet
CanvasNon2DResourceProviderSharedImage::GetSharedImageUsageFlags() const {
  return image_pool_->GetImageInfo().usage;
}

scoped_refptr<CanvasResource>
CanvasNon2DResourceProviderSharedImage::DoExternalOverdrawAndProduceResource(
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
CanvasNon2DResourceProviderSharedImage::DoExternalOverdrawAndSnapshot(
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

void Canvas2DResourceProviderSharedImage::EnsureWriteAccess() {
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

void Canvas2DResourceProviderSharedImage::EndWriteAccess() {
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

scoped_refptr<StaticBitmapImage> Canvas2DResourceProviderSharedImage::Snapshot(
    ImageOrientation orientation) {
  TRACE_EVENT0("blink", "Canvas2DResourceProviderSharedImage::Snapshot");
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
    Flush(FlushReason::kOther);
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

scoped_refptr<UnacceleratedStaticBitmapImage>
Canvas2DResourceProviderSharedImage::UnacceleratedSnapshot(
    ImageOrientation orientation) {
  if (!IsValid()) {
    return nullptr;
  }

  Flush();

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

void CanvasNon2DResourceProviderSharedImage::EnsureWriteAccess() {
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

void CanvasNon2DResourceProviderSharedImage::EndWriteAccess() {
  DCHECK(!resource()->is_cross_thread());

  if (!current_resource_has_write_access_ || IsGpuContextLost()) {
    return;
  }

  if (!is_accelerated_) {
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

scoped_refptr<StaticBitmapImage>
CanvasNon2DResourceProviderSharedImage::Snapshot(ImageOrientation orientation) {
  TRACE_EVENT0("blink", "CanvasNon2DResourceProviderSharedImage::Snapshot");
  if (!IsValid()) {
    return nullptr;
  }

  // We don't need to EndWriteAccess here since that's required to upload the
  // rendering results to the resource's SharedImage (e.g., for GPU compositing)
  // while in this case we are simply returning the rendered CPU-side results to
  // the client.
  if (!is_accelerated_) {
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

CanvasImageProvider*
Canvas2DResourceProviderSharedImage::GetOrCreateCanvasImageProvider() {
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

  canvas_image_provider_ = std::make_unique<CanvasImageProvider>(
      cache_rgba8, cache_f16, GetColorSpace(), GetSharedImageFormat(),
      cc::PlaybackImageProvider::RasterMode::kGpu,
      delegate_ ? delegate_->GetAnimatedImageFrameIndexes() : nullptr);

  return canvas_image_provider_.get();
}

void Canvas2DResourceProviderSharedImage::RasterRecord(
    cc::PaintRecord last_recording) {
  if (!is_accelerated_) {
    WillDrawUnaccelerated();
    if (!skia_canvas_) {
      skia_canvas_ = std::make_unique<cc::SkiaPaintCanvas>(
          GetSkSurface()->getCanvas(), GetOrCreateSWCanvasImageProvider());
    }
    skia_canvas_->drawPicture(std::move(last_recording));
    return;
  }

  if (IsGpuContextLost()) {
    return;
  }

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

  ri->RasterCHROMIUM(
      list.get(), GetOrCreateCanvasImageProvider(), size, full_raster_rect,
      playback_rect, post_translate, post_scale, /*requires_clear=*/false,
      /*raster_inducing_scroll_offsets=*/nullptr, &max_op_size_hint);

  ri->EndRasterCHROMIUM();
  resource()->EndAccess(std::move(access));
}

// For WebGpu RecyclableCanvasResource.
void CanvasNon2DResourceProviderSharedImage::
    OnAcquireRecyclableCanvasResource() {
  EnsureWriteAccess();
}
void CanvasNon2DResourceProviderSharedImage::OnDestroyRecyclableCanvasResource(
    const gpu::SyncToken& sync_token) {
  // RecyclableCanvasResource should be the only one that holds onto
  // |resource_|.
  DCHECK(resource()->HasOneRef());
  resource()->WaitSyncToken(sync_token);
}

void Canvas2DResourceProviderSharedImage::OnFlushForImage(
    cc::PaintImage::ContentId content_id) {
  if (Recorder().getRecordingCanvas().IsCachingImage(content_id)) {
    Flush();
  }
  if (cached_snapshot_ &&
      cached_snapshot_->PaintImageForCurrentFrame().GetContentIdForFrame(0) ==
          content_id) {
    // This handles the case where the cached snapshot is referenced by an
    // ImageBitmap that is being transferred to a worker.
    cached_snapshot_.reset();
  }
}

void CanvasNon2DResourceProviderSharedImage::OnFlushForImage(
    cc::PaintImage::ContentId content_id) {
  if (cached_snapshot_ &&
      cached_snapshot_->PaintImageForCurrentFrame().GetContentIdForFrame(0) ==
          content_id) {
    // This handles the case where the cached snapshot is referenced by an
    // ImageBitmap that is being transferred to a worker.
    cached_snapshot_.reset();
  }
}

void Canvas2DResourceProviderSharedImage::OnMemoryDump(
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

SkSurface* Canvas2DResourceProviderSharedImage::GetSkSurface() const {
  if (!surface_) {
    surface_ = CreateSkSurface();
  }
  return surface_.get();
}

size_t Canvas2DResourceProviderSharedImage::GetSize() const {
  if (!surface_) {
    return 0;
  }
  SkImageInfo info = surface_->imageInfo();
  return info.computeByteSize(info.minRowBytes());
}

std::unique_ptr<Canvas2DResourceProviderBitmap>
Canvas2DResourceProviderBitmap::CreateWithClear(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    CanvasResourceProvider::Delegate* delegate) {
  auto provider = base::WrapUnique<Canvas2DResourceProviderBitmap>(
      new Canvas2DResourceProviderBitmap(size, format, alpha_type, color_space,
                                         hdr_metadata, delegate));
  if (provider->IsValid()) {
    provider->ClearAtCreation();
    // The ClearAtCreation() call cannot turn a CRPBitmap invalid.
    CHECK(provider->IsValid());
    return provider;
  }
  return nullptr;
}

std::unique_ptr<Canvas2DResourceProviderSharedImage>
Canvas2DResourceProviderSharedImage::CreateWithClear(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    RasterMode raster_mode,
    gpu::SharedImageUsageSet shared_image_usage_flags,
    CanvasResourceProvider::Delegate* delegate) {
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

  auto provider = std::make_unique<Canvas2DResourceProviderSharedImage>(
      size, format, alpha_type, color_space, hdr_metadata,
      context_provider_wrapper, is_accelerated, shared_image_usage_flags,
      delegate);
  if (!provider->IsValid()) {
    return nullptr;
  }

  provider->ClearAtCreation();

  // An error might have occurred while clearing.
  return provider->IsValid() ? std::move(provider) : nullptr;
}

std::unique_ptr<Canvas2DResourceProviderSharedImage>
Canvas2DResourceProviderSharedImage::CreateWithClear(
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

std::unique_ptr<Canvas2DResourceProviderSharedImage>
Canvas2DResourceProviderSharedImage::CreateWithClearForSoftwareCompositor(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider,
    CanvasResourceProvider::Delegate* delegate) {
  if (SharedGpuContext::IsGpuCompositingEnabled()) {
    return nullptr;
  }

  CHECK(format == viz::SharedImageFormat::N32Format() ||
        format == viz::SinglePlaneFormat::kRGBA_F16);

  auto provider = std::make_unique<Canvas2DResourceProviderSharedImage>(
      size, format, alpha_type, color_space, hdr_metadata,
      shared_image_interface_provider, delegate);
  if (provider->IsValid()) {
    provider->ClearAtCreation();
    // The ClearAtCreation() call cannot turn a SW CRPSI invalid.
    CHECK(provider->IsValid());
    return provider;
  }

  return nullptr;
}

std::unique_ptr<CanvasNon2DResourceProviderSharedImage>
CanvasNon2DResourceProviderSharedImage::Create(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    gpu::SharedImageUsageSet shared_image_usage_flags,
    CanvasResourceProvider::Delegate* delegate) {
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

  auto provider = std::make_unique<CanvasNon2DResourceProviderSharedImage>(
      size, format, alpha_type, color_space, hdr_metadata,
      context_provider_wrapper,
      /*is_accelerated=*/true, shared_image_usage_flags, delegate);

  return provider->IsValid() ? std::move(provider) : nullptr;
}

std::unique_ptr<CanvasNon2DResourceProviderSharedImage>
CanvasNon2DResourceProviderSharedImage::Create(
    gfx::Size size,
    const Canvas2DColorParams& color_params,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    gpu::SharedImageUsageSet shared_image_usage_flags) {
  return Create(size, color_params.GetSharedImageFormat(),
                color_params.GetAlphaType(), color_params.GetGfxColorSpace(),
                color_params.GetGfxHdrMetadata(),
                std::move(context_provider_wrapper), shared_image_usage_flags);
}

std::unique_ptr<CanvasNon2DResourceProviderSharedImage>
CanvasNon2DResourceProviderSharedImage::CreateForWebGPU(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    gpu::SharedImageUsageSet shared_image_usage_flags,
    CanvasResourceProvider::Delegate* delegate) {
  auto context_provider_wrapper = SharedGpuContext::ContextProviderWrapper();
  // The SharedImages created by this provider serve as a means of import/export
  // between VideoFrames/canvas and WebGPU, e.g.:
  // * Import from VideoFrames into WebGPU via CreateExternalTexture() (the
  //   WebGPU textures will then be read by clients)
  // * Export from WebGPU into a static bitmap image via
  //   GpuCanvasContext::{PaintRenderingResultsToSnapshot, GetImage}() (the
  //   export happens via the WebGPU interface)
  // Hence, both WEBGPU_READ and WEBGPU_WRITE usage are needed here.
  return CanvasNon2DResourceProviderSharedImage::Create(
      size, format, alpha_type, color_space, hdr_metadata,
      std::move(context_provider_wrapper),
      shared_image_usage_flags | gpu::SHARED_IMAGE_USAGE_WEBGPU_READ |
          gpu::SHARED_IMAGE_USAGE_WEBGPU_WRITE,
      delegate);
}

std::unique_ptr<CanvasNon2DResourceProviderSharedImage>
CanvasNon2DResourceProviderSharedImage::CreateForSoftwareCompositor(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider,
    CanvasResourceProvider::Delegate* delegate) {
  if (SharedGpuContext::IsGpuCompositingEnabled()) {
    return nullptr;
  }

  CHECK(format == viz::SharedImageFormat::N32Format() ||
        format == viz::SinglePlaneFormat::kRGBA_F16);

  auto provider = std::make_unique<CanvasNon2DResourceProviderSharedImage>(
      size, format, alpha_type, color_space, hdr_metadata,
      shared_image_interface_provider, delegate);
  return provider->IsValid() ? std::move(provider) : nullptr;
}

std::unique_ptr<CanvasNon2DResourceProviderSharedImage>
CanvasNon2DResourceProviderSharedImage::CreateForSoftwareCompositor(
    gfx::Size size,
    const Canvas2DColorParams& color_params,
    WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider) {
  return CreateForSoftwareCompositor(
      size, color_params.GetSharedImageFormat(), color_params.GetAlphaType(),
      color_params.GetGfxColorSpace(), color_params.GetGfxHdrMetadata(),
      shared_image_interface_provider);
}

CanvasImageProvider::CanvasImageProvider(
    cc::ImageDecodeCache* cache_n32,
    cc::ImageDecodeCache* cache_f16,
    const gfx::ColorSpace& target_color_space,
    viz::SharedImageFormat canvas_format,
    cc::PlaybackImageProvider::RasterMode raster_mode,
    scoped_refptr<const cc::AnimatedImageFrameIndexMap>
        animated_image_frame_indexes)
    : raster_mode_(raster_mode) {
  std::optional<cc::PlaybackImageProvider::Settings> settings =
      cc::PlaybackImageProvider::Settings();
  settings->raster_mode = raster_mode_;
  settings->image_to_current_frame_index = animated_image_frame_indexes;

  cc::TargetColorParams target_color_params;
  target_color_params.color_space = target_color_space;
  playback_image_provider_n32_.emplace(cache_n32, target_color_params,
                                       std::move(settings));
  // If the image provider may require to decode to half float instead of
  // uint8, create a f16 PlaybackImageProvider with the passed cache.
  if (canvas_format == viz::SinglePlaneFormat::kRGBA_F16) {
    DCHECK(cache_f16);
    settings = cc::PlaybackImageProvider::Settings();
    settings->raster_mode = raster_mode_;
    settings->image_to_current_frame_index = animated_image_frame_indexes;
    playback_image_provider_f16_.emplace(cache_f16, target_color_params,
                                         std::move(settings));
  }
}

void CanvasImageProvider::SetAnimatedImageFrameIndexes(
    scoped_refptr<const cc::AnimatedImageFrameIndexMap> indexes) {
  if (playback_image_provider_n32_) {
    playback_image_provider_n32_->SetAnimatedImageFrameIndexes(indexes);
  }
  if (playback_image_provider_f16_) {
    playback_image_provider_f16_->SetAnimatedImageFrameIndexes(indexes);
  }
}

cc::ImageProvider::ScopedResult CanvasImageProvider::GetRasterContent(
    const cc::DrawImage& draw_image) {
  cc::PaintImage paint_image = draw_image.paint_image();
  if (paint_image.IsDeferredPaintRecord()) {
    CHECK(!paint_image.IsPaintWorklet());
    scoped_refptr<CanvasDeferredPaintRecord> canvas_deferred_paint_record(
        static_cast<CanvasDeferredPaintRecord*>(
            paint_image.deferred_paint_record().get()));
    return cc::ImageProvider::ScopedResult(
        canvas_deferred_paint_record->GetPaintRecord());
  }

  // TODO(xidachen): Ensure this function works for paint worklet generated
  // images.
  // If we like to decode high bit depth image source to half float backed
  // image, we need to sniff the image bit depth here to avoid double decoding.
  ImageProvider::ScopedResult scoped_decoded_image;
  if (playback_image_provider_f16_ &&
      draw_image.paint_image().is_high_bit_depth()) {
    scoped_decoded_image =
        playback_image_provider_f16_->GetRasterContent(draw_image);
  } else {
    scoped_decoded_image =
        playback_image_provider_n32_->GetRasterContent(draw_image);
  }

  // Holding onto locked images here is a performance optimization for the
  // gpu image decode cache.  For that cache, it is expensive to lock and
  // unlock gpu discardable, and so it is worth it to hold the lock on
  // these images across multiple potential decodes.  In the software case,
  // locking in this manner makes it easy to run out of discardable memory
  // (backed by shared memory sometimes) because each per-colorspace image
  // decode cache has its own limit.  In the software case, just unlock
  // immediately and let the discardable system manage the cache logic
  // behind the scenes.
  if (!scoped_decoded_image.needs_unlock() || !IsHardwareDecodeCache()) {
    return scoped_decoded_image;
  }

  constexpr int kMaxLockedImagesCount = 500;
  if (!scoped_decoded_image.decoded_image().is_budgeted() ||
      locked_images_.size() > kMaxLockedImagesCount) {
    // If we have exceeded the budget, ReleaseLockedImages any locked decodes.
    ReleaseLockedImages();
  }

  auto decoded_draw_image = scoped_decoded_image.decoded_image();
  return ScopedResult(decoded_draw_image,
                      base::BindOnce(&CanvasImageProvider::CanUnlockImage,
                                     weak_factory_.GetWeakPtr(),
                                     std::move(scoped_decoded_image)));
}

void CanvasImageProvider::CanUnlockImage(ScopedResult image) {
  // We should early out and avoid calling this function for software decodes.
  DCHECK(IsHardwareDecodeCache());

  // Because these image decodes are being done in javascript calling into
  // canvas code, there's no obvious time to do the cleanup.  To handle this,
  // post a cleanup task to run after javascript is done running.
  if (!cleanup_task_pending_) {
    cleanup_task_pending_ = true;
    ThreadScheduler::Current()->CleanupTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&CanvasImageProvider::CleanupLockedImages,
                                  weak_factory_.GetWeakPtr()));
  }

  locked_images_.push_back(std::move(image));
}

void CanvasImageProvider::CleanupLockedImages() {
  cleanup_task_pending_ = false;
  ReleaseLockedImages();
}

bool CanvasImageProvider::IsHardwareDecodeCache() const {
  return raster_mode_ != cc::PlaybackImageProvider::RasterMode::kSoftware;
}

CanvasResourceProvider::CanvasResourceProvider(const ResourceProviderType& type)
    : type_(type), snapshot_paint_image_id_(cc::PaintImage::GetNextId()) {
  CanvasMemoryDumpProvider::Instance()->RegisterClient(this);
}

CanvasResourceProvider::~CanvasResourceProvider() {
  CanvasMemoryDumpProvider::Instance()->UnregisterClient(this);
}


void CanvasResourceProvider::NotifyWillTransfer(
    cc::PaintImage::ContentId content_id) {
  // This is called when an ImageBitmap is about to be transferred. All
  // references to such a bitmap on the current thread must be released, which
  // means that DisplayItemLists that reference it must be flushed.
  GetFlushForImageListener()->NotifyFlushForImage(content_id);
}

CanvasImageProvider*
CanvasResourceProvider::GetOrCreateSWCanvasImageProvider() {
  if (canvas_image_provider_) {
    return canvas_image_provider_.get();
  }

  // Create an ImageDecodeCache for half float images only if the canvas is
  // using half float back storage.
  cc::ImageDecodeCache* cache_f16 = nullptr;
  if (GetSharedImageFormat() == viz::SinglePlaneFormat::kRGBA_F16) {
    cache_f16 = &Image::SharedCCDecodeCache(kRGBA_F16_SkColorType);
  }

  cc::ImageDecodeCache* cache_rgba8 =
      &Image::SharedCCDecodeCache(kN32_SkColorType);

  canvas_image_provider_ = std::make_unique<CanvasImageProvider>(
      cache_rgba8, cache_f16, GetColorSpace(), GetSharedImageFormat(),
      cc::PlaybackImageProvider::RasterMode::kSoftware,
      GetDelegate() ? GetDelegate()->GetAnimatedImageFrameIndexes() : nullptr);

  return canvas_image_provider_.get();
}

void CanvasResourceProvider::RecordingCleared() {

  // Since the recording has been cleared, it contains no draw commands and it
  // is now safe to discard the old copy of canvas content on a subsequent
  // CopyOnWrite.
  must_preserve_content_on_copy_on_write_ = false;
  clear_frame_ = true;
}

MemoryManagedPaintCanvas& CanvasResourceProvider::GetCanvasForTesting() {
  return Recorder().getRecordingCanvas();
}


SkSurfaceProps CanvasResourceProvider::GetSkSurfaceProps() const {
  const bool can_use_lcd_text = GetAlphaType() == kOpaque_SkAlphaType;
  return skia::LegacyDisplayGlobals::ComputeSurfaceProps(can_use_lcd_text);
}

ScopedRasterTimer CanvasResourceProvider::CreateScopedRasterTimer() {
  return ScopedRasterTimer(nullptr, *this,
                           always_enable_raster_timers_for_testing_);
}

void CanvasNon2DResourceProviderSharedImage::FlushRecording(
    cc::PaintRecord last_recording) {
  if (!is_accelerated_) {
    if (!skia_canvas_) {
      if (!canvas_image_provider_) {
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
            delegate_ ? delegate_->GetAnimatedImageFrameIndexes() : nullptr);
      }
      skia_canvas_ = std::make_unique<cc::SkiaPaintCanvas>(
          GetSkSurface()->getCanvas(), canvas_image_provider_.get());
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

    if (!canvas_image_provider_) {
      // Create an ImageDecodeCache for half float images only if the canvas is
      // using half float back storage.
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
          delegate_ ? delegate_->GetAnimatedImageFrameIndexes() : nullptr);
    }

    ri->RasterCHROMIUM(
        list.get(), canvas_image_provider_.get(), size, full_raster_rect,
        playback_rect, post_translate, post_scale, /*requires_clear=*/false,
        /*raster_inducing_scroll_offsets=*/nullptr, &max_op_size_hint);

    ri->EndRasterCHROMIUM();
    resource()->EndAccess(std::move(access));
  }

  // Images are locked for the duration of the rasterization, in case they get
  // used multiple times. We can unlock them once the rasterization is complete.
  if (canvas_image_provider_) {
    canvas_image_provider_->ReleaseLockedImages();
  }
}

std::optional<cc::PaintRecord> CanvasResourceProvider::Flush(
    FlushReason reason /*=FlushReason::kOther*/) {
  if (!Recorder().HasReleasableDrawOps()) {
    return std::nullopt;
  }
  auto timer = CreateScopedRasterTimer();
  bool want_to_print = IsPrinting() || reason == FlushReason::kPrinting ||
                       reason == FlushReason::kCanvasPushFrameWhilePrinting;
  bool preserve_recording = want_to_print && clear_frame_;

  // If a previous flush rasterized some paint ops, we lost part of the
  // recording and must fallback to raster printing instead of vectorial
  // printing.
  clear_frame_ = false;
  cc::PaintRecord recording;
  recording = Recorder().ReleaseMainRecording();
  if (canvas_image_provider_ && GetDelegate()) {
    canvas_image_provider_->SetAnimatedImageFrameIndexes(
        GetDelegate()->GetAnimatedImageFrameIndexes());
  }
  RasterRecord(recording);
  // Images are locked for the duration of the rasterization, in case they get
  // used multiple times. We can unlock them once the rasterization is complete.
  if (canvas_image_provider_) {
    canvas_image_provider_->ReleaseLockedImages();
  }

  last_recording_ =
      preserve_recording ? std::optional(recording) : std::nullopt;

  return recording;
}

void Canvas2DResourceProviderSharedImage::NotifyGpuContextLostTask(
    base::WeakPtr<Canvas2DResourceProviderSharedImage> provider) {
  if (provider && provider->delegate_) {
    // Move `provider` as hint that it shouldn't be reused after this point.
    // The `delegate` owns the provider and can delete it in
    // `NotifyGpuContextLost()`.
    std::move(provider)->delegate_->NotifyGpuContextLost();
  }
}

void CanvasNon2DResourceProviderSharedImage::NotifyGpuContextLostTask(
    base::WeakPtr<CanvasNon2DResourceProviderSharedImage> provider) {
  if (provider && provider->delegate_) {
    // Move `provider` as hint that it shouldn't be reused after this point.
    // The `delegate` owns the provider and can delete it in
    // `NotifyGpuContextLost()`.
    std::move(provider)->delegate_->NotifyGpuContextLost();
  }
}

Canvas2DResourceProviderSharedImage::Canvas2DResourceProviderSharedImage(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    bool is_accelerated,
    gpu::SharedImageUsageSet shared_image_usage_flags,
    CanvasResourceProvider::Delegate* delegate)
    : CanvasResourceProvider(kSharedImage),
      is_accelerated_(is_accelerated),
      is_software_(false),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      size_(size),
      format_(format),
      alpha_type_(alpha_type),
      color_space_(color_space),
      hdr_metadata_(hdr_metadata),
      delegate_(delegate) {
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
                    CanvasResourceProvider::kUnusedResourceExpirationTime)
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
  GetFlushForImageListener()->AddObserver(this);

  if (resource_) {
    EnsureWriteAccess();
  }
}

void Canvas2DResourceProviderSharedImage::InitializeForRecording(
    cc::PaintCanvas* canvas) const {
  if (delegate_) {
    delegate_->InitializeForRecording(canvas);
  }
}

Canvas2DResourceProviderSharedImage::Canvas2DResourceProviderSharedImage(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider,
    CanvasResourceProvider::Delegate* delegate)
    : CanvasResourceProvider(kSharedImage),
      is_accelerated_(false),
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
      delegate_(delegate) {
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
}

Canvas2DResourceProviderSharedImage::~Canvas2DResourceProviderSharedImage() {
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
    GetFlushForImageListener()->RemoveObserver(this);
  }

  // Last chance for outstanding GPU timers to record metrics.
  if (RasterInterface()) {
    CheckGpuTimers(RasterInterface());
  }

  UMA_HISTOGRAM_EXACT_LINEAR("Blink.Canvas.MaximumInflightResources",
                             max_inflight_resources_, 20);
}

void Canvas2DResourceProviderSharedImage::ClearUnusedResources() {
  if (image_pool_) {
    image_pool_->Clear();
  }
}

bool Canvas2DResourceProviderSharedImage::
    unused_resources_reclaim_timer_is_running_for_testing() const {
  return image_pool_ ? image_pool_->IsReclaimTimerRunningForTesting() : false;
}

bool Canvas2DResourceProviderSharedImage::IsSingleBuffered() const {
  return image_pool_ && image_pool_->GetImageInfo().usage.Has(
                            gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE);
}

bool Canvas2DResourceProviderSharedImage::HasUnusedResourcesForTesting() const {
  return image_pool_ && image_pool_->GetPoolSizeForTesting() > 0;
}

gpu::raster::RasterInterface*
Canvas2DResourceProviderSharedImage::RasterInterface() const {
  if (!ContextProviderWrapper()) {
    return nullptr;
  }
  return ContextProviderWrapper()->ContextProvider().RasterInterface();
}

bool Canvas2DResourceProviderSharedImage::IsGpuContextLost() const {
  auto* raster_interface = RasterInterface();
  return !raster_interface ||
         raster_interface->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
}

sk_sp<SkSurface> Canvas2DResourceProviderSharedImage::CreateSkSurface() const {
  TRACE_EVENT0("blink", "Canvas2DResourceProviderSharedImage::CreateSkSurface");

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

CanvasNon2DResourceProviderSharedImage::CanvasNon2DResourceProviderSharedImage(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    bool is_accelerated,
    gpu::SharedImageUsageSet shared_image_usage_flags,
    CanvasResourceProvider::Delegate* delegate)
    : size_(size),
      format_(format),
      alpha_type_(alpha_type),
      color_space_(color_space),
      hdr_metadata_(hdr_metadata),
      delegate_(delegate),
      is_accelerated_(is_accelerated),
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
                    CanvasResourceProvider::kUnusedResourceExpirationTime)
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
  GetFlushForImageListener()->AddObserver(this);

  if (resource_) {
    EnsureWriteAccess();
  }
}

CanvasNon2DResourceProviderSharedImage::CanvasNon2DResourceProviderSharedImage(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider,
    CanvasResourceProvider::Delegate* delegate)
    : size_(size),
      format_(format),
      alpha_type_(alpha_type),
      color_space_(color_space),
      hdr_metadata_(hdr_metadata),
      delegate_(delegate),
      is_accelerated_(false),
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

CanvasNon2DResourceProviderSharedImage::
    ~CanvasNon2DResourceProviderSharedImage() {
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
    GetFlushForImageListener()->RemoveObserver(this);
  }

  // Last chance for outstanding GPU timers to record metrics.
  if (RasterInterface()) {
    CheckGpuTimers(RasterInterface());
  }

  UMA_HISTOGRAM_EXACT_LINEAR("Blink.Canvas.MaximumInflightResources",
                             max_inflight_resources_, 20);
}

void CanvasNon2DResourceProviderSharedImage::ClearUnusedResources() {
  if (image_pool_) {
    image_pool_->Clear();
  }
}

bool CanvasNon2DResourceProviderSharedImage::IsSingleBuffered() const {
  return image_pool_ && image_pool_->GetImageInfo().usage.Has(
                            gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE);
}

void CanvasNon2DResourceProviderSharedImage::OnResourceRefReturned(
    scoped_refptr<CanvasResourceSharedImage>&& resource) {
  if (!resource->IsLost() && resource->HasOneRef() && image_pool_) {
    image_pool_->ReleaseImage(std::move(resource));
  }
}

base::ByteSize CanvasNon2DResourceProviderSharedImage::EstimatedSizeInBytes()
    const {
  base::ByteSize result;
  if (resource_) {
    result += resource_->EstimatedSizeInBytes() * num_inflight_resources_;
  }
  return result;
}

void CanvasNon2DResourceProviderSharedImage::OnMemoryDump(
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

size_t CanvasNon2DResourceProviderSharedImage::GetSize() const {
  if (!surface_) {
    return 0;
  }
  SkImageInfo info = surface_->imageInfo();
  return info.computeByteSize(info.minRowBytes());
}

SkSurface* CanvasNon2DResourceProviderSharedImage::GetSkSurface() const {
  if (!surface_) {
    surface_ = CreateSkSurface();
  }
  return surface_.get();
}

void CanvasNon2DResourceProviderSharedImage::RecordingCleared() {}

void CanvasNon2DResourceProviderSharedImage::InitializeForRecording(
    cc::PaintCanvas* canvas) const {
  if (delegate_) {
    delegate_->InitializeForRecording(canvas);
  }
}

SkSurfaceProps CanvasNon2DResourceProviderSharedImage::GetSkSurfaceProps()
    const {
  const bool can_use_lcd_text = GetAlphaType() == kOpaque_SkAlphaType;
  return skia::LegacyDisplayGlobals::ComputeSurfaceProps(can_use_lcd_text);
}

gpu::raster::RasterInterface*
CanvasNon2DResourceProviderSharedImage::RasterInterface() const {
  if (!ContextProviderWrapper()) {
    return nullptr;
  }
  return ContextProviderWrapper()->ContextProvider().RasterInterface();
}

bool CanvasNon2DResourceProviderSharedImage::IsGpuContextLost() const {
  auto* raster_interface = RasterInterface();
  return !raster_interface ||
         raster_interface->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
}

sk_sp<SkSurface> CanvasNon2DResourceProviderSharedImage::CreateSkSurface()
    const {
  TRACE_EVENT0("blink",
               "CanvasNon2DResourceProviderSharedImage::CreateSkSurface");

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



void CanvasResourceProvider::ClearAtCreation() {
  // Clear the background transparent or opaque, as required. This should only
  // be called when a new resource provider is created to ensure that we're
  // not leaking data or displaying bad pixels (in the case of kOpaque
  // canvases). Instead of adding these commands to our deferred queue, we'll
  // send them directly through to Skia so that they're not replayed for
  // printing operations. See crbug.com/1003114
  DCHECK(IsValid());
  MemoryManagedPaintRecorder recorder(Size(), this);
  if (GetAlphaType() == kOpaque_SkAlphaType) {
    recorder.getRecordingCanvas().clear(SkColors::kBlack);
  } else {
    recorder.getRecordingCanvas().clear(SkColors::kTransparent);
  }

  RasterRecord(recorder.ReleaseMainRecording());
}

void CanvasResourceProvider::RestoreBackBuffer(const cc::PaintImage& image) {
  DCHECK_EQ(image.height(), Size().height());
  DCHECK_EQ(image.width(), Size().width());

  auto sk_image = image.GetSwSkImage();
  DCHECK(sk_image);
  SkPixmap map;
  // We know this SkImage is software backed because it's guaranteed by
  // PaintImage::GetSwSkImage above
  sk_image->peekPixels(&map);
  WritePixels(map.info(), map.addr(), map.rowBytes(), /*x=*/0,
              /*y=*/0);
}


std::unique_ptr<CanvasResourceProvider>
Canvas2DResourceProviderBitmap::CreateForTesting(
    gfx::Size size,
    const Canvas2DColorParams& color_params) {
  return Canvas2DResourceProviderBitmap::CreateWithClear(
      size, color_params.GetSharedImageFormat(), color_params.GetAlphaType(),
      color_params.GetGfxColorSpace(), color_params.GetGfxHdrMetadata());
}

}  // namespace blink
