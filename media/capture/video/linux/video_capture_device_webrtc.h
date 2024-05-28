// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_WEBRTC_H_
#define MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_WEBRTC_H_

#include "media/capture/video/video_capture_device.h"

#include <optional>

#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/capture/video/video_capture_device_factory.h"
#include "media/capture/video_capture_types.h"

#include "third_party/webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "third_party/webrtc/modules/video_capture/raw_video_sink_interface.h"
#include "third_party/webrtc/modules/video_capture/video_capture.h"
#include "third_party/webrtc/modules/video_capture/video_capture_options.h"

namespace media {

class VideoCaptureDeviceWebRtc : public VideoCaptureDevice,
                                 public webrtc::RawVideoSinkInterface {
 public:
  static VideoCaptureErrorOrDevice Create(
      webrtc::VideoCaptureOptions* options,
      const VideoCaptureDeviceDescriptor& device_descriptor);

  explicit VideoCaptureDeviceWebRtc(
      rtc::scoped_refptr<webrtc::VideoCaptureModule> capture_module);

  VideoCaptureDeviceWebRtc(const VideoCaptureDeviceWebRtc&) = delete;
  VideoCaptureDeviceWebRtc& operator=(const VideoCaptureDeviceWebRtc&) = delete;
  VideoCaptureDeviceWebRtc& operator=(const VideoCaptureDeviceWebRtc&&) =
      delete;

  ~VideoCaptureDeviceWebRtc() override;

  // VideoCaptureDevice implementation.
  void AllocateAndStart(const VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override;
  void StopAndDeAllocate() override;
  void TakePhoto(TakePhotoCallback callback) override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;

  int32_t OnRawFrame(uint8_t* video_frame,
                     size_t video_frame_length,
                     const webrtc::VideoCaptureCapability& frame_info,
                     webrtc::VideoRotation rotation,
                     int64_t capture_time_ms) override;

 private:
  rtc::scoped_refptr<webrtc::VideoCaptureModule> capture_module_;
  VideoCaptureFormat capture_format_;
  std::optional<base::TimeDelta> base_time_;

  std::unique_ptr<VideoCaptureDevice::Client> client_;
};

}  // namespace media
#endif  // MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_WEBRTC_H_
