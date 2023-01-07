// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_GPU_MEMORY_BUFFER_UTILS_H_
#define MEDIA_CAPTURE_VIDEO_GPU_MEMORY_BUFFER_UTILS_H_

#include <memory>

#include "media/capture/video/video_capture_device.h"

namespace gfx {
class GpuMemoryBuffer;
}  // namespace gfx

namespace gpu {
class GpuMemoryBufferSupport;
}  // namespace gpu

// Utility class and function for creating and accessing video capture client
// buffers backed with GpuMemoryBuffer buffers.
namespace media {

class ScopedNV12GpuMemoryBufferMapping {
 public:
  explicit ScopedNV12GpuMemoryBufferMapping(
      std::unique_ptr<gfx::GpuMemoryBuffer> gmb);
  ~ScopedNV12GpuMemoryBufferMapping();
  uint8_t* y_plane();
  uint8_t* uv_plane();
  size_t y_stride();
  size_t uv_stride();

 private:
  std::unique_ptr<gfx::GpuMemoryBuffer> gmb_;
};

VideoCaptureDevice::Client::ReserveResult AllocateNV12GpuMemoryBuffer(
    VideoCaptureDevice::Client* capture_client,
    const gfx::Size& buffer_size,
    gpu::GpuMemoryBufferSupport* gmb_support,
    std::unique_ptr<gfx::GpuMemoryBuffer>* out_gpu_memory_buffer,
    VideoCaptureDevice::Client::Buffer* out_capture_buffer);

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_GPU_MEMORY_BUFFER_UTILS_H_
