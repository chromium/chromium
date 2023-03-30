// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/password_infobar_banner_interaction_handler.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/credential_provider_promo/features.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/overlays/default_infobar_overlay_request_factory.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/password_infobar_banner_overlay.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/passwords/test/mock_ios_chrome_save_passwords_infobar_delegate.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for PasswordInfobarBannerInteractionHandler.
class PasswordInfobarBannerInteractionHandlerTest : public PlatformTest {
 public:
  PasswordInfobarBannerInteractionHandlerTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()),
        browser_(std::make_unique<TestBrowser>(browser_state_.get())),
        handler_(browser_.get(),
                 password_modal::PasswordAction::kSave,
                 PasswordInfobarBannerOverlayRequestConfig::RequestSupport()) {
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    InfobarOverlayRequestInserter::CreateForWebState(
        &web_state_, &DefaultInfobarOverlayRequestFactory);
    InfoBarManagerImpl::CreateForWebState(&web_state_);
    std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypePasswordSave,
        MockIOSChromeSavePasswordInfoBarDelegate::Create(@"username",
                                                         @"password"));
    infobar_ = infobar.get();
    InfoBarManagerImpl::FromWebState(&web_state_)
        ->AddInfoBar(std::move(infobar));

    mock_credential_provider_promo_commands_handler_ =
        OCMStrictProtocolMock(@protocol(CredentialProviderPromoCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:
            mock_credential_provider_promo_commands_handler_
                     forProtocol:@protocol(CredentialProviderPromoCommands)];
  }

  MockIOSChromeSavePasswordInfoBarDelegate& mock_delegate() {
    return *static_cast<MockIOSChromeSavePasswordInfoBarDelegate*>(
        infobar_->delegate());
  }

 protected:
  base::test::TaskEnvironment task_environment;
  std::unique_ptr<ChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;

  PasswordInfobarBannerInteractionHandler handler_;
  web::FakeWebState web_state_;
  InfoBarIOS* infobar_;
  id mock_credential_provider_promo_commands_handler_;
};

// Tests that BannerVisibilityChanged() calls InfobarPresenting() and
// InfobarDismissed() on the mock delegate.
TEST_F(PasswordInfobarBannerInteractionHandlerTest, Presentation) {
  EXPECT_CALL(mock_delegate(), InfobarPresenting(true));
  handler_.BannerVisibilityChanged(infobar_, true);
  EXPECT_CALL(mock_delegate(), InfobarDismissed());
  handler_.BannerVisibilityChanged(infobar_, false);
}

// Tests MainButtonTapped() calls Accept() on the mock delegate and resets
// the infobar to be accepted.
TEST_F(PasswordInfobarBannerInteractionHandlerTest, MainButton) {
  ASSERT_FALSE(infobar_->accepted());
  EXPECT_CALL(mock_delegate(), Accept()).WillOnce(testing::Return(true));
  handler_.MainButtonTapped(infobar_);
  EXPECT_TRUE(infobar_->accepted());
}

// Tests that tapping the main button sends CredentialProviderPromo command.
TEST_F(PasswordInfobarBannerInteractionHandlerTest,
       MainButtonTriggersCredentialProviderPromo) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kCredentialProviderExtensionPromo,
      {{"enable_promo_on_password_saved", "true"}});

  [[mock_credential_provider_promo_commands_handler_ expect]
      showCredentialProviderPromoWithTrigger:CredentialProviderPromoTrigger::
                                                 PasswordSaved];

  handler_.MainButtonTapped(infobar_);

  [mock_credential_provider_promo_commands_handler_ verify];
}
