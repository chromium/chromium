// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/trending_queries_field_trial.h"

#import "base/feature_list.h"
#import "base/metrics/field_trial.h"
#import "base/metrics/field_trial_param_associator.h"
#import "base/test/mock_entropy_provider.h"
#import "base/test/scoped_feature_list.h"
#import "components/variations/variations_associated_data.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests for field trial creation for the Trending Queries feature.
class TrendingQueriesFieldTrialTest : public PlatformTest {
 protected:
  void SetUp() override {
    weight_by_id_ = {
        {kTrendingQueriesEnabledModuleEnabledID, 0},
        {kTrendingQueriesEnabledMinimalSpacingModuleEnabledID, 0},
        {kTrendingQueriesEnabledMinimalSpacingRemoveHeaderModuleEnabledID, 0},
        {kTrendingQueriesKeepShortcutsEnabledModuleEnabledID, 0},
        {kTrendingQueriesControlID, 0}};
  }

  void TearDown() override {
    scoped_feature_list_.Reset();
    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::map<variations::VariationID, int> weight_by_id_;
  base::MockEntropyProvider low_entropy_provider_;
};

// Tests default field trial group.
TEST_F(TrendingQueriesFieldTrialTest, TestDefault) {
  auto feature_list = std::make_unique<base::FeatureList>();
  trending_queries_field_trial::CreateTrendingQueriesTrialForTesting(
      weight_by_id_, low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(
      kModularHomeTrendingQueriesClientSideFieldTrialName));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kTrendingQueriesModuleNewUser));
  EXPECT_FALSE(
      base::FeatureList::IsEnabled(kContentSuggestionsUIModuleRefreshNewUser));
}

// Tests control field trial.
TEST_F(TrendingQueriesFieldTrialTest, TestControl) {
  auto feature_list = std::make_unique<base::FeatureList>();
  weight_by_id_[kTrendingQueriesControlID] = 100;
  trending_queries_field_trial::CreateTrendingQueriesTrialForTesting(
      weight_by_id_, low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(
      kModularHomeTrendingQueriesClientSideFieldTrialName));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kTrendingQueriesModuleNewUser));
  EXPECT_FALSE(
      base::FeatureList::IsEnabled(kContentSuggestionsUIModuleRefreshNewUser));
}

// Tests kTrendingQueriesEnabledModuleEnabledID field trial.
TEST_F(TrendingQueriesFieldTrialTest, TestTrendingQueriesEnabledModuleEnabled) {
  auto feature_list = std::make_unique<base::FeatureList>();
  weight_by_id_[kTrendingQueriesEnabledModuleEnabledID] = 100;
  trending_queries_field_trial::CreateTrendingQueriesTrialForTesting(
      weight_by_id_, low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(
      kModularHomeTrendingQueriesClientSideFieldTrialName));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kTrendingQueriesModuleNewUser));
  EXPECT_TRUE(
      base::FeatureList::IsEnabled(kContentSuggestionsUIModuleRefreshNewUser));
  EXPECT_TRUE(base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModuleNewUser, kTrendingQueriesHideShortcutsParam,
      false));
  EXPECT_FALSE(base::GetFieldTrialParamByFeatureAsBool(
      kContentSuggestionsUIModuleRefreshNewUser,
      kContentSuggestionsUIModuleRefreshMinimizeSpacingParam, true));
  EXPECT_FALSE(base::GetFieldTrialParamByFeatureAsBool(
      kContentSuggestionsUIModuleRefreshNewUser,
      kContentSuggestionsUIModuleRefreshRemoveHeadersParam, true));
}

