// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "media/renderers/video_frame_rgba_to_yuva_converter.h"

#include <algorithm>

#include "base/check.h"
#include "base/logging.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "media/base/format_utils.h"
#include "media/base/simple_sync_token_client.h"

namespace media {

std::optional<gpu::SyncToken> CopyRGBATextureToVideoFrame(
    viz::RasterContextProvider* provider,
    const gfx::Size& src_size,
    scoped_refptr<gpu::ClientSharedImage> src_shared_image,
    const gpu::SyncToken& acquire_sync_token,
    VideoFrame* dst_video_frame) {
  const auto dst_shared_image_format =
      VideoPixelFormatToSharedImageFormat(dst_video_frame->format());
  CHECK(dst_shared_image_format && dst_shared_image_format->is_multi_plane());
  CHECK(dst_video_frame->HasSharedImage());
  auto* ri = provider->RasterInterface();
  DCHECK(ri);

  if (ri->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
    DLOG(ERROR) << "Raster context lost.";
    return std::nullopt;
  }

  // Unsupported CopySharedImage calls fail asynchronously on the service side,
  // so check that Skia can render into the destination planes before submitting
  // the command.
  auto* sii = provider->SharedImageInterface();
  if (!sii ||
      !std::ranges::contains(sii->GetCapabilities().skia_writable_yuv_formats,
                             *dst_shared_image_format)) {
    DVLOG(1) << "RGB->YUV conversion does not support "
             << dst_shared_image_format->ToString();
    return std::nullopt;
  }

  std::unique_ptr<gpu::RasterScopedAccess> src_ri_access =
      src_shared_image->BeginRasterAccess(ri, acquire_sync_token,
                                          /*readonly=*/true);

  auto dst_sync_token = dst_video_frame->acquire_sync_token();
  auto dst_shared_image = dst_video_frame->shared_image();
  std::unique_ptr<gpu::RasterScopedAccess> dst_ri_access =
      dst_shared_image->BeginRasterAccess(ri, dst_sync_token,
                                          /*readonly=*/false);

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
  ri->CopySharedImage(src_shared_image->mailbox(), dst_shared_image->mailbox(),
                      0, 0, 0, 0, src_size.width(), src_size.height());
  ri->Flush();

  // Make access to the `dst_video_frame` wait on copy completion. We also
  // update the ReleaseSyncToken here since it's used when the underlying
  // SharedImage resources are returned to the pool.
  gpu::RasterScopedAccess::EndAccess(std::move(dst_ri_access));
  gpu::SyncToken completion_sync_token =
      gpu::RasterScopedAccess::EndAccess(std::move(src_ri_access));
  SimpleSyncTokenClient simple_client(completion_sync_token);
  dst_video_frame->UpdateAcquireSyncToken(completion_sync_token);
  dst_video_frame->UpdateReleaseSyncToken(&simple_client);
  return completion_sync_token;
}

}  // namespace media
