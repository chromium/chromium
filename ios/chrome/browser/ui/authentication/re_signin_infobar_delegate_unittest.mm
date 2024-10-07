// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/re_signin_infobar_delegate.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/memory/ptr_util.h"
#import "base/types/cxx23_to_underlying.h"
#import "components/infobars/core/infobar.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/infobar_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class ReSignInInfoBarDelegateTest : public PlatformTest {
 public:
  ReSignInInfoBarDelegateTest() {
    OCMStub([mock_app_state_ initStage]).andDo(^(NSInvocation* invocation) {
      [invocation setReturnValue:&init_stage_];
    });
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    fake_web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    browser_->GetWebStateList()->InsertWebState(
        std::move(fake_web_state),
        WebStateList::InsertionParams::Automatic().Activate());
    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
    InfoBarManagerImpl::CreateForWebState(web_state());
  }

  ~ReSignInInfoBarDelegateTest() override {
    EXPECT_OCMOCK_VERIFY((id)signin_presenter_);
    EXPECT_OCMOCK_VERIFY((id)mock_app_state_);
  }

  void SetUpMainProfileIOSWithSignedInUser() {
    id<SystemIdentity> chrome_identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(chrome_identity);
    authentication_service()->SignIn(
        chrome_identity,
        signin_metrics::AccessPoint::ACCESS_POINT_RESIGNIN_INFOBAR);
  }

  AuthenticationService* authentication_service() {
    // AuthenticationService currently has no good fake, so constructing the
    // production one via TestProfileIOS is the best we can do.
    return AuthenticationServiceFactory::GetForProfile(profile_.get());
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(profile_.get());
  }

  OCMockObject<SigninPresenter>* signin_presenter() {
    return signin_presenter_;
  }

  web::WebState* web_state() {
    return browser_->GetWebStateList()->GetActiveWebState();
  }

  AppState* mock_app_state() { return mock_app_state_; }

  void set_init_stage(AppInitStage init_stage) { init_stage_ = init_stage; }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<web::NavigationManager> test_navigation_manager_;
  OCMockObject<SigninPresenter>* signin_presenter_ =
      OCMProtocolMock(@protocol(SigninPresenter));
  // SceneState only weakly holds AppState, so keep it alive here.
  AppState* mock_app_state_ = OCMStrictClassMock([AppState class]);
  AppInitStage init_stage_ = AppInitStage::kFinal;
};

TEST_F(ReSignInInfoBarDelegateTest, TestCreateWhenNotPrompting) {
  // User is not signed in, but the "prompt" flag is not set.
  authentication_service()->ResetReauthPromptForSignInAndSync();
  std::unique_ptr<ReSignInInfoBarDelegate> infobar_delegate =
      ReSignInInfoBarDelegate::Create(authentication_service(),
                                      identity_manager(), mock_app_state(),
                                      signin_presenter());
  // Infobar delegate should not be created.
  EXPECT_FALSE(infobar_delegate);
  EXPECT_FALSE(authentication_service()->ShouldReauthPromptForSignInAndSync());
}

TEST_F(ReSignInInfoBarDelegateTest, TestCreateWhenNotSignedIn) {
  // User is not signed in, but the "prompt" flag is set.
  authentication_service()->SetReauthPromptForSignInAndSync();
  std::unique_ptr<ReSignInInfoBarDelegate> infobar_delegate =
      ReSignInInfoBarDelegate::Create(authentication_service(),
                                      identity_manager(), mock_app_state(),
                                      signin_presenter());
  // Infobar delegate should be created.
  EXPECT_TRUE(infobar_delegate);
  EXPECT_TRUE(authentication_service()->ShouldReauthPromptForSignInAndSync());
}

TEST_F(ReSignInInfoBarDelegateTest, TestCreateWhenAlreadySignedIn) {
  // User is signed in and the "prompt" flag is set.
  SetUpMainProfileIOSWithSignedInUser();
  authentication_service()->SetReauthPromptForSignInAndSync();
  std::unique_ptr<ReSignInInfoBarDelegate> infobar_delegate =
      ReSignInInfoBarDelegate::Create(authentication_service(),
                                      identity_manager(), mock_app_state(),
                                      signin_presenter());
  // Infobar delegate should not be created.
  EXPECT_FALSE(infobar_delegate);
  EXPECT_FALSE(authentication_service()->ShouldReauthPromptForSignInAndSync());
}

