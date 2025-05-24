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
#include "gpu/command_buffer/common/sync_token.h"

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

  auto* sii = provider_->SharedImageInterface();
  DCHECK(sii);
  if (shared_image_) {
    sii->DestroySharedImage(sync_token_, std::move(shared_image_));
  }
}

VideoFrameSharedImageCache::CachedData
VideoFrameSharedImageCache::GetOrCreateSharedImage(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    const gpu::SharedImageUsageSet& usage) {
  viz::SharedImageFormat format =
      VideoPixelFormatToSharedImageFormat(video_frame->format());
  CHECK(format.is_multi_plane());
  return GetOrCreateSharedImage(video_frame, raster_context_provider, usage,
                                format, video_frame->ColorSpace());
}

VideoFrameSharedImageCache::CachedData
VideoFrameSharedImageCache::GetOrCreateSharedImage(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    const gpu::SharedImageUsageSet& usage,
    const viz::SharedImageFormat& format,
    const gfx::ColorSpace& color_space) {
  if (shared_image_ && provider_ == raster_context_provider) {
    // Return the cached shared image if it is the same video frame.
    if (video_frame_id_ == video_frame->unique_id()) {
      return {shared_image_, sync_token_, Status::kMatchedVideoFrameId};
    }
    // Return the cached shared image if the video frame data matches the shared
    // image data.
    if (video_frame->coded_size() == shared_image_->size() &&
        color_space == shared_image_->color_space() &&
        format == shared_image_->format() && usage == shared_image_->usage()) {
      return {shared_image_, sync_token_, Status::kMatchedSharedImageMetaData};
    }
  }

  // If we have cached shared image but the provider or video has changed we
  // need to release shared image created on the old context and recreate them.
  ReleaseCachedData();
  provider_ = raster_context_provider;
  CHECK(provider_);
  auto* sii = provider_->SharedImageInterface();
  CHECK(sii);

  shared_image_ = sii->CreateSharedImage(
      {format, video_frame->coded_size(), color_space, kTopLeft_GrSurfaceOrigin,
       kUnpremul_SkAlphaType, usage, "VideoFrameSharedImageCache"},
      gpu::kNullSurfaceHandle);
  CHECK(shared_image_);
  video_frame_id_ = video_frame->unique_id();
  sync_token_ = sii->GenUnverifiedSyncToken();

  return {shared_image_, sync_token_, Status::kCreatedNewSharedImage};
}

void VideoFrameSharedImageCache::UpdateSyncToken(
    const gpu::SyncToken& sync_token) {
  sync_token_ = sync_token;
}

VideoFrameSharedImageCache::CachedData::CachedData(
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    const gpu::SyncToken& sync_token,
    Status status)
    : shared_image(std::move(shared_image)),
      sync_token(sync_token),
      status(status) {}
VideoFrameSharedImageCache::CachedData::~CachedData() = default;

}  // namespace media
