// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_VIDEO_FRAME_SHARED_IMAGE_CACHE_H_
#define MEDIA_RENDERERS_VIDEO_FRAME_SHARED_IMAGE_CACHE_H_

#include "media/base/media_export.h"
#include "media/base/video_frame.h"

namespace gpu {
class ClientSharedImage;
}  // namespace gpu

namespace viz {
class RasterContextProvider;
}  // namespace viz

namespace media {

class MEDIA_EXPORT VideoFrameSharedImageCache {
 public:
  // Returns the cache Status on interaction with VideoFrameSharedImageCache.
  // Specifies whether a new SharedImage was created or not based on VideoFrame
  // id and SharedImage data.
  enum class Status {
    // The cached VideoFrame id matched.
    kMatchedVideoFrameId,
    // The cached SharedImage metadata (size, usage etc) matched.
    kMatchedSharedImageMetaData,
    // Mismatch of id/data, new SharedImage created.
    kCreatedNewSharedImage,
  };

  struct CachedData {
    CachedData(scoped_refptr<gpu::ClientSharedImage> shared_image,
               Status status);
    ~CachedData();

    scoped_refptr<gpu::ClientSharedImage> shared_image;
    Status status;
  };

  VideoFrameSharedImageCache();
  ~VideoFrameSharedImageCache();

  void ReleaseCachedData();

  // Creates a new shared image if the `video_frame` data differs from that in
  // `shared_image_`. This function can be called repeatedly to re-use
  // `shared_image_` in the case of CPU backed VideoFrames. Returns the
  // `shared_image_` along with Status on whether the shared image was created
  // or reused.
  CachedData GetSharedImage(const VideoFrame* video_frame,
                            viz::RasterContextProvider* raster_context_provider,
                            gpu::SharedImageUsageSet usage);

 private:
  scoped_refptr<viz::RasterContextProvider> provider_;

  // Populated by GetSharedImage.
  scoped_refptr<gpu::ClientSharedImage> shared_image_;
  VideoFrame::ID video_frame_id_;
};

}  // namespace media

#endif  // MEDIA_RENDERERS_VIDEO_FRAME_SHARED_IMAGE_CACHE_H_
