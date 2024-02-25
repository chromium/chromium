// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_POOL_IMPL_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_POOL_IMPL_H_

#include <stddef.h>

#include <map>

#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "media/base/video_frame.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/video_capture_buffer_handle.h"
#include "media/capture/video/video_capture_buffer_pool.h"
#include "media/capture/video/video_capture_buffer_tracker_factory.h"
#include "media/capture/video_capture_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if BUILDFLAG(IS_WIN)
#include "media/base/win/dxgi_device_manager.h"
#endif

namespace media {

class CAPTURE_EXPORT VideoCaptureBufferPoolImpl
    : public VideoCaptureBufferPool {
 public:
  VideoCaptureBufferPoolImpl() = delete;
  explicit VideoCaptureBufferPoolImpl(VideoCaptureBufferType buffer_type);
  VideoCaptureBufferPoolImpl(VideoCaptureBufferType buffer_type, int count);
  VideoCaptureBufferPoolImpl(
      VideoCaptureBufferType buffer_type,
      int count,
      std::unique_ptr<VideoCaptureBufferTrackerFactory> buffer_tracker_factory);

  VideoCaptureBufferPoolImpl(const VideoCaptureBufferPoolImpl&) = delete;
  VideoCaptureBufferPoolImpl& operator=(const VideoCaptureBufferPoolImpl&) =
      delete;

  // VideoCaptureBufferPool implementation.
  base::UnsafeSharedMemoryRegion DuplicateAsUnsafeRegion(
      int buffer_id) override;
  std::unique_ptr<VideoCaptureBufferHandle> GetHandleForInProcessAccess(
      int buffer_id) override;
  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle(int buffer_id) override;

  VideoCaptureBufferType GetBufferType(int buffer_id) override;

  VideoCaptureDevice::Client::ReserveResult ReserveForProducer(
      const gfx::Size& dimensions,
      VideoPixelFormat format,
      const mojom::PlaneStridesPtr& strides,
      int frame_feedback_id,
      int* buffer_id,
      int* buffer_id_to_drop) override;
  void RelinquishProducerReservation(int buffer_id) override;
  VideoCaptureDevice::Client::ReserveResult ReserveIdForExternalBuffer(
      CapturedExternalVideoBuffer buffer,
      const gfx::Size& dimensions,
      int* buffer_id_to_drop,
      int* buffer_id) override;
  double GetBufferPoolUtilization() const override;
  void HoldForConsumers(int buffer_id, int num_clients) override;
  void RelinquishConsumerHold(int buffer_id, int num_clients) override;

 private:
  friend class base::RefCountedThreadSafe<VideoCaptureBufferPoolImpl>;
  ~VideoCaptureBufferPoolImpl() override;

  VideoCaptureDevice::Client::ReserveResult ReserveForProducerInternal(
      const gfx::Size& dimensions,
      VideoPixelFormat format,
      const mojom::PlaneStridesPtr& strides,
      int frame_feedback_id,
      int* buffer_id,
      int* tracker_id_to_drop);

  VideoCaptureBufferTracker* GetTracker(int buffer_id)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // The type of the buffer the pool serves.
  VideoCaptureBufferType buffer_type_;

  // The max number of buffers that the pool is allowed to have at any moment.
  const int count_;

  // Protects everything below it.
  mutable base::Lock lock_;

  // The ID of the next buffer.
  int next_buffer_id_ GUARDED_BY(lock_) = 0;

  // The buffers, indexed by the first parameter, a buffer id.
  std::map<int, std::unique_ptr<VideoCaptureBufferTracker>> trackers_
      GUARDED_BY(lock_);

  const std::unique_ptr<VideoCaptureBufferTrackerFactory>
      buffer_tracker_factory_ GUARDED_BY(lock_);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_POOL_IMPL_H_
