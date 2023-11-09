// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/system_identity_manager.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"

namespace {

using CapabilityResult = SystemIdentityManager::CapabilityResult;

// Helper function used to extract the capability from `capabilities` in
// `CanOfferExtendedSyncPromos` and `IsSubjectToParentalControls`.
CapabilityResult FetchCapabilityCompleted(
    std::map<std::string, CapabilityResult> capabilities) {
  DCHECK_EQ(capabilities.size(), 1u);
  return capabilities.begin()->second;
}

}  // anonymous namespace

SystemIdentityManager::SystemIdentityManager() = default;

SystemIdentityManager::~SystemIdentityManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SystemIdentityManager::CanOfferExtendedSyncPromos(
    id<SystemIdentity> identity,
    FetchCapabilityCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FetchCapabilities(
      identity, {kCanOfferExtendedChromeSyncPromosCapabilityName},
      base::BindOnce(&FetchCapabilityCompleted).Then(std::move(callback)));
}

void SystemIdentityManager::IsSubjectToParentalControls(
    id<SystemIdentity> identity,
    FetchCapabilityCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FetchCapabilities(
      identity, {kIsSubjectToParentalControlsCapabilityName},
      base::BindOnce(&FetchCapabilityCompleted).Then(std::move(callback)));
}

void SystemIdentityManager::AddObserver(
    SystemIdentityManagerObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void SystemIdentityManager::RemoveObserver(
    SystemIdentityManagerObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void SystemIdentityManager::FireIdentityListChanged(bool notify_user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers_) {
    observer.OnIdentityListChanged(notify_user);
  }
}

void SystemIdentityManager::FireIdentityUpdated(id<SystemIdentity> identity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers_) {
    observer.OnIdentityUpdated(identity);
  }
}

void SystemIdentityManager::FireIdentityAccessTokenRefreshFailed(
    id<SystemIdentity> identity,
    id<RefreshAccessTokenError> error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers_) {
    observer.OnIdentityAccessTokenRefreshFailed(identity, error);
  }
}
