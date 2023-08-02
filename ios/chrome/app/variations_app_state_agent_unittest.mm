// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/variations_app_state_agent.h"
#import "ios/chrome/app/variations_app_state_agent+testing.h"

#import "base/metrics/field_trial.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "components/variations/pref_names.h"
#import "components/variations/service/variations_field_trial_creator.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#import "ios/chrome/browser/variations/ios_chrome_variations_seed_fetcher.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

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
          removeObjectForKey:@"kLastVariationsSeedFetchTime"];
      state_ = nil;
      mock_fetcher_ = nil;
      [mock_app_state_ stopMocking];
      mock_app_state_ = nil;
      local_state_.Get()->ClearPref(
          variations::prefs::kVariationsLastFetchTime);
    }
  }

  // Create the variations services agent for testing.
  VariationsAppStateAgent* CreateAgent(bool fre,
                                       base::Time lastSeedFetchTime,
                                       int percentage_enabled,
                                       int percentage_control) {
    state_.initStage = InitStageStart;
    state_.transitionQueuedFromVariationsStage = NO;
    VariationsAppStateAgent* agent = [[VariationsAppStateAgent alloc]
        initWithFirstRunExperience:fre
                 lastSeedFetchTime:lastSeedFetchTime
                           fetcher:mock_fetcher_
                enabledGroupWeight:percentage_enabled
                controlGroupWeight:percentage_control];
    [agent setAppState:mock_app_state_];
    return agent;
  }

  // Create the variations services agent that will fetch the seed.
  VariationsAppStateAgent* CreateAgentThatFetches() {
    return CreateAgent(true, base::Time(), 100, 0);
  }

  // Create the variations services agent that will not fetch the seed.
  VariationsAppStateAgent* CreateAgentThatDoesNotFetch() {
    return CreateAgent(false, base::Time::NowFromSystemTime(), 0, 0);
  }

  // Simulate that the fetcher has completed fetching.
  void SimulateFetchCompletion(VariationsAppStateAgent* agent) {
    [mock_fetcher_.delegate
        variationsSeedFetcherDidCompleteFetchWithSuccess:NO];
  }

  // Setter of the current stage of the mock app state. This also invokes
  // AppStateObserver methods `appState:willTransitionToInitStage:` and
  // `appState:didTransitionFromInitStage:`.
  void TransitionAgentToStage(VariationsAppStateAgent* agent,
                              InitStage new_stage) {
    InitStage current_stage = state_.initStage;
    DCHECK_LE(current_stage, new_stage);
    InitStage previous_stage;
    InitStage next_stage;
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
      next_stage = static_cast<InitStage>(current_stage + 1);
      [agent appState:mock_app_state_ willTransitionToInitStage:next_stage];
      previous_stage = current_stage;
      current_stage = next_stage;
      state_.initStage = current_stage;
      [agent appState:mock_app_state_
          didTransitionFromInitStage:previous_stage];
    }
  }

  // Whether the app state agent has attempted to transition to the next stage.
  BOOL IsAppStateQueueTransitionToNextInitStageInvoked() {
    return state_.transitionQueuedFromVariationsStage;
  }

  // Verify that the trial is activated and assigned to group with `group_name`.
  void ExpectThatTrialIsActiveAndAssignedToGroup(std::string group_name) {
    ASSERT_TRUE(
        base::FieldTrialList::IsTrialActive(kIOSChromeVariationsTrialName));
    EXPECT_EQ(base::FieldTrialList::Find(kIOSChromeVariationsTrialName)
                  ->GetGroupNameWithoutActivation(),
              group_name);
  }

  // Verify that the expiry status is logged in UMA.
  void ExpectThatSeedExpiryMetricLogged(
      variations::VariationsSeedExpiry expiry) {
    histogram_tester_.ExpectUniqueSample(kIOSSeedExpiryHistogram, expiry, 1);
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
  base::HistogramTester histogram_tester_;
};

#pragma mark - Test cases

// Tests that on first run, the agent transitions to the next stage from
// InitStageVariationsSeed after seed is fetched, the field trial group
// "Enabled" would be active, and that the metric for non-existing previous seed
// would be logged.
TEST_F(VariationsAppStateAgentTest, EnableSeedFetchOnFirstRun) {
  // Start the agent.
  VariationsAppStateAgent* agent = CreateAgentThatFetches();
  ExpectThatSeedExpiryMetricLogged(
      variations::VariationsSeedExpiry::kFetchTimeMissing);
  TransitionAgentToStage(agent, InitStageVariationsSeed);
  // Verify that the app agent would NOT transitioned to the next init stage if
  // the seed fetch hasn't completed.
  EXPECT_FALSE(IsAppStateQueueTransitionToNextInitStageInvoked());
  // Verify that the app agent would transition to the next init stage when the
  // seed fetch has completed.
  SimulateFetchCompletion(agent);
  EXPECT_TRUE(IsAppStateQueueTransitionToNextInitStageInvoked());
  TransitionAgentToStage(
      agent,
      static_cast<InitStage>(InitStageBrowserObjectsForBackgroundHandlers + 1));
  ExpectThatTrialIsActiveAndAssignedToGroup(
      kIOSChromeVariationsTrialEnabledGroup);
}

