// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/video_frame_yuv_converter.h"

#include <array>

#include "base/memory/scoped_refptr.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
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
    scoped_refptr<gpu::ClientSharedImage> dest_shared_image,
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

  auto source_rect = use_visible_rect ? video_frame->visible_rect()
                                      : gfx::Rect(video_frame->coded_size());

  // This SharedImage will be written to (and later read from) via the raster
  // interface.
  CHECK(raster_context_provider->ContextCapabilities().gpu_rasterization);
  gpu::SharedImageUsageSet src_usage = gpu::SHARED_IMAGE_USAGE_RASTER_READ |
                                       gpu::SHARED_IMAGE_USAGE_RASTER_WRITE;

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

  std::unique_ptr<gpu::RasterScopedAccess> ri_access =
      src_shared_image->BeginRasterAccess(ri, si_sync_token,
                                          /*readonly=*/false);
  ri->WritePixelsYUV(src_shared_image->mailbox(), yuv_pixmap);
  gpu::SyncToken ri_sync_token =
      gpu::RasterScopedAccess::EndAccess(std::move(ri_access));

  std::unique_ptr<gpu::RasterScopedAccess> src_ri_access =
      src_shared_image->BeginRasterAccess(ri, ri_sync_token, /*readonly=*/true);
  std::unique_ptr<gpu::RasterScopedAccess> dst_ri_access =
      dest_shared_image->BeginRasterAccess(ri, dest_sync_token,
                                           /*readonly=*/false);
  ri->CopySharedImage(src_shared_image->mailbox(), dest_shared_image->mailbox(),
                      0, 0, source_rect.x(), source_rect.y(),
                      source_rect.width(), source_rect.height());
  gpu::RasterScopedAccess::EndAccess(std::move(dst_ri_access));
  ri_sync_token = gpu::RasterScopedAccess::EndAccess(std::move(src_ri_access));

  shared_image_cache->UpdateSyncToken(ri_sync_token);
  return ri_sync_token;
}

}  // namespace media::internals
