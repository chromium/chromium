// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/variations_app_state_agent.h"

#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "components/variations/pref_names.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
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
  // Simulate first run.
  id first_run_startup = OCMProtocolMock(@protocol(StartupInformation));
  OCMStub([first_run_startup isFirstRun]).andReturn(YES);
  OCMStub([mock_app_state_ startupInformation]).andReturn(first_run_startup);
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
}

// Tests that the agent immediately transitions to the next stage from
// InitStageVariationsSeed when app is not in first run.
TEST_F(VariationsAppStateAgentTest, DisableSeedFetchOnNonFirstRun) {
  // Set up app init stage to be tested and agent and set expectations.
  OCMStub([mock_app_state_ initStage]).andReturn(InitStageVariationsSeed);
  OCMExpect([mock_app_state_ queueTransitionToNextInitStage]);
  //  Execute.
  VariationsAppStateAgent* agent =
      [[VariationsAppStateAgentForTesting alloc] init];
  [agent setAppState:mock_app_state_];
  [agent appState:mock_app_state_
      didTransitionFromInitStage:GetPreviousStage(InitStageVariationsSeed)];
  EXPECT_OCMOCK_VERIFY(mock_app_state_);
}

// Tests that the fetch time from last launch will be saved when the app goes to
// background.
TEST_F(VariationsAppStateAgentTest, SavesLastSeedFetchTimeOnBackgrounding) {
  base::Time last_fetch_time = base::Time::Now();
  // Set up app init stage to be tested and agent and set expectations.
  OCMStub([mock_app_state_ initStage]).andReturn(InitStageBrowserObjectsForUI);
  VariationsAppStateAgent* agent =
      [[VariationsAppStateAgentForTesting alloc] init];
  [agent setAppState:mock_app_state_];
  [agent sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundInactive];
  local_state_.Get()->SetTime(variations::prefs::kVariationsLastFetchTime,
                              last_fetch_time);
  //  Simulate backgrounding and launch again.
  [agent sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelBackground];
  [mock_app_state_ stopMocking];
  OCMStub([mock_app_state_ initStage]).andReturn(InitStageVariationsSeed);
  agent = [[VariationsAppStateAgentForTesting alloc] init];
  double stored_value = [[NSUserDefaults standardUserDefaults]
      doubleForKey:kLastVariationsSeedFetchTimeKey];
  EXPECT_EQ(base::Time::FromDoubleT(stored_value), last_fetch_time);
  // TODO(crbug.com/1380164): Test freshness logging.
}

// TODO(crbug.com/1380164): Test freshness logging.
