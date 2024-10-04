// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/model/idle/action_runner_impl.h"

#import <iterator>

#import "base/functional/bind.h"
#import "base/ranges/algorithm.h"
#import "components/enterprise/idle/idle_pref_names.h"
#import "components/enterprise/idle/metrics.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_factory.h"

namespace enterprise_idle {

ActionRunnerImpl::ActionRunnerImpl(ProfileIOS* profile)
    : profile_(profile), action_factory_(std::make_unique<ActionFactory>()) {}

ActionRunnerImpl::~ActionRunnerImpl() = default;

void ActionRunnerImpl::Run(
    ActionsCompletedCallback actions_completed_callback) {
  actions_completed_callback_ = std::move(actions_completed_callback);
  ActionQueue actions = GetActions();
  if (actions.empty()) {
    return;
  }
  actions_start_time_ = base::TimeTicks::Now();
  RunNextAction(std::move(actions));
}

void ActionRunnerImpl::SetActionFactoryForTesting(
    std::unique_ptr<ActionFactory> action_factory) {
  action_factory_ = std::move(action_factory);
}

ActionRunnerImpl::ActionQueue ActionRunnerImpl::GetActions() {
  std::vector<ActionType> actions;
  base::ranges::transform(
      profile_->GetPrefs()->GetList(prefs::kIdleTimeoutActions),
      std::back_inserter(actions), [](const base::Value& action) {
        return static_cast<ActionType>(action.GetInt());
      });
  return action_factory_->Build(
      actions, BrowsingDataRemoverFactory::GetForProfile(profile_),
      BrowsingDataRemoverFactory::GetForProfile(
          profile_->GetOffTheRecordProfile()));
}

void ActionRunnerImpl::RunNextAction(ActionQueue actions) {
  DUMP_WILL_BE_CHECK(!actions.empty());
  const std::unique_ptr<Action>& action = actions.top();

  action->Run(profile_, base::BindOnce(&ActionRunnerImpl::OnActionFinished,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       std::move(actions)));
}

void ActionRunnerImpl::OnActionFinished(ActionQueue remaining_actions,
                                        bool succeeded) {
  remaining_actions.pop();

  if (!succeeded) {
    // Previous action failed. Log failure and abort.
    metrics::RecordActionsSuccess(metrics::IdleTimeoutActionType::kAllActions,
                                  false);
    return;
  }

  if (remaining_actions.empty()) {
    // All done. Run callback to show snackbar.
    // Callback can be empty in tests.
    if (actions_completed_callback_) {
      metrics::RecordActionsSuccess(metrics::IdleTimeoutActionType::kAllActions,
                                    true);
      metrics::RecordIdleTimeoutActionTimeTaken(
          metrics::IdleTimeoutActionType::kAllActions,
          base::TimeTicks::Now() - actions_start_time_);

      std::move(actions_completed_callback_).Run();
    }
    return;
  }

  RunNextAction(std::move(remaining_actions));
}

}  // namespace enterprise_idle
