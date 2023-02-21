// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/video_frame_rgba_to_yuva_converter.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/simple_sync_token_client.h"
#include "media/base/wait_and_replace_sync_token_client.h"
#include "media/renderers/video_frame_yuv_converter.h"
#include "media/renderers/video_frame_yuv_mailboxes_holder.h"
#include "skia/ext/rgba_to_yuva.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace {

// Given a gpu::MailboxHolder and a viz::RasterContextProvider, create scoped
// access to the texture as an SkImage.
class ScopedAcceleratedSkImage {
 public:
  static std::unique_ptr<ScopedAcceleratedSkImage> Create(
      viz::RasterContextProvider* provider,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      const gpu::MailboxHolder& mailbox_holder) {
    auto* ri = provider->RasterInterface();
    DCHECK(ri);
    GrDirectContext* gr_context = provider->GrContext();
    DCHECK(gr_context);

    if (!mailbox_holder.mailbox.IsSharedImage()) {
      DLOG(ERROR) << "Cannot created SkImage for non-SharedImage mailbox.";
      return nullptr;
    }

    ri->WaitSyncTokenCHROMIUM(mailbox_holder.sync_token.GetConstData());

    uint32_t texture_id =
        ri->CreateAndConsumeForGpuRaster(mailbox_holder.mailbox);
    if (!texture_id) {
      DLOG(ERROR) << "Failed to create texture for mailbox.";
      return nullptr;
    }
    ri->BeginSharedImageAccessDirectCHROMIUM(
        texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);

    GrGLTextureInfo gl_info = {
        mailbox_holder.texture_target,
        texture_id,
        viz::TextureStorageFormat(
            format, provider->ContextCapabilities().angle_rgbx_internal_format),
    };
    GrBackendTexture backend_texture(size.width(), size.height(),
                                     GrMipmapped::kNo, gl_info);

    SkColorType color_type = viz::ResourceFormatToClosestSkColorType(
        /*gpu_compositing=*/true, format);
    sk_sp<SkImage> sk_image = SkImage::MakeFromTexture(
        gr_context, backend_texture, surface_origin, color_type,
        kOpaque_SkAlphaType, color_space.ToSkColorSpace());
    if (!sk_image) {
      DLOG(ERROR) << "Failed to SkImage for StaticBitmapImage.";
      ri->EndSharedImageAccessDirectCHROMIUM(texture_id);
      ri->DeleteGpuRasterTexture(texture_id);
      return nullptr;
    }

    return base::WrapUnique<ScopedAcceleratedSkImage>(
        new ScopedAcceleratedSkImage(provider, texture_id,
                                     std::move(sk_image)));
  }

  ~ScopedAcceleratedSkImage() {
    auto* ri = provider_->RasterInterface();
    DCHECK(ri);
    GrDirectContext* gr_context = provider_->GrContext();
    DCHECK(gr_context);

    sk_image_ = nullptr;
    if (texture_id_) {
      ri->EndSharedImageAccessDirectCHROMIUM(texture_id_);
      ri->DeleteGpuRasterTexture(texture_id_);
    }
  }

  sk_sp<SkImage> sk_image() { return sk_image_; }

 private:
  ScopedAcceleratedSkImage(viz::RasterContextProvider* provider,
                           uint32_t texture_id,
                           sk_sp<SkImage> sk_image)
      : provider_(provider), texture_id_(texture_id), sk_image_(sk_image) {}

  const raw_ptr<viz::RasterContextProvider> provider_;
  uint32_t texture_id_ = 0;
  sk_sp<SkImage> sk_image_;
};

}  // namespace

