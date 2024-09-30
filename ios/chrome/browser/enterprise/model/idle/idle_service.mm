// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/model/idle/idle_service.h"

#import <UIKit/UIKit.h>

#import "base/check_is_test.h"
#import "components/enterprise/idle/idle_pref_names.h"
#import "components/enterprise/idle/metrics.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"

namespace enterprise_idle {

IdleService::IdleService(ProfileIOS* profile)
    : profile_(profile),
      action_runner_(std::make_unique<ActionRunnerImpl>(profile_)) {
  pref_change_registrar_.Init(profile_->GetPrefs());
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
  return profile_->GetPrefs()->GetTimeDelta(
      enterprise_idle::prefs::kIdleTimeout);
}

void IdleService::OnApplicationWillEnterForeground() {
  base::TimeDelta idle_threshold = GetTimeout();
  base::Time last_active_time = GetLastActiveTime();

  // Do nothing when the policy is unset.
  if (!idle_threshold.is_positive()) {
    return;
  }

  //  This case will happen the first time the browser runs with the
  //  `LastActiveTimestamp` pref or if  the policy is not set.
  if (last_active_time == base::Time()) {
    PostCheckIdleTask(idle_threshold);
  } else if (IsIdleAfterPreviouslyBeingActive()) {
    // Check `IsIdleAfterPreviouslyBeingActive` for more details about this
    // case.
    MaybeRunActionsForState(LastState::kIdleOnBackground);
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
  if (!IsIdleTimeoutPolicySet()) {
    // Do nothing if the policy is not set.
    return;
  }
  // Relying on `OnApplicationWillEnterForeground` to reset the callback
  // is not reliable. The old tasks remain leading to unpredictable scheduling
  // behaviour.
  for (auto& observer : observer_list_) {
    observer.OnApplicationWillEnterBackground();
  }
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
  base::TimeDelta time_to_idle =
      GetLastActiveTime() - base::Time::Now() + GetTimeout();
  return time_to_idle.is_positive() ? time_to_idle : GetTimeout();
}

void IdleService::PostCheckIdleTask(base::TimeDelta time_from_now) {
  // No tasks should be scheduled when the policy is not set.
  CHECK(GetTimeout().is_positive());

  cancelable_actions_callback_.Reset(
      base::BindOnce(&IdleService::CheckIfIdle, weak_factory_.GetWeakPtr()));
  // Post task to check idle state when it can potentially happen.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, cancelable_actions_callback_.callback(), time_from_now);
}

void IdleService::CheckIfIdle() {
  if (IsIdleAfterPreviouslyBeingActive()) {
    MaybeRunActionsForState(LastState::kIdleOnForeground);
    return;
  }

  PostCheckIdleTask(GetPossibleTimeToIdle());
}

bool IdleService::IsIdleAfterPreviouslyBeingActive() {
  base::TimeDelta idle_threshold = GetTimeout();
  base::Time last_active_time = GetLastActiveTime();
  base::Time last_idle_time =
      profile_->GetPrefs()->GetTime(enterprise_idle::prefs::kLastIdleTimestamp);

  // Return false when the policy is not set.
  if (!idle_threshold.is_positive()) {
    return false;
  }

  // There are  two cases we want to run  the actions:
  // 1. If the browser has never been idle, the last idle timestamp will be
  // empty, so just check the last active time.
  // 2. If the browser has been idle at some point before, then became active,
  // and the actions have not run since the last time the browser was active.
  // The goal of #2 is to avoid running the actions every `idle_threshold`
  // minutes if nothing changed in the idle state; i.e. it can be idle now but
  // it may have been idle for a long time with no activity. The conditions are
  // separated for readability.
  bool is_idle_for_first_time =
      last_idle_time == base::Time() &&
      (base::Time::Now() - last_active_time) >= idle_threshold;
  bool is_idle_after_being_active =
      last_idle_time != base::Time() &&
      last_idle_time < (last_active_time + idle_threshold) &&
      (base::Time::Now() - last_active_time) >= idle_threshold;

  return is_idle_for_first_time || is_idle_after_being_active;
}

bool IdleService::IsIdleTimeoutPolicySet() {
  return GetTimeout().is_positive();
}

void IdleService::RunActionsForStateForTesting(LastState last_state) {
  CHECK_IS_TEST();
  MaybeRunActionsForState(last_state);
}

void IdleService::MaybeRunActionsForState(LastState last_state) {
  last_action_set_ =
      GetActionSet(profile_->GetPrefs(),
                   AuthenticationServiceFactory::GetForBrowserState(profile_));

  if (!IsAnyActionNeededToRun()) {
    PostCheckIdleTask(GetTimeout());
    return;
  }

  if (last_state == LastState::kIdleOnBackground) {
    metrics::RecordIdleTimeoutCase(metrics::IdleTimeoutCase::kBackground);
    for (auto& observer : observer_list_) {
      // Show loading UI on re-foreground right away if data will be cleared.
      observer.OnIdleTimeoutOnStartup();
    }
    RunActions();
  } else {
    metrics::RecordIdleTimeoutCase(metrics::IdleTimeoutCase::kForeground);
    idle_timeout_dialog_pending_ = !observer_list_.empty();
    idle_trigger_time_ = base::Time::Now();
    for (auto& observer : observer_list_) {
      // Confirm that the user is not active by showing dialog before running
      // actions.
      observer.OnIdleTimeoutInForeground();
    }
  }

  PostCheckIdleTask(GetTimeout());
}

void IdleService::RunActions() {
  action_runner_->Run(base::BindOnce(&IdleService::OnActionsCompleted,
                                     weak_factory_.GetWeakPtr()));
}

bool IdleService::IsAnyActionNeededToRun() {
  // Returns true if any action will run. The return can be false if
  // 1. the only idle timeout action that is set is signout, but
  // the user is not currently signed in.
  // 2. the actions set are not known (empty IdleTimeoutActions pref).
  return last_action_set_.clear || last_action_set_.close ||
         last_action_set_.signout;
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
  idle_timeout_snackbar_pending_ = true;
  profile_->GetPrefs()->SetTime(enterprise_idle::prefs::kLastIdleTimestamp,
                                base::Time::Now());
  for (auto& observer : observer_list_) {
    observer.OnIdleTimeoutActionsCompleted();
  }
}

base::Time IdleService::GetIdleTriggerTime() {
  return idle_trigger_time_;
}

ActionSet IdleService::GetLastActionSet() {
  return last_action_set_;
}

void IdleService::OnIdleTimeoutDialogPresented() {
  idle_timeout_dialog_pending_ = false;
}

bool IdleService::ShouldIdleTimeoutDialogBePresented() {
  return idle_timeout_dialog_pending_;
}

void IdleService::OnIdleTimeoutSnackbarPresented() {
  idle_timeout_snackbar_pending_ = false;
}

bool IdleService::ShouldIdleTimeoutSnackbarBePresented() {
  return idle_timeout_snackbar_pending_;
}

void IdleService::SetActionRunnerForTesting(
    std::unique_ptr<ActionRunner> action_runner) {
  CHECK_IS_TEST();
  action_runner_ = std::move(action_runner);
}

ActionRunner* IdleService::GetActionRunnerForTesting() {
  CHECK_IS_TEST();
  return action_runner_.get();
}

}  // namespace enterprise_idle
