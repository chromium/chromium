// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/test/browser_task_environment.h"
#include "content/test/test_render_frame_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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
                                    MediaResponseCallback callback) {
    return RequestMediaAccessPermission(request, &callback);
  }
  MOCK_METHOD2(RequestMediaAccessPermission,
               void(const MediaStreamRequest& request,
                    MediaResponseCallback* callback));
  MOCK_METHOD3(CheckMediaAccessPermission,
               bool(RenderFrameHost* render_frame_host,
                    const url::Origin& security_origin,
                    blink::mojom::MediaStreamType type));
};

class MockResponseCallback {
 public:
  MOCK_METHOD2(OnAccessRequestResponse,
               void(const blink::MediaStreamDevices& devices,
                    blink::mojom::MediaStreamRequestResult result));
  MOCK_METHOD1(OnCheckResponse, void(bool have_access));
};

class MockMediaStreamUI : public MediaStreamUI {
 public:
  gfx::NativeViewId OnStarted(base::OnceClosure stop,
                              MediaStreamUI::SourceCallback source) override {
    // gmock cannot handle move-only types:
    return MockOnStarted(base::AdaptCallbackForRepeating(std::move(stop)),
                         source);
  }

  MOCK_METHOD2(MockOnStarted,
               gfx::NativeViewId(base::RepeatingClosure stop,
                                 MediaStreamUI::SourceCallback source));
};

class MockStopStreamHandler {
 public:
  MOCK_METHOD0(OnStop, void());
  MOCK_METHOD1(OnWindowId, void(gfx::NativeViewId window_id));
};

class MockChangeSourceStreamHandler {
 public:
  MOCK_METHOD1(OnChangeSource, void(const DesktopMediaID& media_id));
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
  return
    expected->render_process_id == arg.render_process_id &&
    expected->render_frame_id == arg.render_frame_id &&
    expected->security_origin == arg.security_origin &&
    expected->request_type == arg.request_type &&
    expected->requested_audio_device_id == arg.requested_audio_device_id &&
    expected->requested_video_device_id == arg.requested_video_device_id &&
    expected->audio_type == arg.audio_type &&
    expected->video_type == arg.video_type;
}

TEST_F(MediaStreamUIProxyTest, Deny) {
  auto request = std::make_unique<MediaStreamRequest>(
      0, 0, 0, GURL("http://origin/"), false, blink::MEDIA_GENERATE_STREAM,
      std::string(), std::string(),
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, false);
  MediaStreamRequest* request_ptr = request.get();
  proxy_->RequestAccess(
      std::move(request),
      base::BindOnce(&MockResponseCallback::OnAccessRequestResponse,
                     base::Unretained(&response_callback_)));
  MediaResponseCallback callback;
  EXPECT_CALL(delegate_,
              RequestMediaAccessPermission(SameRequest(request_ptr), _))
      .WillOnce([&](testing::Unused, MediaResponseCallback* cb) {
        callback = std::move(*cb);
      });
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(callback.is_null());

  blink::MediaStreamDevices devices;
  std::move(callback).Run(devices, blink::mojom::MediaStreamRequestResult::OK,
                          std::unique_ptr<MediaStreamUI>());

  blink::MediaStreamDevices response;
  EXPECT_CALL(response_callback_, OnAccessRequestResponse(_, _))
    .WillOnce(SaveArg<0>(&response));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(response.empty());
}

