// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_PROVIDER_H_

#include <algorithm>
#include <memory>
#include <optional>

#include "base/byte_size.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cc/raster/playback_image_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_color_params.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/canvas_snapshot_provider.h"
#include "third_party/blink/renderer/platform/graphics/flush_reason.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/scoped_raster_timer.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/instrumentation/canvas_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"

namespace cc {
class PaintCanvas;
class SkiaPaintCanvas;
}  // namespace cc

namespace gpu {

struct SyncToken;

namespace raster {

class RasterInterface;

}  // namespace raster
}  // namespace gpu

namespace blink {

PLATFORM_EXPORT BASE_DECLARE_FEATURE(kCanvas2DAutoFlushParams);
PLATFORM_EXPORT BASE_DECLARE_FEATURE(kCanvas2DReclaimUnusedResources);

class CanvasRenderingContext2D;
class CanvasResource;
class CanvasResourceSharedImage;
class Canvas2DResourceProviderBitmap;
class CanvasResourceProviderSharedImage;
class MemoryManagedPaintCanvas;
class OffscreenCanvasRenderingContext2D;
class StaticBitmapImage;
class WebGraphicsSharedImageInterfaceProvider;

// Specifies whether the provider should rasterize paint commands on the CPU
// or GPU. This is used to support software raster with GPU compositing.
enum class RasterMode {
  kGPU,
  kCPU,
};

// CanvasResourceProvider
//==============================================================================
//
// This is an abstract base class that encapsulates a drawable graphics
// resource.  Subclasses manage specific resource types (Gpu Textures, Bitmap in
// RAM). CanvasResourceProvider serves as an abstraction layer for these
// resource types. It is designed to serve the needs of Canvas2D, but can also
// be used as a general purpose provider of drawable surfaces for 2D rendering
// with skia.
//
// General usage:
//   1) Use the Create() static method to create an instance
//   2) use Canvas() to get a drawing interface
//   3) Call Snapshot() to acquire a bitmap with the rendered image in it.

class PLATFORM_EXPORT CanvasResourceProvider
    : public base::CheckedObserver,
      public CanvasMemoryDumpClient,
      public CanvasSnapshotProvider,
      public MemoryManagedPaintRecorder::Client,
      public ScopedRasterTimer::Host {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void NotifyGpuContextLost() = 0;
    virtual void InitializeForRecording(cc::PaintCanvas* canvas) const = 0;
    virtual bool IsPrinting() const { return false; }
    virtual bool TransferToGPUTextureWasInvoked() { return false; }
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  enum ResourceProviderType {
    kTexture [[deprecated]] = 0,
    kBitmap = 1,
    kSharedBitmap [[deprecated]] = 2,
    kTextureGpuMemoryBuffer [[deprecated]] = 3,
    kBitmapGpuMemoryBuffer [[deprecated]] = 4,
    kSharedImage = 5,
    kDirectGpuMemoryBuffer [[deprecated]] = 6,
    kPassThrough [[deprecated]] = 7,
    kSwapChain [[deprecated]] = 8,
    kSkiaDawnSharedImage [[deprecated]] = 9,
    kExternalBitmap = 10,
    kMaxValue = kExternalBitmap,
  };
#pragma GCC diagnostic pop

  virtual CanvasResourceProviderSharedImage* AsSharedImageProvider() {
    return nullptr;
  }

  // Used to determine if the provider is going to be initialized or not.
  enum class ShouldInitialize { kNo, kCallClear };

  static std::unique_ptr<CanvasResourceProviderSharedImage>
  CreateSharedImageProviderForSoftwareCompositor(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      ShouldInitialize initialize_provider,
      WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider,
      Delegate* delegate = nullptr);

  static std::unique_ptr<CanvasResourceProviderSharedImage>
  CreateSharedImageProvider(gfx::Size size,
                            viz::SharedImageFormat format,
                            SkAlphaType alpha_type,
                            const gfx::ColorSpace& color_space,
                            ShouldInitialize initialize_provider,
                            base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
                            RasterMode raster_mode,
                            gpu::SharedImageUsageSet shared_image_usage_flags,
                            Delegate* delegate = nullptr);

  static std::unique_ptr<CanvasResourceProviderSharedImage>
  CreateWebGPUImageProvider(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      gpu::SharedImageUsageSet shared_image_usage_flags = {},
      Delegate* delegate = nullptr);

  static std::unique_ptr<CanvasResourceProvider>
  CreateSharedImageProviderForSoftwareCompositor(
      gfx::Size size,
      const Canvas2DColorParams& color_params,
      ShouldInitialize initialize_provider,
      WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider,
      Delegate* delegate = nullptr);

  static std::unique_ptr<CanvasResourceProviderSharedImage>
  CreateSharedImageProvider(gfx::Size size,
                            const Canvas2DColorParams& color_params,
                            ShouldInitialize initialize_provider,
                            base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
                            RasterMode raster_mode,
                            gpu::SharedImageUsageSet shared_image_usage_flags,
                            Delegate* delegate = nullptr);

  static std::unique_ptr<CanvasResourceProvider> CreateWebGPUImageProvider(
      gfx::Size size,
      const Canvas2DColorParams& color_params,
      gpu::SharedImageUsageSet shared_image_usage_flags = {},
      Delegate* delegate = nullptr);

  // Use Snapshot() for capturing a frame that is intended to be displayed via
  // the compositor. Cases that are destined to be transferred via a
  // TransferableResource should call ProduceCanvasResource() instead.
  // The ImageOrientationEnum conveys the desired orientation of the image, and
  // should be derived from the source of the bitmap data.
  virtual scoped_refptr<CanvasResource> ProduceCanvasResource(FlushReason) = 0;
  virtual scoped_refptr<StaticBitmapImage> Snapshot(
      ImageOrientation = ImageOrientationEnum::kDefault) = 0;

  void SetDelegate(Delegate* delegate) { delegate_ = delegate; }

  MemoryManagedPaintCanvas& Canvas();
  // FlushCanvas and preserve recording only if IsPrinting or
  // FlushReason indicates printing in progress.
  std::optional<cc::PaintRecord> FlushCanvas(FlushReason = FlushReason::kOther);

  // TODO(crbug.com/371227617): Trim callsites of this method to those that
  // actually need to pass this info to Skia APIs and then eliminate the
  // method/this class holding `info_` by inlining creation of SkImageInfo at
  // those callsites.
  const SkImageInfo& GetSkImageInfo() const { return info_; }
  SkSurfaceProps GetSkSurfaceProps() const;
  viz::SharedImageFormat GetSharedImageFormat() const override {
    return format_;
  }
  gfx::ColorSpace GetColorSpace() const override { return color_space_; }
  SkAlphaType GetAlphaType() const override { return alpha_type_; }
  gfx::Size Size() const override { return size_; }
  virtual base::ByteSize EstimatedSizeInBytes() const {
    return base::ByteSize(format_.EstimatedSizeInBytes(size_));
  }
  // Returns true if the resource can be used by the display compositor.
  virtual bool SupportsDirectCompositing() const = 0;
  uint32_t ContentUniqueID() const;

  // Indicates that the compositing path is single buffered, meaning that
  // ProduceCanvasResource() return a reference to the same resource each time,
  // which implies that Producing an animation frame may overwrite the resource
  // used by the previous frame. This results in graphics updates skipping the
  // queue, thus reducing latency, but with the possible side effects of tearing
  // (in cases where the resource is scanned out directly) and irregular frame
  // rate.
  virtual bool IsSingleBuffered() const = 0;

  bool IsGpuContextLost() const override;

  virtual bool WritePixels(const SkImageInfo& orig_info,
                           const void* pixels,
                           size_t row_bytes,
                           int x,
                           int y) = 0;

  CanvasResourceProvider(const CanvasResourceProvider&) = delete;
  CanvasResourceProvider& operator=(const CanvasResourceProvider&) = delete;
  ~CanvasResourceProvider() override;

  void RestoreBackBuffer(const cc::PaintImage&);

  ResourceProviderType GetType() const { return type_; }

  void FlushIfRecordingLimitExceeded();

  const MemoryManagedPaintRecorder& Recorder() const { return *recorder_; }
  MemoryManagedPaintRecorder& Recorder() { return *recorder_; }
  std::unique_ptr<MemoryManagedPaintRecorder> ReleaseRecorder();
  void SetRecorder(std::unique_ptr<MemoryManagedPaintRecorder> recorder);

  void InitializeForRecording(cc::PaintCanvas* canvas) const override;

  bool IsPrinting() { return delegate_ && delegate_->IsPrinting(); }

  static void NotifyWillTransfer(cc::PaintImage::ContentId content_id);

  void AlwaysEnableRasterTimersForTesting(bool value) {
    always_enable_raster_timers_for_testing_ = value;
  }

  const std::optional<cc::PaintRecord>& LastRecording() {
    return last_recording_;
  }

 protected:
  class CanvasImageProvider;

  // Returns true iff the resource provider is (a) using a GPU channel for
  // software SharedImages and (b) that channel has been lost.
  virtual bool IsSoftwareSharedImageGpuChannelLost() const;
  static void NotifyGpuContextLostTask(base::WeakPtr<CanvasResourceProvider>);

  SkSurface* GetSkSurface() const;
  bool UnacceleratedWritePixels(const SkImageInfo& orig_info,
                                const void* pixels,
                                size_t row_bytes,
                                int x,
                                int y);

  gpu::raster::RasterInterface* RasterInterface() const;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const {
    return context_provider_wrapper_;
  }

  scoped_refptr<UnacceleratedStaticBitmapImage> UnacceleratedSnapshot(
      ImageOrientation);

  CanvasResourceProvider(const ResourceProviderType&,
                         gfx::Size size,
                         viz::SharedImageFormat format,
                         SkAlphaType alpha_type,
                         const gfx::ColorSpace& color_space,
                         base::WeakPtr<WebGraphicsContext3DProviderWrapper>
                             context_provider_wrapper,
                         Delegate* delegate);

  virtual void RasterRecord(cc::PaintRecord) = 0;
  void UnacceleratedRasterRecord(cc::PaintRecord);

  CanvasImageProvider* GetOrCreateSWCanvasImageProvider();

  ResourceProviderType type_;
  mutable sk_sp<SkSurface> surface_;  // mutable for lazy init
  SkSurface::ContentChangeMode mode_ = SkSurface::kRetain_ContentChangeMode;

  void OnMemoryDump(base::trace_event::ProcessMemoryDump*) override;

  HighEntropyCanvasOpType GetRecorderHighEntropyCanvasOpTypes() const;

  void ReleaseLockedImages();

  void EnsureSkiaCanvas();

  void Clear();

 private:
  friend class FlushForImageListener;

  virtual sk_sp<SkSurface> CreateSkSurface() const = 0;

  size_t ComputeSurfaceSize() const;
  size_t GetSize() const override;

  // Called after the recording was cleared from any draw ops it might have had.
  void RecordingCleared() override;

  // Disables lines drawing as paths if necessary. Drawing lines as paths is
  // only needed for ganesh.
  void DisableLineDrawingAsPathsIfNecessary();

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;

 protected:
  // Note that `info_` should be const, but the relevant SkImageInfo
  // constructors do not exist.
  SkImageInfo info_;
  gfx::Size size_;
  viz::SharedImageFormat format_;
  SkAlphaType alpha_type_;
  gfx::ColorSpace color_space_;

  std::unique_ptr<CanvasImageProvider> canvas_image_provider_;

  std::unique_ptr<cc::SkiaPaintCanvas> skia_canvas_;
  raw_ptr<Delegate> delegate_ = nullptr;

  // Recording accumulating draw ops. This pointer is always valid and safe to
  // dereference.
  std::unique_ptr<MemoryManagedPaintRecorder> recorder_;

  const cc::PaintImage::Id snapshot_paint_image_id_;
  cc::PaintImage::ContentId snapshot_paint_image_content_id_ =
      cc::PaintImage::kInvalidContentId;
  uint32_t snapshot_sk_image_id_ = 0u;

  bool always_enable_raster_timers_for_testing_ = false;

  // The maximum number of draw ops executed on the canvas, after which the
  // underlying GrContext is flushed.
  // Note: This parameter does not affect the flushing of recorded PaintOps.
  // See kMaxRecordedOpBytes above.
  static constexpr int kMaxDrawsBeforeContextFlush = 50;

  // Parameters for the auto-flushing heuristic.
  size_t max_recorded_op_bytes_;
  size_t max_pinned_image_bytes_;

  bool clear_frame_ = true;
  std::optional<cc::PaintRecord> last_recording_;
};

// Renders canvas2D ops to a Skia RAM-backed bitmap. Mailboxing is not
// supported : cannot be directly composited. For usage by (Offscreen)Canvas2D
// as a last-case resort when it is not possible to create
// CanvasResourceProviderSharedImage.
class PLATFORM_EXPORT Canvas2DResourceProviderBitmap
    : public CanvasResourceProvider {
 public:
  ~Canvas2DResourceProviderBitmap() override = default;

  bool IsValid() const override { return GetSkSurface(); }
  bool IsAccelerated() const override { return false; }
  bool SupportsDirectCompositing() const override { return false; }
  bool IsSingleBuffered() const override { return false; }
  scoped_refptr<StaticBitmapImage> Snapshot(
      ImageOrientation = ImageOrientationEnum::kDefault) override;

  scoped_refptr<StaticBitmapImage> DoExternalDrawAndSnapshot(
      base::FunctionRef<void(MemoryManagedPaintCanvas&)> draw_callback,
      ImageOrientation orientation) override {
    NOTREACHED();
  }
  void RasterRecord(cc::PaintRecord last_recording) override;
  bool WritePixels(const SkImageInfo& orig_info,
                   const void* pixels,
                   size_t row_bytes,
                   int x,
                   int y) override;

  static std::unique_ptr<CanvasResourceProvider> CreateForTesting(
      gfx::Size size,
      const Canvas2DColorParams& color_params,
      ShouldInitialize initialize_provider,
      Delegate* delegate = nullptr);

 protected:
  Canvas2DResourceProviderBitmap(ResourceProviderType type,
                                 gfx::Size size,
                                 viz::SharedImageFormat format,
                                 SkAlphaType alpha_type,
                                 const gfx::ColorSpace& color_space);

 private:
  friend class CanvasRenderingContext2D;
  friend class OffscreenCanvasRenderingContext2D;

  static std::unique_ptr<Canvas2DResourceProviderBitmap> Create(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      ShouldInitialize initialize_provider,
      Delegate* delegate = nullptr);

  Canvas2DResourceProviderBitmap(gfx::Size size,
                                 viz::SharedImageFormat format,
                                 SkAlphaType alpha_type,
                                 const gfx::ColorSpace& color_space,
                                 Delegate* delegate);

  scoped_refptr<CanvasResource> ProduceCanvasResource(FlushReason) override {
    // Production of CanvasResources is used with direct compositing, which is
    // not supported by this class.
    return nullptr;
  }
  sk_sp<SkSurface> CreateSkSurface() const override;
};

// * Renders to a SharedImage, which manages memory internally.
// * Layers may be overlay candidates.
class PLATFORM_EXPORT CanvasResourceProviderSharedImage
    : public CanvasResourceProvider,
      public WebGraphicsContext3DProviderWrapper::DestructionObserver,
      public viz::ContextLostObserver,
      public BitmapGpuChannelLostObserver {
 public:
  CanvasResourceProviderSharedImage(
      gfx::Size,
      viz::SharedImageFormat,
      SkAlphaType,
      const gfx::ColorSpace&,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      bool is_accelerated,
      gpu::SharedImageUsageSet shared_image_usage_flags,
      Delegate*);
  CanvasResourceProviderSharedImage(gfx::Size,
                                    viz::SharedImageFormat,
                                    SkAlphaType,
                                    const gfx::ColorSpace&,
                                    WebGraphicsSharedImageInterfaceProvider*,
                                    Delegate*);
  ~CanvasResourceProviderSharedImage() override;

  // Returns the ClientSharedImage backing this CanvasResourceProvider, if one
  // exists, after flushing the resource and signaling that an external write
  // will occur on it. The caller should wait on `internal_access_sync_token`
  // before writing the contents unless the caller's usage model makes such a
  // wait unnecessary (in which case the client should pass `nullptr` for the
  // token together with an explanation at the callsite).
  // `required_shared_image_usages` is a set of usages that the passed-back
  // ClientSharedImage must support. A copy will be performed if either (a) the
  // display compositor is reading the current resource or (b) the current
  // resource does not support `required_shared_image_usages.` In these cases,
  // `was_copy_performed` will be set to true if it is non-null.
  scoped_refptr<gpu::ClientSharedImage>
  GetBackingClientSharedImageForExternalWrite(
      gpu::SharedImageUsageSet required_shared_image_usages,
      gpu::SyncToken& internal_access_sync_token,
      bool* was_copy_performed = nullptr);

  // Signals that an external write has completed, passing the token that should
  // be waited on to ensure that the service-side operations of the external
  // write have completed. Ensures that the next read of this resource (whether
  // via raster or the compositor) waits on this token.
  void EndExternalWrite(const gpu::SyncToken& external_write_sync_token);

  // For WebGpu RecyclableCanvasResource.
  void OnAcquireRecyclableCanvasResource();
  void OnDestroyRecyclableCanvasResource(const gpu::SyncToken& sync_token);

  // Overwrites the current image (either completely or partially) with the
  // passed-in SharedImage. Waits on `ready_sync_token` before copying; pass
  // SyncToken() if no sync is required. Synthesizes a new sync token in
  // `completion_sync_token` which will satisfy after the image copy completes.
  // In practice, this API can be used to replace a resource with the contents
  // of an AcceleratedStaticBitmapImage or with a WebGPUMailboxTexture.
  bool OverwriteImage(const scoped_refptr<gpu::ClientSharedImage>& shared_image,
                      const gfx::Rect& copy_rect,
                      const gpu::SyncToken& ready_sync_token,
                      gpu::SyncToken& completion_sync_token);
  void ClearUnusedResources() { unused_resources_.clear(); }
  void OnResourceRefReturned(
      scoped_refptr<CanvasResourceSharedImage>&& resource);
  void OnDestroyResource() { --num_inflight_resources_; }
  void SetResourceRecyclingEnabled(bool value);

  bool unused_resources_reclaim_timer_is_running_for_testing() const {
    return unused_resources_reclaim_timer_.IsRunning();
  }
  int NumInflightResourcesForTesting() const { return num_inflight_resources_; }
  gpu::SharedImageUsageSet GetSharedImageUsageFlags() const;
  bool HasUnusedResourcesForTesting() const;

  constexpr static base::TimeDelta kUnusedResourceExpirationTime =
      base::Seconds(5);

  // CanvasResourceProvider:
  CanvasResourceProviderSharedImage* AsSharedImageProvider() final {
    return this;
  }
  bool IsAccelerated() const final { return is_accelerated_; }
  base::ByteSize EstimatedSizeInBytes() const override;
  bool SupportsDirectCompositing() const override { return true; }
  scoped_refptr<CanvasResource> ProduceCanvasResource(
      FlushReason reason) override;
  bool IsValid() const override;
  bool IsSoftwareSharedImageGpuChannelLost() const final;

  // ExternalCanvasDrawHelper() is used by clients that require the invocation
  // of WillDrawIfNeeded() before obtaining a canvas and drawing on it.
  void ExternalCanvasDrawHelper(
      base::FunctionRef<void(MemoryManagedPaintCanvas&)> draw_callback);

  scoped_refptr<StaticBitmapImage> DoExternalDrawAndSnapshot(
      base::FunctionRef<void(MemoryManagedPaintCanvas&)> draw_callback,
      ImageOrientation orientation) final;
  void RasterRecord(cc::PaintRecord last_recording) override;
  sk_sp<SkSurface> CreateSkSurface() const override;
  void OnFlushForImage(cc::PaintImage::ContentId content_id);
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd) final;
  scoped_refptr<StaticBitmapImage> Snapshot(
      ImageOrientation = ImageOrientationEnum::kDefault) override;
  bool IsSingleBuffered() const final;
  bool WritePixels(const SkImageInfo& orig_info,
                   const void* pixels,
                   size_t row_bytes,
                   int x,
                   int y) override;
  // Notifies before any unaccelerated drawing will be done on the resource used
  // by this provider.
  void WillDrawUnaccelerated();

