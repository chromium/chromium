// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/model/idle/action_runner_impl.h"

#import "base/test/gmock_callback_support.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/mock_callback.h"
#import "base/time/time.h"
#import "components/enterprise/idle/idle_pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using ::testing::_;

namespace enterprise_idle {

namespace {

class FakeActionFactory : public ActionFactory {
 public:
  FakeActionFactory() = default;
  ~FakeActionFactory() override = default;

  ActionQueue Build(
      const std::vector<ActionType>& action_types,
      BrowsingDataRemover* main_browsing_data_remover,
      BrowsingDataRemover* incognito_browsing_data_remover) override {
    ActionQueue actions;
    for (ActionType action_type : action_types) {
      auto it = associations_.find(action_type);
      if (it != associations_.end()) {
        actions.push(std::move(it->second));
        associations_.erase(it);
      }
    }
    return actions;
  }

  void Associate(ActionType action_type, std::unique_ptr<Action> action) {
    associations_[action_type] = std::move(action);
  }

 private:
  std::map<ActionType, std::unique_ptr<Action>> associations_;
};

class MockAction : public Action {
 public:
  explicit MockAction(ActionType action_type)
      : Action(static_cast<int>(action_type)) {}

  MOCK_METHOD2(Run, void(ProfileIOS*, Continuation));
};

// testing::InvokeArgument<N> does not work with base::OnceCallback, so we
// define our own gMock action to run the 2nd argument.
// Used to mock success and failure in successive actions.
ACTION_P(RunContinuation, success) {
  std::move(const_cast<Action::Continuation&>(arg1)).Run(success);
}

}  // namespace

class IdleActionRunnerTest : public PlatformTest {
 protected:
  void SetUp() override {
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
  }

  void TearDown() override {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
    profile_.reset();
  }

  void SetIdleTimeoutActions(std::vector<ActionType> action_types) {
    base::Value::List actions;
    for (auto action_type : action_types) {
      actions.Append(static_cast<int>(action_type));
    }
    profile()->GetPrefs()->SetList(prefs::kIdleTimeoutActions,
                                   std::move(actions));
  }

  TestProfileIOS* profile() { return profile_.get(); }

 protected:
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that the order of actions in the pref doesn't matter. They still run
// by order of priority.
TEST_F(IdleActionRunnerTest, PrefOrderDoesNotMatter) {
  std::unique_ptr<FakeActionFactory> action_factory =
      std::make_unique<FakeActionFactory>();
  base::MockCallback<ActionRunner::ActionsCompletedCallback>
      actions_completed_callback;
  ActionRunnerImpl runner(profile());
  SetIdleTimeoutActions({ActionType::kCloseTabs,
                         ActionType::kClearBrowsingHistory,
                         ActionType::kSignOut});

  auto close_tabs = std::make_unique<MockAction>(ActionType::kCloseTabs);
  auto clear_history =
      std::make_unique<MockAction>(ActionType::kClearBrowsingHistory);
  auto sign_out = std::make_unique<MockAction>(ActionType::kSignOut);

  testing::InSequence in_sequence;
  EXPECT_CALL(*clear_history, Run(profile(), _))
      .WillOnce(RunContinuation(true));
  EXPECT_CALL(*close_tabs, Run(profile(), _)).WillOnce(RunContinuation(true));
  EXPECT_CALL(*sign_out, Run(profile(), _)).WillOnce(RunContinuation(true));

  action_factory->Associate(ActionType::kCloseTabs, std::move(close_tabs));
  action_factory->Associate(ActionType::kClearBrowsingHistory,
                            std::move(clear_history));
  action_factory->Associate(ActionType::kSignOut, std::move(sign_out));

  EXPECT_CALL(actions_completed_callback, Run()).Times(1);
  runner.SetActionFactoryForTesting(std::move(action_factory));
  runner.Run(actions_completed_callback.Get());
}

// Tests that when a higher-priority action fails, the lower-priority actions
// don't run.
TEST_F(IdleActionRunnerTest, OtherActionsDontRunOnFailure) {
  std::unique_ptr<base::HistogramTester> histogram_tester =
      std::make_unique<base::HistogramTester>();
  std::unique_ptr<FakeActionFactory> action_factory =
      std::make_unique<FakeActionFactory>();
  ActionRunnerImpl runner(profile());
  base::MockCallback<ActionRunner::ActionsCompletedCallback>
      actions_completed_callback;
  SetIdleTimeoutActions({ActionType::kCloseTabs, ActionType::kSignOut});

  auto close_tabs = std::make_unique<MockAction>(ActionType::kCloseTabs);
  auto sign_out = std::make_unique<MockAction>(ActionType::kSignOut);

  // "sign_out" shouldn't run, because "close_tabs" fails.
  testing::InSequence in_sequence;
  EXPECT_CALL(*close_tabs, Run(profile(), _)).WillOnce(RunContinuation(false));
  EXPECT_CALL(*sign_out, Run(_, _)).Times(0);

  action_factory->Associate(ActionType::kCloseTabs, std::move(close_tabs));
  action_factory->Associate(ActionType::kSignOut, std::move(sign_out));

  EXPECT_CALL(actions_completed_callback, Run()).Times(0);
  runner.SetActionFactoryForTesting(std::move(action_factory));
  runner.Run(actions_completed_callback.Get());
  histogram_tester->ExpectUniqueSample(
      "Enterprise.IdleTimeoutPolicies.Success.AllActions", false, 1);
}

// Tests that it does nothing when the "IdleTimeoutActions" pref is empty.
TEST_F(IdleActionRunnerTest, DoNothingWithEmptyPref) {
  std::unique_ptr<FakeActionFactory> action_factory =
      std::make_unique<FakeActionFactory>();
  ActionRunnerImpl runner(profile());
  base::MockCallback<ActionRunner::ActionsCompletedCallback>
      actions_completed_callback;

  // "IdleTimeoutActions" is deliberately unset.
  auto clear_browsing_history =
      std::make_unique<MockAction>(ActionType::kClearBrowsingHistory);
  auto clear_cookies_and_site_data =
      std::make_unique<MockAction>(ActionType::kClearCookiesAndOtherSiteData);

  EXPECT_CALL(*clear_browsing_history, Run(_, _)).Times(0);
  EXPECT_CALL(*clear_cookies_and_site_data, Run(_, _)).Times(0);

  action_factory->Associate(ActionType::kClearBrowsingHistory,
                            std::move(clear_browsing_history));
  action_factory->Associate(ActionType::kClearCookiesAndOtherSiteData,
                            std::move(clear_cookies_and_site_data));

  EXPECT_CALL(actions_completed_callback, Run()).Times(0);
  runner.SetActionFactoryForTesting(std::move(action_factory));
  runner.Run(actions_completed_callback.Get());
}

}  // namespace enterprise_idle
