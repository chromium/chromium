// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_FRAME_MAPPER_H_
#define MEDIA_GPU_TEST_VIDEO_FRAME_MAPPER_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "media/base/video_frame.h"

namespace media {
namespace test {

// VideoFrameMapper is a class for mapping a video frame referred by VideoFrame.
// VideoFrameMapper should be created by using VideoFrameMapperFactory.
class VideoFrameMapper {
 public:
  virtual ~VideoFrameMapper() = default;

  // Maps data referred by |video_frame| and creates a VideoFrame whose dtor
  // unmap the mapped memory.
  virtual scoped_refptr<VideoFrame> Map(
      scoped_refptr<VideoFrame> video_frame) const = 0;

 protected:
  VideoFrameMapper() = default;
  DISALLOW_COPY_AND_ASSIGN(VideoFrameMapper);
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_FRAME_MAPPER_H_
