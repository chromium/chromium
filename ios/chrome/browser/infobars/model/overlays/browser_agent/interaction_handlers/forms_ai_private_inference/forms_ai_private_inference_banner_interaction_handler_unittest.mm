// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/forms_ai_private_inference/forms_ai_private_inference_banner_interaction_handler.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/autofill/model/forms_ai_private_inference_infobar_delegate_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/infobars/model/overlays/default_infobar_overlay_request_factory.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/infobars/model/test/fake_infobar_ios.h"
#import "ios/chrome/browser/infobars/model/test/mock_infobar_delegate.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class FakeFormsAiPrivateInferenceInfoBarDelegateIOS
    : public FormsAiPrivateInferenceInfoBarDelegateIOS {
 public:
  FakeFormsAiPrivateInferenceInfoBarDelegateIOS()
      : FormsAiPrivateInferenceInfoBarDelegateIOS(nullptr) {}

  MOCK_METHOD(void, OnSettingsLinkClicked, (), (override));
  MOCK_METHOD(void, InfoBarDismissed, (), (override));
};

}  // namespace

// Test fixture for FormsAiPrivateInferenceBannerInteractionHandler.
class FormsAiPrivateInferenceBannerInteractionHandlerTest
    : public PlatformTest {
 public:
  FormsAiPrivateInferenceBannerInteractionHandlerTest() {
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    OverlayRequestQueue::CreateForWebState(&web_state_);
    InfobarOverlayRequestInserter::CreateForWebState(
        &web_state_, &DefaultInfobarOverlayRequestFactory);
    InfoBarManagerImpl::CreateForWebState(&web_state_);
  }

 protected:
  web::FakeWebState web_state_;
};

// Tests that ShowModalButtonTapped() calls showAutofillSettingsFromNotice on
// the mock settings command dispatcher and dismisses the banner.
TEST_F(FormsAiPrivateInferenceBannerInteractionHandlerTest,
       ShowModalButtonTapped) {
  id settings_commands_mock = OCMProtocolMock(@protocol(SettingsCommands));
  OCMExpect([settings_commands_mock showAutofillSettingsFromNotice]);

  CommandDispatcher* dispatcher = [[CommandDispatcher alloc] init];
  [dispatcher startDispatchingToTarget:settings_commands_mock
                           forProtocol:@protocol(SettingsCommands)];

  FormsAiPrivateInferenceBannerInteractionHandler handler(dispatcher);

  std::unique_ptr<FakeFormsAiPrivateInferenceInfoBarDelegateIOS> delegate =
      std::make_unique<FakeFormsAiPrivateInferenceInfoBarDelegateIOS>();
  FakeFormsAiPrivateInferenceInfoBarDelegateIOS* mock_delegate = delegate.get();
  InfoBarIOS infobar(InfobarType::kInfobarTypeFormsAiPrivateInference,
                     std::move(delegate));

  EXPECT_CALL(*mock_delegate, OnSettingsLinkClicked());
  EXPECT_CALL(*mock_delegate, InfoBarDismissed());
  handler.ShowModalButtonTapped(&infobar, &web_state_);

  EXPECT_OCMOCK_VERIFY(settings_commands_mock);
}

// Tests that MainButtonTapped() calls Accept() on the mock delegate.
TEST_F(FormsAiPrivateInferenceBannerInteractionHandlerTest, MainButtonTapped) {
  FormsAiPrivateInferenceBannerInteractionHandler handler(nil);

  std::unique_ptr<MockInfobarDelegate> delegate =
      std::make_unique<MockInfobarDelegate>();
  MockInfobarDelegate* mock_delegate = delegate.get();
  InfoBarIOS infobar(InfobarType::kInfobarTypeFormsAiPrivateInference,
                     std::move(delegate));

  EXPECT_CALL(*mock_delegate, Accept()).WillOnce(testing::Return(true));
  handler.MainButtonTapped(&infobar);
}
