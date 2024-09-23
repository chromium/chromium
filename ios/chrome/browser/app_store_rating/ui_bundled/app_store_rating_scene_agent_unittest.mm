// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_store_rating/ui_bundled/app_store_rating_scene_agent.h"

#import "base/test/scoped_feature_list.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "components/variations/service/variations_service.h"
#import "components/variations/service/variations_service_client.h"
#import "components/variations/synthetic_trial_registry.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/app_store_rating/ui_bundled/features.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/mock_promos_manager.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/test/test_network_connection_tracker.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using ::testing::_;
using ::testing::AnyNumber;

namespace {

// TODO(crbug.com/40742801): Remove when fake VariationsServiceClient created.
class TestVariationsServiceClient : public variations::VariationsServiceClient {
 public:
  TestVariationsServiceClient() = default;
  TestVariationsServiceClient(const TestVariationsServiceClient&) = delete;
  TestVariationsServiceClient& operator=(const TestVariationsServiceClient&) =
      delete;
  ~TestVariationsServiceClient() override = default;

  // VariationsServiceClient:
  base::Version GetVersionForSimulation() override { return base::Version(); }
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return nullptr;
  }
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override {
    return nullptr;
  }
  bool OverridesRestrictParameter(std::string* parameter) override {
    return false;
  }
  bool IsEnterprise() override { return false; }
  void RemoveGoogleGroupsFromPrefsForDeletedProfiles(
      PrefService* local_state) override {}

 private:
  // VariationsServiceClient:
  version_info::Channel GetChannel() override {
    return version_info::Channel::UNKNOWN;
  }
};

// Creates a VariationsService and sets it as the TestingApplicationContext's
// VariationService for the life of the instance.
class ScopedVariationsService {
 public:
  ScopedVariationsService() {
    EXPECT_EQ(nullptr,
              TestingApplicationContext::GetGlobal()->GetVariationsService());
    synthetic_trial_registry_ =
        std::make_unique<variations::SyntheticTrialRegistry>();
    variations_service_ = variations::VariationsService::Create(
        std::make_unique<TestVariationsServiceClient>(),
        TestingApplicationContext::GetGlobal()->GetLocalState(),
        /*state_manager=*/nullptr, "dummy-disable-background-switch",
        variations::UIStringOverrider(),
        network::TestNetworkConnectionTracker::CreateGetter(),
        synthetic_trial_registry_.get());
    TestingApplicationContext::GetGlobal()->SetVariationsService(
        variations_service_.get());
  }

  ~ScopedVariationsService() {
    EXPECT_EQ(variations_service_.get(),
              TestingApplicationContext::GetGlobal()->GetVariationsService());
    TestingApplicationContext::GetGlobal()->SetVariationsService(nullptr);
    variations_service_.reset();
  }

  // Overrides the stored permanent country to be one where the default browser
  // check is expected to be disabled, in this case "fr".
  void SimulateCountryWhereDBIgnored() {
    variations_service_->OverrideStoredPermanentCountry("fr");
  }

  // Overrides the stored permanent country to be one where the default browser
  // check is expected to be in use, in this case "us".
  void SimulateCountryWhereDBUsed() {
    variations_service_->OverrideStoredPermanentCountry("us");
  }

  std::unique_ptr<variations::VariationsService> variations_service_;
  std::unique_ptr<variations::SyntheticTrialRegistry> synthetic_trial_registry_;
};

}  // namespace

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
    local_state()->ClearPref(prefs::kAppStoreRatingPolicyEnabled);
  }

  PrefService* local_state() {
    return GetApplicationContext()->GetLocalState();
  }

 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
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
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
    fake_scene_state_ =
        [[FakeSceneState alloc] initWithAppState:mockAppState
                                         profile:profile_.get()];
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
    local_state()->SetBoolean(
        password_manager::prefs::kCredentialProviderEnabledOnStartup, true);
  }

  // Disable Credentials Provider.
  void DisableCPE() {
    local_state()->SetBoolean(
        password_manager::prefs::kCredentialProviderEnabledOnStartup, false);
  }
};

#pragma mark - Tests

// Tests that promo display is not requested when all the conditions are met,
// but the App Store Rating policy is disabled.
TEST_F(AppStoreRatingSceneAgentTest, TestDisabledByPolicy) {
  EXPECT_CALL(*promos_manager_.get(), RegisterPromoForSingleDisplay(_))
      .Times(0);

  ScopedVariationsService scoped_variations_service;
  scoped_variations_service.SimulateCountryWhereDBUsed();
  EnableCPE();
  SetTrueChromeLikelyDefaultBrowser();

  // Disabling the policy.
  local_state()->SetBoolean(prefs::kAppStoreRatingPolicyEnabled, false);

  // Simulating the user launching or resuming the app.
  [test_scene_agent_ sceneState:fake_scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
}

// Tests that promo display is requested when the user meets all eligibility
// conditions.
TEST_F(AppStoreRatingSceneAgentTest, TestAllConditionsMet) {
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::AppStoreRating))
      .Times(1);

  ScopedVariationsService scoped_variations_service;
  scoped_variations_service.SimulateCountryWhereDBUsed();
  EnableCPE();
  SetTrueChromeLikelyDefaultBrowser();

  // Simulating the user launching or resuming the app.
  [test_scene_agent_ sceneState:fake_scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
}

// Tests that promo display is requested when both the CPE and DB conditions
// are met, and the user is in a country where the DB condition is ignored.
TEST_F(AppStoreRatingSceneAgentTest, TestAllConditionsDBIgnored) {
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::AppStoreRating))
      .Times(1);

  ScopedVariationsService scoped_variations_service;
  scoped_variations_service.SimulateCountryWhereDBIgnored();
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
// is met and the user is in a country where the DB condition is in use.
TEST_F(AppStoreRatingSceneAgentTest, TestOnlyDBMetAndDBInUse) {
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::AppStoreRating))
      .Times(1);

  ScopedVariationsService scoped_variations_service;
  scoped_variations_service.SimulateCountryWhereDBUsed();
  DisableCPE();
  SetTrueChromeLikelyDefaultBrowser();

  // Simulating the user launching or resuming the app.
  [test_scene_agent_ sceneState:fake_scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
}

// Tests that promo display is not requested when only the default browser
// condition is met and the user is in a country where the DB condition is
// ignored.
TEST_F(AppStoreRatingSceneAgentTest, TestOnlyDBMetAndDBIgnored) {
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::AppStoreRating))
      .Times(0);

  ScopedVariationsService scoped_variations_service;
  scoped_variations_service.SimulateCountryWhereDBIgnored();
  DisableCPE();
  SetTrueChromeLikelyDefaultBrowser();

  // Simulating the user launching or resuming the app.
  [test_scene_agent_ sceneState:fake_scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
}

// Tests that promo display is not requested when none of the conditions are
// met.
TEST_F(AppStoreRatingSceneAgentTest, TestNoConditionsMet) {
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::AppStoreRating))
      .Times(0);

  ScopedVariationsService scoped_variations_service;
  scoped_variations_service.SimulateCountryWhereDBIgnored();
  DisableCPE();
  SetFalseChromeLikelyDefaultBrowser();

  // Simulating the user launching or resuming the app.
  [test_scene_agent_ sceneState:fake_scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
}
