// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_IN_PROCESS_LAUNCHED_VIDEO_CAPTURE_DEVICE_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_IN_PROCESS_LAUNCHED_VIDEO_CAPTURE_DEVICE_H_

#include "base/functional/callback_forward.h"
#include "base/task/single_thread_task_runner.h"
#include "base/token.h"
#include "content/browser/renderer_host/media/video_capture_provider.h"
#include "content/public/browser/video_capture_device_launcher.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video/video_capture_device.h"

namespace content {

class InProcessLaunchedVideoCaptureDevice : public LaunchedVideoCaptureDevice {
 public:
  InProcessLaunchedVideoCaptureDevice(
      std::unique_ptr<media::VideoCaptureDevice> device,
      scoped_refptr<base::SingleThreadTaskRunner> device_task_runner);
  ~InProcessLaunchedVideoCaptureDevice() override;

  void GetPhotoState(
      media::VideoCaptureDevice::GetPhotoStateCallback callback) override;
  void SetPhotoOptions(
      media::mojom::PhotoSettingsPtr settings,
      media::VideoCaptureDevice::SetPhotoOptionsCallback callback) override;
  void TakePhoto(
      media::VideoCaptureDevice::TakePhotoCallback callback) override;
  void MaybeSuspendDevice() override;
  void ResumeDevice() override;
  void ApplySubCaptureTarget(
      media::mojom::SubCaptureTargetType type,
      const base::Token& target,
      uint32_t sub_capture_target_version,
      base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)>
          callback) override;
  void RequestRefreshFrame() override;

  void SetDesktopCaptureWindowIdAsync(gfx::NativeViewId window_id,
                                      base::OnceClosure done_cb) override;

  void OnUtilizationReport(media::VideoCaptureFeedback feedback) override;

 private:
  void SetDesktopCaptureWindowIdOnDeviceThread(
      media::VideoCaptureDevice* device,
      gfx::NativeViewId window_id,
      base::OnceClosure done_cb);

  std::unique_ptr<media::VideoCaptureDevice> device_;
  const scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_IN_PROCESS_LAUNCHED_VIDEO_CAPTURE_DEVICE_H_
