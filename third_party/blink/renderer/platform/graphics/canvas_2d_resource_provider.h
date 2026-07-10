// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_2D_RESOURCE_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_2D_RESOURCE_PROVIDER_H_

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
#include "third_party/blink/renderer/platform/graphics/flush_for_image_listener.h"
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

#include "base/metrics/field_trial_params.h"

namespace blink {

PLATFORM_EXPORT BASE_DECLARE_FEATURE(kCanvas2DAutoFlushParams);
PLATFORM_EXPORT BASE_DECLARE_FEATURE(kCanvas2DReclaimUnusedResources);
PLATFORM_EXPORT BASE_DECLARE_FEATURE(kAppendCpuUsages);
PLATFORM_EXPORT BASE_DECLARE_FEATURE(kCanvasResourceIsWebGPUCompatible);

PLATFORM_EXPORT extern const base::FeatureParam<int> kMaxRecordedOpKB;
PLATFORM_EXPORT extern const base::FeatureParam<int> kMaxPinnedImageKB;

class CanvasResource;
class CanvasResourceSharedImage;
class Canvas2DResourceProvider;
class CanvasImageProvider;
class MemoryManagedPaintCanvas;
class StaticBitmapImage;
class WebGraphicsSharedImageInterfaceProvider;

// Specifies whether the provider should rasterize paint commands on the CPU
// or GPU. This is used to support software raster with GPU compositing.
enum class RasterMode {
  kGPU,
  kCPU,
};

// Canvas2DResourceProvider
//==============================================================================
//
// Encapsulates a drawable graphics resource for Canvas2D.
// Canvas2DResourceProvider serves as a provider of drawable surfaces for 2D
// rendering with Skia.
//
// General usage:
//   1) Use the Create() static method to create an instance
//   2) use Canvas() to get a drawing interface
//   3) Call Snapshot() to acquire a bitmap with the rendered image in it.

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
enum class CanvasResourceProviderType {
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

class PLATFORM_EXPORT CanvasResourceProviderDelegate {
 public:
  virtual ~CanvasResourceProviderDelegate() = default;

