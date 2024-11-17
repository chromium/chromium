// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/renderers/video_frame_rgba_to_yuva_converter.h"

#include "base/check.h"
#include "base/logging.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "media/base/simple_sync_token_client.h"

namespace media {

bool CopyRGBATextureToVideoFrame(
    viz::RasterContextProvider* provider,
    const gfx::Size& src_size,
    scoped_refptr<gpu::ClientSharedImage> src_shared_image,
    const gpu::SyncToken& acquire_sync_token,
    VideoFrame* dst_video_frame) {
  DCHECK_EQ(dst_video_frame->format(), PIXEL_FORMAT_NV12);
  CHECK(dst_video_frame->HasSharedImage());
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

  ri->WaitSyncTokenCHROMIUM(acquire_sync_token.GetConstData());

  auto dst_sync_token = dst_video_frame->acquire_sync_token();
  auto dst_mailbox = dst_video_frame->shared_image()->mailbox();
  ri->WaitSyncTokenCHROMIUM(dst_sync_token.GetConstData());

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
  ri->CopySharedImage(src_shared_image->mailbox(), dst_mailbox, 0, 0, 0, 0,
                      src_size.width(), src_size.height());
  ri->Flush();

  // Make access to the `dst_video_frame` wait on copy completion. We also
  // update the ReleaseSyncToken here since it's used when the underlying
  // GpuMemoryBuffer and SharedImage resources are returned to the pool.
  gpu::SyncToken completion_sync_token;
  ri->GenUnverifiedSyncTokenCHROMIUM(completion_sync_token.GetData());
  SimpleSyncTokenClient simple_client(completion_sync_token);
  dst_video_frame->UpdateAcquireSyncToken(&simple_client);
  dst_video_frame->UpdateReleaseSyncToken(&simple_client);
  return true;
}

}  // namespace media
