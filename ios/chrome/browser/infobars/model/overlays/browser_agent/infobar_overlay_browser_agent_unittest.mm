// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/infobar_overlay_browser_agent.h"

#import <map>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/test/mock_infobar_interaction_handler.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_type.h"
#import "ios/chrome/browser/infobars/model/test/fake_infobar_ios.h"
#import "ios/chrome/browser/overlays/model/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_presentation_context.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_request_callback_installer.h"
#import "ios/chrome/browser/overlays/model/test/overlay_test_macros.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

using testing::ByMove;
using testing::Return;

namespace {
// Fake dispatch response InfoType for use in tests.
DEFINE_TEST_OVERLAY_RESPONSE_INFO(DispatchInfo);

// Fake OverlayRequestSupport that supports requests configured with
// InfobarOverlayRequestConfigs with a specified InfobarOverlayType.
class FakeInfobarOverlayRequestSupport : public OverlayRequestSupport {
 public:
  FakeInfobarOverlayRequestSupport(InfobarOverlayType overlay_type)
      : overlay_type_(overlay_type) {}
  FakeInfobarOverlayRequestSupport(FakeInfobarOverlayRequestSupport&& other)
      : overlay_type_(other.overlay_type_) {}

  bool IsRequestSupported(OverlayRequest* request) const override {
    InfobarOverlayRequestConfig* config =
        request->GetConfig<InfobarOverlayRequestConfig>();
    return config && config->overlay_type() == overlay_type_;
  }

 private:
  InfobarOverlayType overlay_type_;
};
}  // namespace

