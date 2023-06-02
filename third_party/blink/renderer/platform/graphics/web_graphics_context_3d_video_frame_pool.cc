// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_video_frame_pool.h"

#include "base/feature_list.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "media/base/video_frame.h"
#include "media/renderers/video_frame_rgba_to_yuva_converter.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/renderable_gpu_memory_buffer_video_frame_pool.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

class Context : public media::RenderableGpuMemoryBufferVideoFramePool::Context {
 public:
  explicit Context(base::WeakPtr<blink::WebGraphicsContext3DProviderWrapper>
                       context_provider,
                   gpu::GpuMemoryBufferManager* gmb_manager)
      : weak_context_provider_(context_provider), gmb_manager_(gmb_manager) {}

  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override {
    return gmb_manager_
               ? gmb_manager_->CreateGpuMemoryBuffer(
                     size, format, usage, gpu::kNullSurfaceHandle, nullptr)
               : nullptr;
  }

  void CreateSharedImage(gfx::GpuMemoryBuffer* gpu_memory_buffer,
                         const viz::SharedImageFormat& si_format,
                         const gfx::ColorSpace& color_space,
                         GrSurfaceOrigin surface_origin,
                         SkAlphaType alpha_type,
                         uint32_t usage,
                         gpu::Mailbox& mailbox,
                         gpu::SyncToken& sync_token) override {
    auto* sii = SharedImageInterface();
    if (!sii) {
      return;
    }
    mailbox = sii->CreateSharedImage(
        si_format, gpu_memory_buffer->GetSize(), color_space, surface_origin,
        alpha_type, usage, "WebGraphicsContext3DVideoFramePool",
        gpu_memory_buffer->CloneHandle());
    sync_token = sii->GenVerifiedSyncToken();
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
    if (!sii || !gmb_manager_)
      return;
    mailbox = sii->CreateSharedImage(
        gpu_memory_buffer, gmb_manager_, plane, color_space, surface_origin,
        alpha_type, usage, "WebGraphicsContext2DVideoFramePool");
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

  base::WeakPtr<blink::WebGraphicsContext3DProviderWrapper>
      weak_context_provider_;
  gpu::GpuMemoryBufferManager* gmb_manager_;
};

}  // namespace

WebGraphicsContext3DVideoFramePool::WebGraphicsContext3DVideoFramePool(
    base::WeakPtr<blink::WebGraphicsContext3DProviderWrapper>
        weak_context_provider)
    : WebGraphicsContext3DVideoFramePool(
          std::move(weak_context_provider),
          Platform::Current()->GetGpuMemoryBufferManager()) {}

WebGraphicsContext3DVideoFramePool::WebGraphicsContext3DVideoFramePool(
    base::WeakPtr<blink::WebGraphicsContext3DProviderWrapper>
        weak_context_provider,
    gpu::GpuMemoryBufferManager* gmb_manager)
    : weak_context_provider_(weak_context_provider),
      pool_(media::RenderableGpuMemoryBufferVideoFramePool::Create(
          std::make_unique<Context>(weak_context_provider, gmb_manager))) {}

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
    viz::SharedImageFormat src_format,
    const gfx::Size& src_size,
    const gfx::ColorSpace& src_color_space,
    GrSurfaceOrigin src_surface_origin,
    const gpu::MailboxHolder& src_mailbox_holder,
    const gfx::ColorSpace& dst_color_space,
    FrameReadyCallback callback) {
  if (!weak_context_provider_)
    return false;
  auto* context_provider = weak_context_provider_->ContextProvider();
  if (!context_provider)
    return false;
  auto* raster_context_provider = context_provider->RasterContextProvider();
  if (!raster_context_provider)
    return false;

#if BUILDFLAG(IS_WIN)
  // CopyRGBATextureToVideoFrame below needs D3D shared images on Windows so
  // early out before creating the GMB since it's going to fail anyway.
  if (!context_provider->GetCapabilities().shared_image_d3d)
    return false;
#endif  // BUILDFLAG(IS_WIN)

  auto dst_frame = pool_->MaybeCreateVideoFrame(src_size, dst_color_space);
  if (!dst_frame)
    return false;

  auto* ri = raster_context_provider->RasterInterface();
  DCHECK(ri);
  unsigned query_id = 0;
  ri->GenQueriesEXT(1, &query_id);

#if BUILDFLAG(IS_WIN)
  // On Windows, GMB data read will do synchronization on its own.
  // No need for GL_COMMANDS_COMPLETED_CHROMIUM QueryEXT.
  GLenum queryTarget = GL_COMMANDS_ISSUED_CHROMIUM;
#else
  // QueryEXT functions are used to make sure that CopyRGBATextureToVideoFrame()
  // texture copy is complete before we access GMB data.
  GLenum queryTarget = GL_COMMANDS_COMPLETED_CHROMIUM;
#endif
  ri->BeginQueryEXT(queryTarget, query_id);

  const bool copy_succeeded = media::CopyRGBATextureToVideoFrame(
      raster_context_provider, src_format, src_size, src_color_space,
      src_surface_origin, src_mailbox_holder, dst_frame.get());
  if (!copy_succeeded) {
    ri->DeleteQueriesEXT(1, &query_id);
    return false;
  }

  ri->EndQueryEXT(queryTarget);

  auto on_query_done_cb =
      [](scoped_refptr<media::VideoFrame> frame,
         base::WeakPtr<blink::WebGraphicsContext3DProviderWrapper> ctx_wrapper,
         unsigned query_id, FrameReadyCallback callback) {
        if (ctx_wrapper) {
          if (auto* ctx_provider = ctx_wrapper->ContextProvider()) {
            if (auto* ri_provider = ctx_provider->RasterContextProvider()) {
              auto* ri = ri_provider->RasterInterface();
              ri->DeleteQueriesEXT(1, &query_id);
            }
          }
        }
        std::move(callback).Run(std::move(frame));
      };

  auto* context_support = raster_context_provider->ContextSupport();
  DCHECK(context_support);
  context_support->SignalQuery(
      query_id,
      base::BindOnce(on_query_done_cb, dst_frame, weak_context_provider_,
                     query_id, std::move(callback)));

  return true;
}

