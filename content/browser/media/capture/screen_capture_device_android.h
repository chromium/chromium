// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_SCREEN_CAPTURE_DEVICE_ANDROID_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_SCREEN_CAPTURE_DEVICE_ANDROID_H_

#include <memory>

#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "media/capture/content/android/screen_capture_machine_android.h"
#include "media/capture/video/video_capture_device.h"

namespace content {

// ScreenCaptureDeviceAndroid is an adapter for using
// media::ScreenCaptureMachineAndroid via the media::VideoCaptureDevice
// interface.
class CONTENT_EXPORT ScreenCaptureDeviceAndroid
    : public media::VideoCaptureDevice {
 public:
  ScreenCaptureDeviceAndroid();

  ScreenCaptureDeviceAndroid(const ScreenCaptureDeviceAndroid&) = delete;
  ScreenCaptureDeviceAndroid& operator=(const ScreenCaptureDeviceAndroid&) =
      delete;

  ~ScreenCaptureDeviceAndroid() override;

  // VideoCaptureDevice implementation.
  void AllocateAndStart(const media::VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override;
  void StopAndDeAllocate() override;
  void RequestRefreshFrame() override;
  void OnUtilizationReport(media::VideoCaptureFeedback feedback) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  media::ScreenCaptureMachineAndroid capture_machine_;
  scoped_refptr<media::ThreadSafeCaptureOracle> oracle_proxy_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_SCREEN_CAPTURE_DEVICE_ANDROID_H_
