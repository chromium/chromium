// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/video_capture_buffer_tracker_factory_win.h"

#include <memory>

#include "media/capture/video/shared_memory_buffer_tracker.h"
#include "media/capture/video/win/gpu_memory_buffer_tracker.h"

namespace media {

VideoCaptureBufferTrackerFactoryWin::VideoCaptureBufferTrackerFactoryWin(
    scoped_refptr<DXGIDeviceManager> dxgi_device_manager)
    : dxgi_device_manager_(std::move(dxgi_device_manager)) {}

VideoCaptureBufferTrackerFactoryWin::~VideoCaptureBufferTrackerFactoryWin() {}

std::unique_ptr<VideoCaptureBufferTracker>
VideoCaptureBufferTrackerFactoryWin::CreateTracker(
    VideoCaptureBufferType buffer_type) {
  switch (buffer_type) {
    case VideoCaptureBufferType::kGpuMemoryBuffer:
      if (!dxgi_device_manager_)
        return nullptr;
      return std::make_unique<GpuMemoryBufferTracker>(dxgi_device_manager_);
    default:
      // Since windows capturer outputs NV12 only for GMBs and I420 for software
      // frames the pixel format is used to choose between shmem and gmb
      // trackers. Therefore I420 shmem trackers must not be reusable for NV12
      // format.
      return std::make_unique<SharedMemoryBufferTracker>(
          /*reusable_only_for_same_format=*/true);
  }
}

std::unique_ptr<VideoCaptureBufferTracker>
VideoCaptureBufferTrackerFactoryWin::CreateTrackerForExternalGpuMemoryBuffer(
    gfx::GpuMemoryBufferHandle handle) {
  if (handle.type != gfx::DXGI_SHARED_HANDLE) {
    return nullptr;
  }

  return std::make_unique<GpuMemoryBufferTracker>(std::move(handle),
                                                  dxgi_device_manager_);
}

}  // namespace media
