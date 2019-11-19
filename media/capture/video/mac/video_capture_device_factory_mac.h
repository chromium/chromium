// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of a VideoCaptureDeviceFactory class for Mac.

#ifndef MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_FACTORY_MAC_H_
#define MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_FACTORY_MAC_H_

#include "base/macros.h"
#include "media/capture/video/video_capture_device_factory.h"

namespace media {

// Extension of VideoCaptureDeviceFactory to create and manipulate Mac devices.
class CAPTURE_EXPORT VideoCaptureDeviceFactoryMac
    : public VideoCaptureDeviceFactory {
 public:
  VideoCaptureDeviceFactoryMac();
  ~VideoCaptureDeviceFactoryMac() override;

  static void SetGetDeviceDescriptorsRetryCount(int count);
  static int GetGetDeviceDescriptorsRetryCount();

  std::unique_ptr<VideoCaptureDevice> CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDeviceDescriptors(
      VideoCaptureDeviceDescriptors* device_descriptors) override;
  void GetSupportedFormats(
      const VideoCaptureDeviceDescriptor& device_descriptor,
      VideoCaptureFormats* supported_formats) override;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceFactoryMac);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_FACTORY_MAC_H_
