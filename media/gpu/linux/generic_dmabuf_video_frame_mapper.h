// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_LINUX_GENERIC_DMABUF_VIDEO_FRAME_MAPPER_H_
#define MEDIA_GPU_LINUX_GENERIC_DMABUF_VIDEO_FRAME_MAPPER_H_

#include "media/gpu/media_gpu_export.h"
#include "media/gpu/video_frame_mapper.h"

namespace media {

// The GenericDmaBufVideoFrameMapper implements functionality to map DMABUF-
// backed video frames into memory.
class MEDIA_GPU_EXPORT GenericDmaBufVideoFrameMapper : public VideoFrameMapper {
 public:
  static std::unique_ptr<GenericDmaBufVideoFrameMapper> Create(
      VideoPixelFormat format);

  ~GenericDmaBufVideoFrameMapper() override = default;
  // VideoFrameMapper implementation.
  scoped_refptr<VideoFrame> Map(
      scoped_refptr<const VideoFrame> video_frame) const override;

 private:
  explicit GenericDmaBufVideoFrameMapper(VideoPixelFormat format);

  DISALLOW_COPY_AND_ASSIGN(GenericDmaBufVideoFrameMapper);
};

}  // namespace media
#endif  // MEDIA_GPU_LINUX_GENERIC_DMABUF_VIDEO_FRAME_MAPPER_H_
