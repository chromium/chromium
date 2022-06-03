// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/video_capture_buffer_tracker_factory_win.h"

#include <memory>

#include "media/capture/video/shared_memory_buffer_tracker.h"
#include "media/capture/video/win/gpu_memory_buffer_tracker.h"

namespace media {

VideoCaptureBufferTrackerFactoryWin::VideoCaptureBufferTrackerFactoryWin()
    : dxgi_device_manager_(DXGIDeviceManager::Create()) {}

VideoCaptureBufferTrackerFactoryWin::~VideoCaptureBufferTrackerFactoryWin() {}

std::unique_ptr<VideoCaptureBufferTracker>
VideoCaptureBufferTrackerFactoryWin::CreateTracker(
    VideoCaptureBufferType buffer_type) {
  switch (buffer_type) {
    case VideoCaptureBufferType::kGpuMemoryBuffer:
      return std::make_unique<GpuMemoryBufferTracker>(dxgi_device_manager_);
    default:
      return std::make_unique<SharedMemoryBufferTracker>();
  }
}

std::unique_ptr<VideoCaptureBufferTracker>
VideoCaptureBufferTrackerFactoryWin::CreateTrackerForExternalGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle) {
  // Not supported
  return nullptr;
}

}  // namespace media