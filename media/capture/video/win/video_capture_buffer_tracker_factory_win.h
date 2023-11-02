// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_WIN_H_
#define MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_WIN_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "media/base/win/dxgi_device_manager.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/video_capture_buffer_tracker_factory.h"

namespace media {

class CAPTURE_EXPORT VideoCaptureBufferTrackerFactoryWin
    : public VideoCaptureBufferTrackerFactory {
 public:
  explicit VideoCaptureBufferTrackerFactoryWin(
      scoped_refptr<DXGIDeviceManager> dxgi_device_manager);
  ~VideoCaptureBufferTrackerFactoryWin() override;
  std::unique_ptr<VideoCaptureBufferTracker> CreateTracker(
      VideoCaptureBufferType buffer_type) override;
  std::unique_ptr<VideoCaptureBufferTracker>
  CreateTrackerForExternalGpuMemoryBuffer(
      const gfx::GpuMemoryBufferHandle& handle) override;

 private:
  scoped_refptr<DXGIDeviceManager> dxgi_device_manager_;
  base::WeakPtrFactory<VideoCaptureBufferTrackerFactoryWin> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_WIN_H_