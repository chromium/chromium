// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"

#import <set>

#import "base/strings/utf_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/ui/credential_utils.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_bulk_leak_check_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/password_checkup_metrics.h"

namespace {
using password_manager::CredentialUIEntry;
using password_manager::InsecureType;
using password_manager::LeakCheckCredential;
using State = password_manager::BulkLeakCheckServiceInterface::State;

// Key used to attach UserData to a LeakCheckCredential.
constexpr char kPasswordCheckDataKey[] = "password-check-manager-data-key";

// Class which ensures that IOSChromePasswordCheckManager will stay alive
// until password check is completed even if class what initially created
// IOSChromePasswordCheckManager was destroyed.
class IOSChromePasswordCheckManagerHolder : public LeakCheckCredential::Data {
 public:
  IOSChromePasswordCheckManagerHolder(
      scoped_refptr<IOSChromePasswordCheckManager> manager,
      password_manager::TriggerBackendNotification should_trigger_notification)
      : manager_(std::move(manager)),
        should_trigger_notification_(should_trigger_notification) {}
  ~IOSChromePasswordCheckManagerHolder() override = default;

  std::unique_ptr<Data> Clone() override {
    return std::make_unique<IOSChromePasswordCheckManagerHolder>(
        manager_, should_trigger_notification_);
  }

  password_manager::TriggerBackendNotification should_trigger_notification()
      const {
    return should_trigger_notification_;
  }

 private:
  scoped_refptr<IOSChromePasswordCheckManager> manager_;
  // Certain client use cases require to notify backend if new leaked
  // credentials are found. This member indicate whether that should happen.
  const password_manager::TriggerBackendNotification
      should_trigger_notification_;
};

PasswordCheckState ConvertBulkCheckState(State state) {
  switch (state) {
    case State::kIdle:
      return PasswordCheckState::kIdle;
    case State::kRunning:
      return PasswordCheckState::kRunning;
    case State::kSignedOut:
      return PasswordCheckState::kSignedOut;
    case State::kNetworkError:
      return PasswordCheckState::kOffline;
    case State::kQuotaLimit:
      return PasswordCheckState::kQuotaLimit;
    case State::kCanceled:
      return PasswordCheckState::kCanceled;
    case State::kTokenRequestFailure:
    case State::kHashingFailure:
    case State::kServiceError:
      return PasswordCheckState::kOther;
  }
  NOTREACHED_IN_MIGRATION();
  return PasswordCheckState::kIdle;
}
}  // namespace

IOSChromePasswordCheckManager::IOSChromePasswordCheckManager(
    PrefService* user_prefs,
    password_manager::BulkLeakCheckServiceInterface* bulk_leak_check_service,
    std::unique_ptr<password_manager::SavedPasswordsPresenter>
        saved_passwords_presenter)
    : saved_passwords_presenter_(std::move(saved_passwords_presenter)),
      insecure_credentials_manager_(saved_passwords_presenter_.get()),
      bulk_leak_check_service_adapter_(saved_passwords_presenter_.get(),
                                       bulk_leak_check_service,
                                       user_prefs),
      user_prefs_(user_prefs) {
  observed_saved_passwords_presenter_.Observe(saved_passwords_presenter_.get());

  observed_insecure_credentials_manager_.Observe(
      &insecure_credentials_manager_);

  observed_bulk_leak_check_service_.Observe(bulk_leak_check_service);

  // Instructs the presenter and manager to initialize and build their caches.
  saved_passwords_presenter_->Init();
}

IOSChromePasswordCheckManager::~IOSChromePasswordCheckManager() {
  for (auto& observer : observers_) {
    observer.ManagerWillShutdown(this);
  }

  DCHECK(observers_.empty());
}

void IOSChromePasswordCheckManager::StartPasswordCheck(
    password_manager::LeakDetectionInitiator initiator) {
  // Calls to StartPasswordCheck() will be only processed after
  // OnSavedPasswordsChanged() is called. Meaning that all client calls
  // happening before that will be stored in memory until all conditions are
  // met. Thus initiator value must be stored to ensure that when this method is
  // run, it has the correct value.
  password_check_initiator_ = initiator;

  if (is_initialized_) {
    IOSChromePasswordCheckManagerHolder data(
        scoped_refptr<IOSChromePasswordCheckManager>(this),
        password_manager::ShouldTriggerBackendNotificationForInitiator(
            password_check_initiator_));
    bulk_leak_check_service_adapter_.StartBulkLeakCheck(
        password_check_initiator_, kPasswordCheckDataKey, &data);

    insecure_credentials_manager_.StartWeakCheck(base::BindOnce(
        &IOSChromePasswordCheckManager::OnWeakOrReuseCheckFinished,
        weak_ptr_factory_.GetWeakPtr()));

    insecure_credentials_manager_.StartReuseCheck(base::BindOnce(
        &IOSChromePasswordCheckManager::OnWeakOrReuseCheckFinished,
        weak_ptr_factory_.GetWeakPtr()));

    is_check_running_ = true;
    start_time_ = base::Time::Now();
  } else {
    start_check_on_init_ = true;
  }
}

void IOSChromePasswordCheckManager::StopPasswordCheck() {
  bulk_leak_check_service_adapter_.StopBulkLeakCheck();
  is_check_running_ = false;
}

