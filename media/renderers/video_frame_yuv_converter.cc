// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/renderers/video_frame_yuv_converter.h"

#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/renderers/video_frame_shared_image_cache.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"
#include "third_party/skia/include/core/SkYUVAPixmaps.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace media {

VideoFrameYUVConverter::VideoFrameYUVConverter() = default;
VideoFrameYUVConverter::~VideoFrameYUVConverter() = default;

bool VideoFrameYUVConverter::IsVideoFrameFormatSupported(
    const VideoFrame& video_frame) {
  return std::get<0>(VideoPixelFormatToSkiaValues(video_frame.format())) !=
         SkYUVAInfo::PlaneConfig::kUnknown;
}

void VideoFrameYUVConverter::ConvertYUVVideoFrame(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    const gpu::MailboxHolder& dest_mailbox_holder,
    bool use_visible_rect) {
  CHECK(video_frame);
  CHECK(!video_frame->HasSharedImage());
  DCHECK(IsVideoFrameFormatSupported(*video_frame))
      << "VideoFrame has an unsupported YUV format " << video_frame->format();
  DCHECK(!video_frame->coded_size().IsEmpty())
      << "|video_frame| must have an area > 0";
  DCHECK(raster_context_provider);

  if (!shared_image_cache_) {
    shared_image_cache_ = std::make_unique<VideoFrameSharedImageCache>();
  }

  auto* ri = raster_context_provider->RasterInterface();
  DCHECK(ri);
  ri->WaitSyncTokenCHROMIUM(dest_mailbox_holder.sync_token.GetConstData());

  auto source_rect = use_visible_rect ? video_frame->visible_rect()
                                      : gfx::Rect(video_frame->coded_size());

  // For pure software pixel upload path with video frame that does not have
  // textures.
  const scoped_refptr<gpu::ClientSharedImage>& src_shared_image =
      shared_image_cache_->GetSharedImage(video_frame, raster_context_provider);
  CHECK(src_shared_image);
  const viz::SharedImageFormat si_format = src_shared_image->format();
  constexpr SkAlphaType kPlaneAlphaType = kUnpremul_SkAlphaType;
  SkPixmap pixmaps[SkYUVAInfo::kMaxPlanes] = {};

  for (int plane = 0; plane < si_format.NumberOfPlanes(); ++plane) {
    SkColorType color_type =
        viz::ToClosestSkColorType(/*gpu_compositing=*/true, si_format, plane);
    gfx::Size plane_size =
        si_format.GetPlaneSize(plane, video_frame->coded_size());
    SkImageInfo info = SkImageInfo::Make(gfx::SizeToSkISize(plane_size),
                                         color_type, kPlaneAlphaType);
    pixmaps[plane] =
        SkPixmap(info, video_frame->data(plane), video_frame->stride(plane));
  }

  // Prepare the SkYUVAInfo
  SkISize video_size = gfx::SizeToSkISize(video_frame->coded_size());
  auto plane_config = SkYUVAInfo::PlaneConfig::kUnknown;
  auto subsampling = SkYUVAInfo::Subsampling::kUnknown;
  std::tie(plane_config, subsampling) =
      VideoPixelFormatToSkiaValues(video_frame->format());

  // TODO(crbug.com/41380578): This should really default to rec709.
  SkYUVColorSpace color_space = kRec601_SkYUVColorSpace;
  video_frame->ColorSpace().ToSkYUVColorSpace(video_frame->BitDepth(),
                                              &color_space);
  SkYUVAInfo yuva_info =
      SkYUVAInfo(video_size, plane_config, subsampling, color_space);

  SkYUVAPixmaps yuv_pixmap =
      SkYUVAPixmaps::FromExternalPixmaps(yuva_info, pixmaps);
  ri->WritePixelsYUV(src_shared_image->mailbox(), yuv_pixmap);

  ri->CopySharedImage(src_shared_image->mailbox(), dest_mailbox_holder.mailbox,
                      0, 0, source_rect.x(), source_rect.y(),
                      source_rect.width(), source_rect.height());
}

void VideoFrameYUVConverter::ReleaseCachedData() {
  shared_image_cache_.reset();
}

}  // namespace media
