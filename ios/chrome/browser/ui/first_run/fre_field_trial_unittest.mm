// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/fre_field_trial.h"

#import "base/feature_list.h"
#import "base/metrics/field_trial.h"
#import "base/metrics/field_trial_param_associator.h"
#import "base/test/mock_entropy_provider.h"
#import "base/test/scoped_feature_list.h"
#import "components/variations/variations_associated_data.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace fre_field_trial {
namespace {

using ::base::FeatureList;
using ::base::FieldTrialList;
using ::fre_field_trial::testing::
    CreateNewMICeAndDefaultBrowserFRETrialForTesting;
using ::variations::VariationID;

// Experiment IDs defined for the above field trial groups.
const VariationID kControlTrialID = 3348210;
const VariationID kHoldbackTrialID = 3348217;
const VariationID kFREDefaultBrowserPromoAtFirstRunOnlyID = 3348842;
const VariationID kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosID =
    3348843;
const VariationID kFREDefaultBrowserAndSmallDelayBeforeOtherPromosID = 3348844;
const VariationID kNewMICEFREWithUMADialogSetID = 3348845;
const VariationID kNewMICEFREWithThreeStepsSetID = 3348846;
const VariationID kNewMICEFREWithTwoStepsSetID = 3348847;

}  // namespace

// Tests for field trial creation on FRE for the current experiment. Each test
// case tests one experiment arm.
class FREFieldTrialTest : public PlatformTest {
 protected:
  void SetUp() override {
    weight_by_id_ = {{kControlTrialID, 0},
                     {kHoldbackTrialID, 0},
                     {kNewMICEFREWithUMADialogSetID, 0},
                     {kNewMICEFREWithThreeStepsSetID, 0},
                     {kNewMICEFREWithTwoStepsSetID, 0},
                     {kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosID, 0},
                     {kFREDefaultBrowserAndSmallDelayBeforeOtherPromosID, 0},
                     {kFREDefaultBrowserPromoAtFirstRunOnlyID, 0}};
  }

