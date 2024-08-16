// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/renderers/video_frame_yuv_mailboxes_holder.h"

#include <GLES3/gl3.h>

#include "base/logging.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "media/base/media_switches.h"
#include "media/base/video_util.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkYUVAPixmaps.h"

namespace media {

namespace {

viz::SharedImageFormat PlaneSharedImageFormat(int num_channels,
                                              bool supports_red) {
  switch (num_channels) {
    case 1:
      return supports_red ? viz::SinglePlaneFormat::kR_8
                          : viz::SinglePlaneFormat::kLUMINANCE_8;
    case 2:
      return viz::SinglePlaneFormat::kRG_88;
    case 3:
      return viz::SinglePlaneFormat::kRGBX_8888;
    case 4:
      return viz::SinglePlaneFormat::kRGBA_8888;
  }
  NOTREACHED();
}

// Returns multiplanar format equivalent of a VideoPixelFormat.
viz::SharedImageFormat VideoPixelFormatToSharedImageFormat(
    VideoPixelFormat video_format) {
  switch (video_format) {
    case PIXEL_FORMAT_NV12:
      return viz::MultiPlaneFormat::kNV12;
    case PIXEL_FORMAT_NV16:
      return viz::MultiPlaneFormat::kNV16;
    case PIXEL_FORMAT_NV24:
      return viz::MultiPlaneFormat::kNV24;
    case PIXEL_FORMAT_NV12A:
      return viz::MultiPlaneFormat::kNV12A;
    case PIXEL_FORMAT_P010LE:
      return viz::MultiPlaneFormat::kP010;
    case PIXEL_FORMAT_P210LE:
      return viz::MultiPlaneFormat::kP210;
    case PIXEL_FORMAT_P410LE:
      return viz::MultiPlaneFormat::kP410;
    case PIXEL_FORMAT_I420:
      return viz::MultiPlaneFormat::kI420;
    case PIXEL_FORMAT_I420A:
      return viz::MultiPlaneFormat::kI420A;
    default:
      NOTREACHED();
  }
}

}  // namespace

VideoFrameYUVMailboxesHolder::VideoFrameYUVMailboxesHolder() = default;

VideoFrameYUVMailboxesHolder::~VideoFrameYUVMailboxesHolder() {
  ReleaseCachedData();
}

void VideoFrameYUVMailboxesHolder::ReleaseCachedData() {
  if (holders_[0].mailbox.IsZero())
    return;

  // Don't destroy shared images we don't own.
  if (!created_shared_images_)
    return;

  auto* ri = provider_->RasterInterface();
  DCHECK(ri);
  gpu::SyncToken token;
  ri->GenUnverifiedSyncTokenCHROMIUM(token.GetData());

  auto* sii = provider_->SharedImageInterface();
  DCHECK(sii);
  for (unsigned int i = 0; i < kMaxPlanes; ++i) {
    if (shared_images_[i]) {
      sii->DestroySharedImage(token, std::move(shared_images_[i]));
    }
    holders_[i].mailbox.SetZero();
  }

  created_shared_images_ = false;
}

void VideoFrameYUVMailboxesHolder::VideoFrameToMailboxes(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    gpu::Mailbox mailboxes[SkYUVAInfo::kMaxPlanes],
    bool allow_multiplanar_for_upload) {
  yuva_info_ = VideoFrameGetSkYUVAInfo(video_frame);
  num_planes_ = yuva_info_.planeDimensions(plane_sizes_);

  // If we have cached shared images but the provider or video has changed we
  // need to release shared images created on the old context and recreate them.
  if (created_shared_images_ &&
      (provider_.get() != raster_context_provider ||
       video_frame->coded_size() != cached_video_size_ ||
       video_frame->ColorSpace() != cached_video_color_space_)) {
    ReleaseCachedData();
  }
  provider_ = raster_context_provider;
  DCHECK(provider_);
  auto* ri = provider_->RasterInterface();
  DCHECK(ri);

  if (video_frame->HasTextures()) {
    // Video frames with mailboxes will have shared images per plane as new
    // multiplanar shared image with mailbox path should not go through
    // VideoFrameToMailboxes.
    DCHECK_EQ(num_planes_, video_frame->NumTextures());
    for (size_t plane = 0; plane < video_frame->NumTextures(); ++plane) {
      holders_[plane] = video_frame->mailbox_holder(plane);
      DCHECK(holders_[plane].texture_target == GL_TEXTURE_2D ||
             holders_[plane].texture_target == GL_TEXTURE_EXTERNAL_OES ||
             holders_[plane].texture_target == GL_TEXTURE_RECTANGLE_ARB)
          << "Unsupported texture target " << std::hex << std::showbase
          << holders_[plane].texture_target;
      ri->WaitSyncTokenCHROMIUM(holders_[plane].sync_token.GetConstData());
      mailboxes[plane] = holders_[plane].mailbox;
    }
    return;
  }

  CHECK(!video_frame->HasTextures());
  constexpr SkAlphaType kPlaneAlphaType = kPremul_SkAlphaType;
  auto* sii = provider_->SharedImageInterface();
  DCHECK(sii);

  // These SharedImages will be written to (and later read from) via the raster
  // interface. The full usage depends on whether raster is OOP or is going
  // over the GLES2 interface.
  gpu::SharedImageUsageSet mailbox_usage = gpu::SHARED_IMAGE_USAGE_RASTER_READ |
                                           gpu::SHARED_IMAGE_USAGE_RASTER_WRITE;
  auto& caps = provider_->ContextCapabilities();
  if (caps.gpu_rasterization) {
    mailbox_usage |= gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
  } else {
    // NOTE: This GLES2 usage is *only* for raster, as these SharedImages are
    // created to hold YUV data that is then converted to RGBA via the raster
    // interface before being shared with some other use case (e.g., WebGL).
    // There is no flow wherein these SharedImages are directly exposed to
    // WebGL. Moreover, this raster usage is by definition *only* over GLES2
    // (since this is non-OOP-R). It is critical to specify both of these facts
    // to the service side to ensure that the needed SharedImage backing gets
    // created (see crbug.com/328472684).
    mailbox_usage |= gpu::SHARED_IMAGE_USAGE_GLES2_READ |
                     gpu::SHARED_IMAGE_USAGE_GLES2_WRITE |
                     gpu::SHARED_IMAGE_USAGE_GLES2_FOR_RASTER_ONLY |
                     gpu::SHARED_IMAGE_USAGE_RASTER_OVER_GLES2_ONLY;
  }

  // Enabled with flags UseWritePixelsYUV and
  // UseMultiPlaneFormatForHardwareVideo.
  if (allow_multiplanar_for_upload) {
    SkPixmap pixmaps[SkYUVAInfo::kMaxPlanes] = {};
    viz::SharedImageFormat format =
        VideoPixelFormatToSharedImageFormat(video_frame->format());
    CHECK(format.is_multi_plane());

    // Create a multiplanar shared image to upload the data to, if one doesn't
    // exist already.
    if (!created_shared_images_) {
      auto client_shared_image = sii->CreateSharedImage(
          {format, video_frame->coded_size(), video_frame->ColorSpace(),
           kTopLeft_GrSurfaceOrigin, kPlaneAlphaType, mailbox_usage,
           "VideoFrameYUV"},
          gpu::kNullSurfaceHandle);
      CHECK(client_shared_image);
      holders_[0].mailbox = client_shared_image->mailbox();
      holders_[0].texture_target = GL_TEXTURE_2D;
      shared_images_[0] = std::move(client_shared_image);

      // Split up shared image creation from upload so we only have to wait on
      // one sync token.
      ri->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());

      cached_video_size_ = video_frame->coded_size();
      cached_video_color_space_ = video_frame->ColorSpace();
      created_shared_images_ = true;
    }

    for (size_t plane = 0; plane < num_planes_; ++plane) {
      SkColorType color_type =
          viz::ToClosestSkColorType(/*gpu_compositing=*/true, format, plane);
      SkImageInfo info =
          SkImageInfo::Make(plane_sizes_[plane], color_type, kPlaneAlphaType);
      pixmaps[plane] =
          SkPixmap(info, video_frame->data(plane), video_frame->stride(plane));
    }
    SkYUVAPixmaps yuv_pixmap =
        SkYUVAPixmaps::FromExternalPixmaps(yuva_info_, pixmaps);
    ri->WritePixelsYUV(holders_[0].mailbox, yuv_pixmap);
    mailboxes[0] = holders_[0].mailbox;
    return;
  }

