// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VIDEO_FRAME_MAPPER_H_
#define MEDIA_GPU_VIDEO_FRAME_MAPPER_H_

#include "base/memory/scoped_refptr.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/gpu/chromeos/frame_resource.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// The VideoFrameMapper interface allows mapping video frames into memory so
// that their contents can be accessed directly.
// VideoFrameMapper should be created by using VideoFrameMapperFactory.
class MEDIA_GPU_EXPORT VideoFrameMapper {
 public:
  VideoFrameMapper(const VideoFrameMapper&) = delete;
  VideoFrameMapper& operator=(const VideoFrameMapper&) = delete;

  virtual ~VideoFrameMapper() = default;

  // Maps data referred by |video_frame| and creates a VideoFrame whose dtor
  // unmap the mapped memory. The |permissions| parameter is a bitwise OR of the
  // permissions the mapping needs if it uses mmap. Valid flags for this
  // parameter are combinations of |PROT_READ| and |PROT_WRITE|. This doesn't
  // map into a FrameResource. Callers can wrap this with a VideoFrameResource
  // if needed.
  virtual scoped_refptr<VideoFrame> MapFrame(
      scoped_refptr<const FrameResource> video_frame,
      int permissions) = 0;

  // VideoFrame version of Map(). This wraps the VideoFrame in a FrameResource
  // and calls Map(scoped_refpr<FrameResource> ...).
  scoped_refptr<VideoFrame> Map(scoped_refptr<const VideoFrame> video_frame,
                                int permissions);

  // Returns the allowed pixel format of video frames on Map().
  VideoPixelFormat pixel_format() const { return format_; }

 protected:
  explicit VideoFrameMapper(VideoPixelFormat format) : format_(format) {}

  // The allowed pixel format of video frames on Map().
  VideoPixelFormat format_;
};

}  // namespace media

#endif  // MEDIA_GPU_VIDEO_FRAME_MAPPER_H_
