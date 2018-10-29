// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_MOCK_DEVICE_FACTORY_H_
#define MEDIA_CAPTURE_VIDEO_MOCK_DEVICE_FACTORY_H_

#include <map>

#include "media/capture/video/video_capture_device_factory.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace media {

// Implementation of media::VideoCaptureDeviceFactory that allows clients to
// add mock devices.
class MockDeviceFactory : public media::VideoCaptureDeviceFactory {
 public:
  MockDeviceFactory();
  ~MockDeviceFactory() override;

  void AddMockDevice(media::VideoCaptureDevice* device,
                     const media::VideoCaptureDeviceDescriptor& descriptor);

  // media::VideoCaptureDeviceFactory implementation.
  std::unique_ptr<media::VideoCaptureDevice> CreateDevice(
      const media::VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDeviceDescriptors(
      media::VideoCaptureDeviceDescriptors* device_descriptors) override;
  void GetSupportedFormats(
      const media::VideoCaptureDeviceDescriptor& device_descriptor,
      media::VideoCaptureFormats* supported_formats) override;
  void GetCameraLocationsAsync(
      std::unique_ptr<media::VideoCaptureDeviceDescriptors> device_descriptors,
      DeviceDescriptorsCallback result_callback) override;

 private:
  std::map<media::VideoCaptureDeviceDescriptor, media::VideoCaptureDevice*>
      devices_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_MOCK_DEVICE_FACTORY_H_
