// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"

#include <memory>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/capture/video/shared_memory_buffer_tracker.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "media/capture/video/chromeos/gpu_memory_buffer_tracker.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "media/capture/video/mac/gpu_memory_buffer_tracker_mac.h"
#endif

namespace media {

std::unique_ptr<VideoCaptureBufferTracker>
VideoCaptureBufferTrackerFactoryImpl::CreateTracker(
    VideoCaptureBufferType buffer_type) {
  switch (buffer_type) {
    case VideoCaptureBufferType::kGpuMemoryBuffer:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      return std::make_unique<GpuMemoryBufferTracker>();
#elif BUILDFLAG(IS_MAC)
      return std::make_unique<GpuMemoryBufferTrackerMac>();
#else
      return nullptr;
#endif
    default:
      return std::make_unique<SharedMemoryBufferTracker>();
  }
}

std::unique_ptr<VideoCaptureBufferTracker>
VideoCaptureBufferTrackerFactoryImpl::CreateTrackerForExternalGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle) {
#if BUILDFLAG(IS_MAC)
  return std::make_unique<GpuMemoryBufferTrackerMac>(handle.io_surface);
#else
  return nullptr;
#endif
}

}  // namespace media
