// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of a VideoCaptureDeviceFactoryLinux class.

#ifndef MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_FACTORY_LINUX_H_
#define MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_FACTORY_LINUX_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "media/capture/video/video_capture_device_factory.h"

namespace media {

#if defined(WEBRTC_USE_PIPEWIRE)
class VideoCaptureDeviceFactoryWebRtc;
#endif  // defined(WEBRTC_USE_PIPEWIRE)

// Extension of VideoCaptureDeviceFactory to create and manipulate Linux
// devices.
class CAPTURE_EXPORT VideoCaptureDeviceFactoryLinux
    : public VideoCaptureDeviceFactory {
 public:
  explicit VideoCaptureDeviceFactoryLinux(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  VideoCaptureDeviceFactoryLinux(const VideoCaptureDeviceFactoryLinux&) =
      delete;
  VideoCaptureDeviceFactoryLinux& operator=(
      const VideoCaptureDeviceFactoryLinux&) = delete;

  ~VideoCaptureDeviceFactoryLinux() override;

  VideoCaptureErrorOrDevice CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDevicesInfo(GetDevicesInfoCallback callback) override;

 private:
#if defined(WEBRTC_USE_PIPEWIRE)
  void OnGetDevicesInfo(GetDevicesInfoCallback callback,
                        std::vector<VideoCaptureDeviceInfo> devices_info);

#endif  // defined(WEBRTC_USE_PIPEWIRE)

  std::unique_ptr<VideoCaptureDeviceFactory> factory_;
#if defined(WEBRTC_USE_PIPEWIRE)
  std::unique_ptr<VideoCaptureDeviceFactoryWebRtc> webrtc_factory_;
#endif  // defined(WEBRTC_USE_PIPEWIRE)

  // Must be the last member.
  base::WeakPtrFactory<VideoCaptureDeviceFactoryLinux> weak_ptr_factory_{this};
};

}  // namespace media
#endif  // MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_FACTORY_LINUX_H_
