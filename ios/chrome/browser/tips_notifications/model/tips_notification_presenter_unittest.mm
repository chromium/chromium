// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/model/tips_notification_presenter.h"

#import "ios/chrome/browser/authentication/ui_bundled/signin_presenter.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_commands.h"
#import "ios/chrome/browser/default_browser/model/promo_source.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/docking_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/tips_notifications/model/utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// A test fixture for TipsNotificationPresenter.
class TipsNotificationPresenterTest : public PlatformTest {
 protected:
  TipsNotificationPresenterTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    // Mock the ApplicationCommands protocol to allow the test to proceed.
    application_handler_ = MockHandler(@protocol(ApplicationCommands));
    OCMStub([application_handler_
        prepareToPresentModalWithSnackbarDismissal:NO
                                        completion:([OCMArg invokeBlockWithArgs:
                                                                nil])]);
  }

  id MockHandler(Protocol* protocol) {
    id mock_handler = OCMProtocolMock(protocol);
    [browser_->GetCommandDispatcher() startDispatchingToTarget:mock_handler
                                                   forProtocol:protocol];
    return mock_handler;
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  id application_handler_;
};

#pragma mark - Test Cases

// Tests that the presenter can show the Default Browser promo.
TEST_F(TipsNotificationPresenterTest, TestShowDefaultBrowserPromo) {
  id mock_handler = MockHandler(@protocol(SettingsCommands));
  OCMExpect([mock_handler
      showDefaultBrowserSettingsFromViewController:nil
                                      sourceForUMA:
                                          DefaultBrowserSettingsPageSource::
                                              kTipsNotification]);
  TipsNotificationPresenter::Present(browser_->AsWeakPtr(),
                                     TipsNotificationType::kDefaultBrowser);
  EXPECT_OCMOCK_VERIFY(mock_handler);
}

// Tests that the presenter can show the What's New page.
TEST_F(TipsNotificationPresenterTest, TestShowWhatsNew) {
  id mock_handler = MockHandler(@protocol(WhatsNewCommands));
  OCMExpect([mock_handler showWhatsNew]);
  TipsNotificationPresenter::Present(browser_->AsWeakPtr(),
                                     TipsNotificationType::kWhatsNew);
  EXPECT_OCMOCK_VERIFY(mock_handler);
}

// Tests that the presenter can show the Sign-in page.
TEST_F(TipsNotificationPresenterTest, TestShowSignin) {
  id mock_handler = MockHandler(@protocol(SigninPresenter));
  OCMExpect([mock_handler showSignin:[OCMArg any]]);
  TipsNotificationPresenter::Present(browser_->AsWeakPtr(),
                                     TipsNotificationType::kSignin);
  EXPECT_OCMOCK_VERIFY(mock_handler);
}

// Tests that the presenter can show the Set Up List "See More" menu.
TEST_F(TipsNotificationPresenterTest, TestShowSetUpListContinuation) {
  id mock_handler = MockHandler(@protocol(ContentSuggestionsCommands));
  OCMExpect([mock_handler showSetUpListSeeMoreMenuExpanded:YES]);
  TipsNotificationPresenter::Present(
      browser_->AsWeakPtr(), TipsNotificationType::kSetUpListContinuation);
  EXPECT_OCMOCK_VERIFY(mock_handler);
}

// Tests that the presenter can show the Docking promo.
TEST_F(TipsNotificationPresenterTest, TestShowDocking) {
  id mock_handler = MockHandler(@protocol(DockingPromoCommands));
  OCMExpect([mock_handler showDockingPromo:YES]);
  TipsNotificationPresenter::Present(browser_->AsWeakPtr(),
                                     TipsNotificationType::kDocking);
  EXPECT_OCMOCK_VERIFY(mock_handler);
}

// Tests that the presenter can show the Omnibox Position promo.
TEST_F(TipsNotificationPresenterTest, TestShowOmniboxPosition) {
  id mock_handler = MockHandler(@protocol(BrowserCoordinatorCommands));
  OCMExpect([mock_handler showOmniboxPositionChoice]);
  TipsNotificationPresenter::Present(browser_->AsWeakPtr(),
                                     TipsNotificationType::kOmniboxPosition);
  EXPECT_OCMOCK_VERIFY(mock_handler);
}

// Tests that the presenter can show the Lens promo.
TEST_F(TipsNotificationPresenterTest, TestShowLensPromo) {
  id mock_handler = MockHandler(@protocol(BrowserCoordinatorCommands));
  OCMExpect([mock_handler showLensPromo]);
  TipsNotificationPresenter::Present(browser_->AsWeakPtr(),
                                     TipsNotificationType::kLens);
  EXPECT_OCMOCK_VERIFY(mock_handler);
}

// Tests that the presenter can show the Enhanced Safe Browsing promo.
TEST_F(TipsNotificationPresenterTest, TestShowEnhancedSafeBrowsingPromo) {
  id mock_handler = MockHandler(@protocol(BrowserCoordinatorCommands));
  OCMExpect([mock_handler showEnhancedSafeBrowsingPromo]);
  TipsNotificationPresenter::Present(
      browser_->AsWeakPtr(), TipsNotificationType::kEnhancedSafeBrowsing);
  EXPECT_OCMOCK_VERIFY(mock_handler);
}

// Tests that the presenter can show the CPE promo.
TEST_F(TipsNotificationPresenterTest, TestShowCPEPromo) {
  id mock_handler = MockHandler(@protocol(CredentialProviderPromoCommands));
  OCMExpect(
      [mock_handler showCredentialProviderPromoWithTrigger:
                        CredentialProviderPromoTrigger::TipsNotification]);
  TipsNotificationPresenter::Present(browser_->AsWeakPtr(),
                                     TipsNotificationType::kCPE);
  EXPECT_OCMOCK_VERIFY(mock_handler);
}

// Tests that the presenter can show the Lens Overlay promo.
TEST_F(TipsNotificationPresenterTest, TestShowLensOverlayPromo) {
  id mock_handler = MockHandler(@protocol(BrowserCoordinatorCommands));
  OCMExpect([mock_handler showSearchWhatYouSeePromo]);
  TipsNotificationPresenter::Present(browser_->AsWeakPtr(),
                                     TipsNotificationType::kLensOverlay);
  EXPECT_OCMOCK_VERIFY(mock_handler);
}
