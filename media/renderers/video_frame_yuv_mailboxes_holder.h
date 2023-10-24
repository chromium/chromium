// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_VIDEO_FRAME_YUV_MAILBOXES_HOLDER_H_
#define MEDIA_RENDERERS_VIDEO_FRAME_YUV_MAILBOXES_HOLDER_H_

#include "media/base/media_export.h"
#include "media/base/video_frame.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"
#include "third_party/skia/include/gpu/GrYUVABackendTextures.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"

class SkColorSpace;
class SkImage;
class SkSurface;

namespace viz {
class RasterContextProvider;
}  // namespace viz

namespace media {

class MEDIA_EXPORT VideoFrameYUVMailboxesHolder {
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
      gpu::Mailbox mailboxes[SkYUVAInfo::kMaxPlanes],
      bool allow_multiplanar_for_upload);

  // Returns a YUV SkImage for the specified video frame. If
  // `reinterpret_color_space` is non-nullptr, then the SkImage will be
  // reinterpreted to be in the specified value. Otherwise, it will be
  // in `video_frame`'s color space.
  sk_sp<SkImage> VideoFrameToSkImage(
      const VideoFrame* video_frame,
      viz::RasterContextProvider* raster_context_provider,
      sk_sp<SkColorSpace> reinterpret_color_space);

  // Creates SkSurfaces for each plane for the specified video frame. Returns
  // true only if surfaces for all planes were created.
  bool VideoFrameToPlaneSkSurfaces(
      const VideoFrame* video_frame,
      viz::RasterContextProvider* raster_context_provider,
      sk_sp<SkSurface> surfaces[SkYUVAInfo::kMaxPlanes]);

  const SkYUVAInfo& yuva_info() const { return yuva_info_; }

  // Utility to convert a media pixel format to SkYUVAInfo.
  static std::tuple<SkYUVAInfo::PlaneConfig, SkYUVAInfo::Subsampling>
  VideoPixelFormatToSkiaValues(VideoPixelFormat video_format);

  // Utility to populate a SkYUVAInfo from a video frame.
  static SkYUVAInfo VideoFrameGetSkYUVAInfo(const VideoFrame* video_frame);

 private:
  static constexpr size_t kMaxPlanes =
      static_cast<size_t>(SkYUVAInfo::kMaxPlanes);

  // Like VideoFrameToMailboxes but imports the textures from the mailboxes and
  // returns the planes as a set of YUVA GrBackendTextures. If |for_surface| is
  // true, then select color types and pixel formats that are renderable as
  // SkSurfaces.
  GrYUVABackendTextures VideoFrameToSkiaTextures(
      const VideoFrame* video_frame,
      viz::RasterContextProvider* raster_context_provider,
      bool for_surface);

  void ImportTextures(bool for_surface);

  scoped_refptr<viz::RasterContextProvider> provider_;
  bool imported_textures_ = false;
  bool created_shared_images_ = false;
  gfx::Size cached_video_size_;
  gfx::ColorSpace cached_video_color_space_;

  // The properties of the most recently received video frame.
  size_t num_planes_ = 0;
  SkYUVAInfo yuva_info_;
  SkISize plane_sizes_[SkYUVAInfo::kMaxPlanes];

  // Populated by VideoFrameToMailboxes.
  std::array<gpu::MailboxHolder, kMaxPlanes> holders_;

  // Populated by ImportTextures.
  struct YUVPlaneTextureInfo {
    GrGLTextureInfo texture = {0, 0};
    bool is_shared_image = false;
  };
  std::array<YUVPlaneTextureInfo, kMaxPlanes> textures_;
};

}  // namespace media

#endif  // MEDIA_RENDERERS_VIDEO_FRAME_YUV_MAILBOXES_HOLDER_H_
