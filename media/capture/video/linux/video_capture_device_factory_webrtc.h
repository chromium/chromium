// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_FACTORY_WEBRTC_H_
#define MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_FACTORY_WEBRTC_H_

#include "media/capture/video/video_capture_device_factory.h"

#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "media/capture/video_capture_types.h"

#include "third_party/webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "third_party/webrtc/modules/video_capture/video_capture_options.h"

namespace media {

// Extension of VideoCaptureDeviceFactory to create and manipulate WebRTC
// devices.
class CAPTURE_EXPORT VideoCaptureDeviceFactoryWebRtc
    : public VideoCaptureDeviceFactory,
      webrtc::VideoCaptureOptions::Callback {
 public:
  VideoCaptureDeviceFactoryWebRtc();

  VideoCaptureDeviceFactoryWebRtc(const VideoCaptureDeviceFactoryWebRtc&) =
      delete;
  VideoCaptureDeviceFactoryWebRtc& operator=(
      const VideoCaptureDeviceFactoryWebRtc&) = delete;
  VideoCaptureDeviceFactoryWebRtc& operator=(
      const VideoCaptureDeviceFactoryWebRtc&&) = delete;

  ~VideoCaptureDeviceFactoryWebRtc() override;

  VideoCaptureErrorOrDevice CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDevicesInfo(GetDevicesInfoCallback callback) override;

  void OnInitialized(webrtc::VideoCaptureOptions::Status status) override;

  // Check if the necessary services are available on the system.
  // Specifically this means that PipeWire and xdg-desktop-portal are running.
  // The return will always be true at first and may change during device
  // enumeration. So it should be checked again in the GetDevicesInfo()
  // callback.
  bool IsAvailable();

  static webrtc::VideoType WebRtcVideoTypeFromChromiumPixelFormat(
      VideoPixelFormat pixel_format);
  static VideoPixelFormat WebRtcVideoTypeToChromiumPixelFormat(
      webrtc::VideoType video_type);

 private:
  // Collect the camera devices and call the GetDevicesInfo() callback.
  void FinishGetDevicesInfo();
  GetDevicesInfoCallback callback_;

  std::unique_ptr<webrtc::VideoCaptureOptions> options_;
  webrtc::VideoCaptureOptions::Status status_{
      webrtc::VideoCaptureOptions::Status::UNINITIALIZED};

  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;
};

}  // namespace media
#endif  // MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_FACTORY_WEBRTC_H_
