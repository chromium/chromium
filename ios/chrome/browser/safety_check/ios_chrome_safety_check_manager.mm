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
#import "ios/chrome/browser/upgrade/upgrade_recommended_details.h"
#import "ios/chrome/browser/upgrade/upgrade_utils.h"
#import "ios/chrome/common/channel_info.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSChromeSafetyCheckManager::IOSChromeSafetyCheckManager(
    PrefService* pref_service,
    scoped_refptr<IOSChromePasswordCheckManager> password_check_manager,
    const scoped_refptr<base::SequencedTaskRunner> task_runner)
    : pref_service_(pref_service),
      password_check_manager_(password_check_manager),
      task_runner_(task_runner) {
  CHECK(pref_service_);
  CHECK(password_check_manager_);
  CHECK(task_runner_);

  password_check_manager_observation_.Observe(password_check_manager.get());

  pref_change_registrar_.Init(pref_service);

  pref_change_registrar_.Add(
      prefs::kSafeBrowsingEnabled,
      base::BindRepeating(
          &IOSChromeSafetyCheckManager::OnSafeBrowsingPrefChanged,
          weak_ptr_factory_.GetWeakPtr()));

  pref_change_registrar_.Add(
      prefs::kSafeBrowsingEnhanced,
      base::BindRepeating(
          &IOSChromeSafetyCheckManager::OnSafeBrowsingPrefChanged,
          weak_ptr_factory_.GetWeakPtr()));

  // Process the initial Safe Browsing pref values.
  OnSafeBrowsingPrefChanged();

  // Query the Omaha service to process the initial Update Chrome check state.
  StartOmahaCheck();
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

// TODO(crbug.com/1462786): Add UMA logs related to the Safe Browsing check.
void IOSChromeSafetyCheckManager::SetSafeBrowsingCheckState(
    SafeBrowsingSafetyCheckState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  safe_browsing_check_state_ = state;

  for (auto& observer : observers_) {
    observer.SafeBrowsingCheckStateChanged(safe_browsing_check_state_);
  }
}

// TODO(crbug.com/1462786): Add UMA logs related to the Password check.
void IOSChromeSafetyCheckManager::ConvertAndSetPasswordCheckState(
    PasswordCheckState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(password_check_manager_);

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
  CHECK(password_check_manager_);

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
  password_check_state_ = state;

  for (auto& observer : observers_) {
    observer.PasswordCheckStateChanged(password_check_state_);
  }
}

// TODO(crbug.com/1462786): Add UMA logs related to the Update Chrome check.
void IOSChromeSafetyCheckManager::SetUpdateChromeCheckState(
    UpdateChromeSafetyCheckState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  update_chrome_check_state_ = state;

  for (auto& observer : observers_) {
    observer.UpdateChromeCheckStateChanged(update_chrome_check_state_);
  }
}

// TODO(crbug.com/1462786): Add UMA logs related to the Update Chrome check.
void IOSChromeSafetyCheckManager::SetUpdateChromeDetails(
    GURL upgrade_url,
    std::string next_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  upgrade_url_ = upgrade_url;
  next_version_ = next_version;
}

// TODO(crbug.com/1462786): Add UMA logs related to the Safe Browsing check.
void IOSChromeSafetyCheckManager::OnSafeBrowsingPrefChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(pref_service_);

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
