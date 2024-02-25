// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"

#include <memory>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/capture/video/shared_memory_buffer_tracker.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "media/capture/video/chromeos/gpu_memory_buffer_tracker_cros.h"
#elif BUILDFLAG(IS_APPLE)
#include "media/capture/video/apple/gpu_memory_buffer_tracker_apple.h"
#elif BUILDFLAG(IS_LINUX)
#include "media/capture/video/linux/v4l2_gpu_memory_buffer_tracker.h"
#elif BUILDFLAG(IS_WIN)
#include "media/capture/video/win/gpu_memory_buffer_tracker_win.h"
#endif

namespace media {

VideoCaptureBufferTrackerFactoryImpl::VideoCaptureBufferTrackerFactoryImpl() {}

#if BUILDFLAG(IS_WIN)
VideoCaptureBufferTrackerFactoryImpl::VideoCaptureBufferTrackerFactoryImpl(
    scoped_refptr<DXGIDeviceManager> dxgi_device_manager)
    : dxgi_device_manager_(std::move(dxgi_device_manager)) {}
#endif

VideoCaptureBufferTrackerFactoryImpl::~VideoCaptureBufferTrackerFactoryImpl() =
    default;

std::unique_ptr<VideoCaptureBufferTracker>
VideoCaptureBufferTrackerFactoryImpl::CreateTracker(
    VideoCaptureBufferType buffer_type) {
  switch (buffer_type) {
    case VideoCaptureBufferType::kGpuMemoryBuffer:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      return std::make_unique<GpuMemoryBufferTrackerCros>();
#elif BUILDFLAG(IS_APPLE)
      return std::make_unique<GpuMemoryBufferTrackerApple>();
#elif BUILDFLAG(IS_LINUX)
      return std::make_unique<V4L2GpuMemoryBufferTracker>();
#elif BUILDFLAG(IS_WIN)
      if (!dxgi_device_manager_) {
        return nullptr;
      }
      return std::make_unique<GpuMemoryBufferTrackerWin>(dxgi_device_manager_);
#else
      return nullptr;
#endif
    default:
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
      // Since Windows and macOS capturer outputs NV12 only for GMBs and I420
      // for software frames, the pixel format is used to choose between shmem
      // and gmb trackers. Therefore I420 shmem trackers must not be reusable
      // for NV12 format.
      return std::make_unique<SharedMemoryBufferTracker>(
          /*reusable_only_for_same_format=*/true);
#else
      return std::make_unique<SharedMemoryBufferTracker>();
#endif
  }
}

std::unique_ptr<VideoCaptureBufferTracker>
VideoCaptureBufferTrackerFactoryImpl::CreateTrackerForExternalGpuMemoryBuffer(
    gfx::GpuMemoryBufferHandle handle) {
#if BUILDFLAG(IS_APPLE)
  return std::make_unique<GpuMemoryBufferTrackerApple>(handle.io_surface);
#elif BUILDFLAG(IS_WIN)
  if (handle.type != gfx::DXGI_SHARED_HANDLE) {
    return nullptr;
  }
  return std::make_unique<GpuMemoryBufferTrackerWin>(std::move(handle),
                                                     dxgi_device_manager_);
#else
  return nullptr;
#endif
}

}  // namespace media
