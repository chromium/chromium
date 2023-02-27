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
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/mock_promos_manager.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/signin_util.h"
#import "ios/chrome/browser/ui/post_restore_signin/features.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/abseil-cpp/absl/types/optional.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ::testing::_;
using ::testing::AnyNumber;

namespace {
const char kFakePreRestoreAccountEmail[] = "person@example.org";
}  // namespace

// Tests the PostRestoreAppAgent.
class PostRestoreAppAgentTest : public PlatformTest {
 public:
  explicit PostRestoreAppAgentTest() { CreateAppAgent(); }

  IOSChromeScopedTestingLocalState local_state_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  PostRestoreAppAgent* appAgent_;
  std::unique_ptr<MockPromosManager> promos_manager_;
  AuthenticationService* auth_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
  id mockAppState_;

  void CreateAppAgent() {
    appAgent_ =
        [[PostRestoreAppAgent alloc] initWithPromosManager:CreatePromosManager()
                                     authenticationService:CreateAuthService()
                                                localState:local_state_.Get()];
    mockAppState_ = OCMClassMock([AppState class]);
    [appAgent_ setAppState:mockAppState_];
  }

  MockPromosManager* CreatePromosManager() {
    promos_manager_ = std::make_unique<MockPromosManager>();

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

  void MockAppStateChange(InitStage initStage) {
    OCMStub([mockAppState_ initStage]).andReturn(initStage);
    [appAgent_ appState:mockAppState_
        didTransitionFromInitStage:InitStageStart];
  }

  void SetFakePreRestoreAccountInfo() {
    AccountInfo accountInfo;
    accountInfo.email = kFakePreRestoreAccountEmail;
    StorePreRestoreIdentity(local_state_.Get(), accountInfo);
  }

  void EnableFeatureVariationAlert() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {base::test::FeatureRefAndParams(
            post_restore_signin::features::kIOSNewPostRestoreExperience,
            {{post_restore_signin::features::kIOSNewPostRestoreExperienceParam,
              "true"}})},
        {});
  }
};

TEST_F(PostRestoreAppAgentTest, maybeRegisterPromo) {
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
  EnableFeatureVariationAlert();
  MockAppStateChange(InitStageFinal);
}

TEST_F(PostRestoreAppAgentTest, registerPromoAlert) {
  EXPECT_CALL(*promos_manager_.get(),
              RegisterPromoForSingleDisplay(
                  promos_manager::Promo::PostRestoreSignInAlert))
      .Times(1);

  EnableFeatureVariationAlert();
  SetFakePreRestoreAccountInfo();
  MockAppStateChange(InitStageFinal);
}

TEST_F(PostRestoreAppAgentTest, registerPromoDisablesReauthPrompt) {
  EnableFeatureVariationAlert();
  SetFakePreRestoreAccountInfo();
  auth_service_->SetReauthPromptForSignInAndSync();
  EXPECT_TRUE(auth_service_->ShouldReauthPromptForSignInAndSync());
  MockAppStateChange(InitStageFinal);
  EXPECT_FALSE(auth_service_->ShouldReauthPromptForSignInAndSync());
}

TEST_F(PostRestoreAppAgentTest, deregisterPromoAlert) {
  EXPECT_CALL(*promos_manager_.get(), DeregisterPromo(_)).Times(1);
  EXPECT_CALL(*promos_manager_.get(),
              DeregisterPromo(promos_manager::Promo::PostRestoreSignInAlert))
      .Times(1);

  EnableFeatureVariationAlert();
  SetFakePreRestoreAccountInfo();
  ClearPreRestoreIdentity(local_state_.Get());
  MockAppStateChange(InitStageFinal);
}

TEST_F(PostRestoreAppAgentTest, featureVariationSwitchToAlert) {
  EXPECT_CALL(*promos_manager_.get(),
              RegisterPromoForSingleDisplay(
                  promos_manager::Promo::PostRestoreSignInAlert))
      .Times(1);

  EnableFeatureVariationAlert();
  SetFakePreRestoreAccountInfo();

  MockAppStateChange(InitStageFinal);
}
