// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/google_services/manage_accounts/coordinator/manage_accounts_mediator.h"

#import "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/browser/settings/google_services/manage_accounts/coordinator/manage_accounts_mediator_delegate.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class ManageAccountsMediatorTest : public PlatformTest {
 protected:
  void SetUp() final {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    profile_ = std::move(builder).Build();
    delegate_mock_ =
        OCMStrictProtocolMock(@protocol(ManageAccountsMediatorDelegate));
    mediator_ = [[ManageAccountsMediator alloc]
        initWithAccountManagerService:ChromeAccountManagerServiceFactory::
                                          GetForProfile(profile_.get())
                          authService:AuthenticationServiceFactory::
                                          GetForProfile(profile_.get())
                      identityManager:IdentityManagerFactory::GetForProfile(
                                          profile_.get())];
    mediator_.delegate = delegate_mock_;
  }

  void TearDown() final {
    [mediator_ disconnect];
    EXPECT_OCMOCK_VERIFY((id)delegate_mock_);
  }

  PrefService* local_state() {
    return GetApplicationContext()->GetLocalState();
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  id<ManageAccountsMediatorDelegate> delegate_mock_;
  ManageAccountsMediator* mediator_;
};

// Tests that the mediator informs the delegate when sign-in becomes disabled.
TEST_F(ManageAccountsMediatorTest, TestSigninDisabled) {
  OCMExpect([delegate_mock_ manageAccountsMediatorWantsToBeStopped:mediator_]);
  local_state()->SetBoolean(prefs::kSigninAllowedOnDevice, false);
}
