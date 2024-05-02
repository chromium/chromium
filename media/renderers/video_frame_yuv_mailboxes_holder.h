// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_VIDEO_FRAME_YUV_MAILBOXES_HOLDER_H_
#define MEDIA_RENDERERS_VIDEO_FRAME_YUV_MAILBOXES_HOLDER_H_

#include "media/base/media_export.h"
#include "media/base/video_frame.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"

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

  // Extracts shared image information if |video_frame| is texture backed or
  // creates new shared images and uploads YUV data to GPU if |video_frame| is
  // mappable. This function can be called repeatedly to re-use shared images in
  // the case of CPU backed VideoFrames. The planes are returned in |mailboxes|.
  void VideoFrameToMailboxes(
      const VideoFrame* video_frame,
      viz::RasterContextProvider* raster_context_provider,
      gpu::Mailbox mailboxes[SkYUVAInfo::kMaxPlanes],
      bool allow_multiplanar_for_upload);

  const SkYUVAInfo& yuva_info() const { return yuva_info_; }

  // Utility to populate a SkYUVAInfo from a video frame.
  static SkYUVAInfo VideoFrameGetSkYUVAInfo(const VideoFrame* video_frame);

 private:
  static constexpr size_t kMaxPlanes =
      static_cast<size_t>(SkYUVAInfo::kMaxPlanes);

  scoped_refptr<viz::RasterContextProvider> provider_;
  bool created_shared_images_ = false;
  gfx::Size cached_video_size_;
  gfx::ColorSpace cached_video_color_space_;

  // The properties of the most recently received video frame.
  size_t num_planes_ = 0;
  SkYUVAInfo yuva_info_;
  SkISize plane_sizes_[SkYUVAInfo::kMaxPlanes];

  // Populated by VideoFrameToMailboxes.
  std::array<gpu::MailboxHolder, kMaxPlanes> holders_;
  std::array<scoped_refptr<gpu::ClientSharedImage>, kMaxPlanes> shared_images_;
};

}  // namespace media

#endif  // MEDIA_RENDERERS_VIDEO_FRAME_YUV_MAILBOXES_HOLDER_H_