namespace {

void ApplyMetadataAndRunCallback(
    scoped_refptr<media::VideoFrame> src_video_frame,
    WebGraphicsContext3DVideoFramePool::FrameReadyCallback orig_callback,
    scoped_refptr<media::VideoFrame> converted_video_frame) {
  if (!converted_video_frame) {
    std::move(orig_callback).Run(nullptr);
    return;
  }
  // TODO(https://crbug.com/1302284): handle cropping before conversion
  auto wrapped_format = converted_video_frame->format();
  auto wrapped = media::VideoFrame::WrapVideoFrame(
      std::move(converted_video_frame), wrapped_format,
      src_video_frame->visible_rect(), src_video_frame->natural_size());
  wrapped->set_timestamp(src_video_frame->timestamp());
  // TODO(https://crbug.com/1302283): old metadata might not be applicable to
  // new frame
  wrapped->metadata().MergeMetadataFrom(src_video_frame->metadata());

  std::move(orig_callback).Run(std::move(wrapped));
}

BASE_FEATURE(kGpuMemoryBufferReadbackFromTexture,
             "GpuMemoryBufferReadbackFromTexture",
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
bool IsRK3399Board() {
  const std::string board = base::SysInfo::GetLsbReleaseBoard();
  const char* kRK3399Boards[] = {
      "bob",
      "kevin",
      "rainier",
      "scarlet",
  };
  for (const char* b : kRK3399Boards) {
    if (board.find(b) == 0u)  // if |board| starts with |b|.
      return true;
  }
  return false;
}
#endif  // BUILDFLAG(IS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
}  // namespace

bool WebGraphicsContext3DVideoFramePool::ConvertVideoFrame(
    scoped_refptr<media::VideoFrame> src_video_frame,
    const gfx::ColorSpace& dst_color_space,
    FrameReadyCallback callback) {
  auto format = src_video_frame->format();
  DCHECK(format == media::PIXEL_FORMAT_XBGR ||
         format == media::PIXEL_FORMAT_ABGR ||
         format == media::PIXEL_FORMAT_XRGB ||
         format == media::PIXEL_FORMAT_ARGB)
      << "Invalid format " << format;
  DCHECK_EQ(src_video_frame->NumTextures(), std::size_t{1});
  viz::SharedImageFormat texture_format;
  switch (format) {
    case media::PIXEL_FORMAT_XBGR:
      texture_format = viz::SinglePlaneFormat::kRGBX_8888;
      break;
    case media::PIXEL_FORMAT_ABGR:
      texture_format = viz::SinglePlaneFormat::kRGBA_8888;
      break;
    case media::PIXEL_FORMAT_XRGB:
      texture_format = viz::SinglePlaneFormat::kBGRX_8888;
      break;
    case media::PIXEL_FORMAT_ARGB:
      texture_format = viz::SinglePlaneFormat::kBGRA_8888;
      break;
    default:
      NOTREACHED();
      return false;
  }

  return CopyRGBATextureToVideoFrame(
      texture_format, src_video_frame->coded_size(),
      src_video_frame->ColorSpace(),
      src_video_frame->metadata().texture_origin_is_top_left
          ? kTopLeft_GrSurfaceOrigin
          : kBottomLeft_GrSurfaceOrigin,
      src_video_frame->mailbox_holder(0), dst_color_space,
      WTF::BindOnce(ApplyMetadataAndRunCallback, src_video_frame,
                    std::move(callback)));
}

// static
bool WebGraphicsContext3DVideoFramePool::
    IsGpuMemoryBufferReadbackFromTextureEnabled() {
#if BUILDFLAG(IS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
  // The GL driver used on RK3399 has a problem to enable One copy canvas
  // capture. See b/238144592.
  // TODO(b/239503724): Remove this code when RK3399 reaches EOL.
  if (IsRK3399Board())
    return false;
#endif  // BUILDFLAG(IS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)

  return base::FeatureList::IsEnabled(kGpuMemoryBufferReadbackFromTexture);
}

}  // namespace blink
