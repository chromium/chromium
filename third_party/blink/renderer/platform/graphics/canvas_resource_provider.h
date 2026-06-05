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
#include "third_party/blink/renderer/platform/graphics/canvas_snapshot_info.h"
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
class CanvasNon2DResourceProviderSharedImage;
class Canvas2DResourceProviderSharedImage;
class CanvasImageProvider;
class MemoryManagedPaintCanvas;
class OffscreenCanvasRenderingContext2D;
class StaticBitmapImage;
class WebGraphicsSharedImageInterfaceProvider;

class FlushForImageObserver : public base::CheckedObserver {
 public:
  virtual void OnFlushForImage(cc::PaintImage::ContentId content_id) = 0;
};

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
      public MemoryManagedPaintRecorder::Client,
      public ScopedRasterTimer::Host {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void NotifyGpuContextLost() = 0;
    virtual void InitializeForRecording(cc::PaintCanvas* canvas) const = 0;
    virtual bool IsPrinting() const { return false; }
    virtual scoped_refptr<const cc::AnimatedImageFrameIndexMap>
    GetAnimatedImageFrameIndexes() const {
      return nullptr;
    }
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
    kExternalBitmap [[deprecated]] = 10,
    kMaxValue = kExternalBitmap,
  };
#pragma GCC diagnostic pop

  virtual Canvas2DResourceProviderSharedImage* AsSharedImageProvider() {
    return nullptr;
  }

  // The ImageOrientationEnum conveys the desired orientation of the image, and
  // should be derived from the source of the bitmap data.
  virtual scoped_refptr<StaticBitmapImage> Snapshot(
      ImageOrientation = ImageOrientationEnum::kDefault) = 0;

  void SetDelegate(Delegate* delegate) { delegate_ = delegate; }

  MemoryManagedPaintCanvas& GetCanvasForTesting();
  std::optional<cc::PaintRecord> Flush(FlushReason = FlushReason::kOther);
  virtual ScopedRasterTimer CreateScopedRasterTimer();

  virtual bool IsAccelerated() const = 0;
  virtual bool IsValid() const = 0;
  virtual bool IsGpuContextLost() const = 0;
  SkSurfaceProps GetSkSurfaceProps() const;
  virtual viz::SharedImageFormat GetSharedImageFormat() const = 0;
  virtual const gfx::ColorSpace& GetColorSpace() const = 0;
  virtual const gfx::HDRMetadata& GetHdrMetadata() const = 0;
  virtual SkAlphaType GetAlphaType() const = 0;
  virtual gfx::Size Size() const = 0;
  virtual base::ByteSize EstimatedSizeInBytes() const {
    return base::ByteSize(GetSharedImageFormat().EstimatedSizeInBytes(Size()));
  }

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

  virtual void FlushIfRecordingLimitExceeded() = 0;

  virtual const MemoryManagedPaintRecorder& Recorder() const = 0;
  virtual MemoryManagedPaintRecorder& Recorder() = 0;
  virtual std::unique_ptr<MemoryManagedPaintRecorder> ReleaseRecorder() = 0;
  virtual void SetRecorder(
      std::unique_ptr<MemoryManagedPaintRecorder> recorder) = 0;

  void InitializeForRecording(cc::PaintCanvas* canvas) const override;

  bool IsPrinting() { return delegate_ && delegate_->IsPrinting(); }

  static void NotifyWillTransfer(cc::PaintImage::ContentId content_id);

  constexpr static base::TimeDelta kUnusedResourceExpirationTime =
      base::Seconds(5);

  void AlwaysEnableRasterTimersForTesting(bool value) {
    always_enable_raster_timers_for_testing_ = value;
  }

  const std::optional<cc::PaintRecord>& LastRecording() {
    return last_recording_;
  }

 protected:
  SkSurface* GetSkSurface() const;

  scoped_refptr<UnacceleratedStaticBitmapImage> UnacceleratedSnapshot(
      ImageOrientation);

  CanvasResourceProvider(const ResourceProviderType&,
                         Delegate* delegate);

  virtual void RasterRecord(cc::PaintRecord) = 0;
  void UnacceleratedRasterRecord(cc::PaintRecord);

  CanvasImageProvider* GetOrCreateSWCanvasImageProvider();

  ResourceProviderType type_;
  mutable sk_sp<SkSurface> surface_;  // mutable for lazy init

  // Whether the content of the current resource must be transferred to a new
  // resource on CopyOnWrite. True by default, but can be set to false as an
  // optimization if the current resource is known to have been cleared.
  // This is only used for Canvas2D.
  bool must_preserve_content_on_copy_on_write_ = true;

  void OnMemoryDump(base::trace_event::ProcessMemoryDump*) override;


 private:
  friend class FlushForImageListener;

  virtual sk_sp<SkSurface> CreateSkSurface() const = 0;

  size_t ComputeSurfaceSize() const;
  size_t GetSize() const override;

  // Called after the recording was cleared from any draw ops it might have had.
  // Canvas2D-specific, as it is called only when `recorder_` is
  // instantiated by Canvas2D-specific subclasses.
  void RecordingCleared() override;

 protected:
  // Should only be called from static Create*() methods.
  // TODO(crbug.com/352263194): Eliminate this method by inlining its body at
  // callsites.
  void ClearAtCreation();

  std::unique_ptr<CanvasImageProvider> canvas_image_provider_;

  std::unique_ptr<cc::SkiaPaintCanvas> skia_canvas_;
  raw_ptr<Delegate> delegate_ = nullptr;

  const cc::PaintImage::Id snapshot_paint_image_id_;
  cc::PaintImage::ContentId snapshot_paint_image_content_id_ =
      cc::PaintImage::kInvalidContentId;
  uint32_t snapshot_sk_image_id_ = 0u;

  bool always_enable_raster_timers_for_testing_ = false;

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
  bool IsGpuContextLost() const override { return true; }
  scoped_refptr<StaticBitmapImage> Snapshot(
      ImageOrientation = ImageOrientationEnum::kDefault) override;

  void RasterRecord(cc::PaintRecord last_recording) override;
  bool WritePixels(const SkImageInfo& orig_info,
                   const void* pixels,
                   size_t row_bytes,
                   int x,
                   int y) override;

  static std::unique_ptr<CanvasResourceProvider> CreateForTesting(
      gfx::Size size,
      const Canvas2DColorParams& color_params);

  viz::SharedImageFormat GetSharedImageFormat() const override {
    return format_;
  }
  const gfx::ColorSpace& GetColorSpace() const override { return color_space_; }
  const gfx::HDRMetadata& GetHdrMetadata() const override {
    return hdr_metadata_;
  }
  SkAlphaType GetAlphaType() const override { return alpha_type_; }
  gfx::Size Size() const override { return size_; }

  void FlushIfRecordingLimitExceeded() override;

  const MemoryManagedPaintRecorder& Recorder() const override {
    return *recorder_;
  }
  MemoryManagedPaintRecorder& Recorder() override { return *recorder_; }
  std::unique_ptr<MemoryManagedPaintRecorder> ReleaseRecorder() override;
  void SetRecorder(
      std::unique_ptr<MemoryManagedPaintRecorder> recorder) override;

 private:
  friend class CanvasRenderingContext2D;
  friend class OffscreenCanvasRenderingContext2D;

  // The returned instance will have been cleared at creation.
  static std::unique_ptr<Canvas2DResourceProviderBitmap> CreateWithClear(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      const gfx::HDRMetadata& hdr_metadata,
      CanvasResourceProvider::Delegate* delegate = nullptr);
  static std::unique_ptr<Canvas2DResourceProviderBitmap> CreateWithClear(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      CanvasResourceProvider::Delegate* delegate = nullptr) {
    return CreateWithClear(size, format, alpha_type, color_space,
                           gfx::HDRMetadata(), delegate);
  }
  Canvas2DResourceProviderBitmap(gfx::Size size,
                                 viz::SharedImageFormat format,
                                 SkAlphaType alpha_type,
                                 const gfx::ColorSpace& color_space,
                                 const gfx::HDRMetadata& hdr_metadata,
                                 Delegate* delegate);

  sk_sp<SkSurface> CreateSkSurface() const override;

  gfx::Size size_;
  viz::SharedImageFormat format_;
  SkAlphaType alpha_type_;
  gfx::ColorSpace color_space_;
  gfx::HDRMetadata hdr_metadata_;
  std::unique_ptr<MemoryManagedPaintRecorder> recorder_;
  size_t max_recorded_op_bytes_;
  size_t max_pinned_image_bytes_;
};

