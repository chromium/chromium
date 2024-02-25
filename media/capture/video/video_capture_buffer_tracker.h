// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_TRACKER_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_TRACKER_H_

#include <memory>

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/synchronization/lock.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video/video_capture_buffer_handle.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/system/buffer.h"

namespace gfx {
struct GpuMemoryBufferHandle;
}

namespace media {

// Keeps track of the state of a given mappable resource. This is a base class
// for implementations using different kinds of storage.
class CAPTURE_EXPORT VideoCaptureBufferTracker {
 public:
  VideoCaptureBufferTracker() = default;
  virtual bool Init(const gfx::Size& dimensions,
                    VideoPixelFormat format,
                    const mojom::PlaneStridesPtr& strides) = 0;
  virtual ~VideoCaptureBufferTracker() {}

  bool IsHeldByProducerOrConsumer() const {
    return held_by_producer_ || consumer_hold_count_ > 0;
  }
  void SetHeldByProducer(bool value);
  void AddConsumerHolds(int count);
  void RemoveConsumerHolds(int count);

  void set_frame_feedback_id(int value) { frame_feedback_id_ = value; }
  int frame_feedback_id() { return frame_feedback_id_; }

  // Returns true if |handle| refers to the same buffer as |this|. This is used
  // to reuse buffers that were externally allocated.
  virtual bool IsSameGpuMemoryBuffer(
      const gfx::GpuMemoryBufferHandle& handle) const;

  // Returns true if |this| matches the specified parameters. This is used to
  // reuse buffers that were internally allocated.
  virtual bool IsReusableForFormat(const gfx::Size& dimensions,
                                   VideoPixelFormat format,
                                   const mojom::PlaneStridesPtr& strides) = 0;

  virtual uint32_t GetMemorySizeInBytes() = 0;

  virtual std::unique_ptr<VideoCaptureBufferHandle> GetMemoryMappedAccess() = 0;

  virtual base::UnsafeSharedMemoryRegion DuplicateAsUnsafeRegion() = 0;
  virtual gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle() = 0;

  // Returns buffer type of the underlying resource. If the result of calling
  // this method is `kGpuMemoryBuffer`, attempting to call
  // `GetGpuMemoryBufferHandle()` on this tracker is allowed. If the result of
  // calling this method is `kSharedMemory`, attempting to call
  // `DuplicateAsUnsafeRegion()` is allowed.
  virtual VideoCaptureBufferType GetBufferType() = 0;

  // This is called when the number of consumers goes from zero to non-zero (in
  // which case |is_held_by_consumers| is true) or from non-zero to zero (in
  // which case |is_held_by_consumers| is false).
  virtual void OnHeldByConsumersChanged(bool is_held_by_consumers);

  // External buffers are to be freed in least-recently-used order. This
  // function returns a number which is greater for more recently used buffers.
  uint64_t LastCustomerUseSequenceNumber() const {
    return last_customer_use_sequence_number_;
  }

  // This is called when the a tracker is reused or created.
  virtual void UpdateExternalData(CapturedExternalVideoBuffer buffer) {}

 private:
  // Indicates whether this VideoCaptureBufferTracker is currently referenced by
  // the producer.
  bool held_by_producer_ = false;

  // Number of consumer processes which hold this VideoCaptureBufferTracker.
  int consumer_hold_count_ = 0;

  int frame_feedback_id_ = 0;

  uint64_t last_customer_use_sequence_number_ = 0;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_TRACKER_H_
