// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_VIDEO_FRAME_YUV_CONVERTER_H_
#define MEDIA_RENDERERS_VIDEO_FRAME_YUV_CONVERTER_H_

#include <array>

#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/media_export.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace viz {
class RasterContextProvider;
}  // namespace viz

namespace media {

class VideoFrame;

// Converts YUV video frames to RGB format and stores the results in the
// provided mailbox. The caller of functions in this class maintains ownership
// of the destination mailbox. VideoFrames that wrap external textures can be
// I420 or NV12 format. Automatically handles upload of CPU memory backed
// VideoFrames in I420 format. Converting CPU backed VideoFrames requires
// creation of shared images to upload the frame to the GPU where the conversion
// takes place.
// IMPORTANT: Callers of this function can cache this class and call
// ConvertYUVVideoFrame() to prevent repeated creation/deletion of shared
// images.
class MEDIA_EXPORT VideoFrameYUVConverter {
 public:
  static void ConvertYUVVideoFrameNoCaching(
      const VideoFrame* video_frame,
      viz::RasterContextProvider* raster_context_provider,
      const gpu::MailboxHolder& dest_mailbox_holder);

  // TODO(crbug.com/1108154): Will merge this uploading path
  // with ConvertYUVVideoFrameYUVWithGrContext after solving
  // issue 1120911, 1120912
  static bool ConvertYUVVideoFrameWithSkSurfaceNoCaching(
      const VideoFrame* video_frame,
      viz::RasterContextProvider* raster_context_provider,
      const gpu::MailboxHolder& dest_mailbox_holder,
      unsigned int internal_format,
      unsigned int type,
      bool flip_y,
      bool use_visible_rect);

  VideoFrameYUVConverter();
  ~VideoFrameYUVConverter();

  void ConvertYUVVideoFrame(const VideoFrame* video_frame,
                            viz::RasterContextProvider* raster_context_provider,
                            const gpu::MailboxHolder& dest_mailbox_holder);
  void ReleaseCachedData();

 private:
  void ConvertFromVideoFrameYUVWithGrContext(
      const VideoFrame* video_frame,
      viz::RasterContextProvider* raster_context_provider,
      const gpu::MailboxHolder& dest_mailbox_holder);
  void ConvertFromVideoFrameYUVSkia(
      const VideoFrame* video_frame,
      viz::RasterContextProvider* raster_context_provider,
      unsigned int texture_target,
      unsigned int texture_id);
  bool ConvertYUVVideoFrameWithSkSurface(
      const VideoFrame* video_frame,
      viz::RasterContextProvider* raster_context_provider,
      const gpu::MailboxHolder& dest_mailbox_holder,
      unsigned int internal_format,
      unsigned int type,
      bool flip_y,
      bool use_visible_rect);

  class VideoFrameYUVMailboxesHolder;
  std::unique_ptr<VideoFrameYUVMailboxesHolder> holder_;
};
}  // namespace media

#endif  // MEDIA_RENDERERS_VIDEO_FRAME_YUV_CONVERTER_H_