// * Subclass of CanvasResourceProvider that is specialized for usage
// * by Canvas2D.
class PLATFORM_EXPORT Canvas2DResourceProviderSharedImage
    : public CanvasResourceProvider,
      public CanvasResourceSharedImage::Client,
      public FlushForImageObserver,
      public WebGraphicsContext3DProviderWrapper::DestructionObserver,
      public viz::ContextLostObserver,
      public BitmapGpuChannelLostObserver {
 public:
  // The returned instance will have been cleared at creation.
  static std::unique_ptr<Canvas2DResourceProviderSharedImage> CreateWithClear(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      const gfx::HDRMetadata& hdr_metadata,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      RasterMode raster_mode,
      gpu::SharedImageUsageSet shared_image_usage_flags,
      CanvasResourceProvider::Delegate* delegate = nullptr);
  static std::unique_ptr<Canvas2DResourceProviderSharedImage> CreateWithClear(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper,
      RasterMode raster_mode,
      gpu::SharedImageUsageSet shared_image_usage_flags,
      CanvasResourceProvider::Delegate* delegate = nullptr) {
    return CreateWithClear(size, format, alpha_type, color_space,
                           gfx::HDRMetadata(), context_provider_wrapper,
                           raster_mode, shared_image_usage_flags, delegate);
  }
  static std::unique_ptr<Canvas2DResourceProviderSharedImage> CreateWithClear(
      gfx::Size size,
      const Canvas2DColorParams& color_params,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      RasterMode raster_mode,
      gpu::SharedImageUsageSet shared_image_usage_flags);

  // The returned instance will have been cleared at creation.
  static std::unique_ptr<Canvas2DResourceProviderSharedImage>
  CreateWithClearForSoftwareCompositor(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      const gfx::HDRMetadata& hdr_metadata,
      WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider,
      CanvasResourceProvider::Delegate* delegate = nullptr);
  static std::unique_ptr<Canvas2DResourceProviderSharedImage>
  CreateWithClearForSoftwareCompositor(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider,
      CanvasResourceProvider::Delegate* delegate = nullptr) {
    return CreateWithClearForSoftwareCompositor(
        size, format, alpha_type, color_space, gfx::HDRMetadata(),
        shared_image_interface_provider, delegate);
  }

  Canvas2DResourceProviderSharedImage(
      gfx::Size,
      viz::SharedImageFormat,
      SkAlphaType,
      const gfx::ColorSpace&,
      const gfx::HDRMetadata&,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      bool is_accelerated,
      gpu::SharedImageUsageSet shared_image_usage_flags,
      Delegate*);
  Canvas2DResourceProviderSharedImage(gfx::Size,
                                      viz::SharedImageFormat,
                                      SkAlphaType,
                                      const gfx::ColorSpace&,
                                      const gfx::HDRMetadata&,
                                      WebGraphicsSharedImageInterfaceProvider*,
                                      Delegate*);
  ~Canvas2DResourceProviderSharedImage() override;

  void ClearUnusedResources();
  gpu::SharedImageUsageSet GetSharedImageUsageFlags() const;
  bool unused_resources_reclaim_timer_is_running_for_testing() const;
  bool HasUnusedResourcesForTesting() const;
  bool IsSingleBuffered() const;

  bool IsAccelerated() const override { return is_accelerated_; }
  bool IsSoftware() const { return is_software_; }
  bool IsGpuContextLost() const override;

  viz::SharedImageFormat GetSharedImageFormat() const override {
    return format_;
  }
  const gfx::ColorSpace& GetColorSpace() const override { return color_space_; }
  const gfx::HDRMetadata& GetHdrMetadata() const override {
    return hdr_metadata_;
  }
  SkAlphaType GetAlphaType() const override { return alpha_type_; }
  gfx::Size Size() const override { return size_; }

  void FlushIfRecordingLimitExceeded() override;

  // WebGraphicsContext3DProviderWrapper::DestructionObserver implementation.
  void OnContextDestroyed() override;
  void OnResourceRefReturned(
      scoped_refptr<CanvasResourceSharedImage>&& resource) override;
  void OnDestroyResource() override { --num_inflight_resources_; }
  int NumInflightResourcesForTesting() const { return num_inflight_resources_; }
  base::ByteSize EstimatedSizeInBytes() const override;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd) override;

  virtual scoped_refptr<CanvasResource> ProduceCanvasResource(FlushReason);

  void EnsureWriteAccess();
  void EndWriteAccess();

  scoped_refptr<CanvasResourceSharedImage> NewOrRecycledResource();

  // CanvasResourceProvider:
  void OnFlushForImage(cc::PaintImage::ContentId content_id) override;
  void RasterRecord(cc::PaintRecord last_recording) override;
  bool IsValid() const override;
  Canvas2DResourceProviderSharedImage* AsSharedImageProvider() final {
    return this;
  }
  scoped_refptr<StaticBitmapImage> Snapshot(
      ImageOrientation = ImageOrientationEnum::kDefault) override;
  bool WritePixels(const SkImageInfo& orig_info,
                   const void* pixels,
                   size_t row_bytes,
                   int x,
                   int y) override;

  const MemoryManagedPaintRecorder& Recorder() const override {
    return *recorder_;
  }
  MemoryManagedPaintRecorder& Recorder() override { return *recorder_; }
  std::unique_ptr<MemoryManagedPaintRecorder> ReleaseRecorder() override;
  void SetRecorder(
      std::unique_ptr<MemoryManagedPaintRecorder> recorder) override;

  void SetResourceRecyclingEnabled(bool value);

  // Signals that the ongoing transfer of this resource to WebGPU has completed,
  // passing the token that should be waited on to ensure that the service-side
  // operations of the WebGPU write have completed. Ensures that the next read
  // of this resource (whether via raster or the compositor) waits on this
  // token.
  void TransferBackFromWebGPU(const gpu::SyncToken& webgpu_write_sync_token);

  ScopedRasterTimer CreateScopedRasterTimer() override;

 private:
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const {
    return context_provider_wrapper_;
  }

  // viz::ContextLostObserver implementation.
  void OnContextLost() override;

  // BitmapGpuChannelLostObserver implementation.
  void OnGpuChannelLost() override;

  bool ShouldReplaceTargetBuffer(
      PaintImage::ContentId content_id = PaintImage::kInvalidContentId);

  CanvasImageProvider* GetOrCreateCanvasImageProvider();
  std::unique_ptr<gpu::RasterScopedAccess> WillDrawInternal();

  // Notifies before any unaccelerated drawing will be done on the resource used
  // by this provider.
  void WillDrawUnaccelerated();
  void DisableLineDrawingAsPathsIfNecessary();

  sk_sp<SkSurface> CreateSkSurface() const override;
  gpu::raster::RasterInterface* RasterInterface() const;

  base::WeakPtr<Canvas2DResourceProviderSharedImage> CreateWeakPtr();

  static void NotifyGpuContextLostTask(
      base::WeakPtr<Canvas2DResourceProviderSharedImage>);

  // The maximum number of in-flight resources waiting to be used for
  // recycling.
  static constexpr int kMaxRecycledCanvasResources = 3;

  CanvasResourceSharedImage* resource() {
    return static_cast<CanvasResourceSharedImage*>(resource_.get());
  }
  const CanvasResourceSharedImage* resource() const {
    return static_cast<const CanvasResourceSharedImage*>(resource_.get());
  }

  // If this instance is single-buffered or |resource_recycling_enabled_| is
  // false, |image_pool_| will not recycle resources.
  std::unique_ptr<gpu::SharedImagePool<CanvasResourceSharedImage>> image_pool_;

  scoped_refptr<CanvasResourceSharedImage> resource_;

  bool current_resource_has_write_access_ = false;

  cc::PaintImage::ContentId cached_content_id_ =
      cc::PaintImage::kInvalidContentId;
  scoped_refptr<StaticBitmapImage> cached_snapshot_;

  const bool is_accelerated_;
  const bool is_software_;
  bool is_cleared_ = false;
  bool notified_context_lost_ = false;

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  base::WeakPtr<WebGraphicsSharedImageInterfaceProvider>
      shared_image_interface_provider_;

  // `raster_context_provider_` holds a reference on the shared
  // `RasterContextProvider`, to keep it alive until it notifies us after the
  // GPU context is lost. Without this, instances of this class would not get
  // notified after the shared `WebGraphicsContext3DProviderWrapper` instance is
  // recreated.
  scoped_refptr<viz::RasterContextProvider> raster_context_provider_;

  bool resource_recycling_enabled_ = true;
  int num_inflight_resources_ = 0;
  int max_inflight_resources_ = 0;

  gfx::Size size_;
  viz::SharedImageFormat format_;
  SkAlphaType alpha_type_;
  gfx::ColorSpace color_space_;
  gfx::HDRMetadata hdr_metadata_;

  std::unique_ptr<MemoryManagedPaintRecorder> recorder_;
  size_t max_recorded_op_bytes_;
  size_t max_pinned_image_bytes_;

  base::WeakPtrFactory<Canvas2DResourceProviderSharedImage> weak_ptr_factory_{
      this};
};

