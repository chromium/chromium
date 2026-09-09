// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_task/features.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/app/background_mode_buildflags.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using BackgroundContinuedProcessingFeaturesTest = PlatformTest;

// Tests that Background Continued Processing is enabled by default (on iOS 26+
// with the buildflag set) and can be disabled via the killswitch.
TEST_F(BackgroundContinuedProcessingFeaturesTest, TestKillswitch) {
#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
  if (@available(iOS 26.0, *)) {
    EXPECT_TRUE(IsBackgroundContinuedProcessingEnabled());
  } else {
    EXPECT_FALSE(IsBackgroundContinuedProcessingEnabled());
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kEnableBackgroundContinuedProcessing);
  EXPECT_FALSE(IsBackgroundContinuedProcessingEnabled());
#else
  EXPECT_FALSE(IsBackgroundContinuedProcessingEnabled());
#endif
}
