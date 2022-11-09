// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/variations_app_state_agent.h"
#import "ios/chrome/app/variations_app_state_agent+testing.h"

#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "components/variations/pref_names.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/prefs/browser_prefs.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/variations/ios_chrome_variations_seed_fetcher.h"
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

// Helper object that manages initStage transitions for the mock app state used
// in VariationsAppStateAgentTest.
@interface StateForMockAppState : NSObject

@property(nonatomic, assign) InitStage initStage;
@property(nonatomic, assign) BOOL transitionQueuedFromVariationsStage;

@end

@implementation StateForMockAppState
@end

// Unit tests for VariationsAppStateAgent.
class VariationsAppStateAgentTest : public PlatformTest {
 protected:
  VariationsAppStateAgentTest() {
    // Mocks the app state such that the mock_app_state_.initStage always
    // returns state_.initStage, and `[mock_app_state_
    // queueTransitionToNextInitStage]` sets
    // state_.transitionQueuedFromVariationsStage to YES.
    state_ = [[StateForMockAppState alloc] init];
    mock_app_state_ = OCMClassMock([AppState class]);
    // Use a local variable to prevent capturing `this` in  member field).
    StateForMockAppState* state = state_;
    OCMStub([mock_app_state_ initStage]).andDo(^(NSInvocation* inv) {
      InitStage initStage = state.initStage;
      [inv setReturnValue:&initStage];
    });
    OCMStub([mock_app_state_ queueTransitionToNextInitStage])
        .andDo(^(NSInvocation* inv) {
          state.transitionQueuedFromVariationsStage = YES;
        });
    // Mocks the fetcher.
    mock_fetcher_ =
        OCMPartialMock([[IOSChromeVariationsSeedFetcher alloc] init]);
    OCMStub([mock_fetcher_ startSeedFetch]);
    // Set up scene state from the mock app state.
    scene_state_ = [[SceneState alloc] initWithAppState:mock_app_state_];
  }

  ~VariationsAppStateAgentTest() override {
    @autoreleasepool {
      [[NSUserDefaults standardUserDefaults]
          removeObjectForKey:kLastVariationsSeedFetchTimeKey];
      mock_fetcher_ = nil;
      [mock_app_state_ stopMocking];
      mock_app_state_ = nil;
      local_state_.Get()->ClearPref(
          variations::prefs::kVariationsLastFetchTime);
    }
  }

  // Create the variations services agent for testing.
  VariationsAppStateAgent* CreateAgent(bool first_run) {
    state_.initStage = InitStageStart;
    state_.transitionQueuedFromVariationsStage = NO;
    VariationsAppStateAgent* agent =
        [[VariationsAppStateAgent alloc] initWithFirstRunStatus:first_run
                                                        fetcher:mock_fetcher_
                                                 featureEnabled:YES];
    [agent setAppState:mock_app_state_];
    return agent;
  }

  // Simulate that the fetcher has completed fetching.
  void SimulateFetchCompletion(VariationsAppStateAgent* agent) {
    [mock_fetcher_.delegate didFetchSeedSuccess:NO];
  }

  // Setter of the current stage of the mock app state. This also invokes
  // AppStateObserver methods `appState:willTransitionToInitStage:` and
  // `appState:didTransitionFromInitStage:`.
  void TransitionAgentToStage(VariationsAppStateAgent* observer,
                              InitStage new_stage) {
    InitStage current_stage = state_.initStage;
    DCHECK_LE(current_stage, new_stage);
    InitStage previous_stage;
    while (current_stage < new_stage) {
      bool transition_queued_from_variations_stage =
          IsAppStateQueueTransitionToNextInitStageInvoked();
      if (current_stage < InitStageVariationsSeed) {
        // If the seed should be fetched for `agent`, please make sure
        // `SimulateFetchCompletion(agent)` prior to calling this method.
        ASSERT_FALSE(transition_queued_from_variations_stage);
      } else {
        ASSERT_TRUE(transition_queued_from_variations_stage);
      }
      previous_stage = current_stage;
      current_stage = static_cast<InitStage>(current_stage + 1);
      state_.initStage = current_stage;
      [observer appState:mock_app_state_
          didTransitionFromInitStage:previous_stage];
    }
  }

  // Whether the app state has attempted to transition to the next stage.
  BOOL IsAppStateQueueTransitionToNextInitStageInvoked() {
    return state_.transitionQueuedFromVariationsStage;
  }

  // Gets the current scene state to simulate activation level transitions.
  SceneState* GetSceneState() { return scene_state_; }

  // Test PrefService dependencies.
  base::test::TaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;

  // VariationsAppStateAgent dependencies.
  IOSChromeVariationsSeedFetcher* mock_fetcher_;
  SceneState* scene_state_;
  id mock_app_state_;
  StateForMockAppState* state_;
};

#pragma mark - Test cases

