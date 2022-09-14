// Copyright 2020 The Chromium Authors
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
  explicit GpuMemoryBufferTrackerMac(
      base::ScopedCFTypeRef<IOSurfaceRef> io_surface);

  GpuMemoryBufferTrackerMac(const GpuMemoryBufferTrackerMac&) = delete;
  GpuMemoryBufferTrackerMac& operator=(const GpuMemoryBufferTrackerMac&) =
      delete;

  ~GpuMemoryBufferTrackerMac() override;

  // VideoCaptureBufferTracker
  bool Init(const gfx::Size& dimensions,
            VideoPixelFormat format,
            const mojom::PlaneStridesPtr& strides) override;
  bool IsSameGpuMemoryBuffer(
      const gfx::GpuMemoryBufferHandle& handle) const override;
  bool IsReusableForFormat(const gfx::Size& dimensions,
                           VideoPixelFormat format,
                           const mojom::PlaneStridesPtr& strides) override;
  uint32_t GetMemorySizeInBytes() override;
  std::unique_ptr<VideoCaptureBufferHandle> GetMemoryMappedAccess() override;
  base::UnsafeSharedMemoryRegion DuplicateAsUnsafeRegion() override;
  mojo::ScopedSharedBufferHandle DuplicateAsMojoBuffer() override;
  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle() override;
  void OnHeldByConsumersChanged(bool is_held_by_consumers) override;

 private:
  bool is_external_io_surface_ = false;
  gfx::ScopedIOSurface io_surface_;

  // External IOSurfaces come from a CVPixelBufferPool. An IOSurface in a
  // CVPixelBufferPool will be reused by the pool as soon IOSurfaceIsInUse is
  // false. To prevent reuse while consumers are accessing the IOSurface, use
  // |in_use_for_consumers_| to maintain IOSurfaceIsInUse as true.
  gfx::ScopedInUseIOSurface in_use_for_consumers_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_MAC_GPU_MEMORY_BUFFER_TRACKER_MAC_H_
