// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_ACTION_RUNNER_IMPL_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_ACTION_RUNNER_IMPL_H_

#import "ios/chrome/browser/enterprise/model/idle/action.h"
#import "ios/chrome/browser/enterprise/model/idle/action_runner.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace enterprise_idle {

class ActionRunnerImpl : public ActionRunner {
 public:
  using ActionQueue = ActionFactory::ActionQueue;

  ActionRunnerImpl(ProfileIOS* profile);
  ~ActionRunnerImpl() override;

  ActionRunnerImpl(const ActionRunnerImpl&) = delete;
  ActionRunnerImpl& operator=(const ActionRunnerImpl&) = delete;
  ActionRunnerImpl(ActionRunnerImpl&&) = delete;
  ActionRunnerImpl& operator=(ActionRunnerImpl&&) = delete;

  // Runs all the actions, in order of priority. Actions are run sequentially,
  // not in parallel. If an action fails for whatever reason, skips the
  // remaining actions.
  void Run(ActionsCompletedCallback actions_completed_callback) override;
  void SetActionFactoryForTesting(
      std::unique_ptr<ActionFactory> action_factory);

 private:
  // Defines the set of actions to be run when Run() is called. `actions` is
  // typically the value of a *Actions policy, e.g. IdleTimeoutActions.
  ActionQueue GetActions();
  // Helper function for Run() and OnActionFinished(). Run the first action in
  // the queue, and schedules the rest of the actions to run when the first
  // action is done.
  void RunNextAction(ActionQueue actions);

  // Callback used by RunSingleAction. Runs when an action finishes, and kicks
  // off the next action (if there's one).
  void OnActionFinished(ActionQueue remaining_actions, bool succeeded);

  base::TimeTicks actions_start_time_;
  ActionsCompletedCallback actions_completed_callback_;
  raw_ptr<ProfileIOS> profile_;
  std::unique_ptr<ActionFactory> action_factory_;
  base::WeakPtrFactory<ActionRunnerImpl> weak_ptr_factory_{this};
};

}  // namespace enterprise_idle

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_ACTION_RUNNER_IMPL_H_
