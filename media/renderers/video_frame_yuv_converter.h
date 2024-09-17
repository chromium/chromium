// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_VIDEO_FRAME_YUV_CONVERTER_H_
#define MEDIA_RENDERERS_VIDEO_FRAME_YUV_CONVERTER_H_

#include <array>

#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/media_export.h"

namespace viz {
class RasterContextProvider;
}  // namespace viz

namespace media {

class VideoFrame;
class VideoFrameYUVMailboxesHolder;

// Converts YUV video frames to RGB format and stores the results in the
// provided mailbox. The caller of functions in this class maintains ownership
// of the destination mailbox. VideoFrames that wrap external textures can be
// I420 or NV12 format. Automatically handles upload of CPU memory backed
// VideoFrames in I420 format. Converting CPU backed VideoFrames requires
// creation of shared images to upload the frame to the GPU where the conversion
// takes place. This will not perform any color space conversion besides the
// YUV to RGB conversion (it will ignore the color space of the SharedImage
// backing the destination mailbox).
// IMPORTANT: Callers of this function can cache this class and call
// ConvertYUVVideoFrame() to prevent repeated creation/deletion of shared
// images.
class MEDIA_EXPORT VideoFrameYUVConverter {
 public:
  // These parameters are only supported by ConvertYUVVideoFrame et al when the
  // specified RasterContextProvider also has a GrContext (equivalently, when
  // OOP-R is disabled). Isolate them in their own structure, so they can
  // eventually be removed once OOP-R is universal.
  struct GrParams {
    unsigned int internal_format = GL_RGBA;
    unsigned int type = GL_UNSIGNED_BYTE;
    bool flip_y = false;
    bool use_visible_rect = false;
  };

  VideoFrameYUVConverter();
  ~VideoFrameYUVConverter();
  static bool IsVideoFrameFormatSupported(const VideoFrame& video_frame);

  // For pure software pixel upload path with video frame that does not have
  // textures.
  bool ConvertYUVVideoFrame(const VideoFrame* video_frame,
                            viz::RasterContextProvider* raster_context_provider,
                            const gpu::MailboxHolder& dest_mailbox_holder,
                            std::optional<GrParams> gr_params = std::nullopt);
  void ReleaseCachedData();

 private:
  std::unique_ptr<VideoFrameYUVMailboxesHolder> holder_;
};
}  // namespace media

#endif  // MEDIA_RENDERERS_VIDEO_FRAME_YUV_CONVERTER_H_
