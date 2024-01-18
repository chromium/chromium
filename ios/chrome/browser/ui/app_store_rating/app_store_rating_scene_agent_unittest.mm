// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/app_store_rating/app_store_rating_scene_agent.h"

#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/mock_promos_manager.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using ::testing::_;
using ::testing::AnyNumber;

// Test fixture for testing AppStoreRatingSceneAgent class.
class AppStoreRatingSceneAgentTest : public PlatformTest {
 protected:
  AppStoreRatingSceneAgentTest() {
    CreateMockPromosManager();
    CreateFakeSceneState();
    CreateAppStoreRatingSceneAgent();
  }

  ~AppStoreRatingSceneAgentTest() override {
    ClearDefaultBrowserPromoData();
    local_state_.Get()->ClearPref(prefs::kAppStoreRatingPolicyEnabled);
  }

 protected:
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  web::WebTaskEnvironment task_environment_;
  AppStoreRatingSceneAgent* test_scene_agent_;
  std::unique_ptr<MockPromosManager> promos_manager_;
  FakeSceneState* fake_scene_state_;

  // Create a MockPromosManager.
  void CreateMockPromosManager() {
    promos_manager_ = std::make_unique<MockPromosManager>();
  }

  // Create a FakeSceneState.
  void CreateFakeSceneState() {
    id mockAppState = OCMClassMock([AppState class]);
    TestChromeBrowserState::Builder builder;
    browser_state_ = builder.Build();
    fake_scene_state_ =
        [[FakeSceneState alloc] initWithAppState:mockAppState
                                    browserState:browser_state_.get()];
  }

  // Create an AppStoreRatingSceneAgent to test.
  void CreateAppStoreRatingSceneAgent() {
    test_scene_agent_ = [[AppStoreRatingSceneAgent alloc]
        initWithPromosManager:promos_manager_.get()];
    test_scene_agent_.sceneState = fake_scene_state_;
  }

  // Ensure that Chrome is considered as default browser.
  void SetTrueChromeLikelyDefaultBrowser() { LogOpenHTTPURLFromExternalURL(); }

  // Ensure that Chrome is not considered as default browser.
  void SetFalseChromeLikelyDefaultBrowser() { ClearDefaultBrowserPromoData(); }

  // Enable Credentials Provider.
  void EnableCPE() {
    browser_state_->GetPrefs()->SetBoolean(
        password_manager::prefs::kCredentialProviderEnabledOnStartup, true);
  }

  // Disable Credentials Provider.
  void DisableCPE() {
    browser_state_->GetPrefs()->SetBoolean(
        password_manager::prefs::kCredentialProviderEnabledOnStartup, false);
  }
};

#pragma mark - Tests

// Tests that promo display is not requested when the App Store Rating policy is
// disabled.
TEST_F(AppStoreRatingSceneAgentTest, TestDisabledByPolicy) {
  EXPECT_CALL(*promos_manager_.get(), RegisterPromoForSingleDisplay(_))
      .Times(0);

  EnableCPE();
  SetTrueChromeLikelyDefaultBrowser();

  // Disabling the policy.
  local_state_.Get()->SetBoolean(prefs::kAppStoreRatingPolicyEnabled, false);

  // Simulating the user launching or resuming the app.
  [test_scene_agent_ sceneState:fake_scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
}

// Tests that promo display is requested when the user meets all
// requirements to be considered engaged.
TEST_F(AppStoreRatingSceneAgentTest, TestAllConditionsMet) {
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::AppStoreRating))
      .Times(1);

  EnableCPE();
  SetTrueChromeLikelyDefaultBrowser();

  // Simulating the user launching or resuming the app.
  [test_scene_agent_ sceneState:fake_scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
}

// Tests that promo display is requested when only the CPE condition is met.
TEST_F(AppStoreRatingSceneAgentTest, TestOnlyCPEMet) {
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::AppStoreRating))
      .Times(1);

  EnableCPE();
  SetFalseChromeLikelyDefaultBrowser();

  // Simulating the user launching or resuming the app.
  [test_scene_agent_ sceneState:fake_scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
}

// Tests that promo display is requested when only the default browser condition
// is met.
TEST_F(AppStoreRatingSceneAgentTest, TestOnlyDBMet) {
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::AppStoreRating))
      .Times(1);

  DisableCPE();
  SetTrueChromeLikelyDefaultBrowser();

  // Simulating the user launching or resuming the app.
  [test_scene_agent_ sceneState:fake_scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
}

// Tests that promo display is NOT requested when none of the conditions are
// met.
TEST_F(AppStoreRatingSceneAgentTest, TestPromoNotRequestedLoosenedTriggers) {
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::AppStoreRating))
      .Times(0);

  DisableCPE();
  SetFalseChromeLikelyDefaultBrowser();

  // Simulating the user launching or resuming the app.
  [test_scene_agent_ sceneState:fake_scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
}
