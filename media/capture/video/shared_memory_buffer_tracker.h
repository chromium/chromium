// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_SHARED_MEMORY_BUFFER_TRACKER_H_
#define MEDIA_CAPTURE_VIDEO_SHARED_MEMORY_BUFFER_TRACKER_H_

#include "media/capture/video/video_capture_buffer_handle.h"
#include "media/capture/video/video_capture_buffer_tracker.h"

namespace gfx {
class Size;
}

namespace media {

// A tracker backed by unsafe shared memory. An unsafe region is necessary
// because a buffer may be used multiple times in an output media::VideoFrame to
// a decoder cross-process where it is written.
class SharedMemoryBufferTracker final : public VideoCaptureBufferTracker {
 public:
  // If |strict_pixel_format| is false, a tracker of a sufficient size
  // may be reused for any requested pixel format.
  // Otherwise, the tracker may be reused only for the same pixel format
  // as was used for the initialization.
  // It may be useful if the capturer is using different pixel format for
  // ShMem and GpuMemory buffers.
  explicit SharedMemoryBufferTracker(bool strict_pixel_format = false);

  SharedMemoryBufferTracker(const SharedMemoryBufferTracker&) = delete;
  SharedMemoryBufferTracker& operator=(const SharedMemoryBufferTracker&) =
      delete;

  ~SharedMemoryBufferTracker() override;

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
  base::UnsafeSharedMemoryRegion region_;
  base::WritableSharedMemoryMapping mapping_;
  // Pixel format for the underlying buffer.
  VideoPixelFormat format_;
  // Is the tracker reusable only for the same pixel format as used for
  // initialization of the tracker.
  const bool strict_pixel_format_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_SHARED_MEMORY_BUFFER_TRACKER_H_
