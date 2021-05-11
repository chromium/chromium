// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_VIDEO_FRAME_YUV_MAILBOXES_HOLDER_H_
#define MEDIA_RENDERERS_VIDEO_FRAME_YUV_MAILBOXES_HOLDER_H_

#include "media/base/video_frame.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"
#include "third_party/skia/include/gpu/GrYUVABackendTextures.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"

namespace viz {
class RasterContextProvider;
}  // namespace viz

namespace media {

class VideoFrameYUVMailboxesHolder {
 public:
  VideoFrameYUVMailboxesHolder();
  ~VideoFrameYUVMailboxesHolder();

  void ReleaseCachedData();
  void ReleaseTextures();

  // Extracts shared image information if |video_frame| is texture backed or
  // creates new shared images and uploads YUV data to GPU if |video_frame| is
  // mappable. This function can be called repeatedly to re-use shared images in
  // the case of CPU backed VideoFrames. The planes are returned in |mailboxes|.
  void VideoFrameToMailboxes(
      const VideoFrame* video_frame,
      viz::RasterContextProvider* raster_context_provider,
      gpu::Mailbox mailboxes[]);

  // Like VideoFrameToMailboxes but imports the textures from the mailboxes and
  // returns the planes as a set of YUVA GrBackendTextures.
  GrYUVABackendTextures VideoFrameToSkiaTextures(
      const VideoFrame* video_frame,
      viz::RasterContextProvider* raster_context_provider);

  SkYUVAPixmaps VideoFrameToSkiaPixmaps(const VideoFrame* video_frame);

  SkYUVAInfo::PlaneConfig plane_config() const { return plane_config_; }

  SkYUVAInfo::Subsampling subsampling() const { return subsampling_; }

  // Utility to convert a gfx::ColorSpace to a SkYUVColorSpace.
  static SkYUVColorSpace ColorSpaceToSkYUVColorSpace(
      const gfx::ColorSpace& color_space);

  // Utility to convert a media pixel format to SkYUVAInfo.
  static std::tuple<SkYUVAInfo::PlaneConfig, SkYUVAInfo::Subsampling>
  VideoPixelFormatToSkiaValues(VideoPixelFormat video_format);

 private:
  static constexpr size_t kMaxPlanes =
      static_cast<size_t>(SkYUVAInfo::kMaxPlanes);

  void ImportTextures();
  size_t NumPlanes() {
    return static_cast<size_t>(SkYUVAInfo::NumPlanes(plane_config_));
  }

  scoped_refptr<viz::RasterContextProvider> provider_;
  bool imported_textures_ = false;
  SkYUVAInfo::PlaneConfig plane_config_ = SkYUVAInfo::PlaneConfig::kUnknown;
  SkYUVAInfo::Subsampling subsampling_ = SkYUVAInfo::Subsampling::kUnknown;
  bool created_shared_images_ = false;
  gfx::Size cached_video_size_;
  gfx::ColorSpace cached_video_color_space_;
  std::array<gpu::MailboxHolder, kMaxPlanes> holders_;

  struct YUVPlaneTextureInfo {
    GrGLTextureInfo texture = {0, 0};
    bool is_shared_image = false;
  };
  std::array<YUVPlaneTextureInfo, kMaxPlanes> textures_;
};

}  // namespace media

#endif  // MEDIA_RENDERERS_VIDEO_FRAME_YUV_MAILBOXES_HOLDER_H_