namespace media {

bool CopyRGBATextureToVideoFrame(viz::RasterContextProvider* provider,
                                 viz::ResourceFormat src_format,
                                 const gfx::Size& src_size,
                                 const gfx::ColorSpace& src_color_space,
                                 GrSurfaceOrigin src_surface_origin,
                                 const gpu::MailboxHolder& src_mailbox_holder,
                                 VideoFrame* dst_video_frame) {
  DCHECK_EQ(dst_video_frame->format(), PIXEL_FORMAT_NV12);

  auto* ri = provider->RasterInterface();
  DCHECK(ri);

  // If context is lost for any reason e.g. creating shared image failed, we
  // cannot distinguish between OOP and non-OOP raster based on GrContext().
  if (ri->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
    DLOG(ERROR) << "Raster context lost.";
    return false;
  }

#if BUILDFLAG(IS_WIN)
  // CopyToGpuMemoryBuffer is only supported for D3D shared images on Windows.
  if (!provider->ContextCapabilities().shared_image_d3d) {
    DLOG(ERROR) << "CopyToGpuMemoryBuffer not supported.";
    return false;
  }
#endif  // BUILDFLAG(IS_WIN)

  if ((!provider->GrContext() ||
       provider->ContextCapabilities().supports_yuv_rgb_conversion) &&
      src_mailbox_holder.mailbox.IsSharedImage()) {
    SkYUVAInfo yuva_info =
        VideoFrameYUVMailboxesHolder::VideoFrameGetSkYUVAInfo(dst_video_frame);
    gpu::Mailbox yuva_mailboxes[SkYUVAInfo::kMaxPlanes];
    ri->WaitSyncTokenCHROMIUM(src_mailbox_holder.sync_token.GetConstData());
    for (int plane = 0; plane < yuva_info.numPlanes(); ++plane) {
      gpu::MailboxHolder dst_mailbox_holder =
          dst_video_frame->mailbox_holder(plane);
      ri->WaitSyncTokenCHROMIUM(dst_mailbox_holder.sync_token.GetConstData());
      yuva_mailboxes[plane] = dst_mailbox_holder.mailbox;
    }
    ri->ConvertRGBAToYUVAMailboxes(
        yuva_info.yuvColorSpace(), yuva_info.planeConfig(),
        yuva_info.subsampling(), yuva_mailboxes, src_mailbox_holder.mailbox);
  } else {
    // Create an accelerated SkImage for the source.
    auto scoped_sk_image = ScopedAcceleratedSkImage::Create(
        provider, src_format, src_size, src_color_space, src_surface_origin,
        src_mailbox_holder);
    if (!scoped_sk_image) {
      DLOG(ERROR) << "Failed to create accelerated SkImage for RGBA to YUVA "
                     "conversion.";
      return false;
    }

    // Create SkSurfaces for the destination planes.
    sk_sp<SkSurface> sk_surfaces[SkYUVAInfo::kMaxPlanes];
    SkSurface* sk_surface_ptrs[SkYUVAInfo::kMaxPlanes] = {nullptr};
    VideoFrameYUVMailboxesHolder holder;
    if (!holder.VideoFrameToPlaneSkSurfaces(dst_video_frame, provider,
                                            sk_surfaces)) {
      DLOG(ERROR) << "Failed to create SkSurfaces for VideoFrame.";
      return false;
    }

    // Make GrContext wait for `dst_video_frame`. Waiting on the mailbox tokens
    // here ensures that all writes are completed in cases where the underlying
    // GpuMemoryBuffer and SharedImage resources have been reused.
    ri->Flush();
    WaitAndReplaceSyncTokenClient client(ri);
    for (int plane = 0; plane < holder.yuva_info().numPlanes(); ++plane) {
      sk_surface_ptrs[plane] = sk_surfaces[plane].get();
      dst_video_frame->UpdateMailboxHolderSyncToken(plane, &client);
    }

    // Do the blit.
    skia::BlitRGBAToYUVA(scoped_sk_image->sk_image().get(), sk_surface_ptrs,
                         holder.yuva_info());
    provider->GrContext()->flushAndSubmit(false);
  }
  ri->Flush();

  const size_t num_planes = dst_video_frame->layout().num_planes();

#if BUILDFLAG(IS_WIN)
  // For shared memory GMBs on Windows we needed to explicitly request a copy
  // from the shared image GPU texture to the GMB.
  DCHECK(dst_video_frame->HasGpuMemoryBuffer());
  DCHECK_EQ(dst_video_frame->GetGpuMemoryBuffer()->GetType(),
            gfx::SHARED_MEMORY_BUFFER);

  gpu::SyncToken blit_done_sync_token;
  ri->GenUnverifiedSyncTokenCHROMIUM(blit_done_sync_token.GetData());

  auto* sii = provider->SharedImageInterface();
  for (size_t plane = 0; plane < num_planes; ++plane) {
    const auto& mailbox = dst_video_frame->mailbox_holder(plane).mailbox;
    sii->CopyToGpuMemoryBuffer(blit_done_sync_token, mailbox);
  }

  // Synchronize RasterInterface with SharedImageInterface. We want to generate
  // the final SyncToken from the RasterInterface since callers might be using
  // RasterInterface::Finish() to ensure synchronization in cases where
  // SignalSyncToken can't be used (e.g. webrtc video frame adapter).
  auto copy_to_gmb_done_sync_token = sii->GenUnverifiedSyncToken();
  ri->WaitSyncTokenCHROMIUM(copy_to_gmb_done_sync_token.GetData());
#endif  // BUILDFLAG(IS_WIN)

  // Make access to the `dst_video_frame` wait on copy completion. We also
  // update the ReleaseSyncToken here since it's used when the underlying
  // GpuMemoryBuffer and SharedImage resources are returned to the pool.
  gpu::SyncToken completion_sync_token;
  ri->GenSyncTokenCHROMIUM(completion_sync_token.GetData());
  SimpleSyncTokenClient simple_client(completion_sync_token);
  for (size_t plane = 0; plane < num_planes; ++plane)
    dst_video_frame->UpdateMailboxHolderSyncToken(plane, &simple_client);
  dst_video_frame->UpdateReleaseSyncToken(&simple_client);
  return true;
}

}  // namespace media
