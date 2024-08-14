// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/system_identity_manager.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"

namespace {

using CapabilityResult = SystemIdentityManager::CapabilityResult;
using DismissViewCallback = SystemIdentityManager::DismissViewCallback;

// Helper function used to extract the capability from `capabilities` in
// `CanShowHistorySyncOptInsWithoutMinorModeRestrictions()` and
// `IsSubjectToParentalControls`.
CapabilityResult FetchCapabilityCompleted(
    std::map<std::string, CapabilityResult> capabilities) {
  DCHECK_EQ(capabilities.size(), 1u);
  return capabilities.begin()->second;
}

}  // anonymous namespace

SystemIdentityManager::PresentDialogConfiguration::
    PresentDialogConfiguration() {}

SystemIdentityManager::PresentDialogConfiguration::
    ~PresentDialogConfiguration() {}

SystemIdentityManager::PresentDialogConfiguration::PresentDialogConfiguration(
    SystemIdentityManager::PresentDialogConfiguration&& configuration) {
  identity = configuration.identity;
  view_controller = configuration.view_controller;
  animated = configuration.animated;
  dismissal_completion = std::move(configuration.dismissal_completion);
}

SystemIdentityManager::SystemIdentityManager() = default;

SystemIdentityManager::~SystemIdentityManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SystemIdentityManager::
    CanShowHistorySyncOptInsWithoutMinorModeRestrictions(
        id<SystemIdentity> identity,
        FetchCapabilityCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FetchCapabilities(
      identity,
      {kCanShowHistorySyncOptInsWithoutMinorModeRestrictionsCapabilityName},
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

DismissViewCallback SystemIdentityManager::PresentAccountDetailsController(
    id<SystemIdentity> identity,
    UIViewController* view_controller,
    bool animated,
    base::OnceClosure dismissal_completion) {
  SystemIdentityManager::PresentDialogConfiguration configuration;
  configuration.identity = identity;
  configuration.view_controller = view_controller;
  configuration.animated = animated;
  configuration.dismissal_completion = std::move(dismissal_completion);
  return PresentAccountDetailsController(std::move(configuration));
}

DismissViewCallback
SystemIdentityManager::PresentWebAndAppSettingDetailsController(
    id<SystemIdentity> identity,
    UIViewController* view_controller,
    bool animated,
    base::OnceClosure dismissal_completion) {
  SystemIdentityManager::PresentDialogConfiguration configuration;
  configuration.identity = identity;
  configuration.view_controller = view_controller;
  configuration.animated = animated;
  configuration.dismissal_completion = std::move(dismissal_completion);
  return PresentWebAndAppSettingDetailsController(std::move(configuration));
}

DismissViewCallback
SystemIdentityManager::PresentLinkedServicesSettingsDetailsController(
    id<SystemIdentity> identity,
    UIViewController* view_controller,
    bool animated,
    base::OnceClosure dismissal_completion) {
  SystemIdentityManager::PresentDialogConfiguration configuration;
  configuration.identity = identity;
  configuration.view_controller = view_controller;
  configuration.animated = animated;
  configuration.dismissal_completion = std::move(dismissal_completion);
  return PresentLinkedServicesSettingsDetailsController(
      std::move(configuration));
}

void SystemIdentityManager::FireIdentityListChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers_) {
    observer.OnIdentityListChanged();
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
