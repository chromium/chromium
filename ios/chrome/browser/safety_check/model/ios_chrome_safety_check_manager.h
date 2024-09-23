// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFETY_CHECK_MODEL_IOS_CHROME_SAFETY_CHECK_MANAGER_H_
#define IOS_CHROME_BROWSER_SAFETY_CHECK_MODEL_IOS_CHROME_SAFETY_CHECK_MANAGER_H_

#import "base/memory/scoped_refptr.h"
#import "base/memory/weak_ptr.h"
#import "base/observer_list.h"
#import "base/observer_list_types.h"
#import "base/sequence_checker.h"
#import "base/task/sequenced_task_runner.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/prefs/pref_change_registrar.h"
#import "ios/chrome/browser/omaha/model/omaha_service.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"

class IOSChromeSafetyCheckManager;
class PrefService;
struct UpgradeRecommendedDetails;

// All IOSChromeSafetyCheckManagerObserver events will be evaluated on the
// same sequence the IOSChromeSafetyCheckManager is created on.
class IOSChromeSafetyCheckManagerObserver : public base::CheckedObserver {
 public:
  // Called whenever the Safety Check determines a change in the Password check
  // state (i.e. when the user has reused passwords, weak passwords, no
  // compromised password, etc.). Also provides the latest count of insecure
  // credentials.
  virtual void PasswordCheckStateChanged(
      PasswordSafetyCheckState state,
      password_manager::InsecurePasswordCounts insecure_password_counts) {}
  // Called whenever the Safety Check determines a change in the Safe Browsing
  // check state (i.e. when Safe Browsing is enabled, disabled, the check
  // is currently running, etc.)
  virtual void SafeBrowsingCheckStateChanged(
      SafeBrowsingSafetyCheckState state) {}
  // Called whenever the Safety Check determines a change in the Update Chrome
  // check state (i.e. when Chrome is up to date, Chrome is out of date, the
  // check is currently running, etc.)
  virtual void UpdateChromeCheckStateChanged(
      UpdateChromeSafetyCheckState state) {}
  // Called whenever the Safety Check begins the async process of evaluating the
  // Password check, Safe Browsing check, and/or Update check.
  virtual void RunningStateChanged(RunningSafetyCheckState state) {}
  // Notifies the observer that `safety_check_manager` has begun shutting down.
  // Observers should remove themselves from the manager via
  // `safety_check_manager->RemoveObserver(...)` when this happens.
  virtual void ManagerWillShutdown(
      IOSChromeSafetyCheckManager* safety_check_manager) {}
};

