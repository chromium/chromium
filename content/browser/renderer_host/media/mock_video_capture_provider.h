// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MOCK_VIDEO_CAPTURE_PROVIDER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MOCK_VIDEO_CAPTURE_PROVIDER_H_

#include "base/functional/callback_forward.h"
#include "base/token.h"
#include "content/browser/renderer_host/media/video_capture_provider.h"
#include "content/public/browser/video_capture_device_launcher.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-forward.h"
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
  MOCK_METHOD(void,
              OpenNativeScreenCapturePicker,
              (DesktopMediaID::Type type,
               base::OnceCallback<void(DesktopMediaID::Id)> created_callback,
               base::OnceCallback<void(webrtc::DesktopCapturer::Source)>
                   picker_callback,
               base::OnceCallback<void()> cancel_callback,
               base::OnceCallback<void()> error_callback),
              (override));
  MOCK_METHOD(void,
              CloseNativeScreenCapturePicker,
              (DesktopMediaID device_id),
              (override));
};

class MockVideoCaptureDeviceLauncher : public VideoCaptureDeviceLauncher {
 public:
  MockVideoCaptureDeviceLauncher();
  ~MockVideoCaptureDeviceLauncher() override;

  MOCK_METHOD(void,
              LaunchDeviceAsync,
              (const std::string& device_id,
               blink::mojom::MediaStreamType stream_type,
               const media::VideoCaptureParams& params,
               base::WeakPtr<media::VideoFrameReceiver> receiver,
               base::OnceClosure connection_lost_cb,
               Callbacks* callbacks,
               base::OnceClosure done_cb,
               mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>
                   video_effects_processor),
              (override));

  MOCK_METHOD(void, AbortLaunch, ());
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
  MOCK_METHOD4(
      ApplySubCaptureTarget,
      void(
          media::mojom::SubCaptureTargetType type,
          const base::Token& target,
          uint32_t sub_capture_target_version,
          base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)>));
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