TEST_F(MediaStreamUIProxyTest, AcceptAndStart) {
  auto request = std::make_unique<MediaStreamRequest>(
      0, 0, 0, GURL("http://origin/"), false, blink::MEDIA_GENERATE_STREAM,
      std::string(), std::string(),
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, false);
  MediaStreamRequest* request_ptr = request.get();
  proxy_->RequestAccess(
      std::move(request),
      base::BindOnce(&MockResponseCallback::OnAccessRequestResponse,
                     base::Unretained(&response_callback_)));
  MediaResponseCallback callback;
  EXPECT_CALL(delegate_,
              RequestMediaAccessPermission(SameRequest(request_ptr), _))
      .WillOnce([&](testing::Unused, MediaResponseCallback* cb) {
        callback = std::move(*cb);
      });
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(callback.is_null());

  blink::MediaStreamDevices devices;
  devices.push_back(blink::MediaStreamDevice(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "Mic", "Mic"));
  auto ui = std::make_unique<MockMediaStreamUI>();
  EXPECT_CALL(*ui, MockOnStarted(_, _)).WillOnce(Return(0));
  std::move(callback).Run(devices, blink::mojom::MediaStreamRequestResult::OK,
                          std::move(ui));

  blink::MediaStreamDevices response;
  EXPECT_CALL(response_callback_, OnAccessRequestResponse(_, _))
    .WillOnce(SaveArg<0>(&response));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(response.empty());

  proxy_->OnStarted(base::OnceClosure(), MediaStreamUI::SourceCallback(),
                    MediaStreamUIProxy::WindowIdCallback());
  base::RunLoop().RunUntilIdle();
}

// Verify that the proxy can be deleted before the request is processed.
TEST_F(MediaStreamUIProxyTest, DeleteBeforeAccepted) {
  auto request = std::make_unique<MediaStreamRequest>(
      0, 0, 0, GURL("http://origin/"), false, blink::MEDIA_GENERATE_STREAM,
      std::string(), std::string(),
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, false);
  MediaStreamRequest* request_ptr = request.get();
  proxy_->RequestAccess(
      std::move(request),
      base::BindOnce(&MockResponseCallback::OnAccessRequestResponse,
                     base::Unretained(&response_callback_)));
  MediaResponseCallback callback;
  EXPECT_CALL(delegate_,
              RequestMediaAccessPermission(SameRequest(request_ptr), _))
      .WillOnce([&](testing::Unused, MediaResponseCallback* cb) {
        callback = std::move(*cb);
      });
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(callback.is_null());

  proxy_.reset();

  blink::MediaStreamDevices devices;
  std::unique_ptr<MediaStreamUI> ui;
  std::move(callback).Run(devices, blink::mojom::MediaStreamRequestResult::OK,
                          std::move(ui));
}

