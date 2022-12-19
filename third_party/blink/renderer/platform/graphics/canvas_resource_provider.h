// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_PROVIDER_H_

#include "base/notreached.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/raster/playback_image_provider.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_host.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/instrumentation/canvas_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkSurface.h"

class GrDirectContext;

namespace cc {
class ImageDecodeCache;
class PaintCanvas;
}  // namespace cc

namespace gpu {
namespace gles2 {

class GLES2Interface;

}  // namespace gles2

namespace raster {

class RasterInterface;

}  // namespace raster
}  // namespace gpu

namespace blink {

class CanvasResourceDispatcher;
class WebGraphicsContext3DProviderWrapper;

// CanvasResourceProvider
//==============================================================================
//
// This is an abstract base class that encapsulates a drawable graphics
// resource.  Subclasses manage specific resource types (Gpu Textures,
// GpuMemoryBuffer, Bitmap in RAM). CanvasResourceProvider serves as an
// abstraction layer for these resource types. It is designed to serve
// the needs of Canvas2DLayerBridge, but can also be used as a general purpose
// provider of drawable surfaces for 2D rendering with skia.
//
// General usage:
//   1) Use the Create() static method to create an instance
//   2) use Canvas() to get a drawing interface
//   3) Call Snapshot() to acquire a bitmap with the rendered image in it.

class PLATFORM_EXPORT CanvasResourceProvider
    : public WebGraphicsContext3DProviderWrapper::DestructionObserver,
      public base::CheckedObserver,
      public CanvasMemoryDumpClient,
      public MemoryManagedPaintCanvas::Client {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  enum ResourceProviderType {
    kTexture [[deprecated]] = 0,
    kBitmap = 1,
    kSharedBitmap = 2,
    kTextureGpuMemoryBuffer [[deprecated]] = 3,
    kBitmapGpuMemoryBuffer [[deprecated]] = 4,
    kSharedImage = 5,
    kDirectGpuMemoryBuffer [[deprecated]] = 6,
    kPassThrough = 7,
    kSwapChain = 8,
    kSkiaDawnSharedImage [[deprecated]] = 9,
    kMaxValue = kSkiaDawnSharedImage,
  };
#pragma GCC diagnostic pop

  // The following parameters attempt to reach a compromise between not flushing
  // too often, and not accumulating an unreasonable backlog.  Flushing too
  // often will hurt performance due to overhead costs. Accumulating large
  // backlogs, in the case of OOPR-Canvas, results in poor parellelism and
  // janky UI. With OOPR-Canvas disabled, it is still desirable to flush
  // periodically to guard against run-away memory consumption caused by
  // PaintOpBuffers that grow indefinitely. The OOPr-related jank is caused by
  // long-running RasterCHROMIUM calls that monopolize the main thread
  // of the GPU process.  By flushing periodically, we allow the rasterization
  // of canvas contents to be interleaved with other compositing and UI work.
  static constexpr size_t kMaxRecordedOpBytes = 4 * 1024 * 1024;
  // The same value as is used in content::WebGraphicsConext3DProviderImpl.
  static constexpr uint64_t kDefaultMaxPinnedImageBytes = 64 * 1024 * 1024;

  using RestoreMatrixClipStackCb =
      base::RepeatingCallback<void(cc::PaintCanvas*)>;

  // TODO(juanmihd@ bug/1078518) Check whether FilterQuality is needed in all
  // these Create methods below, or just call setFilterQuality explicitly.

  // Used to determine if the provider is going to be initialized or not,
  // ignored by PassThrough
  enum class ShouldInitialize { kNo, kCallClear };

  static std::unique_ptr<CanvasResourceProvider> CreateBitmapProvider(
      const SkImageInfo& info,
      cc::PaintFlags::FilterQuality filter_quality,
      ShouldInitialize initialize_provider);

  static std::unique_ptr<CanvasResourceProvider> CreateSharedBitmapProvider(
      const SkImageInfo& info,
      cc::PaintFlags::FilterQuality filter_quality,
      ShouldInitialize initialize_provider,
      base::WeakPtr<CanvasResourceDispatcher>);

  static std::unique_ptr<CanvasResourceProvider> CreateSharedImageProvider(
      const SkImageInfo& info,
      cc::PaintFlags::FilterQuality filter_quality,
      ShouldInitialize initialize_provider,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      RasterMode raster_mode,
      bool is_origin_top_left,
      uint32_t shared_image_usage_flags);

  static std::unique_ptr<CanvasResourceProvider> CreateWebGPUImageProvider(
      const SkImageInfo& info,
      bool is_origin_top_left,
      uint32_t shared_image_usage_flags = 0);

  static std::unique_ptr<CanvasResourceProvider> CreatePassThroughProvider(
      const SkImageInfo& info,
      cc::PaintFlags::FilterQuality filter_quality,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::WeakPtr<CanvasResourceDispatcher>,
      bool is_origin_top_left);

  static std::unique_ptr<CanvasResourceProvider> CreateSwapChainProvider(
      const SkImageInfo& info,
      cc::PaintFlags::FilterQuality filter_quality,
      ShouldInitialize initialize_provider,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::WeakPtr<CanvasResourceDispatcher>,
      bool is_origin_top_left);

  // Use Snapshot() for capturing a frame that is intended to be displayed via
  // the compositor. Cases that are destined to be transferred via a
  // TransferableResource should call ProduceCanvasResource() instead.
  // The ImageOrientationEnum conveys the desired orientation of the image, and
  // should be derived from the source of the bitmap data.
  virtual scoped_refptr<CanvasResource> ProduceCanvasResource() = 0;
  virtual scoped_refptr<StaticBitmapImage> Snapshot(
      const ImageOrientation& = ImageOrientationEnum::kDefault) = 0;

  void SetCanvasResourceHost(CanvasResourceHost* resource_host) {
    resource_host_ = resource_host;
  }

  static void SetMaxPinnedImageBytesForTesting(size_t value) {
    max_pinned_image_bytes_ = value;
  }
  static void ResetMaxPinnedImageBytesForTesting() {
    max_pinned_image_bytes_ = kDefaultMaxPinnedImageBytes;
  }

  // WebGraphicsContext3DProvider::DestructionObserver implementation.
  void OnContextDestroyed() override;

  cc::PaintCanvas* Canvas(bool needs_will_draw = false);
  void ReleaseLockedImages();
  // FlushCanvas and do not preserve recordings.
  void FlushCanvas();
  // FlushCanvas and preserve recordings.
  absl::optional<cc::PaintRecord> FlushCanvasAndMaybePreserveRecording(
      bool printing);
  const SkImageInfo& GetSkImageInfo() const { return info_; }
  SkSurfaceProps GetSkSurfaceProps() const;
  gfx::ColorSpace GetColorSpace() const;
  void SetFilterQuality(cc::PaintFlags::FilterQuality quality) {
    filter_quality_ = quality;
  }
  gfx::Size Size() const;
  bool IsOriginTopLeft() const { return is_origin_top_left_; }
  virtual bool IsValid() const = 0;
  virtual bool IsAccelerated() const = 0;
  // Returns true if the resource can be used by the display compositor.
  virtual bool SupportsDirectCompositing() const = 0;
  virtual bool SupportsSingleBuffering() const { return false; }
  uint32_t ContentUniqueID() const;
  CanvasResourceDispatcher* ResourceDispatcher() {
    return resource_dispatcher_.get();
  }

  // Indicates that the compositing path is single buffered, meaning that
  // ProduceCanvasResource() return a reference to the same resource each time,
  // which implies that Producing an animation frame may overwrite the resource
  // used by the previous frame. This results in graphics updates skipping the
  // queue, thus reducing latency, but with the possible side effects of tearing
  // (in cases where the resource is scanned out directly) and irregular frame
  // rate.
  bool IsSingleBuffered() const { return is_single_buffered_; }

  // Attempt to enable single buffering mode on this resource provider.  May
  // fail if the CanvasResourcePRovider subclass does not support this mode of
  // operation.
  void TryEnableSingleBuffering();

  // Only works in single buffering mode.
  bool ImportResource(scoped_refptr<CanvasResource>&&);

  void RecycleResource(scoped_refptr<CanvasResource>&&);
  void SetResourceRecyclingEnabled(bool);
  void ClearRecycledResources();
  scoped_refptr<CanvasResource> NewOrRecycledResource();

  SkSurface* GetSkSurface() const;
  bool IsGpuContextLost() const;
  virtual bool WritePixels(const SkImageInfo& orig_info,
                           const void* pixels,
                           size_t row_bytes,
                           int x,
                           int y);

  virtual gpu::Mailbox GetBackingMailboxForOverwrite(
      MailboxSyncMode sync_mode) {
    NOTREACHED();
    return gpu::Mailbox();
  }
  virtual GLenum GetBackingTextureTarget() const { return GL_TEXTURE_2D; }
  virtual void* GetPixelBufferAddressForOverwrite() {
    NOTREACHED();
    return nullptr;
  }
  virtual uint32_t GetSharedImageUsageFlags() const {
    NOTREACHED();
    return 0;
  }

  CanvasResourceProvider(const CanvasResourceProvider&) = delete;
  CanvasResourceProvider& operator=(const CanvasResourceProvider&) = delete;
  ~CanvasResourceProvider() override;

  base::WeakPtr<CanvasResourceProvider> CreateWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Notifies the provider when the texture params associated with |resource|
  // are modified externally from the provider's SkSurface.
  virtual void NotifyTexParamsModified(const CanvasResource* resource) {}

  size_t cached_resources_count_for_testing() const {
    return canvas_resources_.size();
  }

  void SkipQueuedDrawCommands();
  void SetRestoreClipStackCallback(RestoreMatrixClipStackCb);
  void RestoreBackBuffer(const cc::PaintImage&);

  ResourceProviderType GetType() const { return type_; }
  bool HasRecordedDrawOps() const;

  void OnDestroyResource();

  virtual void OnAcquireRecyclableCanvasResource() {}
  virtual void OnDestroyRecyclableCanvasResource(
      const gpu::SyncToken& sync_token) {}

  void FlushIfRecordingLimitExceeded();

  size_t TotalOpCount() const { return recorder_.TotalOpCount(); }
  size_t TotalOpBytesUsed() const { return recorder_.OpBytesUsed(); }
  size_t TotalPinnedImageBytes() const { return total_pinned_image_bytes_; }

  void DidPinImage(size_t bytes) override;

  bool IsPrinting() { return resource_host_ && resource_host_->IsPrinting(); }

  void ClearFrame() { clear_frame_ = true; }

  static void NotifyWillTransfer(cc::PaintImage::ContentId content_id);

 protected:
  class CanvasImageProvider;

  gpu::gles2::GLES2Interface* ContextGL() const;
  gpu::raster::RasterInterface* RasterInterface() const;
  GrDirectContext* GetGrContext() const;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const {
    return context_provider_wrapper_;
  }
  GrSurfaceOrigin GetGrSurfaceOrigin() const {
    return is_origin_top_left_ ? kTopLeft_GrSurfaceOrigin
                               : kBottomLeft_GrSurfaceOrigin;
  }
  cc::PaintFlags::FilterQuality FilterQuality() const {
    return filter_quality_;
  }

  scoped_refptr<StaticBitmapImage> SnapshotInternal(const ImageOrientation&);
  scoped_refptr<CanvasResource> GetImportedResource() const;
  absl::optional<cc::PaintRecord> FlushCanvasInternal(bool preserve_recording);

  CanvasResourceProvider(const ResourceProviderType&,
                         const SkImageInfo&,
                         cc::PaintFlags::FilterQuality,
                         bool is_origin_top_left,
                         base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
                         base::WeakPtr<CanvasResourceDispatcher>);

  // Its important to use this method for generating PaintImage wrapped canvas
  // snapshots to get a cache hit from cc's ImageDecodeCache. This method
  // ensures that the PaintImage ID for the snapshot, used for keying
  // decodes/uploads in the cache is invalidated only when the canvas contents
  // change.
  cc::PaintImage MakeImageSnapshot();
  virtual void RasterRecord(cc::PaintRecord);
  void RasterRecordOOP(cc::PaintRecord last_recording,
                       bool needs_clear,
                       gpu::Mailbox mailbox);

  CanvasImageProvider* GetOrCreateCanvasImageProvider();
  void TearDownSkSurface();

  // Will only notify a will draw if its needed. This is initially done for the
  // CanvasResourceProviderSharedImage use case.
  virtual void WillDrawIfNeeded() {}

  ResourceProviderType type_;
  mutable sk_sp<SkSurface> surface_;  // mutable for lazy init
  SkSurface::ContentChangeMode mode_ = SkSurface::kRetain_ContentChangeMode;

  virtual void OnFlushForImage(cc::PaintImage::ContentId content_id);
  void OnMemoryDump(base::trace_event::ProcessMemoryDump*) override;

 private:
  friend class FlushForImageListener;
  virtual sk_sp<SkSurface> CreateSkSurface() const = 0;
  virtual scoped_refptr<CanvasResource> CreateResource();
  virtual bool UseOopRasterization() { return false; }
  bool UseHardwareDecodeCache() const {
    return IsAccelerated() && context_provider_wrapper_;
  }
  // Notifies before any drawing will be done on the resource used by this
  // provider.
  virtual void WillDraw() {}

  size_t ComputeSurfaceSize() const;
  size_t GetSize() const override;

  cc::ImageDecodeCache* ImageDecodeCacheRGBA8();
  cc::ImageDecodeCache* ImageDecodeCacheF16();
  void EnsureSkiaCanvas();

  void Clear();

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher_;
  // Note that `info_` should be const, but the relevant SkImageInfo
  // constructors do not exist.
  SkImageInfo info_;
  cc::PaintFlags::FilterQuality filter_quality_;
  const bool is_origin_top_left_;
  std::unique_ptr<CanvasImageProvider> canvas_image_provider_;
  std::unique_ptr<cc::SkiaPaintCanvas> skia_canvas_;
  MemoryManagedPaintRecorder recorder_{this};

  size_t total_pinned_image_bytes_ = 0;

  const cc::PaintImage::Id snapshot_paint_image_id_;
  cc::PaintImage::ContentId snapshot_paint_image_content_id_ =
      cc::PaintImage::kInvalidContentId;
  uint32_t snapshot_sk_image_id_ = 0u;

  // When and if |resource_recycling_enabled_| is false, |canvas_resources_|
  // will only hold one CanvasResource at most.
  WTF::Vector<scoped_refptr<CanvasResource>> canvas_resources_;
  bool resource_recycling_enabled_ = true;
  bool is_single_buffered_ = false;
  bool oopr_uses_dmsaa_ = false;

  // The maximum number of in-flight resources waiting to be used for
  // recycling.
  static constexpr int kMaxRecycledCanvasResources = 2;
  // The maximum number of draw ops executed on the canvas, after which the
  // underlying GrContext is flushed.
  // Note: This parameter does not affect the flushing of recorded PaintOps.
  // See kMaxRecordedOpBytes above.
  static constexpr int kMaxDrawsBeforeContextFlush = 50;

  int num_inflight_resources_ = 0;
  int max_inflight_resources_ = 0;

  RestoreMatrixClipStackCb restore_clip_stack_callback_;

  CanvasResourceHost* resource_host_;

  bool clear_frame_ = true;
  static size_t max_pinned_image_bytes_;

  base::WeakPtrFactory<CanvasResourceProvider> weak_ptr_factory_{this};
};

ALWAYS_INLINE void CanvasResourceProvider::FlushIfRecordingLimitExceeded() {
  // When printing we avoid flushing if it is still possible to print in
  // vector mode.
  if (IsPrinting() && clear_frame_)
    return;
  if (TotalOpBytesUsed() > kMaxRecordedOpBytes ||
      total_pinned_image_bytes_ > max_pinned_image_bytes_) {
    FlushCanvas();
  }
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_PROVIDER_H_
