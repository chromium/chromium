// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MOCK_VIDEO_CAPTURE_PROVIDER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MOCK_VIDEO_CAPTURE_PROVIDER_H_

#include "base/callback_forward.h"
#include "base/token.h"
#include "content/browser/renderer_host/media/video_capture_provider.h"
#include "content/public/browser/video_capture_device_launcher.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockVideoCaptureProvider : public VideoCaptureProvider {
 public:
  MockVideoCaptureProvider();
  ~MockVideoCaptureProvider() override;

  MOCK_METHOD(void, GetDeviceInfosAsync, (GetDeviceInfosCallback), (override));
  MOCK_METHOD(std::unique_ptr<VideoCaptureDeviceLauncher>,
              CreateDeviceLauncher,
              (),
              (override));
};

class MockVideoCaptureDeviceLauncher : public VideoCaptureDeviceLauncher {
 public:
  MockVideoCaptureDeviceLauncher();
  ~MockVideoCaptureDeviceLauncher() override;

  MOCK_METHOD7(DoLaunchDeviceAsync,
               void(const std::string& device_id,
                    blink::mojom::MediaStreamType stream_type,
                    const media::VideoCaptureParams& params,
                    base::WeakPtr<media::VideoFrameReceiver>* receiver,
                    base::OnceClosure* connection_lost_cb,
                    Callbacks* callbacks,
                    base::OnceClosure* done_cb));

  MOCK_METHOD0(AbortLaunch, void());

  void LaunchDeviceAsync(const std::string& device_id,
                         blink::mojom::MediaStreamType stream_type,
                         const media::VideoCaptureParams& params,
                         base::WeakPtr<media::VideoFrameReceiver> receiver,
                         base::OnceClosure connection_lost_cb,
                         Callbacks* callbacks,
                         base::OnceClosure done_cb) override {
    DoLaunchDeviceAsync(device_id, stream_type, params, &receiver,
                        &connection_lost_cb, callbacks, &done_cb);
  }
};

class MockLaunchedVideoCaptureDevice : public LaunchedVideoCaptureDevice {
 public:
  MockLaunchedVideoCaptureDevice();
  ~MockLaunchedVideoCaptureDevice() override;

  MOCK_CONST_METHOD1(
      DoGetPhotoState,
      void(media::VideoCaptureDevice::GetPhotoStateCallback* callback));
  MOCK_METHOD2(
      DoSetPhotoOptions,
      void(media::mojom::PhotoSettingsPtr* settings,
           media::VideoCaptureDevice::SetPhotoOptionsCallback* callback));
  MOCK_METHOD1(DoTakePhoto,
               void(media::VideoCaptureDevice::TakePhotoCallback* callback));
  MOCK_METHOD0(MaybeSuspendDevice, void());
  MOCK_METHOD0(ResumeDevice, void());
  MOCK_METHOD3(Crop,
               void(const base::Token& crop_id,
                    uint32_t crop_version,
                    base::OnceCallback<void(media::mojom::CropRequestResult)>));
  MOCK_METHOD0(RequestRefreshFrame, void());
  MOCK_METHOD2(DoSetDesktopCaptureWindowId,
               void(gfx::NativeViewId window_id, base::OnceClosure* done_cb));
  MOCK_METHOD1(OnUtilizationReport, void(media::VideoCaptureFeedback));

  void GetPhotoState(
      media::VideoCaptureDevice::GetPhotoStateCallback callback) override {
    DoGetPhotoState(&callback);
  }

  void SetPhotoOptions(
      media::mojom::PhotoSettingsPtr settings,
      media::VideoCaptureDevice::SetPhotoOptionsCallback callback) override {
    DoSetPhotoOptions(&settings, &callback);
  }

  void TakePhoto(
      media::VideoCaptureDevice::TakePhotoCallback callback) override {
    DoTakePhoto(&callback);
  }

  void SetDesktopCaptureWindowIdAsync(gfx::NativeViewId window_id,
                                      base::OnceClosure done_cb) override {
    DoSetDesktopCaptureWindowId(window_id, &done_cb);
  }
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MOCK_VIDEO_CAPTURE_PROVIDER_H_