PasswordCheckState IOSChromePasswordCheckManager::GetPasswordCheckState()
    const {
  if (saved_passwords_presenter_->GetSavedPasswords().empty()) {
    return PasswordCheckState::kNoPasswords;
  }
  return ConvertBulkCheckState(
      bulk_leak_check_service_adapter_.GetBulkLeakCheckState());
}

std::optional<base::Time>
IOSChromePasswordCheckManager::GetLastPasswordCheckTime() const {
  if (!user_prefs_->HasPrefPath(
          password_manager::prefs::kLastTimePasswordCheckCompleted)) {
    return last_completed_weak_or_reuse_check_;
  }

  base::Time last_password_check =
      base::Time::FromSecondsSinceUnixEpoch(user_prefs_->GetDouble(
          password_manager::prefs::kLastTimePasswordCheckCompleted));

  if (!last_completed_weak_or_reuse_check_.has_value()) {
    return last_password_check;
  }

  return std::max(last_password_check,
                  last_completed_weak_or_reuse_check_.value());
}

std::vector<CredentialUIEntry>
IOSChromePasswordCheckManager::GetInsecureCredentials() const {
  return insecure_credentials_manager_.GetInsecureCredentialEntries();
}

void IOSChromePasswordCheckManager::ShutdownOnUIThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : observers_) {
    observer.ManagerWillShutdown(this);
  }

  DCHECK(observers_.empty());

  observed_bulk_leak_check_service_.Reset();
  observed_insecure_credentials_manager_.Reset();
  observed_saved_passwords_presenter_.Reset();
}

void IOSChromePasswordCheckManager::OnSavedPasswordsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  // Observing saved passwords to update possible kNoPasswords state.
  NotifyPasswordCheckStatusChanged();
  if (!std::exchange(is_initialized_, true) && start_check_on_init_) {
    StartPasswordCheck(password_manager::LeakDetectionInitiator::kEditCheck);
  }
}

void IOSChromePasswordCheckManager::OnInsecureCredentialsChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : observers_) {
    observer.InsecureCredentialsChanged();
  }
}

void IOSChromePasswordCheckManager::OnStateChanged(State state) {
  if (state == State::kIdle && is_check_running_) {
    // Saving time of last successful password check
    user_prefs_->SetDouble(
        password_manager::prefs::kLastTimePasswordCheckCompleted,
        base::Time::Now().InSecondsFSinceUnixEpoch());

    LogInsecureCredentialsCountMetrics();
  }
  if (state != State::kRunning) {
    // If check was running
    if (is_check_running_) {
      const base::TimeDelta elapsed = base::Time::Now() - start_time_;
      const base::TimeDelta minimum_duration =
          tests_hook::PasswordCheckMinimumDuration();
      if (elapsed < minimum_duration) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&IOSChromePasswordCheckManager::
                               NotifyPasswordCheckStatusChanged,
                           weak_ptr_factory_.GetWeakPtr()),
            minimum_duration - elapsed);
        is_check_running_ = false;
        return;
      }
    }
    is_check_running_ = false;
  }
  NotifyPasswordCheckStatusChanged();
}

void IOSChromePasswordCheckManager::OnCredentialDone(
    const LeakCheckCredential& credential,
    password_manager::IsLeaked is_leaked) {
  if (is_leaked) {
    password_manager::TriggerBackendNotification should_trigger_notification =
        credential.GetUserData(kPasswordCheckDataKey)
            ? static_cast<IOSChromePasswordCheckManagerHolder*>(
                  credential.GetUserData(kPasswordCheckDataKey))
                  ->should_trigger_notification()
            : password_manager::TriggerBackendNotification(false);
    insecure_credentials_manager_.SaveInsecureCredential(
        credential, should_trigger_notification);
  }
}

void IOSChromePasswordCheckManager::OnBulkCheckServiceShutDown() {
  observed_bulk_leak_check_service_.Reset();
}

void IOSChromePasswordCheckManager::OnWeakOrReuseCheckFinished() {
  last_completed_weak_or_reuse_check_ = base::Time::Now();
  NotifyPasswordCheckStatusChanged();
}

void IOSChromePasswordCheckManager::NotifyPasswordCheckStatusChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : observers_) {
    observer.PasswordCheckStatusChanged(GetPasswordCheckState());
  }
}

void IOSChromePasswordCheckManager::MuteCredential(
    const CredentialUIEntry& credential) {
  insecure_credentials_manager_.MuteCredential(credential);
}

void IOSChromePasswordCheckManager::UnmuteCredential(
    const CredentialUIEntry& credential) {
  insecure_credentials_manager_.UnmuteCredential(credential);
}

void IOSChromePasswordCheckManager::LogInsecureCredentialsCountMetrics() {
  std::vector<CredentialUIEntry> insecure_credentials =
      GetInsecureCredentials();
  std::set<std::pair<std::u16string, std::u16string>> unique_entries;
  std::set<std::pair<std::u16string, std::u16string>> unique_unmuted_entries;

  for (const auto& credential : insecure_credentials) {
    unique_entries.insert({credential.username, credential.password});
    for (const auto& [insecure_type, insecure_metadata] :
         credential.password_issues) {
      if (!insecure_metadata.is_muted.value()) {
        unique_unmuted_entries.insert(
            {credential.username, credential.password});
        break;
      }
    }
  }

  password_manager::LogCountOfInsecureUsernamePasswordPairs(
      unique_entries.size());
  password_manager::LogCountOfUnmutedInsecureUsernamePasswordPairs(
      unique_unmuted_entries.size());
}
