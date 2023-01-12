// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/re_signin_infobar_delegate.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/memory/ptr_util.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_utils.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class ReSignInInfoBarDelegateTest : public PlatformTest {
 public:
  ReSignInInfoBarDelegateTest() {}

 protected:
  void SetUp() override {
  }

  void SetUpMainChromeBrowserStateNotSignedIn() {
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        chrome_browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
  }

  void SetUpMainChromeBrowserStateWithSignedInUser() {
    SetUpMainChromeBrowserStateNotSignedIn();

    id<SystemIdentity> chrome_identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(chrome_identity);
    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    authentication_service->SignIn(chrome_identity);
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

TEST_F(ReSignInInfoBarDelegateTest, TestCreateWhenNotPrompting) {
  // User is not signed in, but the "prompt" flag is not set.
  SetUpMainChromeBrowserStateNotSignedIn();
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  authentication_service->ResetReauthPromptForSignInAndSync();
  std::unique_ptr<ReSignInInfoBarDelegate> infobar_delegate =
      ReSignInInfoBarDelegate::CreateInfoBarDelegate(
          chrome_browser_state_.get(), nil);
  // Infobar delegate should not be created.
  EXPECT_FALSE(infobar_delegate.get());
  EXPECT_FALSE(authentication_service->ShouldReauthPromptForSignInAndSync());
}

TEST_F(ReSignInInfoBarDelegateTest, TestCreateWhenNotSignedIn) {
  // User is not signed in, but the "prompt" flag is set.
  SetUpMainChromeBrowserStateNotSignedIn();
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  authentication_service->SetReauthPromptForSignInAndSync();
  std::unique_ptr<ReSignInInfoBarDelegate> infobar_delegate =
      ReSignInInfoBarDelegate::CreateInfoBarDelegate(
          chrome_browser_state_.get(), nil);
  // Infobar delegate should be created.
  EXPECT_TRUE(infobar_delegate.get());
  EXPECT_TRUE(authentication_service->ShouldReauthPromptForSignInAndSync());
}

TEST_F(ReSignInInfoBarDelegateTest, TestCreateWhenAlreadySignedIn) {
  // User is signed in and the "prompt" flag is set.
  SetUpMainChromeBrowserStateWithSignedInUser();
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  authentication_service->SetReauthPromptForSignInAndSync();
  std::unique_ptr<ReSignInInfoBarDelegate> infobar_delegate =
      ReSignInInfoBarDelegate::CreateInfoBarDelegate(
          chrome_browser_state_.get(), nil);
  // Infobar delegate should not be created.
  EXPECT_FALSE(infobar_delegate.get());
  EXPECT_FALSE(authentication_service->ShouldReauthPromptForSignInAndSync());
}

TEST_F(ReSignInInfoBarDelegateTest, TestCreateWhenIncognito) {
  // Tab is incognito, and the "prompt" flag is set.
  SetUpMainChromeBrowserStateNotSignedIn();
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  authentication_service->SetReauthPromptForSignInAndSync();
  std::unique_ptr<ReSignInInfoBarDelegate> infobar_delegate =
      ReSignInInfoBarDelegate::CreateInfoBarDelegate(
          chrome_browser_state_->GetOffTheRecordChromeBrowserState(), nil);
  // Infobar delegate should not be created.
  EXPECT_FALSE(infobar_delegate.get());
  EXPECT_TRUE(authentication_service->ShouldReauthPromptForSignInAndSync());
}

TEST_F(ReSignInInfoBarDelegateTest, TestMessages) {
  SetUpMainChromeBrowserStateNotSignedIn();
  std::unique_ptr<ReSignInInfoBarDelegate> delegate(
      new ReSignInInfoBarDelegate(chrome_browser_state_.get(), nil));
  EXPECT_EQ(ConfirmInfoBarDelegate::BUTTON_OK, delegate->GetButtons());
  std::u16string message_text = delegate->GetMessageText();
  EXPECT_GT(message_text.length(), 0U);
  std::u16string button_label =
      delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK);
  EXPECT_GT(button_label.length(), 0U);
}

TEST_F(ReSignInInfoBarDelegateTest, TestAccept) {
  SetUpMainChromeBrowserStateNotSignedIn();
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  authentication_service->SetReauthPromptForSignInAndSync();

  id presenter = OCMProtocolMock(@protocol(SigninPresenter));
  [[presenter expect]
      showSignin:[OCMArg checkWithBlock:^BOOL(id command) {
        EXPECT_TRUE([command isKindOfClass:[ShowSigninCommand class]]);
        EXPECT_EQ(AuthenticationOperationReauthenticate,
                  static_cast<ShowSigninCommand*>(command).operation);
        return YES;
      }]];

  std::unique_ptr<infobars::InfoBar> infobar(
      CreateConfirmInfoBar(ReSignInInfoBarDelegate::CreateInfoBarDelegate(
          chrome_browser_state_.get(), presenter)));
  InfoBarIOS* infobarIOS = static_cast<InfoBarIOS*>(infobar.get());

  ReSignInInfoBarDelegate* delegate =
      static_cast<ReSignInInfoBarDelegate*>(infobarIOS->delegate());
  EXPECT_TRUE(delegate->Accept());
  EXPECT_FALSE(authentication_service->ShouldReauthPromptForSignInAndSync());
}

TEST_F(ReSignInInfoBarDelegateTest, TestInfoBarDismissed) {
  SetUpMainChromeBrowserStateNotSignedIn();
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  authentication_service->SetReauthPromptForSignInAndSync();

  id presenter = OCMProtocolMock(@protocol(SigninPresenter));
  [[presenter reject] showSignin:[OCMArg any]];

  std::unique_ptr<infobars::InfoBar> infobar(
      CreateConfirmInfoBar(ReSignInInfoBarDelegate::CreateInfoBarDelegate(
          chrome_browser_state_.get(), presenter)));
  InfoBarIOS* infobarIOS = static_cast<InfoBarIOS*>(infobar.get());

  ReSignInInfoBarDelegate* delegate =
      static_cast<ReSignInInfoBarDelegate*>(infobarIOS->delegate());
  delegate->InfoBarDismissed();
  EXPECT_FALSE(authentication_service->ShouldReauthPromptForSignInAndSync());
}

}  // namespace
