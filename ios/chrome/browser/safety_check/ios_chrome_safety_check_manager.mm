// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safety_check/ios_chrome_safety_check_manager.h"

#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSChromeSafetyCheckManager::IOSChromeSafetyCheckManager(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  CHECK(pref_service_);

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

SafeBrowsingSafetyCheckState
IOSChromeSafetyCheckManager::GetSafeBrowsingCheckState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return safe_browsing_check_state_;
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
