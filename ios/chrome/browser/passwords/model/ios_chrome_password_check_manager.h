// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_CHECK_MANAGER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_CHECK_MANAGER_H_

#include <optional>

#import "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"
#include "components/password_manager/core/browser/ui/bulk_leak_check_service_adapter.h"
#include "components/password_manager/core/browser/ui/credential_utils.h"
#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

class IOSChromePasswordCheckManager;
class PrefService;

// Enum which represents possible states of Password Check on UI.
// It's created based on BulkLeakCheckService::State.
enum class PasswordCheckState {
  kCanceled,
  kIdle,
  kNoPasswords,
  kOffline,
  kOther,
  kQuotaLimit,
  kRunning,
  kSignedOut,
};

// This class handles the bulk password check feature.
class IOSChromePasswordCheckManager final
    : public RefcountedKeyedService,
      public password_manager::SavedPasswordsPresenter::Observer,
      public password_manager::InsecureCredentialsManager::Observer,
      public password_manager::BulkLeakCheckServiceInterface::Observer {
 public:
  // Observer of IOSChromePasswordCheckManager.
  class Observer : public base::CheckedObserver {
   public:
    // Notifies the observer that the password check status has changed to
    // `state`.
    virtual void PasswordCheckStatusChanged(PasswordCheckState state) {}
    // Notifies the observer that the list of insecure credentials has changed.
    virtual void InsecureCredentialsChanged() {}
    // Notifies the observer that the `password_check_manager` is about to shut
    // down. Observers should remove themselves from the manager using
    // `password_check_manager->RemoveObserver(...)` at this time.
    virtual void ManagerWillShutdown(
        IOSChromePasswordCheckManager* password_check_manager) {}
  };

  explicit IOSChromePasswordCheckManager(
      PrefService* user_prefs,
      password_manager::BulkLeakCheckServiceInterface* bulk_leak_check_service,
      std::unique_ptr<password_manager::SavedPasswordsPresenter>
          saved_passwords_presenter);

  // Requests to start a check for insecure passwords.
  void StartPasswordCheck(password_manager::LeakDetectionInitiator initiator);

  // Stops checking for insecure passwords.
  void StopPasswordCheck();

  // Returns the current state of the password check.
  PasswordCheckState GetPasswordCheckState() const;

  // The elapsed time since one of the insecure checks was last performed.
  std::optional<base::Time> GetLastPasswordCheckTime() const;

  // Obtains all insecure credentials that are present in the password store.
  std::vector<password_manager::CredentialUIEntry> GetInsecureCredentials()
      const;

  // RefCountedKeyedService
  void ShutdownOnUIThread() final;

  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }
  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  password_manager::SavedPasswordsPresenter* GetSavedPasswordsPresenter() {
    return saved_passwords_presenter_.get();
  }

  // Mutes the provided compromised credential.
  void MuteCredential(const password_manager::CredentialUIEntry& credential);

  // Unmutes the provided muted compromised credential.
  void UnmuteCredential(const password_manager::CredentialUIEntry& credential);

  base::WeakPtr<IOSChromePasswordCheckManager> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  ~IOSChromePasswordCheckManager() override;

  // password_manager::SavedPasswordsPresenter::Observer:
  void OnSavedPasswordsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;

  // password_manager::InsecureCredentialsManager::Observer:
  void OnInsecureCredentialsChanged() override;

  // password_manager::BulkLeakCheckServiceInterface::Observer:
  void OnStateChanged(
      password_manager::BulkLeakCheckServiceInterface::State state) override;
  void OnCredentialDone(const password_manager::LeakCheckCredential& credential,
                        password_manager::IsLeaked is_leaked) override;
  void OnBulkCheckServiceShutDown() override;

  void OnWeakOrReuseCheckFinished();

  void NotifyPasswordCheckStatusChanged();

  // Logs counts of insecure credentials after each password check.
  void LogInsecureCredentialsCountMetrics();

  // Remembers whether a password check is running right now.
  bool is_check_running_ = false;

  // Used by `insecure_credentials_manager_` to obtain the list of saved
  // passwords.
  std::unique_ptr<password_manager::SavedPasswordsPresenter>
      saved_passwords_presenter_;

  // Used to obtain the list of insecure credentials.
  password_manager::InsecureCredentialsManager insecure_credentials_manager_;

  // Adapter used to start, monitor and stop a bulk leak check.
  password_manager::BulkLeakCheckServiceAdapter
      bulk_leak_check_service_adapter_;

  // Boolean that remembers whether the delegate is initialized. This is done
  // when the delegate obtains the list of saved passwords for the first time.
  bool is_initialized_ = false;

  // Boolean that indicate whether Password Check should be started right after
  // delegate is initialized.
  bool start_check_on_init_ = false;

  // Time when password check was started. Used to calculate delay in case
  // when password check run less than 3 seconds.
  base::Time start_time_;

  // Store when the last weak or reuse check was completed.
  std::optional<base::Time> last_completed_weak_or_reuse_check_;

  // Pref service.
  const raw_ptr<PrefService> user_prefs_;

  // This indicate what was the reason to start the password check.
  password_manager::LeakDetectionInitiator password_check_initiator_ =
      password_manager::LeakDetectionInitiator::kClientUseCaseUnspecified;

  // A scoped observer for `saved_passwords_presenter_`.
  base::ScopedObservation<password_manager::SavedPasswordsPresenter,
                          password_manager::SavedPasswordsPresenter::Observer>
      observed_saved_passwords_presenter_{this};

  // A scoped observer for `insecure_credentials_manager_`.
  base::ScopedObservation<
      password_manager::InsecureCredentialsManager,
      password_manager::InsecureCredentialsManager::Observer>
      observed_insecure_credentials_manager_{this};

  // A scoped observer for the BulkLeakCheckService.
  base::ScopedObservation<
      password_manager::BulkLeakCheckServiceInterface,
      password_manager::BulkLeakCheckServiceInterface::Observer>
      observed_bulk_leak_check_service_{this};

  // Observers to listen to password check changes.
  base::ObserverList<Observer, true> observers_;

  // Validates IOSChromePasswordCheckManager::Observer events are evaluated on
  // the same sequence that IOSChromePasswordCheckManager was created on.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IOSChromePasswordCheckManager> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_CHECK_MANAGER_H_
