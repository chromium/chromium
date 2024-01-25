// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/model/idle/idle_service.h"

#import "base/memory/raw_ptr.h"
#import "base/test/gmock_callback_support.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/enterprise/idle/idle_features.h"
#import "components/enterprise/idle/idle_pref_names.h"
#import "components/enterprise/idle/metrics.h"
#import "ios/chrome/browser/enterprise/model/idle/action_runner.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using ::testing::_;

namespace enterprise_idle {

class ChromeBrowserState;
class ActionRunner;

// Tests that the idle service schedules tasks and runs actions as expected when
// the browser is starting up, re-foregrounded or already in foreground. Also
// tests that the actions run at the right time when the value of the
// IdleTimeout policy changes.
class IdleTimeoutServiceTest : public PlatformTest {
  // Mocks the `Run()` method which is used to check that actions run the
  // right time(s) in the tests.
  class MockActionRunner : public ActionRunner {
   public:
    MockActionRunner() {}
    MOCK_METHOD(void, Run, (ActionsCompletedCallback), (override));
    ~MockActionRunner() override {}
  };

 public:
  class MockObserver : public IdleService::Observer {
   public:
    MockObserver() {}
    ~MockObserver() override {}
    MOCK_METHOD(void, OnIdleTimeoutInForeground, (), (override));
    MOCK_METHOD(void, OnIdleTimeoutOnStartup, (), (override));
    MOCK_METHOD(void, OnIdleTimeoutActionsCompleted, (), (override));
    MOCK_METHOD(void, OnApplicationWillEnterBackground, (), (override));
  };

  IdleTimeoutServiceTest() = default;

  void SetIdleTimeoutPolicy(base::TimeDelta timeout) {
    browser_state_.get()->GetPrefs()->SetTimeDelta(prefs::kIdleTimeout,
                                                   timeout);
  }

  void SetLastActiveTime(base::Time time) {
    local_state_.Get()->SetTime(prefs::kLastActiveTimestamp, time);
  }

  base::Time GetLastIdleTime() {
    return browser_state_.get()->GetPrefs()->GetTime(prefs::kLastIdleTimestamp);
  }

  void InitIdleService() {
    idle_service_ = std::make_unique<IdleService>(
        browser_state_->GetOriginalChromeBrowserState());
    idle_service_->SetActionRunnerForTesting(
        base::WrapUnique(new MockActionRunner()));
    action_runner_ = static_cast<MockActionRunner*>(
        idle_service_->GetActionRunnerForTesting());
    idle_service_->AddObserver(&mock_observer_);
  }

  void SignIn() {
    // Sign in.
    FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);
    authentication_service_->SignIn(
        identity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  }

  void SetUp() override {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeatures({enterprise_idle::kIdleTimeout}, {});
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    browser_state_ = test_cbs_builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    authentication_service_ = static_cast<AuthenticationService*>(
        AuthenticationServiceFactory::GetForBrowserState(browser_state_.get()));
  }

