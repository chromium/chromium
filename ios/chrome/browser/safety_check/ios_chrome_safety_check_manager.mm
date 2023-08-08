// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safety_check/ios_chrome_safety_check_manager.h"

#import <vector>

#import "base/functional/bind.h"
#import "base/location.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/omaha/omaha_service.h"
#import "ios/chrome/browser/safety_check/ios_chrome_safety_check_manager_utils.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/upgrade/upgrade_recommended_details.h"
#import "ios/chrome/browser/upgrade/upgrade_utils.h"
#import "ios/chrome/common/channel_info.h"

IOSChromeSafetyCheckManager::IOSChromeSafetyCheckManager(
    PrefService* pref_service,
    PrefService* local_pref_service,
    scoped_refptr<IOSChromePasswordCheckManager> password_check_manager,
    const scoped_refptr<base::SequencedTaskRunner> task_runner)
    : pref_service_(pref_service),
      local_pref_service_(local_pref_service),
      password_check_manager_(password_check_manager),
      task_runner_(task_runner) {
  CHECK(pref_service_);
  CHECK(local_pref_service_);
  CHECK(password_check_manager_);
  CHECK(task_runner_);

  password_check_manager_observation_.Observe(password_check_manager.get());

  pref_change_registrar_.Init(pref_service);

  pref_change_registrar_.Add(
      prefs::kSafeBrowsingEnabled,
      base::BindRepeating(
          &IOSChromeSafetyCheckManager::UpdateSafeBrowsingCheckState,
          weak_ptr_factory_.GetWeakPtr()));

  pref_change_registrar_.Add(
      prefs::kSafeBrowsingEnhanced,
      base::BindRepeating(
          &IOSChromeSafetyCheckManager::UpdateSafeBrowsingCheckState,
          weak_ptr_factory_.GetWeakPtr()));

  RestorePreviousSafetyCheckState();
}

IOSChromeSafetyCheckManager::~IOSChromeSafetyCheckManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observers_.empty());
}

void IOSChromeSafetyCheckManager::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : observers_) {
    observer.ManagerWillShutdown(this);
  }

  DCHECK(observers_.empty());

  pref_change_registrar_.RemoveAll();
  pref_service_ = nullptr;
  local_pref_service_ = nullptr;
}

void IOSChromeSafetyCheckManager::StartSafetyCheck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Do nothing if the Safety Check is already running.
  if (running_safety_check_state_ == RunningSafetyCheckState::kRunning) {
    return;
  }

  // Asynchronous checks
  StartPasswordCheck();
  StartUpdateChromeCheck();

  // Synchronous checks
  UpdateSafeBrowsingCheckState();
}

void IOSChromeSafetyCheckManager::StopSafetyCheck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Do nothing if the Safety Check is not running.
  if (running_safety_check_state_ != RunningSafetyCheckState::kRunning) {
    return;
  }

  StopPasswordCheck();
  StopUpdateChromeCheck();
}

void IOSChromeSafetyCheckManager::RestorePreviousSafetyCheckState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  absl::optional<SafeBrowsingSafetyCheckState> safe_browsing_check_state =
      SafeBrowsingSafetyCheckStateForName(local_pref_service_->GetString(
          prefs::kIosSafetyCheckManagerSafeBrowsingCheckResult));

  if (safe_browsing_check_state.has_value() &&
      safe_browsing_check_state.value() !=
          SafeBrowsingSafetyCheckState::kRunning) {
    SetSafeBrowsingCheckState(safe_browsing_check_state.value());
  }

  absl::optional<PasswordSafetyCheckState> password_check_state =
      PasswordSafetyCheckStateForName(local_pref_service_->GetString(
          prefs::kIosSafetyCheckManagerPasswordCheckResult));

  if (password_check_state.has_value() &&
      password_check_state.value() != PasswordSafetyCheckState::kRunning) {
    SetPasswordCheckState(password_check_state.value());
  }

  absl::optional<UpdateChromeSafetyCheckState> update_chrome_check_state =
      UpdateChromeSafetyCheckStateForName(local_pref_service_->GetString(
          prefs::kIosSafetyCheckManagerUpdateCheckResult));

  if (update_chrome_check_state.has_value() &&
      update_chrome_check_state.value() !=
          UpdateChromeSafetyCheckState::kRunning) {
    SetUpdateChromeCheckState(update_chrome_check_state.value());
  }
}

