// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tile_ablation_field_trial.h"

#import "base/feature_list.h"
#import "base/metrics/field_trial.h"
#import "base/metrics/field_trial_param_associator.h"
#import "base/test/mock_entropy_provider.h"
#import "base/test/scoped_feature_list.h"
#import "components/variations/variations_associated_data.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/field_trial_constants.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests for field trial creation for the Trending Queries feature.
class TileAblationFieldTrialTest : public PlatformTest {
 protected:
  void TearDown() override {
    scoped_feature_list_.Reset();
    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::MockEntropyProvider low_entropy_provider_;
};

// Tests default field trial group.
TEST_F(TileAblationFieldTrialTest, TestDefault) {
  auto feature_list = std::make_unique<base::FeatureList>();

  std::map<variations::VariationID, int> weight_by_id = {};

  tile_ablation_field_trial::CreateTileAblationTrialForTesting(
      std::move(weight_by_id), low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));

  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(
      field_trial_constants::kTileAblationFieldTrialName));
  EXPECT_FALSE(IsTileAblationEnabled());

  TileAblationBehavior experiment_type = GetTileAblationBehavior();

  EXPECT_EQ(experiment_type, TileAblationBehavior::kDisabled);
}

// Tests default field trial group (i.e. the control group).
TEST_F(TileAblationFieldTrialTest, TestControl) {
  auto feature_list = std::make_unique<base::FeatureList>();

  std::map<variations::VariationID, int> weight_by_id = {
      {field_trial_constants::kTileAblationControlID, 100}};

  tile_ablation_field_trial::CreateTileAblationTrialForTesting(
      std::move(weight_by_id), low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));

  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(
      field_trial_constants::kTileAblationFieldTrialName));
  EXPECT_FALSE(IsTileAblationEnabled());

  TileAblationBehavior experiment_type = GetTileAblationBehavior();

  EXPECT_EQ(experiment_type, TileAblationBehavior::kDisabled);
}

// Tests field trial group where users are included in the hide only the MVTs
// group.
TEST_F(TileAblationFieldTrialTest, TileAblationMVTOnlyGroup) {
  auto feature_list = std::make_unique<base::FeatureList>();

  std::map<variations::VariationID, int> weight_by_id = {
      {field_trial_constants::kTileAblationMVTOnlyID, 100}};

  tile_ablation_field_trial::CreateTileAblationTrialForTesting(
      std::move(weight_by_id), low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(
      field_trial_constants::kTileAblationFieldTrialName));
  EXPECT_TRUE(IsTileAblationEnabled());

  TileAblationBehavior experiment_type = GetTileAblationBehavior();

  EXPECT_EQ(experiment_type, TileAblationBehavior::kTileAblationMVTOnly);
}

// Tests field trial group where users are included in the hide the MVTs and
// Shortcuts group.
TEST_F(TileAblationFieldTrialTest, TileAblationMVTAndShortcutsGroup) {
  auto feature_list = std::make_unique<base::FeatureList>();

  std::map<variations::VariationID, int> weight_by_id = {
      {field_trial_constants::kTileAblationMVTAndShortcutsID, 100}};

  tile_ablation_field_trial::CreateTileAblationTrialForTesting(
      std::move(weight_by_id), low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(
      field_trial_constants::kTileAblationFieldTrialName));
  EXPECT_TRUE(IsTileAblationEnabled());

  TileAblationBehavior experiment_type = GetTileAblationBehavior();

  EXPECT_EQ(experiment_type,
            TileAblationBehavior::kTileAblationMVTAndShortcuts);
}
