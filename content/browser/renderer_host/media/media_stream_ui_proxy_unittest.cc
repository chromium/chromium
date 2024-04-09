// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/gmock_move_support.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_view_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::_;
using testing::Return;
using testing::SaveArg;

namespace content {

namespace {
class MockRenderFrameHostDelegate : public RenderFrameHostDelegate {
 public:
  void RequestMediaAccessPermission(const MediaStreamRequest& request,
                                    MediaResponseCallback callback) override {
    return RequestMediaAccessPermission(request, &callback);
  }
  const blink::web_pref::WebPreferences& GetOrCreateWebPreferences() override {
    return mock_web_preferences_;
  }
  blink::ColorProviderColorMaps GetColorProviderColorMaps() const override {
    return mock_color_provider_colors_;
  }
  MOCK_METHOD2(RequestMediaAccessPermission,
               void(const MediaStreamRequest& request,
                    MediaResponseCallback* callback));
  MOCK_METHOD3(CheckMediaAccessPermission,
               bool(RenderFrameHostImpl* render_frame_host,
                    const url::Origin& security_origin,
                    blink::mojom::MediaStreamType type));

 private:
  blink::web_pref::WebPreferences mock_web_preferences_;
  blink::ColorProviderColorMaps mock_color_provider_colors_;
};

class MockResponseCallback {
 public:
  MOCK_METHOD2(OnAccessRequestResponse,
               void(const blink::mojom::StreamDevicesSet& stream_devices_set,
                    blink::mojom::MediaStreamRequestResult result));
  MOCK_METHOD1(OnCheckResponse, void(bool have_access));
};

class MockMediaStreamUI : public MediaStreamUI {
 public:
  gfx::NativeViewId OnStarted(base::RepeatingClosure stop,
                              MediaStreamUI::SourceCallback source,
                              const std::string& label,
                              std::vector<DesktopMediaID> screen_capture_ids,
                              StateChangeCallback state_change) override {
    return MockOnStarted(std::move(stop), source);
  }

  void OnDeviceStoppedForSourceChange(
      const std::string& label,
      const DesktopMediaID& old_media_id,
      const DesktopMediaID& new_media_id,
      bool captured_surface_control_active) override {}

  void OnDeviceStopped(const std::string& label,
                       const DesktopMediaID& media_id) override {}

  MOCK_METHOD2(MockOnStarted,
               gfx::NativeViewId(base::OnceClosure stop,
                                 MediaStreamUI::SourceCallback source));
};

class MockStopStreamHandler {
 public:
  MOCK_METHOD0(OnStop, void());
  MOCK_METHOD1(OnWindowId, void(gfx::NativeViewId window_id));
};

class MockChangeSourceStreamHandler {
 public:
  MOCK_METHOD2(OnChangeSource,
               void(const DesktopMediaID& media_id,
                    bool captured_surface_control_active));
};

}  // namespace

class MediaStreamUIProxyTest : public testing::Test {
 public:
  MediaStreamUIProxyTest() {
    proxy_ = MediaStreamUIProxy::CreateForTests(&delegate_);
  }

