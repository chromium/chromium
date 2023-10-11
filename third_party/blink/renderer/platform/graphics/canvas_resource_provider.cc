// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"

#include "base/debug/dump_without_crashing.h"
#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "cc/paint/decode_stashing_image_provider.h"
#include "cc/paint/display_item_list.h"
#include "cc/tiles/software_image_decode_cache.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_feature_info.h"
#include "skia/buildflags.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/instrumentation/canvas_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GpuTypes.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"

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
  void AddObserver(CanvasResourceProvider* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(CanvasResourceProvider* observer) {
    observers_.RemoveObserver(observer);
  }

  void NotifyFlushForImage(cc::PaintImage::ContentId content_id) {
    for (CanvasResourceProvider& obs : observers_)
      obs.OnFlushForImage(content_id);
  }

 private:
  friend class WTF::ThreadSpecific<FlushForImageListener>;
  base::ObserverList<CanvasResourceProvider> observers_;
};

static FlushForImageListener* GetFlushForImageListener() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<FlushForImageListener>,
                                  flush_for_image_listener, ());
  return flush_for_image_listener;
}

namespace {

bool IsGMBAllowed(const SkImageInfo& info, const gpu::Capabilities& caps) {
  const gfx::Size size(info.width(), info.height());
  const gfx::BufferFormat buffer_format =
      viz::SinglePlaneSharedImageFormatToBufferFormat(
          viz::SkColorTypeToSinglePlaneSharedImageFormat(info.colorType()));
  return gpu::IsImageSizeValidForGpuMemoryBufferFormat(size, buffer_format) &&
         gpu::IsImageFromGpuMemoryBufferFormatSupported(buffer_format, caps);
}

}  // namespace

class CanvasResourceProvider::CanvasImageProvider : public cc::ImageProvider {
 public:
  CanvasImageProvider(cc::ImageDecodeCache* cache_n32,
                      cc::ImageDecodeCache* cache_f16,
                      const gfx::ColorSpace& target_color_space,
                      SkColorType target_color_type,
                      cc::PlaybackImageProvider::RasterMode raster_mode);
  CanvasImageProvider(const CanvasImageProvider&) = delete;
  CanvasImageProvider& operator=(const CanvasImageProvider&) = delete;
  ~CanvasImageProvider() override = default;

  // cc::ImageProvider implementation.
  cc::ImageProvider::ScopedResult GetRasterContent(
      const cc::DrawImage&) override;

  void ReleaseLockedImages() { locked_images_.clear(); }

 private:
  void CanUnlockImage(ScopedResult);
  void CleanupLockedImages();
  bool IsHardwareDecodeCache() const;

  cc::PlaybackImageProvider::RasterMode raster_mode_;
  bool cleanup_task_pending_ = false;
  Vector<ScopedResult> locked_images_;
  absl::optional<cc::PlaybackImageProvider> playback_image_provider_n32_;
  absl::optional<cc::PlaybackImageProvider> playback_image_provider_f16_;

  base::WeakPtrFactory<CanvasImageProvider> weak_factory_{this};
};

// * Renders to a Skia RAM-backed bitmap.
// * Mailboxing is not supported : cannot be directly composited.
class CanvasResourceProviderBitmap : public CanvasResourceProvider {
 public:
  CanvasResourceProviderBitmap(
      const SkImageInfo& info,
      cc::PaintFlags::FilterQuality filter_quality,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher,
      CanvasResourceHost* resource_host)
      : CanvasResourceProvider(kBitmap,
                               info,
                               filter_quality,
                               /*is_origin_top_left=*/true,
                               /*context_provider_wrapper=*/nullptr,
                               std::move(resource_dispatcher),
                               resource_host) {}

  ~CanvasResourceProviderBitmap() override = default;

  bool IsValid() const final { return GetSkSurface(); }
  bool IsAccelerated() const final { return false; }
  bool SupportsDirectCompositing() const override { return false; }

 private:
  scoped_refptr<CanvasResource> ProduceCanvasResource(FlushReason) override {
    return nullptr;  // Does not support direct compositing
  }

  scoped_refptr<StaticBitmapImage> Snapshot(
      FlushReason reason,
      ImageOrientation orientation) override {
    TRACE_EVENT0("blink", "CanvasResourceProviderBitmap::Snapshot");
    return SnapshotInternal(orientation, reason);
  }

  sk_sp<SkSurface> CreateSkSurface() const override {
    TRACE_EVENT0("blink", "CanvasResourceProviderBitmap::CreateSkSurface");

    const auto info = GetSkImageInfo().makeAlphaType(kPremul_SkAlphaType);
    const auto props = GetSkSurfaceProps();
    return SkSurfaces::Raster(info, &props);
  }
};

// * Renders to a shared memory bitmap.
// * Uses SharedBitmaps to pass frames directly to the compositor.
class CanvasResourceProviderSharedBitmap : public CanvasResourceProviderBitmap {
 public:
  CanvasResourceProviderSharedBitmap(
      const SkImageInfo& info,
      cc::PaintFlags::FilterQuality filter_quality,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher,
      CanvasResourceHost* resource_host)
      : CanvasResourceProviderBitmap(info,
                                     filter_quality,
                                     std::move(resource_dispatcher),
                                     resource_host) {
    DCHECK(ResourceDispatcher());
    type_ = kSharedBitmap;
  }
  ~CanvasResourceProviderSharedBitmap() override = default;
  bool SupportsDirectCompositing() const override { return true; }

 private:
  scoped_refptr<CanvasResource> CreateResource() final {
    SkImageInfo info = GetSkImageInfo();
    if (!viz::SkColorTypeToSinglePlaneSharedImageFormat(info.colorType())
             .IsBitmapFormatSupported()) {
      // If the rendering format is not supported, downgrade to 8-bits.
      // TODO(junov): Should we try 12-12-12-12 and 10-10-10-2?
      info = info.makeColorType(kN32_SkColorType);
    }

    return CanvasResourceSharedBitmap::Create(info, CreateWeakPtr(),
                                              FilterQuality());
  }

  scoped_refptr<CanvasResource> ProduceCanvasResource(
      FlushReason reason) final {
    DCHECK(GetSkSurface());
    scoped_refptr<CanvasResource> output_resource = NewOrRecycledResource();
    if (!output_resource)
      return nullptr;

    auto paint_image = MakeImageSnapshot(reason);
    if (!paint_image)
      return nullptr;
    DCHECK(!paint_image.IsTextureBacked());

    output_resource->TakeSkImage(paint_image.GetSwSkImage());

    return output_resource;
  }
};

// * Renders to a SharedImage, which manages memory internally.
// * Layers are overlay candidates.
class CanvasResourceProviderSharedImage : public CanvasResourceProvider {
 public:
  CanvasResourceProviderSharedImage(
      const SkImageInfo& info,
      cc::PaintFlags::FilterQuality filter_quality,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper,
      bool is_origin_top_left,
      bool is_accelerated,
      uint32_t shared_image_usage_flags,
      CanvasResourceHost* resource_host)
      : CanvasResourceProvider(kSharedImage,
                               info,
                               filter_quality,
                               is_origin_top_left,
                               std::move(context_provider_wrapper),
                               /*resource_dispatcher=*/nullptr,
                               resource_host),
        is_accelerated_(is_accelerated),
        shared_image_usage_flags_(shared_image_usage_flags),
        use_oop_rasterization_(is_accelerated && ContextProviderWrapper()
                                                     ->ContextProvider()
                                                     ->GetCapabilities()
                                                     .supports_oop_raster) {
    resource_ = NewOrRecycledResource();
    GetFlushForImageListener()->AddObserver(this);

    if (resource_)
      EnsureWriteAccess();
  }

