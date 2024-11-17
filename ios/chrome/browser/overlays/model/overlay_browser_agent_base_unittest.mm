// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/overlay_browser_agent_base.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_callback_installer.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_presentation_context.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_request_callback_installer.h"
#import "ios/chrome/browser/overlays/model/test/overlay_test_macros.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

namespace {
// The modality to use in tests.
const OverlayModality kModality = OverlayModality::kWebContentArea;
// Request configs used in tests.
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(SupportedConfig);
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(UnsupportedConfig);
DEFINE_TEST_OVERLAY_RESPONSE_INFO(DispatchInfo);

// Fake version of OverlayBrowserAgentBase used for tests.
class FakeOverlayBrowserAgent
    : public OverlayBrowserAgentBase,
      public BrowserUserData<FakeOverlayBrowserAgent> {
 public:
  // The mock callback receiver used by the FakeOverlayRequestCallbackInstaller
  // whose callback installation is driven by the OverlayBrowserAgentBase
  // superclass.
  MockOverlayRequestCallbackReceiver& mock_callback_receiver() {
    return mock_callback_receiver_;
  }

 private:
  friend class BrowserUserData<FakeOverlayBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  FakeOverlayBrowserAgent(Browser* browser) : OverlayBrowserAgentBase(browser) {
    // Add a fake callback installer for kModality that supports requests
    // configured with SupportedConfig and dispatched responses with
    // DispatchInfo.
    std::unique_ptr<FakeOverlayRequestCallbackInstaller> installer =
        std::make_unique<FakeOverlayRequestCallbackInstaller>(
            &mock_callback_receiver_, std::set<const OverlayResponseSupport*>(
                                          {DispatchInfo::ResponseSupport()}));
    installer->set_request_support(SupportedConfig::RequestSupport());
    AddInstaller(std::move(installer), kModality);
  }

  testing::StrictMock<MockOverlayRequestCallbackReceiver>
      mock_callback_receiver_;
};
BROWSER_USER_DATA_KEY_IMPL(FakeOverlayBrowserAgent)
}  // namespace

// Test fixture for OverlayBrowserAgentBase.
class OverlayBrowserAgentBaseTest : public PlatformTest {
 public:
  OverlayBrowserAgentBaseTest() {
    // Create the Browser and set up the browser agent.
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    FakeOverlayBrowserAgent::CreateForBrowser(browser_.get());
    // Set up the infobar OverlayPresenter.
    OverlayPresenter::FromBrowser(browser_.get(), kModality)
        ->SetPresentationContext(&presentation_context_);
    // Add and active a WebState over which to present overlays.
    browser_->GetWebStateList()->InsertWebState(
        std::make_unique<web::FakeWebState>(),
        WebStateList::InsertionParams::Automatic().Activate());
    web_state_ = browser_->GetWebStateList()->GetActiveWebState();
  }

  ~OverlayBrowserAgentBaseTest() override {
    OverlayPresenter::FromBrowser(browser_.get(), kModality)
        ->SetPresentationContext(nullptr);
  }

  // Returns the mock callback receiver for the browser agent.
  MockOverlayRequestCallbackReceiver& mock_callback_receiver() {
    return FakeOverlayBrowserAgent::FromBrowser(browser_.get())
        ->mock_callback_receiver();
  }

  // Returns `web_state_`'s request queue.
  OverlayRequestQueue* queue() {
    return OverlayRequestQueue::FromWebState(web_state_, kModality);
  }

  // Cancels all requests in `web_state_`'s queue.
  void CancelRequests() { queue()->CancelAllRequests(); }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
  raw_ptr<web::WebState> web_state_ = nullptr;
  std::unique_ptr<Browser> browser_;
  FakeOverlayPresentationContext presentation_context_;
};

// Tests that callbacks are successfully set up for supported requests.
TEST_F(OverlayBrowserAgentBaseTest, SupportedRequestCallbackSetup) {
  // Add a supported request to the queue so that its presentation is simulated
  // in the fake presentation context, triggering the BrowserAgent to install
  // its callbacks on the request.
  std::unique_ptr<OverlayRequest> added_request =
      OverlayRequest::CreateWithConfig<SupportedConfig>();
  OverlayRequest* request = added_request.get();
  queue()->AddRequest(std::move(added_request));

  // Dispatch a response through this presented request, expecting the dispatch
  // callback to be executed on the mock receiver.
  EXPECT_CALL(mock_callback_receiver(),
              DispatchCallback(request, DispatchInfo::ResponseSupport()));
  request->GetCallbackManager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<DispatchInfo>());

  // Cancel the request, expecting the completion callback to be executed on the
  // mock receiver.
  EXPECT_CALL(mock_callback_receiver(), CompletionCallback(request));
  CancelRequests();
}

// Tests that callbacks are not executed for supported requests.
TEST_F(OverlayBrowserAgentBaseTest, UnsupportedRequestCallbackSetup) {
  // Add an unsupported request to the queue so that its presentation is
  // simulated in the fake presentation context.  Since the added request is
  // unsupported, no callbacks should have been installed.
  std::unique_ptr<OverlayRequest> added_request =
      OverlayRequest::CreateWithConfig<UnsupportedConfig>();
  OverlayRequest* request = added_request.get();
  queue()->AddRequest(std::move(added_request));

  // Dispatch a response through this presented request without expecting the
  // dispatch callback to be executed on the mock receiver.
  std::unique_ptr<OverlayResponse> response =
      OverlayResponse::CreateWithInfo<DispatchInfo>();
  request->GetCallbackManager()->DispatchResponse(std::move(response));

  // Cancel the request without expecting the completion callback to be executed
  // on the mock receiver.
  CancelRequests();
}
