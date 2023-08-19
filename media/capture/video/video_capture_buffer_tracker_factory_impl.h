// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_IMPL_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_IMPL_H_

#include <memory>

#include "media/capture/capture_export.h"
#include "media/capture/video/video_capture_buffer_tracker_factory.h"

#if BUILDFLAG(IS_WIN)
#include "media/base/win/dxgi_device_manager.h"
#endif

namespace media {

class CAPTURE_EXPORT VideoCaptureBufferTrackerFactoryImpl
    : public VideoCaptureBufferTrackerFactory {
 public:
  VideoCaptureBufferTrackerFactoryImpl();
#if BUILDFLAG(IS_WIN)
  explicit VideoCaptureBufferTrackerFactoryImpl(
      scoped_refptr<DXGIDeviceManager> dxgi_device_manager);
#endif
  std::unique_ptr<VideoCaptureBufferTracker> CreateTracker(
      VideoCaptureBufferType buffer_type) override;
  std::unique_ptr<VideoCaptureBufferTracker>
  CreateTrackerForExternalGpuMemoryBuffer(
      gfx::GpuMemoryBufferHandle handle) override;
  ~VideoCaptureBufferTrackerFactoryImpl() override;

 private:
#if BUILDFLAG(IS_WIN)
  scoped_refptr<DXGIDeviceManager> dxgi_device_manager_;
#endif
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_IMPL_H_
