// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_LINUX_GPU_MEMORY_BUFFER_VIDEO_FRAME_MAPPER_H_
#define MEDIA_GPU_LINUX_GPU_MEMORY_BUFFER_VIDEO_FRAME_MAPPER_H_

#include "media/gpu/media_gpu_export.h"
#include "media/gpu/video_frame_mapper.h"

namespace media {

// The GpuMemoryBufferVideoFrameMapper implements functionality to map
// GpuMemoryBuffer-based video frames into memory.
class MEDIA_GPU_EXPORT GpuMemoryBufferVideoFrameMapper
    : public VideoFrameMapper {
 public:
  static std::unique_ptr<GpuMemoryBufferVideoFrameMapper> Create(
      VideoPixelFormat format);

  ~GpuMemoryBufferVideoFrameMapper() override = default;

  // VideoFrameMapper implementation.
  scoped_refptr<VideoFrame> Map(
      scoped_refptr<const VideoFrame> video_frame) const override;

 private:
  explicit GpuMemoryBufferVideoFrameMapper(VideoPixelFormat format);

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferVideoFrameMapper);
};

}  // namespace media
#endif  // MEDIA_GPU_LINUX_GPU_MEMORY_BUFFER_VIDEO_FRAME_MAPPER_H_