  void TearDown() override {
    idle_service_->RemoveObserver(&mock_observer_);
    idle_service_->Shutdown();
    idle_service_.reset();
    browser_state_.reset();
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::Options::DEFAULT,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockObserver mock_observer_;
  raw_ptr<MockActionRunner> action_runner_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<IdleService> idle_service_;
  IOSChromeScopedTestingLocalState local_state_;
  raw_ptr<AuthenticationService> authentication_service_;
};

// Test that the observer methods are not called when the policy is not set, and
// that the idle check return false.
TEST_F(IdleTimeoutServiceTest, IdleTimeoutPolicyNotSet) {
  InitIdleService();
  // The pref value should not be positive if the policy is not set.
  ASSERT_FALSE(browser_state_->GetPrefs()
                   ->GetTimeDelta(enterprise_idle::prefs::kIdleTimeout)
                   .is_positive());
  EXPECT_CALL(*action_runner_, Run(_)).Times(0);
  EXPECT_CALL(mock_observer_, OnIdleTimeoutOnStartup()).Times(0);
  idle_service_->OnApplicationWillEnterForeground();
  EXPECT_CALL(mock_observer_, OnApplicationWillEnterBackground()).Times(0);
  idle_service_->OnApplicationWillEnterBackground();
  EXPECT_FALSE(idle_service_->IsIdleAfterPreviouslyBeingActive());
}

// When policy timeout is set after being unset.
TEST_F(IdleTimeoutServiceTest, IdleTimeoutPrefsSet_OnPolicySet) {
  InitIdleService();
  EXPECT_CALL(*action_runner_, Run(_)).Times(0);
  idle_service_->OnApplicationWillEnterForeground();
  task_environment_.FastForwardBy(base::Seconds(30));
  // Shorten timeout and expect immediate call to observers after pref change.
  EXPECT_CALL(mock_observer_, OnIdleTimeoutInForeground()).Times(1);
  SetIdleTimeoutPolicy(base::Minutes(1));
  task_environment_.FastForwardBy(base::Seconds(30));
}

// When policy timeout is shortened, actions should run right away.
TEST_F(IdleTimeoutServiceTest, IdleTimeoutPrefsSet_OnPolicyChange) {
  SetIdleTimeoutPolicy(base::Minutes(3));
  InitIdleService();
  EXPECT_CALL(*action_runner_, Run(_)).Times(0);
  idle_service_->OnApplicationWillEnterForeground();

  // Shorten timeout and expect immediate call to observers after pref change.
  task_environment_.FastForwardBy(base::Minutes(2));
  EXPECT_CALL(mock_observer_, OnIdleTimeoutInForeground());
  SetIdleTimeoutPolicy(base::Minutes(1));

  task_environment_.FastForwardBy(base::Seconds(30));
  // Increase the timeout again and make sure observers are not called except on
  // idle.
  EXPECT_CALL(mock_observer_, OnIdleTimeoutInForeground()).Times(1);
  SetIdleTimeoutPolicy(base::Minutes(2));
  task_environment_.FastForwardBy(base::Minutes(1));
}

// Start-up Case: last active time and last idle time unset.
TEST_F(IdleTimeoutServiceTest, NoActionsRunOnStartup_FirstRunWithPolicySet) {
  SetIdleTimeoutPolicy(base::Minutes(1));
  InitIdleService();
  // No call expected when foregrounded.
  EXPECT_CALL(*action_runner_, Run(_)).Times(0);
  EXPECT_CALL(mock_observer_, OnIdleTimeoutOnStartup()).Times(0);
  idle_service_->OnApplicationWillEnterForeground();
  EXPECT_CALL(mock_observer_, OnIdleTimeoutInForeground()).Times(2);
  task_environment_.FastForwardBy(base::Minutes(2));
}

// Start-up Case: current time < last active time + idle threshold.
TEST_F(IdleTimeoutServiceTest, NoActionsRunOnStartup_NoBackgroundTimeout) {
  // Sets the last active time to 30s ago.
  SetLastActiveTime(base::Time::Now() - base::Seconds(30));
  SetIdleTimeoutPolicy(base::Minutes(1));
  InitIdleService();

  EXPECT_CALL(mock_observer_, OnIdleTimeoutOnStartup()).Times(0);
  EXPECT_CALL(*action_runner_, Run(_)).Times(0);
  idle_service_->OnApplicationWillEnterForeground();
  EXPECT_CALL(mock_observer_, OnIdleTimeoutInForeground()).Times(2);
  task_environment_.FastForwardBy(base::Minutes(2));
}

// Start-up Case: base::Time::Now() > last_active_time + idle threshold.
// Last idle time is not set.
TEST_F(IdleTimeoutServiceTest, ActionsRunOnStartup_PostBackgroundTimeout) {
  std::unique_ptr<base::HistogramTester> histogram_tester =
      std::make_unique<base::HistogramTester>();

  SetLastActiveTime(base::Time::Now() - base::Seconds(90));
  SetIdleTimeoutPolicy(base::Minutes(1));
  InitIdleService();

  EXPECT_CALL(mock_observer_, OnIdleTimeoutOnStartup());
  EXPECT_CALL(*action_runner_, Run(_)).Times(1);
  idle_service_->OnApplicationWillEnterForeground();
  // The background case should by recorded at this point.
  histogram_tester->ExpectUniqueSample(
      "Enterprise.IdleTimeoutPolicies.IdleTimeoutCase",
      metrics::IdleTimeoutCase::kBackground, 1);
  EXPECT_CALL(mock_observer_, OnIdleTimeoutInForeground()).Times(2);
  task_environment_.FastForwardBy(base::Minutes(2));
  // The histogram should now have the previous timeout in background case and
  // the last two foreground cases.
  EXPECT_THAT(histogram_tester->GetAllSamples(
                  "Enterprise.IdleTimeoutPolicies.IdleTimeoutCase"),
              testing::ElementsAre(
                  base::Bucket(metrics::IdleTimeoutCase::kForeground, 2),
                  base::Bucket(metrics::IdleTimeoutCase::kBackground, 1)));
}

// Backgrund then reforeground case:
// base::Time::Now() - last_active_time < idle_threshold.
TEST_F(
    IdleTimeoutServiceTest,
    NoActionsRunInBackground_NoActionsRunOnReforegroundWithPrevUserActivity) {
  SetIdleTimeoutPolicy(base::Minutes(1));
  InitIdleService();
  idle_service_->OnApplicationWillEnterForeground();
  task_environment_.FastForwardBy(base::Seconds(30));
  // Simulate user activity.
  SetLastActiveTime(base::Time::Now());

  // Observers triggered on background (to dismiss any dialog).
  EXPECT_CALL(*action_runner_, Run(_)).Times(0);
  EXPECT_CALL(mock_observer_, OnApplicationWillEnterBackground()).Times(1);
  idle_service_->OnApplicationWillEnterBackground();
  task_environment_.FastForwardBy(base::Seconds(30));

  // Idle threshold was not exceeded as idle time = 30s.
  EXPECT_CALL(*action_runner_, Run(_)).Times(0);
  idle_service_->OnApplicationWillEnterForeground();

  // Ensure that idle state is detected after that.
  EXPECT_CALL(mock_observer_, OnIdleTimeoutInForeground()).Times(1);
  EXPECT_CALL(*action_runner_, Run(_)).Times(1);
  task_environment_.FastForwardBy(base::Minutes(1));
  idle_service_->RunActions();
}

// Backgrund then reforeground case:
// last_idle_time > last_active_time + idle_threshold
TEST_F(IdleTimeoutServiceTest,
       NoActionsRunInBackground_NoActionsRunOnReforegrounWithPrevTimeout) {
  SetIdleTimeoutPolicy(base::Minutes(1));
  InitIdleService();
  idle_service_->OnApplicationWillEnterForeground();

  // Fast forward and invoke the `OnActionsCompleted` callback to set the last
  // idle time.
  EXPECT_CALL(mock_observer_, OnIdleTimeoutInForeground()).Times(1);
  EXPECT_CALL(mock_observer_, OnIdleTimeoutActionsCompleted()).Times(1);
  task_environment_.FastForwardBy(base::Minutes(1));
  idle_service_->OnActionsCompleted();
  // `OnIdleTimeoutInForeground` should not be called again.
  task_environment_.FastForwardBy(base::Minutes(1));

  EXPECT_CALL(mock_observer_, OnIdleTimeoutInForeground()).Times(0);
  idle_service_->OnApplicationWillEnterBackground();
  task_environment_.FastForwardBy(base::Minutes(1));

  // No action run on foreground because run was called last idle time and the
  // browser was not active after that. Reforegrounding sets the last active
  // time, so next time the idle state is triggered, actions will run.
  idle_service_->OnApplicationWillEnterForeground();
  // Ensure that idle state is detected after that but actions run after 1
  // minute of no activity since foregrounding.
  EXPECT_CALL(mock_observer_, OnIdleTimeoutInForeground()).Times(1);
  task_environment_.FastForwardBy(base::Minutes(1));
}

// Backgrund then reforeground case:
// base::Time::Now() - last_active_time > idle_threshold
TEST_F(IdleTimeoutServiceTest,
       NoActionsRunInBackground_ActionsRunOnReforeground) {
  SetIdleTimeoutPolicy(base::Minutes(1));
  InitIdleService();
  idle_service_->OnApplicationWillEnterForeground();
  task_environment_.FastForwardBy(base::Seconds(30));
  // Simulate user activity.
  SetLastActiveTime(base::Time::Now());

  EXPECT_CALL(mock_observer_, OnApplicationWillEnterBackground()).Times(1);
  EXPECT_CALL(*action_runner_, Run(_)).Times(0);
  idle_service_->OnApplicationWillEnterBackground();
  task_environment_.FastForwardBy(base::Seconds(80));

  // Actions should run on startup (behind loading screen).
  EXPECT_CALL(*action_runner_, Run(_)).Times(1);
  EXPECT_CALL(mock_observer_, OnIdleTimeoutOnStartup()).Times(1);
  idle_service_->OnApplicationWillEnterForeground();

  // Ensure that idle state is detected after that.
  EXPECT_CALL(mock_observer_, OnIdleTimeoutInForeground()).Times(1);
  task_environment_.FastForwardBy(base::Minutes(1));
}

// Foreground case: idle check should be scheduled for the right time when it
// might become idle.
TEST_F(IdleTimeoutServiceTest, ActionsRunAtCorrectTimesWhileForegrounded) {
  SetIdleTimeoutPolicy(base::Minutes(3));
  InitIdleService();
  idle_service_->OnApplicationWillEnterForeground();

  task_environment_.FastForwardBy(base::Seconds(40));
  // Simulate user activity at t=40s.
  SetLastActiveTime(base::Time::Now());

  // At t=3, actions will not run because idle time has not reached 3.
  // Should check again and run at t=3:40.
  EXPECT_CALL(mock_observer_, OnIdleTimeoutInForeground()).Times(0);
  EXPECT_CALL(*action_runner_, Run(_)).Times(0);
  task_environment_.FastForwardBy(base::Minutes(3) - base::Seconds(40));

  EXPECT_CALL(mock_observer_, OnIdleTimeoutInForeground()).Times(1);
  task_environment_.FastForwardBy(base::Seconds(40));
  idle_service_->OnActionsCompleted();

  // Since the actions have completed last time, and the user continued being
  // idle, observer should not be called.
  EXPECT_CALL(mock_observer_, OnIdleTimeoutInForeground()).Times(0);
  task_environment_.FastForwardBy(base::Minutes(3));
}

// If there are observers for the service, the service should call
// `OnIdleTimeoutInForeground` without running the actions right away. The
// observer will decide if actions should run or not.
TEST_F(IdleTimeoutServiceTest,
       ActionsDoNotRunWhenObserverDoesNotInvokeCallback) {
  SetIdleTimeoutPolicy(base::Minutes(3));
  InitIdleService();
  idle_service_->OnApplicationWillEnterForeground();
  task_environment_.FastForwardBy(base::Seconds(40));
  // Simulate user activity at t=40s.
  SetLastActiveTime(base::Time::Now());

  // At t=3, actions will not run because idle time has not reached 3.
  // Should check again and run at t=3:40.
  EXPECT_CALL(*action_runner_, Run(_)).Times(0);
  task_environment_.FastForwardBy(base::Minutes(3) - base::Seconds(40));

  // Not running the callback passed to `OnIdleTimeoutInForeground` means no
  // actions should run.
  testing::InSequence in_sequence;
  EXPECT_CALL(mock_observer_, OnIdleTimeoutInForeground());
  EXPECT_CALL(*action_runner_, Run(_)).Times(0);
  task_environment_.FastForwardBy(base::Seconds(40));
}

// If the only action set is signout, and the user is not signed in when timeout
// is detected, the actions should not run.
TEST_F(IdleTimeoutServiceTest, NoActionsRunWhenNotNeeded) {
  SetLastActiveTime(base::Time::Now() - base::Seconds(90));
  SetIdleTimeoutPolicy(base::Minutes(1));
  InitIdleService();
  base::Value::List actions;
  actions.Append(static_cast<int>(enterprise_idle::ActionType::kSignOut));
  browser_state_->GetPrefs()->SetList(
      enterprise_idle::prefs::kIdleTimeoutActions, std::move(actions));

  EXPECT_CALL(mock_observer_, OnIdleTimeoutOnStartup()).Times(0);
  EXPECT_CALL(*action_runner_, Run(_)).Times(0);
  idle_service_->OnApplicationWillEnterForeground();
}

// If the only action set is signout, and the user is signed in when timeout is
// detected, the actions should run.
TEST_F(IdleTimeoutServiceTest, ActionsRunWhenNeeded_OnlySignoutSet) {
  SetLastActiveTime(base::Time::Now() - base::Seconds(90));
  SetIdleTimeoutPolicy(base::Minutes(1));
  InitIdleService();
  SignIn();
  base::Value::List actions;
  actions.Append(static_cast<int>(enterprise_idle::ActionType::kSignOut));
  browser_state_->GetPrefs()->SetList(
      enterprise_idle::prefs::kIdleTimeoutActions, std::move(actions));

  EXPECT_CALL(mock_observer_, OnIdleTimeoutOnStartup());
  EXPECT_CALL(*action_runner_, Run(_)).Times(1);
  idle_service_->OnApplicationWillEnterForeground();
}

// If the only action set is not signout, the action should run regardless of
// the sign-in state.
TEST_F(IdleTimeoutServiceTest, ActionsRunWhenNeeded_OnlyActionSetIsNotSignOut) {
  SetLastActiveTime(base::Time::Now() - base::Seconds(90));
  SetIdleTimeoutPolicy(base::Minutes(1));
  InitIdleService();
  base::Value::List actions;
  actions.Append(
      static_cast<int>(enterprise_idle::ActionType::kClearBrowsingHistory));
  browser_state_->GetPrefs()->SetList(
      enterprise_idle::prefs::kIdleTimeoutActions, std::move(actions));

  EXPECT_CALL(mock_observer_, OnIdleTimeoutOnStartup());
  EXPECT_CALL(*action_runner_, Run(_)).Times(1);
  idle_service_->OnApplicationWillEnterForeground();
}

// If the only action set is signout, and the user is signed in when timeout is
// detected, the actions should run.
TEST_F(IdleTimeoutServiceTest,
       ActionsRunWhenNeeded_ManyActionsSetAndUserNotSignedIn) {
  SetLastActiveTime(base::Time::Now() - base::Seconds(90));
  SetIdleTimeoutPolicy(base::Minutes(1));
  InitIdleService();
  base::Value::List actions;
  actions.Append(
      static_cast<int>(enterprise_idle::ActionType::kClearBrowsingHistory));
  actions.Append(static_cast<int>(enterprise_idle::ActionType::kSignOut));
  browser_state_->GetPrefs()->SetList(
      enterprise_idle::prefs::kIdleTimeoutActions, std::move(actions));

  EXPECT_CALL(mock_observer_, OnIdleTimeoutOnStartup());
  EXPECT_CALL(*action_runner_, Run(_)).Times(1);
  idle_service_->OnApplicationWillEnterForeground();
}

}  // namespace enterprise_idle
