// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_video_frame_pool.h"

#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/common/task_annotator.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/context_support.h"
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

class Context : public media::RenderableGpuMemoryBufferVideoFramePool::Context {
 public:
  explicit Context(base::WeakPtr<blink::WebGraphicsContext3DProviderWrapper>
                       context_provider)
      : weak_context_provider_(context_provider) {}

  scoped_refptr<gpu::ClientSharedImage> CreateSharedImage(
      const gfx::Size& size,
      gfx::BufferUsage buffer_usage,
      const viz::SharedImageFormat& si_format,
      const gfx::ColorSpace& color_space,
      gpu::SharedImageUsageSet usage,
      gpu::SyncToken& sync_token) override {
    auto* sii = SharedImageInterface();
    if (!sii) {
      return nullptr;
    }
    auto client_shared_image =
        sii->CreateSharedImage({si_format, size, color_space, usage,
                                "WebGraphicsContext3DVideoFramePool"},
                               gpu::kNullSurfaceHandle, buffer_usage);
    if (!client_shared_image) {
      return nullptr;
    }
    sync_token = sii->GenVerifiedSyncToken();
    return client_shared_image;
  }

  void DestroySharedImage(
      const gpu::SyncToken& sync_token,
      scoped_refptr<gpu::ClientSharedImage> shared_image) override {
    CHECK(shared_image);
    shared_image->UpdateDestructionSyncToken(sync_token);
  }

  const gpu::SharedImageCapabilities& GetCapabilities() override {
    return SharedImageInterface()->GetCapabilities();
  }

 private:
  gpu::SharedImageInterface* SharedImageInterface() const {
    if (!weak_context_provider_)
      return nullptr;
    return weak_context_provider_->ContextProvider().SharedImageInterface();
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
    if (auto* raster_context_provider =
            weak_context_provider_->ContextProvider().RasterContextProvider()) {
      return raster_context_provider->RasterInterface();
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
  auto& context_provider = ctx_wrapper->ContextProvider();
  auto* raster_context_provider = context_provider.RasterContextProvider();
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
          if (auto* ri_provider =
                  ctx_wrapper->ContextProvider().RasterContextProvider()) {
            auto* ri = ri_provider->RasterInterface();
            ri->DeleteQueriesEXT(1, &query_id);
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
    const gpu::SyncToken& blit_done_sync_token,
    base::OnceClosure callback) {
  CHECK(dst_frame->HasMappableSharedImage());
  CHECK(!dst_frame->HasNativeGpuMemoryBuffer());
  CHECK(dst_frame->HasSharedImage());

  DCHECK(ctx_wrapper);
  auto& context_provider = ctx_wrapper->ContextProvider();
  auto* raster_context_provider = context_provider.RasterContextProvider();
  DCHECK(raster_context_provider);
  auto* ri = raster_context_provider->RasterInterface();
  DCHECK(ri);

  auto* sii = context_provider.SharedImageInterface();
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
  dst_frame->UpdateAcquireSyncToken(completion_sync_token);
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

BASE_FEATURE(kUseCopyToGpuMemoryBufferAsync,
#if BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

std::optional<gpu::SyncToken>
WebGraphicsContext3DVideoFramePool::CopyRGBATextureToVideoFrame(
    const gfx::Size& src_size,
    scoped_refptr<gpu::ClientSharedImage> src_shared_image,
    const gpu::SyncToken& acquire_sync_token,
    const gfx::ColorSpace& dst_color_space,
    FrameReadyCallback callback) {
  TRACE_EVENT("media", "CopyRGBATextureToVideoFrame");
  int flow_id = trace_flow_seqno_.GetNext();
  TRACE_EVENT_INSTANT("media", "CopyRGBATextureToVideoFrame",
                      perfetto::Flow::ProcessScoped(flow_id));
  if (!weak_context_provider_)
    return std::nullopt;
  auto& context_provider = weak_context_provider_->ContextProvider();
  auto* raster_context_provider = context_provider.RasterContextProvider();
  if (!raster_context_provider)
    return std::nullopt;

#if BUILDFLAG(IS_WIN)
  // CopyToGpuMemoryBuffer is only supported for D3D shared images on Windows.
  if (!context_provider.SharedImageInterface()
           ->GetCapabilities()
           .shared_image_d3d) {
    DVLOG(1) << "CopyToGpuMemoryBuffer not supported.";
    return std::nullopt;
  }
#endif  // BUILDFLAG(IS_WIN)

  auto dst_frame = pool_->MaybeCreateVideoFrame(src_size, dst_color_space);
  if (!dst_frame) {
    return std::nullopt;
  }
  CHECK(dst_frame->HasSharedImage());

  std::optional<gpu::SyncToken> completion_sync_token =
      media::CopyRGBATextureToVideoFrame(raster_context_provider, src_size,
                                         src_shared_image, acquire_sync_token,
                                         dst_frame.get());
  if (!completion_sync_token) {
    return std::nullopt;
  }

  // VideoFrame::UpdateAcquireSyncToken requires that the video frame have
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
            frame->UpdateAcquireSyncToken(empty_sync_token);
            frame->UpdateReleaseSyncToken(&simple_client);
            std::move(callback).Run(std::move(frame));
          },
          std::move(dst_frame), std::move(callback), flow_id));

  if (!dst_frame_ptr->HasNativeGpuMemoryBuffer()) {
    // For shared memory GMBs we needed to explicitly request a copy
    // from the shared image GPU texture to the GMB.
    CopyToGpuMemoryBuffer(weak_context_provider_, dst_frame_ptr,
                          completion_sync_token.value(),
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

  return completion_sync_token;
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
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_LINUX)
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
  return CopyRGBATextureToVideoFrame(
             src_video_frame->coded_size(), src_video_frame->shared_image(),
             src_video_frame->acquire_sync_token(), dst_color_space,
             blink::BindOnce(ApplyMetadataAndRunCallback, src_video_frame,
                             std::move(callback)))
      .has_value();
}

// static
bool WebGraphicsContext3DVideoFramePool::
    IsGpuMemoryBufferReadbackFromTextureEnabled() {
  return base::FeatureList::IsEnabled(kGpuMemoryBufferReadbackFromTexture);
}

}  // namespace blink
