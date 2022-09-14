// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_FILE_VIDEO_CAPTURE_DEVICE_FACTORY_H_
#define MEDIA_CAPTURE_VIDEO_FILE_VIDEO_CAPTURE_DEVICE_FACTORY_H_

#include "media/capture/video/video_capture_device_factory.h"

namespace media {

// Extension of VideoCaptureDeviceFactory to create and manipulate file-backed
// fake devices. These devices play back video-only files as video capture
// input.
// The |device_descriptor.display_name| passed into the Create() method is
// interpreted as a (platform-specific) file path to a video file to be used as
// a source.
class CAPTURE_EXPORT FileVideoCaptureDeviceFactory
    : public VideoCaptureDeviceFactory {
 public:
  FileVideoCaptureDeviceFactory() {}
  ~FileVideoCaptureDeviceFactory() override {}

  VideoCaptureErrorOrDevice CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDevicesInfo(GetDevicesInfoCallback callback) override;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_FILE_VIDEO_CAPTURE_DEVICE_FACTORY_H_