  ~MediaStreamUIProxyTest() override {
    proxy_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  BrowserTaskEnvironment task_environment_;

  MockRenderFrameHostDelegate delegate_;
  MockResponseCallback response_callback_;
  std::unique_ptr<MediaStreamUIProxy> proxy_;
};

MATCHER_P(SameRequest, expected, "") {
  return expected->render_process_id == arg.render_process_id &&
         expected->render_frame_id == arg.render_frame_id &&
         expected->security_origin == arg.security_origin &&
         expected->request_type == arg.request_type &&
         expected->requested_audio_device_ids ==
             arg.requested_audio_device_ids &&
         expected->requested_video_device_ids ==
             arg.requested_video_device_ids &&
         expected->audio_type == arg.audio_type &&
         expected->video_type == arg.video_type;
}

TEST_F(MediaStreamUIProxyTest, Deny) {
  auto request = std::make_unique<MediaStreamRequest>(
      0, 0, 0, url::Origin::Create(GURL("http://origin/")), false,
      blink::MEDIA_GENERATE_STREAM,
      /*requested_audio_device_ids=*/std::vector<std::string>{},
      /*requested_video_device_ids=*/std::vector<std::string>{},
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
      /*disable_local_echo=*/false, /*request_pan_tilt_zoom_permission=*/false,
      /*captured_surface_control_active=*/false);
  MediaStreamRequest* request_ptr = request.get();
  proxy_->RequestAccess(
      std::move(request),
      base::BindOnce(&MockResponseCallback::OnAccessRequestResponse,
                     base::Unretained(&response_callback_)));
  MediaResponseCallback callback;
  EXPECT_CALL(delegate_,
              RequestMediaAccessPermission(SameRequest(request_ptr), _))
      .WillOnce([&callback](testing::Unused, MediaResponseCallback* cb) {
        callback = std::move(*cb);
      });
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(callback.is_null());

  blink::mojom::StreamDevicesSet stream_devices_set;
  stream_devices_set.stream_devices.emplace_back(
      blink::mojom::StreamDevices::New());
  std::move(callback).Run(stream_devices_set,
                          blink::mojom::MediaStreamRequestResult::OK,
                          std::unique_ptr<MediaStreamUI>());

  blink::mojom::StreamDevicesSetPtr response;
  EXPECT_CALL(response_callback_, OnAccessRequestResponse(_, _))
      .WillOnce([&response](const blink::mojom::StreamDevicesSet& arg0,
                            blink::mojom::MediaStreamRequestResult arg1) {
        response = arg0.Clone();
      });
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(blink::ToMediaStreamDevicesList(*response).empty());
}

TEST_F(MediaStreamUIProxyTest, AcceptAndStart) {
  auto request = std::make_unique<MediaStreamRequest>(
      0, 0, 0, url::Origin::Create(GURL("http://origin/")), false,
      blink::MEDIA_GENERATE_STREAM,
      /*requested_audio_device_ids=*/std::vector<std::string>{},
      /*requested_video_device_ids=*/std::vector<std::string>{},
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
      /*disable_local_echo=*/false, /*request_pan_tilt_zoom_permission=*/false,
      /*captured_surface_control_active=*/false);
  MediaStreamRequest* request_ptr = request.get();
  proxy_->RequestAccess(
      std::move(request),
      base::BindOnce(&MockResponseCallback::OnAccessRequestResponse,
                     base::Unretained(&response_callback_)));
  MediaResponseCallback callback;
  EXPECT_CALL(delegate_,
              RequestMediaAccessPermission(SameRequest(request_ptr), _))
      .WillOnce([&callback](testing::Unused, MediaResponseCallback* cb) {
        callback = std::move(*cb);
      });
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(callback.is_null());

  blink::mojom::StreamDevicesSet stream_devices_set;
  stream_devices_set.stream_devices.emplace_back(
      blink::mojom::StreamDevices::New());
  blink::mojom::StreamDevices& devices = *stream_devices_set.stream_devices[0];
  devices.audio_device = blink::MediaStreamDevice(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "Mic", "Mic");
  auto ui = std::make_unique<MockMediaStreamUI>();
  EXPECT_CALL(*ui, MockOnStarted(_, _)).WillOnce(Return(0));
  std::move(callback).Run(stream_devices_set,
                          blink::mojom::MediaStreamRequestResult::OK,
                          std::move(ui));

  blink::mojom::StreamDevicesSetPtr response;
  EXPECT_CALL(response_callback_, OnAccessRequestResponse(_, _))
      .WillOnce([&response](const blink::mojom::StreamDevicesSet& arg0,
                            blink::mojom::MediaStreamRequestResult arg1) {
        response = arg0.Clone();
      });
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(blink::ToMediaStreamDevicesList(*response).empty());

  proxy_->OnStarted(base::OnceClosure(), MediaStreamUI::SourceCallback(),
                    MediaStreamUIProxy::WindowIdCallback(),
                    /*label=*/std::string(), /*screen_share_ids=*/{},
                    MediaStreamUI::StateChangeCallback());
  base::RunLoop().RunUntilIdle();
}

// Verify that the proxy can be deleted before the request is processed.
TEST_F(MediaStreamUIProxyTest, DeleteBeforeAccepted) {
  auto request = std::make_unique<MediaStreamRequest>(
      0, 0, 0, url::Origin::Create(GURL("http://origin/")), false,
      blink::MEDIA_GENERATE_STREAM,
      /*requested_audio_device_ids=*/std::vector<std::string>{},
      /*requested_video_device_ids=*/std::vector<std::string>{},
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
      /*disable_local_echo=*/false, /*request_pan_tilt_zoom_permission=*/false,
      /*captured_surface_control_active=*/false);
  MediaStreamRequest* request_ptr = request.get();
  proxy_->RequestAccess(
      std::move(request),
      base::BindOnce(&MockResponseCallback::OnAccessRequestResponse,
                     base::Unretained(&response_callback_)));
  MediaResponseCallback callback;
  EXPECT_CALL(delegate_,
              RequestMediaAccessPermission(SameRequest(request_ptr), _))
      .WillOnce([&callback](testing::Unused, MediaResponseCallback* cb) {
        callback = std::move(*cb);
      });
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(callback.is_null());

  proxy_.reset();

  std::unique_ptr<MediaStreamUI> ui;
  blink::mojom::StreamDevicesSet stream_devices_set;
  stream_devices_set.stream_devices.emplace_back(
      blink::mojom::StreamDevices::New());
  std::move(callback).Run(stream_devices_set,
                          blink::mojom::MediaStreamRequestResult::OK,
                          std::move(ui));
}

TEST_F(MediaStreamUIProxyTest, StopFromUI) {
  auto request = std::make_unique<MediaStreamRequest>(
      0, 0, 0, url::Origin::Create(GURL("http://origin/")), false,
      blink::MEDIA_GENERATE_STREAM,
      /*requested_audio_device_ids=*/std::vector<std::string>{},
      /*requested_video_device_ids=*/std::vector<std::string>{},
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
      /*disable_local_echo=*/false, /*request_pan_tilt_zoom_permission=*/false,
      /*captured_surface_control_active=*/false);
  MediaStreamRequest* request_ptr = request.get();
  proxy_->RequestAccess(
      std::move(request),
      base::BindOnce(&MockResponseCallback::OnAccessRequestResponse,
                     base::Unretained(&response_callback_)));
  MediaResponseCallback callback;
  EXPECT_CALL(delegate_,
              RequestMediaAccessPermission(SameRequest(request_ptr), _))
      .WillOnce([&callback](testing::Unused, MediaResponseCallback* cb) {
        callback = std::move(*cb);
      });
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(callback.is_null());

  base::OnceClosure stop_callback;

  blink::mojom::StreamDevicesSet stream_devices_set;
  stream_devices_set.stream_devices.emplace_back(
      blink::mojom::StreamDevices::New());
  blink::mojom::StreamDevices& devices = *stream_devices_set.stream_devices[0];
  devices.audio_device = blink::MediaStreamDevice(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "Mic", "Mic");
  auto ui = std::make_unique<MockMediaStreamUI>();
  EXPECT_CALL(*ui, MockOnStarted(_, _))
      .WillOnce([&stop_callback](auto closure, auto) {
        stop_callback = std::move(closure);
        return 0;
      });

  std::move(callback).Run(stream_devices_set,
                          blink::mojom::MediaStreamRequestResult::OK,
                          std::move(ui));

  blink::mojom::StreamDevicesSetPtr response;
  EXPECT_CALL(response_callback_, OnAccessRequestResponse(_, _))
      .WillOnce([&response](const blink::mojom::StreamDevicesSet& arg0,
                            blink::mojom::MediaStreamRequestResult arg1) {
        response = arg0.Clone();
      });
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(blink::ToMediaStreamDevicesList(*response).empty());

  MockStopStreamHandler stop_handler;
  proxy_->OnStarted(base::BindOnce(&MockStopStreamHandler::OnStop,
                                   base::Unretained(&stop_handler)),
                    MediaStreamUI::SourceCallback(),
                    MediaStreamUIProxy::WindowIdCallback(),
                    /*label=*/std::string(), /*screen_share_ids=*/{},
                    MediaStreamUI::StateChangeCallback());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(stop_callback);
  EXPECT_CALL(stop_handler, OnStop());
  std::move(stop_callback).Run();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaStreamUIProxyTest, WindowIdCallbackCalled) {
  auto request = std::make_unique<MediaStreamRequest>(
      0, 0, 0, url::Origin::Create(GURL("http://origin/")), false,
      blink::MEDIA_GENERATE_STREAM,
      /*requested_audio_device_ids=*/std::vector<std::string>{},
      /*requested_video_device_ids=*/std::vector<std::string>{},
      blink::mojom::MediaStreamType::NO_SERVICE,
      blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
      /*disable_local_echo=*/false, /*request_pan_tilt_zoom_permission=*/false,
      /*captured_surface_control_active=*/false);
  MediaStreamRequest* request_ptr = request.get();

  proxy_->RequestAccess(
      std::move(request),
      base::BindOnce(&MockResponseCallback::OnAccessRequestResponse,
                     base::Unretained(&response_callback_)));
  MediaResponseCallback callback;
  EXPECT_CALL(delegate_,
              RequestMediaAccessPermission(SameRequest(request_ptr), _))
      .WillOnce([&callback](testing::Unused, MediaResponseCallback* cb) {
        callback = std::move(*cb);
      });
  base::RunLoop().RunUntilIdle();

  const int kWindowId = 1;
  auto ui = std::make_unique<MockMediaStreamUI>();
  EXPECT_CALL(*ui, MockOnStarted(_, _)).WillOnce(Return(kWindowId));

  blink::mojom::StreamDevicesSet stream_devices_set;
  stream_devices_set.stream_devices.emplace_back(
      blink::mojom::StreamDevices::New());
  std::move(callback).Run(stream_devices_set,
                          blink::mojom::MediaStreamRequestResult::OK,
                          std::move(ui));
  EXPECT_CALL(response_callback_, OnAccessRequestResponse(_, _));

  MockStopStreamHandler handler;
  EXPECT_CALL(handler, OnWindowId(kWindowId));

  proxy_->OnStarted(base::BindOnce(&MockStopStreamHandler::OnStop,
                                   base::Unretained(&handler)),
                    MediaStreamUI::SourceCallback(),
                    base::BindOnce(&MockStopStreamHandler::OnWindowId,
                                   base::Unretained(&handler)),
                    /*label=*/std::string(), /*screen_share_ids=*/{},
                    MediaStreamUI::StateChangeCallback());
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaStreamUIProxyTest, ChangeSourceFromUI) {
  auto request = std::make_unique<MediaStreamRequest>(
      0, 0, 0, url::Origin::Create(GURL("http://origin/")), false,
      blink::MEDIA_GENERATE_STREAM,
      /*requested_audio_device_ids=*/std::vector<std::string>{},
      /*requested_video_device_ids=*/std::vector<std::string>{},
      blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE,
      blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
      /*disable_local_echo=*/false, /*request_pan_tilt_zoom_permission=*/false,
      /*captured_surface_control_active=*/false);
  MediaStreamRequest* request_ptr = request.get();
  proxy_->RequestAccess(
      std::move(request),
      base::BindOnce(&MockResponseCallback::OnAccessRequestResponse,
                     base::Unretained(&response_callback_)));
  MediaResponseCallback callback;
  EXPECT_CALL(delegate_,
              RequestMediaAccessPermission(SameRequest(request_ptr), _))
      .WillOnce([&callback](testing::Unused, MediaResponseCallback* cb) {
        callback = std::move(*cb);
      });
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(callback.is_null());

  MediaStreamUI::SourceCallback source_callback;

  blink::mojom::StreamDevicesSet stream_devices_set;
  stream_devices_set.stream_devices.emplace_back(
      blink::mojom::StreamDevices::New());
  blink::mojom::StreamDevices& devices = *stream_devices_set.stream_devices[0];
  devices.video_device = blink::MediaStreamDevice(
      blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
      "fake_desktop_video_device", "Fake Desktop Video Device");
  auto ui = std::make_unique<MockMediaStreamUI>();
  EXPECT_CALL(*ui, MockOnStarted(_, _)).WillOnce([&](auto, auto callback) {
    source_callback = std::move(callback);
    return 0;
  });
  std::move(callback).Run(stream_devices_set,
                          blink::mojom::MediaStreamRequestResult::OK,
                          std::move(ui));

  blink::mojom::StreamDevicesSetPtr response;
  EXPECT_CALL(response_callback_, OnAccessRequestResponse(_, _))
      .WillOnce([&response](const blink::mojom::StreamDevicesSet& arg0,
                            blink::mojom::MediaStreamRequestResult arg1) {
        response = arg0.Clone();
      });
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(blink::ToMediaStreamDevicesList(*response).empty());

  MockStopStreamHandler stop_handler;
  MockChangeSourceStreamHandler source_handler;
  proxy_->OnStarted(
      base::BindOnce(&MockStopStreamHandler::OnStop,
                     base::Unretained(&stop_handler)),
      base::BindRepeating(&MockChangeSourceStreamHandler::OnChangeSource,
                          base::Unretained(&source_handler)),
      MediaStreamUIProxy::WindowIdCallback(), /*label=*/std::string(),
      /*screen_share_ids=*/{}, MediaStreamUI::StateChangeCallback());
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(source_callback.is_null());
  EXPECT_CALL(source_handler,
              OnChangeSource(DesktopMediaID(),
                             /*captured_surface_control_active=*/false));
  source_callback.Run(DesktopMediaID(),
                      /*captured_surface_control_active=*/false);
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaStreamUIProxyTest, ChangeTabSourceFromUI) {
  auto request = std::make_unique<MediaStreamRequest>(
      0, 0, 0, url::Origin::Create(GURL("http://origin/")), false,
      blink::MEDIA_GENERATE_STREAM,
      /*requested_audio_device_ids=*/std::vector<std::string>{},
      /*requested_video_device_ids=*/std::vector<std::string>{},
      blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE,
      blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
      /*disable_local_echo=*/false, /*request_pan_tilt_zoom_permission=*/false,
      /*captured_surface_control_active=*/false);
  MediaStreamRequest* request_ptr = request.get();
  proxy_->RequestAccess(
      std::move(request),
      base::BindOnce(&MockResponseCallback::OnAccessRequestResponse,
                     base::Unretained(&response_callback_)));
  MediaResponseCallback callback;
  EXPECT_CALL(delegate_,
              RequestMediaAccessPermission(SameRequest(request_ptr), _))
      .WillOnce([&callback](testing::Unused, MediaResponseCallback* cb) {
        callback = std::move(*cb);
      });
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(callback.is_null());

  MediaStreamUI::SourceCallback source_callback;

  blink::mojom::StreamDevicesSet stream_devices_set;
  stream_devices_set.stream_devices.emplace_back(
      blink::mojom::StreamDevices::New());
  blink::mojom::StreamDevices& devices = *stream_devices_set.stream_devices[0];
  devices.video_device = blink::MediaStreamDevice(
      blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
      "fake_tab_video_device", "Fake Tab Video Device");
  base::OnceClosure stop_callback;
  auto ui = std::make_unique<MockMediaStreamUI>();
  EXPECT_CALL(*ui, MockOnStarted(_, _))
      .WillOnce([&source_callback, &stop_callback](auto stop, auto callback) {
        source_callback = std::move(callback);
        stop_callback = std::move(stop);
        return 0;
      });
  std::move(callback).Run(stream_devices_set,
                          blink::mojom::MediaStreamRequestResult::OK,
                          std::move(ui));

  blink::mojom::StreamDevicesSetPtr response;
  EXPECT_CALL(response_callback_, OnAccessRequestResponse(_, _))
      .Times(2)
      .WillRepeatedly([&](const blink::mojom::StreamDevicesSet& arg0,
                          blink::mojom::MediaStreamRequestResult arg1) {
        response = arg0.Clone();
      });
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(blink::ToMediaStreamDevicesList(*response).empty());

  MockStopStreamHandler stop_handler;
  // No stop event should be triggered.
  EXPECT_CALL(stop_handler, OnStop).Times(0);

  MockChangeSourceStreamHandler source_handler;

  proxy_->OnStarted(
      base::BindOnce(&MockStopStreamHandler::OnStop,
                     base::Unretained(&stop_handler)),
      base::BindRepeating(&MockChangeSourceStreamHandler::OnChangeSource,
                          base::Unretained(&source_handler)),
      MediaStreamUIProxy::WindowIdCallback(), /*label=*/std::string(),
      /*screen_share_ids=*/{}, MediaStreamUI::StateChangeCallback());
  base::RunLoop().RunUntilIdle();

  // Switching source tab will trigger another MediaStreamRequest
  request = std::make_unique<MediaStreamRequest>(
      0, 0, 0, url::Origin::Create(GURL("http://origin/")), false,
      blink::MEDIA_GENERATE_STREAM,
      /*requested_audio_device_ids=*/std::vector<std::string>{},
      /*requested_video_device_ids=*/std::vector<std::string>{},
      blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE,
      blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
      /*disable_local_echo=*/false, /*request_pan_tilt_zoom_permission=*/false,
      /*captured_surface_control_active=*/false);
  request_ptr = request.get();
  proxy_->RequestAccess(
      std::move(request),
      base::BindOnce(&MockResponseCallback::OnAccessRequestResponse,
                     base::Unretained(&response_callback_)));
  EXPECT_CALL(delegate_,
              RequestMediaAccessPermission(SameRequest(request_ptr), _))
      .WillOnce([&callback](testing::Unused, MediaResponseCallback* cb) {
        callback = std::move(*cb);
      });
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(callback.is_null());

  ui = std::make_unique<MockMediaStreamUI>();

  devices = blink::mojom::StreamDevices();
  devices.video_device = blink::MediaStreamDevice(
      blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
      "fake_tab_video_device", "Fake Tab Video Device");

  std::move(callback).Run(stream_devices_set,
                          blink::mojom::MediaStreamRequestResult::OK,
                          std::make_unique<MockMediaStreamUI>());

  // Replacing the UI will cause the existing one to trigger the callback.
  ASSERT_FALSE(stop_callback.is_null());
  std::move(stop_callback).Run();

  base::RunLoop().RunUntilIdle();
}

// Basic tests for permissions policy checks through the MediaStreamUIProxy.
// These tests are not meant to cover every edge case as the PermissionsPolicy
// class itself is tested thoroughly in permissions_policy_unittest.cc and in
// render_frame_host_permissions_policy_unittest.cc.
class MediaStreamUIProxyPermissionsPolicyTest
    : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    NavigateAndCommit(GURL("https://example.com"));
  }