  ~CanvasResourceProviderSharedImage() override {
    GetFlushForImageListener()->RemoveObserver(this);
    // Issue any skia work using this resource before destroying any buffer
    // that may have a reference in skia.
    if (is_accelerated_ && !use_oop_rasterization_)
      FlushGrContext();
  }

  bool IsAccelerated() const final { return is_accelerated_; }
  bool SupportsDirectCompositing() const override { return true; }
  bool IsValid() const final {
    if (!use_oop_rasterization_)
      return GetSkSurface() && !IsGpuContextLost();
    else
      return !IsGpuContextLost();
  }

  bool SupportsSingleBuffering() const override {
    return shared_image_usage_flags_ &
           gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE;
  }
  gpu::Mailbox GetBackingMailboxForOverwrite(
      MailboxSyncMode sync_mode) override {
    DCHECK(is_accelerated_);

    if (IsGpuContextLost())
      return gpu::Mailbox();

    WillDrawInternal(false);
    return resource_->GetOrCreateGpuMailbox(sync_mode);
  }

  GLenum GetBackingTextureTarget() const override {
    return resource()->TextureTarget();
  }

  uint32_t GetSharedImageUsageFlags() const override {
    return shared_image_usage_flags_;
  }

  bool WritePixels(const SkImageInfo& orig_info,
                   const void* pixels,
                   size_t row_bytes,
                   int x,
                   int y) override {
    if (!use_oop_rasterization_) {
      return CanvasResourceProvider::WritePixels(orig_info, pixels, row_bytes,
                                                 x, y);
    }

    TRACE_EVENT0("blink", "CanvasResourceProviderSharedImage::WritePixels");
    if (IsGpuContextLost())
      return false;

    WillDrawInternal(true);
    RasterInterface()->WritePixels(
        GetBackingMailboxForOverwrite(kOrderingBarrier), x, y,
        /*dst_plane_index=*/0, GetBackingTextureTarget(),
        SkPixmap(orig_info, pixels, row_bytes));

    // If the overdraw optimization kicked in, we need to indicate that the
    // pixels do not need to be cleared, otherwise the subsequent
    // rasterizations will clobber canvas contents.
    if (x <= 0 && y <= 0 && orig_info.width() >= Size().width() &&
        orig_info.height() >= Size().height())
      is_cleared_ = true;

    return true;
  }

  scoped_refptr<CanvasResource> CreateResource() final {
    TRACE_EVENT0("blink", "CanvasResourceProviderSharedImage::CreateResource");
    if (IsGpuContextLost())
      return nullptr;

    return CanvasResourceRasterSharedImage::Create(
        GetSkImageInfo(), ContextProviderWrapper(), CreateWeakPtr(),
        FilterQuality(), IsOriginTopLeft(), is_accelerated_,
        shared_image_usage_flags_);
  }

  bool UseOopRasterization() final { return use_oop_rasterization_; }

  void NotifyTexParamsModified(const CanvasResource* resource) override {
    if (!is_accelerated_ || use_oop_rasterization_)
      return;

    if (resource_.get() == resource) {
      DCHECK(!current_resource_has_write_access_);
      // Note that the call below is guarenteed to not issue any GPU work for
      // the backend texture since we ensure that all skia work on the resource
      // is issued before releasing write access.
      auto tex = SkSurfaces::GetBackendTexture(
          surface_.get(), SkSurfaces::BackendHandleAccess::kFlushRead);
      GrBackendTextures::GLTextureParametersModified(&tex);
    }
  }

 protected:
  scoped_refptr<CanvasResource> ProduceCanvasResource(
      FlushReason reason) override {
    TRACE_EVENT0("blink",
                 "CanvasResourceProviderSharedImage::ProduceCanvasResource");
    if (IsGpuContextLost())
      return nullptr;

    FlushCanvas(reason);
    // Its important to end read access and ref the resource before the WillDraw
    // call below. Since it relies on resource ref-count to trigger
    // copy-on-write and asserts that we only have write access when the
    // provider has the only ref to the resource, to ensure there are no other
    // readers.
    EndWriteAccess();
    if (!resource_) {
      return nullptr;
    }
    scoped_refptr<CanvasResource> resource = resource_;
    resource->SetFilterQuality(FilterQuality());
    if (ContextProviderWrapper()
            ->ContextProvider()
            ->GetCapabilities()
            .disable_2d_canvas_copy_on_write) {
      // A readback operation may alter the texture parameters, which may affect
      // the compositor's behavior. Therefore, we must trigger copy-on-write
      // even though we are not technically writing to the texture, only to its
      // parameters. This issue is Android-WebView specific: crbug.com/585250.
      WillDraw();
    }

    return resource;
  }