// Tests that the agent immediately transitions to the next stage from
// InitStageVariationsSeed when the user is not running the first time after
// installation, even if placed in the enabled group.
// Also, test that the user is NOT assigned to any experiment group. This is to
// make sure that users who installed before the experiment is setup would not
// be enrolled.
TEST_F(VariationsAppStateAgentTest, DisableSeedFetchOnNonFirstRun) {
  // Start the agent.
  VariationsAppStateAgent* agent =
      CreateAgent(/*fre=*/false, /*lastSeedFetchTime=*/base::Time(),
                  /*percentage_enabled=*/100, /*percentage_control=*/0);
  ExpectThatSeedExpiryMetricLogged(
      variations::VariationsSeedExpiry::kFetchTimeMissing);
  TransitionAgentToStage(agent, InitStageVariationsSeed);
  // Verify that the app agent would transitioned to the next init stage even if
  // the seed fetch hasn't completed.
  EXPECT_TRUE(IsAppStateQueueTransitionToNextInitStageInvoked());
  EXPECT_FALSE(
      base::FieldTrialList::IsTrialActive(kIOSChromeVariationsTrialName));
}

// Tests that the agent immediately transitions to the next stage from
// InitStageVariationsSeed when the user is placed in "control" group even when
// the user is running first time after installation, and that the metric for
// non-existing previous seed would be logged.
TEST_F(VariationsAppStateAgentTest, DisableSeedFetchOnFirstRunInControlGroup) {
  // Start the agent that is should fetch the seed if placed in enabled group.
  VariationsAppStateAgent* agent =
      CreateAgent(/*fre=*/true, /*lastSeedFetchTime=*/base::Time(),
                  /*percentage_enabled=*/0, /*percentage_control=*/100);
  ExpectThatSeedExpiryMetricLogged(
      variations::VariationsSeedExpiry::kFetchTimeMissing);
  TransitionAgentToStage(agent, InitStageVariationsSeed);
  // Verify that the app agent would transitioned to the next init stage even if
  // the seed fetch hasn't completed.
  EXPECT_TRUE(IsAppStateQueueTransitionToNextInitStageInvoked());
  TransitionAgentToStage(
      agent,
      static_cast<InitStage>(InitStageBrowserObjectsForBackgroundHandlers + 1));
  ExpectThatTrialIsActiveAndAssignedToGroup(
      kIOSChromeVariationsTrialControlGroup);
}

// Tests that the agent immediately transitions to the next stage from
// InitStageVariationsSeed when the user is placed in "default" group even when
// the user is running first time after installation, and that the metric for
// non-existing previous seed would be logged.
TEST_F(VariationsAppStateAgentTest, DisableSeedFetchOnFirstRunInDefaultGroup) {
  // Start the agent that is should fetch the seed if placed in enabled group.
  VariationsAppStateAgent* agent =
      CreateAgent(/*fre=*/true, /*lastSeedFetchTime=*/base::Time(),
                  /*percentage_enabled=*/0, /*percentage_control=*/0);
  ExpectThatSeedExpiryMetricLogged(
      variations::VariationsSeedExpiry::kFetchTimeMissing);
  TransitionAgentToStage(agent, InitStageVariationsSeed);
  // Verify that the app agent would transitioned to the next init stage even if
  // the seed fetch hasn't completed.
  EXPECT_TRUE(IsAppStateQueueTransitionToNextInitStageInvoked());
  TransitionAgentToStage(
      agent,
      static_cast<InitStage>(InitStageBrowserObjectsForBackgroundHandlers + 1));
  ExpectThatTrialIsActiveAndAssignedToGroup(
      kIOSChromeVariationsTrialDefaultGroup);
}

// Tests that the agent immediately transitions to the next stage from
// InitStageVariationsSeed when the user exists the last FRE experience and
// relaunches, even when the group assignment is "Enabled".
TEST_F(VariationsAppStateAgentTest,
       DisableSeedFetchWhenUserExitsFREAndRelaunch) {
  // Start the agent.
  VariationsAppStateAgent* agent = CreateAgent(
      /*fre=*/true,
      /*lastSeedFetchTime=*/base::Time::NowFromSystemTime() - base::Days(1),
      /*percentage_enabled=*/100, /*percentage_control=*/0);
  ExpectThatSeedExpiryMetricLogged(
      variations::VariationsSeedExpiry::kNotExpired);
  TransitionAgentToStage(agent, InitStageVariationsSeed);
  // Verify that the app agent would transitioned to the next init stage even if
  // the seed fetch hasn't completed.
  EXPECT_TRUE(IsAppStateQueueTransitionToNextInitStageInvoked());
}

