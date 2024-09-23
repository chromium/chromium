// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_GPU_MEMORY_BUFFER_VIDEO_FRAME_MAPPER_H_
#define MEDIA_GPU_CHROMEOS_GPU_MEMORY_BUFFER_VIDEO_FRAME_MAPPER_H_

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

  GpuMemoryBufferVideoFrameMapper(const GpuMemoryBufferVideoFrameMapper&) =
      delete;
  GpuMemoryBufferVideoFrameMapper& operator=(
      const GpuMemoryBufferVideoFrameMapper&) = delete;

  ~GpuMemoryBufferVideoFrameMapper() override = default;

  // VideoFrameMapper implementation.
  scoped_refptr<VideoFrame> MapFrame(
      scoped_refptr<const FrameResource> video_frame,
      int permissions) override;

 private:
  explicit GpuMemoryBufferVideoFrameMapper(VideoPixelFormat format);
};

}  // namespace media
#endif  // MEDIA_GPU_CHROMEOS_GPU_MEMORY_BUFFER_VIDEO_FRAME_MAPPER_H_
