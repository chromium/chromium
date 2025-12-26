// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/features/features.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/intelligence/actuation/actuation_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using ActuationFeaturesTest = PlatformTest;

TEST_F(ActuationFeaturesTest, IsActuationEnabledDefault) {
  EXPECT_FALSE(IsActuationEnabled());
}

TEST_F(ActuationFeaturesTest, IsActionDisabled_Default) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeature(kActuationTools);
  // Arbitrarily select kClick to test that actions are enabled by default.
  EXPECT_FALSE(IsActionDisabled(optimization_guide::proto::Action::kClick));
}

TEST_F(ActuationFeaturesTest, IsActionDisabled_ActionDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;

  base::FieldTrialParams params;
  params["DisabledActions"] = "ClickAction,TypeAction";
  scoped_feature_list.InitAndEnableFeatureWithParameters(kActuationTools,
                                                         params);

  EXPECT_TRUE(IsActionDisabled(optimization_guide::proto::Action::kClick));
  EXPECT_TRUE(IsActionDisabled(optimization_guide::proto::Action::kType));
  // ScrollAction is NOT in the disabled list, so it should be enabled.
  EXPECT_FALSE(IsActionDisabled(optimization_guide::proto::Action::kScroll));
}

TEST_F(ActuationFeaturesTest, IsActionDisabled_ActionNotSet) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeature(kActuationTools);
  EXPECT_TRUE(
      IsActionDisabled(optimization_guide::proto::Action::ACTION_NOT_SET));
}