void IOSChromeSafetyCheckManager::StartPasswordCheck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Do nothing if the Password check is already running.
  if (password_check_state_ == PasswordSafetyCheckState::kRunning) {
    return;
  }

  ignore_password_check_changes_ = false;

  previous_password_check_state_ = password_check_state_;

  password_check_manager_->StartPasswordCheck();

  // NOTE: There's no need to explicitly set `password_check_state_` to
  // `kRunning` here because this class conforms to
  // `IOSChromePasswordCheckManager::Observer`. Whenever the observer method
  // `PasswordCheckStatusChanged()` is called with a running state,
  // `password_check_state_` will then be set to `kRunning`.
}

void IOSChromeSafetyCheckManager::StopPasswordCheck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Do nothing if the Password Check is not running.
  if (password_check_state_ != PasswordSafetyCheckState::kRunning) {
    return;
  }

  SetPasswordCheckState(previous_password_check_state_);

  ignore_password_check_changes_ = true;

  password_check_manager_->StopPasswordCheck();
}

void IOSChromeSafetyCheckManager::StartUpdateChromeCheck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Do nothing if the Update Chrome check is already running.
  if (update_chrome_check_state_ == UpdateChromeSafetyCheckState::kRunning) {
    return;
  }

  ignore_omaha_changes_ = false;

  previous_update_chrome_check_state_ = update_chrome_check_state_;

  StartOmahaCheck();
}

void IOSChromeSafetyCheckManager::StopUpdateChromeCheck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (update_chrome_check_state_ != UpdateChromeSafetyCheckState::kRunning) {
    return;
  }

  SetUpdateChromeCheckState(previous_update_chrome_check_state_);

  ignore_omaha_changes_ = true;
}

void IOSChromeSafetyCheckManager::PasswordCheckStatusChanged(
    PasswordCheckState state) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &IOSChromeSafetyCheckManager::ConvertAndSetPasswordCheckState,
          weak_ptr_factory_.GetWeakPtr(), std::move(state)));
}

void IOSChromeSafetyCheckManager::InsecureCredentialsChanged() {
  // The insecure credentials list may have changed while the Password check is
  // running, so schedule a refresh of the Password Check Status.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &IOSChromeSafetyCheckManager::RefreshOutdatedPasswordCheckState,
          weak_ptr_factory_.GetWeakPtr()));
}

SafeBrowsingSafetyCheckState
IOSChromeSafetyCheckManager::GetSafeBrowsingCheckState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return safe_browsing_check_state_;
}

PasswordSafetyCheckState IOSChromeSafetyCheckManager::GetPasswordCheckState()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return password_check_state_;
}

UpdateChromeSafetyCheckState
IOSChromeSafetyCheckManager::GetUpdateChromeCheckState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return update_chrome_check_state_;
}

// Returns the Chrome app upgrade URL for the App Store.
const GURL& IOSChromeSafetyCheckManager::GetChromeAppUpgradeUrl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return upgrade_url_;
}

std::vector<password_manager::CredentialUIEntry>
IOSChromeSafetyCheckManager::GetInsecureCredentials() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return password_check_manager_->GetInsecureCredentials();
}

// Returns the Chrome app next version.
std::string IOSChromeSafetyCheckManager::GetChromeAppNextVersion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return next_version_;
}

// TODO(crbug.com/1462786): Add UMA logs related to the Safe Browsing check.
void IOSChromeSafetyCheckManager::SetSafeBrowsingCheckState(
    SafeBrowsingSafetyCheckState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (safe_browsing_check_state_ == state) {
    return;
  }

  safe_browsing_check_state_ = state;

  local_pref_service_->SetString(
      prefs::kIosSafetyCheckManagerSafeBrowsingCheckResult,
      NameForSafetyCheckState(state));

  for (auto& observer : observers_) {
    observer.SafeBrowsingCheckStateChanged(safe_browsing_check_state_);
  }
}

