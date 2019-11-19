// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_buffer_pool_impl.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "media/capture/video/video_capture_buffer_handle.h"
#include "media/capture/video/video_capture_buffer_tracker.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/buffer_format_util.h"

namespace media {

VideoCaptureBufferPoolImpl::VideoCaptureBufferPoolImpl(
    std::unique_ptr<VideoCaptureBufferTrackerFactory> buffer_tracker_factory,
    VideoCaptureBufferType buffer_type,
    int count)
    : buffer_type_(buffer_type),
      count_(count),
      next_buffer_id_(0),
      buffer_tracker_factory_(std::move(buffer_tracker_factory)) {
  DCHECK_GT(count, 0);
}

VideoCaptureBufferPoolImpl::~VideoCaptureBufferPoolImpl() = default;

base::UnsafeSharedMemoryRegion
VideoCaptureBufferPoolImpl::DuplicateAsUnsafeRegion(int buffer_id) {
  base::AutoLock lock(lock_);

  VideoCaptureBufferTracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED() << "Invalid buffer_id.";
    return {};
  }
  return tracker->DuplicateAsUnsafeRegion();
}

mojo::ScopedSharedBufferHandle
VideoCaptureBufferPoolImpl::DuplicateAsMojoBuffer(int buffer_id) {
  base::AutoLock lock(lock_);

  VideoCaptureBufferTracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED() << "Invalid buffer_id.";
    return mojo::ScopedSharedBufferHandle();
  }
  return tracker->DuplicateAsMojoBuffer();
}

mojom::SharedMemoryViaRawFileDescriptorPtr
VideoCaptureBufferPoolImpl::CreateSharedMemoryViaRawFileDescriptorStruct(
    int buffer_id) {
// This requires platforms where base::SharedMemoryHandle is backed by a
// file descriptor.
#if defined(OS_LINUX)
  base::AutoLock lock(lock_);

  VideoCaptureBufferTracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED() << "Invalid buffer_id.";
    return 0u;
  }

  // Convert the mojo::ScopedSharedBufferHandle to a PlatformSharedMemoryRegion
  // in order to extract the platform file descriptor.
  base::subtle::PlatformSharedMemoryRegion platform_region =
      mojo::UnwrapPlatformSharedMemoryRegion(tracker->DuplicateAsMojoBuffer());
  if (!platform_region.IsValid()) {
    NOTREACHED();
    return 0u;
  }
  base::subtle::ScopedFDPair fds = platform_region.PassPlatformHandle();
  auto result = mojom::SharedMemoryViaRawFileDescriptor::New();
  result->file_descriptor_handle = mojo::WrapPlatformFile(fds.fd.release());
  result->shared_memory_size_in_bytes = tracker->GetMemorySizeInBytes();
  return result;
#else
  NOTREACHED();
  return mojom::SharedMemoryViaRawFileDescriptorPtr();
#endif
}

std::unique_ptr<VideoCaptureBufferHandle>
VideoCaptureBufferPoolImpl::GetHandleForInProcessAccess(int buffer_id) {
  base::AutoLock lock(lock_);

  VideoCaptureBufferTracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED() << "Invalid buffer_id.";
    return nullptr;
  }

  return tracker->GetMemoryMappedAccess();
}

gfx::GpuMemoryBufferHandle VideoCaptureBufferPoolImpl::GetGpuMemoryBufferHandle(
    int buffer_id) {
  base::AutoLock lock(lock_);

  VideoCaptureBufferTracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED() << "Invalid buffer_id.";
    return gfx::GpuMemoryBufferHandle();
  }

  return tracker->GetGpuMemoryBufferHandle();
}

VideoCaptureDevice::Client::ReserveResult
VideoCaptureBufferPoolImpl::ReserveForProducer(
    const gfx::Size& dimensions,
    VideoPixelFormat format,
    const mojom::PlaneStridesPtr& strides,
    int frame_feedback_id,
    int* buffer_id,
    int* buffer_id_to_drop) {
  base::AutoLock lock(lock_);
  return ReserveForProducerInternal(dimensions, format, strides,
                                    frame_feedback_id, buffer_id,
                                    buffer_id_to_drop);
}

void VideoCaptureBufferPoolImpl::RelinquishProducerReservation(int buffer_id) {
  base::AutoLock lock(lock_);
  VideoCaptureBufferTracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED() << "Invalid buffer_id.";
    return;
  }
  DCHECK(tracker->held_by_producer());
  tracker->set_held_by_producer(false);
}

