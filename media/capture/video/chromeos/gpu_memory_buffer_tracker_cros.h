// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_GPU_MEMORY_BUFFER_TRACKER_CROS_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_GPU_MEMORY_BUFFER_TRACKER_CROS_H_

#include "media/capture/video/chromeos/camera_buffer_factory.h"
#include "media/capture/video/video_capture_buffer_tracker.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace gpu {
class ClientSharedImage;
}

namespace media {

// Tracker specifics for Chrome OS GpuMemoryBuffer.
class CAPTURE_EXPORT GpuMemoryBufferTrackerCros final
    : public VideoCaptureBufferTracker {
 public:
  GpuMemoryBufferTrackerCros();

  GpuMemoryBufferTrackerCros(const GpuMemoryBufferTrackerCros&) = delete;
  GpuMemoryBufferTrackerCros& operator=(const GpuMemoryBufferTrackerCros&) =
      delete;

  ~GpuMemoryBufferTrackerCros() override;

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
  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle() override;

  VideoCaptureBufferType GetBufferType() override;

 private:
  CameraBufferFactory buffer_factory_;
  scoped_refptr<gpu::ClientSharedImage> shared_image_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_GPU_MEMORY_BUFFER_TRACKER_CROS_H_
