// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_buffer_pool_impl.h"

#include <memory>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "media/capture/video/video_capture_buffer_handle.h"
#include "media/capture/video/video_capture_buffer_pool_util.h"
#include "media/capture/video/video_capture_buffer_tracker.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "media/capture/video/video_capture_device_client.h"
#include "ui/gfx/buffer_format_util.h"

namespace media {

VideoCaptureBufferPoolImpl::VideoCaptureBufferPoolImpl(
    VideoCaptureBufferType buffer_type)
    : VideoCaptureBufferPoolImpl(
          buffer_type,
          kVideoCaptureDefaultMaxBufferPoolSize,
          std::make_unique<media::VideoCaptureBufferTrackerFactoryImpl>()) {}

VideoCaptureBufferPoolImpl::VideoCaptureBufferPoolImpl(
    VideoCaptureBufferType buffer_type,
    int count)
    : VideoCaptureBufferPoolImpl(
          buffer_type,
          count,
          std::make_unique<media::VideoCaptureBufferTrackerFactoryImpl>()) {}

VideoCaptureBufferPoolImpl::VideoCaptureBufferPoolImpl(
    VideoCaptureBufferType buffer_type,
    int count,
    std::unique_ptr<VideoCaptureBufferTrackerFactory> buffer_tracker_factory)
    : buffer_type_(buffer_type),
      count_(count),
      buffer_tracker_factory_(std::move(buffer_tracker_factory)) {
  DCHECK_GT(count, 0);
}

VideoCaptureBufferPoolImpl::~VideoCaptureBufferPoolImpl() = default;

base::UnsafeSharedMemoryRegion
VideoCaptureBufferPoolImpl::DuplicateAsUnsafeRegion(int buffer_id) {
  base::AutoLock lock(lock_);

  VideoCaptureBufferTracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED_IN_MIGRATION() << "Invalid buffer_id.";
    return {};
  }
  return tracker->DuplicateAsUnsafeRegion();
}

std::unique_ptr<VideoCaptureBufferHandle>
VideoCaptureBufferPoolImpl::GetHandleForInProcessAccess(int buffer_id) {
  base::AutoLock lock(lock_);

  VideoCaptureBufferTracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED_IN_MIGRATION() << "Invalid buffer_id.";
    return nullptr;
  }

  return tracker->GetMemoryMappedAccess();
}

gfx::GpuMemoryBufferHandle VideoCaptureBufferPoolImpl::GetGpuMemoryBufferHandle(
    int buffer_id) {
  base::AutoLock lock(lock_);
  VideoCaptureBufferTracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED_IN_MIGRATION() << "Invalid buffer_id.";
    return gfx::GpuMemoryBufferHandle();
  }

  return tracker->GetGpuMemoryBufferHandle();
}

VideoCaptureBufferType VideoCaptureBufferPoolImpl::GetBufferType(
    int buffer_id) {
  base::AutoLock lock(lock_);

  VideoCaptureBufferTracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED() << "Unrecognized buffer id, buffer_id=" << buffer_id;
  }

  return tracker->GetBufferType();
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
    NOTREACHED_IN_MIGRATION() << "Invalid buffer_id.";
    return;
  }
  tracker->SetHeldByProducer(false);
}

VideoCaptureDevice::Client::ReserveResult
VideoCaptureBufferPoolImpl::ReserveIdForExternalBuffer(
    CapturedExternalVideoBuffer buffer,
    const gfx::Size& dimensions,
    int* buffer_id_to_drop,
    int* buffer_id) {
  DCHECK(buffer_id);
  DCHECK(buffer_id_to_drop);
  base::AutoLock lock(lock_);

  // Look for a tracker that matches this buffer and is not in use. While
  // iterating, find the least recently used tracker.
  *buffer_id_to_drop = kInvalidId;
  *buffer_id = kInvalidId;
  auto lru_tracker_it = trackers_.end();
  for (auto it = trackers_.begin(); it != trackers_.end(); ++it) {
    VideoCaptureBufferTracker* const tracker = it->second.get();
    if (tracker->IsHeldByProducerOrConsumer()) {
      continue;
    }

    if (tracker->IsSameGpuMemoryBuffer(buffer.handle)) {
      tracker->SetHeldByProducer(true);
      tracker->UpdateExternalData(std::move(buffer));
      *buffer_id = it->first;
      return VideoCaptureDevice::Client::ReserveResult::kSucceeded;
    }

    if (lru_tracker_it == trackers_.end() ||
        lru_tracker_it->second->LastCustomerUseSequenceNumber() >
            tracker->LastCustomerUseSequenceNumber()) {
      lru_tracker_it = it;
    }
  }

  // Preferably grow the pool by creating a new tracker. If we're at maximum
  // size, reallocate by deleting an existing one.
  if (trackers_.size() == static_cast<size_t>(count_)) {
    if (lru_tracker_it == trackers_.end()) {
      // We're out of space, and can't find an unused tracker to reallocate.
      DLOG(ERROR) << __func__
                  << " max buffer count exceeded count_ = " << count_;
      return VideoCaptureDevice::Client::ReserveResult::kMaxBufferCountExceeded;
    }
    *buffer_id_to_drop = lru_tracker_it->first;
    trackers_.erase(lru_tracker_it);
  }

  // Create the new tracker.
  auto tracker =
      buffer_tracker_factory_->CreateTrackerForExternalGpuMemoryBuffer(
          std::move(buffer.handle));
#if BUILDFLAG(IS_WIN)
  // Windows needs to create buffer from external handle, but mac doesn't.
  if (!tracker ||
      !tracker->Init(dimensions, buffer.format.pixel_format, nullptr)) {
    DLOG(ERROR) << "Error initializing VideoCaptureBufferTracker";
    return VideoCaptureDevice::Client::ReserveResult::kAllocationFailed;
  }
  tracker->UpdateExternalData(std::move(buffer));
#endif
  tracker->SetHeldByProducer(true);
  const int new_buffer_id = next_buffer_id_++;
  trackers_[new_buffer_id] = std::move(tracker);
  *buffer_id = new_buffer_id;
  return VideoCaptureDevice::Client::ReserveResult::kSucceeded;
}