  scoped_refptr<StaticBitmapImage> Snapshot(
      FlushReason reason,
      ImageOrientation orientation) override {
    TRACE_EVENT0("blink", "CanvasResourceProviderSharedImage::Snapshot");
    if (!IsValid())
      return nullptr;

    // We don't need to EndWriteAccess here since that's required to make the
    // rendering results visible on the GpuMemoryBuffer while we return cpu
    // memory, rendererd to by skia, here.
    if (!is_accelerated_)
      return SnapshotInternal(orientation, reason);

    if (!cached_snapshot_) {
      FlushCanvas(reason);
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

  void WillDrawIfNeeded() final {
    if (cached_snapshot_) {
      WillDraw();
    }
  }

  void WillDrawInternal(bool write_to_local_texture) {
    DCHECK(resource_);

    if (IsGpuContextLost())
      return;

    // Since the resource will be updated, the cached snapshot is no longer
    // valid. Note that it is important to release this reference here to not
    // trigger copy-on-write below from the resource ref in the snapshot.
    // Note that this is valid for single buffered mode also, since while the
    // resource/mailbox remains the same, the snapshot needs an updated sync
    // token for these writes.
    cached_snapshot_.reset();

    // We don't need to do copy-on-write for the resource here since writes to
    // the GMB are deferred until it needs to be dispatched to the display
    // compositor via ProduceCanvasResource.
    if (is_accelerated_ && ShouldReplaceTargetBuffer(cached_content_id_)) {
      cached_content_id_ = PaintImage::kInvalidContentId;
      DCHECK(!current_resource_has_write_access_)
          << "Write access must be released before sharing the resource";

      auto old_resource = std::move(resource_);
      auto* old_resource_shared_image =
          static_cast<CanvasResourceSharedImage*>(old_resource.get());
      resource_ = NewOrRecycledResource();
      DCHECK(resource_);

      auto* raster_interface = RasterInterface();
      if (raster_interface) {
        if (!use_oop_rasterization_)
          TearDownSkSurface();

        if (mode_ == SkSurface::kRetain_ContentChangeMode) {
          auto old_mailbox = old_resource_shared_image->GetOrCreateGpuMailbox(
              kOrderingBarrier);
          auto mailbox = resource()->GetOrCreateGpuMailbox(kOrderingBarrier);

          raster_interface->CopySharedImage(
              old_mailbox, mailbox, GetBackingTextureTarget(), 0, 0, 0, 0,
              Size().width(), Size().height(), false /* unpack_flip_y */,
              false /* unpack_premultiply_alpha */);
        } else if (use_oop_rasterization_) {
          // If we're not copying over the previous contents, we need to ensure
          // that the image is cleared on the next BeginRasterCHROMIUM.
          is_cleared_ = false;
        }

        // In non-OOPR mode we need to update the client side SkSurface with the
        // copied texture. Recreating SkSurface here matches the GPU process
        // behaviour that will happen in OOPR mode.
        if (!use_oop_rasterization_) {
          EnsureWriteAccess();
          GetSkSurface();
        }
      } else {
        EnsureWriteAccess();
        if (surface_) {
          // Take read access to the outgoing resource for the skia copy below.
          if (!old_resource_shared_image->HasReadAccess()) {
            old_resource_shared_image->BeginReadAccess();
          }
          surface_->replaceBackendTexture(CreateGrTextureForResource(),
                                          GetGrSurfaceOrigin(), mode_);
          if (!old_resource_shared_image->HasReadAccess()) {
            old_resource_shared_image->EndReadAccess();
          }
        }
      }
      UMA_HISTOGRAM_BOOLEAN("Blink.Canvas.ContentChangeMode",
                            mode_ == SkSurface::kRetain_ContentChangeMode);
      mode_ = SkSurface::kRetain_ContentChangeMode;
    }

    if (write_to_local_texture)
      EnsureWriteAccess();
    else
      EndWriteAccess();

    if (resource()) {
      resource()->WillDraw();
    }
  }

  void WillDraw() override { WillDrawInternal(true); }

  void RasterRecord(cc::PaintRecord last_recording) override {
    if (!use_oop_rasterization_) {
      CanvasResourceProvider::RasterRecord(std::move(last_recording));
      return;
    }
    WillDrawInternal(true);
    const bool needs_clear = !is_cleared_;
    is_cleared_ = true;
    RasterRecordOOP(std::move(last_recording), needs_clear,
                    resource()->GetOrCreateGpuMailbox(kUnverifiedSyncToken));
  }

  bool ShouldReplaceTargetBuffer(
      PaintImage::ContentId content_id = PaintImage::kInvalidContentId) {
    // If the canvas is single buffered, concurrent read/writes to the resource
    // are allowed. Note that we ignore the resource lost case as well since
    // that only indicates that we did not get a sync token for read/write
    // synchronization which is not a requirement for single buffered canvas.
    if (IsSingleBuffered())
      return false;

    // If the resource was lost, we can not use it for writes again.
    if (resource()->IsLost())
      return true;

    // We have the only ref to the resource which implies there are no active
    // readers.
    if (resource_->HasOneRef())
      return false;

    // Its possible to have deferred work in skia which uses this resource. Try
    // flushing once to see if that releases the read refs. We can avoid a copy
    // by queuing this work before writing to this resource.
    if (is_accelerated_) {
      // Another context may have a read reference to this resource. Flush the
      // deferred queue in that context so that we don't need to copy.
      GetFlushForImageListener()->NotifyFlushForImage(content_id);

      if (!use_oop_rasterization_) {
        skgpu::ganesh::FlushAndSubmit(surface_);
      }
    }

    return !resource_->HasOneRef();
  }

  sk_sp<SkSurface> CreateSkSurface() const override {
    TRACE_EVENT0("blink", "CanvasResourceProviderSharedImage::CreateSkSurface");
    if (IsGpuContextLost() || !resource_)
      return nullptr;

    const auto props = GetSkSurfaceProps();
    if (is_accelerated_) {
      return SkSurfaces::WrapBackendTexture(
          GetGrContext(), CreateGrTextureForResource(), GetGrSurfaceOrigin(),
          0 /* msaa_sample_count */, GetSkImageInfo().colorType(),
          GetSkImageInfo().refColorSpace(), &props);
    }

    // For software raster path, we render into cpu memory managed internally
    // by SkSurface and copy the rendered results to the GMB before dispatching
    // it to the display compositor.
    return SkSurfaces::Raster(resource_->CreateSkImageInfo(), &props);
  }

  GrBackendTexture CreateGrTextureForResource() const {
    DCHECK(is_accelerated_);

    return resource()->CreateGrTexture();
  }

  void FlushGrContext() {
    DCHECK(is_accelerated_);

    // The resource may have been imported and used in skia. Make sure any
    // operations using this resource are flushed to the underlying context.
    // Note that its not sufficient to flush the SkSurface here since it will
    // only perform a GrContext flush if that SkSurface has any pending ops. And
    // this resource may be written to or read from skia without using the
    // SkSurface here.
    if (IsGpuContextLost())
      return;
    GetGrContext()->flushAndSubmit();
  }

  void EnsureWriteAccess() {
    DCHECK(resource_);
    // In software mode, we don't need write access to the resource during
    // drawing since it is executed on cpu memory managed by skia. We ensure
    // exclusive access to the resource when the results are copied onto the
    // GMB in EndWriteAccess.
    DCHECK(resource_->HasOneRef() || IsSingleBuffered() || !is_accelerated_)
        << "Write access requires exclusive access to the resource";
    DCHECK(!resource()->is_cross_thread())
        << "Write access is only allowed on the owning thread";

    if (current_resource_has_write_access_ || IsGpuContextLost())
      return;

    if (is_accelerated_ && !use_oop_rasterization_) {
      resource()->BeginWriteAccess();
    }

    // For the non-accelerated path, we don't need a texture for writes since
    // its on the CPU, but we set this bit to know whether the GMB needs to be
    // updated.
    current_resource_has_write_access_ = true;
  }

  void EndWriteAccess() {
    DCHECK(!resource()->is_cross_thread());

    if (!current_resource_has_write_access_ || IsGpuContextLost())
      return;

    if (is_accelerated_) {
      // We reset |mode_| here since the draw commands which overwrite the
      // complete canvas must have been flushed at this point without triggering
      // copy-on-write.
      mode_ = SkSurface::kRetain_ContentChangeMode;

      if (!use_oop_rasterization_) {
        // Issue any skia work using this resource before releasing write
        // access.
        FlushGrContext();
        resource()->EndWriteAccess();
      }
    } else {
      // Currently we never use OOP raster when the resource is not accelerated
      // so we check that assumption here.
      DCHECK(!use_oop_rasterization_);
      if (ShouldReplaceTargetBuffer())
        resource_ = NewOrRecycledResource();
      if (!resource() || !GetSkSurface()) {
        return;
      }
      resource()->CopyRenderingResultsToGpuMemoryBuffer(
          GetSkSurface()->makeImageSnapshot());
    }

    current_resource_has_write_access_ = false;
  }

  CanvasResourceSharedImage* resource() {
    return static_cast<CanvasResourceSharedImage*>(resource_.get());
  }
  const CanvasResourceSharedImage* resource() const {
    return static_cast<const CanvasResourceSharedImage*>(resource_.get());
  }

  // For WebGpu RecyclableCanvasResource.
  void OnAcquireRecyclableCanvasResource() override { EnsureWriteAccess(); }
  void OnDestroyRecyclableCanvasResource(
      const gpu::SyncToken& sync_token) override {
    // RecyclableCanvasResource should be the only one that holds onto
    // |resource_|.
    DCHECK(resource_->HasOneRef());
    resource_->WaitSyncToken(sync_token);
  }

  void OnFlushForImage(cc::PaintImage::ContentId content_id) override {
    CanvasResourceProvider::OnFlushForImage(content_id);
    if (cached_snapshot_ &&
        cached_snapshot_->PaintImageForCurrentFrame().GetContentIdForFrame(0) ==
            content_id) {
      // This handles the case where the cached snapshot is referenced by an
      // ImageBitmap that is being transferred to a worker.
      cached_snapshot_.reset();
    }
  }

 private:
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd) override {
    CanvasResourceProvider::OnMemoryDump(pmd);
    resource()->OnMemoryDump(pmd, GetSkImageInfo().bytesPerPixel());
  }

  const bool is_accelerated_;
  const uint32_t shared_image_usage_flags_;
  bool current_resource_has_write_access_ = false;
  const bool use_oop_rasterization_;
  bool is_cleared_ = false;
  scoped_refptr<CanvasResource> resource_;
  scoped_refptr<StaticBitmapImage> cached_snapshot_;
  PaintImage::ContentId cached_content_id_ = PaintImage::kInvalidContentId;
};

// This class does nothing except answering to ProduceCanvasResource() by piping
// it to NewOrRecycledResource().  This ResourceProvider is meant to be used
// with an imported external CanvasResource, and all drawing and lifetime logic
// must be kept at a higher level.
class CanvasResourceProviderPassThrough final : public CanvasResourceProvider {
 public:
  CanvasResourceProviderPassThrough(
      const SkImageInfo& info,
      cc::PaintFlags::FilterQuality filter_quality,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher,
      bool is_origin_top_left,
      CanvasResourceHost* resource_host)
      : CanvasResourceProvider(kPassThrough,
                               info,
                               filter_quality,
                               is_origin_top_left,
                               std::move(context_provider_wrapper),
                               std::move(resource_dispatcher),
                               resource_host) {}

