// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MOCK_VIDEO_CAPTURE_PROVIDER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MOCK_VIDEO_CAPTURE_PROVIDER_H_

#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
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

  MOCK_METHOD(void,
              DoGetDeviceInfosAsync,
              (media::mojom::VideoCaptureDeviceEnumerationMode enumeration_mode,
               GetDeviceInfosCallback result_callback));
  void GetDeviceInfosAsync(
      media::mojom::VideoCaptureDeviceEnumerationMode enumeration_mode,
      GetDeviceInfosCallback result_callback) override;
  std::optional<media::mojom::VideoCaptureDeviceEnumerationMode>
  last_enumeration_mode() const {
    if (enumeration_modes_.empty()) {
      return std::nullopt;
    }
    return enumeration_modes_.back();
  }
  const std::vector<media::mojom::VideoCaptureDeviceEnumerationMode>&
  enumeration_modes() const {
    return enumeration_modes_;
  }
  void clear_enumeration_modes() { enumeration_modes_.clear(); }
  MOCK_METHOD(std::unique_ptr<VideoCaptureDeviceLauncher>,
              CreateDeviceLauncher,
              (),
              (override));
  MOCK_METHOD(
      void,
      OpenNativeScreenCapturePicker,
      (DesktopMediaID::Type type,
       base::OnceCallback<void(DesktopMediaID::Id)> created_callback,
       base::OnceCallback<void(webrtc::DesktopCapturer::Source)>
           picker_callback,
       base::OnceCallback<void()> cancel_callback,
       base::OnceCallback<void()> error_callback,
       base::OnceCallback<void(DesktopMediaID::Id)> stop_audio_callback),
      (override));
  MOCK_METHOD(void,
              CloseNativeScreenCapturePicker,
              (DesktopMediaID device_id),
              (override));
#if BUILDFLAG(IS_MAC)
  MOCK_METHOD(
      void,
      GetApplicationAudioCaptureId,
      (DesktopMediaID::Id session_id,
       base::OnceCallback<void(
           const std::optional<desktop_capture::ApplicationAudioCaptureId>&)>
           callback),
      (override));
#endif

 private:
  std::vector<media::mojom::VideoCaptureDeviceEnumerationMode>
      enumeration_modes_;
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
               base::OnceClosure done_cb),
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
  MOCK_METHOD0(InvalidateBuffers, void());
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
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MOCK_VIDEO_CAPTURE_PROVIDER_H_
