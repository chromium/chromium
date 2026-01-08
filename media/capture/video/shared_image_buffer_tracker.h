// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_SHARED_IMAGE_BUFFER_TRACKER_H_
#define MEDIA_CAPTURE_VIDEO_SHARED_IMAGE_BUFFER_TRACKER_H_

#include "media/capture/video/video_capture_buffer_tracker.h"

namespace gpu {
class ClientSharedImage;
}

namespace media {

// A tracker backed by a SharedImage.
// This tracker is not intended to be reused. It serves as a workaround to
// pass SharedImages through VideoCaptureBufferPool.
class SharedImageBufferTracker final : public VideoCaptureBufferTracker {
 public:
  explicit SharedImageBufferTracker(
      scoped_refptr<gpu::ClientSharedImage> client_shared_image);
  ~SharedImageBufferTracker() override;

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
  media::mojom::VideoBufferHandlePtr GetVideoBufferHandle() override;

  VideoCaptureBufferType GetBufferType() override;

  void UpdateExternalData(CapturedExternalVideoBuffer buffer) override;

 private:
  scoped_refptr<gpu::ClientSharedImage> client_shared_image_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_SHARED_IMAGE_BUFFER_TRACKER_H_
