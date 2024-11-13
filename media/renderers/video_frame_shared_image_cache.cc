// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/video_frame_shared_image_cache.h"

#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"

namespace media {

namespace {

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

VideoFrameSharedImageCache::VideoFrameSharedImageCache() = default;

VideoFrameSharedImageCache::~VideoFrameSharedImageCache() {
  ReleaseCachedData();
}

void VideoFrameSharedImageCache::ReleaseCachedData() {
  // Don't destroy shared image we don't own.
  if (!shared_image_) {
    return;
  }

  auto* ri = provider_->RasterInterface();
  DCHECK(ri);
  gpu::SyncToken token;
  ri->GenUnverifiedSyncTokenCHROMIUM(token.GetData());

  auto* sii = provider_->SharedImageInterface();
  DCHECK(sii);
  if (shared_image_) {
    sii->DestroySharedImage(token, std::move(shared_image_));
  }
}

const scoped_refptr<gpu::ClientSharedImage>&
VideoFrameSharedImageCache::GetSharedImage(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider) {
  // If we have cached shared image but the provider or video has changed we
  // need to release shared image created on the old context and recreate them.
  if (shared_image_ &&
      (provider_.get() != raster_context_provider ||
       video_frame->coded_size() != shared_image_->size() ||
       video_frame->ColorSpace() != shared_image_->color_space())) {
    ReleaseCachedData();
  }
  provider_ = raster_context_provider;
  CHECK(provider_);
  auto* ri = provider_->RasterInterface();
  CHECK(ri);
  auto* sii = provider_->SharedImageInterface();
  CHECK(sii);

  CHECK(!video_frame->HasSharedImage());

  // Create a multiplanar shared image to upload the data to, if one doesn't
  // exist already.
  if (!shared_image_) {
    // This SharedImage will be written to (and later read from) via the raster
    // interface. The full usage depends on whether raster is OOP or is going
    // over the GLES2 interface.
    gpu::SharedImageUsageSet mailbox_usage =
        gpu::SHARED_IMAGE_USAGE_RASTER_READ |
        gpu::SHARED_IMAGE_USAGE_RASTER_WRITE;
    auto& caps = provider_->ContextCapabilities();
    if (caps.gpu_rasterization) {
      mailbox_usage |= gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
    } else {
      // NOTE: This GLES2 usage is *only* for raster, as this SharedImage is
      // created to hold YUV data that is then converted to RGBA via the raster
      // interface before being shared with some other use case (e.g., WebGL).
      // There is no flow wherein this SharedImage is directly exposed to
      // WebGL. Moreover, this raster usage is by definition *only* over GLES2
      // (since this is non-OOP-R). It is critical to specify both of these
      // facts to the service side to ensure that the needed SharedImage backing
      // gets created (see crbug.com/328472684).
      mailbox_usage |= gpu::SHARED_IMAGE_USAGE_GLES2_READ |
                       gpu::SHARED_IMAGE_USAGE_GLES2_WRITE |
                       gpu::SHARED_IMAGE_USAGE_GLES2_FOR_RASTER_ONLY |
                       gpu::SHARED_IMAGE_USAGE_RASTER_OVER_GLES2_ONLY;
    }

    viz::SharedImageFormat format =
        VideoPixelFormatToSharedImageFormat(video_frame->format());
    CHECK(format.is_multi_plane());

    shared_image_ = sii->CreateSharedImage(
        {format, video_frame->coded_size(), video_frame->ColorSpace(),
         kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, mailbox_usage,
         "VideoFrameYUV"},
        gpu::kNullSurfaceHandle);
    CHECK(shared_image_);

    ri->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());
  }

  return shared_image_;
}

}  // namespace media
