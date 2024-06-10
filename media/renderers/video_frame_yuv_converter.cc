// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/video_frame_yuv_converter.h"

#include <GLES3/gl3.h>

#include "base/logging.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/renderers/video_frame_yuv_mailboxes_holder.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"

namespace media {

VideoFrameYUVConverter::VideoFrameYUVConverter() = default;
VideoFrameYUVConverter::~VideoFrameYUVConverter() = default;

bool VideoFrameYUVConverter::IsVideoFrameFormatSupported(
    const VideoFrame& video_frame) {
  return std::get<0>(VideoPixelFormatToSkiaValues(video_frame.format())) !=
         SkYUVAInfo::PlaneConfig::kUnknown;
}

bool VideoFrameYUVConverter::ConvertYUVVideoFrameNoCaching(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    const gpu::MailboxHolder& dest_mailbox_holder,
    std::optional<GrParams> gr_params) {
  VideoFrameYUVConverter converter;
  return converter.ConvertYUVVideoFrame(video_frame, raster_context_provider,
                                        dest_mailbox_holder, gr_params);
}

bool VideoFrameYUVConverter::ConvertYUVVideoFrame(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    const gpu::MailboxHolder& dest_mailbox_holder,
    std::optional<GrParams> gr_params) {
  DCHECK(video_frame);
  DCHECK(IsVideoFrameFormatSupported(*video_frame))
      << "VideoFrame has an unsupported YUV format " << video_frame->format();
  DCHECK(!video_frame->coded_size().IsEmpty())
      << "|video_frame| must have an area > 0";
  DCHECK(raster_context_provider);

  if (!holder_)
    holder_ = std::make_unique<VideoFrameYUVMailboxesHolder>();

  // The RasterInterface path does not support flip_y.
  if (gr_params) {
    DCHECK(!gr_params->flip_y);
  }

  auto* ri = raster_context_provider->RasterInterface();
  DCHECK(ri);
  ri->WaitSyncTokenCHROMIUM(dest_mailbox_holder.sync_token.GetConstData());

  auto source_rect = gr_params && gr_params->use_visible_rect
                         ? video_frame->visible_rect()
                         : gfx::Rect(video_frame->coded_size());

  if (!video_frame->HasTextures() && IsWritePixelsYUVEnabled()) {
    // For pure software pixel upload paths with video frames that don't have
    // textures.
    gpu::Mailbox mailboxes[SkYUVAInfo::kMaxPlanes]{};
    holder_->VideoFrameToMailboxes(video_frame, raster_context_provider,
                                   mailboxes,
                                   /*allow_multiplanar_for_upload=*/true);
    gpu::Mailbox src_mailbox = mailboxes[0];
    ri->CopySharedImage(src_mailbox, dest_mailbox_holder.mailbox, GL_TEXTURE_2D,
                        0, 0, source_rect.x(), source_rect.y(),
                        source_rect.width(), source_rect.height(),
                        /*unpack_flip_y=*/false,
                        /*unpack_premultiply_alpha=*/false);
  } else if (video_frame->shared_image_format_type() !=
                 SharedImageFormatType::kLegacy &&
             video_frame->HasTextures()) {
    // For new multiplanar shared images path with video frames that have
    // textures.
    gpu::Mailbox src_mailbox = video_frame->mailbox_holder(0).mailbox;
    ri->CopySharedImage(src_mailbox, dest_mailbox_holder.mailbox, GL_TEXTURE_2D,
                        0, 0, source_rect.x(), source_rect.y(),
                        source_rect.width(), source_rect.height(),
                        /*unpack_flip_y=*/false,
                        /*unpack_premultiply_alpha=*/false);
  } else {
    // For legacy multiplanar cases or software pixel upload cases without
    // IsWritePixelsYUVEnabled().
    gpu::Mailbox mailboxes[SkYUVAInfo::kMaxPlanes]{};
    holder_->VideoFrameToMailboxes(video_frame, raster_context_provider,
                                   mailboxes,
                                   /*allow_multiplanar_for_upload=*/false);
    ri->ConvertYUVAMailboxesToRGB(
        dest_mailbox_holder.mailbox, source_rect.x(), source_rect.y(),
        source_rect.width(), source_rect.height(),
        holder_->yuva_info().yuvColorSpace(), nullptr,
        holder_->yuva_info().planeConfig(), holder_->yuva_info().subsampling(),
        mailboxes);
  }
  return true;
}

void VideoFrameYUVConverter::ReleaseCachedData() {
  holder_.reset();
}

}  // namespace media
