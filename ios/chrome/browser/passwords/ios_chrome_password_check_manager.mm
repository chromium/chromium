// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"

#import <set>

#import "base/strings/utf_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/ui/credential_utils.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/passwords/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_bulk_leak_check_service_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/passwords/password_checkup_metrics.h"

namespace {
using password_manager::CredentialUIEntry;
using password_manager::InsecureType;
using password_manager::LeakCheckCredential;
using State = password_manager::BulkLeakCheckServiceInterface::State;

// Key used to attach UserData to a LeakCheckCredential.
constexpr char kPasswordCheckDataKey[] = "password-check-manager-data-key";
// Minimum time the check should be running.
constexpr base::TimeDelta kDelay = base::Seconds(3);

// Class which ensures that IOSChromePasswordCheckManager will stay alive
// until password check is completed even if class what initially created
// IOSChromePasswordCheckManager was destroyed.
class IOSChromePasswordCheckManagerHolder : public LeakCheckCredential::Data {
 public:
  explicit IOSChromePasswordCheckManagerHolder(
      scoped_refptr<IOSChromePasswordCheckManager> manager)
      : manager_(std::move(manager)) {}
  ~IOSChromePasswordCheckManagerHolder() override = default;

  std::unique_ptr<Data> Clone() override {
    return std::make_unique<IOSChromePasswordCheckManagerHolder>(manager_);
  }

 private:
  scoped_refptr<IOSChromePasswordCheckManager> manager_;
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
  NOTREACHED();
  return PasswordCheckState::kIdle;
}
}  // namespace

IOSChromePasswordCheckManager::IOSChromePasswordCheckManager(
    scoped_refptr<password_manager::PasswordStoreInterface> profile_store,
    scoped_refptr<password_manager::PasswordStoreInterface> account_store,
    password_manager::AffiliationService* affiliation_service,
    password_manager::BulkLeakCheckServiceInterface* bulk_leak_check_service,
    PrefService* user_prefs)
    : profile_store_(profile_store),
      account_store_(account_store),
      saved_passwords_presenter_(affiliation_service,
                                 profile_store_,
                                 account_store_),
      insecure_credentials_manager_(&saved_passwords_presenter_,
                                    profile_store_,
                                    account_store_),
      bulk_leak_check_service_adapter_(&saved_passwords_presenter_,
                                       bulk_leak_check_service,
                                       user_prefs),
      user_prefs_(user_prefs) {
  observed_saved_passwords_presenter_.Observe(&saved_passwords_presenter_);

  observed_insecure_credentials_manager_.Observe(
      &insecure_credentials_manager_);

  observed_bulk_leak_check_service_.Observe(bulk_leak_check_service);

  // Instructs the presenter and manager to initialize and build their caches.
  saved_passwords_presenter_.Init();
}

IOSChromePasswordCheckManager::~IOSChromePasswordCheckManager() {
  DCHECK(observers_.empty());
}

void IOSChromePasswordCheckManager::StartPasswordCheck() {
  if (is_initialized_) {
    IOSChromePasswordCheckManagerHolder data(
        scoped_refptr<IOSChromePasswordCheckManager>(this));
    bulk_leak_check_service_adapter_.StartBulkLeakCheck(kPasswordCheckDataKey,
                                                        &data);

    if (password_manager::features::IsPasswordCheckupEnabled()) {
      insecure_credentials_manager_.StartWeakCheck(base::BindOnce(
          &IOSChromePasswordCheckManager::OnWeakOrReuseCheckFinished,
          weak_ptr_factory_.GetWeakPtr()));

      insecure_credentials_manager_.StartReuseCheck(base::BindOnce(
          &IOSChromePasswordCheckManager::OnWeakOrReuseCheckFinished,
          weak_ptr_factory_.GetWeakPtr()));
    }

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
  if (saved_passwords_presenter_.GetSavedPasswords().empty()) {
    return PasswordCheckState::kNoPasswords;
  }
  return ConvertBulkCheckState(
      bulk_leak_check_service_adapter_.GetBulkLeakCheckState());
}

absl::optional<base::Time>
IOSChromePasswordCheckManager::GetLastPasswordCheckTime() const {
  if (!user_prefs_->HasPrefPath(
          password_manager::prefs::kLastTimePasswordCheckCompleted)) {
    return last_completed_weak_or_reuse_check_;
  }

  base::Time last_password_check =
      base::Time::FromDoubleT(user_prefs_->GetDouble(
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

void IOSChromePasswordCheckManager::OnSavedPasswordsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  // Observing saved passwords to update possible kNoPasswords state.
  NotifyPasswordCheckStatusChanged();
  if (!std::exchange(is_initialized_, true) && start_check_on_init_) {
    StartPasswordCheck();
  }
}

void IOSChromePasswordCheckManager::OnInsecureCredentialsChanged() {
  for (auto& observer : observers_) {
    observer.InsecureCredentialsChanged();
  }
}

void IOSChromePasswordCheckManager::OnStateChanged(State state) {
  if (state == State::kIdle && is_check_running_) {
    // Saving time of last successful password check
    user_prefs_->SetDouble(
        password_manager::prefs::kLastTimePasswordCheckCompleted,
        base::Time::Now().ToDoubleT());
    user_prefs_->SetTime(
        password_manager::prefs::kSyncedLastTimePasswordCheckCompleted,
        base::Time::Now());

    LogInsecureCredentialsCountMetrics();
  }
  if (state != State::kRunning) {
    // If check was running
    if (is_check_running_) {
      const base::TimeDelta elapsed = base::Time::Now() - start_time_;
      if (elapsed < kDelay) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&IOSChromePasswordCheckManager::
                               NotifyPasswordCheckStatusChanged,
                           weak_ptr_factory_.GetWeakPtr()),
            kDelay - elapsed);
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
    insecure_credentials_manager_.SaveInsecureCredential(credential);
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
