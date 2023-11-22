// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_IDLE_SERVICE_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_IDLE_SERVICE_H_

#import "base/cancelable_callback.h"
#import "base/memory/weak_ptr.h"
#import "base/time/time.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/prefs/pref_change_registrar.h"
#import "ios/chrome/browser/enterprise/model/idle/action_runner_impl.h"

class ChromeBrowserState;

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
    virtual void OnClearDataOnStartup() = 0;
    virtual void OnIdleTimeoutActionsCompleted() = 0;
  };

  explicit IdleService(ChromeBrowserState* browser_state);

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

  void SetActionRunnerForTesting(std::unique_ptr<ActionRunner> action_runner);
  ActionRunner* GetActionRunnerForTesting();

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
  // Runs the actions based on `IdleTimeoutActions` and update the UI based on
  // the last state the browser was  idle in.
  void RunActionsForState(LastState last_state);
  void RunActions();
  // Shows the snackbar after actions have completed.
  void OnActionsCompleted();
  // Calculates the time to when the browser might become idle.
  base::TimeDelta GetPossibleTimeToIdle();

  void SetLastActiveTime();
  base::Time GetLastActiveTime();

  ChromeBrowserState* browser_state_;
  std::unique_ptr<ActionRunner> action_runner_;
  PrefChangeRegistrar pref_change_registrar_;
  base::CancelableOnceCallback<void()> cancelable_actions_callback_;
  base::ObserverList<Observer, true> observer_list_;
  base::WeakPtrFactory<IdleService> weak_factory_{this};
};

}  // namespace enterprise_idle

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_IDLE_SERVICE_H_
