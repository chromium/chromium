// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of a VideoCaptureDeviceFactory class for Mac.

#ifndef MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_FACTORY_MAC_H_
#define MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_FACTORY_MAC_H_

#include "media/capture/video/video_capture_device_factory.h"

namespace media {

// Extension of VideoCaptureDeviceFactory to create and manipulate Mac devices.
class CAPTURE_EXPORT VideoCaptureDeviceFactoryMac
    : public VideoCaptureDeviceFactory {
 public:
  VideoCaptureDeviceFactoryMac();

  VideoCaptureDeviceFactoryMac(const VideoCaptureDeviceFactoryMac&) = delete;
  VideoCaptureDeviceFactoryMac& operator=(const VideoCaptureDeviceFactoryMac&) =
      delete;

  ~VideoCaptureDeviceFactoryMac() override;

  static void SetGetDevicesInfoRetryCount(int count);
  static int GetGetDevicesInfoRetryCount();

  VideoCaptureErrorOrDevice CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDevicesInfo(GetDevicesInfoCallback callback) override;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_FACTORY_MAC_H_