void VideoCaptureBufferPoolImpl::HoldForConsumers(int buffer_id,
                                                  int num_clients) {
  base::AutoLock lock(lock_);
  VideoCaptureBufferTracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED_IN_MIGRATION() << "Invalid buffer_id.";
    return;
  }
  tracker->AddConsumerHolds(num_clients);
  // Note: The buffer will stay held by the producer until
  // RelinquishProducerReservation() (usually called by destructor of the object
  // wrapping this tracker, e.g. a VideoFrame).
}

void VideoCaptureBufferPoolImpl::RelinquishConsumerHold(int buffer_id,
                                                        int num_clients) {
  base::AutoLock lock(lock_);
  VideoCaptureBufferTracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED_IN_MIGRATION() << "Invalid buffer_id.";
    return;
  }
  tracker->RemoveConsumerHolds(num_clients);
}

double VideoCaptureBufferPoolImpl::GetBufferPoolUtilization() const {
  base::AutoLock lock(lock_);
  int num_buffers_held = 0;
  for (const auto& entry : trackers_) {
    VideoCaptureBufferTracker* const tracker = entry.second.get();
    if (tracker->IsHeldByProducerOrConsumer())
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
  *buffer_id = kInvalidId;
  uint32_t largest_memory_size_in_bytes = 0;
  auto tracker_to_drop = trackers_.end();
  for (auto it = trackers_.begin(); it != trackers_.end(); ++it) {
    VideoCaptureBufferTracker* const tracker = it->second.get();
    if (!tracker->IsHeldByProducerOrConsumer()) {
      if (tracker->IsReusableForFormat(dimensions, pixel_format, strides)) {
        // Reuse this buffer
        tracker->SetHeldByProducer(true);
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
      DLOG(ERROR) << __func__
                  << " max buffer count exceeded count_ = " << count_;
      return VideoCaptureDevice::Client::ReserveResult::kMaxBufferCountExceeded;
    }
    *buffer_id_to_drop = tracker_to_drop->first;
    trackers_.erase(tracker_to_drop);
  }

  // Create the new tracker.
  VideoCaptureBufferType buffer_type = buffer_type_;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  // If MediaFoundationD3D11VideoCapture or VideoCaptureDeviceAVFoundation fails
  // to produce NV12 as is expected on these platforms when the target buffer
  // type is `kGpuMemoryBuffer`, a shared memory buffer may be sent instead.
  if (buffer_type == VideoCaptureBufferType::kGpuMemoryBuffer &&
      pixel_format != PIXEL_FORMAT_NV12
#if BUILDFLAG(IS_MAC)
      && base::FeatureList::IsEnabled(kFallbackToSharedMemoryIfNotNv12OnMac)
#endif
  ) {
    buffer_type = VideoCaptureBufferType::kSharedMemory;
  }
#endif
  std::unique_ptr<VideoCaptureBufferTracker> tracker =
      buffer_tracker_factory_->CreateTracker(buffer_type);
  if (!tracker || !tracker->Init(dimensions, pixel_format, strides)) {
    DLOG(ERROR) << "Error initializing VideoCaptureBufferTracker";
    return VideoCaptureDevice::Client::ReserveResult::kAllocationFailed;
  }

  tracker->SetHeldByProducer(true);
  tracker->set_frame_feedback_id(frame_feedback_id);
  const int new_buffer_id = next_buffer_id_++;
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
