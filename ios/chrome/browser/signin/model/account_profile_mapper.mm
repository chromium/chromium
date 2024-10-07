// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_profile_mapper.h"

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/profile/model/constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"

AccountProfileMapper::AccountProfileMapper(
    SystemIdentityManager* system_identity_manager)
    : system_identity_manager_(system_identity_manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  system_identity_manager_observation_.Observe(system_identity_manager);
}

AccountProfileMapper::~AccountProfileMapper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void AccountProfileMapper::AddObserver(AccountProfileMapper::Observer* observer,
                                       std::string_view profile_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_lists_per_profile_name_[std::string(profile_name)].AddObserver(
      observer);
}

void AccountProfileMapper::RemoveObserver(
    AccountProfileMapper::Observer* observer,
    std::string_view profile_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_lists_per_profile_name_[std::string(profile_name)].RemoveObserver(
      observer);
}

bool AccountProfileMapper::IsSigninSupported() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return system_identity_manager_->IsSigninSupported();
}

void AccountProfileMapper::IterateOverIdentities(
    IdentityIteratorCallback callback,
    std::string_view profile_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::set<std::string> profile_names_to_notify;
  auto manager_callback =
      base::BindRepeating(&AccountProfileMapper::ProcessIdentitiesForProfile,
                          base::Unretained(this), profile_name,
                          std::ref(profile_names_to_notify), callback);
  system_identity_manager_->IterateOverIdentities(manager_callback);
  // If any identities have been updated or added, the profile observers need to
  // be notified about identity list changes.
  if (!profile_names_to_notify.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&AccountProfileMapper::NotifyIdentityListChanged,
                       GetWeakPtr(), profile_names_to_notify));
  }
}

void AccountProfileMapper::OnIdentityListChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::set<std::string> known_gaia_ids_before_iteration;
  for (const auto& [gaia_id, profile_name] : profile_name_per_gaia_id_) {
    known_gaia_ids_before_iteration.insert(
        known_gaia_ids_before_iteration.end(), gaia_id);
  }
  std::set<std::string> profile_names_to_notify;
  auto callback = base::BindRepeating(
      &AccountProfileMapper::ProcessIdentityToUpdateMapping,
      base::Unretained(this), std::ref(known_gaia_ids_before_iteration),
      std::ref(profile_names_to_notify));
  system_identity_manager_->IterateOverIdentities(callback);
  // All gaia ids that left in `known_gaia_ids_before_iteration` are gaia ids
  // for identities that disappeared. The observers for those profiles need to
  // be notified of the identity list changed.
  for (const std::string& gaia_id : known_gaia_ids_before_iteration) {
    CHECK(profile_name_per_gaia_id_.count(gaia_id));
    const std::string& profile_name = profile_name_per_gaia_id_[gaia_id];
    profile_names_to_notify.insert(profile_name);
    profile_name_per_gaia_id_.erase(gaia_id);
  }
  // If any identities have been (re)assigned, the profile observers need to
  // be notified about identity list changes.
  NotifyIdentityListChanged(profile_names_to_notify);
}

void AccountProfileMapper::OnIdentityUpdated(id<SystemIdentity> identity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // First, check if the identity can be assigned (or needs to be re-assigned)
  // to a profile.
  std::set<std::string> profile_names_to_notify;
  CheckIdentityProfile(identity, profile_names_to_notify);
  NotifyIdentityListChanged(profile_names_to_notify);

  auto it =
      profile_name_per_gaia_id_.find(base::SysNSStringToUTF8(identity.gaiaID));
  if (it != profile_name_per_gaia_id_.end()) {
    NotifyIdentityUpdated(identity, it->second);
  }
}

void AccountProfileMapper::OnIdentityAccessTokenRefreshFailed(
    id<SystemIdentity> identity,
    id<RefreshAccessTokenError> error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it =
      profile_name_per_gaia_id_.find(base::SysNSStringToUTF8(identity.gaiaID));
  if (it != profile_name_per_gaia_id_.end()) {
    NotifyAccessTokenRefreshFailed(identity, error, it->second);
  }
}

SystemIdentityManager::IteratorResult
AccountProfileMapper::ProcessIdentityToUpdateMapping(
    std::set<std::string>& known_gaia_ids_before_iteration,
    std::set<std::string>& profile_names_to_notify,
    id<SystemIdentity> identity) {
  // `gaiaID` can be removed from `known_gaia_ids_before_iteration` since this
  // identity still exists.
  known_gaia_ids_before_iteration.erase(
      base::SysNSStringToUTF8(identity.gaiaID));
  // Test if the identity has been moved to another profile.
  CheckIdentityProfile(identity, profile_names_to_notify);
  return SystemIdentityManager::IteratorResult::kContinueIteration;
}

