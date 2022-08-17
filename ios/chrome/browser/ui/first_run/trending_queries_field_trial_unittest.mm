// Copyright 2022 The Chromium Authors. All rights reserved.
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
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests for field trial creation for the Trending Queries feature.
class TrendingQueriesFieldTrialTest : public PlatformTest {
 protected:
  void SetUp() override {
    std::map<variations::VariationID, int> weight_by_id_ = {
        {kTrendingQueriesEnabledAllUsersID, 0},
        {kTrendingQueriesEnabledAllUsersHideShortcutsID, 0},
        {kTrendingQueriesEnabledDisabledFeedID, 0},
        {kTrendingQueriesEnabledSignedOutID, 0},
        {kTrendingQueriesEnabledNeverShowModuleID, 0},
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
  ASSERT_TRUE(
      base::FieldTrialList::IsTrialActive(kTrendingQueriesFieldTrialName));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kTrendingQueriesModule));
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
  ASSERT_TRUE(
      base::FieldTrialList::IsTrialActive(kTrendingQueriesFieldTrialName));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kTrendingQueriesModule));
}

// Tests kTrendingQueriesEnabledAllUsersID field trial.
TEST_F(TrendingQueriesFieldTrialTest, TestEnabledAllUsers) {
  auto feature_list = std::make_unique<base::FeatureList>();
  weight_by_id_[kTrendingQueriesEnabledAllUsersID] = 100;
  trending_queries_field_trial::CreateTrendingQueriesTrialForTesting(
      weight_by_id_, low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(
      base::FieldTrialList::IsTrialActive(kTrendingQueriesFieldTrialName));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kTrendingQueriesModule));
  EXPECT_FALSE(base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesHideShortcutsParam, true));
  EXPECT_FALSE(base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesDisabledFeedParam, true));
  EXPECT_FALSE(base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesSignedOutParam, true));
  EXPECT_FALSE(base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesNeverShowModuleParam, false));
}

// Tests kTrendingQueriesEnabledAllUsersHideShortcutsID field trial.
TEST_F(TrendingQueriesFieldTrialTest, TestEnabledHideShortcuts) {
  auto feature_list = std::make_unique<base::FeatureList>();
  weight_by_id_[kTrendingQueriesEnabledAllUsersHideShortcutsID] = 100;
  trending_queries_field_trial::CreateTrendingQueriesTrialForTesting(
      weight_by_id_, low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(
      base::FieldTrialList::IsTrialActive(kTrendingQueriesFieldTrialName));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kTrendingQueriesModule));
  EXPECT_TRUE(base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesHideShortcutsParam, false));
  EXPECT_FALSE(base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesDisabledFeedParam, true));
  EXPECT_FALSE(base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesSignedOutParam, true));
  EXPECT_FALSE(base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesNeverShowModuleParam, false));
}

// Tests kTrendingQueriesEnabledDisabledFeedID field trial.
TEST_F(TrendingQueriesFieldTrialTest, TestEnabledDisabledFeed) {
  auto feature_list = std::make_unique<base::FeatureList>();
  weight_by_id_[kTrendingQueriesEnabledDisabledFeedID] = 100;
  trending_queries_field_trial::CreateTrendingQueriesTrialForTesting(
      weight_by_id_, low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(
      base::FieldTrialList::IsTrialActive(kTrendingQueriesFieldTrialName));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kTrendingQueriesModule));
  EXPECT_FALSE(base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesHideShortcutsParam, true));
  EXPECT_TRUE(base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesDisabledFeedParam, false));
  EXPECT_FALSE(base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesSignedOutParam, true));
  EXPECT_FALSE(base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesNeverShowModuleParam, false));
}

// Tests kTrendingQueriesEnabledSignedOutID field trial.
TEST_F(TrendingQueriesFieldTrialTest, TestEnabledSignedOut) {
  auto feature_list = std::make_unique<base::FeatureList>();
  weight_by_id_[kTrendingQueriesEnabledSignedOutID] = 100;
  trending_queries_field_trial::CreateTrendingQueriesTrialForTesting(
      weight_by_id_, low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(
      base::FieldTrialList::IsTrialActive(kTrendingQueriesFieldTrialName));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kTrendingQueriesModule));
  EXPECT_TRUE(base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesHideShortcutsParam, false));
  EXPECT_FALSE(base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesDisabledFeedParam, true));
  EXPECT_TRUE(base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesSignedOutParam, false));
  EXPECT_FALSE(base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesNeverShowModuleParam, false));
}

// Tests kTrendingQueriesEnabledNeverShowModuleID field trial.
TEST_F(TrendingQueriesFieldTrialTest, TestEnabledNeverShowModule) {
  auto feature_list = std::make_unique<base::FeatureList>();
  weight_by_id_[kTrendingQueriesEnabledNeverShowModuleID] = 100;
  trending_queries_field_trial::CreateTrendingQueriesTrialForTesting(
      weight_by_id_, low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(
      base::FieldTrialList::IsTrialActive(kTrendingQueriesFieldTrialName));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kTrendingQueriesModule));
  EXPECT_TRUE(base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesHideShortcutsParam, false));
  EXPECT_FALSE(base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesDisabledFeedParam, false));
  EXPECT_FALSE(base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesSignedOutParam, false));
  EXPECT_TRUE(base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesNeverShowModuleParam, false));
}