  void TearDown() override {
    scoped_feature_list_.Reset();
    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::map<variations::VariationID, int> weight_by_id_;
  base::MockEntropyProvider low_entropy_provider_;
};

// Tests default FRE field trial.
TEST_F(FREFieldTrialTest, TestFREDefault) {
  // Create the FRE trial with an empty feature list.
  auto feature_list = std::make_unique<FeatureList>();
  // To force default behavior, all groups have a probability of 0.
  CreateNewMICeAndDefaultBrowserFRETrialForTesting(
      weight_by_id_, low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(
      FieldTrialList::IsTrialActive(kIOSMICeAndDefaultBrowserTrialName));
  EXPECT_TRUE(FeatureList::IsEnabled(kEnableFREUIModuleIOS));
  EXPECT_EQ(NewDefaultBrowserPromoFRE::kDisabled,
            GetFREDefaultBrowserScreenPromoFRE());
  EXPECT_EQ(NewMobileIdentityConsistencyFRE::kOld,
            GetNewMobileIdentityConsistencyFRE());
}

// Tests control FRE field trial.
TEST_F(FREFieldTrialTest, TestFREControl) {
  // Create the FRE trial with an empty feature list.
  auto feature_list = std::make_unique<FeatureList>();
  weight_by_id_[kControlTrialID] = 100;
  CreateNewMICeAndDefaultBrowserFRETrialForTesting(
      weight_by_id_, low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(
      FieldTrialList::IsTrialActive(kIOSMICeAndDefaultBrowserTrialName));
  EXPECT_TRUE(FeatureList::IsEnabled(kEnableFREUIModuleIOS));
  EXPECT_EQ(NewDefaultBrowserPromoFRE::kDisabled,
            GetFREDefaultBrowserScreenPromoFRE());
  EXPECT_EQ(NewMobileIdentityConsistencyFRE::kOld,
            GetNewMobileIdentityConsistencyFRE());
}

// Tests holdback FRE field trial.
TEST_F(FREFieldTrialTest, TestFREHoldback) {
  // Create the FRE trial with an empty feature list.
  auto feature_list = std::make_unique<FeatureList>();
  weight_by_id_[kHoldbackTrialID] = 100;
  CreateNewMICeAndDefaultBrowserFRETrialForTesting(
      weight_by_id_, low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(
      FieldTrialList::IsTrialActive(kIOSMICeAndDefaultBrowserTrialName));
  EXPECT_FALSE(FeatureList::IsEnabled(kEnableFREUIModuleIOS));
  EXPECT_EQ(NewDefaultBrowserPromoFRE::kDisabled,
            GetFREDefaultBrowserScreenPromoFRE());
}

// Tests default browser promo (first run) FRE field trial.
TEST_F(FREFieldTrialTest, TestFREDefaultBrowserPromoFirstRun) {
  // Create the FRE trial with an empty feature list.
  auto feature_list = std::make_unique<FeatureList>();
  weight_by_id_[kFREDefaultBrowserPromoAtFirstRunOnlyID] = 100;
  CreateNewMICeAndDefaultBrowserFRETrialForTesting(
      weight_by_id_, low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(
      FieldTrialList::IsTrialActive(kIOSMICeAndDefaultBrowserTrialName));
  EXPECT_TRUE(FeatureList::IsEnabled(kEnableFREUIModuleIOS));
  EXPECT_EQ(NewDefaultBrowserPromoFRE::kFirstRunOnly,
            GetFREDefaultBrowserScreenPromoFRE());
  EXPECT_EQ(NewMobileIdentityConsistencyFRE::kOld,
            GetNewMobileIdentityConsistencyFRE());
}

// Tests default browser promo (default delay) FRE field trial.
TEST_F(FREFieldTrialTest, TestFREDefaultBrowserPromoDefaultDelay) {
  // Create the FRE trial with an empty feature list.
  auto feature_list = std::make_unique<FeatureList>();
  weight_by_id_[kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosID] = 100;
  CreateNewMICeAndDefaultBrowserFRETrialForTesting(
      weight_by_id_, low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(
      FieldTrialList::IsTrialActive(kIOSMICeAndDefaultBrowserTrialName));
  EXPECT_TRUE(FeatureList::IsEnabled(kEnableFREUIModuleIOS));
  EXPECT_EQ(NewDefaultBrowserPromoFRE::kDefaultDelay,
            GetFREDefaultBrowserScreenPromoFRE());
  EXPECT_EQ(NewMobileIdentityConsistencyFRE::kOld,
            GetNewMobileIdentityConsistencyFRE());
}

// Tests default browser promo (short delay) FRE field trial.
TEST_F(FREFieldTrialTest, TestFREDefaultBrowserPromoSmallDelay) {
  // Create the FRE trial with an empty feature list.
  auto feature_list = std::make_unique<FeatureList>();
  weight_by_id_[kFREDefaultBrowserAndSmallDelayBeforeOtherPromosID] = 100;
  CreateNewMICeAndDefaultBrowserFRETrialForTesting(
      weight_by_id_, low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(
      FieldTrialList::IsTrialActive(kIOSMICeAndDefaultBrowserTrialName));
  EXPECT_TRUE(FeatureList::IsEnabled(kEnableFREUIModuleIOS));
  EXPECT_EQ(NewDefaultBrowserPromoFRE::kShortDelay,
            GetFREDefaultBrowserScreenPromoFRE());
  EXPECT_EQ(NewMobileIdentityConsistencyFRE::kOld,
            GetNewMobileIdentityConsistencyFRE());
}

// Tests MICe with UMA dialog FRE field trial.
TEST_F(FREFieldTrialTest, TestFREUMADialogMICe) {
  // Create the FRE trial with an empty feature list.
  auto feature_list = std::make_unique<FeatureList>();
  weight_by_id_[kNewMICEFREWithUMADialogSetID] = 100;
  CreateNewMICeAndDefaultBrowserFRETrialForTesting(
      weight_by_id_, low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(
      FieldTrialList::IsTrialActive(kIOSMICeAndDefaultBrowserTrialName));
  EXPECT_TRUE(FeatureList::IsEnabled(kEnableFREUIModuleIOS));
  EXPECT_EQ(NewDefaultBrowserPromoFRE::kDisabled,
            GetFREDefaultBrowserScreenPromoFRE());
  EXPECT_EQ(NewMobileIdentityConsistencyFRE::kUMADialog,
            GetNewMobileIdentityConsistencyFRE());
}

// Tests MICe with three steps FRE field trial.
TEST_F(FREFieldTrialTest, TestFREThreeStepMICe) {
  // Create the FRE trial with an empty feature list.
  auto feature_list = std::make_unique<FeatureList>();
  weight_by_id_[kNewMICEFREWithThreeStepsSetID] = 100;
  CreateNewMICeAndDefaultBrowserFRETrialForTesting(
      weight_by_id_, low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(
      FieldTrialList::IsTrialActive(kIOSMICeAndDefaultBrowserTrialName));
  EXPECT_TRUE(FeatureList::IsEnabled(kEnableFREUIModuleIOS));
  EXPECT_EQ(NewDefaultBrowserPromoFRE::kDisabled,
            GetFREDefaultBrowserScreenPromoFRE());
  EXPECT_EQ(NewMobileIdentityConsistencyFRE::kThreeSteps,
            GetNewMobileIdentityConsistencyFRE());
}

// Tests MICe with two steps FRE field trial.
TEST_F(FREFieldTrialTest, TestFRETwoStepMICe) {
  // Create the FRE trial with an empty feature list.
  auto feature_list = std::make_unique<FeatureList>();
  weight_by_id_[kNewMICEFREWithTwoStepsSetID] = 100;
  CreateNewMICeAndDefaultBrowserFRETrialForTesting(
      weight_by_id_, low_entropy_provider_, feature_list.get());

  // Substitute the existing feature list with the one with field trial
  // configurations we are testing, and check assertions.
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  ASSERT_TRUE(
      FieldTrialList::IsTrialActive(kIOSMICeAndDefaultBrowserTrialName));
  EXPECT_TRUE(FeatureList::IsEnabled(kEnableFREUIModuleIOS));
  EXPECT_EQ(NewDefaultBrowserPromoFRE::kDisabled,
            GetFREDefaultBrowserScreenPromoFRE());
  EXPECT_EQ(NewMobileIdentityConsistencyFRE::kTwoSteps,
            GetNewMobileIdentityConsistencyFRE());
}

}  // namespace fre_field_trial
