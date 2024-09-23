// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_SERVICE_LAUNCHED_VIDEO_CAPTURE_DEVICE_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_SERVICE_LAUNCHED_VIDEO_CAPTURE_DEVICE_H_

#include "base/functional/callback_forward.h"
#include "base/token.h"
#include "content/browser/renderer_host/media/video_capture_provider.h"
#include "content/public/browser/video_capture_device_launcher.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/video_source.mojom.h"

namespace content {

// Implementation of LaunchedVideoCaptureDevice that uses
// video_capture::mojom::VideoCaptureService.
class ServiceLaunchedVideoCaptureDevice : public LaunchedVideoCaptureDevice {
 public:
  ServiceLaunchedVideoCaptureDevice(
      mojo::Remote<video_capture::mojom::VideoSource> source,
      mojo::Remote<video_capture::mojom::PushVideoStreamSubscription>
          subscription,
      base::OnceClosure connection_lost_cb);
  ~ServiceLaunchedVideoCaptureDevice() override;

  // LaunchedVideoCaptureDevice implementation.
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
  void OnLostConnectionToSourceOrSubscription();
  void OnGetPhotoStateResponse(
      media::VideoCaptureDevice::GetPhotoStateCallback callback,
      media::mojom::PhotoStatePtr capabilities) const;
  void OnSetPhotoOptionsResponse(
      media::VideoCaptureDevice::SetPhotoOptionsCallback callback,
      bool success);
  void OnTakePhotoResponse(
      media::VideoCaptureDevice::TakePhotoCallback callback,
      media::mojom::BlobPtr blob);

  mojo::Remote<video_capture::mojom::VideoSource> source_;
  mojo::Remote<video_capture::mojom::PushVideoStreamSubscription> subscription_;
  base::OnceClosure connection_lost_cb_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_SERVICE_LAUNCHED_VIDEO_CAPTURE_DEVICE_H_
