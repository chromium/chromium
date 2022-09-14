// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/fre_field_trial.h"

#import "base/feature_list.h"
#import "base/metrics/field_trial.h"
#import "base/metrics/field_trial_param_associator.h"
#import "base/test/mock_entropy_provider.h"
#import "base/test/scoped_feature_list.h"
#import "components/variations/variations_associated_data.h"
#import "ios/chrome/browser/ui/first_run/field_trial_constants.h"
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

// Experiment IDs defined for field trial groups.
const VariationID kControlTrialID = 3348210;
const VariationID kFREDefaultBrowserPromoAtFirstRunOnlyID = 3348842;
const VariationID kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosID =
    3348843;
const VariationID kFREDefaultBrowserAndSmallDelayBeforeOtherPromosID = 3348844;

}  // namespace

// Tests for field trial creation on FRE for the current experiment. Each test
// case tests one experiment arm.
class FREFieldTrialTest : public PlatformTest {
 protected:
  void SetUp() override {
    weight_by_id_ = {{kControlTrialID, 0},
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
}

}  // namespace fre_field_trial
