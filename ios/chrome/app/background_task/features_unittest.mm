// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_task/features.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/app/background_mode_buildflags.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using BackgroundContinuedProcessingFeaturesTest = PlatformTest;

// Tests that background continued processing is disabled by default.
TEST_F(BackgroundContinuedProcessingFeaturesTest, DefaultDisabled) {
  EXPECT_FALSE(IsBackgroundContinuedProcessingEnabled());
}

// Tests that enabling `kEnableBackgroundContinuedProcessing` enables background
// continued processing on iOS 26+ when built with the buildflag.
TEST_F(BackgroundContinuedProcessingFeaturesTest, FeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kEnableBackgroundContinuedProcessing);
#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
  if (@available(iOS 26.0, *)) {
    EXPECT_TRUE(IsBackgroundContinuedProcessingEnabled());
  } else {
    EXPECT_FALSE(IsBackgroundContinuedProcessingEnabled());
  }
#else
  EXPECT_FALSE(IsBackgroundContinuedProcessingEnabled());
#endif
}
