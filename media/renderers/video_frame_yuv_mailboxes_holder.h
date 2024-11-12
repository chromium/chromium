// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_VIDEO_FRAME_YUV_MAILBOXES_HOLDER_H_
#define MEDIA_RENDERERS_VIDEO_FRAME_YUV_MAILBOXES_HOLDER_H_

#include "media/base/media_export.h"
#include "media/base/video_frame.h"

namespace gpu {
class ClientSharedImage;
}  // namespace gpu

namespace viz {
class RasterContextProvider;
}  // namespace viz

namespace media {

class MEDIA_EXPORT VideoFrameYUVMailboxesHolder {
 public:
  VideoFrameYUVMailboxesHolder();
  ~VideoFrameYUVMailboxesHolder();

  void ReleaseCachedData();

  // Creates a new shared image if the `video_frame` data differs from that in
  // `shared_image_`. This function can be called repeatedly to re-use
  // `shared_image_` in the case of CPU backed VideoFrames.
  const scoped_refptr<gpu::ClientSharedImage>& GetSharedImage(
      const VideoFrame* video_frame,
      viz::RasterContextProvider* raster_context_provider);

 private:
  scoped_refptr<viz::RasterContextProvider> provider_;

  // Populated by GetSharedImage.
  scoped_refptr<gpu::ClientSharedImage> shared_image_;
};

}  // namespace media

#endif  // MEDIA_RENDERERS_VIDEO_FRAME_YUV_MAILBOXES_HOLDER_H_