 protected:
  // The header policy should only be set once on page load, so we refresh the
  // page to simulate that.
  void RefreshPageAndSetHeaderPolicy(
      blink::mojom::PermissionsPolicyFeature feature) {
    auto navigation = NavigationSimulator::CreateRendererInitiated(
        main_rfh()->GetLastCommittedURL(), main_rfh());
    navigation->SetPermissionsPolicyHeader(
        {{feature, /*allowed_origins=*/{}, /*self_if_matches=*/std::nullopt,
          /*matches_all_origins=*/false,
          /*matches_opaque_src=*/false}});
    navigation->Commit();
  }

  void GetResultForRequest(std::unique_ptr<MediaStreamRequest> request,
                           blink::MediaStreamDevices* devices_out,
                           blink::mojom::MediaStreamRequestResult* result_out) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&MediaStreamUIProxyPermissionsPolicyTest::
                                      GetResultForRequestOnIOThread,
                                  base::Unretained(this), std::move(request)));
    run_loop.Run();
    *devices_out = devices_;
    *result_out = result_;
  }

  std::unique_ptr<MediaStreamRequest> CreateRequest(
      RenderFrameHost* rfh,
      blink::mojom::MediaStreamType mic_type,
      blink::mojom::MediaStreamType cam_type) {
    return std::make_unique<MediaStreamRequest>(
        rfh->GetProcess()->GetID(), rfh->GetRoutingID(), 0,
        url::Origin::Create(rfh->GetLastCommittedURL()), false,
        blink::MEDIA_GENERATE_STREAM,
        /*requested_audio_device_ids=*/std::vector<std::string>{},
        /*requested_video_device_ids=*/std::vector<std::string>{}, mic_type,
        cam_type,
        /*disable_local_echo=*/false,
        /*request_pan_tilt_zoom_permission=*/false,
        /*captured_surface_control_active=*/false);
  }

 private:
  class TestRFHDelegate : public RenderFrameHostDelegate {
   public:
    void RequestMediaAccessPermission(const MediaStreamRequest& request,
                                      MediaResponseCallback callback) override {
      blink::mojom::StreamDevicesSet stream_devices_set;
      stream_devices_set.stream_devices.emplace_back(
          blink::mojom::StreamDevices::New());
      blink::mojom::StreamDevices& devices =
          *stream_devices_set.stream_devices[0];
      if (request.audio_type ==
          blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
        devices.audio_device = blink::MediaStreamDevice(
            blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "Mic", "Mic");
      }
      if (request.video_type ==
          blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
        devices.video_device = blink::MediaStreamDevice(
            blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, "Camera",
            "Camera");
      }
      auto ui = std::make_unique<MockMediaStreamUI>();
      std::move(callback).Run(stream_devices_set,
                              blink::mojom::MediaStreamRequestResult::OK,
                              std::move(ui));
    }

    const blink::web_pref::WebPreferences& GetOrCreateWebPreferences()
        override {
      return mock_web_preferences_;
    }

    blink::ColorProviderColorMaps GetColorProviderColorMaps() const override {
      return mock_color_provider_colors_;
    }

   private:
    blink::web_pref::WebPreferences mock_web_preferences_;
    blink::ColorProviderColorMaps mock_color_provider_colors_;
  };

  void GetResultForRequestOnIOThread(
      std::unique_ptr<MediaStreamRequest> request) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    proxy_ = MediaStreamUIProxy::CreateForTests(&delegate_);
    proxy_->RequestAccess(
        std::move(request),
        base::BindOnce(&MediaStreamUIProxyPermissionsPolicyTest::
                           FinishedGetResultOnIOThread,
                       base::Unretained(this)));
  }

  void FinishedGetResultOnIOThread(
      const blink::mojom::StreamDevicesSet& stream_devices_set,
      blink::mojom::MediaStreamRequestResult result) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    proxy_.reset();
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &MediaStreamUIProxyPermissionsPolicyTest::FinishedGetResult,
            base::Unretained(this), stream_devices_set.Clone(), result));
  }

  void FinishedGetResult(blink::mojom::StreamDevicesSetPtr stream_devices_set,
                         blink::mojom::MediaStreamRequestResult result) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    devices_ = blink::ToMediaStreamDevicesList(*stream_devices_set);
    result_ = result;
    std::move(quit_closure_).Run();
  }

  // These should only be accessed on the UI thread.
  blink::MediaStreamDevices devices_;
  blink::mojom::MediaStreamRequestResult result_;
  base::OnceClosure quit_closure_;

  // These should only be accessed on the IO thread.
  TestRFHDelegate delegate_;
  std::unique_ptr<MediaStreamUIProxy> proxy_;
};