SystemIdentityManager::IteratorResult
AccountProfileMapper::ProcessIdentitiesForProfile(
    std::string_view profile_name,
    std::set<std::string>& profile_names_to_notify,
    IdentityIteratorCallback callback,
    id<SystemIdentity> identity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto existing_mapping_it =
      profile_name_per_gaia_id_.find(base::SysNSStringToUTF8(identity.gaiaID));
  if (existing_mapping_it == profile_name_per_gaia_id_.end()) {
    // This identity isn't assigned to a profile yet. Test if it can be.
    if (!CheckIdentityProfile(identity, profile_names_to_notify)) {
      // The identity can't be assigned to a profile (probably because the
      // hosted domain is not yet available), so `identity` cannot be presented
      // to `callback`.
      return SystemIdentityManager::IteratorResult::kContinueIteration;
    }
    existing_mapping_it = profile_name_per_gaia_id_.find(
        base::SysNSStringToUTF8(identity.gaiaID));
    // CheckIdentityProfile did update the profile.
    CHECK(existing_mapping_it != profile_name_per_gaia_id_.end());
  }
  // If the feature flag is enabled, filter out identities that belong to
  // another profile.
  if (base::FeatureList::IsEnabled(kSeparateProfilesForManagedAccounts) &&
      profile_name != existing_mapping_it->second) {
    // Not the interesting profile, skip this identity.
    return SystemIdentityManager::IteratorResult::kContinueIteration;
  }
  switch (callback.Run(identity)) {
    case AccountProfileMapper::IteratorResult::kContinueIteration:
      return SystemIdentityManager::IteratorResult::kContinueIteration;
    case AccountProfileMapper::IteratorResult::kInterruptIteration:
      return SystemIdentityManager::IteratorResult::kInterruptIteration;
  }
  NOTREACHED();
}

bool AccountProfileMapper::CheckIdentityProfile(
    id<SystemIdentity> identity,
    std::set<std::string>& profile_names_to_notify) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/331783685): Need to filter out identities that are filtered
  // by enterprise policy, and remove that filter done by
  // ChromeAccountManagerService.
  if (!base::FeatureList::IsEnabled(kSeparateProfilesForManagedAccounts)) {
    // If the multi-profile feature is not enabled, there is no need to async
    // fetch the hosted domain. All identities are set to every profile.
    CheckIdentityProfileWithHostedDomain(identity, @"",
                                         profile_names_to_notify);
    return true;
  }
  NSString* hosted_domain =
      system_identity_manager_->GetCachedHostedDomainForIdentity(identity);
  if (hosted_domain) {
    // If the hosted domain is in cache, the identity can checked synchronously.
    CheckIdentityProfileWithHostedDomain(identity, hosted_domain,
                                         profile_names_to_notify);
    return true;
  }
  auto hosted_domain_callback =
      base::BindOnce(&AccountProfileMapper::HostedDomainedFetched,
                     base::Unretained(this), identity);
  system_identity_manager_->GetHostedDomain(identity,
                                            std::move(hosted_domain_callback));
  // `identity` may have been set to a profile previously.
  return profile_name_per_gaia_id_.count(
             base::SysNSStringToUTF8(identity.gaiaID)) > 0;
}

void AccountProfileMapper::HostedDomainedFetched(id<SystemIdentity> identity,
                                                 NSString* hosted_domain,
                                                 NSError* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::set<std::string> profile_names_to_notify;
  if (error) {
    // TODO(crbug.com/331783685): Need to retry.
    // For now, the identity is assigned to the personal profile.
    CheckIdentityProfileWithHostedDomain(identity, @"",
                                         profile_names_to_notify);
  } else {
    // Update the identity profile if needed.
    CheckIdentityProfileWithHostedDomain(identity, hosted_domain,
                                         profile_names_to_notify);
  }
  // Notify observers for all the profiles that were updated.
  NotifyIdentityListChanged(profile_names_to_notify);
}

void AccountProfileMapper::CheckIdentityProfileWithHostedDomain(
    id<SystemIdentity> identity,
    NSString* hosted_domain,
    std::set<std::string>& profile_names_to_notify) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto existing_mapping_it =
      profile_name_per_gaia_id_.find(base::SysNSStringToUTF8(identity.gaiaID));
  if (existing_mapping_it == profile_name_per_gaia_id_.end()) {
    // Add `identity` to its new profile.
    AddIdentityToProfile(identity, hosted_domain, profile_names_to_notify);
    return;
  }
  // The `identity` is already assigned to a profile. Verify that it's the
  // correct one.
  // TODO(crbug.com/331783685): Remove assumption that "Default" is the
  // personal profile.
  bool is_in_personal_profile =
      (existing_mapping_it->second == std::string(kIOSChromeInitialProfile));
  // TODO(crbug.com/331783685): Need to make sure no cached hosted domain is
  // nil, and no hosted domain is @"".
  bool is_managed_account = hosted_domain.length > 0;
  if (is_in_personal_profile == is_managed_account) {
    // The identity is wrongly assigned - managed identity in the personal
    // profile, or vice versa. Reassign it.
    RemoveIdentityFromProfile(identity, profile_names_to_notify);
    AddIdentityToProfile(identity, hosted_domain, profile_names_to_notify);
  }
}