// Tests kTrendingQueriesEnabledMinimalSpacingModuleEnabledID field trial.
TEST_F(TrendingQueriesFieldTrialTest,
       TestTrendingQueriesEnabledMinimalSpacingModuleEnabled) {
  auto feature_list = std::make_unique<base::FeatureList>();
  weight_by_id_[kTrendingQueriesEnabledMinimalSpacingModuleEnabledID] = 100;
  trending_queries_field_trial::CreateTrendingQueriesTrialForTesting(
      weight_by_id_, low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(
      kModularHomeTrendingQueriesClientSideFieldTrialName));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kTrendingQueriesModuleNewUser));
  EXPECT_TRUE(
      base::FeatureList::IsEnabled(kContentSuggestionsUIModuleRefreshNewUser));
  EXPECT_TRUE(base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModuleNewUser, kTrendingQueriesHideShortcutsParam,
      false));
  EXPECT_TRUE(base::GetFieldTrialParamByFeatureAsBool(
      kContentSuggestionsUIModuleRefreshNewUser,
      kContentSuggestionsUIModuleRefreshMinimizeSpacingParam, false));
  EXPECT_FALSE(base::GetFieldTrialParamByFeatureAsBool(
      kContentSuggestionsUIModuleRefreshNewUser,
      kContentSuggestionsUIModuleRefreshRemoveHeadersParam, true));
}

// Tests kTrendingQueriesEnabledMinimalSpacingRemoveHeaderModuleEnabledID field
// trial.
TEST_F(TrendingQueriesFieldTrialTest,
       TestTrendingQueriesEnabledMinimalSpacingRemoveHeaderModuleEnabled) {
  auto feature_list = std::make_unique<base::FeatureList>();
  weight_by_id_
      [kTrendingQueriesEnabledMinimalSpacingRemoveHeaderModuleEnabledID] = 100;
  trending_queries_field_trial::CreateTrendingQueriesTrialForTesting(
      weight_by_id_, low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(
      kModularHomeTrendingQueriesClientSideFieldTrialName));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kTrendingQueriesModuleNewUser));
  EXPECT_TRUE(
      base::FeatureList::IsEnabled(kContentSuggestionsUIModuleRefreshNewUser));
  EXPECT_TRUE(base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModuleNewUser, kTrendingQueriesHideShortcutsParam,
      false));
  EXPECT_TRUE(base::GetFieldTrialParamByFeatureAsBool(
      kContentSuggestionsUIModuleRefreshNewUser,
      kContentSuggestionsUIModuleRefreshMinimizeSpacingParam, false));
  EXPECT_TRUE(base::GetFieldTrialParamByFeatureAsBool(
      kContentSuggestionsUIModuleRefreshNewUser,
      kContentSuggestionsUIModuleRefreshRemoveHeadersParam, false));
}

// Tests kTrendingQueriesKeepShortcutsEnabledModuleEnabledID field trial.
TEST_F(TrendingQueriesFieldTrialTest,
       TestTrendingQueriesKeepShortcutsEnabledModuleEnabled) {
  auto feature_list = std::make_unique<base::FeatureList>();
  weight_by_id_[kTrendingQueriesKeepShortcutsEnabledModuleEnabledID] = 100;
  trending_queries_field_trial::CreateTrendingQueriesTrialForTesting(
      weight_by_id_, low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(
      kModularHomeTrendingQueriesClientSideFieldTrialName));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kTrendingQueriesModuleNewUser));
  EXPECT_TRUE(
      base::FeatureList::IsEnabled(kContentSuggestionsUIModuleRefreshNewUser));
  EXPECT_FALSE(base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModuleNewUser, kTrendingQueriesHideShortcutsParam, true));
  EXPECT_FALSE(base::GetFieldTrialParamByFeatureAsBool(
      kContentSuggestionsUIModuleRefreshNewUser,
      kContentSuggestionsUIModuleRefreshMinimizeSpacingParam, true));
  EXPECT_FALSE(base::GetFieldTrialParamByFeatureAsBool(
      kContentSuggestionsUIModuleRefreshNewUser,
      kContentSuggestionsUIModuleRefreshRemoveHeadersParam, true));
}