  // Create shared images to upload the data to, if they doesn't exist already.
  if (!created_shared_images_) {
    for (size_t plane = 0; plane < num_planes_; ++plane) {
      gfx::Size tex_size = {plane_sizes_[plane].width(),
                            plane_sizes_[plane].height()};
      int num_channels = yuva_info_.numChannelsInPlane(plane);
      viz::SharedImageFormat format =
          PlaneSharedImageFormat(num_channels, caps.texture_rg);
      auto client_shared_image =
          sii->CreateSharedImage({format, tex_size, video_frame->ColorSpace(),
                                  kTopLeft_GrSurfaceOrigin, kPlaneAlphaType,
                                  mailbox_usage, "VideoFrameYUV"},
                                 gpu::kNullSurfaceHandle);
      CHECK(client_shared_image);
      holders_[plane].mailbox = client_shared_image->mailbox();
      holders_[plane].texture_target = GL_TEXTURE_2D;
      shared_images_[plane] = std::move(client_shared_image);
    }

    // Split up shared image creation from upload so we only have to wait on
    // one sync token.
    ri->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());

    cached_video_size_ = video_frame->coded_size();
    cached_video_color_space_ = video_frame->ColorSpace();
    created_shared_images_ = true;
  }

  for (size_t plane = 0; plane < num_planes_; ++plane) {
    int num_channels = yuva_info_.numChannelsInPlane(plane);
    SkColorType color_type = SkYUVAPixmapInfo::DefaultColorTypeForDataType(
        SkYUVAPixmaps::DataType::kUnorm8, num_channels);
    SkImageInfo info =
        SkImageInfo::Make(plane_sizes_[plane], color_type, kPlaneAlphaType);
    ri->WritePixels(
        holders_[plane].mailbox, /*dst_x_offset=*/0, /*dst_y_offset=*/0,
        GL_TEXTURE_2D,
        SkPixmap(info, video_frame->data(plane), video_frame->stride(plane)));
    mailboxes[plane] = holders_[plane].mailbox;
  }
}

// static
SkYUVAInfo VideoFrameYUVMailboxesHolder::VideoFrameGetSkYUVAInfo(
    const VideoFrame* video_frame) {
  SkISize video_size{video_frame->coded_size().width(),
                     video_frame->coded_size().height()};
  auto plane_config = SkYUVAInfo::PlaneConfig::kUnknown;
  auto subsampling = SkYUVAInfo::Subsampling::kUnknown;
  std::tie(plane_config, subsampling) =
      VideoPixelFormatToSkiaValues(video_frame->format());

  // TODO(crbug.com/41380578): This should really default to rec709.
  SkYUVColorSpace color_space = kRec601_SkYUVColorSpace;
  video_frame->ColorSpace().ToSkYUVColorSpace(video_frame->BitDepth(),
                                              &color_space);
  return SkYUVAInfo(video_size, plane_config, subsampling, color_space);
}

}  // namespace media
