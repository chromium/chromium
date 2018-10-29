// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"

#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/test_browser_thread_bundle.h"
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
                    MediaStreamType type));
};

class MockResponseCallback {
 public:
  MOCK_METHOD2(OnAccessRequestResponse,
               void(const MediaStreamDevices& devices,
               content::MediaStreamRequestResult result));
  MOCK_METHOD1(OnCheckResponse, void(bool have_access));
};

class MockMediaStreamUI : public MediaStreamUI {
 public:
  MOCK_METHOD1(OnStarted, gfx::NativeViewId(const base::Closure& stop));
};

class MockStopStreamHandler {
 public:
  MOCK_METHOD0(OnStop, void());
  MOCK_METHOD1(OnWindowId, void(gfx::NativeViewId window_id));
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
  TestBrowserThreadBundle thread_bundle_;

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

// These tests are flaky on Linux. https://crbug.com/826483
#if defined(OS_LINUX)
#define MAYBE_DeleteBeforeAccepted DISABLED_DeleteBeforeAccepted
#define MAYBE_Deny DISABLED_Deny
#define MAYBE_AcceptAndStart DISABLED_AcceptAndStart
#define MAYBE_StopFromUI DISABLED_StopFromUI
#define MAYBE_WindowIdCallbackCalled DISABLED_WindowIdCallbackCalled
#else
#define MAYBE_DeleteBeforeAccepted DeleteBeforeAccepted
#define MAYBE_Deny Deny
#define MAYBE_AcceptAndStart AcceptAndStart
#define MAYBE_StopFromUI StopFromUI
#define MAYBE_WindowIdCallbackCalled WindowIdCallbackCalled
#endif

TEST_F(MediaStreamUIProxyTest, MAYBE_Deny) {
  std::unique_ptr<MediaStreamRequest> request(new MediaStreamRequest(
      0, 0, 0, GURL("http://origin/"), false, MEDIA_GENERATE_STREAM,
      std::string(), std::string(), MEDIA_DEVICE_AUDIO_CAPTURE,
      MEDIA_DEVICE_VIDEO_CAPTURE, false));
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

  MediaStreamDevices devices;
  std::move(callback).Run(devices, MEDIA_DEVICE_OK,
                          std::unique_ptr<MediaStreamUI>());

  MediaStreamDevices response;
  EXPECT_CALL(response_callback_, OnAccessRequestResponse(_, _))
    .WillOnce(SaveArg<0>(&response));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(response.empty());
}

TEST_F(MediaStreamUIProxyTest, MAYBE_AcceptAndStart) {
  std::unique_ptr<MediaStreamRequest> request(new MediaStreamRequest(
      0, 0, 0, GURL("http://origin/"), false, MEDIA_GENERATE_STREAM,
      std::string(), std::string(), MEDIA_DEVICE_AUDIO_CAPTURE,
      MEDIA_DEVICE_VIDEO_CAPTURE, false));
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

  MediaStreamDevices devices;
  devices.push_back(
      MediaStreamDevice(MEDIA_DEVICE_AUDIO_CAPTURE, "Mic", "Mic"));
  std::unique_ptr<MockMediaStreamUI> ui(new MockMediaStreamUI());
  EXPECT_CALL(*ui, OnStarted(_)).WillOnce(Return(0));
  std::move(callback).Run(devices, MEDIA_DEVICE_OK, std::move(ui));

  MediaStreamDevices response;
  EXPECT_CALL(response_callback_, OnAccessRequestResponse(_, _))
    .WillOnce(SaveArg<0>(&response));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(response.empty());

  proxy_->OnStarted(base::Closure(), MediaStreamUIProxy::WindowIdCallback());
  base::RunLoop().RunUntilIdle();
}

// Verify that the proxy can be deleted before the request is processed.
TEST_F(MediaStreamUIProxyTest, MAYBE_DeleteBeforeAccepted) {
  std::unique_ptr<MediaStreamRequest> request(new MediaStreamRequest(
      0, 0, 0, GURL("http://origin/"), false, MEDIA_GENERATE_STREAM,
      std::string(), std::string(), MEDIA_DEVICE_AUDIO_CAPTURE,
      MEDIA_DEVICE_VIDEO_CAPTURE, false));
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

  MediaStreamDevices devices;
  std::unique_ptr<MediaStreamUI> ui;
  std::move(callback).Run(devices, MEDIA_DEVICE_OK, std::move(ui));
}

TEST_F(MediaStreamUIProxyTest, MAYBE_StopFromUI) {
  std::unique_ptr<MediaStreamRequest> request(new MediaStreamRequest(
      0, 0, 0, GURL("http://origin/"), false, MEDIA_GENERATE_STREAM,
      std::string(), std::string(), MEDIA_DEVICE_AUDIO_CAPTURE,
      MEDIA_DEVICE_VIDEO_CAPTURE, false));
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

  MediaStreamDevices devices;
  devices.push_back(
      MediaStreamDevice(MEDIA_DEVICE_AUDIO_CAPTURE, "Mic", "Mic"));
  std::unique_ptr<MockMediaStreamUI> ui(new MockMediaStreamUI());
  EXPECT_CALL(*ui, OnStarted(_))
      .WillOnce(testing::DoAll(SaveArg<0>(&stop_callback), Return(0)));
  std::move(callback).Run(devices, MEDIA_DEVICE_OK, std::move(ui));

  MediaStreamDevices response;
  EXPECT_CALL(response_callback_, OnAccessRequestResponse(_, _))
    .WillOnce(SaveArg<0>(&response));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(response.empty());

  MockStopStreamHandler stop_handler;
  proxy_->OnStarted(base::BindOnce(&MockStopStreamHandler::OnStop,
                                   base::Unretained(&stop_handler)),
                    MediaStreamUIProxy::WindowIdCallback());
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(stop_callback.is_null());
  EXPECT_CALL(stop_handler, OnStop());
  stop_callback.Run();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaStreamUIProxyTest, MAYBE_WindowIdCallbackCalled) {
  std::unique_ptr<MediaStreamRequest> request(new MediaStreamRequest(
      0, 0, 0, GURL("http://origin/"), false, MEDIA_GENERATE_STREAM,
      std::string(), std::string(), MEDIA_NO_SERVICE,
      MEDIA_GUM_DESKTOP_VIDEO_CAPTURE, false));
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
  std::unique_ptr<MockMediaStreamUI> ui(new MockMediaStreamUI());
  EXPECT_CALL(*ui, OnStarted(_)).WillOnce(Return(kWindowId));

  std::move(callback).Run(MediaStreamDevices(), MEDIA_DEVICE_OK, std::move(ui));
  EXPECT_CALL(response_callback_, OnAccessRequestResponse(_, _));

  MockStopStreamHandler handler;
  EXPECT_CALL(handler, OnWindowId(kWindowId));

  proxy_->OnStarted(base::BindOnce(&MockStopStreamHandler::OnStop,
                                   base::Unretained(&handler)),
                    base::BindOnce(&MockStopStreamHandler::OnWindowId,
                                   base::Unretained(&handler)));
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
  void RefreshPageAndSetHeaderPolicy(RenderFrameHost* rfh,
                                     blink::mojom::FeaturePolicyFeature feature,
                                     bool enabled) {
    NavigateAndCommit(rfh->GetLastCommittedURL());
    std::vector<url::Origin> whitelist;
    if (enabled)
      whitelist.push_back(rfh->GetLastCommittedOrigin());
    RenderFrameHostTester::For(rfh)->SimulateFeaturePolicyHeader(feature,
                                                                 whitelist);
  }

  void GetResultForRequest(std::unique_ptr<MediaStreamRequest> request,
                           MediaStreamDevices* devices_out,
                           MediaStreamRequestResult* result_out) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(
            &MediaStreamUIProxyFeaturePolicyTest::GetResultForRequestOnIOThread,
            base::Unretained(this), std::move(request)));
    run_loop.Run();
    *devices_out = devices_;
    *result_out = result_;
  }

  std::unique_ptr<MediaStreamRequest> CreateRequest(RenderFrameHost* rfh,
                                                    MediaStreamType mic_type,
                                                    MediaStreamType cam_type) {
    return std::make_unique<MediaStreamRequest>(
        rfh->GetProcess()->GetID(), rfh->GetRoutingID(), 0,
        rfh->GetLastCommittedURL(), false, MEDIA_GENERATE_STREAM, std::string(),
        std::string(), mic_type, cam_type, false);
  }

 private:
  class TestRFHDelegate : public RenderFrameHostDelegate {
    void RequestMediaAccessPermission(const MediaStreamRequest& request,
                                      MediaResponseCallback callback) override {
      MediaStreamDevices devices;
      if (request.audio_type == MEDIA_DEVICE_AUDIO_CAPTURE) {
        devices.push_back(
            MediaStreamDevice(MEDIA_DEVICE_AUDIO_CAPTURE, "Mic", "Mic"));
      }
      if (request.video_type == MEDIA_DEVICE_VIDEO_CAPTURE) {
        devices.push_back(
            MediaStreamDevice(MEDIA_DEVICE_VIDEO_CAPTURE, "Camera", "Camera"));
      }
      std::unique_ptr<MockMediaStreamUI> ui(new MockMediaStreamUI());
      std::move(callback).Run(devices, MEDIA_DEVICE_OK, std::move(ui));
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

  void FinishedGetResultOnIOThread(const MediaStreamDevices& devices,
                                   MediaStreamRequestResult result) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    proxy_.reset();
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&MediaStreamUIProxyFeaturePolicyTest::FinishedGetResult,
                       base::Unretained(this), devices, result));
  }

  void FinishedGetResult(const MediaStreamDevices& devices,
                         MediaStreamRequestResult result) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    devices_ = devices;
    result_ = result;
    quit_closure_.Run();
  }

  // These should only be accessed on the UI thread.
  MediaStreamDevices devices_;
  MediaStreamRequestResult result_;
  base::Closure quit_closure_;

  // These should only be accessed on the IO thread.
  TestRFHDelegate delegate_;
  std::unique_ptr<MediaStreamUIProxy> proxy_;
};

