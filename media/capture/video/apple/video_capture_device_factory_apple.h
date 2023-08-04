// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of a VideoCaptureDeviceFactory class for Mac.

#ifndef MEDIA_CAPTURE_VIDEO_APPLE_VIDEO_CAPTURE_DEVICE_FACTORY_APPLE_H_
#define MEDIA_CAPTURE_VIDEO_APPLE_VIDEO_CAPTURE_DEVICE_FACTORY_APPLE_H_

#include "media/capture/video/video_capture_device_factory.h"

namespace media {

// Extension of VideoCaptureDeviceFactory to create and manipulate Mac
// devices.
class CAPTURE_EXPORT VideoCaptureDeviceFactoryApple
    : public VideoCaptureDeviceFactory {
 public:
  VideoCaptureDeviceFactoryApple();

  VideoCaptureDeviceFactoryApple(const VideoCaptureDeviceFactoryApple&) =
      delete;
  VideoCaptureDeviceFactoryApple& operator=(
      const VideoCaptureDeviceFactoryApple&) = delete;

  VideoCaptureErrorOrDevice CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDevicesInfo(GetDevicesInfoCallback callback) override;
};

CAPTURE_EXPORT bool ShouldEnableGpuMemoryBuffer(const std::string& device_id);

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_APPLE_VIDEO_CAPTURE_DEVICE_FACTORY_APPLE_H_