TEST_F(MediaStreamUIProxyTest, StopFromUI) {
  auto request = std::make_unique<MediaStreamRequest>(
      0, 0, 0, GURL("http://origin/"), false, blink::MEDIA_GENERATE_STREAM,
      std::string(), std::string(),
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, false);
  MediaStreamRequest* request_ptr = request.get();
  proxy_->RequestAccess(
      std::move(request),
      base::BindOnce(&MockResponseCallback::OnAccessRequestResponse,
                     base::Unretained(&response_callback_)));
  MediaResponseCallback callback;
  EXPECT_CALL(delegate_,
              RequestMediaAccessPermission(SameRequest(request_ptr), _))
      .WillOnce([&](testing::Unused, MediaResponseCallback* cb) {
        callback = std::move(*cb);
      });
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(callback.is_null());

  base::Closure stop_callback;

  blink::MediaStreamDevices devices;
  devices.push_back(blink::MediaStreamDevice(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "Mic", "Mic"));
  auto ui = std::make_unique<MockMediaStreamUI>();
  EXPECT_CALL(*ui, MockOnStarted(_, _))
      .WillOnce(testing::DoAll(SaveArg<0>(&stop_callback), Return(0)));
  std::move(callback).Run(devices, blink::mojom::MediaStreamRequestResult::OK,
                          std::move(ui));

  blink::MediaStreamDevices response;
  EXPECT_CALL(response_callback_, OnAccessRequestResponse(_, _))
    .WillOnce(SaveArg<0>(&response));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(response.empty());

  MockStopStreamHandler stop_handler;
  proxy_->OnStarted(base::BindOnce(&MockStopStreamHandler::OnStop,
                                   base::Unretained(&stop_handler)),
                    MediaStreamUI::SourceCallback(),
                    MediaStreamUIProxy::WindowIdCallback());
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(stop_callback.is_null());
  EXPECT_CALL(stop_handler, OnStop());
  stop_callback.Run();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaStreamUIProxyTest, WindowIdCallbackCalled) {
  auto request = std::make_unique<MediaStreamRequest>(
      0, 0, 0, GURL("http://origin/"), false, blink::MEDIA_GENERATE_STREAM,
      std::string(), std::string(), blink::mojom::MediaStreamType::NO_SERVICE,
      blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE, false);
  MediaStreamRequest* request_ptr = request.get();

  proxy_->RequestAccess(
      std::move(request),
      base::BindOnce(&MockResponseCallback::OnAccessRequestResponse,
                     base::Unretained(&response_callback_)));
  MediaResponseCallback callback;
  EXPECT_CALL(delegate_,
              RequestMediaAccessPermission(SameRequest(request_ptr), _))
      .WillOnce([&](testing::Unused, MediaResponseCallback* cb) {
        callback = std::move(*cb);
      });
  base::RunLoop().RunUntilIdle();

  const int kWindowId = 1;
  auto ui = std::make_unique<MockMediaStreamUI>();
  EXPECT_CALL(*ui, MockOnStarted(_, _)).WillOnce(Return(kWindowId));

  std::move(callback).Run(blink::MediaStreamDevices(),
                          blink::mojom::MediaStreamRequestResult::OK,
                          std::move(ui));
  EXPECT_CALL(response_callback_, OnAccessRequestResponse(_, _));

  MockStopStreamHandler handler;
  EXPECT_CALL(handler, OnWindowId(kWindowId));

  proxy_->OnStarted(base::BindOnce(&MockStopStreamHandler::OnStop,
                                   base::Unretained(&handler)),
                    MediaStreamUI::SourceCallback(),
                    base::BindOnce(&MockStopStreamHandler::OnWindowId,
                                   base::Unretained(&handler)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaStreamUIProxyTest, ChangeSourceFromUI) {
  auto request = std::make_unique<MediaStreamRequest>(
      0, 0, 0, GURL("http://origin/"), false, blink::MEDIA_GENERATE_STREAM,
      std::string(), std::string(),
      blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE,
      blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE, false);
  MediaStreamRequest* request_ptr = request.get();
  proxy_->RequestAccess(
      std::move(request),
      base::BindOnce(&MockResponseCallback::OnAccessRequestResponse,
                     base::Unretained(&response_callback_)));
  MediaResponseCallback callback;
  EXPECT_CALL(delegate_,
              RequestMediaAccessPermission(SameRequest(request_ptr), _))
      .WillOnce([&](testing::Unused, MediaResponseCallback* cb) {
        callback = std::move(*cb);
      });
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(callback.is_null());

  MediaStreamUI::SourceCallback source_callback;

  blink::MediaStreamDevices devices;
  devices.push_back(blink::MediaStreamDevice(
      blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
      "fake_desktop_video_device", "Fake Desktop Video Device"));
  auto ui = std::make_unique<MockMediaStreamUI>();
  EXPECT_CALL(*ui, MockOnStarted(_, _))
      .WillOnce(testing::DoAll(SaveArg<1>(&source_callback), Return(0)));
  std::move(callback).Run(devices, blink::mojom::MediaStreamRequestResult::OK,
                          std::move(ui));

  blink::MediaStreamDevices response;
  EXPECT_CALL(response_callback_, OnAccessRequestResponse(_, _))
      .WillOnce(SaveArg<0>(&response));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(response.empty());

  MockStopStreamHandler stop_handler;
  MockChangeSourceStreamHandler source_handler;
  proxy_->OnStarted(
      base::BindOnce(&MockStopStreamHandler::OnStop,
                     base::Unretained(&stop_handler)),
      base::BindRepeating(&MockChangeSourceStreamHandler::OnChangeSource,
                          base::Unretained(&source_handler)),
      MediaStreamUIProxy::WindowIdCallback());
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(source_callback.is_null());
  EXPECT_CALL(source_handler, OnChangeSource(DesktopMediaID()));
  source_callback.Run(DesktopMediaID());
  base::RunLoop().RunUntilIdle();
}

// Basic tests for feature policy checks through the MediaStreamUIProxy. These
// tests are not meant to cover every edge case as the FeaturePolicy class
// itself is tested thoroughly in feature_policy_unittest.cc and in
// render_frame_host_feature_policy_unittest.cc.
class MediaStreamUIProxyFeaturePolicyTest
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
      blink::mojom::FeaturePolicyFeature feature) {
    NavigateAndCommit(main_rfh()->GetLastCommittedURL());
    std::vector<url::Origin> empty_allowlist;
    RenderFrameHostTester::For(main_rfh())
        ->SimulateFeaturePolicyHeader(feature, empty_allowlist);
  }

  void GetResultForRequest(std::unique_ptr<MediaStreamRequest> request,
                           blink::MediaStreamDevices* devices_out,
                           blink::mojom::MediaStreamRequestResult* result_out) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(
            &MediaStreamUIProxyFeaturePolicyTest::GetResultForRequestOnIOThread,
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
        rfh->GetLastCommittedURL(), false, blink::MEDIA_GENERATE_STREAM,
        std::string(), std::string(), mic_type, cam_type, false);
  }

 private:
  class TestRFHDelegate : public RenderFrameHostDelegate {
    void RequestMediaAccessPermission(const MediaStreamRequest& request,
                                      MediaResponseCallback callback) override {
      blink::MediaStreamDevices devices;
      if (request.audio_type ==
          blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
        devices.push_back(blink::MediaStreamDevice(
            blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "Mic", "Mic"));
      }
      if (request.video_type ==
          blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
        devices.push_back(blink::MediaStreamDevice(
            blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, "Camera",
            "Camera"));
      }
      auto ui = std::make_unique<MockMediaStreamUI>();
      std::move(callback).Run(
          devices, blink::mojom::MediaStreamRequestResult::OK, std::move(ui));
    }
  };

  void GetResultForRequestOnIOThread(
      std::unique_ptr<MediaStreamRequest> request) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    proxy_ = MediaStreamUIProxy::CreateForTests(&delegate_);
    proxy_->RequestAccess(
        std::move(request),
        base::BindOnce(
            &MediaStreamUIProxyFeaturePolicyTest::FinishedGetResultOnIOThread,
            base::Unretained(this)));
  }

  void FinishedGetResultOnIOThread(
      const blink::MediaStreamDevices& devices,
      blink::mojom::MediaStreamRequestResult result) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    proxy_.reset();
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&MediaStreamUIProxyFeaturePolicyTest::FinishedGetResult,
                       base::Unretained(this), devices, result));
  }

  void FinishedGetResult(const blink::MediaStreamDevices& devices,
                         blink::mojom::MediaStreamRequestResult result) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    devices_ = devices;
    result_ = result;
    quit_closure_.Run();
  }

  // These should only be accessed on the UI thread.
  blink::MediaStreamDevices devices_;
  blink::mojom::MediaStreamRequestResult result_;
  base::Closure quit_closure_;

  // These should only be accessed on the IO thread.
  TestRFHDelegate delegate_;
  std::unique_ptr<MediaStreamUIProxy> proxy_;
};

TEST_F(MediaStreamUIProxyFeaturePolicyTest, FeaturePolicy) {
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
      blink::mojom::FeaturePolicyFeature::kMicrophone);
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
  RefreshPageAndSetHeaderPolicy(blink::mojom::FeaturePolicyFeature::kCamera);
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
  RefreshPageAndSetHeaderPolicy(blink::mojom::FeaturePolicyFeature::kCamera);
  GetResultForRequest(
      CreateRequest(main_rfh(), blink::mojom::MediaStreamType::NO_SERVICE,
                    blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE),
      &devices, &result);
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED, result);
  ASSERT_EQ(0u, devices.size());
}

}  // namespace content