// TODO(crbug.com/1462786): Add UMA logs related to the Password check.
void IOSChromeSafetyCheckManager::ConvertAndSetPasswordCheckState(
    PasswordCheckState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the Password check reports the device is offline, propogate this
  // information to the Update Chrome check.
  if (state == PasswordCheckState::kOffline) {
    SetUpdateChromeCheckState(UpdateChromeSafetyCheckState::kNetError);
  }

  const std::vector<password_manager::CredentialUIEntry> insecure_credentials =
      password_check_manager_->GetInsecureCredentials();

  PasswordSafetyCheckState check_state = CalculatePasswordSafetyCheckState(
      state, insecure_credentials, password_check_state_);

  SetPasswordCheckState(check_state);
}

// TODO(crbug.com/1462786): Add UMA logs related to the Password check.
void IOSChromeSafetyCheckManager::RefreshOutdatedPasswordCheckState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  PasswordCheckState state = password_check_manager_->GetPasswordCheckState();

  // If the Password check reports the device is offline, propogate this
  // information to the Update Chrome check.
  if (state == PasswordCheckState::kOffline) {
    SetUpdateChromeCheckState(UpdateChromeSafetyCheckState::kNetError);
  }

  const std::vector<password_manager::CredentialUIEntry> insecure_credentials =
      password_check_manager_->GetInsecureCredentials();

  PasswordSafetyCheckState check_state = CalculatePasswordSafetyCheckState(
      state, insecure_credentials, password_check_state_);

  SetPasswordCheckState(check_state);
}

// TODO(crbug.com/1462786): Add UMA logs related to the Password check.
void IOSChromeSafetyCheckManager::SetPasswordCheckState(
    PasswordSafetyCheckState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (password_check_state_ == state || ignore_password_check_changes_) {
    return;
  }

  password_check_state_ = state;

  local_pref_service_->SetString(
      prefs::kIosSafetyCheckManagerPasswordCheckResult,
      NameForSafetyCheckState(state));

  for (auto& observer : observers_) {
    observer.PasswordCheckStateChanged(password_check_state_);
  }

  RefreshSafetyCheckRunningState();
}

// TODO(crbug.com/1462786): Add UMA logs related to the Update Chrome check.
void IOSChromeSafetyCheckManager::SetUpdateChromeCheckState(
    UpdateChromeSafetyCheckState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (update_chrome_check_state_ == state || ignore_omaha_changes_) {
    return;
  }

  update_chrome_check_state_ = state;

  local_pref_service_->SetString(prefs::kIosSafetyCheckManagerUpdateCheckResult,
                                 NameForSafetyCheckState(state));

  for (auto& observer : observers_) {
    observer.UpdateChromeCheckStateChanged(update_chrome_check_state_);
  }

  RefreshSafetyCheckRunningState();
}

// TODO(crbug.com/1462786): Add UMA logs related to the Update Chrome check.
void IOSChromeSafetyCheckManager::SetUpdateChromeDetails(
    GURL upgrade_url,
    std::string next_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ignore_omaha_changes_) {
    return;
  }

  upgrade_url_ = upgrade_url;
  next_version_ = next_version;
}

// TODO(crbug.com/1462786): Add UMA logs related to the Safe Browsing check.
void IOSChromeSafetyCheckManager::UpdateSafeBrowsingCheckState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (pref_service_->IsManagedPreference(prefs::kSafeBrowsingEnabled)) {
    SetSafeBrowsingCheckState(SafeBrowsingSafetyCheckState::kManaged);
  } else if (pref_service_->GetBoolean(prefs::kSafeBrowsingEnabled)) {
    SetSafeBrowsingCheckState(SafeBrowsingSafetyCheckState::kSafe);
  } else {
    SetSafeBrowsingCheckState(SafeBrowsingSafetyCheckState::kUnsafe);
  }
}

// TODO(crbug.com/1462786): Add UMA logs related to the Update Chrome check.
void IOSChromeSafetyCheckManager::StartOmahaCheck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SetUpdateChromeCheckState(UpdateChromeSafetyCheckState::kRunning);

  // Only make Omaha requests on the proper channels.
  switch (::GetChannel()) {
    case version_info::Channel::STABLE:
    case version_info::Channel::BETA:
    case version_info::Channel::DEV:
    case version_info::Channel::CANARY: {
      OmahaService::CheckNow(
          base::BindOnce(&IOSChromeSafetyCheckManager::HandleOmahaResponse,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    }
    default:
      break;
  }

  // If the Omaha response isn't recieved after `kOmahaNetworkWaitTime`,
  // consider this an Omaha failure.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&IOSChromeSafetyCheckManager::HandleOmahaError,
                     weak_ptr_factory_.GetWeakPtr()),
      kOmahaNetworkWaitTime);
}

