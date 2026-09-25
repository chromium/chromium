// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/universal_optout/model/eligibility_utils.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/simple_test_clock.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "components/metrics/metrics_state_manager.h"
#import "components/metrics/test/test_enabled_state_provider.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/universal_optout/features.h"
#import "components/universal_optout/prefs.h"
#import "components/universal_optout/universal_optout_service.h"
#import "components/variations/service/test_variations_service.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/web/public/web_client.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace universal_optout {

class UniversalOptOutEligibilityUtilsTest : public PlatformTest {
 public:
  UniversalOptOutEligibilityUtilsTest() {
    prefs::RegisterProfilePrefs(pref_service_.registry());
    variations::TestVariationsService::RegisterPrefs(pref_service_.registry());

    enabled_state_provider_ =
        std::make_unique<metrics::TestEnabledStateProvider>(/*consent=*/true,
                                                            /*enabled=*/true);
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        &pref_service_, enabled_state_provider_.get(),
        /*backup_registry_key=*/std::wstring(),
        /*user_data_dir=*/base::FilePath(),
        metrics::StartupVisibility::kUnknown);
    variations_service_ = std::make_unique<variations::TestVariationsService>(
        &pref_service_, metrics_state_manager_.get());

    base::Time start_time;
    CHECK(base::Time::FromString("2026-08-11T12:00:00Z", &start_time));
    test_clock_.SetNow(start_time);
  }

  ~UniversalOptOutEligibilityUtilsTest() override {
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:@"UniversalOptOutEligibilityOverride"];
  }

  void EnableAllFeatures() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kUniversalOptOut,
                              features::kUniversalOptOutExtension,
                              features::kUniversalOptOutSettings},
        /*disabled_features=*/{});
  }

  std::unique_ptr<UniversalOptOutService> CreateOptOutService(bool eligible) {
    auto service = std::make_unique<UniversalOptOutService>(
        pref_service_, *variations_service_,
        *identity_test_env_.identity_manager(), test_clock_);
    pref_service_.SetBoolean(prefs::kUniversalOptOutEligible, eligible);
    return service;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<metrics::TestEnabledStateProvider> enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<variations::TestVariationsService> variations_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  base::SimpleTestClock test_clock_;
};

// Tests that a profile with default preferences is not eligible.
TEST_F(UniversalOptOutEligibilityUtilsTest, TestNotEligibleByDefault) {
  EnableAllFeatures();
  EXPECT_EQ(web::UniversalOptOutState::kNotEligible,
            GetUniversalOptOutState(&pref_service_));
}

// Tests that when feature flags are uninitialized/disabled, an ineligible
// profile returns kNotEligible without evaluating feature flags.
TEST_F(UniversalOptOutEligibilityUtilsTest,
       TestIneligibleDoesNotTriggerFinchActivation) {
  // Feature list is intentionally empty (features disabled).
  EXPECT_EQ(web::UniversalOptOutState::kNotEligible,
            GetUniversalOptOutState(&pref_service_));
}

// Tests that setting kUniversalOptOutEligible pref makes the profile eligible.
TEST_F(UniversalOptOutEligibilityUtilsTest, TestEligibleViaPref) {
  EnableAllFeatures();
  pref_service_.SetBoolean(prefs::kUniversalOptOutEligible, true);
  EXPECT_EQ(web::UniversalOptOutState::kEligible,
            GetUniversalOptOutState(&pref_service_));
}

// Tests that setting kUniversalOptOutEnabled pref marks the state as enabled.
TEST_F(UniversalOptOutEligibilityUtilsTest, TestEnabledViaPref) {
  EnableAllFeatures();
  pref_service_.SetBoolean(prefs::kUniversalOptOutEnabled, true);
  EXPECT_EQ(web::UniversalOptOutState::kEnabled,
            GetUniversalOptOutState(&pref_service_));
}

// Tests eligibility when evaluated via UniversalOptOutService.
TEST_F(UniversalOptOutEligibilityUtilsTest, TestEligibilityViaService) {
  EnableAllFeatures();
  std::unique_ptr<UniversalOptOutService> eligible_service =
      CreateOptOutService(/*eligible=*/true);
  EXPECT_EQ(web::UniversalOptOutState::kEligible,
            GetUniversalOptOutState(&pref_service_, eligible_service.get()));

  std::unique_ptr<UniversalOptOutService> ineligible_service =
      CreateOptOutService(/*eligible=*/false);
  EXPECT_EQ(web::UniversalOptOutState::kNotEligible,
            GetUniversalOptOutState(&pref_service_, ineligible_service.get()));
}

// Tests that state is kNotEligible if the base UniversalOptOut feature is
// disabled.
TEST_F(UniversalOptOutEligibilityUtilsTest, TestBaseFeatureDisabled) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kUniversalOptOutExtension,
                            features::kUniversalOptOutSettings},
      /*disabled_features=*/{features::kUniversalOptOut});
  pref_service_.SetBoolean(prefs::kUniversalOptOutEligible, true);
  EXPECT_EQ(web::UniversalOptOutState::kNotEligible,
            GetUniversalOptOutState(&pref_service_));
}

// Tests that state is kNotEligible if kUniversalOptOutSettings is disabled.
TEST_F(UniversalOptOutEligibilityUtilsTest, TestSettingsFeatureDisabled) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kUniversalOptOut,
                            features::kUniversalOptOutExtension},
      /*disabled_features=*/{features::kUniversalOptOutSettings});
  pref_service_.SetBoolean(prefs::kUniversalOptOutEligible, true);
  EXPECT_EQ(web::UniversalOptOutState::kNotEligible,
            GetUniversalOptOutState(&pref_service_));
}

// Tests that the experimental flag kForcedOff overrides both eligible and
// enabled states.
TEST_F(UniversalOptOutEligibilityUtilsTest,
       TestForcedOffOverridesEligibleAndEnabled) {
  EnableAllFeatures();
  pref_service_.SetBoolean(prefs::kUniversalOptOutEligible, true);
  pref_service_.SetBoolean(prefs::kUniversalOptOutEnabled, true);

  [[NSUserDefaults standardUserDefaults]
      setInteger:static_cast<NSInteger>(
                     experimental_flags::UniversalOptOutEligibilityOverride::
                         kForcedOff)
          forKey:@"UniversalOptOutEligibilityOverride"];

  EXPECT_EQ(web::UniversalOptOutState::kNotEligible,
            GetUniversalOptOutState(&pref_service_));
}

// Tests that the experimental flag kForcedOn overrides an ineligible state.
TEST_F(UniversalOptOutEligibilityUtilsTest,
       TestForcedOnOverridesIneligibleState) {
  EnableAllFeatures();
  pref_service_.SetBoolean(prefs::kUniversalOptOutEligible, false);
  pref_service_.SetBoolean(prefs::kUniversalOptOutEnabled, false);

  [[NSUserDefaults standardUserDefaults]
      setInteger:
          static_cast<NSInteger>(
              experimental_flags::UniversalOptOutEligibilityOverride::kForcedOn)
          forKey:@"UniversalOptOutEligibilityOverride"];

  EXPECT_EQ(web::UniversalOptOutState::kEligible,
            GetUniversalOptOutState(&pref_service_));

  // If also enabled via pref, it should report enabled.
  pref_service_.SetBoolean(prefs::kUniversalOptOutEnabled, true);
  EXPECT_EQ(web::UniversalOptOutState::kEnabled,
            GetUniversalOptOutState(&pref_service_));
}

}  // namespace universal_optout