// Tests that the agent immediately transitions to the next stage from
// InitStageVariationsSeed when the seed fetch has completed before then.
TEST_F(VariationsAppStateAgentTest,
       TransitionToNextStageIfSeedFetchedBeforeReachingVariationsStage) {
  VariationsAppStateAgent* agent = CreateAgentThatFetches();
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

// Tests that the field trial group from last run ("Enabled") would be active
// for subsequent runs, even though the seed would not be fetched.
TEST_F(VariationsAppStateAgentTest, PreviousGroupAssignmentPersisted) {
  InitStage stageAfterChromeInitialization =
      static_cast<InitStage>(InitStageBrowserObjectsForBackgroundHandlers + 1);
  // Simulate first run.
  {
    VariationsAppStateAgent* first_agent =
        CreateAgent(/*fre=*/true, /*lastSeedFetchTime=*/base::Time(),
                    /*percentage_enabled=*/100, /*percentage_control=*/0);
    SimulateFetchCompletion(first_agent);
    TransitionAgentToStage(first_agent, stageAfterChromeInitialization);
  }
  // Simulate a second run; this time the seed would not be fetched, and
  // group assignment should NOT be recreated.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithEmptyFeatureAndFieldTrialLists();
    VariationsAppStateAgent* second_agent =
        CreateAgent(/*fre=*/false, /*lastSeedFetchTime=*/base::Time(),
                    /*percentage_enabled=*/0, /*percentage_control=*/0);
    TransitionAgentToStage(second_agent, stageAfterChromeInitialization);
    ExpectThatTrialIsActiveAndAssignedToGroup(
        kIOSChromeVariationsTrialEnabledGroup);
  }
  // Third run.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithEmptyFeatureAndFieldTrialLists();
    VariationsAppStateAgent* third_agent =
        CreateAgent(/*fre=*/false, /*lastSeedFetchTime=*/base::Time(),
                    /*percentage_enabled=*/0, /*percentage_control=*/0);
    TransitionAgentToStage(third_agent, stageAfterChromeInitialization);
    ExpectThatTrialIsActiveAndAssignedToGroup(
        kIOSChromeVariationsTrialEnabledGroup);
  }
}

// Tests that if the app presents first run a second time, experiment group
// should be re-assigned.
TEST_F(VariationsAppStateAgentTest, ReassignGroupOnSecondFirstRun) {
  InitStage stageAfterChromeInitialization =
      static_cast<InitStage>(InitStageBrowserObjectsForBackgroundHandlers + 1);
  // Simulate first run in control group.
  {
    VariationsAppStateAgent* control_agent =
        CreateAgent(/*fre=*/true, /*lastSeedFetchTime=*/base::Time(),
                    /*percentage_enabled=*/0, /*percentage_control=*/100);
    TransitionAgentToStage(control_agent, stageAfterChromeInitialization);
    ExpectThatTrialIsActiveAndAssignedToGroup(
        kIOSChromeVariationsTrialControlGroup);
  }
  // Simulate the scenario that the previous run hasn't finished and the app
  // starts again with first run experience. This time the experiment group
  // should be re-assigned.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithEmptyFeatureAndFieldTrialLists();
    VariationsAppStateAgent* enabled_agent_1 =
        CreateAgent(/*fre=*/true, /*lastSeedFetchTime=*/base::Time(),
                    /*percentage_enabled=*/100, /*percentage_control=*/0);
    SimulateFetchCompletion(enabled_agent_1);
    TransitionAgentToStage(enabled_agent_1, stageAfterChromeInitialization);
    ExpectThatTrialIsActiveAndAssignedToGroup(
        kIOSChromeVariationsTrialEnabledGroup);
  }
  // Start a subsequent session and check that the group assignment tallies with
  // the on given during the "second first run."
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithEmptyFeatureAndFieldTrialLists();
    VariationsAppStateAgent* enabled_agent_2 =
        CreateAgent(/*fre=*/false, /*lastSeedFetchTime=*/base::Time(),
                    /*percentage_enabled=*/100, /*percentage_control=*/0);
    TransitionAgentToStage(enabled_agent_2, stageAfterChromeInitialization);
    ExpectThatTrialIsActiveAndAssignedToGroup(
        kIOSChromeVariationsTrialEnabledGroup);
  }
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
  VariationsAppStateAgent* agent = CreateAgentThatFetches();
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
  VariationsAppStateAgent* agent = CreateAgentThatDoesNotFetch();
  TransitionAgentToStage(agent, stageAfterChromeInitialization);
  [agent sceneState:GetSceneState()
      transitionedToActivationLevel:SceneActivationLevelForegroundInactive];
  local_state_.Get()->SetTime(variations::prefs::kVariationsLastFetchTime,
                              last_fetch_time);
  //  Simulate backgrounding and launch again.
  [agent sceneState:GetSceneState()
      transitionedToActivationLevel:SceneActivationLevelBackground];
  agent = [[VariationsAppStateAgent alloc] init];
  histogram_tester_.ExpectUniqueSample(
      kIOSSeedExpiryHistogram, variations::VariationsSeedExpiry::kNotExpired,
      2);
}