// * Subclass of CanvasResourceProvider that is specialized for usage
// * by non-Canvas2D clients.
class PLATFORM_EXPORT CanvasNon2DResourceProviderSharedImage
    : public CanvasMemoryDumpClient,
      public MemoryManagedPaintRecorder::Client,
      public CanvasResourceSharedImage::Client,
      public FlushForImageObserver,
      public WebGraphicsContext3DProviderWrapper::DestructionObserver,
      public viz::ContextLostObserver,
      public BitmapGpuChannelLostObserver,
      public ScopedRasterTimer::Host {
 public:
  static std::unique_ptr<CanvasNon2DResourceProviderSharedImage> Create(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      const gfx::HDRMetadata& hdr_metadata,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      gpu::SharedImageUsageSet shared_image_usage_flags,
      CanvasResourceProvider::Delegate* delegate = nullptr);
  static std::unique_ptr<CanvasNon2DResourceProviderSharedImage> Create(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper,
      gpu::SharedImageUsageSet shared_image_usage_flags,
      CanvasResourceProvider::Delegate* delegate = nullptr) {
    return Create(size, format, alpha_type, color_space, gfx::HDRMetadata(),
                  context_provider_wrapper, shared_image_usage_flags, delegate);
  }

  static std::unique_ptr<CanvasNon2DResourceProviderSharedImage> Create(
      gfx::Size size,
      const Canvas2DColorParams& color_params,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      gpu::SharedImageUsageSet shared_image_usage_flags);

  static std::unique_ptr<CanvasNon2DResourceProviderSharedImage>
  CreateForWebGPU(gfx::Size size,
                  viz::SharedImageFormat format,
                  SkAlphaType alpha_type,
                  const gfx::ColorSpace& color_space,
                  const gfx::HDRMetadata& hdr_metadata,
                  gpu::SharedImageUsageSet shared_image_usage_flags = {},
                  CanvasResourceProvider::Delegate* delegate = nullptr);
  static std::unique_ptr<CanvasNon2DResourceProviderSharedImage>
  CreateForWebGPU(gfx::Size size,
                  viz::SharedImageFormat format,
                  SkAlphaType alpha_type,
                  const gfx::ColorSpace& color_space,
                  gpu::SharedImageUsageSet shared_image_usage_flags = {},
                  CanvasResourceProvider::Delegate* delegate = nullptr) {
    return CreateForWebGPU(size, format, alpha_type, color_space,
                           gfx::HDRMetadata(), shared_image_usage_flags,
                           delegate);
  }

  static std::unique_ptr<CanvasNon2DResourceProviderSharedImage>
  CreateForSoftwareCompositor(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      const gfx::HDRMetadata& hdr_metadata,
      WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider,
      CanvasResourceProvider::Delegate* delegate = nullptr);
  static std::unique_ptr<CanvasNon2DResourceProviderSharedImage>
  CreateForSoftwareCompositor(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider,
      CanvasResourceProvider::Delegate* delegate = nullptr) {
    return CreateForSoftwareCompositor(
        size, format, alpha_type, color_space, gfx::HDRMetadata(),
        shared_image_interface_provider, delegate);
  }

  static std::unique_ptr<CanvasNon2DResourceProviderSharedImage>
  CreateForSoftwareCompositor(
      gfx::Size size,
      const Canvas2DColorParams& color_params,
      WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider);

  CanvasNon2DResourceProviderSharedImage(
      gfx::Size,
      viz::SharedImageFormat,
      SkAlphaType,
      const gfx::ColorSpace&,
      const gfx::HDRMetadata&,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      bool is_accelerated,
      gpu::SharedImageUsageSet shared_image_usage_flags,
      CanvasResourceProvider::Delegate*);
  CanvasNon2DResourceProviderSharedImage(
      gfx::Size,
      viz::SharedImageFormat,
      SkAlphaType,
      const gfx::ColorSpace&,
      const gfx::HDRMetadata&,
      WebGraphicsSharedImageInterfaceProvider*,
      CanvasResourceProvider::Delegate*);
  ~CanvasNon2DResourceProviderSharedImage() override;

  void ClearUnusedResources();
  gpu::SharedImageUsageSet GetSharedImageUsageFlags() const;
  bool IsSingleBuffered() const;

  bool IsAccelerated() const { return is_accelerated_; }
  bool IsSoftware() const { return is_software_; }
  bool IsGpuContextLost() const;

  CanvasResourceProvider::ResourceProviderType GetType() const {
    return CanvasResourceProvider::kSharedImage;
  }

  SkSurface* GetSkSurface() const;

  gfx::Size Size() const { return size_; }
  viz::SharedImageFormat GetSharedImageFormat() const { return format_; }
  const gfx::ColorSpace& GetColorSpace() const { return color_space_; }
  const gfx::HDRMetadata& GetHdrMetadata() const { return hdr_metadata_; }
  SkAlphaType GetAlphaType() const { return alpha_type_; }
  CanvasSnapshotInfo GetInfo() const {
    return {
        .alpha_type = GetAlphaType(),
        .color_space = GetColorSpace(),
        .hdr_metadata = GetHdrMetadata(),
        .format = GetSharedImageFormat(),
        .size = Size(),
    };
  }

  // WebGraphicsContext3DProviderWrapper::DestructionObserver implementation.
  void OnContextDestroyed() override;

  void OnResourceRefReturned(
      scoped_refptr<CanvasResourceSharedImage>&& resource) override;
  void OnDestroyResource() override { --num_inflight_resources_; }
  base::ByteSize EstimatedSizeInBytes() const;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd) override;
  size_t GetSize() const override;

  // MemoryManagedPaintRecorder::Client implementation.
  void RecordingCleared() override;
  void InitializeForRecording(cc::PaintCanvas* canvas) const override;

  SkSurfaceProps GetSkSurfaceProps() const;

  void EnsureWriteAccess();
  void EndWriteAccess();

  scoped_refptr<CanvasResourceSharedImage> NewOrRecycledResource();

  scoped_refptr<CanvasResource> ProduceCanvasResource();

  // FlushForImageObserver implementation:
  void OnFlushForImage(cc::PaintImage::ContentId content_id) override;

  bool IsValid() const;
  scoped_refptr<StaticBitmapImage> Snapshot(
      ImageOrientation = ImageOrientationEnum::kDefault);

  // NOTE: Can only be used if this instance is accelerated.
  bool UploadToBackingSharedImage(const SkPixmap& pixmap,
                                  uint32_t src_x,
                                  uint32_t src_y);

  scoped_refptr<CanvasResource> DoExternalOverdrawAndProduceResource(
      base::FunctionRef<void(cc::PaintCanvas&)> draw_callback);

  scoped_refptr<StaticBitmapImage> DoExternalOverdrawAndSnapshot(
      base::FunctionRef<void(cc::PaintCanvas&)> draw_callback,
      ImageOrientation orientation);

  // For WebGpu RecyclableCanvasResource.
  void OnAcquireRecyclableCanvasResource();
  void OnDestroyRecyclableCanvasResource(const gpu::SyncToken& sync_token);

  // This is a workaround to ensure WaitSyncToken() is still called even when
  // copying is effectively skipped due to a dummy WebGPU texture.
  void PrepareForWebGPUDummyMailbox();

  // Returns the ClientSharedImage backing this CanvasResourceProvider, if one
  // exists, after flushing the resource and signaling that an external write
  // will occur on it. The caller should wait on `internal_access_sync_token`
  // before writing the contents. When the external write is complete, the
  // caller should call `EndExternalWrite()`.
  scoped_refptr<gpu::ClientSharedImage> BeginExternalOverwrite(
      gpu::SyncToken& internal_access_sync_token);

  // Copies the contents of the passed-in SharedImage at `copy_rect` into this
  // instance's SharedImage. Waits on `ready_sync_token` before copying; pass
  // SyncToken() if no sync is required. Synthesizes a new sync token in
  // `completion_sync_token` which will satisfy after the image copy completes.
  // NOTE: Can only be used if this instance is accelerated.
  bool CopyToBackingSharedImage(
      const scoped_refptr<gpu::ClientSharedImage>& shared_image,
      uint32_t src_x,
      uint32_t src_y,
      const gpu::SyncToken& ready_sync_token,
      gpu::SyncToken& completion_sync_token);

  // Signals that an external write has completed, passing the token that should
  // be waited on to ensure that the service-side operations of the external
  // write have completed. Ensures that the next read of this resource (whether
  // via raster or the compositor) waits on this token.
  void EndExternalWrite(const gpu::SyncToken& external_write_sync_token);

  sk_sp<SkSurface> CreateSkSurface() const;
  gpu::raster::RasterInterface* RasterInterface() const;

  base::WeakPtr<CanvasNon2DResourceProviderSharedImage> CreateWeakPtr();

  static void NotifyGpuContextLostTask(
      base::WeakPtr<CanvasNon2DResourceProviderSharedImage>);

  // The maximum number of in-flight resources waiting to be used for
  // recycling.
  static constexpr int kMaxRecycledCanvasResources = 3;

  CanvasResourceSharedImage* resource() {
    return static_cast<CanvasResourceSharedImage*>(resource_.get());
  }
  const CanvasResourceSharedImage* resource() const {
    return static_cast<const CanvasResourceSharedImage*>(resource_.get());
  }

 private:
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const {
    return context_provider_wrapper_;
  }

  // viz::ContextLostObserver implementation.
  void OnContextLost() override;

  // BitmapGpuChannelLostObserver implementation.
  void OnGpuChannelLost() override;

  bool ShouldReplaceTargetBuffer(
      PaintImage::ContentId content_id = PaintImage::kInvalidContentId);
  void FlushRecording(cc::PaintRecord last_recording);

  std::unique_ptr<gpu::RasterScopedAccess> WillDrawInternal();

  const gfx::Size size_;
  const viz::SharedImageFormat format_;
  const SkAlphaType alpha_type_;
  const gfx::ColorSpace color_space_;
  const gfx::HDRMetadata hdr_metadata_;
  const raw_ptr<CanvasResourceProvider::Delegate> delegate_;
  const bool is_accelerated_;
  const bool is_software_;

  mutable sk_sp<SkSurface> surface_;
  uint32_t snapshot_sk_image_id_ = 0u;
  const cc::PaintImage::Id snapshot_paint_image_id_;
  cc::PaintImage::ContentId snapshot_paint_image_content_id_ =
      cc::PaintImage::kInvalidContentId;

  std::unique_ptr<CanvasImageProvider> canvas_image_provider_;
  std::unique_ptr<cc::SkiaPaintCanvas> skia_canvas_;
  std::unique_ptr<MemoryManagedPaintRecorder> recorder_for_external_draws_;

  // If this instance is single-buffered or |resource_recycling_enabled_| is
  // false, |image_pool_| will not recycle resources.
  std::unique_ptr<gpu::SharedImagePool<CanvasResourceSharedImage>> image_pool_;

  scoped_refptr<CanvasResourceSharedImage> resource_;

  bool current_resource_has_write_access_ = false;

  cc::PaintImage::ContentId cached_content_id_ =
      cc::PaintImage::kInvalidContentId;
  scoped_refptr<StaticBitmapImage> cached_snapshot_;

  bool is_cleared_ = false;
  bool notified_context_lost_ = false;

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  base::WeakPtr<WebGraphicsSharedImageInterfaceProvider>
      shared_image_interface_provider_;

  // `raster_context_provider_` holds a reference on the shared
  // `RasterContextProvider`, to keep it alive until it notifies us after the
  // GPU context is lost. Without this, instances of this class would not get
  // notified after the shared `WebGraphicsContext3DProviderWrapper` instance is
  // recreated.
  scoped_refptr<viz::RasterContextProvider> raster_context_provider_;

  int num_inflight_resources_ = 0;
  int max_inflight_resources_ = 0;

  base::WeakPtrFactory<CanvasNon2DResourceProviderSharedImage>
      weak_ptr_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_PROVIDER_H_