  ~CanvasResourceProviderPassThrough() override = default;
  bool IsValid() const final { return true; }
  bool IsAccelerated() const final { return true; }
  bool SupportsDirectCompositing() const override { return true; }
  bool SupportsSingleBuffering() const override { return true; }

 private:
  scoped_refptr<CanvasResource> CreateResource() final {
    // This class has no CanvasResource to provide: this must be imported via
    // ImportResource() and kept in the parent class.
    NOTREACHED();
    return nullptr;
  }

  scoped_refptr<CanvasResource> ProduceCanvasResource(FlushReason) final {
    return NewOrRecycledResource();
  }

  sk_sp<SkSurface> CreateSkSurface() const override {
    NOTREACHED();
    return nullptr;
  }

  scoped_refptr<StaticBitmapImage> Snapshot(FlushReason,
                                            ImageOrientation) override {
    auto resource = GetImportedResource();
    if (IsGpuContextLost() || !resource)
      return nullptr;
    return resource->Bitmap();
  }
};

// * Renders to back buffer of a shared image swap chain.
// * Presents swap chain and exports front buffer mailbox to compositor to
//   support low latency mode.
// * Layers are overlay candidates.
class CanvasResourceProviderSwapChain final : public CanvasResourceProvider {
 public:
  CanvasResourceProviderSwapChain(
      const SkImageInfo& info,
      cc::PaintFlags::FilterQuality filter_quality,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher,
      CanvasResourceHost* resource_host)
      : CanvasResourceProvider(kSwapChain,
                               info,
                               filter_quality,
                               /*is_origin_top_left=*/true,
                               std::move(context_provider_wrapper),
                               std::move(resource_dispatcher),
                               resource_host),
        use_oop_rasterization_(ContextProviderWrapper()
                                   ->ContextProvider()
                                   ->GetCapabilities()
                                   .supports_oop_raster) {
    resource_ = CanvasResourceSwapChain::Create(
        GetSkImageInfo(), ContextProviderWrapper(), CreateWeakPtr(),
        FilterQuality());
    // CanvasResourceProviderSwapChain can only operate in a single buffered
    // mode so enable it as soon as possible.
    TryEnableSingleBuffering();
    DCHECK(IsSingleBuffered());
  }
  ~CanvasResourceProviderSwapChain() override = default;

  bool IsValid() const final {
    if (!use_oop_rasterization_)
      return GetSkSurface() && !IsGpuContextLost();
    else
      return !IsGpuContextLost();
  }

  bool IsAccelerated() const final { return true; }
  bool SupportsDirectCompositing() const override { return true; }
  bool SupportsSingleBuffering() const override { return true; }

 private:
  void WillDraw() override {
    needs_present_ = true;
    needs_flush_ = true;
  }

  scoped_refptr<CanvasResource> CreateResource() final {
    TRACE_EVENT0("blink", "CanvasResourceProviderSwapChain::CreateResource");
    return resource_;
  }

  scoped_refptr<CanvasResource> ProduceCanvasResource(
      FlushReason reason) override {
    DCHECK(IsSingleBuffered());
    TRACE_EVENT0("blink",
                 "CanvasResourceProviderSwapChain::ProduceCanvasResource");
    if (!IsValid())
      return nullptr;

    FlushIfNeeded(reason);

    if (needs_present_) {
      resource_->PresentSwapChain();
      needs_present_ = false;
    }
    return resource_;
  }

  scoped_refptr<StaticBitmapImage> Snapshot(FlushReason reason,
                                            ImageOrientation) override {
    TRACE_EVENT0("blink", "CanvasResourceProviderSwapChain::Snapshot");

    if (!IsValid())
      return nullptr;

    FlushIfNeeded(reason);

    return resource_->Bitmap();
  }

  sk_sp<SkSurface> CreateSkSurface() const override {
    TRACE_EVENT0("blink", "CanvasResourceProviderSwapChain::CreateSkSurface");
    if (IsGpuContextLost() || !resource_)
      return nullptr;

    GrGLTextureInfo texture_info = {};
    texture_info.fID = resource_->GetBackBufferTextureId();
    texture_info.fTarget = resource_->TextureTarget();
    texture_info.fFormat =
        ContextProviderWrapper()->ContextProvider()->GetGrGLTextureFormat(
            viz::SkColorTypeToSinglePlaneSharedImageFormat(
                GetSkImageInfo().colorType()));

    auto backend_texture = GrBackendTextures::MakeGL(
        Size().width(), Size().height(), skgpu::Mipmapped::kNo, texture_info);

    const auto props = GetSkSurfaceProps();
    return SkSurfaces::WrapBackendTexture(
        GetGrContext(), backend_texture, kTopLeft_GrSurfaceOrigin,
        0 /* msaa_sample_count */, GetSkImageInfo().colorType(),
        GetSkImageInfo().refColorSpace(), &props);
  }

  void RasterRecord(cc::PaintRecord last_recording) override {
    TRACE_EVENT0("blink", "CanvasResourceProviderSwapChain::RasterRecord");
    if (!use_oop_rasterization_) {
      CanvasResourceProvider::RasterRecord(std::move(last_recording));
      return;
    }
    WillDraw();
    RasterRecordOOP(last_recording, initial_needs_clear_,
                    resource_->GetBackBufferMailbox());
    initial_needs_clear_ = false;
  }

  bool UseOopRasterization() final { return use_oop_rasterization_; }

  bool WritePixels(const SkImageInfo& orig_info,
                   const void* pixels,
                   size_t row_bytes,
                   int x,
                   int y) override {
    if (!use_oop_rasterization_) {
      return CanvasResourceProvider::WritePixels(orig_info, pixels, row_bytes,
                                                 x, y);
    }

    TRACE_EVENT0("blink", "CanvasResourceProviderSwapChain::WritePixels");
    if (IsGpuContextLost())
      return false;

    WillDraw();
    RasterInterface()->WritePixels(
        resource_->GetBackBufferMailbox(), x, y, /*dst_plane_index=*/0,
        GetBackingTextureTarget(), SkPixmap(orig_info, pixels, row_bytes));
    return true;
  }

  void FlushIfNeeded(FlushReason reason) {
    if (needs_flush_) {
      // This only flushes recorded draw ops.
      FlushCanvas(reason);
      // Call flushAndSubmit() explicitly so that any non-draw-op rendering by
      // Skia is flushed to GL.  This is needed specifically for WritePixels().
      if (!use_oop_rasterization_)
        GetGrContext()->flushAndSubmit();

      needs_flush_ = false;
    }
  }

