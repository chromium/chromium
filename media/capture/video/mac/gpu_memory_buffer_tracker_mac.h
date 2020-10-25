// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_MAC_GPU_MEMORY_BUFFER_TRACKER_MAC_H_
#define MEDIA_CAPTURE_VIDEO_MAC_GPU_MEMORY_BUFFER_TRACKER_MAC_H_

#include "media/capture/video/video_capture_buffer_tracker.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/mac/io_surface.h"

namespace media {

class CAPTURE_EXPORT GpuMemoryBufferTrackerMac final
    : public VideoCaptureBufferTracker {
 public:
  GpuMemoryBufferTrackerMac();
  ~GpuMemoryBufferTrackerMac() override;

  // VideoCaptureBufferTracker
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
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferTrackerMac);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_MAC_GPU_MEMORY_BUFFER_TRACKER_MAC_H_