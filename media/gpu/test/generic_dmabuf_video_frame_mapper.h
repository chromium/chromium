// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_GENERIC_DMABUF_VIDEO_FRAME_MAPPER_H_
#define MEDIA_GPU_TEST_GENERIC_DMABUF_VIDEO_FRAME_MAPPER_H_

#include "media/gpu/test/video_frame_mapper.h"

namespace media {
namespace test {

// VideoFrameMapper for DMAbuf-backed VideoFrame.
class GenericDmaBufVideoFrameMapper : public VideoFrameMapper {
 public:
  GenericDmaBufVideoFrameMapper() = default;
  ~GenericDmaBufVideoFrameMapper() override = default;

  // VideoFrameMapper implementation.
  scoped_refptr<VideoFrame> Map(
      scoped_refptr<VideoFrame> video_frame) const override;
  DISALLOW_COPY_AND_ASSIGN(GenericDmaBufVideoFrameMapper);
};

}  // namespace test
}  // namespace media
#endif  // MEDIA_GPU_TEST_GENERIC_DMABUF_VIDEO_FRAME_MAPPER_H_