// Tests that on first run, the agent transitions to the next stage from
// InitStageVariationsSeed after seed is fetched, and that the metric for first
// run would be logged.
TEST_F(VariationsAppStateAgentTest, EnableSeedFetchOnFirstRun) {
  // Start the agent.
  VariationsAppStateAgent* agent = CreateAgent(/*first_run=*/true);
  TransitionAgentToStage(agent, InitStageVariationsSeed);
  // Verify that the app agent would NOT transitioned to the next init stage if
  // the seed fetch hasn't completed.
  EXPECT_FALSE(IsAppStateQueueTransitionToNextInitStageInvoked());
  // Verify that the app agent would transition to the next init stage when the
  // seed fetch has completed.
  SimulateFetchCompletion(agent);
  EXPECT_TRUE(IsAppStateQueueTransitionToNextInitStageInvoked());
  // TODO(crbug.com/1380164): Test that first run metric is logged.
}

// Tests that the agent immediately transitions to the next stage from
// InitStageVariationsSeed when the user is not running the first time after
// installation.
TEST_F(VariationsAppStateAgentTest, DisableSeedFetchOnNonFirstRun) {
  // Start the agent.
  VariationsAppStateAgent* agent = CreateAgent(/*first_run=*/false);
  TransitionAgentToStage(agent, InitStageVariationsSeed);
  // Verify that the app agent would transitioned to the next init stage even if
  // the seed fetch hasn't completed.
  EXPECT_TRUE(IsAppStateQueueTransitionToNextInitStageInvoked());
  // TODO(crbug.com/1380164): Test that first run metric is logged.
}

// Tests that the agent immediately transitions to the next stage from
// InitStageVariationsSeed when the seed fetch has completed before then.
TEST_F(VariationsAppStateAgentTest,
       TransitionToNextStageIfSeedFetchedBeforeReachingVariationsStage) {
  VariationsAppStateAgent* agent = CreateAgent(/*first_run=*/true);
  // Simulate that the seed fetch has completed right before
  // InitStageVariationsSeed is reached.
  TransitionAgentToStage(agent,
                         static_cast<InitStage>(InitStageVariationsSeed - 1));
  SimulateFetchCompletion(agent);
  // Verify that even if the seed fetch has completed, the agent should not have
  // transitioned to the next stage because the app has not reached
  // InitStageVariationsSeed yet,
  EXPECT_FALSE(IsAppStateQueueTransitionToNextInitStageInvoked());
  // Verify that arriving at InitStageVariationsSeed would trigger a transition
  // to the next stage immediately.
  TransitionAgentToStage(agent, InitStageVariationsSeed);
  EXPECT_TRUE(IsAppStateQueueTransitionToNextInitStageInvoked());
}

// Tests that if the seed fetch does not complete before the scene transitions
// to foreground, the LaunchScreenViewController would be displayed.
TEST_F(VariationsAppStateAgentTest, LaunchScreenDisplaysIfSeedIsNotFetched) {
  // Sets expectation.
  id mock_scene_state = OCMPartialMock(GetSceneState());
  id mock_window = [OCMockObject mockForClass:[UIWindow class]];
  OCMExpect([mock_window
      setRootViewController:[OCMArg checkWithBlock:^BOOL(UIViewController* vc) {
        return vc.view.accessibilityIdentifier ==
               first_run::kLaunchScreenAccessibilityIdentifier;
      }]]);
  OCMExpect([mock_window makeKeyAndVisible]);
  OCMStub([mock_scene_state window]).andReturn(mock_window);
  // Starts an agent that fetches the seed.
  VariationsAppStateAgent* agent = CreateAgent(/*first_run=*/true);
  // Simulate that the seed fetch has completed right before
  // InitStageVariationsSeed is reached.
  TransitionAgentToStage(agent, InitStageVariationsSeed);
  [agent sceneState:mock_scene_state
      transitionedToActivationLevel:SceneActivationLevelForegroundInactive];
  EXPECT_OCMOCK_VERIFY(mock_window);
}

// Tests that the fetch time from last launch will be saved when the app goes to
// background.
TEST_F(VariationsAppStateAgentTest, SavesLastSeedFetchTimeOnBackgrounding) {
  InitStage stageAfterChromeInitialization =
      static_cast<InitStage>(InitStageBrowserObjectsForBackgroundHandlers + 1);
  // Simulate foregrounding and setting up Chrome.
  base::Time last_fetch_time = base::Time::Now();
  VariationsAppStateAgent* agent = CreateAgent(/*first_run=*/false);
  TransitionAgentToStage(agent, stageAfterChromeInitialization);
  [agent sceneState:GetSceneState()
      transitionedToActivationLevel:SceneActivationLevelForegroundInactive];
  local_state_.Get()->SetTime(variations::prefs::kVariationsLastFetchTime,
                              last_fetch_time);
  //  Simulate backgrounding and launch again.
  [agent sceneState:GetSceneState()
      transitionedToActivationLevel:SceneActivationLevelBackground];
  agent = CreateAgent(/*first_run=*/false);
  TransitionAgentToStage(agent, InitStageVariationsSeed);
  double stored_value = [[NSUserDefaults standardUserDefaults]
      doubleForKey:kLastVariationsSeedFetchTimeKey];
  EXPECT_EQ(base::Time::FromDoubleT(stored_value), last_fetch_time);
  // TODO(crbug.com/1380164): Test freshness logging.
}

// TODO(crbug.com/1380164): Test freshness logging.
