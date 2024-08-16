// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_LINUX_V4L2_GPU_MEMORY_BUFFER_TRACKER_H_
#define MEDIA_CAPTURE_VIDEO_LINUX_V4L2_GPU_MEMORY_BUFFER_TRACKER_H_

#include <atomic>

#include "media/capture/capture_export.h"
#include "media/capture/video/video_capture_buffer_tracker.h"
#include "media/capture/video/video_capture_gpu_channel_host.h"

namespace gpu {
class ClientSharedImage;
}

namespace media {

// Tracker specifics for Linux Mappable shared image.
// This class is owned by `VideoCaptureBufferPoolImpl`, which instantiates
// `this` using its `CreateTracker()` method. It is then accessed on both the
// main thread and the `VideoCaptureDeviceLinux::task_runner_`'s thread pool.
//
// When video capture device, like `VideoCaptureDeviceLinux`, produce frames,
// they are sent to the `VideoCaptureDeviceClient` through its
// `OnIncomingCapturedBufferExt()` method. This method then calls into
// `VideoCaptureBufferPoolImpl::HoldForConsumers()` on the device client's task
// runner thread pool, which then holds the tracker so it can be populated with
// frame data.
//
// When clients are done consuming the frame held in this tracker, the tracker
// is notified on the main thread through its `RemoveConsumerHolds()` method,
// which is called by `BroadcastingReceiver::OnFinishedConsumingBuffer()`. If
// the GPU context is lost, the tracker is notified on the main thread through
// its `OnContextLost()` method and then marks itself invalid. Further calls
// from the thread pool to the tracker's `IsReusableForFormat()` method will
// then return false. The `VideoCaptureBufferPoolImpl` will then no longer
// use this tracker and will release it when the buffer pool is full.
// TODO(crbug.com/40263579): Rename this class, file name, comments as well as
// related unit tests in follow up CL as it uses MappableSI now instead of
// GpuMemoryBuffers.
class CAPTURE_EXPORT V4L2GpuMemoryBufferTracker final
    : public VideoCaptureBufferTracker,
      public VideoCaptureGpuContextLostObserver {
 public:
  V4L2GpuMemoryBufferTracker();
  ~V4L2GpuMemoryBufferTracker() override;

  // VideoCaptureBufferTracker implementation.
  bool Init(const gfx::Size& dimensions,
            VideoPixelFormat format,
            const mojom::PlaneStridesPtr& strides) override;

  // Returns true if |this| matches the specified parameters and the GPU context
  // is not lost.
  bool IsReusableForFormat(const gfx::Size& dimensions,
                           VideoPixelFormat format,
                           const mojom::PlaneStridesPtr& strides) override;

  uint32_t GetMemorySizeInBytes() override;

  std::unique_ptr<VideoCaptureBufferHandle> GetMemoryMappedAccess() override;

  base::UnsafeSharedMemoryRegion DuplicateAsUnsafeRegion() override;
  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle() override;

  VideoCaptureBufferType GetBufferType() override;

  // VideoCaptureGpuContextLostObserver implementation.
  void OnContextLost() override;

 private:
  std::atomic_bool is_valid_{false};
  scoped_refptr<gpu::ClientSharedImage> shared_image_;
  base::WeakPtrFactory<VideoCaptureGpuContextLostObserver> weak_ptr_factory_{
      this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_LINUX_V4L2_GPU_MEMORY_BUFFER_TRACKER_H_
