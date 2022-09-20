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
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/signin/signin_util.h"
#import "ios/chrome/browser/ui/post_restore_signin/features.h"
#import "testing/platform_test.h"
#import "third_party/abseil-cpp/absl/types/optional.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests the PostRestoreAppAgent.
class PostRestoreAppAgentTest : public PlatformTest {
 public:
  explicit PostRestoreAppAgentTest() { CreateAppAgent(); }

  PostRestoreAppAgent* appAgent_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  std::unique_ptr<PromosManager> promos_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  id mockAppState_;

  void CreateAppAgent() {
    CreatePromosManager();
    appAgent_ = [[PostRestoreAppAgent alloc] init];
    appAgent_.promosManager = promos_manager_.get();
    mockAppState_ = OCMClassMock([AppState class]);
    [appAgent_ setAppState:mockAppState_];
  }

  void CreatePromosManager() {
    CreatePrefs();
    promos_manager_ = std::make_unique<PromosManager>(local_state_.get());
    promos_manager_->Init();
  }

  // Create pref registry for tests.
  void CreatePrefs() {
    local_state_ = std::make_unique<TestingPrefServiceSimple>();

    local_state_->registry()->RegisterListPref(
        prefs::kIosPromosManagerImpressions);
    local_state_->registry()->RegisterListPref(
        prefs::kIosPromosManagerActivePromos);
    local_state_->registry()->RegisterListPref(
        prefs::kIosPromosManagerSingleDisplayActivePromos);
  }

  int CountSingleDisplayActivePromos() {
    return local_state_
        ->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
        .size();
  }

  void MockAppStateChange(InitStage initStage) {
    OCMStub([mockAppState_ initStage]).andReturn(initStage);
    [appAgent_ appState:mockAppState_
        didTransitionFromInitStage:InitStageStart];
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
  ClearPreRestoreIdentity();
  MockAppStateChange(InitStageFinal);
  EXPECT_EQ(CountSingleDisplayActivePromos(), 0);

  AccountInfo accountInfo;
  accountInfo.email = std::string("person@example.org");
  StorePreRestoreIdentity(accountInfo);
  MockAppStateChange(InitStageFinal);
  EXPECT_EQ(CountSingleDisplayActivePromos(), 0);

  ClearPreRestoreIdentity();
  EnableFeatureVariationFullscreen();
  MockAppStateChange(InitStageFinal);
  EXPECT_EQ(CountSingleDisplayActivePromos(), 0);
}

TEST_F(PostRestoreAppAgentTest, registerPromoFullscreen) {
  EnableFeatureVariationFullscreen();
  AccountInfo accountInfo;
  accountInfo.email = std::string("person@example.org");
  StorePreRestoreIdentity(accountInfo);
  MockAppStateChange(InitStageFinal);
  const base::Value::List& promos =
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos);
  std::string expectedName = promos_manager::NameForPromo(
      promos_manager::Promo::PostRestoreSignInFullscreen);
  EXPECT_EQ(promos[0], expectedName);
}

TEST_F(PostRestoreAppAgentTest, registerPromoAlert) {
  EnableFeatureVariationAlert();
  AccountInfo accountInfo;
  accountInfo.email = std::string("person@example.org");
  StorePreRestoreIdentity(accountInfo);
  MockAppStateChange(InitStageFinal);
  const base::Value::List& promos =
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos);
  std::string expectedName = promos_manager::NameForPromo(
      promos_manager::Promo::PostRestoreSignInAlert);
  EXPECT_EQ(promos[0], expectedName);
}