// Test fixture for InfobarOverlayBrowserAgent.
class InfobarOverlayBrowserAgentTest
    : public testing::TestWithParam<InfobarOverlayType> {
 public:
  InfobarOverlayBrowserAgentTest()
      : profile_(TestProfileIOS::Builder().Build()),
        browser_(std::make_unique<TestBrowser>(profile_.get())),
        interaction_handler_builder_(InfobarType::kInfobarTypeConfirm) {
    // Add an activated WebState into whose queues infobar OverlayRequests will
    // be added.
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state_ = web_state.get();
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
    // Set up the OverlayPresenter's presentation context so that presentation
    // can be faked.
    presenter()->SetPresentationContext(&presentation_context_);
    // Create the request supports for each InfobarOverlayType.
    request_supports_.emplace(
        InfobarOverlayType::kBanner,
        FakeInfobarOverlayRequestSupport(InfobarOverlayType::kBanner));
    request_supports_.emplace(
        InfobarOverlayType::kModal,
        FakeInfobarOverlayRequestSupport(InfobarOverlayType::kModal));
    // Create the interaction handler and set up the mock handlers to return
    // fake callback installers.
    std::unique_ptr<InfobarInteractionHandler> interaction_handler =
        std::move(interaction_handler_builder_).Build();
    EXPECT_CALL(*mock_handler(InfobarOverlayType::kBanner), CreateInstaller())
        .WillOnce(Return(ByMove(CreateInstaller(InfobarOverlayType::kBanner))));
    EXPECT_CALL(*mock_handler(InfobarOverlayType::kModal), CreateInstaller())
        .WillOnce(Return(ByMove(CreateInstaller(InfobarOverlayType::kModal))));
    // Set up the browser agent and mock interaction handler.
    InfobarOverlayBrowserAgent::CreateForBrowser(browser_.get());
    InfobarOverlayBrowserAgent::FromBrowser(browser_.get())
        ->AddInfobarInteractionHandler(std::move(interaction_handler));
  }
  ~InfobarOverlayBrowserAgentTest() override {
    presenter()->SetPresentationContext(nullptr);
  }

  // Creates the OverlayRequestCallbackInstaller to return from
  // CreateInstaller() for the mock interaction handler for `overlay_type`.
  // Returned installers forwards callbacks to the receivers in
  // `mock_callback_receivers_`.
  std::unique_ptr<FakeOverlayRequestCallbackInstaller> CreateInstaller(
      InfobarOverlayType overlay_type) {
    std::unique_ptr<FakeOverlayRequestCallbackInstaller> installer =
        std::make_unique<FakeOverlayRequestCallbackInstaller>(
            &mock_callback_receivers_[overlay_type],
            std::set<const OverlayResponseSupport*>(
                {DispatchInfo::ResponseSupport()}));
    installer->set_request_support(&request_supports_.at(overlay_type));
    return installer;
  }

  // Creates an OverlayRequest configured with an InfobarOverlayRequestConfig
  // that has the same InfobarOverlayType as the test fixture.
  std::unique_ptr<OverlayRequest> CreateRequest() {
    return OverlayRequest::CreateWithConfig<InfobarOverlayRequestConfig>(
        &infobar_, GetParam(), false);
  }

  OverlayModality modality() {
    return GetParam() == InfobarOverlayType::kBanner
               ? OverlayModality::kInfobarBanner
               : OverlayModality::kInfobarModal;
  }
  OverlayPresenter* presenter() {
    return OverlayPresenter::FromBrowser(browser_.get(), modality());
  }
  OverlayRequestQueue* queue() {
    return OverlayRequestQueue::FromWebState(web_state_, modality());
  }
  MockInfobarInteractionHandler::Handler* mock_handler(
      InfobarOverlayType overlay_type) {
    return interaction_handler_builder_.mock_handler(overlay_type);
  }
  MockInfobarInteractionHandler::Handler* mock_handler() {
    return mock_handler(GetParam());
  }
  MockOverlayRequestCallbackReceiver* mock_callback_receiver() {
    return &mock_callback_receivers_[GetParam()];
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<web::WebState> web_state_ = nullptr;
  std::map<InfobarOverlayType, FakeInfobarOverlayRequestSupport>
      request_supports_;
  std::map<InfobarOverlayType, MockOverlayRequestCallbackReceiver>
      mock_callback_receivers_;
  MockInfobarInteractionHandler::Builder interaction_handler_builder_;
  FakeOverlayPresentationContext presentation_context_;
  FakeInfobarIOS infobar_;
};

// Tests the overlay presentation flow for a given InfobarOverlayType.
TEST_P(InfobarOverlayBrowserAgentTest, OverlayPresentation) {
  // Add a supported infobar request to the queue, expecting
  // MockInfobarBannerInteractionHandler::Handler::InfobarVisibilityChanged() to
  // be called.
  std::unique_ptr<OverlayRequest> added_request = CreateRequest();
  OverlayRequest* request = added_request.get();
  EXPECT_CALL(*mock_handler(),
              InfobarVisibilityChanged(&infobar_, /*visible=*/true));
  queue()->AddRequest(std::move(added_request));
  // Verify that dispatched responses sent through `request`'s callback manager
  // are received by the expected receiver.
  EXPECT_CALL(*mock_callback_receiver(),
              DispatchCallback(request, DispatchInfo::ResponseSupport()));
  request->GetCallbackManager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<DispatchInfo>());
  // Simulate dismissal of the request's UI, expecting
  // MockInfobarBannerInteractionHandler::Handler::InfobarVisibilityChanged()
  // and MockOverlayRequestCallbackReceiver::CompletionCallback() to
  // be called.
  EXPECT_CALL(*mock_handler(),
              InfobarVisibilityChanged(&infobar_, /*visible=*/false));
  EXPECT_CALL(*mock_callback_receiver(), CompletionCallback(request));
  presentation_context_.SimulateDismissalForRequest(
      request, OverlayDismissalReason::kUserInteraction);
}

INSTANTIATE_TEST_SUITE_P(/* No InstantiationName */,
                         InfobarOverlayBrowserAgentTest,
                         testing::Values(InfobarOverlayType::kBanner,
                                         InfobarOverlayType::kModal));