  virtual void NotifyGpuContextLost() = 0;
  virtual void InitializeForRecording(cc::PaintCanvas* canvas) const = 0;
  virtual bool IsPrinting() const { return false; }
  // This is used to apply a map of frame indexes to be used by
  // PlaybackImageProvider::GetRasterContent. When the delegate is a
  // CanvasRenderingContextHost, it is treated as an index into an array
  // of maps, one per ElementImage which has been drawn into the canvas by
  // a call to drawElementImage(). This is only used by canvas2d; webgl and
  // webgpu canvases don't need this because they rasterize each ElementImage
  // as a stand-alone PaintOpBuffer.
  virtual scoped_refptr<const cc::AnimatedImageFrameIndexMap>
  GetAnimatedImageFrameIndexes(uint32_t id) const {
    return nullptr;
  }
  virtual void DidFlush() {}
};

// * Subclass of CanvasResourceProvider that is specialized for usage
// * by Canvas2D.
class PLATFORM_EXPORT Canvas2DResourceProvider
    : public CanvasResourceSharedImage::Client,
      public WebGraphicsContext3DProviderWrapper::DestructionObserver,
      public viz::ContextLostObserver,
      public BitmapGpuChannelLostObserver,
      public CanvasMemoryDumpClient,
      public MemoryManagedPaintRecorder::Client,
      public ScopedRasterTimer::Host {
 public:
  constexpr static base::TimeDelta kUnusedResourceExpirationTime =
      base::Seconds(5);

  // The returned instance will have been cleared at creation.
  static std::unique_ptr<Canvas2DResourceProvider> CreateWithClear(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      const gfx::HDRMetadata& hdr_metadata,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      RasterMode raster_mode,
      gpu::SharedImageUsageSet shared_image_usage_flags,
      CanvasResourceProviderDelegate* delegate = nullptr);
  static std::unique_ptr<Canvas2DResourceProvider> CreateWithClear(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper,
      RasterMode raster_mode,
      gpu::SharedImageUsageSet shared_image_usage_flags,
      CanvasResourceProviderDelegate* delegate = nullptr) {
    return CreateWithClear(size, format, alpha_type, color_space,
                           gfx::HDRMetadata(), context_provider_wrapper,
                           raster_mode, shared_image_usage_flags, delegate);
  }
  static std::unique_ptr<Canvas2DResourceProvider> CreateWithClear(
      gfx::Size size,
      const Canvas2DColorParams& color_params,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      RasterMode raster_mode,
      gpu::SharedImageUsageSet shared_image_usage_flags);

  // The returned instance will have been cleared at creation.
  static std::unique_ptr<Canvas2DResourceProvider>
  CreateWithClearForSoftwareCompositor(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      const gfx::HDRMetadata& hdr_metadata,
      WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider,
      CanvasResourceProviderDelegate* delegate = nullptr);
  static std::unique_ptr<Canvas2DResourceProvider>
  CreateWithClearForSoftwareCompositor(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider,
      CanvasResourceProviderDelegate* delegate = nullptr) {
    return CreateWithClearForSoftwareCompositor(
        size, format, alpha_type, color_space, gfx::HDRMetadata(),
        shared_image_interface_provider, delegate);
  }

  ~Canvas2DResourceProvider() override;

  gpu::SharedImageUsageSet GetSharedImageUsageFlags() const;
  bool unused_resources_reclaim_timer_is_running_for_testing() const;
  bool HasUnusedResourcesForTesting() const;
  bool IsSingleBuffered() const;

  bool IsAccelerated() const { return is_accelerated_; }
  bool IsSoftware() const { return is_software_; }
  void SetDelegate(CanvasResourceProviderDelegate* delegate) {
    delegate_ = delegate;
  }
  bool IsPrinting() const { return delegate_ && delegate_->IsPrinting(); }

  viz::SharedImageFormat GetSharedImageFormat() const { return format_; }
  const gfx::ColorSpace& GetColorSpace() const { return color_space_; }
  const gfx::HDRMetadata& GetHdrMetadata() const { return hdr_metadata_; }
  SkAlphaType GetAlphaType() const { return alpha_type_; }
  gfx::Size Size() const { return size_; }

  size_t max_recorded_op_bytes() const { return max_recorded_op_bytes_; }
  size_t max_pinned_image_bytes() const { return max_pinned_image_bytes_; }
  bool clear_frame() const { return clear_frame_; }

  int NumInflightResourcesForTesting() const { return num_inflight_resources_; }
  base::ByteSize EstimatedSizeInBytes() const;

  virtual scoped_refptr<CanvasResource> ProduceCanvasResource();
  void OnFlushForImage(cc::PaintImage::ContentId content_id);

  bool IsValid() const;
  virtual scoped_refptr<StaticBitmapImage> Snapshot(
      ImageOrientation = ImageOrientationEnum::kDefault);
  std::optional<cc::PaintRecord> Flush(FlushReason = FlushReason::kOther);
  void ReleaseImageProviderImages();
  const std::optional<cc::PaintRecord>& LastRecording();

  void SetAnimatedImageFrameIndexes(
      scoped_refptr<const cc::AnimatedImageFrameIndexMap>);
  virtual bool WritePixels(const SkImageInfo& orig_info,
                           const void* pixels,
                           size_t row_bytes,
                           int x,
                           int y);

  const MemoryManagedPaintRecorder& Recorder() const { return *recorder_; }
  MemoryManagedPaintRecorder& Recorder() { return *recorder_; }
  std::unique_ptr<MemoryManagedPaintRecorder> ReleaseRecorder();
  void SetRecorder(std::unique_ptr<MemoryManagedPaintRecorder> recorder);

  void SetResourceRecyclingEnabled(bool value);

  // Signals that the ongoing transfer of this resource to WebGPU has completed,
  // passing the token that should be waited on to ensure that the service-side
  // operations of the WebGPU write have completed. Ensures that the next read
  // of this resource (whether via raster or the compositor) waits on this
  // token.
  void TransferBackFromWebGPU(const gpu::SyncToken& webgpu_write_sync_token);

  void AlwaysEnableRasterTimersForTesting(bool value) {
    always_enable_raster_timers_for_testing_ = value;
  }
  virtual void RasterRecord(cc::PaintRecord last_recording);
  MemoryManagedPaintCanvas& GetCanvasForTesting();
  void RestoreBackBuffer(const cc::PaintImage&);

 protected:
  Canvas2DResourceProvider(gfx::Size,
                           viz::SharedImageFormat,
                           SkAlphaType,
                           const gfx::ColorSpace&,
                           const gfx::HDRMetadata&,
                           base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
                           bool is_accelerated,
                           gpu::SharedImageUsageSet shared_image_usage_flags,
                           CanvasResourceProviderDelegate*);
  Canvas2DResourceProvider(gfx::Size,
                           viz::SharedImageFormat,
                           SkAlphaType,
                           const gfx::ColorSpace&,
                           const gfx::HDRMetadata&,
                           WebGraphicsSharedImageInterfaceProvider*,
                           CanvasResourceProviderDelegate*);

  scoped_refptr<UnacceleratedStaticBitmapImage> UnacceleratedSnapshot(
      ImageOrientation);

 private:
  void ClearUnusedResources();
  bool IsGpuContextLost() const;

  // WebGraphicsContext3DProviderWrapper::DestructionObserver implementation.
  void OnContextDestroyed() override;
  void OnResourceRefReturned(
      scoped_refptr<CanvasResourceSharedImage>&& resource) override;
  void OnDestroyResource() override { --num_inflight_resources_; }
  // CanvasMemoryDumpClient implementation.
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd) override;
  size_t GetSize() const override;

