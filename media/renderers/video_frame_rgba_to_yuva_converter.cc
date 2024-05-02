// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/video_frame_rgba_to_yuva_converter.h"

#include "base/check.h"
#include "base/logging.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "media/base/simple_sync_token_client.h"
#include "media/renderers/video_frame_yuv_mailboxes_holder.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

bool CopyRGBATextureToVideoFrame(viz::RasterContextProvider* provider,
                                 viz::SharedImageFormat src_format,
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

  // With OOP raster, if RGB->YUV conversion is unsupported, the CopySharedImage
  // calls will fail on the service side with no ability to detect failure on
  // the client side. Check for support here and early out if it's unsupported.
  if (!provider->ContextCapabilities().supports_rgb_to_yuv_conversion) {
    DVLOG(1) << "RGB->YUV conversion not supported";
    return false;
  }

#if BUILDFLAG(IS_WIN)
  // CopyToGpuMemoryBuffer is only supported for D3D shared images on Windows.
  if (!provider->SharedImageInterface()->GetCapabilities().shared_image_d3d) {
    DVLOG(1) << "CopyToGpuMemoryBuffer not supported.";
    return false;
  }
#endif  // BUILDFLAG(IS_WIN)

  ri->WaitSyncTokenCHROMIUM(src_mailbox_holder.sync_token.GetConstData());
  if (dst_video_frame->shared_image_format_type() ==
      SharedImageFormatType::kLegacy) {
    SkYUVAInfo yuva_info =
        VideoFrameYUVMailboxesHolder::VideoFrameGetSkYUVAInfo(dst_video_frame);
    gpu::Mailbox yuva_mailboxes[SkYUVAInfo::kMaxPlanes];
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
    gpu::MailboxHolder dst_mailbox_holder = dst_video_frame->mailbox_holder(0);
    ri->WaitSyncTokenCHROMIUM(dst_mailbox_holder.sync_token.GetConstData());

    // `unpack_flip_y` should be set if the surface origin of the source
    // doesn't match that of the destination, which is created with
    // kTopLeft_GrSurfaceOrigin.
    // TODO(crbug.com/40271944): If this codepath is used with destinations
    // that are created with other surface origins, will need to generalize
    // this.
    bool unpack_flip_y = (src_surface_origin != kTopLeft_GrSurfaceOrigin);

    // Note: the destination video frame can have a coded size that is larger
    // than that of the source video to account for alignment needs. In this
    // case, both this codepath and the the legacy codepath above stretch to
    // fill the destination. Cropping would clearly be more correct, but
    // implementing that behavior in CopySharedImage() for the MultiplanarSI
    // case resulted in pixeltest failures due to pixel bleeding around image
    // borders that we weren't able to resolve (see crbug.com/1451025 for
    // details).
    // TODO(crbug.com/40270413): Update this comment when we resolve that bug
    // and change CopySharedImage() to crop rather than stretch.
    ri->CopySharedImage(src_mailbox_holder.mailbox, dst_mailbox_holder.mailbox,
                        GL_TEXTURE_2D, 0, 0, 0, 0, src_size.width(),
                        src_size.height(), unpack_flip_y,
                        /*unpack_premultiply_alpha=*/false);
  }
  ri->Flush();

#if BUILDFLAG(IS_WIN)
  // For shared memory GMBs on Windows we needed to explicitly request a copy
  // from the shared image GPU texture to the GMB.
  DCHECK(dst_video_frame->HasGpuMemoryBuffer());
  DCHECK_EQ(dst_video_frame->GetGpuMemoryBuffer()->GetType(),
            gfx::SHARED_MEMORY_BUFFER);

  gpu::SyncToken blit_done_sync_token;
  ri->GenUnverifiedSyncTokenCHROMIUM(blit_done_sync_token.GetData());

  auto* sii = provider->SharedImageInterface();
  for (size_t plane = 0; plane < dst_video_frame->NumTextures(); ++plane) {
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
  for (size_t plane = 0; plane < dst_video_frame->NumTextures(); ++plane) {
    dst_video_frame->UpdateMailboxHolderSyncToken(plane, &simple_client);
  }
  dst_video_frame->UpdateReleaseSyncToken(&simple_client);
  return true;
}

}  // namespace media