void VideoCaptureBufferPoolImpl::HoldForConsumers(int buffer_id,
                                                  int num_clients) {
  base::AutoLock lock(lock_);
  VideoCaptureBufferTracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED() << "Invalid buffer_id.";
    return;
  }
  DCHECK(tracker->held_by_producer());
  DCHECK(!tracker->consumer_hold_count());

  tracker->set_consumer_hold_count(num_clients);
  // Note: |held_by_producer()| will stay true until
  // RelinquishProducerReservation() (usually called by destructor of the object
  // wrapping this tracker, e.g. a VideoFrame).
}

void VideoCaptureBufferPoolImpl::RelinquishConsumerHold(int buffer_id,
                                                        int num_clients) {
  base::AutoLock lock(lock_);
  VideoCaptureBufferTracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED() << "Invalid buffer_id.";
    return;
  }
  DCHECK_GE(tracker->consumer_hold_count(), num_clients);

  tracker->set_consumer_hold_count(tracker->consumer_hold_count() -
                                   num_clients);
}

double VideoCaptureBufferPoolImpl::GetBufferPoolUtilization() const {
  base::AutoLock lock(lock_);
  int num_buffers_held = 0;
  for (const auto& entry : trackers_) {
    VideoCaptureBufferTracker* const tracker = entry.second.get();
    if (tracker->held_by_producer() || tracker->consumer_hold_count() > 0)
      ++num_buffers_held;
  }
  return static_cast<double>(num_buffers_held) / count_;
}

VideoCaptureDevice::Client::ReserveResult
VideoCaptureBufferPoolImpl::ReserveForProducerInternal(
    const gfx::Size& dimensions,
    VideoPixelFormat pixel_format,
    const mojom::PlaneStridesPtr& strides,
    int frame_feedback_id,
    int* buffer_id,
    int* buffer_id_to_drop) {
  lock_.AssertAcquired();

  // Look for a tracker that's allocated, big enough, and not in use. Track the
  // largest one that's not big enough, in case we have to reallocate a tracker.
  *buffer_id_to_drop = kInvalidId;
  uint32_t largest_memory_size_in_bytes = 0;
  auto tracker_to_drop = trackers_.end();
  for (auto it = trackers_.begin(); it != trackers_.end(); ++it) {
    VideoCaptureBufferTracker* const tracker = it->second.get();
    if (!tracker->consumer_hold_count() && !tracker->held_by_producer()) {
      if (tracker->IsReusableForFormat(dimensions, pixel_format, strides)) {
        // Reuse this buffer
        tracker->set_held_by_producer(true);
        tracker->set_frame_feedback_id(frame_feedback_id);
        *buffer_id = it->first;
        return VideoCaptureDevice::Client::ReserveResult::kSucceeded;
      }
      if (tracker->GetMemorySizeInBytes() > largest_memory_size_in_bytes) {
        largest_memory_size_in_bytes = tracker->GetMemorySizeInBytes();
        tracker_to_drop = it;
      }
    }
  }

  // Preferably grow the pool by creating a new tracker. If we're at maximum
  // size, reallocate by deleting an existing one.
  if (trackers_.size() == static_cast<size_t>(count_)) {
    if (tracker_to_drop == trackers_.end()) {
      // We're out of space, and can't find an unused tracker to reallocate.
      *buffer_id = kInvalidId;
      return VideoCaptureDevice::Client::ReserveResult::kMaxBufferCountExceeded;
    }
    *buffer_id_to_drop = tracker_to_drop->first;
    trackers_.erase(tracker_to_drop);
  }

  // Create the new tracker.
  const int new_buffer_id = next_buffer_id_++;

  std::unique_ptr<VideoCaptureBufferTracker> tracker =
      buffer_tracker_factory_->CreateTracker(buffer_type_);
  if (!tracker->Init(dimensions, pixel_format, strides)) {
    DLOG(ERROR) << "Error initializing VideoCaptureBufferTracker";
    *buffer_id = kInvalidId;
    return VideoCaptureDevice::Client::ReserveResult::kAllocationFailed;
  }

  tracker->set_held_by_producer(true);
  tracker->set_frame_feedback_id(frame_feedback_id);
  trackers_[new_buffer_id] = std::move(tracker);

  *buffer_id = new_buffer_id;
  return VideoCaptureDevice::Client::ReserveResult::kSucceeded;
}

VideoCaptureBufferTracker* VideoCaptureBufferPoolImpl::GetTracker(
    int buffer_id) {
  auto it = trackers_.find(buffer_id);
  return (it == trackers_.end()) ? nullptr : it->second.get();
}

}  // namespace media
