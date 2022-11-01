// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/variations_app_state_agent.h"

#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "components/variations/pref_names.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
NSString* kLastVariationsSeedFetchTimeKey = @"kLastVariationsSeedFetchTime";

// Simulate the existance for an unexpired seed from previous run.
void SimulateUnexpiredSeed() {
  [[NSUserDefaults standardUserDefaults]
      setDouble:base::Time::NowFromSystemTime().ToDoubleT()
         forKey:kLastVariationsSeedFetchTimeKey];
}

// Simulate the existance for an expired seed from previous run.
void SimulateExpiredSeed() {
  // Set the offset past zero so that it's not treated as a null value.
  base::Time distantPast = base::Time::UnixEpoch() + base::Days(1);
  [[NSUserDefaults standardUserDefaults]
      setDouble:distantPast.ToDoubleT()
         forKey:kLastVariationsSeedFetchTimeKey];
}

}  // namespace

// TODO(crbug.com/1372180): Expose delegate methods.

// Test class for VariationsAppStateAgent that overrides
// `shouldFetchVariationsSeed` and exposes methods that implement
// IOSChromeFirstRunVariationsSeedManagerDelegate.
@interface VariationsAppStateAgentForTesting : VariationsAppStateAgent
@end

@implementation VariationsAppStateAgentForTesting

// TODO(crbug.com/1372180): rewrite and/or remove once the original method is
// re-implemented.
- (BOOL)shouldTurnOnFeature {
  return YES;
}

@end

// Unit tests for VariationsAppStateAgent.
class VariationsAppStateAgentTest : public PlatformTest {
 protected:
  VariationsAppStateAgentTest() {
    mock_app_state_ = OCMClassMock([AppState class]);
    scene_state_ = [[SceneState alloc] initWithAppState:mock_app_state_];
  }

  ~VariationsAppStateAgentTest() override {
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:kLastVariationsSeedFetchTimeKey];
    [mock_app_state_ stopMocking];
    local_state_.Get()->ClearPref(variations::prefs::kVariationsLastFetchTime);
  }

  InitStage GetPreviousStage(InitStage currentStage) {
    return static_cast<InitStage>(currentStage - 1);
  }

  id mock_app_state_;
  SceneState* scene_state_;
  base::test::TaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
};

// Tests that on first run, the agent transitions to the next stage from
// InitStageVariationsSeed after seed is fetched, and that the metric for first
// run would be logged.
TEST_F(VariationsAppStateAgentTest, EnableSeedFetchOnFirstRun) {
  // Set up app init stage to be tested and agent and set expectations.
  OCMStub([mock_app_state_ initStage]).andReturn(InitStageVariationsSeed);
  OCMStub([mock_app_state_ queueTransitionToNextInitStage])
      .andDo(^(NSInvocation* inv) {
        FAIL() << "Should not transition to next init stage since the seed "
                  "fetching is not completed";
      });
  //  Execute.
  VariationsAppStateAgent* agent =
      [[VariationsAppStateAgentForTesting alloc] init];
  [agent setAppState:mock_app_state_];
  [agent appState:mock_app_state_
      didTransitionFromInitStage:GetPreviousStage(InitStageVariationsSeed)];
  EXPECT_OCMOCK_VERIFY(mock_app_state_);
  // TODO(crbug.com/1380164): Test that first run metric is logged.
}

// Tests that the agent immediately transitions to the next stage from
// InitStageVariationsSeed when the seed should not be fetched, and that the
// metric for unexpired seed would be logged.
TEST_F(VariationsAppStateAgentTest,
       DisableSeedFetchOnNonFirstRunWithUnexpiredSeed) {
  // Set up app init stage to be tested and agent and set expectations.
  OCMStub([mock_app_state_ initStage]).andReturn(InitStageVariationsSeed);
  OCMExpect([mock_app_state_ queueTransitionToNextInitStage]);
  //  Execute.
  SimulateUnexpiredSeed();
  VariationsAppStateAgent* agent =
      [[VariationsAppStateAgentForTesting alloc] init];
  [agent setAppState:mock_app_state_];
  [agent appState:mock_app_state_
      didTransitionFromInitStage:GetPreviousStage(InitStageVariationsSeed)];
  EXPECT_OCMOCK_VERIFY(mock_app_state_);
  // TODO(crbug.com/1380164): Test that unexpired seed metric is logged.
}

// Tests that the agent immediately transitions to the next stage from
// InitStageVariationsSeed when the seed should not be fetched, and that the
// metric for expired seed would be logged.
TEST_F(VariationsAppStateAgentTest,
       DisableSeedFetchOnNonFirstRunWithExpiredSeed) {
  // Set up app init stage to be tested and agent and set expectations.
  OCMStub([mock_app_state_ initStage]).andReturn(InitStageVariationsSeed);
  OCMExpect([mock_app_state_ queueTransitionToNextInitStage]);
  //  Execute.
  SimulateExpiredSeed();
  VariationsAppStateAgent* agent =
      [[VariationsAppStateAgentForTesting alloc] init];
  [agent setAppState:mock_app_state_];
  [agent appState:mock_app_state_
      didTransitionFromInitStage:GetPreviousStage(InitStageVariationsSeed)];
  EXPECT_OCMOCK_VERIFY(mock_app_state_);
  // TODO(crbug.com/1380164): Test that expired seed metric is logged.
}

// Tests that the fetch time from last launch will be saved when the app goes to
// background.
TEST_F(VariationsAppStateAgentTest, SavesLastSeedFetchTimeOnBackgrounding) {
  base::Time lastFetchTime = base::Time::Now();
  // Set up app init stage to be tested and agent and set expectations.
  OCMStub([mock_app_state_ initStage]).andReturn(InitStageBrowserObjectsForUI);
  VariationsAppStateAgent* agent =
      [[VariationsAppStateAgentForTesting alloc] init];
  [agent setAppState:mock_app_state_];
  local_state_.Get()->SetTime(variations::prefs::kVariationsLastFetchTime,
                              lastFetchTime);
  //  Simulate backgrounding and launch again.
  [agent sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelBackground];
  [mock_app_state_ stopMocking];
  OCMStub([mock_app_state_ initStage]).andReturn(InitStageVariationsSeed);
  agent = [[VariationsAppStateAgentForTesting alloc] init];
  [agent setAppState:mock_app_state_];
  [agent appState:mock_app_state_
      didTransitionFromInitStage:GetPreviousStage(InitStageVariationsSeed)];
  EXPECT_OCMOCK_VERIFY(mock_app_state_);
}
