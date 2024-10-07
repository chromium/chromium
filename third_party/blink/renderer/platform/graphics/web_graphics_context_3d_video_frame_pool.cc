// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_video_frame_pool.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/trace_event/trace_event_impl.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "media/base/simple_sync_token_client.h"
#include "media/base/video_frame.h"
#include "media/renderers/video_frame_rgba_to_yuva_converter.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/renderable_gpu_memory_buffer_video_frame_pool.h"
#include "perfetto/tracing/track_event_args.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace blink {

namespace {

BASE_FEATURE(kUseCopyToGpuMemoryBufferAsync,
             "UseCopyToGpuMemoryBufferAsync",
#if BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

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

  scoped_refptr<gpu::ClientSharedImage> CreateSharedImage(
      gfx::GpuMemoryBuffer* gpu_memory_buffer,
      const viz::SharedImageFormat& si_format,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      gpu::SharedImageUsageSet usage,
      gpu::SyncToken& sync_token) override {
    auto* sii = SharedImageInterface();
    if (!sii) {
      return nullptr;
    }
    auto client_shared_image = sii->CreateSharedImage(
        {si_format, gpu_memory_buffer->GetSize(), color_space, surface_origin,
         alpha_type, usage, "WebGraphicsContext3DVideoFramePool"},
        gpu_memory_buffer->CloneHandle());
    CHECK(client_shared_image);
    sync_token = sii->GenVerifiedSyncToken();
    return client_shared_image;
  }

  scoped_refptr<gpu::ClientSharedImage> CreateSharedImage(
      const gfx::Size& size,
      gfx::BufferUsage buffer_usage,
      const viz::SharedImageFormat& si_format,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      gpu::SharedImageUsageSet usage,
      gpu::SyncToken& sync_token) override {
    auto* sii = SharedImageInterface();
    if (!sii) {
      return nullptr;
    }
    auto client_shared_image = sii->CreateSharedImage(
        {si_format, size, color_space, surface_origin, alpha_type, usage,
         "WebGraphicsContext3DVideoFramePool"},
        gpu::kNullSurfaceHandle, buffer_usage);
    if (!client_shared_image) {
      return nullptr;
    }
#if BUILDFLAG(IS_MAC)
    client_shared_image->SetColorSpaceOnNativeBuffer(color_space);
#endif
    sync_token = sii->GenVerifiedSyncToken();
    return client_shared_image;
  }

  void DestroySharedImage(const gpu::SyncToken& sync_token,
                          scoped_refptr<gpu::ClientSharedImage> shared_image,
                          const bool is_mappable_si_enabled) override {
    auto* sii = SharedImageInterface();
    if (!sii)
      return;
    CHECK(shared_image);
    if (is_mappable_si_enabled) {
      shared_image->UpdateDestructionSyncToken(sync_token);
    } else {
      sii->DestroySharedImage(sync_token, std::move(shared_image));
    }
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
  raw_ptr<gpu::GpuMemoryBufferManager> gmb_manager_;
};

}  // namespace

WebGraphicsContext3DVideoFramePool::WebGraphicsContext3DVideoFramePool(
    base::WeakPtr<blink::WebGraphicsContext3DProviderWrapper>
        weak_context_provider)
    : WebGraphicsContext3DVideoFramePool(
          std::move(weak_context_provider),
          SharedGpuContext::GetGpuMemoryBufferManager()) {}

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

namespace {
void SignalGpuCompletion(
    base::WeakPtr<blink::WebGraphicsContext3DProviderWrapper> ctx_wrapper,
    GLenum query_target,
    base::OnceClosure callback) {
  DCHECK(ctx_wrapper);
  auto* context_provider = ctx_wrapper->ContextProvider();
  DCHECK(context_provider);
  auto* raster_context_provider = context_provider->RasterContextProvider();
  DCHECK(raster_context_provider);
  auto* ri = raster_context_provider->RasterInterface();
  DCHECK(ri);

  unsigned query_id = 0;
  ri->GenQueriesEXT(1, &query_id);
  ri->BeginQueryEXT(query_target, query_id);
  ri->EndQueryEXT(query_target);

  auto on_query_done_lambda =
      [](base::WeakPtr<blink::WebGraphicsContext3DProviderWrapper> ctx_wrapper,
         unsigned query_id, base::OnceClosure wrapped_callback) {
        if (ctx_wrapper) {
          if (auto* ctx_provider = ctx_wrapper->ContextProvider()) {
            if (auto* ri_provider = ctx_provider->RasterContextProvider()) {
              auto* ri = ri_provider->RasterInterface();
              ri->DeleteQueriesEXT(1, &query_id);
            }
          }
        }
        std::move(wrapped_callback).Run();
      };

  auto* context_support = raster_context_provider->ContextSupport();
  DCHECK(context_support);
  context_support->SignalQuery(
      query_id, base::BindOnce(on_query_done_lambda, std::move(ctx_wrapper),
                               query_id, std::move(callback)));
}

void CopyToGpuMemoryBuffer(
    base::WeakPtr<blink::WebGraphicsContext3DProviderWrapper> ctx_wrapper,
    media::VideoFrame* dst_frame,
    base::OnceClosure callback) {
  CHECK(dst_frame->HasMappableGpuBuffer());
  CHECK(!dst_frame->HasNativeGpuMemoryBuffer());
  CHECK_EQ(dst_frame->shared_image_format_type(),
           media::SharedImageFormatType::kSharedImageFormat);
  CHECK(dst_frame->HasSharedImage());

  DCHECK(ctx_wrapper);
  auto* context_provider = ctx_wrapper->ContextProvider();
  DCHECK(context_provider);
  auto* raster_context_provider = context_provider->RasterContextProvider();
  DCHECK(raster_context_provider);
  auto* ri = raster_context_provider->RasterInterface();
  DCHECK(ri);

  gpu::SyncToken blit_done_sync_token;
  ri->GenUnverifiedSyncTokenCHROMIUM(blit_done_sync_token.GetData());

  auto* sii = context_provider->SharedImageInterface();
  DCHECK(sii);

  const bool use_async_copy =
      base::FeatureList::IsEnabled(kUseCopyToGpuMemoryBufferAsync);
  const auto mailbox = dst_frame->shared_image()->mailbox();
  if (use_async_copy) {
    auto copy_to_gmb_done_lambda = [](base::OnceClosure callback,
                                      bool success) {
      if (!success) {
        DLOG(ERROR) << "CopyToGpuMemoryBufferAsync failed!";
        base::debug::DumpWithoutCrashing();
      }
      std::move(callback).Run();
    };

    sii->CopyToGpuMemoryBufferAsync(
        blit_done_sync_token, mailbox,
        base::BindOnce(std::move(copy_to_gmb_done_lambda),
                       std::move(callback)));
  } else {
    sii->CopyToGpuMemoryBuffer(blit_done_sync_token, mailbox);
  }

  // Synchronize RasterInterface with SharedImageInterface.
  auto copy_to_gmb_done_sync_token = sii->GenUnverifiedSyncToken();
  ri->WaitSyncTokenCHROMIUM(copy_to_gmb_done_sync_token.GetData());

  // Make access to the `dst_frame` wait on copy completion. We also update the
  // ReleaseSyncToken here since it's used when the underlying GpuMemoryBuffer
  // and SharedImage resources are returned to the pool. This is not necessary
  // since we'll set the empty sync token on the video frame on GPU completion.
  // But if we ever refactor this code to have a "don't wait for GMB" mode, the
  // correct sync token on the video frame will be needed.
  gpu::SyncToken completion_sync_token;
  ri->GenUnverifiedSyncTokenCHROMIUM(completion_sync_token.GetData());
  media::SimpleSyncTokenClient simple_client(completion_sync_token);
  dst_frame->UpdateMailboxHolderSyncToken(&simple_client);
  dst_frame->UpdateReleaseSyncToken(&simple_client);

  // Do not use a query to track copy completion on Windows when using the new
  // CopyToGpuMemoryBufferAsync API which performs an async copy that cannot be
  // tracked using the command buffer.
  if (!use_async_copy) {
    // On Windows, shared memory CopyToGpuMemoryBuffer will do synchronization
    // on its own. No need for GL_COMMANDS_COMPLETED_CHROMIUM QueryEXT.
    SignalGpuCompletion(std::move(ctx_wrapper), GL_COMMANDS_ISSUED_CHROMIUM,
                        std::move(callback));
  }
}
}  // namespace

bool WebGraphicsContext3DVideoFramePool::CopyRGBATextureToVideoFrame(
    viz::SharedImageFormat src_format,
    const gfx::Size& src_size,
    const gfx::ColorSpace& src_color_space,
    GrSurfaceOrigin src_surface_origin,
    const gpu::MailboxHolder& src_mailbox_holder,
    const gfx::ColorSpace& dst_color_space,
    FrameReadyCallback callback) {
  TRACE_EVENT("media", "CopyRGBATextureToVideoFrame");
  int flow_id = trace_flow_seqno_.GetNext();
  TRACE_EVENT_INSTANT("media", "CopyRGBATextureToVideoFrame",
                      perfetto::Flow::ProcessScoped(flow_id));
  if (!weak_context_provider_)
    return false;
  auto* context_provider = weak_context_provider_->ContextProvider();
  if (!context_provider)
    return false;
  auto* raster_context_provider = context_provider->RasterContextProvider();
  if (!raster_context_provider)
    return false;

#if BUILDFLAG(IS_WIN)
  // CopyToGpuMemoryBuffer is only supported for D3D shared images on Windows.
  if (!context_provider->SharedImageInterface()
           ->GetCapabilities()
           .shared_image_d3d) {
    DVLOG(1) << "CopyToGpuMemoryBuffer not supported.";
    return false;
  }
#endif  // BUILDFLAG(IS_WIN)

  auto dst_frame = pool_->MaybeCreateVideoFrame(src_size, dst_color_space);
  if (!dst_frame) {
    return false;
  }
  CHECK(dst_frame->HasSharedImage());

  if (!media::CopyRGBATextureToVideoFrame(
          raster_context_provider, src_format, src_size, src_color_space,
          src_surface_origin, src_mailbox_holder, dst_frame.get())) {
    return false;
  }

  // VideoFrame::UpdateMailboxHolderSyncToken requires that the video frame have
  // a single owner. So cache the pointer for later use after the std::move().
  [[maybe_unused]] auto* dst_frame_ptr = dst_frame.get();

  // The worker can be terminated at any time and in such cases `dst_frame`
  // destructor might be call on mojo IO-thread instead of the worker's thread.
  // It breaks threading rules for using GPU objects. Using a cancelable
  // callback ensures that `dst_frame` is dropped when the worker terminates.
  auto wrapped_callback =
      std::make_unique<base::CancelableOnceClosure>(base::BindOnce(
          [](scoped_refptr<media::VideoFrame> frame,
             FrameReadyCallback callback, int flow_id) {
            TRACE_EVENT_INSTANT(
                "media", "CopyRGBATextureToVideoFrame",
                perfetto::TerminatingFlow::ProcessScoped(flow_id));
            // We can just clear the sync token from the video frame now that
            // we've synchronized with the GPU.
            gpu::SyncToken empty_sync_token;
            media::SimpleSyncTokenClient simple_client(empty_sync_token);
            frame->UpdateMailboxHolderSyncToken(&simple_client);
            frame->UpdateReleaseSyncToken(&simple_client);
            std::move(callback).Run(std::move(frame));
          },
          std::move(dst_frame), std::move(callback), flow_id));

  if (!dst_frame_ptr->HasNativeGpuMemoryBuffer()) {
    // For shared memory GMBs we needed to explicitly request a copy
    // from the shared image GPU texture to the GMB.
    CopyToGpuMemoryBuffer(weak_context_provider_, dst_frame_ptr,
                          wrapped_callback->callback());
  } else {
    // QueryEXT functions are used to make sure that
    // CopyRGBATextureToVideoFrame() texture copy is complete before we access
    // GMB data.
    SignalGpuCompletion(weak_context_provider_, GL_COMMANDS_COMPLETED_CHROMIUM,
                        wrapped_callback->callback());
  }

  // Cleanup stale callbacks before adding a new one. It's ok to loop until the
  // first non-cancelled callback since they should execute in order anyway.
  while (!pending_gpu_completion_callbacks_.empty() &&
         pending_gpu_completion_callbacks_.front()->IsCancelled()) {
    pending_gpu_completion_callbacks_.pop_front();
  }
  pending_gpu_completion_callbacks_.push_back(std::move(wrapped_callback));

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
  DCHECK(src_video_frame->HasSharedImage());
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
      NOTREACHED_IN_MIGRATION();
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
  return base::FeatureList::IsEnabled(kGpuMemoryBufferReadbackFromTexture);
}

}  // namespace blink
