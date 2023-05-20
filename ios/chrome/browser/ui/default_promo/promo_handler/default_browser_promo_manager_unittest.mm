// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/promo_handler/default_browser_promo_manager.h"

#import "components/signin/public/base/signin_metrics.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/default_browser/utils_test_support.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class DefaultBrowserPromoManagerTest : public PlatformTest {
 public:
  DefaultBrowserPromoManagerTest() : PlatformTest() {}

 protected:
  void SetUp() override {
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    RegisterLocalStatePrefs(local_state_->registry());
    TestingApplicationContext::GetGlobal()->SetLocalState(local_state_.get());
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(AuthenticationServiceFactory::GetDefaultFactory()));
    browser_state_ = builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    view_controller_ = [[UIViewController alloc] init];
    default_browser_promo_manager_ = [[DefaultBrowserPromoManager alloc]
        initWithBaseViewController:view_controller_
                           browser:browser_.get()];
  }

  void TearDown() override {
    browser_state_.reset();
    TestingApplicationContext::GetGlobal()->SetLocalState(nullptr);
    TestingApplicationContext::GetGlobal()->SetLastShutdownClean(false);
    local_state_.reset();
    ClearDefaultBrowserPromoData();
    default_browser_promo_manager_ = nil;
  }

  void SignIn() {
    FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);
    AuthenticationServiceFactory::GetForBrowserState(browser_state_.get())
        ->SignIn(identity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  }

  std::unique_ptr<TestChromeBrowserState> browser_state_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<Browser> browser_;
  UIViewController* view_controller_;
  DefaultBrowserPromoManager* default_browser_promo_manager_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
};

// Tests that the DefaultPromoTypeMadeForIOS tailored promo is shown when it was
// detected that the user is likely interested in the promo.
TEST_F(DefaultBrowserPromoManagerTest, ShowTailoredPromoMadeForIOS) {
  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(true);
  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id mockDefaultPromoCommandsHandler =
      OCMProtocolMock(@protocol(DefaultPromoCommands));
  [dispatcher startDispatchingToTarget:mockDefaultPromoCommandsHandler
                           forProtocol:@protocol(DefaultPromoCommands)];
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
  OCMExpect([mockDefaultPromoCommandsHandler showTailoredPromoMadeForIOS]);
  [default_browser_promo_manager_ start];
  EXPECT_OCMOCK_VERIFY(mockDefaultPromoCommandsHandler);
}

// Tests that the DefaultPromoTypeStaySafe tailored promo is shown when it was
// detected that the user is likely interested in the promo.
TEST_F(DefaultBrowserPromoManagerTest, ShowTailoredPromoStaySafe) {
  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(true);
  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id mockDefaultPromoCommandsHandler =
      OCMProtocolMock(@protocol(DefaultPromoCommands));
  [dispatcher startDispatchingToTarget:mockDefaultPromoCommandsHandler
                           forProtocol:@protocol(DefaultPromoCommands)];
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);
  OCMExpect([mockDefaultPromoCommandsHandler showTailoredPromoStaySafe]);
  [default_browser_promo_manager_ start];
  EXPECT_OCMOCK_VERIFY(mockDefaultPromoCommandsHandler);
}

// Tests that the DefaultPromoTypeAllTabs tailored promo is shown when it was
// detected that the user is likely interested in the promo.
TEST_F(DefaultBrowserPromoManagerTest, ShowTailoredPromoAllTabs) {
  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(true);
  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id mockDefaultPromoCommandsHandler =
      OCMProtocolMock(@protocol(DefaultPromoCommands));
  [dispatcher startDispatchingToTarget:mockDefaultPromoCommandsHandler
                           forProtocol:@protocol(DefaultPromoCommands)];
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
  SignIn();
  OCMExpect([mockDefaultPromoCommandsHandler showTailoredPromoAllTabs]);
  [default_browser_promo_manager_ start];
  EXPECT_OCMOCK_VERIFY(mockDefaultPromoCommandsHandler);
}

// Tests that the DefaultPromoTypeGeneral promo is shown when it was detected
// that the user is likely interested in the promo.
TEST_F(DefaultBrowserPromoManagerTest, showDefaultBrowserFullscreenPromo) {
  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(true);
  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id mockDefaultPromoCommandsHandler =
      OCMProtocolMock(@protocol(DefaultPromoCommands));
  [dispatcher startDispatchingToTarget:mockDefaultPromoCommandsHandler
                           forProtocol:@protocol(DefaultPromoCommands)];
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
  SignIn();
  OCMExpect(
      [mockDefaultPromoCommandsHandler showDefaultBrowserFullscreenPromo]);
  [default_browser_promo_manager_ start];
  EXPECT_OCMOCK_VERIFY(mockDefaultPromoCommandsHandler);
}

}  // namespace
