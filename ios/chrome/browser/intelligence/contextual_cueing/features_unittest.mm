// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/features.h"

#import "base/metrics/field_trial_params.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace contextual_cueing {

using ContextualCueingFeaturesTest = PlatformTest;

TEST_F(ContextualCueingFeaturesTest,
       IsGeminiContextualSuggestionsCuesOnDeviceClassifierEnabled) {
  base::FieldTrialParams params;
  params[kGeminiContextualSuggestionsCuesOnDeviceClassifierParam] = "true";

  // Disabled without PageActionMenu dependency.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kGeminiContextualSuggestionsCues, params);
    EXPECT_FALSE(IsGeminiContextualSuggestionsCuesOnDeviceClassifierEnabled());
  }

  // Enabled when PageActionMenu and feature with param are enabled.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeaturesAndParameters(
        {{kPageActionMenu, {}}, {kGeminiContextualSuggestionsCues, params}},
        {});
    EXPECT_TRUE(IsGeminiContextualSuggestionsCuesOnDeviceClassifierEnabled());
  }
}

TEST_F(ContextualCueingFeaturesTest,
       IsGeminiContextualSuggestionsCuesAllowGpuExecutionEnabled) {
  base::FieldTrialParams params;
  params[kGeminiContextualSuggestionsCuesAllowGpuExecutionParam] = "true";

  // Disabled without PageActionMenu dependency.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kGeminiContextualSuggestionsCues, params);
    EXPECT_FALSE(IsGeminiContextualSuggestionsCuesAllowGpuExecutionEnabled());
  }

  // Enabled when PageActionMenu and feature with param are enabled.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeaturesAndParameters(
        {{kPageActionMenu, {}}, {kGeminiContextualSuggestionsCues, params}},
        {});
    EXPECT_TRUE(IsGeminiContextualSuggestionsCuesAllowGpuExecutionEnabled());
  }
}

TEST_F(
    ContextualCueingFeaturesTest,
    IgnoreContextualCueingThresholdsForcesOnDeviceClassifierAndServerExecution) {
  base::FieldTrialParams params;
  params[kGeminiContextualSuggestionsCuesIgnoreThresholdsParam] = "true";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{kPageActionMenu, {}}, {kGeminiContextualSuggestionsCues, params}}, {});

  EXPECT_TRUE(IsIgnoreContextualCueingThresholdsEnabled());
  EXPECT_TRUE(IsGeminiContextualSuggestionsCuesEnabled());
  EXPECT_TRUE(IsGeminiContextualSuggestionsCuesOnDeviceClassifierEnabled());
  EXPECT_TRUE(IsGeminiContextualSuggestionsCuesServerModelExecutionEnabled());
}

}  // namespace contextual_cueing
