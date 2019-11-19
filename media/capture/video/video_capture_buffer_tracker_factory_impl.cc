// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"

#include <memory>

#include "media/capture/video/shared_memory_buffer_tracker.h"

#if defined(OS_CHROMEOS)
#include "media/capture/video/chromeos/gpu_memory_buffer_tracker.h"
#endif

namespace media {

std::unique_ptr<VideoCaptureBufferTracker>
VideoCaptureBufferTrackerFactoryImpl::CreateTracker(
    VideoCaptureBufferType buffer_type) {
  switch (buffer_type) {
    case VideoCaptureBufferType::kGpuMemoryBuffer:
#if defined(OS_CHROMEOS)
      return std::make_unique<GpuMemoryBufferTracker>();
#else
      return nullptr;
#endif
    default:
      return std::make_unique<SharedMemoryBufferTracker>();
  }
}

}  // namespace media
