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

  gpu::Mailbox src_mailbox;
  if (!video_frame->HasTextures()) {
    // For pure software pixel upload path with video frame that does not have
    // textures.
    src_mailbox =
        holder_->VideoFrameToMailbox(video_frame, raster_context_provider);
  } else {
    // For video frame with shared image that has textures.
    src_mailbox = video_frame->mailbox_holder(0).mailbox;
  }
  ri->CopySharedImage(src_mailbox, dest_mailbox_holder.mailbox, GL_TEXTURE_2D,
                      0, 0, source_rect.x(), source_rect.y(),
                      source_rect.width(), source_rect.height(),
                      /*unpack_flip_y=*/false,
                      /*unpack_premultiply_alpha=*/false);
  return true;
}

void VideoFrameYUVConverter::ReleaseCachedData() {
  holder_.reset();
}

}  // namespace media