  void EnsureWriteAccess();
  void EndWriteAccess();

  scoped_refptr<CanvasResourceSharedImage> NewOrRecycledResource();

  // MemoryManagedPaintRecorder::Client implementation.
  void InitializeForRecording(cc::PaintCanvas* canvas) const override;
  void RecordingCleared() override;

  void ApplyAnimatedImageFrameIndexesForId(SkCanvas* canvas, uint32_t id);

  SkSurface* GetSkSurface() const;
  CanvasImageProvider* GetOrCreateSWCanvasImageProvider();

 private:
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const {
    return context_provider_wrapper_;
  }

  // Should only be called from static Create*() methods.
  // TODO(crbug.com/352263194): Eliminate this method by inlining its body at
  // callsites.
  void ClearAtCreation();

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

  SkSurfaceProps GetSkSurfaceProps() const;
  virtual sk_sp<SkSurface> CreateSkSurface() const;
  gpu::raster::RasterInterface* RasterInterface() const;

  base::WeakPtr<Canvas2DResourceProvider> CreateWeakPtr();

  static void NotifyGpuContextLostTask(base::WeakPtr<Canvas2DResourceProvider>);

  // The maximum number of in-flight resources waiting to be used for
  // recycling.
  static constexpr int kMaxRecycledCanvasResources = 3;

  CanvasResourceSharedImage* resource() {
    return static_cast<CanvasResourceSharedImage*>(resource_.get());
  }
  const CanvasResourceSharedImage* resource() const {
    return static_cast<const CanvasResourceSharedImage*>(resource_.get());
  }

  std::unique_ptr<CanvasImageProvider> canvas_image_provider_;
  // If this instance is single-buffered or |resource_recycling_enabled_| is
  // false, |image_pool_| will not recycle resources.
  std::unique_ptr<gpu::SharedImagePool<CanvasResourceSharedImage>> image_pool_;

  scoped_refptr<CanvasResourceSharedImage> resource_;

  // Whether the content of the current resource must be transferred to a new
  // resource on CopyOnWrite. True by default, but can be set to false as an
  // optimization if the current resource is known to have been cleared.
  bool must_preserve_content_on_copy_on_write_ = true;

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
  raw_ptr<CanvasResourceProviderDelegate> delegate_ = nullptr;
  mutable sk_sp<SkSurface> surface_;
  std::unique_ptr<cc::SkiaPaintCanvas> skia_canvas_;
  const cc::PaintImage::Id snapshot_paint_image_id_;
  cc::PaintImage::ContentId snapshot_paint_image_content_id_ =
      cc::PaintImage::kInvalidContentId;
  uint32_t snapshot_sk_image_id_ = 0u;

  bool clear_frame_ = true;
  std::optional<cc::PaintRecord> last_recording_;
  bool always_enable_raster_timers_for_testing_ = false;

  base::WeakPtrFactory<Canvas2DResourceProvider> weak_ptr_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_2D_RESOURCE_PROVIDER_H_