  // This is a workaround to ensure WaitSyncToken() is still called even when
  // copying is effectively skipped due to a dummy WebGPU texture.
  void PrepareForWebGPUDummyMailbox();

  scoped_refptr<CanvasResource> ProduceCanvasResource() {
    return ProduceCanvasResource(FlushReason::kOther);
  }

  // WebGraphicsContext3DProvider::DestructionObserver implementation.
  void OnContextDestroyed() override;

 private:
  CanvasImageProvider* GetOrCreateCanvasImageProvider();
  scoped_refptr<CanvasResourceSharedImage> CreateResource();

  // The maximum number of in-flight resources waiting to be used for
  // recycling.
  static constexpr int kMaxRecycledCanvasResources = 3;

  struct UnusedResource {
    UnusedResource(base::TimeTicks last_use,
                   scoped_refptr<CanvasResourceSharedImage> resource)
        : last_use(last_use), resource(std::move(resource)) {}
    base::TimeTicks last_use;
    scoped_refptr<CanvasResourceSharedImage> resource;
  };

  void RegisterUnusedResource(
      scoped_refptr<CanvasResourceSharedImage>&& resource);
  scoped_refptr<CanvasResourceSharedImage> NewOrRecycledResource();
  bool IsResourceUsable(CanvasResourceSharedImage* resource);
  CanvasResourceSharedImage* resource() {
    return static_cast<CanvasResourceSharedImage*>(resource_.get());
  }
  const CanvasResourceSharedImage* resource() const {
    return static_cast<const CanvasResourceSharedImage*>(resource_.get());
  }
  bool ShouldReplaceTargetBuffer(
      PaintImage::ContentId content_id = PaintImage::kInvalidContentId);
  void EnsureWriteAccess();
  void EndWriteAccess();
  std::unique_ptr<gpu::RasterScopedAccess> WillDrawInternal();

