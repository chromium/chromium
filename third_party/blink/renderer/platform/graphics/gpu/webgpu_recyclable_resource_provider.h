// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_RECYCLABLE_RESOURCE_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_RECYCLABLE_RESOURCE_PROVIDER_H_

#include <memory>

#include "base/byte_size.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "cc/paint/paint_image.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "third_party/blink/public/platform/web_graphics_shared_image_interface_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_color_params.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/instrumentation/canvas_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {
class PaintCanvas;
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

class PLATFORM_EXPORT WebGpuRecyclableResourceProvider
    : public CanvasMemoryDumpClient,
      public WebGraphicsContext3DProviderWrapper::DestructionObserver {
 public:
  static std::unique_ptr<WebGpuRecyclableResourceProvider> Create(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      const gfx::HDRMetadata& hdr_metadata);
  ~WebGpuRecyclableResourceProvider() override;

  gfx::Size Size() const { return size_; }
  viz::SharedImageFormat GetSharedImageFormat() const { return format_; }
  const gfx::ColorSpace& GetColorSpace() const { return color_space_; }
  SkAlphaType GetAlphaType() const { return alpha_type_; }

  scoped_refptr<gpu::ClientSharedImage> GetSharedImage() const;
  gpu::SyncToken GetSyncToken() const;


  // NOTE: Can only be used if this instance is accelerated.
  bool UploadToBackingSharedImage(const SkPixmap& pixmap,
                                  uint32_t src_x,
                                  uint32_t src_y);

  void DoExternalOverdraw(
      base::FunctionRef<void(cc::PaintCanvas&)> draw_callback);

  // This is a workaround to ensure WaitSyncToken() is still called even when
  // copying is effectively skipped due to a dummy WebGPU texture.
  void PrepareForWebGPUDummyMailbox();

  // Returns the ClientSharedImage backing this
  // WebGpuRecyclableResourceProvider, if one exists, after flushing the
  // resource and signaling that an external write will occur on it. The caller
  // should wait on `internal_access_sync_token` before writing the contents.
  // When the external write is complete, the caller should call
  // `EndExternalWrite()`.
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
  void WaitSyncToken(const gpu::SyncToken& sync_token);

 private:
  WebGpuRecyclableResourceProvider(
      gfx::Size,
      viz::SharedImageFormat,
      SkAlphaType,
      const gfx::ColorSpace&,
      const gfx::HDRMetadata&,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>);

  bool IsGpuContextLost() const;
  CanvasImageProvider* GetOrCreateImageProvider();

  // WebGraphicsContext3DProviderWrapper::DestructionObserver implementation.
  void OnContextDestroyed() override;

  // CanvasMemoryDumpClient implementation.
  base::ByteSize EstimatedSizeInBytes() const;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd) override;
  size_t GetSize() const override;



  gpu::raster::RasterInterface* RasterInterface() const;

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const {
    return context_provider_wrapper_;
  }



  std::unique_ptr<gpu::RasterScopedAccess> WillDrawInternal();

  const gfx::Size size_;
  const viz::SharedImageFormat format_;
  const SkAlphaType alpha_type_;
  const gfx::ColorSpace color_space_;
  const gfx::HDRMetadata hdr_metadata_;

  std::unique_ptr<CanvasImageProvider> canvas_image_provider_;
  std::unique_ptr<MemoryManagedPaintRecorder> recorder_for_external_draws_;

  scoped_refptr<gpu::ClientSharedImage> shared_image_;
  gpu::SyncToken acquire_sync_token_;
  gpu::SyncToken release_sync_token_;

  bool is_cleared_ = false;

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_RECYCLABLE_RESOURCE_PROVIDER_H_
