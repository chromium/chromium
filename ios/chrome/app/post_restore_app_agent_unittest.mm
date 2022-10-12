// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/post_restore_app_agent.h"

#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#import "ios/chrome/browser/signin/signin_util.h"
#import "ios/chrome/browser/ui/post_restore_signin/features.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/abseil-cpp/absl/types/optional.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  std::unique_ptr<PromosManager> promos_manager_;
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

  PromosManager* CreatePromosManager() {
    promos_manager_ = std::make_unique<PromosManager>(local_state_.Get());
    promos_manager_->Init();
    return promos_manager_.get();
  }

  AuthenticationService* CreateAuthService() {
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));
    browser_state_ = builder.Build();
    auth_service_ =
        AuthenticationServiceFactory::GetForBrowserState(browser_state_.get());
    return auth_service_;
  }

  int CountSingleDisplayActivePromos() {
    return local_state_.Get()
        ->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
        .size();
  }

  void ExpectRegisteredPromo(promos_manager::Promo promo) {
    const base::Value::List& promos = local_state_.Get()->GetList(
        prefs::kIosPromosManagerSingleDisplayActivePromos);
    EXPECT_EQ(promos.size(), unsigned(1));
    EXPECT_EQ(promos[0], promos_manager::NameForPromo(promo));
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

  void EnableFeatureVariationFullscreen() {
    scoped_feature_list_.InitAndEnableFeature(
        post_restore_signin::features::kIOSNewPostRestoreExperience);
  }

  void EnableFeatureVariationAlert() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {base::test::ScopedFeatureList::FeatureAndParams(
            post_restore_signin::features::kIOSNewPostRestoreExperience,
            {{post_restore_signin::features::kIOSNewPostRestoreExperienceParam,
              "true"}})},
        {});
  }
};

TEST_F(PostRestoreAppAgentTest, maybeRegisterPromo) {
  // Ensure that no promos are registered initially.
  EXPECT_EQ(CountSingleDisplayActivePromos(), 0);

  // Scenarios which should not register a promo.
  ClearPreRestoreIdentity(local_state_.Get());
  MockAppStateChange(InitStageFinal);
  EXPECT_EQ(CountSingleDisplayActivePromos(), 0);

  SetFakePreRestoreAccountInfo();
  MockAppStateChange(InitStageFinal);
  EXPECT_EQ(CountSingleDisplayActivePromos(), 0);

  ClearPreRestoreIdentity(local_state_.Get());
  EnableFeatureVariationFullscreen();
  MockAppStateChange(InitStageFinal);
  EXPECT_EQ(CountSingleDisplayActivePromos(), 0);
}

TEST_F(PostRestoreAppAgentTest, registerPromoFullscreen) {
  EnableFeatureVariationFullscreen();
  SetFakePreRestoreAccountInfo();
  MockAppStateChange(InitStageFinal);
  ExpectRegisteredPromo(promos_manager::Promo::PostRestoreSignInFullscreen);
}

TEST_F(PostRestoreAppAgentTest, registerPromoAlert) {
  EnableFeatureVariationAlert();
  SetFakePreRestoreAccountInfo();
  MockAppStateChange(InitStageFinal);
  ExpectRegisteredPromo(promos_manager::Promo::PostRestoreSignInAlert);
}

TEST_F(PostRestoreAppAgentTest, registerPromoDisablesReauthPrompt) {
  EnableFeatureVariationFullscreen();
  SetFakePreRestoreAccountInfo();
  auth_service_->SetReauthPromptForSignInAndSync();
  EXPECT_TRUE(auth_service_->ShouldReauthPromptForSignInAndSync());
  MockAppStateChange(InitStageFinal);
  EXPECT_FALSE(auth_service_->ShouldReauthPromptForSignInAndSync());
}

TEST_F(PostRestoreAppAgentTest, deregisterPromoFullscreen) {
  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::PostRestoreSignInFullscreen);
  EXPECT_EQ(CountSingleDisplayActivePromos(), 1);

  EnableFeatureVariationAlert();
  ClearPreRestoreIdentity(local_state_.Get());
  MockAppStateChange(InitStageFinal);
  EXPECT_EQ(CountSingleDisplayActivePromos(), 0);
}

TEST_F(PostRestoreAppAgentTest, deregisterPromoAlert) {
  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::PostRestoreSignInAlert);
  EXPECT_EQ(CountSingleDisplayActivePromos(), 1);

  EnableFeatureVariationAlert();
  ClearPreRestoreIdentity(local_state_.Get());
  MockAppStateChange(InitStageFinal);
  EXPECT_EQ(CountSingleDisplayActivePromos(), 0);
}

TEST_F(PostRestoreAppAgentTest, featureVariationSwitchToFullscreen) {
  // Simulate that the Alert promo was previously registered.
  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::PostRestoreSignInAlert);
  EXPECT_EQ(CountSingleDisplayActivePromos(), 1);

  EnableFeatureVariationFullscreen();
  SetFakePreRestoreAccountInfo();

  MockAppStateChange(InitStageFinal);
  ExpectRegisteredPromo(promos_manager::Promo::PostRestoreSignInFullscreen);
}

TEST_F(PostRestoreAppAgentTest, featureVariationSwitchToAlert) {
  // Simulate that the Fullscreen promo was previously registered.
  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::PostRestoreSignInFullscreen);
  EXPECT_EQ(CountSingleDisplayActivePromos(), 1);

  EnableFeatureVariationAlert();
  SetFakePreRestoreAccountInfo();

  MockAppStateChange(InitStageFinal);
  ExpectRegisteredPromo(promos_manager::Promo::PostRestoreSignInAlert);
}
