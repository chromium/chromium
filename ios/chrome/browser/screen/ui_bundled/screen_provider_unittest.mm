// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/screen/ui_bundled/screen_provider.h"

#import <vector>

#import "ios/chrome/browser/screen/ui_bundled/screen_provider+protected.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_type.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using ScreenProviderTest = PlatformTest;

// Verifies that `userFacingScreens` excludes the trailing
// `ScreenType::kStepsCompleted` sentinel and does not advance the cursor.
TEST_F(ScreenProviderTest, UserFacingScreensExcludesStepsCompletedSentinel) {
  ScreenProvider* provider = [[ScreenProvider alloc]
      initWithScreens:{ScreenType::kSignIn, ScreenType::kHistorySync,
                       ScreenType::kDefaultBrowserPromo,
                       ScreenType::kStepsCompleted}];

  std::vector<ScreenType> expected = {ScreenType::kSignIn,
                                      ScreenType::kHistorySync,
                                      ScreenType::kDefaultBrowserPromo};
  EXPECT_EQ(provider.userFacingScreens, expected);
  EXPECT_EQ([provider nextScreenType], ScreenType::kSignIn);
}

// Verifies that `-setNextScreenType:` advances the cursor only when a
// user-facing screen is found at or after the current cursor position
// (including index 0 before advancing), and never rewinds to already-passed
// screens.
TEST_F(ScreenProviderTest, SetNextScreenTypeAdvancesCursorOnlyWhenFound) {
  ScreenProvider* provider = [[ScreenProvider alloc]
      initWithScreens:{ScreenType::kSignIn, ScreenType::kHistorySync,
                       ScreenType::kChoice, ScreenType::kDefaultBrowserPromo,
                       ScreenType::kLensInteractivePromo,
                       ScreenType::kStepsCompleted}];

  EXPECT_FALSE([provider setNextScreenType:ScreenType::kBestFeatures]);
  EXPECT_FALSE([provider setNextScreenType:ScreenType::kStepsCompleted]);
  EXPECT_TRUE([provider setNextScreenType:ScreenType::kSignIn]);
  EXPECT_EQ([provider nextScreenType], ScreenType::kSignIn);

  // Already-passed screens must not rewind the cursor.
  EXPECT_FALSE([provider setNextScreenType:ScreenType::kSignIn]);
  EXPECT_FALSE([provider setNextScreenType:ScreenType::kBestFeatures]);
  EXPECT_EQ([provider nextScreenType], ScreenType::kHistorySync);

  EXPECT_TRUE([provider setNextScreenType:ScreenType::kDefaultBrowserPromo]);
  EXPECT_FALSE([provider setNextScreenType:ScreenType::kChoice]);
  EXPECT_EQ([provider nextScreenType], ScreenType::kDefaultBrowserPromo);
  EXPECT_EQ([provider nextScreenType], ScreenType::kLensInteractivePromo);
  EXPECT_EQ([provider nextScreenType], ScreenType::kStepsCompleted);
}

}  // namespace