// This class handles the bulk of the safety check feature, namely:
//
// 1. Monitors:
//    - Password check status and compromised credentials list
//    - Enhanced Safe Browsing enablement
//    - App update status
//    - Safety Check execution status (running/complete)
//    - Results of previous Safety Check runs
//
// 2. Automatic Safety Check Runs:
//    - Triggers Safety Check automatically if the last run is older than
//      'kSafetyCheckAutorunDelay' (e.g., 30 days), regardless of whether the
//      previous run was manual or automatic.
//
// 3. Observer Notifications:
//    - Notifies `IOSChromeSafetyCheckManagerObserver` of any state changes
//      (e.g., compromised password detected, Safety Check completed).
class IOSChromeSafetyCheckManager
    : public KeyedService,
      public IOSChromePasswordCheckManager::Observer,
      public OmahaServiceObserver {
 public:
  explicit IOSChromeSafetyCheckManager(
      PrefService* pref_service,
      PrefService* local_pref_service,
      scoped_refptr<IOSChromePasswordCheckManager> password_check_manager,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  IOSChromeSafetyCheckManager(const IOSChromeSafetyCheckManager&) = delete;
  IOSChromeSafetyCheckManager& operator=(const IOSChromeSafetyCheckManager&) =
      delete;

  ~IOSChromeSafetyCheckManager() override;

  // KeyedService implementation.
  void Shutdown() override;

  // Starts the Safety Check, which comprises starting
  // the: [1] Safe Browsing check, [2] Update Chrome check, and [3] Passwords
  // Check, and notifies any observers of the change.
  //
  // NOTE: If the Safety Check is already running, does nothing.
  void StartSafetyCheck();

  // Stops the currently running Safety Check, if any, which comprises stopping
  // the: [1] Safe Browsing check, [2] Update Chrome check, and [3] Passwords
  // Check, and notifies any observers of the change.
  //
  // NOTE: If the Safety Check is not currently running, does nothing.
  void StopSafetyCheck();

  // `IOSChromePasswordCheckManager::Observer` implementation.
  void PasswordCheckStatusChanged(PasswordCheckState state) override;
  void InsecureCredentialsChanged() override;
  void ManagerWillShutdown(
      IOSChromePasswordCheckManager* password_check_manager) override;

  // `OmahaServiceObserver` implementation.
  void UpgradeRecommendedDetailsChanged(
      UpgradeRecommendedDetails details) override;
  void ServiceWillShutdown(OmahaService* omaha_service) override;

  // Adds/removes an observer to be notified of PasswordSafetyCheckState,
  // SafeBrowsingSafetyCheckState, UpdateChromeSafetyCheckState, and
  // RunningSafetyCheckState events.
  void AddObserver(IOSChromeSafetyCheckManagerObserver* observer);
  void RemoveObserver(IOSChromeSafetyCheckManagerObserver* observer);

  // Returns the current state of the Safe Browsing check.
  SafeBrowsingSafetyCheckState GetSafeBrowsingCheckState() const;

  // Returns the current state of the Password check.
  PasswordSafetyCheckState GetPasswordCheckState() const;

  // Returns the current state of the Update Chrome check.
  UpdateChromeSafetyCheckState GetUpdateChromeCheckState() const;

  // Returns the current insecure password counts.
  password_manager::InsecurePasswordCounts GetInsecurePasswordCounts() const;

  // Returns the App Store Chrome upgrade URL.
  const GURL& GetChromeAppUpgradeUrl() const;

  // Returns the next Chrome app version.
  std::string GetChromeAppNextVersion() const;

  // Returns all insecure credentials that are present, provided by the Password
  // Check Manager.
  std::vector<password_manager::CredentialUIEntry> GetInsecureCredentials()
      const;

  // Returns the time of the last Safety Check run, if ever.
  base::Time GetLastSafetyCheckRunTime() const;

  // Ingests the Omaha response, `details`, to determine if the app is up to
  // date.
  //
  // If the app is up-to-date, calls `SetUpdateChromeCheckState()` to reflect
  // the new, updated state.
  //
  // If the app is outdated, sets `upgrade_url_` and `next_version_` to maintain
  // the upgrade details.
  void HandleOmahaResponse(UpgradeRecommendedDetails details);

  // For unit-testing only.
  void StartOmahaCheckForTesting();
  RunningSafetyCheckState GetRunningCheckStateForTesting() const;
  void SetPasswordCheckStateForTesting(PasswordSafetyCheckState state);
  void SetInsecurePasswordCountsForTesting(
      password_manager::InsecurePasswordCounts counts);
  void PasswordCheckStatusChangedForTesting(PasswordCheckState state);
  void InsecureCredentialsChangedForTesting();
  void RestorePreviousSafetyCheckStateForTesting();

 private:
  // Restores the Safety Check Manager with the previous check states, if any,
  // from Prefs.
  void RestorePreviousSafetyCheckState();

  // Starts the asynchronous Password check, and notifies any observers of the
  // change.
  void StartPasswordCheck();

  // Stops the currently running Password check, if any, and notifies any
  // observers of the change.
  void StopPasswordCheck();

  // Starts the asynchronous Update Chrome check, and notifies any observers of
  // the change.
  void StartUpdateChromeCheck();

  // Stops the currently running Update Chrome check, if any, and notifies any
  // observers of the change.
  void StopUpdateChromeCheck();

  // Sets `safe_browsing_check_state_` to `state` and notifies any observers
  // of the change.
  void SetSafeBrowsingCheckState(SafeBrowsingSafetyCheckState state);

  // Updates `password_check_state_` to `state` and notifies any observers
  // of the change.
  void SetPasswordCheckState(PasswordSafetyCheckState state);

  // Updates `insecure_password_counts_` to `counts`. Does not notify any
  // observers of the change.
  void SetInsecurePasswordCounts(
      password_manager::InsecurePasswordCounts counts);

  // Updates `update_chrome_check_state_` to `state` and notifies any observers
  // of the change.
  void SetUpdateChromeCheckState(UpdateChromeSafetyCheckState state);

  // Converts `state` (`PasswordCheckState`) to type
  // `PasswordSafetyCheckState`, then calls
  // `SetPasswordCheckState(PasswordSafetyCheckState state)`.
  void ConvertAndSetPasswordCheckState(PasswordCheckState state);

  // Updates `password_check_state_` to the latest, correct value based on
  // changes in the insecure credentials list. Then calls
  // `SetPasswordCheckState(PasswordSafetyCheckState state)`.
  //
  // NOTE: This method exists to cover an edge case where the insecure
  // credentials list may change while the Password check is currently running.
  void RefreshOutdatedPasswordCheckState();

  // Reads the latest Safe Browsing values from Prefs, and updates the internal
  // Safe Browsing check state.
  void UpdateSafeBrowsingCheckState();

  // Sets the app's upgrade details provided by the Omaha service.
  void SetUpdateChromeDetails(GURL upgrade_url, std::string next_version);

  // Starts an async request to the Omaha service for whether the current device
  // is up to date.
  //
  // NOTE: The response from the Omaha service is handled by
  // `HandleOmahaResponse()`.
  void StartOmahaCheck();

  // Checks if the Update Chrome check is still running after
  // `kOmahaNetworkWaitTime` has elapsed. If so, considers this an Omaha
  // error, and updates all relevant state to reflect the error. (If not, does
  // nothing.)
  //
  // NOTE: This method is called `kOmahaNetworkWaitTime` after
  // `StartOmahaCheck()`.
  void HandleOmahaError();

  // Checks if any checks are currently running. If so, sets
  // `running_safety_check_state_` to the running state, and notifies any
  // observers of the change.
  void RefreshSafetyCheckRunningState();

  // Logs the time of the current Safety Check run to Prefs.
  void LogCurrentSafetyCheckRunTime();

  // Current running state of the Safety Check. If any checks are currently
  // running (e.g. Safe Browsing check, Update Chrome check, Passwords Check),
  // then the Safety Check is considered to be running, too. For example:
  //
  //               (✓ = running, x = not running)
  //
  // +--------+----------+---------------+-------+--------------+
  // | Update | Password | Safe Browsing |       | Safety Check |
  // +--------+----------+---------------+-------+--------------+
  // |    ✓   |     ✓    |       ✓       |   =   |       ✓      |
  // +--------+----------+---------------+-------+--------------+
  // |    x   |     ✓    |       ✓       |   =   |       ✓      |
  // +--------+----------+---------------+-------+--------------+
  // |    ✓   |     x    |       ✓       |   =   |       ✓      |
  // +--------+----------+---------------+-------+--------------+
  // |    ✓   |     ✓    |       x       |   =   |       ✓      |
  // +--------+----------+---------------+-------+--------------+
  // |    x   |     x    |       ✓       |   =   |       ✓      |
  // +--------+----------+---------------+-------+--------------+
  // |    ✓   |     x    |       x       |   =   |       ✓      |
  // +--------+----------+---------------+-------+--------------+
  // |    x   |     ✓    |       x       |   =   |       ✓      |
  // +--------+----------+---------------+-------+--------------+
  // |    x   |     x    |       x       |   =   |       x      |
  // +--------+----------+---------------+-------+--------------+
  RunningSafetyCheckState running_safety_check_state_ =
      RunningSafetyCheckState::kDefault;

  // Current state of the Safe Browsing check.
  SafeBrowsingSafetyCheckState safe_browsing_check_state_ =
      SafeBrowsingSafetyCheckState::kDefault;

  // Current state of the Password check.
  PasswordSafetyCheckState password_check_state_ =
      PasswordSafetyCheckState::kDefault;

  // Previous state of the Password check. (Used as a fallback state, which
  // enables users to cancel a currently running Password check.)
  PasswordSafetyCheckState previous_password_check_state_ =
      PasswordSafetyCheckState::kDefault;

  // Current state of the Update Chrome check.
  UpdateChromeSafetyCheckState update_chrome_check_state_ =
      UpdateChromeSafetyCheckState::kDefault;

  // Previous state of the Update Chrome check. (Used as a fallback state, which
  // enables users to cancel a currently running Update Chrome check.)
  UpdateChromeSafetyCheckState previous_update_chrome_check_state_ =
      UpdateChromeSafetyCheckState::kDefault;

  // The count of passwords flagged as compromised, dismissed, reused, and weak
  // by the most recent Safety Check run.
  password_manager::InsecurePasswordCounts insecure_password_counts_ = {
      /* compromised */ 0, /* dismissed */ 0, /* reused */ 0,
      /* weak */ 0};

  // The count of passwords flagged as compromised, dismissed, reused, and weak
  // by the previous Safety Check run.
  password_manager::InsecurePasswordCounts previous_insecure_password_counts_ =
      {/* compromised */ 0, /* dismissed */ 0, /* reused */ 0,
       /* weak */ 0};

  // The last time the Safety Check was run, if ever.
  base::Time last_safety_check_run_time_;

  // If `ignore_omaha_changes_` is true when either
  // `HandleOmahaResponse()` or `HandleOmahaError()` are called, nothing
  // happens. Effectively, this enables users to cancel a currently running
  // Update Chrome check.
  //
  // NOTE: `ignore_omaha_changes_` is reset to false when the Safety Check is
  // run again.
  bool ignore_omaha_changes_ = false;

  // If `ignore_password_check_changes_` is true when Password Check Manager
  // observer methods are called, nothing happens. Effectively, this enables
  // users to cancel a currently running Password check.
  //
  // NOTE: `ignore_password_check_changes_` is reset to false when the Safety
  // Check is run again.
  bool ignore_password_check_changes_ = false;

  // The app upgrade URL generated by the Omaha service.
  //
  // NOTE: This may be an empty, invalid URL, which doesn't necessarily indicate
  // an issue. Rather, an empty, invalid URL likely means the URL was never set
  // because the app is already up to date.
  GURL upgrade_url_;

  // The app's next version generated by the Omaha service.
  //
  // NOTE: This may be empty, which doesn't necessarily indicate
  // an issue. Rather, it likely means the next version was never set
  // because the app is already up to date.
  std::string next_version_;

  // Observers to listen to Safety Check changes.
  base::ObserverList<IOSChromeSafetyCheckManagerObserver, true> observers_;

  // Weak pointer to the pref service, which checks the user's Enhanced Safe
  // Browsing state.
  raw_ptr<PrefService> pref_service_;

  // Weak pointer to the local-state pref service, which stores information
  // about the latest Safety Check run (e.g. the results of each check, the
  // timestamp of the run, etc.)
  raw_ptr<PrefService> local_pref_service_;

  // Refcounted pointer to the IOSChromeSafetyCheckManager to use.
  scoped_refptr<IOSChromePasswordCheckManager> password_check_manager_;

  // Registrar for pref changes notifications.
  PrefChangeRegistrar pref_change_registrar_;

  // Validates IOSChromeSafetyCheckManager::Observer events are evaluated on the
  // same sequence that IOSChromeSafetyCheckManager was created on.
  SEQUENCE_CHECKER(sequence_checker_);

  // Ensures IOSChromePasswordCheckManager::Observer events are posted on the
  // same sequence that IOSChromeSafetyCheckManager was created on.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<IOSChromeSafetyCheckManager> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SAFETY_CHECK_MODEL_IOS_CHROME_SAFETY_CHECK_MANAGER_H_
