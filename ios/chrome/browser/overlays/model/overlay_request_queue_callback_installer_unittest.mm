// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/overlay_request_queue_callback_installer.h"

#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_request_callback_installer.h"
#import "ios/chrome/browser/overlays/model/test/overlay_test_macros.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

namespace {
// OverlayModality used in tests.
const OverlayModality kModality = OverlayModality::kWebContentArea;
// OverlayRequest ConfigType used for testing.
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(SupportedConfig);
// OverlayResponse InfoType used for testing.
DEFINE_TEST_OVERLAY_RESPONSE_INFO(DispatchInfo);
}  // namespace

// Test fixture for OverlayRequestQueueCallbackInstaller.
class OverlayRequestQueueCallbackInstallerTest : public PlatformTest {
 public:
  OverlayRequestQueueCallbackInstallerTest() {
    OverlayRequestQueue::CreateForWebState(&web_state_);
    queue_installer_ =
        OverlayRequestQueueCallbackInstaller::Create(&web_state_, kModality);
    std::unique_ptr<FakeOverlayRequestCallbackInstaller> request_installer =
        std::make_unique<FakeOverlayRequestCallbackInstaller>(
            &callback_receiver_, std::set<const OverlayResponseSupport*>(
                                     {DispatchInfo::ResponseSupport()}));
    request_installer->set_request_support(SupportedConfig::RequestSupport());
    queue_installer_->AddRequestCallbackInstaller(std::move(request_installer));
  }
  ~OverlayRequestQueueCallbackInstallerTest() override = default;

  OverlayRequestQueue* queue() {
    return OverlayRequestQueue::FromWebState(&web_state_, kModality);
  }

 protected:
  web::FakeWebState web_state_;
  testing::StrictMock<MockOverlayRequestCallbackReceiver> callback_receiver_;
  std::unique_ptr<OverlayRequestQueueCallbackInstaller> queue_installer_;
};

// Tests that callbacks are installed for supported requests added to the queue.
TEST_F(OverlayRequestQueueCallbackInstallerTest, InstallForSupportedRequest) {
  std::unique_ptr<OverlayRequest> added_request =
      OverlayRequest::CreateWithConfig<SupportedConfig>();
  OverlayRequest* request = added_request.get();
  queue()->AddRequest(std::move(added_request));

  // Dispatch a response through `request`, expecting the dispatch callback to
  // be executed on the mock receiver.
  EXPECT_CALL(callback_receiver_,
              DispatchCallback(request, DispatchInfo::ResponseSupport()));
  request->GetCallbackManager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<DispatchInfo>());

  // Cancel the requests, expecting the completion callback to be executed on
  // the mock receiver.
  EXPECT_CALL(callback_receiver_, CompletionCallback(request));
  queue()->CancelAllRequests();
}
