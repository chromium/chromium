// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/model/idle/idle_service.h"

#import <UIKit/UIKit.h>

#import "base/check_is_test.h"
#import "components/enterprise/idle/idle_features.h"
#import "components/enterprise/idle/idle_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

namespace enterprise_idle {

IdleService::IdleService(ChromeBrowserState* browser_state)
    : browser_state_(browser_state),
      action_runner_(std::make_unique<ActionRunnerImpl>(browser_state_)) {
  if (!base::FeatureList::IsEnabled(kIdleTimeout)) {
    return;
  }

  pref_change_registrar_.Init(browser_state_->GetPrefs());
  pref_change_registrar_.Add(
      enterprise_idle::prefs::kIdleTimeout,
      base::BindRepeating(&IdleService::OnIdleTimeoutPrefChanged,
                          base::Unretained(this)));
}

IdleService::~IdleService() = default;

void IdleService::Shutdown() {
  pref_change_registrar_.RemoveAll();
  action_runner_.reset();
}

void IdleService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void IdleService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

base::TimeDelta IdleService::GetTimeout() const {
  return browser_state_->GetPrefs()->GetTimeDelta(
      enterprise_idle::prefs::kIdleTimeout);
}

void IdleService::OnApplicationWillEnterForeground() {
  DCHECK(base::FeatureList::IsEnabled(kIdleTimeout));

  base::TimeDelta idle_threshold = GetTimeout();
  base::Time last_active_time = GetLastActiveTime();
  base::Time last_idle_time = browser_state_->GetPrefs()->GetTime(
      enterprise_idle::prefs::kLastIdleTimestamp);

  // Do nothing when the policy is unset.
  if (!idle_threshold.is_positive()) {
    return;
  }

  //  This case will happen the first time the browser runs with the
  //  `LastActiveTimestamp` pref or if  the policy is not set.
  if (last_active_time == base::Time()) {
    PostCheckIdleTask(idle_threshold);
  }

  // There are  two cases we want to run  the actions:
  // 1. If the browser has never been idle, the last idle timestamp will be
  // empty, so just check the last active time.
  // 2. If the browser has been inactive,  and the actions were not
  // run while the browser was backgrounded or closed.
  // The conditions are separated for readability.
  else if (last_idle_time == base::Time() &&
           (base::Time::Now() - last_active_time) >= idle_threshold) {
    RunActionsForState(LastState::kIdleOnBackground);
  } else if (last_idle_time != base::Time() &&
             last_idle_time <= last_active_time + idle_threshold) {
    RunActionsForState(LastState::kIdleOnBackground);
  } else {
    // The browser's last state was:
    // 1. active less than `idle_threshold` minutes ago.
    // 2. idle and the actions were already run when it was idle.
    // Restart the timer as foregrounding is considered new user activity.
    PostCheckIdleTask(idle_threshold);
  }

  SetLastActiveTime();
}

void IdleService::OnApplicationWillEnterBackground() {
  // Relying on `OnApplicationWillEnterForeground` to reset the callback
  // is not reliable. The old tasks remain leading to unpredictable scheduling
  // behaviour.
  cancelable_actions_callback_.Cancel();
}

void IdleService::OnIdleTimeoutPrefChanged() {
  if (GetTimeout().is_positive()) {
    CheckIfIdle();
  } else {
    // Cancel any outstanding callback if idle timeout is no longer valid.
    cancelable_actions_callback_.Cancel();
  }
}

base::TimeDelta IdleService::GetPossibleTimeToIdle() {
  return GetLastActiveTime() - base::Time::Now() + GetTimeout();
}

void IdleService::PostCheckIdleTask(base::TimeDelta time_from_now) {
  cancelable_actions_callback_.Reset(
      base::BindOnce(&IdleService::CheckIfIdle, weak_factory_.GetWeakPtr()));
  // Post task to check idle state when it can potentially happen.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, cancelable_actions_callback_.callback(), time_from_now);
}

void IdleService::CheckIfIdle() {
  DCHECK(base::FeatureList::IsEnabled(kIdleTimeout));
  base::TimeDelta idle_threshold = GetTimeout();
  base::Time last_active_time = GetLastActiveTime();

  if ((base::Time::Now() - last_active_time) >= idle_threshold) {
    RunActionsForState(LastState::kIdleOnForeground);
    return;
  }

  PostCheckIdleTask(GetPossibleTimeToIdle());
}

void IdleService::RunActionsForState(LastState last_state) {
  DCHECK(base::FeatureList::IsEnabled(kIdleTimeout));
  if (last_state == LastState::kIdleOnBackground) {
    // TODO: check if data will be cleared.
    for (auto& observer : observer_list_) {
      // Show loading UI on re-foreground right away if data will be clared.
      observer.OnClearDataOnStartup();
    }
    RunActions();
  } else if (observer_list_.empty()) {
    RunActions();
  } else {
    for (auto& observer : observer_list_) {
      // Confirm that the user is not active by showing dialog before running
      // actions.
      observer.OnIdleTimeoutInForeground();
    }
  }

  browser_state_->GetPrefs()->SetTime(
      enterprise_idle::prefs::kLastIdleTimestamp, base::Time::Now());
  PostCheckIdleTask(GetTimeout());
}

void IdleService::RunActions() {
  action_runner_->Run(base::BindOnce(&IdleService::OnActionsCompleted,
                                     weak_factory_.GetWeakPtr()));
}

void IdleService::SetLastActiveTime() {
  GetApplicationContext()->GetLocalState()->SetTime(
      enterprise_idle::prefs::kLastActiveTimestamp, base::Time::Now());
}

base::Time IdleService::GetLastActiveTime() {
  return GetApplicationContext()->GetLocalState()->GetTime(
      enterprise_idle::prefs::kLastActiveTimestamp);
}

void IdleService::OnActionsCompleted() {
  // TODO: Implement this method to show snackbar.
}

void IdleService::SetActionRunnerForTesting(
    std::unique_ptr<ActionRunner> action_runner) {
  CHECK_IS_TEST();
  action_runner_ = std::move(action_runner);
}

ActionRunner* IdleService::GetActionRunnerForTesting() {
  return action_runner_.get();
}

}  // namespace enterprise_idle
