// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_NON_2D_RESOURCE_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_NON_2D_RESOURCE_PROVIDER_H_

#include <memory>

#include "base/byte_size.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "cc/paint/paint_image.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_pool.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "third_party/blink/public/platform/web_graphics_shared_image_interface_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_color_params.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/canvas_snapshot_info.h"
#include "third_party/blink/renderer/platform/graphics/flush_for_image_listener.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/instrumentation/canvas_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {
class AnimatedImageFrameIndexMap;
class PaintCanvas;
class SkiaPaintCanvas;
}  // namespace cc

namespace gfx {
class ColorSpace;
struct HDRMetadata;
class Size;
}  // namespace gfx

namespace gpu {
class RasterScopedAccess;
namespace raster {
class RasterInterface;
}  // namespace raster
}  // namespace gpu

namespace blink {

class CanvasImageProvider;
class CanvasResourceProviderDelegate;
class WebGraphicsSharedImageInterfaceProvider;

class PLATFORM_EXPORT CanvasNon2DResourceProvider
    : public CanvasMemoryDumpClient,
      public MemoryManagedPaintRecorder::Client,
      public CanvasResourceSharedImage::Client,
      public FlushForImageObserver,
      public WebGraphicsContext3DProviderWrapper::DestructionObserver,
      public viz::ContextLostObserver,
      public BitmapGpuChannelLostObserver {
 public:
  static std::unique_ptr<CanvasNon2DResourceProvider> Create(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      const gfx::HDRMetadata& hdr_metadata,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      gpu::SharedImageUsageSet shared_image_usage_flags,
      CanvasResourceProviderDelegate* delegate = nullptr);
  static std::unique_ptr<CanvasNon2DResourceProvider> Create(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper,
      gpu::SharedImageUsageSet shared_image_usage_flags,
      CanvasResourceProviderDelegate* delegate = nullptr) {
    return Create(size, format, alpha_type, color_space, gfx::HDRMetadata(),
                  context_provider_wrapper, shared_image_usage_flags, delegate);
  }

  static std::unique_ptr<CanvasNon2DResourceProvider> Create(
      gfx::Size size,
      const Canvas2DColorParams& color_params,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      gpu::SharedImageUsageSet shared_image_usage_flags);

  static std::unique_ptr<CanvasNon2DResourceProvider> CreateForWebGPU(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      const gfx::HDRMetadata& hdr_metadata,
      gpu::SharedImageUsageSet shared_image_usage_flags = {},
      CanvasResourceProviderDelegate* delegate = nullptr);
  static std::unique_ptr<CanvasNon2DResourceProvider> CreateForWebGPU(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      gpu::SharedImageUsageSet shared_image_usage_flags = {},
      CanvasResourceProviderDelegate* delegate = nullptr) {
    return CreateForWebGPU(size, format, alpha_type, color_space,
                           gfx::HDRMetadata(), shared_image_usage_flags,
                           delegate);
  }

  static std::unique_ptr<CanvasNon2DResourceProvider>
  CreateForSoftwareCompositor(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      const gfx::HDRMetadata& hdr_metadata,
      WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider,
      CanvasResourceProviderDelegate* delegate = nullptr);
  static std::unique_ptr<CanvasNon2DResourceProvider>
  CreateForSoftwareCompositor(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider,
      CanvasResourceProviderDelegate* delegate = nullptr) {
    return CreateForSoftwareCompositor(
        size, format, alpha_type, color_space, gfx::HDRMetadata(),
        shared_image_interface_provider, delegate);
  }

  static std::unique_ptr<CanvasNon2DResourceProvider>
  CreateForSoftwareCompositor(
      gfx::Size size,
      const Canvas2DColorParams& color_params,
      WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider);

  ~CanvasNon2DResourceProvider() override;

  gpu::SharedImageUsageSet GetSharedImageUsageFlags() const;
  bool IsSingleBuffered() const;

  bool IsSoftware() const { return is_software_; }

  CanvasImageProvider* GetOrCreateImageProvider();
  void SetAnimatedImageFrameIndexes(
      scoped_refptr<const cc::AnimatedImageFrameIndexMap>);

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

  base::ByteSize EstimatedSizeInBytes() const;

  scoped_refptr<CanvasResource> ProduceCanvasResource();

  bool IsValid() const;
  scoped_refptr<StaticBitmapImage> Snapshot(
      ImageOrientation = ImageOrientationEnum::kDefault);

  scoped_refptr<CanvasResource> DoExternalOverdrawAndProduceResource(
      base::FunctionRef<void(cc::PaintCanvas&)> draw_callback);

  scoped_refptr<StaticBitmapImage> DoExternalOverdrawAndSnapshot(
      base::FunctionRef<void(cc::PaintCanvas&)> draw_callback,
      ImageOrientation orientation);

  // Returns the ClientSharedImage backing this CanvasNon2DResourceProvider, if
  // one exists, after flushing the resource and signaling that an external
  // write will occur on it. The caller should wait on
  // `internal_access_sync_token` before writing the contents. When the external
  // write is complete, the caller should call `EndExternalWrite()`.
  scoped_refptr<gpu::ClientSharedImage> BeginExternalOverwrite(
      gpu::SyncToken& internal_access_sync_token);

  // Signals that an external write has completed, passing the token that should
  // be waited on to ensure that the service-side operations of the external
  // write have completed. Ensures that the next read of this resource (whether
  // via raster or the compositor) waits on this token.
  void EndExternalWrite(const gpu::SyncToken& external_write_sync_token);

  gpu::raster::RasterInterface* RasterInterface() const;

  CanvasResourceSharedImage* resource() {
    return static_cast<CanvasResourceSharedImage*>(resource_.get());
  }
  const CanvasResourceSharedImage* resource() const {
    return static_cast<const CanvasResourceSharedImage*>(resource_.get());
  }

 private:
  CanvasNon2DResourceProvider(
      gfx::Size,
      viz::SharedImageFormat,
      SkAlphaType,
      const gfx::ColorSpace&,
      const gfx::HDRMetadata&,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      gpu::SharedImageUsageSet shared_image_usage_flags,
      CanvasResourceProviderDelegate*);
  CanvasNon2DResourceProvider(gfx::Size,
                              viz::SharedImageFormat,
                              SkAlphaType,
                              const gfx::ColorSpace&,
                              const gfx::HDRMetadata&,
                              WebGraphicsSharedImageInterfaceProvider*,
                              CanvasResourceProviderDelegate*);

  void ClearUnusedResources();
  bool IsGpuContextLost() const;

  SkSurface* GetSkSurface() const;

  // WebGraphicsContext3DProviderWrapper::DestructionObserver implementation.
  void OnContextDestroyed() override;

  void OnResourceRefReturned(
      scoped_refptr<CanvasResourceSharedImage>&& resource) override;
  void OnDestroyResource() override { --num_inflight_resources_; }
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd) override;
  size_t GetSize() const override;

  // MemoryManagedPaintRecorder::Client implementation.
  void RecordingCleared() override;
  void InitializeForRecording(cc::PaintCanvas* canvas) const override;

  SkSurfaceProps GetSkSurfaceProps() const;

  void EnsureWriteAccess();
  void EndWriteAccess();

  scoped_refptr<CanvasResourceSharedImage> NewOrRecycledResource();

  // FlushForImageObserver implementation:
  void OnFlushForImage(cc::PaintImage::ContentId content_id) override;

  sk_sp<SkSurface> CreateSkSurface() const;

  base::WeakPtr<CanvasNon2DResourceProvider> CreateWeakPtr();

  static void NotifyGpuContextLostTask(
      base::WeakPtr<CanvasNon2DResourceProvider>);

  // The maximum number of in-flight resources waiting to be used for
  // recycling.
  static constexpr int kMaxRecycledCanvasResources = 3;

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
  const raw_ptr<CanvasResourceProviderDelegate> delegate_;

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

  base::WeakPtrFactory<CanvasNon2DResourceProvider> weak_ptr_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_NON_2D_RESOURCE_PROVIDER_H_
