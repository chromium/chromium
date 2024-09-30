// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_IDLE_SERVICE_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_IDLE_SERVICE_H_

#import "base/cancelable_callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/time/time.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/prefs/pref_change_registrar.h"
#import "ios/chrome/browser/enterprise/model/idle/action_runner_impl.h"
#import "ios/chrome/browser/enterprise/model/idle/idle_timeout_policy_utils.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace enterprise_idle {

// Manages the idle state of a profile for the IdleTimeout enterprise
// policy. Keeps track of the policy's value, and triggers actions to run when
// the browser is idle in foreground or on startup.
class IdleService : public KeyedService {
 public:
  enum class LastState { kIdleOnBackground, kIdleOnForeground };

  // Observer handling the browsing timing out without being active.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnIdleTimeoutInForeground() = 0;
    virtual void OnIdleTimeoutOnStartup() = 0;
    virtual void OnIdleTimeoutActionsCompleted() = 0;
    virtual void OnApplicationWillEnterBackground() = 0;
  };

  explicit IdleService(ProfileIOS* profile);

  IdleService(const IdleService&) = delete;
  IdleService& operator=(const IdleService&) = delete;
  ~IdleService() override;

  // Adds and removes observers. Called when the scene agent implementing the
  // observer  is created or destroyed.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  // TODO(b/301676922): Move calls to `OnApplicationWillEnterForeground`
  // and `OnApplicationWillEnterBackground` to an AppStateObserver.
  // Called when the application is foregrounded, which can be after one of
  // two cases:
  // 1. Cold Start when the browser was not already running
  // 2. Warm start when the browser was already running in the background
  void OnApplicationWillEnterForeground();
  // Called when the application is backgrounded to cancel the scheduled task.
  // This ensure that actions do not run when the app is backgrounded or when
  // the screen is locked.
  void OnApplicationWillEnterBackground();
  // Checks whether the browser has been idle for the first time since last
  // being active.
  bool IsIdleAfterPreviouslyBeingActive();
  // Returns true if the `IdleTimeout` pref is set.
  bool IsIdleTimeoutPolicySet();
  // Runs actions on timeout after it has been confirmed that the user is idle.
  void RunActions();
  // Shows the snackbar after actions have completed.
  void OnActionsCompleted();
  // Returns the time `onIdleTimeoutInForeground` is triggered.
  // Used to determine the start of the countdown displayed. Usually the
  // countdown is 30s, but might need to be adjusted if the dialog was already
  // started on a different scene that was closed.
  base::Time GetIdleTriggerTime();
  // Returns the action set at the time of idle timeout detection.
  // Used for consistency of types across observers.
  ActionSet GetLastActionSet();

  // Called when a timeout confirmation dialog has been dismissed or expired to
  // unset `idle_timeout_notification_pending_` which prevent other observers
  // from trying to reshow the dialog.
  void OnIdleTimeoutDialogPresented();
  bool ShouldIdleTimeoutDialogBePresented();
  // Called when the snackbar has been displayed to unset
  // `idle_timeout_notification_pending_` which ensures that the snackbar does
  // not show more than once on start-up.
  void OnIdleTimeoutSnackbarPresented();
  bool ShouldIdleTimeoutSnackbarBePresented();

  void SetActionRunnerForTesting(std::unique_ptr<ActionRunner> action_runner);
  ActionRunner* GetActionRunnerForTesting();
  // Test wrapper for `RunActionsForState`.
  void RunActionsForStateForTesting(LastState last_state);

  void Shutdown() override;

 private:
  // Returns the value of the `kIdleTimeout` pref.
  base::TimeDelta GetTimeout() const;
  // Called when the IdleTimeout policy changes.
  void OnIdleTimeoutPrefChanged();
  // Posts a task that checks whether the browser has been idle for time >=
  // `idle_threshold_".
  void PostCheckIdleTask(base::TimeDelta time_from_now);
  // Checks whether the idle state has been reached. If idle state is reached,
  // it calls `RunActionsForState` to run actions. Otherwise, it posts a task to
  // check again when the browser might possibly become idle.
  void CheckIfIdle();
  // Checks if any action needs to run on idle timeout.
  bool IsAnyActionNeededToRun();
  // Runs the actions based on `IdleTimeoutActions` and update the UI based on
  // the last state the browser was  idle in.
  void MaybeRunActionsForState(LastState last_state);
  // Calculates the time to when the browser might become idle.
  base::TimeDelta GetPossibleTimeToIdle();

  void SetLastActiveTime();
  base::Time GetLastActiveTime();

  base::Time idle_trigger_time_;
  ActionSet last_action_set_;
  bool idle_timeout_dialog_pending_{false};
  bool idle_timeout_snackbar_pending_{false};
  raw_ptr<ProfileIOS> profile_;
  std::unique_ptr<ActionRunner> action_runner_;
  PrefChangeRegistrar pref_change_registrar_;
  base::CancelableOnceCallback<void()> cancelable_actions_callback_;
  base::ObserverList<Observer, true> observer_list_;
  base::WeakPtrFactory<IdleService> weak_factory_{this};
};

}  // namespace enterprise_idle

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_IDLE_SERVICE_H_
