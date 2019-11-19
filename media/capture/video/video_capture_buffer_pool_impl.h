// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_POOL_IMPL_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_POOL_IMPL_H_

#include <stddef.h>

#include <map>

#include "base/files/file.h"
#include "base/macros.h"
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

namespace media {

class CAPTURE_EXPORT VideoCaptureBufferPoolImpl
    : public VideoCaptureBufferPool {
 public:
  explicit VideoCaptureBufferPoolImpl(
      std::unique_ptr<VideoCaptureBufferTrackerFactory> buffer_tracker_factory,
      VideoCaptureBufferType buffer_type,
      int count);

  // VideoCaptureBufferPool implementation.
  base::UnsafeSharedMemoryRegion DuplicateAsUnsafeRegion(
      int buffer_id) override;
  mojo::ScopedSharedBufferHandle DuplicateAsMojoBuffer(int buffer_id) override;
  mojom::SharedMemoryViaRawFileDescriptorPtr
  CreateSharedMemoryViaRawFileDescriptorStruct(int buffer_id) override;
  std::unique_ptr<VideoCaptureBufferHandle> GetHandleForInProcessAccess(
      int buffer_id) override;
  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle(int buffer_id) override;
  VideoCaptureDevice::Client::ReserveResult ReserveForProducer(
      const gfx::Size& dimensions,
      VideoPixelFormat format,
      const mojom::PlaneStridesPtr& strides,
      int frame_feedback_id,
      int* buffer_id,
      int* buffer_id_to_drop) override;
  void RelinquishProducerReservation(int buffer_id) override;
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

  VideoCaptureBufferTracker* GetTracker(int buffer_id);

  // The type of the buffer the pool serves.
  VideoCaptureBufferType buffer_type_;

  // The max number of buffers that the pool is allowed to have at any moment.
  const int count_;

  // Protects everything below it.
  mutable base::Lock lock_;

  // The ID of the next buffer.
  int next_buffer_id_;

  // The buffers, indexed by the first parameter, a buffer id.
  std::map<int, std::unique_ptr<VideoCaptureBufferTracker>> trackers_;

  const std::unique_ptr<VideoCaptureBufferTrackerFactory>
      buffer_tracker_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureBufferPoolImpl);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_POOL_IMPL_H_
