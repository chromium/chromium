// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "media/renderers/video_frame_yuv_converter.h"

#include <array>

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

namespace media::internals {

bool IsPixelFormatSupportedForYuvSharedImageConversion(
    VideoPixelFormat video_format) {
  // To expand support for additional VideoFormats expand this switch.
  switch (video_format) {
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_P010LE:
    case PIXEL_FORMAT_NV16:
    case PIXEL_FORMAT_P210LE:
    case PIXEL_FORMAT_NV24:
    case PIXEL_FORMAT_P410LE:
    case PIXEL_FORMAT_NV12A:
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_I420A:
      return true;
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12:
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_UYVY:
    case PIXEL_FORMAT_YUY2:
    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_MJPEG:
    case PIXEL_FORMAT_Y16:
    case PIXEL_FORMAT_XR30:
    case PIXEL_FORMAT_XB30:
    case PIXEL_FORMAT_BGRA:
    case PIXEL_FORMAT_RGBAF16:
    case PIXEL_FORMAT_I422A:
    case PIXEL_FORMAT_I444A:
    case PIXEL_FORMAT_YUV420AP10:
    case PIXEL_FORMAT_YUV422AP10:
    case PIXEL_FORMAT_YUV444AP10:
    case PIXEL_FORMAT_UNKNOWN:
      return false;
  }
}

gpu::SyncToken ConvertYuvVideoFrameToRgbSharedImage(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    const gpu::Mailbox& dest_mailbox,
    const gpu::SyncToken& dest_sync_token,
    bool use_visible_rect,
    VideoFrameSharedImageCache* shared_image_cache) {
  CHECK(video_frame);
  CHECK(!video_frame->HasSharedImage());
  DCHECK(
      IsPixelFormatSupportedForYuvSharedImageConversion(video_frame->format()))
      << "VideoFrame has an unsupported YUV format " << video_frame->format();
  DCHECK(!video_frame->coded_size().IsEmpty())
      << "|video_frame| must have an area > 0";
  DCHECK(raster_context_provider);

  // Callers may choose to provide cache which ensures that the source yuv
  // shared images are cached across convert calls.
  std::unique_ptr<VideoFrameSharedImageCache> local_si_cache;
  if (!shared_image_cache) {
    local_si_cache = std::make_unique<VideoFrameSharedImageCache>();
    shared_image_cache = local_si_cache.get();
  }

  auto* ri = raster_context_provider->RasterInterface();
  DCHECK(ri);
  ri->WaitSyncTokenCHROMIUM(dest_sync_token.GetConstData());

  auto source_rect = use_visible_rect ? video_frame->visible_rect()
                                      : gfx::Rect(video_frame->coded_size());

  // This SharedImage will be written to (and later read from) via the raster
  // interface. The full usage depends on whether raster is OOP or is going
  // over the GLES2 interface.
  gpu::SharedImageUsageSet src_usage = gpu::SHARED_IMAGE_USAGE_RASTER_READ |
                                       gpu::SHARED_IMAGE_USAGE_RASTER_WRITE;
  const auto& caps = raster_context_provider->ContextCapabilities();
  if (caps.gpu_rasterization) {
    src_usage |= gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
  } else {
    // NOTE: This GLES2 usage is *only* for raster, as this SharedImage is
    // created to hold YUV data that is then converted to RGBA via the raster
    // interface before being shared with some other use case (e.g., WebGL).
    // There is no flow wherein this SharedImage is directly exposed to
    // WebGL. Moreover, this raster usage is by definition *only* over GLES2
    // (since this is non-OOP-R). It is critical to specify both of these
    // facts to the service side to ensure that the needed SharedImage backing
    // gets created (see crbug.com/328472684).
    src_usage |= gpu::SHARED_IMAGE_USAGE_GLES2_READ |
                 gpu::SHARED_IMAGE_USAGE_GLES2_WRITE |
                 gpu::SHARED_IMAGE_USAGE_GLES2_FOR_RASTER_ONLY |
                 gpu::SHARED_IMAGE_USAGE_RASTER_OVER_GLES2_ONLY;
  }

  // For pure software pixel upload path with video frame that does not have
  // textures.
  auto [src_shared_image, si_sync_token, status] =
      shared_image_cache->GetOrCreateSharedImage(
          video_frame, raster_context_provider, src_usage);
  CHECK(src_shared_image);
  if (status == VideoFrameSharedImageCache::Status::kMatchedVideoFrameId) {
    // Since the video frame id matches, no need to upload pixels or copy shared
    // image again.
    return dest_sync_token;
  }

  ri->WaitSyncTokenCHROMIUM(si_sync_token.GetConstData());
  const viz::SharedImageFormat si_format = src_shared_image->format();
  constexpr SkAlphaType kPlaneAlphaType = kUnpremul_SkAlphaType;
  std::array<SkPixmap, SkYUVAInfo::kMaxPlanes> pixmaps = {};

  for (int plane = 0; plane < si_format.NumberOfPlanes(); ++plane) {
    SkColorType color_type = viz::ToClosestSkColorType(si_format, plane);
    gfx::Size plane_size =
        si_format.GetPlaneSize(plane, video_frame->coded_size());
    SkImageInfo info = SkImageInfo::Make(gfx::SizeToSkISize(plane_size),
                                         color_type, kPlaneAlphaType);
    pixmaps[plane] =
        SkPixmap(info, video_frame->data(plane), video_frame->stride(plane));
  }

  // Prepare the SkYUVAInfo
  SkISize video_size = gfx::SizeToSkISize(video_frame->coded_size());
  SkYUVAInfo::PlaneConfig plane_config = ToSkYUVAPlaneConfig(si_format);
  SkYUVAInfo::Subsampling subsampling = ToSkYUVASubsampling(si_format);

  // TODO(crbug.com/41380578): This should really default to rec709.
  SkYUVColorSpace color_space = kRec601_SkYUVColorSpace;
  video_frame->ColorSpace().ToSkYUVColorSpace(video_frame->BitDepth(),
                                              &color_space);
  SkYUVAInfo yuva_info =
      SkYUVAInfo(video_size, plane_config, subsampling, color_space);

  SkYUVAPixmaps yuv_pixmap =
      SkYUVAPixmaps::FromExternalPixmaps(yuva_info, pixmaps.data());
  ri->WritePixelsYUV(src_shared_image->mailbox(), yuv_pixmap);

  ri->CopySharedImage(src_shared_image->mailbox(), dest_mailbox, 0, 0,
                      source_rect.x(), source_rect.y(), source_rect.width(),
                      source_rect.height());

  gpu::SyncToken ri_sync_token;
  ri->GenUnverifiedSyncTokenCHROMIUM(ri_sync_token.GetData());

  shared_image_cache->UpdateSyncToken(ri_sync_token);
  return ri_sync_token;
}

}  // namespace media::internals