void AccountProfileMapper::AddIdentityToProfile(
    id<SystemIdentity> identity,
    NSString* hosted_domain,
    std::set<std::string>& profile_names_to_notify) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string gaia_id = base::SysNSStringToUTF8(identity.gaiaID);
  CHECK(profile_name_per_gaia_id_.count(gaia_id) == 0);
  CHECK(hosted_domain);
  // TODO(crbug.com/331783685): Remove assumption that "Default" is the
  // personal profile.
  std::string new_profile_name = std::string(kIOSChromeInitialProfile);
  // TODO(crbug.com/331783685): Need to make sure no cached hosted domain is
  // nil, and no hosted domain is @"".
  if (base::FeatureList::IsEnabled(kSeparateProfilesForManagedAccounts) &&
      hosted_domain.length != 0) {
    // This is a managed identity, so search for the next available profile.
    std::set<std::string_view> used_profile_names;
    for (const auto& [_, profile_name] : profile_name_per_gaia_id_) {
      used_profile_names.insert(profile_name);
    }
    // TODO(crbug.com/331783685): Instead of using the pre-created test
    // profiles, read from `ProfileAttributesIOS::GetAttachedGaiaIds` here.
    std::optional<int> num_test_profiles =
        experimental_flags::DisplaySwitchProfile();
    if (num_test_profiles.has_value()) {
      for (int index = 0; index < num_test_profiles.value(); index++) {
        // See ProfileManagerIOSImpl::LoadProfiles().
        std::string name_candidate =
            "TestProfile" + base::NumberToString(index + 1);
        if (used_profile_names.count(name_candidate) == 0) {
          new_profile_name = name_candidate;
          break;
        }
      }
    }
    // Note: It's possible that there was no available profile. In that case,
    // fall back to adding the managed identity to the default profile.
  }
  profile_name_per_gaia_id_[gaia_id] = new_profile_name;
  // Make sure observers for this profile will be notified for this new
  // identity.
  profile_names_to_notify.insert(new_profile_name);
}

void AccountProfileMapper::RemoveIdentityFromProfile(
    id<SystemIdentity> identity,
    std::set<std::string>& profile_names_to_notify) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string gaia_id = base::SysNSStringToUTF8(identity.gaiaID);
  CHECK(profile_name_per_gaia_id_.count(gaia_id));
  std::string profile_name = profile_name_per_gaia_id_[gaia_id];
  profile_name_per_gaia_id_.erase(gaia_id);
  // Make sure observers for this profile will be notified for this removed
  // identity.
  profile_names_to_notify.insert(profile_name);
}

void AccountProfileMapper::NotifyIdentityListChanged(
    const std::set<std::string>& profile_names_to_notify) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (profile_names_to_notify.empty()) {
    return;
  }
  if (base::FeatureList::IsEnabled(kSeparateProfilesForManagedAccounts)) {
    for (const std::string& profile_name : profile_names_to_notify) {
      auto it = observer_lists_per_profile_name_.find(profile_name);
      if (it == observer_lists_per_profile_name_.end()) {
        return;
      }
      for (Observer& observer : it->second) {
        observer.OnIdentityListChanged();
      }
    }
  } else {
    // If the experimental flag is not enabled, notify all profiles.
    for (const auto& [name, observer_list] : observer_lists_per_profile_name_) {
      for (Observer& observer : observer_list) {
        observer.OnIdentityListChanged();
      }
    }
  }
}

void AccountProfileMapper::NotifyIdentityUpdated(
    id<SystemIdentity> identity,
    std::string_view profile_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (base::FeatureList::IsEnabled(kSeparateProfilesForManagedAccounts)) {
    auto it = observer_lists_per_profile_name_.find(profile_name);
    if (it == observer_lists_per_profile_name_.end()) {
      return;
    }
    for (Observer& observer : it->second) {
      observer.OnIdentityUpdated(identity);
    }
  } else {
    // If the experimental flag is not enabled, notify all profiles.
    for (const auto& [name, observer_list] : observer_lists_per_profile_name_) {
      for (Observer& observer : observer_list) {
        observer.OnIdentityUpdated(identity);
      }
    }
  }
}

void AccountProfileMapper::NotifyAccessTokenRefreshFailed(
    id<SystemIdentity> identity,
    id<RefreshAccessTokenError> error,
    std::string_view profile_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (base::FeatureList::IsEnabled(kSeparateProfilesForManagedAccounts)) {
    auto it = observer_lists_per_profile_name_.find(profile_name);
    if (it == observer_lists_per_profile_name_.end()) {
      return;
    }
    for (Observer& observer : it->second) {
      observer.OnIdentityAccessTokenRefreshFailed(identity, error);
    }
  } else {
    // If the experimental flag is not enabled, notify all profiles.
    for (const auto& [name, observer_list] : observer_lists_per_profile_name_) {
      for (Observer& observer : observer_list) {
        observer.OnIdentityAccessTokenRefreshFailed(identity, error);
      }
    }
  }
}

base::WeakPtr<AccountProfileMapper> AccountProfileMapper::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
