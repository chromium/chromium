// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/coordinator/first_run_post_action_provider.h"

#import "base/test/scoped_feature_list.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/safari_data_import/model/features.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_type.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class FirstRunPostActionProviderTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    // Register prefs needed by ShouldShowSafariDataImportEntryPoint if any.
    // Assuming it uses standard prefs or we can mock it via feature flags.
  }

  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  base::test::ScopedFeatureList feature_list_;
};

// Tests that Safari Import is skipped if the Guided Tour has started.
TEST_F(FirstRunPostActionProviderTest, SkipSafariImportIfTourStarted) {
  feature_list_.InitAndEnableFeatureWithParameters(kBestOfAppFRE,
                                                   {{"variant", "4"}});

  FirstRunPostActionProvider* provider = [[FirstRunPostActionProvider alloc]
      initWithPrefService:pref_service_.get()];

  // Advance to Guided Tour.
  BOOL foundGuidedTour = NO;
  ScreenType type = [provider nextScreenType];
  while (type != kStepsCompleted) {
    if (type == kGuidedTour) {
      foundGuidedTour = YES;
      break;
    }
    type = [provider nextScreenType];
  }

  ASSERT_TRUE(foundGuidedTour);

  // Mark tour as started.
  [provider setGuidedTourStarted:YES];

  // The next screen should NOT be kSafariImport.
  type = [provider nextScreenType];
  EXPECT_NE(type, kSafariImport);
  EXPECT_EQ(type, kStepsCompleted);
}

// Tests that Safari Import is NOT skipped if the Guided Tour has NOT started.
// TODO(crbug.com/481585370): Re-enable this test.
TEST_F(FirstRunPostActionProviderTest,
       DISABLED_ShowSafariImportIfTourNotStarted) {
  feature_list_.InitAndEnableFeatureWithParameters(kBestOfAppFRE,
                                                   {{"variant", "4"}});

  FirstRunPostActionProvider* provider = [[FirstRunPostActionProvider alloc]
      initWithPrefService:pref_service_.get()];

  BOOL foundGuidedTour = NO;
  ScreenType type = [provider nextScreenType];
  while (type != kStepsCompleted) {
    if (type == kGuidedTour) {
      foundGuidedTour = YES;
      break;
    }
    type = [provider nextScreenType];
  }

  ASSERT_TRUE(foundGuidedTour);

  // Do NOT mark tour as started.
  [provider setGuidedTourStarted:NO];

  // The next screen should be kSafariImport.
  type = [provider nextScreenType];
  EXPECT_EQ(type, kSafariImport);
}
