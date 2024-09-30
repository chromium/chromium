// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_MOCK_DEVICE_FACTORY_H_
#define MEDIA_CAPTURE_VIDEO_MOCK_DEVICE_FACTORY_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "media/capture/video/video_capture_device_factory.h"

namespace media {

// Implementation of media::VideoCaptureDeviceFactory that allows clients to
// add mock devices.
class MockDeviceFactory : public media::VideoCaptureDeviceFactory {
 public:
  MockDeviceFactory();
  ~MockDeviceFactory() override;

  void AddMockDevice(media::VideoCaptureDevice* device,
                     const media::VideoCaptureDeviceDescriptor& descriptor);
  void RemoveAllDevices();

  // media::VideoCaptureDeviceFactory implementation.
  VideoCaptureErrorOrDevice CreateDevice(
      const media::VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDevicesInfo(GetDevicesInfoCallback callback) override;

 private:
  std::map<media::VideoCaptureDeviceDescriptor,
           raw_ptr<media::VideoCaptureDevice, CtnExperimental>>
      devices_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_MOCK_DEVICE_FACTORY_H_