TEST_F(ReSignInInfoBarDelegateTest, TestCreateWhenIncognito) {
  // Tab is incognito, and the "prompt" flag is set.
  authentication_service()->SetReauthPromptForSignInAndSync();
  std::unique_ptr<ReSignInInfoBarDelegate> infobar_delegate =
      ReSignInInfoBarDelegate::Create(/*authentication_service=*/nullptr,
                                      /*identity_manager=*/nullptr,
                                      mock_app_state(), signin_presenter());
  // Infobar delegate should not be created.
  EXPECT_FALSE(infobar_delegate);
  EXPECT_TRUE(authentication_service()->ShouldReauthPromptForSignInAndSync());
}

TEST_F(ReSignInInfoBarDelegateTest, TestMessages) {
  authentication_service()->SetReauthPromptForSignInAndSync();
  std::unique_ptr<ReSignInInfoBarDelegate> delegate =
      ReSignInInfoBarDelegate::Create(authentication_service(),
                                      identity_manager(), mock_app_state(),
                                      signin_presenter());
  EXPECT_EQ(ConfirmInfoBarDelegate::BUTTON_OK, delegate->GetButtons());
  std::u16string message_text = delegate->GetMessageText();
  EXPECT_GT(message_text.length(), 0U);
  std::u16string button_label =
      delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK);
  EXPECT_GT(button_label.length(), 0U);
}

TEST_F(ReSignInInfoBarDelegateTest, TestAccept) {
  authentication_service()->SetReauthPromptForSignInAndSync();

  [[signin_presenter() expect]
      showSignin:[OCMArg checkWithBlock:^BOOL(id command) {
        EXPECT_TRUE([command isKindOfClass:[ShowSigninCommand class]]);
        EXPECT_EQ(AuthenticationOperation::kResignin,
                  static_cast<ShowSigninCommand*>(command).operation);
        return YES;
      }]];

  std::unique_ptr<ReSignInInfoBarDelegate> delegate =
      ReSignInInfoBarDelegate::Create(authentication_service(),
                                      identity_manager(), mock_app_state(),
                                      signin_presenter());
  EXPECT_TRUE(delegate->Accept());
  EXPECT_FALSE(authentication_service()->ShouldReauthPromptForSignInAndSync());
}

TEST_F(ReSignInInfoBarDelegateTest, TestInfoBarDismissed) {
  authentication_service()->SetReauthPromptForSignInAndSync();

  [[signin_presenter() reject] showSignin:[OCMArg any]];

  std::unique_ptr<ReSignInInfoBarDelegate> delegate =
      ReSignInInfoBarDelegate::Create(authentication_service(),
                                      identity_manager(), mock_app_state(),
                                      signin_presenter());
  delegate->InfoBarDismissed();
  EXPECT_FALSE(authentication_service()->ShouldReauthPromptForSignInAndSync());
}

// Tests that no delegate is returned when the app state is not
// AppInitStage::kFinal.
TEST_F(ReSignInInfoBarDelegateTest, TestAppStateBeforeStageFinal) {
  set_init_stage(
      static_cast<AppInitStage>(base::to_underlying(AppInitStage::kFinal) - 1));
  std::unique_ptr<ReSignInInfoBarDelegate> infobar_delegate =
      ReSignInInfoBarDelegate::Create(authentication_service(),
                                      identity_manager(), mock_app_state(),
                                      signin_presenter());
  // Infobar delegate should not be created.
  EXPECT_FALSE(infobar_delegate);
}

// Tests that the infobar is removed as soon as the user signs in.
TEST_F(ReSignInInfoBarDelegateTest, TestInfoBarDismissedBySignin) {
  authentication_service()->SetReauthPromptForSignInAndSync();

  [[signin_presenter() reject] showSignin:[OCMArg any]];

  std::unique_ptr<ReSignInInfoBarDelegate> delegate =
      ReSignInInfoBarDelegate::Create(authentication_service(),
                                      identity_manager(), mock_app_state(),
                                      signin_presenter());
  std::unique_ptr<InfoBarIOS> info_bar_ios = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypeConfirm, std::move(delegate));
  InfoBarManagerImpl::FromWebState(web_state())
      ->AddInfoBar(std::move(info_bar_ios));
  // Test that the info bar was added.
  EXPECT_EQ(InfoBarManagerImpl::FromWebState(web_state())->infobars().size(),
            1u);
  // Sign-in from NTP.
  id<SystemIdentity> chrome_identity = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->AddIdentity(chrome_identity);
  authentication_service()->SignIn(
      chrome_identity,
      signin_metrics::AccessPoint::ACCESS_POINT_NTP_SIGNED_OUT_ICON);
  // Test that the info bar has been removed.
  EXPECT_EQ(InfoBarManagerImpl::FromWebState(web_state())->infobars().size(),
            0u);
}

}  // namespace
