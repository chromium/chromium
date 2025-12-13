// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/first_run_profile_agent.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/app/profile/first_run_profile_agent+Testing.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/guided_tour_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/welcome_back/model/features.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Tests the FirstRunProfileAgent.
class FirstRunProfileAgentTest : public PlatformTest {
 public:
  explicit FirstRunProfileAgentTest() {
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();

    profile_state_ = [[ProfileState alloc] initWithAppState:nil];
    profile_state_.profile = profile_.get();

    profile_agent_ = [[FirstRunProfileAgent alloc] init];
    [profile_state_ addAgent:profile_agent_];
  }

  ~FirstRunProfileAgentTest() override { profile_state_.profile = nullptr; }

 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList enabled_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  FirstRunProfileAgent* profile_agent_;
  ProfileState* profile_state_;
};

// Validates that the correct metric is logged when the user rejects Guided Tour
// promo.
TEST_F(FirstRunProfileAgentTest, GuidedTourPromoMetrics) {
  enabled_feature_list_.InitAndEnableFeatureWithParameters(
      kBestOfAppFRE, {{kWelcomeBackParam, "4"}});
  base::HistogramTester tester;
  [profile_agent_ dismissGuidedTourPromo];
  tester.ExpectTotalCount("IOS.GuidedTour.Promo.DidAccept", 1);
}

// Validates that the correct metric is logged when a step in the Guided Tour
// finishes.
TEST_F(FirstRunProfileAgentTest, GuidedTourStepMetrics) {
  enabled_feature_list_.InitAndEnableFeatureWithParameters(
      kBestOfAppFRE, {{kWelcomeBackParam, "4"}});
  base::HistogramTester tester;
  [profile_agent_ nextTappedForStep:GuidedTourStep::kTabGridIncognito];
  tester.ExpectBucketCount("IOS.GuidedTour.DidFinishStep", 1, 1);
}