  void RecycleResource(scoped_refptr<CanvasResourceSharedImage>&& resource);
  void MaybePostUnusedResourcesReclaimTask();
  void ClearOldUnusedResources();
  base::WeakPtr<CanvasResourceProviderSharedImage> CreateWeakPtr();

  // `viz::ContextLostObserver`:
  void OnContextLost() final;

  // BitmapGpuChannelLostObserver:
  void OnGpuChannelLost() final;

  // If this instance is single-buffered or |resource_recycling_enabled_| is
  // false, |unused_resources_| will be empty.
  Vector<UnusedResource> unused_resources_;
  int num_inflight_resources_ = 0;
  int max_inflight_resources_ = 0;
  base::OneShotTimer unused_resources_reclaim_timer_;
  bool resource_recycling_enabled_ = true;

  // `raster_context_provider_` holds a reference on the shared
  // `RasterContextProvider`, to keep it alive until it notifies us after the
  // GPU context is lost. Without this, no `CanvasResourceProvider` would get
  // notified after the shared `WebGraphicsContext3DProviderWrapper` instance is
  // recreated.
  scoped_refptr<viz::RasterContextProvider> raster_context_provider_;
  base::WeakPtr<WebGraphicsSharedImageInterfaceProvider>
      shared_image_interface_provider_;
  const bool is_accelerated_;
  gpu::SharedImageUsageSet shared_image_usage_flags_;
  bool current_resource_has_write_access_ = false;
  bool is_software_ = false;
  bool is_cleared_ = false;

  // The resource that is currently being used by this provider.
  scoped_refptr<CanvasResourceSharedImage> resource_;
  scoped_refptr<StaticBitmapImage> cached_snapshot_;
  cc::PaintImage::ContentId cached_content_id_ =
      cc::PaintImage::kInvalidContentId;

  bool notified_context_lost_ = false;
  base::WeakPtrFactory<CanvasResourceProviderSharedImage> weak_ptr_factory_{
      this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_PROVIDER_H_
