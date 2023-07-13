// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFETY_CHECK_IOS_CHROME_SAFETY_CHECK_MANAGER_H_
#define IOS_CHROME_BROWSER_SAFETY_CHECK_IOS_CHROME_SAFETY_CHECK_MANAGER_H_

#import "base/memory/weak_ptr.h"
#import "base/observer_list.h"
#import "base/observer_list_types.h"
#import "base/sequence_checker.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/prefs/pref_change_registrar.h"
#import "ios/chrome/browser/safety_check/ios_chrome_safety_check_manager_constants.h"

class IOSChromeSafetyCheckManager;
class PrefService;

// All IOSChromeSafetyCheckManagerObserver events will be evaluated on the
// same sequence the IOSChromeSafetyCheckManager is created on.
class IOSChromeSafetyCheckManagerObserver : public base::CheckedObserver {
 public:
  // Called whenever the Safety Check determines a change in the Password check
  // state (i.e. when user has reused passwords, weak passwords, no compromised
  // password, etc.)
  virtual void PasswordCheckStateChanged(PasswordSafetyCheckState state) {}
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
// 1. Observing changes in the password check status and compromised credentials
// list.
// 2. Determining if the user has Enhanced Safe Browsing enabled.
// 3. Determining if the browser is up-to-date.
// 4. Determining if the Safety Check is currently running.
// 5. Determining if the previous Safety Check run found any issues.
//
// This class notifies its observers (`IOSChromeSafetyCheckManagerObserver`)
// when state from the above list change.
class IOSChromeSafetyCheckManager : public KeyedService {
 public:
  explicit IOSChromeSafetyCheckManager(PrefService* pref_service);

  IOSChromeSafetyCheckManager(const IOSChromeSafetyCheckManager&) = delete;
  IOSChromeSafetyCheckManager& operator=(const IOSChromeSafetyCheckManager&) =
      delete;

  ~IOSChromeSafetyCheckManager() override;

  // KeyedService implementation.
  void Shutdown() override;

  // Adds/removes an observer to be notified of PasswordSafetyCheckState,
  // SafeBrowsingSafetyCheckState, UpdateChromeSafetyCheckState, and
  // RunningSafetyCheckState events.
  void AddObserver(IOSChromeSafetyCheckManagerObserver* observer);
  void RemoveObserver(IOSChromeSafetyCheckManagerObserver* observer);

  // Returns the current state of the Safe Browsing check.
  SafeBrowsingSafetyCheckState GetSafeBrowsingCheckState() const;

 private:
  // Sets `safe_browsing_check_state_` to `state` and notifies any observers
  // of the change.
  void SetSafeBrowsingCheckState(SafeBrowsingSafetyCheckState state);

  // Called when a Safe Browsing pref value changes.
  void OnSafeBrowsingPrefChanged();

  // Observers to listen to Safety Check changes.
  base::ObserverList<IOSChromeSafetyCheckManagerObserver> observers_;

  // Current state of the Safe Browsing check.
  SafeBrowsingSafetyCheckState safe_browsing_check_state_ =
      SafeBrowsingSafetyCheckState::kDefault;

  // Weak pointer to the pref service, which checks the user's Enhanced Safe
  // Browsing state.
  raw_ptr<PrefService> pref_service_;

  // Registrar for pref changes notifications.
  PrefChangeRegistrar pref_change_registrar_;

  // Validates IOSChromeSafetyCheckManager::Observer events are evaluated on the
  // same sequence that IOSChromeSafetyCheckManager was created on.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IOSChromeSafetyCheckManager> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SAFETY_CHECK_IOS_CHROME_SAFETY_CHECK_MANAGER_H_
