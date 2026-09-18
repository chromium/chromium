// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_classification/features.h"

#import "base/test/scoped_feature_list.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class PageClassificationFeaturesTest : public PlatformTest {};

// Tests that the default classification mode is kOnDeviceOnly when the feature
// is disabled.
TEST_F(PageClassificationFeaturesTest,
       DefaultClassificationModeIsOnDeviceOnly) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kPageClassification);

  EXPECT_EQ(GetPageClassificationMode(), PageClassificationMode::kOnDeviceOnly);
}

// Tests that the classification mode is parsed correctly when set to
// verticals_only.
TEST_F(PageClassificationFeaturesTest, ClassificationModeVerticalsOnly) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kPageClassification, {{kPageClassificationModeParam, "verticals_only"}});

  EXPECT_EQ(GetPageClassificationMode(),
            PageClassificationMode::kVerticalsOnly);
}

// Tests that the classification mode is parsed correctly when set to
// on_device_with_fallback.
TEST_F(PageClassificationFeaturesTest, ClassificationModeOnDeviceWithFallback) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kPageClassification,
      {{kPageClassificationModeParam, "on_device_with_fallback"}});

  EXPECT_EQ(GetPageClassificationMode(),
            PageClassificationMode::kOnDeviceWithVerticalsFallback);
}

// Tests the on-device classifier parameter parsing.
TEST_F(PageClassificationFeaturesTest, OnDeviceClassifierParam) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kPageClassification,
      {{kPageClassificationOnDeviceClassifierParam, "true"}});

  EXPECT_TRUE(IsPageClassificationOnDeviceClassifierEnabled());
}

// Tests the allow GPU execution parameter parsing.
TEST_F(PageClassificationFeaturesTest, AllowGpuExecutionParam) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kPageClassification,
      {{kPageClassificationAllowGpuExecutionParam, "true"}});

  EXPECT_TRUE(IsPageClassificationAllowGpuExecutionEnabled());
}

// Tests the title and URL only parameter default and override.
TEST_F(PageClassificationFeaturesTest, TitleAndUrlOnlyParam) {
  EXPECT_TRUE(IsPageClassificationTitleAndUrlOnlyEnabled());

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kPageClassification,
      {{kPageClassificationTitleAndUrlOnlyParam, "false"}});

  EXPECT_FALSE(IsPageClassificationTitleAndUrlOnlyEnabled());
}
