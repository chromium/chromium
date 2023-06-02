// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_field_trial.h"

#import "base/feature_list.h"
#import "base/metrics/field_trial.h"
#import "base/metrics/field_trial_param_associator.h"
#import "base/test/mock_entropy_provider.h"
#import "base/test/scoped_feature_list.h"
#import "components/ntp_tiles/features.h"
#import "components/variations/variations_associated_data.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_field_trial_constants.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests for field trial creation for the new tab page field trial experiments.
class NewTabPageFieldTrialTest : public PlatformTest {
 protected:
  void TearDown() override {
    scoped_feature_list_.Reset();
    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::MockEntropyProvider low_entropy_provider_;
};

// Tests default field trial group.
TEST_F(NewTabPageFieldTrialTest, TestDefault) {
  auto feature_list = std::make_unique<base::FeatureList>();

  std::map<variations::VariationID, int> weight_by_id = {};

  new_tab_page_field_trial::CreateNewTabPageFieldTrialForTesting(
      std::move(weight_by_id), low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));

  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(
      ntp_tiles::kNewTabPageFieldTrial.name));
  EXPECT_FALSE(base::FeatureList::IsEnabled(ntp_tiles::kNewTabPageFieldTrial));

  ntp_tiles::NewTabPageFieldTrialExperimentBehavior experiment_type =
      ntp_tiles::GetNewTabPageFieldTrialExperimentType();

  EXPECT_EQ(experiment_type,
            ntp_tiles::NewTabPageFieldTrialExperimentBehavior::kDefault);
}

// Tests that the tile ablation control group uses default behavior.
TEST_F(NewTabPageFieldTrialTest, TestTileAblationControl) {
  auto feature_list = std::make_unique<base::FeatureList>();

  std::map<variations::VariationID, int> weight_by_id = {
      {field_trial_constants::kTileAblationControlID, 100}};

  new_tab_page_field_trial::CreateNewTabPageFieldTrialForTesting(
      std::move(weight_by_id), low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));

  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(
      ntp_tiles::kNewTabPageFieldTrial.name));
  EXPECT_FALSE(base::FeatureList::IsEnabled(ntp_tiles::kNewTabPageFieldTrial));

  ntp_tiles::NewTabPageFieldTrialExperimentBehavior experiment_type =
      ntp_tiles::GetNewTabPageFieldTrialExperimentType();

  EXPECT_EQ(experiment_type,
            ntp_tiles::NewTabPageFieldTrialExperimentBehavior::kDefault);
}

// Tests the tile ablation hiding all tiles apps group.
TEST_F(NewTabPageFieldTrialTest, TestTileAblationHideAllGroup) {
  auto feature_list = std::make_unique<base::FeatureList>();

  std::map<variations::VariationID, int> weight_by_id = {
      {field_trial_constants::kTileAblationHideAllID, 100}};

  new_tab_page_field_trial::CreateNewTabPageFieldTrialForTesting(
      std::move(weight_by_id), low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(
      ntp_tiles::kNewTabPageFieldTrial.name));
  EXPECT_TRUE(base::FeatureList::IsEnabled(ntp_tiles::kNewTabPageFieldTrial));

  ntp_tiles::NewTabPageFieldTrialExperimentBehavior experiment_type =
      ntp_tiles::GetNewTabPageFieldTrialExperimentType();

  EXPECT_EQ(
      experiment_type,
      ntp_tiles::NewTabPageFieldTrialExperimentBehavior::kTileAblationHideAll);
}

// Tests the tile ablation hiding only MVTs group.
TEST_F(NewTabPageFieldTrialTest, TestTileAblationHideOnlyMVTGroup) {
  auto feature_list = std::make_unique<base::FeatureList>();

  std::map<variations::VariationID, int> weight_by_id = {
      {field_trial_constants::kTileAblationHideOnlyMVTID, 100}};

  new_tab_page_field_trial::CreateNewTabPageFieldTrialForTesting(
      std::move(weight_by_id), low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(
      ntp_tiles::kNewTabPageFieldTrial.name));
  EXPECT_TRUE(base::FeatureList::IsEnabled(ntp_tiles::kNewTabPageFieldTrial));

  ntp_tiles::NewTabPageFieldTrialExperimentBehavior experiment_type =
      ntp_tiles::GetNewTabPageFieldTrialExperimentType();

  EXPECT_EQ(experiment_type, ntp_tiles::NewTabPageFieldTrialExperimentBehavior::
                                 kTileAblationHideMVTOnly);
}
