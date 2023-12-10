// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/post_restore_app_agent.h"

#import "base/test/scoped_feature_list.h"
#import "base/values.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/mock_promos_manager.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using ::testing::_;
using ::testing::AnyNumber;
using testing::NiceMock;

namespace {
const char kFakePreRestoreAccountEmail[] = "person@example.org";
}  // namespace

// Tests the PostRestoreAppAgent.
class PostRestoreAppAgentTest : public PlatformTest {
 public:
  explicit PostRestoreAppAgentTest() { CreateAppAgent(); }

  void CreateAppAgent() {
    app_agent_ =
        [[PostRestoreAppAgent alloc] initWithPromosManager:CreatePromosManager()
                                     authenticationService:CreateAuthService()
                                           identityManager:GetIdentityManager()
                                                localState:local_state_.Get()];
    mockAppState_ = OCMClassMock([AppState class]);
    [app_agent_ setAppState:mockAppState_];
  }

  MockPromosManager* CreatePromosManager() {
    promos_manager_ = std::make_unique<NiceMock<MockPromosManager>>();

    return promos_manager_.get();
  }

  AuthenticationService* CreateAuthService() {
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(AuthenticationServiceFactory::GetDefaultFactory()));
    browser_state_ = builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    auth_service_ =
        AuthenticationServiceFactory::GetForBrowserState(browser_state_.get());
    return auth_service_;
  }

  signin::IdentityManager* GetIdentityManager() {
    return IdentityManagerFactory::GetForBrowserState(browser_state_.get());
  }

  void MockAppStateChange(InitStage initStage) {
    OCMStub([mockAppState_ initStage]).andReturn(initStage);
    [app_agent_ appState:mockAppState_
        didTransitionFromInitStage:InitStageStart];
  }

  void SetFakePreRestoreAccountInfo() {
    AccountInfo accountInfo;
    accountInfo.email = kFakePreRestoreAccountEmail;
    StorePreRestoreIdentity(local_state_.Get(), accountInfo,
                            /*history_sync_enabled=*/false);
  }

  // Signs in a fake identity.
  void SignIn() {
    FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(fake_identity);
    auth_service_->SignIn(fake_identity,
                          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  }

 protected:
  IOSChromeScopedTestingLocalState local_state_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<MockPromosManager> promos_manager_;
  AuthenticationService* auth_service_;
  id mockAppState_;
  PostRestoreAppAgent* app_agent_;
};

// Tests the logic of whether a promo should be registered.
TEST_F(PostRestoreAppAgentTest, MaybeRegisterPromo) {
  EXPECT_CALL(*promos_manager_.get(), RegisterPromoForSingleDisplay(_))
      .Times(0);
  EXPECT_CALL(*promos_manager_.get(), RegisterPromoForContinuousDisplay(_))
      .Times(0);

  // Scenarios which should not register a promo.
  ClearPreRestoreIdentity(local_state_.Get());
  MockAppStateChange(InitStageFinal);

  SetFakePreRestoreAccountInfo();
  MockAppStateChange(InitStageFinal);

  ClearPreRestoreIdentity(local_state_.Get());
  MockAppStateChange(InitStageFinal);
}

// Tests that the alert promo is registered.
TEST_F(PostRestoreAppAgentTest, RegisterPromoAlert) {
  EXPECT_CALL(*promos_manager_.get(),
              RegisterPromoForSingleDisplay(
                  promos_manager::Promo::PostRestoreSignInAlert))
      .Times(1);

  SetFakePreRestoreAccountInfo();
  MockAppStateChange(InitStageFinal);
}

// Tests that the reauth prompt is disabled.
TEST_F(PostRestoreAppAgentTest, RegisterPromoDisablesReauthPrompt) {
  SetFakePreRestoreAccountInfo();
  auth_service_->SetReauthPromptForSignInAndSync();
  EXPECT_TRUE(auth_service_->ShouldReauthPromptForSignInAndSync());
  MockAppStateChange(InitStageFinal);
  EXPECT_FALSE(auth_service_->ShouldReauthPromptForSignInAndSync());
}

// Tests that the alert promo is deregistered with the right conditions.
TEST_F(PostRestoreAppAgentTest, DeregisterPromoAlert) {
  EXPECT_CALL(*promos_manager_.get(), DeregisterPromo(_)).Times(1);
  EXPECT_CALL(*promos_manager_.get(),
              DeregisterPromo(promos_manager::Promo::PostRestoreSignInAlert))
      .Times(1);

  SetFakePreRestoreAccountInfo();
  ClearPreRestoreIdentity(local_state_.Get());
  MockAppStateChange(InitStageFinal);
}

// Tests that if a signin occurs, the promo is deregistered.
TEST_F(PostRestoreAppAgentTest, DeregisterPromoOnSignin) {
  SetFakePreRestoreAccountInfo();
  MockAppStateChange(InitStageFinal);

  EXPECT_CALL(*promos_manager_.get(), DeregisterPromo(_)).Times(1);
  EXPECT_CALL(*promos_manager_.get(),
              DeregisterPromo(promos_manager::Promo::PostRestoreSignInAlert))
      .Times(1);

  SignIn();
}
