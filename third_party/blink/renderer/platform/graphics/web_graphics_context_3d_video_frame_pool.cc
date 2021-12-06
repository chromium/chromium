// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_video_frame_pool.h"

#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "media/renderers/video_frame_rgba_to_yuva_converter.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/renderable_gpu_memory_buffer_video_frame_pool.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"

namespace blink {

namespace {

class Context : public media::RenderableGpuMemoryBufferVideoFramePool::Context {
 public:
  explicit Context(base::WeakPtr<blink::WebGraphicsContext3DProviderWrapper>
                       context_provider)
      : weak_context_provider_(context_provider) {}

  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override {
    auto* gmb_manager = GpuMemoryBufferManager();
    return gmb_manager
               ? gmb_manager->CreateGpuMemoryBuffer(
                     size, format, usage, gpu::kNullSurfaceHandle, nullptr)
               : nullptr;
  }

  void CreateSharedImage(gfx::GpuMemoryBuffer* gpu_memory_buffer,
                         gfx::BufferPlane plane,
                         const gfx::ColorSpace& color_space,
                         GrSurfaceOrigin surface_origin,
                         SkAlphaType alpha_type,
                         uint32_t usage,
                         gpu::Mailbox& mailbox,
                         gpu::SyncToken& sync_token) override {
    auto* sii = SharedImageInterface();
    auto* gmb_manager = GpuMemoryBufferManager();
    if (!sii || !gmb_manager)
      return;
    mailbox =
        sii->CreateSharedImage(gpu_memory_buffer, gmb_manager, plane,
                               color_space, surface_origin, alpha_type, usage);
    sync_token = sii->GenVerifiedSyncToken();
  }

  void DestroySharedImage(const gpu::SyncToken& sync_token,
                          const gpu::Mailbox& mailbox) override {
    auto* sii = SharedImageInterface();
    if (!sii)
      return;
    sii->DestroySharedImage(sync_token, mailbox);
  }

 private:
  gpu::SharedImageInterface* SharedImageInterface() const {
    if (!weak_context_provider_)
      return nullptr;
    auto* context_provider = weak_context_provider_->ContextProvider();
    if (!context_provider)
      return nullptr;
    return context_provider->SharedImageInterface();
  }

  gpu::GpuMemoryBufferManager* GpuMemoryBufferManager() const {
    auto* gpu_factories = Platform::Current()->GetGpuFactories();
    return gpu_factories ? gpu_factories->GpuMemoryBufferManager() : nullptr;
  }

  base::WeakPtr<blink::WebGraphicsContext3DProviderWrapper>
      weak_context_provider_;
};

}  // namespace

WebGraphicsContext3DVideoFramePool::WebGraphicsContext3DVideoFramePool(
    base::WeakPtr<blink::WebGraphicsContext3DProviderWrapper>
        weak_context_provider)
    : weak_context_provider_(weak_context_provider),
      pool_(media::RenderableGpuMemoryBufferVideoFramePool::Create(
          std::make_unique<Context>(weak_context_provider))) {}

WebGraphicsContext3DVideoFramePool::~WebGraphicsContext3DVideoFramePool() =
    default;

gpu::raster::RasterInterface*
WebGraphicsContext3DVideoFramePool::GetRasterInterface() const {
  if (weak_context_provider_) {
    if (auto* context_provider = weak_context_provider_->ContextProvider()) {
      if (auto* raster_context_provider =
              context_provider->RasterContextProvider()) {
        return raster_context_provider->RasterInterface();
      }
    }
  }
  return nullptr;
}

bool WebGraphicsContext3DVideoFramePool::CopyRGBATextureToVideoFrame(
    viz::ResourceFormat src_format,
    const gfx::Size& src_size,
    const gfx::ColorSpace& src_color_space,
    GrSurfaceOrigin src_surface_origin,
    const gpu::MailboxHolder& src_mailbox_holder,
    const gfx::ColorSpace& dst_color_space,
    FrameReadyCallback callback) {
  // Issue `callback` with a nullptr VideoFrame if we return early.
  base::ScopedClosureRunner failure_runner(WTF::Bind(
      [](FrameReadyCallback* callback) { std::move(*callback).Run(nullptr); },
      base::Unretained(&callback)));

  if (!weak_context_provider_)
    return false;
  auto* context_provider = weak_context_provider_->ContextProvider();
  if (!context_provider)
    return false;
  auto* raster_context_provider = context_provider->RasterContextProvider();
  if (!raster_context_provider)
    return false;

  scoped_refptr<media::VideoFrame> dst_frame =
      pool_->MaybeCreateVideoFrame(src_size, dst_color_space);
  if (!dst_frame)
    return false;

  gpu::SyncToken copy_done_sync_token;
  const bool copy_succeeded = media::CopyRGBATextureToVideoFrame(
      raster_context_provider, src_format, src_size, src_color_space,
      src_surface_origin, src_mailbox_holder, dst_frame.get(),
      copy_done_sync_token);
  if (!copy_succeeded)
    return false;

  IgnoreResult(failure_runner.Release());
  raster_context_provider->ContextSupport()->SignalSyncToken(
      copy_done_sync_token, base::BindOnce(std::move(callback), dst_frame));
  return true;
}

}  // namespace blink
