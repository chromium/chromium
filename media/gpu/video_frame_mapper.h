// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VIDEO_FRAME_MAPPER_H_
#define MEDIA_GPU_VIDEO_FRAME_MAPPER_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// The VideoFrameMapper interface allows mapping video frames into memory so
// that their contents can be accessed directly.
// VideoFrameMapper should be created by using VideoFrameMapperFactory.
class MEDIA_GPU_EXPORT VideoFrameMapper {
 public:
  virtual ~VideoFrameMapper() = default;

  // Maps data referred by |video_frame| and creates a VideoFrame whose dtor
  // unmap the mapped memory.
  virtual scoped_refptr<VideoFrame> Map(
      scoped_refptr<const VideoFrame> video_frame) const = 0;

  // Returns the allowed pixel format of video frames on Map().
  VideoPixelFormat pixel_format() const { return format_; }

 protected:
  explicit VideoFrameMapper(VideoPixelFormat format) : format_(format) {}

  // The allowed pixel format of video frames on Map().
  VideoPixelFormat format_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameMapper);
};

}  // namespace media

#endif  // MEDIA_GPU_VIDEO_FRAME_MAPPER_H_
