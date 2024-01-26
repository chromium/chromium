// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webrtc/media_stream_devices_controller.h"

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_permission_controller.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/public/browser/render_widget_host_view.h"
#include "ui/android/window_android.h"
#endif

using testing::_;

namespace {

blink::MediaStreamDevices CreateFakeDevices(
    blink::mojom::MediaStreamType type) {
  blink::MediaStreamDevices devices;
  devices.reserve(3);
  for (size_t i = 0; i < devices.capacity(); ++i) {
    devices.emplace_back(type, "id_" + base::NumberToString(i),
                         "name " + base::NumberToString(i));
  }
  return devices;
}

class FakeEnumerator : public webrtc::MediaStreamDeviceEnumeratorImpl {
 public:
  FakeEnumerator()
      : audio_capture_devices_(CreateFakeDevices(
            blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE)),
        video_capture_devices_(CreateFakeDevices(
            blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE)) {}

  const blink::MediaStreamDevices& GetAudioCaptureDevices() const override {
    return audio_capture_devices_;
  }

  const blink::MediaStreamDevices& GetVideoCaptureDevices() const override {
    return video_capture_devices_;
  }

 private:
  const blink::MediaStreamDevices audio_capture_devices_;
  const blink::MediaStreamDevices video_capture_devices_;
};

class MockPermissionController : public content::MockPermissionController {
 public:
  MOCK_METHOD(
      void,
      RequestPermissionsFromCurrentDocument,
      (content::RenderFrameHost * render_frame_host,
       content::PermissionRequestDescription request_description,
       base::OnceCallback<
           void(const std::vector<blink::mojom::PermissionStatus>&)> callback));
};

}  // namespace

class MediaStreamDevicesControllerTest : public testing::Test {
  void SetUp() override { InitializeWebContents(); }

  void InitializeWebContents() {
    browser_context_.SetPermissionControllerForTesting(
        std::make_unique<MockPermissionController>());
    web_contents_ =
        test_web_contents_factory_.CreateWebContents(&browser_context_);
    render_frame_host_ = web_contents_->GetPrimaryMainFrame();
    render_frame_host_id_ = render_frame_host_->GetGlobalId();
    content::OverrideLastCommittedOrigin(render_frame_host_, origin_);

#if BUILDFLAG(IS_ANDROID)
    // Create a scoped window so that
    // WebContents::GetNativeView()->GetWindowAndroid() does not return
    // null.
    window_ = ui::WindowAndroid::CreateForTesting();
    window_.get()->get()->AddChild(web_contents_->GetNativeView());
    web_contents_->GetRenderWidgetHostView()->Show();
#endif
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  FakeEnumerator enumerator_;
  content::TestBrowserContext browser_context_;
  content::TestWebContentsFactory test_web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<content::RenderFrameHost> render_frame_host_;
  content::GlobalRenderFrameHostId render_frame_host_id_;
  const url::Origin origin_ = url::Origin::Create(GURL{"https://stuff.com"});

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window_;
#endif
};

TEST_F(MediaStreamDevicesControllerTest, RequestPermissions) {
  auto* mock_permission_controller = static_cast<MockPermissionController*>(
      browser_context_.GetPermissionController());
  ON_CALL(*mock_permission_controller, GetPermissionResultForCurrentDocument)
      .WillByDefault([](blink::PermissionType permission,
                        content::RenderFrameHost* render_frame_host) {
        return content::PermissionResult{
            content::PermissionStatus::GRANTED,
            content::PermissionStatusSource::UNSPECIFIED,
        };
      });

  const auto kRequestedAudioCaptureDevice =
      enumerator_.GetAudioCaptureDevices().back();
  const auto kRequestedVideoCaptureDevice =
      enumerator_.GetVideoCaptureDevices().back();

  content::PermissionRequestDescription expected_description{
      {blink::PermissionType::AUDIO_CAPTURE,
       blink::PermissionType::VIDEO_CAPTURE},
      false};
  expected_description.requested_audio_capture_device_ids = {
      kRequestedAudioCaptureDevice.id};
  expected_description.requested_video_capture_device_ids = {
      kRequestedVideoCaptureDevice.id};

  EXPECT_CALL(*mock_permission_controller,
              RequestPermissionsFromCurrentDocument(render_frame_host_.get(),
                                                    expected_description, _))
      .WillOnce(
          [](auto*, auto,
             base::OnceCallback<void(
                 const std::vector<content::PermissionStatus>&)> callback) {
            std::move(callback).Run({content::PermissionStatus::GRANTED,
                                     content::PermissionStatus::GRANTED});
          });

  blink::MediaStreamDevice returned_audio_capture_device;
  blink::MediaStreamDevice returned_video_capture_device;
  base::test::TestFuture<blink::mojom::MediaStreamRequestResult,
                         blink::mojom::StreamDevicesPtr>
      result_future;
  webrtc::MediaStreamDevicesController::RequestPermissions(
      content::MediaStreamRequest{
          /*render_process_id=*/render_frame_host_id_.child_id,
          /*render_frame_id=*/render_frame_host_id_.frame_routing_id,
          /*page_request_id=*/0, /*url_origin=*/origin_, /*user_gesture=*/false,
          /*request_type=*/
          blink::MediaStreamRequestType::MEDIA_GENERATE_STREAM,
          /*requested_audio_device_id=*/kRequestedAudioCaptureDevice.id,
          /*requested_video_device_id=*/kRequestedVideoCaptureDevice.id,
          blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
          blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
          /*disable_local_echo=*/false,
          /*request_pan_tilt_zoom_permission=*/false},
      &enumerator_,
      base::BindLambdaForTesting(
          [&](const blink::mojom::StreamDevicesSet& stream_devices_set,
              blink::mojom::MediaStreamRequestResult result,
              bool blocked_by_permissions_policy, ContentSetting audio_setting,
              ContentSetting video_setting) {
            CHECK_EQ(stream_devices_set.stream_devices.size(), 1u);
            result_future.SetValue(
                result, stream_devices_set.stream_devices[0]->Clone());
          }));
  auto [result, stream_devices] = result_future.Take();
  ASSERT_EQ(result, blink::mojom::MediaStreamRequestResult::OK);
  ASSERT_TRUE(stream_devices->audio_device.has_value());
  EXPECT_TRUE(
      stream_devices->audio_device->IsSameDevice(kRequestedAudioCaptureDevice));
  ASSERT_TRUE(stream_devices->video_device.has_value());
  EXPECT_TRUE(
      stream_devices->video_device->IsSameDevice(kRequestedVideoCaptureDevice));
}
