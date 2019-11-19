// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_GPU_MEMORY_BUFFER_TRACKER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_GPU_MEMORY_BUFFER_TRACKER_H_

#include "media/capture/video/chromeos/camera_buffer_factory.h"
#include "media/capture/video/video_capture_buffer_tracker.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace media {

// Tracker specifics for Chrome OS GpuMemoryBuffer.
class CAPTURE_EXPORT GpuMemoryBufferTracker final
    : public VideoCaptureBufferTracker {
 public:
  GpuMemoryBufferTracker();
  ~GpuMemoryBufferTracker() override;

  // Implementation of VideoCaptureBufferTracker:
  bool Init(const gfx::Size& dimensions,
            VideoPixelFormat format,
            const mojom::PlaneStridesPtr& strides) override;
  bool IsReusableForFormat(const gfx::Size& dimensions,
                           VideoPixelFormat format,
                           const mojom::PlaneStridesPtr& strides) override;
  uint32_t GetMemorySizeInBytes() override;
  std::unique_ptr<VideoCaptureBufferHandle> GetMemoryMappedAccess() override;
  base::UnsafeSharedMemoryRegion DuplicateAsUnsafeRegion() override;
  mojo::ScopedSharedBufferHandle DuplicateAsMojoBuffer() override;
  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle() override;

 private:
  CameraBufferFactory buffer_factory_;
  std::unique_ptr<gfx::GpuMemoryBuffer> buffer_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferTracker);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_GPU_MEMORY_BUFFER_TRACKER_H_