TEST_F(MediaStreamUIProxyFeaturePolicyTest, FeaturePolicy) {
  MediaStreamDevices devices;
  MediaStreamRequestResult result;

  // Default FP.
  GetResultForRequest(CreateRequest(main_rfh(), MEDIA_DEVICE_AUDIO_CAPTURE,
                                    MEDIA_DEVICE_VIDEO_CAPTURE),
                      &devices, &result);
  EXPECT_EQ(MEDIA_DEVICE_OK, result);
  ASSERT_EQ(2u, devices.size());
  EXPECT_EQ(MEDIA_DEVICE_AUDIO_CAPTURE, devices[0].type);
  EXPECT_EQ(MEDIA_DEVICE_VIDEO_CAPTURE, devices[1].type);

  // Mic disabled.
  RefreshPageAndSetHeaderPolicy(main_rfh(),
                                blink::mojom::FeaturePolicyFeature::kMicrophone,
                                /*enabled=*/false);
  GetResultForRequest(CreateRequest(main_rfh(), MEDIA_DEVICE_AUDIO_CAPTURE,
                                    MEDIA_DEVICE_VIDEO_CAPTURE),
                      &devices, &result);
  EXPECT_EQ(MEDIA_DEVICE_OK, result);
  ASSERT_EQ(1u, devices.size());
  EXPECT_EQ(MEDIA_DEVICE_VIDEO_CAPTURE, devices[0].type);

  // Camera disabled.
  RefreshPageAndSetHeaderPolicy(main_rfh(),
                                blink::mojom::FeaturePolicyFeature::kCamera,
                                /*enabled=*/false);
  GetResultForRequest(CreateRequest(main_rfh(), MEDIA_DEVICE_AUDIO_CAPTURE,
                                    MEDIA_DEVICE_VIDEO_CAPTURE),
                      &devices, &result);
  EXPECT_EQ(MEDIA_DEVICE_OK, result);
  ASSERT_EQ(1u, devices.size());
  EXPECT_EQ(MEDIA_DEVICE_AUDIO_CAPTURE, devices[0].type);

  // Camera disabled resulting in no devices being returned.
  RefreshPageAndSetHeaderPolicy(main_rfh(),
                                blink::mojom::FeaturePolicyFeature::kCamera,
                                /*enabled=*/false);
  GetResultForRequest(
      CreateRequest(main_rfh(), MEDIA_NO_SERVICE, MEDIA_DEVICE_VIDEO_CAPTURE),
      &devices, &result);
  EXPECT_EQ(MEDIA_DEVICE_PERMISSION_DENIED, result);
  ASSERT_EQ(0u, devices.size());
}

}  // namespace content
