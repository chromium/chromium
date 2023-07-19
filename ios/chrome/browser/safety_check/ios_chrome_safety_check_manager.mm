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
#import "ios/chrome/browser/safety_check/ios_chrome_safety_check_manager_utils.h"

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