// NOTE: In a fast-follow CL, this method may expand to store Omaha results
// in Prefs or NSUserDefaults.
//
// For now, all Omaha data will be maintained in-memory and tied to the
// lifecycle of the this class.
void IOSChromeSafetyCheckManager::HandleOmahaResponse(
    UpgradeRecommendedDetails details) {
  UpdateChromeSafetyCheckState state = UpdateChromeSafetyCheckState::kDefault;

  if (details.is_up_to_date) {
    state = UpdateChromeSafetyCheckState::kUpToDate;
  } else if (!details.upgrade_url.is_valid() || details.next_version.empty() ||
             !base::Version(details.next_version).IsValid()) {
    state = UpdateChromeSafetyCheckState::kOmahaError;
  } else {
    state = UpdateChromeSafetyCheckState::kOutOfDate;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&IOSChromeSafetyCheckManager::SetUpdateChromeCheckState,
                     weak_ptr_factory_.GetWeakPtr(), std::move(state)));

  if (details.upgrade_url.is_valid() && !details.next_version.empty()) {
    GURL upgrade_url = details.upgrade_url;
    std::string next_version = details.next_version;

    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&IOSChromeSafetyCheckManager::SetUpdateChromeDetails,
                       weak_ptr_factory_.GetWeakPtr(), std::move(upgrade_url),
                       std::move(next_version)));
  }
}

// TODO(crbug.com/1462786): Add UMA logs related to the Update Chrome check.
void IOSChromeSafetyCheckManager::HandleOmahaError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (update_chrome_check_state_ == UpdateChromeSafetyCheckState::kRunning) {
    SetUpdateChromeCheckState(UpdateChromeSafetyCheckState::kOmahaError);
  }
}

void IOSChromeSafetyCheckManager::RefreshSafetyCheckRunningState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RunningSafetyCheckState current_state = running_safety_check_state_;

  const bool running_checks =
      safe_browsing_check_state_ == SafeBrowsingSafetyCheckState::kRunning ||
      password_check_state_ == PasswordSafetyCheckState::kRunning ||
      update_chrome_check_state_ == UpdateChromeSafetyCheckState::kRunning;

  RunningSafetyCheckState new_state = running_checks
                                          ? RunningSafetyCheckState::kRunning
                                          : RunningSafetyCheckState::kDefault;

  // Do nothing if the current and new states match, i.e. there's no need to
  // notify observers that nothing has changed.
  if (current_state == new_state) {
    return;
  }

  running_safety_check_state_ = new_state;

  for (auto& observer : observers_) {
    observer.RunningStateChanged(running_safety_check_state_);
  }
}

void IOSChromeSafetyCheckManager::AddObserver(
    IOSChromeSafetyCheckManagerObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.AddObserver(observer);
}

void IOSChromeSafetyCheckManager::RemoveObserver(
    IOSChromeSafetyCheckManagerObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.RemoveObserver(observer);
}

void IOSChromeSafetyCheckManager::StartOmahaCheckForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StartOmahaCheck();
}

void IOSChromeSafetyCheckManager::HandleOmahaResponseForTesting(
    UpgradeRecommendedDetails details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HandleOmahaResponse(details);
}

RunningSafetyCheckState
IOSChromeSafetyCheckManager::GetRunningCheckStateForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return running_safety_check_state_;
}

void IOSChromeSafetyCheckManager::SetPasswordCheckStateForTesting(
    PasswordSafetyCheckState state) {
  SetPasswordCheckState(state);
}

void IOSChromeSafetyCheckManager::InsecureCredentialsChangedForTesting() {
  InsecureCredentialsChanged();
}

void IOSChromeSafetyCheckManager::PasswordCheckStatusChangedForTesting(
    PasswordCheckState state) {
  PasswordCheckStatusChanged(state);
}

void IOSChromeSafetyCheckManager::RestorePreviousSafetyCheckStateForTesting() {
  RestorePreviousSafetyCheckState();
}
