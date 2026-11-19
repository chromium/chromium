// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/coordinator/first_run_post_action_provider.h"

#import <memory>
#import <utility>

#import "base/feature_list.h"
#import "base/metrics/field_trial.h"
#import "base/test/scoped_feature_list.h"
#import "base/values.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/safari_data_import/model/features.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_type.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class FirstRunPostActionProviderTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    // Register prefs needed by ShouldShowSafariDataImportEntryPoint if any.
    // Assuming it uses standard prefs or we can mock it via feature flags.
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kAppStoreGeminiPromoTriggered, false);
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    local_state_->registry()->RegisterListPref(prefs::kFirstRunDeferredScreens);
  }

  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  base::test::ScopedFeatureList feature_list_;
};

// Tests that all post-FRE screens are skipped when the App Store In-App event
// promo is enabled and the Gemini promo has triggered.
TEST_F(FirstRunPostActionProviderTest, SkipScreensForGeminiPromo) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kPageActionMenu}, {});

  pref_service_->SetBoolean(prefs::kAppStoreGeminiPromoTriggered, true);

  FirstRunPostActionProvider* provider = [[FirstRunPostActionProvider alloc]
      initWithProfilePrefs:pref_service_.get()
                localState:local_state_.get()];

  ScreenType type = [provider nextScreenType];
  EXPECT_EQ(type, ScreenType::kStepsCompleted);
}

// Tests that Safari Import is skipped if the Guided Tour has started.
TEST_F(FirstRunPostActionProviderTest, SkipSafariImportIfTourStarted) {
  feature_list_.InitAndEnableFeatureWithParameters(kBestOfAppFRE,
                                                   {{"variant", "4"}});

  FirstRunPostActionProvider* provider = [[FirstRunPostActionProvider alloc]
      initWithProfilePrefs:pref_service_.get()
                localState:local_state_.get()];

  // Advance to Guided Tour.
  BOOL foundGuidedTour = NO;
  ScreenType type = [provider nextScreenType];
  while (type != ScreenType::kStepsCompleted) {
    if (type == ScreenType::kGuidedTour) {
      foundGuidedTour = YES;
      break;
    }
    type = [provider nextScreenType];
  }

  ASSERT_TRUE(foundGuidedTour);

  // Mark tour as started.
  [provider setGuidedTourStarted:YES];

  // The next screen should NOT be `kSafariImport`.
  type = [provider nextScreenType];
  EXPECT_NE(type, ScreenType::kSafariImport);
  EXPECT_EQ(type, ScreenType::kStepsCompleted);
}

// Tests that Safari Import is NOT skipped if the Guided Tour has NOT started.
TEST_F(FirstRunPostActionProviderTest, ShowSafariImportIfTourNotStarted) {
  if (!@available(iOS 18.2, *)) {
    GTEST_SKIP() << "Safari data import requires iOS 18.2+.";
  }

  feature_list_.InitAndEnableFeatureWithParameters(kBestOfAppFRE,
                                                   {{"variant", "4"}});

  FirstRunPostActionProvider* provider = [[FirstRunPostActionProvider alloc]
      initWithProfilePrefs:pref_service_.get()
                localState:local_state_.get()];

  BOOL foundGuidedTour = NO;
  ScreenType type = [provider nextScreenType];
  while (type != ScreenType::kStepsCompleted) {
    if (type == ScreenType::kGuidedTour) {
      foundGuidedTour = YES;
      break;
    }
    type = [provider nextScreenType];
  }

  ASSERT_TRUE(foundGuidedTour);

  // Do NOT mark tour as started.
  [provider setGuidedTourStarted:NO];

  // The next screen should be `kSafariImport`.
  type = [provider nextScreenType];
  EXPECT_EQ(type, ScreenType::kSafariImport);
}

// Tests that post-First Run actions are skipped when `kFREAblation` is in the
// `kSkipAllExceptSignInAndChoice` arm and screens were deferred.
TEST_F(FirstRunPostActionProviderTest,
       SkipPostActionsForSkipAllExceptSignInAndChoice) {
  feature_list_.InitWithFeaturesAndParameters(
      {{first_run::kFREAblation,
        {{first_run::kFREAblationParam,
          first_run::kFREAblationParamSkipAllExceptSignInAndChoice}}}},
      {kBestOfAppFRE});

  base::ListValue deferred_screens;
  deferred_screens.Append(static_cast<int>(ScreenType::kDefaultBrowserPromo));
  local_state_->SetList(prefs::kFirstRunDeferredScreens,
                        std::move(deferred_screens));

  FirstRunPostActionProvider* provider = [[FirstRunPostActionProvider alloc]
      initWithProfilePrefs:pref_service_.get()
                localState:local_state_.get()];

  EXPECT_EQ([provider nextScreenType], ScreenType::kStepsCompleted);
}

// Tests that post-First Run actions are still returned when `kFREAblation` is
// in the `kSkipHistorySync` arm.
TEST_F(FirstRunPostActionProviderTest, KeepPostActionsForSkipHistorySync) {
  feature_list_.InitWithFeaturesAndParameters(
      {{first_run::kFREAblation,
        {{first_run::kFREAblationParam,
          first_run::kFREAblationParamSkipHistorySync}}}},
      {kBestOfAppFRE});

  base::ListValue deferred_screens;
  deferred_screens.Append(static_cast<int>(ScreenType::kHistorySync));
  local_state_->SetList(prefs::kFirstRunDeferredScreens,
                        std::move(deferred_screens));

  FirstRunPostActionProvider* provider = [[FirstRunPostActionProvider alloc]
      initWithProfilePrefs:pref_service_.get()
                localState:local_state_.get()];

  EXPECT_EQ([provider nextScreenType], ScreenType::kSyncedSetUp);
}

// Tests that post-First Run actions are kept and `kFREAblation` is not
// activated when `prefs::kFirstRunDeferredScreens` is empty.
TEST_F(FirstRunPostActionProviderTest,
       KeepPostActionsWhenDeferredScreensEmpty) {
  feature_list_.InitWithFeaturesAndParameters(
      {{first_run::kFREAblation,
        {{first_run::kFREAblationParam,
          first_run::kFREAblationParamSkipAllExceptSignInAndChoice}}}},
      {kBestOfAppFRE});
  base::FieldTrial* trial =
      base::FeatureList::GetFieldTrial(first_run::kFREAblation);
  ASSERT_NE(trial, nullptr);

  FirstRunPostActionProvider* provider = [[FirstRunPostActionProvider alloc]
      initWithProfilePrefs:pref_service_.get()
                localState:local_state_.get()];

  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(trial->trial_name()));
  EXPECT_EQ([provider nextScreenType], ScreenType::kSyncedSetUp);
}
