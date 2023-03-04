// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/ios_popular_sites_field_trial.h"

#import "base/feature_list.h"
#import "base/metrics/field_trial.h"
#import "base/metrics/field_trial_param_associator.h"
#import "base/test/mock_entropy_provider.h"
#import "base/test/scoped_feature_list.h"
#import "components/ntp_tiles/features.h"
#import "components/variations/variations_associated_data.h"
#import "ios/chrome/browser/ui/ntp/field_trial_constants.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests for field trial creation for the Trending Queries feature.
class PopularSitesImprovedSuggestionsFieldTrialTest : public PlatformTest {
 protected:
  void TearDown() override {
    scoped_feature_list_.Reset();
    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::MockEntropyProvider low_entropy_provider_;
};

// Tests default field trial group.
TEST_F(PopularSitesImprovedSuggestionsFieldTrialTest, TestDefault) {
  auto feature_list = std::make_unique<base::FeatureList>();

  std::map<variations::VariationID, int> weight_by_id = {};

  ios_popular_sites_field_trial::CreateImprovedSuggestionsTrialForTesting(
      std::move(weight_by_id), low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));

  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(
      field_trial_constants::
          kIOSPopularSitesImprovedSuggestionsFieldTrialName));
  EXPECT_FALSE(base::FeatureList::IsEnabled(
      ntp_tiles::kIOSPopularSitesImprovedSuggestions));

  ntp_tiles::IOSDefaultPopularSitesExperimentBehavior experiment_type =
      ntp_tiles::GetDefaultPopularSitesExperimentType();

  EXPECT_EQ(experiment_type,
            ntp_tiles::IOSDefaultPopularSitesExperimentBehavior::kDefault);
}

// Tests default field trial group (i.e. the control group).
TEST_F(PopularSitesImprovedSuggestionsFieldTrialTest, TestControl) {
  auto feature_list = std::make_unique<base::FeatureList>();

  std::map<variations::VariationID, int> weight_by_id = {
      {field_trial_constants::kIOSPopularSitesImprovedSuggestionsControlID,
       100}};

  ios_popular_sites_field_trial::CreateImprovedSuggestionsTrialForTesting(
      std::move(weight_by_id), low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));

  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(
      field_trial_constants::
          kIOSPopularSitesImprovedSuggestionsFieldTrialName));
  EXPECT_FALSE(base::FeatureList::IsEnabled(
      ntp_tiles::kIOSPopularSitesImprovedSuggestions));

  ntp_tiles::IOSDefaultPopularSitesExperimentBehavior experiment_type =
      ntp_tiles::GetDefaultPopularSitesExperimentType();

  EXPECT_EQ(experiment_type,
            ntp_tiles::IOSDefaultPopularSitesExperimentBehavior::kDefault);
}

// Tests field trial group where apps are included in the popular sites
// suggestions.
TEST_F(PopularSitesImprovedSuggestionsFieldTrialTest,
       TestIncludePopularAppsGroup) {
  auto feature_list = std::make_unique<base::FeatureList>();

  std::map<variations::VariationID, int> weight_by_id = {
      {field_trial_constants::
           kIOSPopularSitesImprovedSuggestionsWithAppsEnabledID,
       100}};

  ios_popular_sites_field_trial::CreateImprovedSuggestionsTrialForTesting(
      std::move(weight_by_id), low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(
      field_trial_constants::
          kIOSPopularSitesImprovedSuggestionsFieldTrialName));
  EXPECT_TRUE(base::FeatureList::IsEnabled(
      ntp_tiles::kIOSPopularSitesImprovedSuggestions));

  ntp_tiles::IOSDefaultPopularSitesExperimentBehavior experiment_type =
      ntp_tiles::GetDefaultPopularSitesExperimentType();

  EXPECT_EQ(
      experiment_type,
      ntp_tiles::IOSDefaultPopularSitesExperimentBehavior::kIncludePopularApps);
}

// Tests field trial group where apps are excluded in the popular sites
// suggestions.
TEST_F(PopularSitesImprovedSuggestionsFieldTrialTest,
       TestExcludePopularAppsGroup) {
  auto feature_list = std::make_unique<base::FeatureList>();

  std::map<variations::VariationID, int> weight_by_id = {
      {field_trial_constants::
           kIOSPopularSitesImprovedSuggestionsWithoutAppsEnabledID,
       100}};

  ios_popular_sites_field_trial::CreateImprovedSuggestionsTrialForTesting(
      std::move(weight_by_id), low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(
      field_trial_constants::
          kIOSPopularSitesImprovedSuggestionsFieldTrialName));
  EXPECT_TRUE(base::FeatureList::IsEnabled(
      ntp_tiles::kIOSPopularSitesImprovedSuggestions));

  ntp_tiles::IOSDefaultPopularSitesExperimentBehavior experiment_type =
      ntp_tiles::GetDefaultPopularSitesExperimentType();

  EXPECT_EQ(
      experiment_type,
      ntp_tiles::IOSDefaultPopularSitesExperimentBehavior::kExcludePopularApps);
}