TEST_F(MediaStreamUIProxyPermissionsPolicyTest, PermissionsPolicy) {
  blink::MediaStreamDevices devices;
  blink::mojom::MediaStreamRequestResult result;

  // Default FP.
  GetResultForRequest(
      CreateRequest(main_rfh(),
                    blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
                    blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE),
      &devices, &result);
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, result);
  ASSERT_EQ(2u, devices.size());
  EXPECT_EQ(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
            devices[0].type);
  EXPECT_EQ(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
            devices[1].type);

  // Mic disabled.
  RefreshPageAndSetHeaderPolicy(
      blink::mojom::PermissionsPolicyFeature::kMicrophone);
  GetResultForRequest(
      CreateRequest(main_rfh(),
                    blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
                    blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE),
      &devices, &result);
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, result);
  ASSERT_EQ(1u, devices.size());
  EXPECT_EQ(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
            devices[0].type);

  // Camera disabled.
  RefreshPageAndSetHeaderPolicy(
      blink::mojom::PermissionsPolicyFeature::kCamera);
  GetResultForRequest(
      CreateRequest(main_rfh(),
                    blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
                    blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE),
      &devices, &result);
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, result);
  ASSERT_EQ(1u, devices.size());
  EXPECT_EQ(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
            devices[0].type);

  // Camera disabled resulting in no devices being returned.
  RefreshPageAndSetHeaderPolicy(
      blink::mojom::PermissionsPolicyFeature::kCamera);
  GetResultForRequest(
      CreateRequest(main_rfh(), blink::mojom::MediaStreamType::NO_SERVICE,
                    blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE),
      &devices, &result);
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED, result);
  ASSERT_EQ(0u, devices.size());
}

}  // namespace content