  bool needs_present_ = false;
  bool needs_flush_ = false;
  const bool use_oop_rasterization_;
  // This only matters for the initial backbuffer mailbox, since the frontbuffer
  // will always have the back texture copied to it prior to any new commands.
  bool initial_needs_clear_ = true;
  scoped_refptr<CanvasResourceSwapChain> resource_;
};

std::unique_ptr<CanvasResourceProvider>
CanvasResourceProvider::CreateBitmapProvider(
    const SkImageInfo& info,
    cc::PaintFlags::FilterQuality filter_quality,
    ShouldInitialize should_initialize,
    CanvasResourceHost* resource_host) {
  auto provider = std::make_unique<CanvasResourceProviderBitmap>(
      info, filter_quality, /*resource_dispatcher=*/nullptr, resource_host);
  if (provider->IsValid()) {
    if (should_initialize ==
        CanvasResourceProvider::ShouldInitialize::kCallClear)
      provider->Clear();
    return provider;
  }
  return nullptr;
}

std::unique_ptr<CanvasResourceProvider>
CanvasResourceProvider::CreateSharedBitmapProvider(
    const SkImageInfo& info,
    cc::PaintFlags::FilterQuality filter_quality,
    ShouldInitialize should_initialize,
    base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher,
    CanvasResourceHost* resource_host) {
  // SharedBitmapProvider has to have a valid resource_dispatecher to be able to
  // be created.
  if (!resource_dispatcher)
    return nullptr;

  auto provider = std::make_unique<CanvasResourceProviderSharedBitmap>(
      info, filter_quality, std::move(resource_dispatcher), resource_host);
  if (provider->IsValid()) {
    if (should_initialize ==
        CanvasResourceProvider::ShouldInitialize::kCallClear)
      provider->Clear();
    return provider;
  }

  return nullptr;
}

std::unique_ptr<CanvasResourceProvider>
CanvasResourceProvider::CreateSharedImageProvider(
    const SkImageInfo& info,
    cc::PaintFlags::FilterQuality filter_quality,
    ShouldInitialize should_initialize,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    RasterMode raster_mode,
    uint32_t shared_image_usage_flags,
    CanvasResourceHost* resource_host) {
  // IsGpuCompositingEnabled can re-create the context if it has been lost, do
  // this up front so that we can fail early and not expose ourselves to
  // use after free bugs (crbug.com/1126424)
  const bool is_gpu_compositing_enabled =
      SharedGpuContext::IsGpuCompositingEnabled();

  // If the context is lost we don't want to re-create it here, the resulting
  // resource provider would be invalid anyway
  if (!context_provider_wrapper ||
      context_provider_wrapper->ContextProvider()->IsContextLost())
    return nullptr;

  const auto& capabilities =
      context_provider_wrapper->ContextProvider()->GetCapabilities();
  if ((info.width() < 1 || info.height() < 1 ||
       info.width() > capabilities.max_texture_size ||
       info.height() > capabilities.max_texture_size)) {
    return nullptr;
  }

  const bool is_accelerated = raster_mode == RasterMode::kGPU;

  SkImageInfo adjusted_info = info;
  // TODO(https://crbug.com/1210946): Pass in info as is for all cases.
  // Overriding the info to use RGBA instead of N32 is needed because code
  // elsewhere assumes RGBA. OTOH the software path seems to be assuming N32
  // somewhere in the later pipeline but for offscreen canvas only.
  if (!(shared_image_usage_flags & gpu::SHARED_IMAGE_USAGE_WEBGPU)) {
    adjusted_info = adjusted_info.makeColorType(
        is_accelerated && info.colorType() != kRGBA_F16_SkColorType
            ? kRGBA_8888_SkColorType
            : info.colorType());
  }

  const bool is_gpu_memory_buffer_image_allowed =
      is_gpu_compositing_enabled && IsGMBAllowed(adjusted_info, capabilities) &&
      SharedGpuContext::GetGpuMemoryBufferManager();

  if (raster_mode == RasterMode::kCPU && !is_gpu_memory_buffer_image_allowed)
    return nullptr;

  // If we cannot use overlay, we have to remove the scanout flag and the
  // concurrent read write flag.
  const auto& shared_image_caps = context_provider_wrapper->ContextProvider()
                                      ->SharedImageInterface()
                                      ->GetCapabilities();
  if (!is_gpu_memory_buffer_image_allowed ||
      (is_accelerated && !shared_image_caps.supports_scanout_shared_images)) {
    shared_image_usage_flags &= ~gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE;
    shared_image_usage_flags &= ~gpu::SHARED_IMAGE_USAGE_SCANOUT;
  }

#if BUILDFLAG(IS_MAC)
  if ((shared_image_usage_flags & gpu::SHARED_IMAGE_USAGE_SCANOUT) &&
      is_accelerated && adjusted_info.colorType() == kRGBA_8888_SkColorType) {
    // GPU-accelerated scannout usage on Mac uses IOSurface.  Must switch from
    // RGBA_8888 to BGRA_8888 in that case.
    adjusted_info = adjusted_info.makeColorType(kBGRA_8888_SkColorType);
  }
#endif

  // Use top left origin for shared image CanvasResourceProviders since those
  // can be used for rendering with Skia, and Skia's Graphite backend doesn't
  // support bottom left origin SkSurfaces.
  constexpr bool kIsOriginTopLeft = true;
  auto provider = std::make_unique<CanvasResourceProviderSharedImage>(
      adjusted_info, filter_quality, context_provider_wrapper, kIsOriginTopLeft,
      is_accelerated, shared_image_usage_flags, resource_host);
  if (provider->IsValid()) {
    if (should_initialize ==
        CanvasResourceProvider::ShouldInitialize::kCallClear)
      provider->Clear();
    return provider;
  }

  return nullptr;
}

std::unique_ptr<CanvasResourceProvider>
CanvasResourceProvider::CreateWebGPUImageProvider(
    const SkImageInfo& info,
    uint32_t shared_image_usage_flags,
    CanvasResourceHost* resource_host) {
  auto context_provider_wrapper = SharedGpuContext::ContextProviderWrapper();
  return CreateSharedImageProvider(
      info, cc::PaintFlags::FilterQuality::kLow,
      CanvasResourceProvider::ShouldInitialize::kNo,
      std::move(context_provider_wrapper), RasterMode::kGPU,
      shared_image_usage_flags | gpu::SHARED_IMAGE_USAGE_WEBGPU, resource_host);
}

std::unique_ptr<CanvasResourceProvider>
CanvasResourceProvider::CreatePassThroughProvider(
    const SkImageInfo& info,
    cc::PaintFlags::FilterQuality filter_quality,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher,
    bool is_origin_top_left,
    CanvasResourceHost* resource_host) {
  // SharedGpuContext::IsGpuCompositingEnabled can potentially replace the
  // context_provider_wrapper, so it's important to call that first as it can
  // invalidate the weak pointer.
  if (!SharedGpuContext::IsGpuCompositingEnabled() || !context_provider_wrapper)
    return nullptr;

  const auto& capabilities =
      context_provider_wrapper->ContextProvider()->GetCapabilities();
  if (info.width() > capabilities.max_texture_size ||
      info.height() > capabilities.max_texture_size) {
    return nullptr;
  }

  const auto& shared_image_capabilities =
      context_provider_wrapper->ContextProvider()
          ->SharedImageInterface()
          ->GetCapabilities();
  // Either swap_chain or gpu memory buffer should be enabled for this be used
  if (!shared_image_capabilities.shared_image_swap_chain &&
      (!IsGMBAllowed(info, capabilities) ||
       !Platform::Current()->GetGpuMemoryBufferManager())) {
    return nullptr;
  }

  auto provider = std::make_unique<CanvasResourceProviderPassThrough>(
      info, filter_quality, context_provider_wrapper, resource_dispatcher,
      is_origin_top_left, resource_host);
  if (provider->IsValid()) {
    // All the other type of resources are doing a clear here. As a
    // CanvasResourceProvider of type PassThrough is used to delegate the
    // internal parts of the resource and provider to other classes, we should
    // not attempt to do a clear here. clear is not needed here.
    return provider;
  }

  return nullptr;
}

std::unique_ptr<CanvasResourceProvider>
CanvasResourceProvider::CreateSwapChainProvider(
    const SkImageInfo& info,
    cc::PaintFlags::FilterQuality filter_quality,
    ShouldInitialize should_initialize,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher,
    CanvasResourceHost* resource_host) {
  // SharedGpuContext::IsGpuCompositingEnabled can potentially replace the
  // context_provider_wrapper, so it's important to call that first as it can
  // invalidate the weak pointer.
  if (!SharedGpuContext::IsGpuCompositingEnabled() || !context_provider_wrapper)
    return nullptr;

  const auto& capabilities =
      context_provider_wrapper->ContextProvider()->GetCapabilities();
  const auto& shared_image_capabilities =
      context_provider_wrapper->ContextProvider()
          ->SharedImageInterface()
          ->GetCapabilities();

  if (info.width() > capabilities.max_texture_size ||
      info.height() > capabilities.max_texture_size ||
      !shared_image_capabilities.shared_image_swap_chain) {
    return nullptr;
  }

  auto provider = std::make_unique<CanvasResourceProviderSwapChain>(
      info, filter_quality, context_provider_wrapper, resource_dispatcher,
      resource_host);
  if (provider->IsValid()) {
    if (should_initialize ==
        CanvasResourceProvider::ShouldInitialize::kCallClear)
      provider->Clear();
    return provider;
  }

  return nullptr;
}

CanvasResourceProvider::CanvasImageProvider::CanvasImageProvider(
    cc::ImageDecodeCache* cache_n32,
    cc::ImageDecodeCache* cache_f16,
    const gfx::ColorSpace& target_color_space,
    SkColorType canvas_color_type,
    cc::PlaybackImageProvider::RasterMode raster_mode)
    : raster_mode_(raster_mode) {
  absl::optional<cc::PlaybackImageProvider::Settings> settings =
      cc::PlaybackImageProvider::Settings();
  settings->raster_mode = raster_mode_;

  cc::TargetColorParams target_color_params;
  target_color_params.color_space = target_color_space;
  playback_image_provider_n32_.emplace(cache_n32, target_color_params,
                                       std::move(settings));
  // If the image provider may require to decode to half float instead of
  // uint8, create a f16 PlaybackImageProvider with the passed cache.
  if (canvas_color_type == kRGBA_F16_SkColorType) {
    DCHECK(cache_f16);
    settings = cc::PlaybackImageProvider::Settings();
    settings->raster_mode = raster_mode_;
    playback_image_provider_f16_.emplace(cache_f16, target_color_params,
                                         std::move(settings));
  }
}

cc::ImageProvider::ScopedResult
CanvasResourceProvider::CanvasImageProvider::GetRasterContent(
    const cc::DrawImage& draw_image) {
  // TODO(xidachen): Ensure this function works for paint worklet generated
  // images.
  // If we like to decode high bit depth image source to half float backed
  // image, we need to sniff the image bit depth here to avoid double decoding.
  ImageProvider::ScopedResult scoped_decoded_image;
  if (playback_image_provider_f16_ &&
      draw_image.paint_image().is_high_bit_depth()) {
    DCHECK(playback_image_provider_f16_);
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

void CanvasResourceProvider::CanvasImageProvider::CanUnlockImage(
    ScopedResult image) {
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

void CanvasResourceProvider::CanvasImageProvider::CleanupLockedImages() {
  cleanup_task_pending_ = false;
  ReleaseLockedImages();
}

bool CanvasResourceProvider::CanvasImageProvider::IsHardwareDecodeCache()
    const {
  return raster_mode_ != cc::PlaybackImageProvider::RasterMode::kSoftware;
}

BASE_FEATURE(kCanvas2DAutoFlushParams,
             "Canvas2DAutoFlushParams",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The following parameters attempt to reach a compromise between not flushing
// too often, and not accumulating an unreasonable backlog. Flushing too
// often will hurt performance due to overhead costs. Accumulating large
// backlogs, in the case of OOPR-Canvas, results in poor parellelism and
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
                                               4 * 1024);

const base::FeatureParam<int> kMaxPinnedImageKB(&kCanvas2DAutoFlushParams,
                                                "max_pinned_image_kb",
                                                64 * 1024);

CanvasResourceProvider::CanvasResourceProvider(
    const ResourceProviderType& type,
    const SkImageInfo& info,
    cc::PaintFlags::FilterQuality filter_quality,
    bool is_origin_top_left,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher,
    CanvasResourceHost* resource_host)
    : type_(type),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      resource_dispatcher_(resource_dispatcher),
      filter_quality_(filter_quality),
      is_origin_top_left_(is_origin_top_left),
      snapshot_paint_image_id_(cc::PaintImage::GetNextId()),
      resource_host_(resource_host) {
  info_ = info;
  max_recorded_op_bytes_ = static_cast<size_t>(kMaxRecordedOpKB.Get()) * 1024;
  max_pinned_image_bytes_ = static_cast<size_t>(kMaxPinnedImageKB.Get()) * 1024;
  if (context_provider_wrapper_) {
    context_provider_wrapper_->AddObserver(this);
    const auto& caps =
        context_provider_wrapper_->ContextProvider()->GetCapabilities();
    oopr_uses_dmsaa_ = !caps.msaa_is_slow && !caps.avoid_stencil_buffers;
  }

  CanvasMemoryDumpProvider::Instance()->RegisterClient(this);
  recorder_.beginRecording(Size());
}

CanvasResourceProvider::~CanvasResourceProvider() {
  UMA_HISTOGRAM_EXACT_LINEAR("Blink.Canvas.MaximumInflightResources",
                             max_inflight_resources_, 20);
  if (context_provider_wrapper_)
    context_provider_wrapper_->RemoveObserver(this);
  CanvasMemoryDumpProvider::Instance()->UnregisterClient(this);

  // Last chance for outstanding GPU timers to record metrics.
  if (RasterInterface()) {
    CheckGpuTimers(RasterInterface());
  }
}

void CanvasResourceProvider::FlushIfRecordingLimitExceeded() {
  // When printing we avoid flushing if it is still possible to print in
  // vector mode.
  if (IsPrinting() && clear_frame_) {
    return;
  }
  if (UNLIKELY(TotalOpBytesUsed() > max_recorded_op_bytes_) ||
      UNLIKELY(TotalImageBytesUsed() > max_pinned_image_bytes_)) {
    FlushCanvas(FlushReason::kRecordingLimitExceeded);
  }
}

SkSurface* CanvasResourceProvider::GetSkSurface() const {
  if (!surface_)
    surface_ = CreateSkSurface();
  return surface_.get();
}

void CanvasResourceProvider::NotifyWillTransfer(
    cc::PaintImage::ContentId content_id) {
  // This is called when an ImageBitmap is about to be transferred. All
  // references to such a bitmap on the current thread must be released, which
  // means that DisplayItemLists that reference it must be flushed.
  GetFlushForImageListener()->NotifyFlushForImage(content_id);
}

void CanvasResourceProvider::EnsureSkiaCanvas() {
  WillDraw();

  if (skia_canvas_)
    return;

  cc::SkiaPaintCanvas::ContextFlushes context_flushes;
  if (IsAccelerated() && ContextProviderWrapper() &&
      !ContextProviderWrapper()
           ->ContextProvider()
           ->GetGpuFeatureInfo()
           .IsWorkaroundEnabled(gpu::DISABLE_2D_CANVAS_AUTO_FLUSH)) {
    context_flushes.enable = true;
    context_flushes.max_draws_before_flush = kMaxDrawsBeforeContextFlush;
  }
  skia_canvas_ = std::make_unique<cc::SkiaPaintCanvas>(
      GetSkSurface()->getCanvas(), GetOrCreateCanvasImageProvider(),
      context_flushes);
}

CanvasResourceProvider::CanvasImageProvider*
CanvasResourceProvider::GetOrCreateCanvasImageProvider() {
  if (!canvas_image_provider_) {
    // Create an ImageDecodeCache for half float images only if the canvas is
    // using half float back storage.
    cc::ImageDecodeCache* cache_f16 = nullptr;
    if (GetSkImageInfo().colorType() == kRGBA_F16_SkColorType)
      cache_f16 = ImageDecodeCacheF16();

    auto raster_mode = cc::PlaybackImageProvider::RasterMode::kSoftware;
    if (UseHardwareDecodeCache()) {
      raster_mode = UseOopRasterization()
                        ? cc::PlaybackImageProvider::RasterMode::kOop
                        : cc::PlaybackImageProvider::RasterMode::kGpu;
    }
    canvas_image_provider_ = std::make_unique<CanvasImageProvider>(
        ImageDecodeCacheRGBA8(), cache_f16, GetColorSpace(), info_.colorType(),
        raster_mode);
  }
  return canvas_image_provider_.get();
}

void CanvasResourceProvider::InitializeForRecording(
    cc::PaintCanvas* canvas) const {
  if (resource_host_) {
    resource_host_->InitializeForRecording(canvas);
  }
}

cc::PaintCanvas* CanvasResourceProvider::Canvas(bool needs_will_draw) {
  // TODO(https://crbug.com/1211912): Video frames don't work without
  // WillDrawIfNeeded(), but we are getting memory leak on CreatePattern
  // with it. There should be a better way to solve this.
  if (needs_will_draw)
    WillDrawIfNeeded();

  return recorder_.getRecordingCanvas();
}

void CanvasResourceProvider::OnContextDestroyed() {
  if (skia_canvas_)
    skia_canvas_->reset_image_provider();
  canvas_image_provider_.reset();
}

void CanvasResourceProvider::OnFlushForImage(PaintImage::ContentId content_id) {
  if (Canvas()) {
    MemoryManagedPaintCanvas* canvas =
        static_cast<MemoryManagedPaintCanvas*>(Canvas());
    if (canvas->IsCachingImage(content_id)) {
      FlushCanvas(FlushReason::kSourceImageWillChange);
    }
  }
}

void CanvasResourceProvider::ReleaseLockedImages() {
  if (canvas_image_provider_)
    canvas_image_provider_->ReleaseLockedImages();
}

scoped_refptr<StaticBitmapImage> CanvasResourceProvider::SnapshotInternal(
    ImageOrientation orientation,
    FlushReason reason) {
  if (!IsValid())
    return nullptr;

  auto paint_image = MakeImageSnapshot(reason);
  DCHECK(!paint_image.IsTextureBacked());
  return UnacceleratedStaticBitmapImage::Create(std::move(paint_image),
                                                orientation);
}

cc::PaintImage CanvasResourceProvider::MakeImageSnapshot(FlushReason reason) {
  FlushCanvas(reason);
  auto sk_image = GetSkSurface()->makeImageSnapshot();
  if (!sk_image)
    return cc::PaintImage();

  auto last_snapshot_sk_image_id = snapshot_sk_image_id_;
  snapshot_sk_image_id_ = sk_image->uniqueID();

  // Ensure that a new PaintImage::ContentId is used only when the underlying
  // SkImage changes. This is necessary to ensure that the same image results
  // in a cache hit in cc's ImageDecodeCache.
  if (snapshot_paint_image_content_id_ == PaintImage::kInvalidContentId ||
      last_snapshot_sk_image_id != snapshot_sk_image_id_) {
    snapshot_paint_image_content_id_ = PaintImage::GetNextContentId();
  }

  return PaintImageBuilder::WithDefault()
      .set_id(snapshot_paint_image_id_)
      .set_image(std::move(sk_image), snapshot_paint_image_content_id_)
      .TakePaintImage();
}

gpu::gles2::GLES2Interface* CanvasResourceProvider::ContextGL() const {
  if (!context_provider_wrapper_)
    return nullptr;
  return context_provider_wrapper_->ContextProvider()->ContextGL();
}

gpu::raster::RasterInterface* CanvasResourceProvider::RasterInterface() const {
  if (!context_provider_wrapper_)
    return nullptr;
  return context_provider_wrapper_->ContextProvider()->RasterInterface();
}

GrDirectContext* CanvasResourceProvider::GetGrContext() const {
  if (!context_provider_wrapper_)
    return nullptr;
  return context_provider_wrapper_->ContextProvider()->GetGrContext();
}

gfx::Size CanvasResourceProvider::Size() const {
  return gfx::Size(info_.width(), info_.height());
}

SkSurfaceProps CanvasResourceProvider::GetSkSurfaceProps() const {
  const bool can_use_lcd_text =
      GetSkImageInfo().alphaType() == kOpaque_SkAlphaType;
  return skia::LegacyDisplayGlobals::ComputeSurfaceProps(can_use_lcd_text);
}

gfx::ColorSpace CanvasResourceProvider::GetColorSpace() const {
  auto* color_space = GetSkImageInfo().colorSpace();
  return color_space ? gfx::ColorSpace(*color_space)
                     : gfx::ColorSpace::CreateSRGB();
}

absl::optional<cc::PaintRecord> CanvasResourceProvider::FlushCanvas(
    FlushReason reason) {
  if (!HasRecordedDrawOps()) {
    return absl::nullopt;
  }
  ScopedRasterTimer timer(IsAccelerated() ? RasterInterface() : nullptr, *this,
                          always_enable_raster_timers_for_testing_);
  DCHECK(reason != FlushReason::kNone);
  bool want_to_print = (IsPrinting() && reason != FlushReason::kClear) ||
                       reason == FlushReason::kPrinting ||
                       reason == FlushReason::kCanvasPushFrameWhilePrinting;
  bool preserve_recording = want_to_print && clear_frame_;

  // If a previous flush rasterized some paint ops, we lost part of the
  // recording and must fallback to raster printing instead of vectorial
  // printing. Record the reason why this happened.
  if (want_to_print && !clear_frame_) {
    printing_fallback_reason_ = last_flush_reason_;
  }
  last_flush_reason_ = reason;
  clear_frame_ = false;
  if (reason == FlushReason::kClear) {
    clear_frame_ = true;
    printing_fallback_reason_ = FlushReason::kNone;
  }
  cc::PaintRecord recording = recorder_.finishRecordingAsPicture();
  RasterRecord(recording);
  last_recording_ =
      preserve_recording ? absl::optional(recording) : absl::nullopt;
  return recording;
}

void CanvasResourceProvider::RasterRecord(cc::PaintRecord last_recording) {
  EnsureSkiaCanvas();
  skia_canvas_->drawPicture(std::move(last_recording));
  skgpu::ganesh::FlushAndSubmit(GetSkSurface());
}

void CanvasResourceProvider::RasterRecordOOP(cc::PaintRecord last_recording,
                                             bool needs_clear,
                                             gpu::Mailbox mailbox) {
  if (IsGpuContextLost())
    return;
  gpu::raster::RasterInterface* ri = RasterInterface();
  SkColor4f background_color =
      GetSkImageInfo().alphaType() == kOpaque_SkAlphaType
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

  const bool can_use_lcd_text =
      GetSkImageInfo().alphaType() == kOpaque_SkAlphaType;
  ri->BeginRasterCHROMIUM(background_color, needs_clear,
                          /*msaa_sample_count=*/oopr_uses_dmsaa_ ? 1 : 0,
                          oopr_uses_dmsaa_ ? gpu::raster::MsaaMode::kDMSAA
                                           : gpu::raster::MsaaMode::kNoMSAA,
                          can_use_lcd_text, /*visible=*/true, GetColorSpace(),
                          /*hdr_headroom=*/1.f, mailbox.name);

  ri->RasterCHROMIUM(list.get(), GetOrCreateCanvasImageProvider(), size,
                     full_raster_rect, playback_rect, post_translate,
                     post_scale, false /* requires_clear */, &max_op_size_hint);

  ri->EndRasterCHROMIUM();
}

bool CanvasResourceProvider::IsGpuContextLost() const {
  auto* raster_interface = RasterInterface();
  return !raster_interface ||
         raster_interface->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
}

bool CanvasResourceProvider::WritePixels(const SkImageInfo& orig_info,
                                         const void* pixels,
                                         size_t row_bytes,
                                         int x,
                                         int y) {
  TRACE_EVENT0("blink", "CanvasResourceProvider::WritePixels");

  DCHECK(IsValid());
  DCHECK(!HasRecordedDrawOps());

  EnsureSkiaCanvas();

  bool wrote_pixels = GetSkSurface()->getCanvas()->writePixels(
      orig_info, pixels, row_bytes, x, y);

  if (wrote_pixels) {
    // WritePixels content is not saved in recording. Calling WritePixels
    // therefore invalidates `last_recording_` because it's now missing that
    // information.
    last_recording_ = absl::nullopt;
  }
  return wrote_pixels;
}

void CanvasResourceProvider::Clear() {
  // Clear the background transparent or opaque, as required. This should only
  // be called when a new resource provider is created to ensure that we're
  // not leaking data or displaying bad pixels (in the case of kOpaque
  // canvases). Instead of adding these commands to our deferred queue, we'll
  // send them directly through to Skia so that they're not replayed for
  // printing operations. See crbug.com/1003114
  DCHECK(IsValid());
  if (info_.alphaType() == kOpaque_SkAlphaType)
    Canvas()->clear(SkColors::kBlack);
  else
    Canvas()->clear(SkColors::kTransparent);

  FlushCanvas(FlushReason::kClear);
}

uint32_t CanvasResourceProvider::ContentUniqueID() const {
  return GetSkSurface()->generationID();
}

scoped_refptr<CanvasResource> CanvasResourceProvider::CreateResource() {
  // Needs to be implemented in subclasses that use resource recycling.
  NOTREACHED();
  return nullptr;
}

cc::ImageDecodeCache* CanvasResourceProvider::ImageDecodeCacheRGBA8() {
  if (UseHardwareDecodeCache()) {
    return context_provider_wrapper_->ContextProvider()->ImageDecodeCache(
        kN32_SkColorType);
  }

  return &Image::SharedCCDecodeCache(kN32_SkColorType);
}

cc::ImageDecodeCache* CanvasResourceProvider::ImageDecodeCacheF16() {
  if (UseHardwareDecodeCache()) {
    return context_provider_wrapper_->ContextProvider()->ImageDecodeCache(
        kRGBA_F16_SkColorType);
  }
  return &Image::SharedCCDecodeCache(kRGBA_F16_SkColorType);
}

void CanvasResourceProvider::RecycleResource(
    scoped_refptr<CanvasResource>&& resource) {
  // We don't want to keep an arbitrary large number of canvases.
  if (canvas_resources_.size() >
      static_cast<unsigned int>(kMaxRecycledCanvasResources))
    return;

  // Need to check HasOneRef() because if there are outstanding references to
  // the resource, it cannot be safely recycled.
  if (resource->HasOneRef() && resource_recycling_enabled_ &&
      !is_single_buffered_) {
    canvas_resources_.push_back(std::move(resource));
  }
}

void CanvasResourceProvider::SetResourceRecyclingEnabled(bool value) {
  resource_recycling_enabled_ = value;
  if (!resource_recycling_enabled_)
    ClearRecycledResources();
}

void CanvasResourceProvider::ClearRecycledResources() {
  canvas_resources_.clear();
}

void CanvasResourceProvider::OnDestroyResource() {
  --num_inflight_resources_;
}

scoped_refptr<CanvasResource> CanvasResourceProvider::NewOrRecycledResource() {
  if (canvas_resources_.empty()) {
    canvas_resources_.push_back(CreateResource());
    ++num_inflight_resources_;
    if (num_inflight_resources_ > max_inflight_resources_)
      max_inflight_resources_ = num_inflight_resources_;
  }

  if (IsSingleBuffered()) {
    DCHECK_EQ(canvas_resources_.size(), 1u);
    return canvas_resources_.back();
  }

  scoped_refptr<CanvasResource> resource = std::move(canvas_resources_.back());
  canvas_resources_.pop_back();
  return resource;
}

void CanvasResourceProvider::TryEnableSingleBuffering() {
  if (IsSingleBuffered() || !SupportsSingleBuffering())
    return;
  is_single_buffered_ = true;
  ClearRecycledResources();
}

bool CanvasResourceProvider::ImportResource(
    scoped_refptr<CanvasResource>&& resource) {
  if (!IsSingleBuffered() || !SupportsSingleBuffering())
    return false;
  canvas_resources_.clear();
  canvas_resources_.push_back(std::move(resource));
  return true;
}

scoped_refptr<CanvasResource> CanvasResourceProvider::GetImportedResource()
    const {
  if (!IsSingleBuffered() || !SupportsSingleBuffering())
    return nullptr;
  DCHECK_LE(canvas_resources_.size(), 1u);
  if (canvas_resources_.empty())
    return nullptr;
  return canvas_resources_.back();
}

void CanvasResourceProvider::SkipQueuedDrawCommands() {
  // Note that this function only gets called when canvas needs a full repaint,
  // so always update the |mode_| to discard the old copy of canvas content.
  mode_ = SkSurface::kDiscard_ContentChangeMode;
  clear_frame_ = true;
  last_flush_reason_ = FlushReason::kNone;
  printing_fallback_reason_ = FlushReason::kNone;
  if (!HasRecordedDrawOps())
    return;
  recorder_.finishRecordingAsPicture();
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
  WritePixels(map.info(), map.addr(), map.rowBytes(), /*x=*/0, /*y=*/0);
}

bool CanvasResourceProvider::HasRecordedDrawOps() const {
  return recorder_.HasRecordedDrawOps();
}

void CanvasResourceProvider::TearDownSkSurface() {
  skia_canvas_ = nullptr;
  surface_ = nullptr;
}

size_t CanvasResourceProvider::ComputeSurfaceSize() const {
  if (!surface_)
    return 0;

  SkImageInfo info = surface_->imageInfo();
  return info.computeByteSize(info.minRowBytes());
}

void CanvasResourceProvider::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd) {
  if (!surface_)
    return;

  for (const auto& resource : canvas_resources_) {
    // Don't report, to avoid double-counting.
    if (resource->HasDetailedMemoryDumpProvider()) {
      return;
    }
  }

  std::string dump_name =
      base::StringPrintf("canvas/ResourceProvider/SkSurface/0x%" PRIXPTR,
                         reinterpret_cast<uintptr_t>(surface_.get()));
  auto* dump = pmd->CreateAllocatorDump(dump_name);

  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  ComputeSurfaceSize());
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameObjectCount,
                  base::trace_event::MemoryAllocatorDump::kUnitsObjects, 1);

  // SkiaMemoryDumpProvider reports only sk_glyph_cache and sk_resource_cache.
  // So the SkSurface is suballocation of malloc, not SkiaDumpProvider.
  if (const char* system_allocator_name =
          base::trace_event::MemoryDumpManager::GetInstance()
              ->system_allocator_pool_name()) {
    pmd->AddSuballocation(dump->guid(), system_allocator_name);
  }
}

size_t CanvasResourceProvider::GetSize() const {
  return ComputeSurfaceSize();
}

}  // namespace blink
