// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_profile_mapper.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"

AccountProfileMapper::AccountProfileMapper(
    SystemIdentityManager* system_identity_manager,
    size_t profile_count)
    : system_identity_manager_(system_identity_manager),
      profile_count_(profile_count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  profile_index_per_gaia_id_ = [[NSMutableDictionary alloc] init];
  system_identity_manager_observation_.Observe(system_identity_manager);
}

AccountProfileMapper::~AccountProfileMapper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void AccountProfileMapper::AddObserver(AccountProfileMapper::Observer* observer,
                                       size_t profile_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_lists_per_profile_index_[profile_index].AddObserver(observer);
}

void AccountProfileMapper::RemoveObserver(
    AccountProfileMapper::Observer* observer,
    size_t profile_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_lists_per_profile_index_[profile_index].RemoveObserver(observer);
}

bool AccountProfileMapper::IsSigninSupported() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return system_identity_manager_->IsSigninSupported();
}

void AccountProfileMapper::IterateOverIdentities(
    IdentityIteratorCallback callback,
    size_t profile_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::set<size_t> profile_indexes_to_notify;
  auto manager_callback =
      base::BindRepeating(&AccountProfileMapper::ProcessIdentitiesForProfile,
                          base::Unretained(this), profile_index,
                          &profile_indexes_to_notify, callback);
  system_identity_manager_->IterateOverIdentities(manager_callback);
  // If the identities have been updated or added, the profile observers need to
  // be notified about identity list changes.
  for (size_t index : profile_indexes_to_notify) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&AccountProfileMapper::NotifyIdentityListChanged,
                       GetWeakPtr(), index));
  }
}

void AccountProfileMapper::OnIdentityListChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NSMutableSet* known_gaia_ids_before_iteration =
      [NSMutableSet setWithArray:profile_index_per_gaia_id_.allKeys];
  std::set<size_t> profile_indexes_to_notify;
  auto callback = base::BindRepeating(
      &AccountProfileMapper::ProcessIdentityToUpdateMapping,
      base::Unretained(this), known_gaia_ids_before_iteration,
      &profile_indexes_to_notify);
  system_identity_manager_->IterateOverIdentities(callback);
  // All gaia ids that left in `known_gaia_ids_before_iteration` are gaia ids
  // for identities that disappeared. The observers for those profiles need to
  // be notified of the identity list changed.
  for (NSString* gaia_id in known_gaia_ids_before_iteration) {
    NSNumber* profile_index_number = profile_index_per_gaia_id_[gaia_id];
    CHECK(profile_index_number);
    profile_indexes_to_notify.insert(profile_index_number.unsignedLongValue);
    [profile_index_per_gaia_id_ removeObjectForKey:gaia_id];
  }
  // If the identities have been updated or added, the profile observers need to
  // be notified about identity list changes.
  for (size_t profile_index : profile_indexes_to_notify) {
    NotifyIdentityListChanged(profile_index);
  }
}

void AccountProfileMapper::OnIdentityUpdated(id<SystemIdentity> identity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::set<size_t> profile_indexes_to_notify;
  // Test if the identity can be assigned to a profile.
  CheckIdentityProfile(identity, &profile_indexes_to_notify);
  for (size_t profile_index : profile_indexes_to_notify) {
    NotifyIdentityListChanged(profile_index);
  }
  NSNumber* profile_index_number = profile_index_per_gaia_id_[identity.gaiaID];
  if (profile_index_number) {
    NotifyIdentityUpdated(identity, profile_index_number.unsignedLongValue);
  }
}

void AccountProfileMapper::OnIdentityAccessTokenRefreshFailed(
    id<SystemIdentity> identity,
    id<RefreshAccessTokenError> error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NSNumber* profile_index_number = profile_index_per_gaia_id_[identity.gaiaID];
  if (profile_index_number) {
    NotifyAccessTokenRefreshFailed(identity, error,
                                   profile_index_number.unsignedLongValue);
  }
}

SystemIdentityManager::IteratorResult
AccountProfileMapper::ProcessIdentityToUpdateMapping(
    NSMutableSet* known_gaia_ids_before_iteration,
    std::set<size_t>* profile_indexes_to_notify,
    id<SystemIdentity> identity) {
  // `gaiaID` can be removed from `known_gaia_ids_before_iteration` since this
  // identity still exists.
  [known_gaia_ids_before_iteration removeObject:identity.gaiaID];
  // Test if the identity has been moved to another profile.
  CheckIdentityProfile(identity, profile_indexes_to_notify);
  return SystemIdentityManager::IteratorResult::kContinueIteration;
}

SystemIdentityManager::IteratorResult
AccountProfileMapper::ProcessIdentitiesForProfile(
    size_t profile_index,
    std::set<size_t>* profile_indexes_to_notify,
    IdentityIteratorCallback callback,
    id<SystemIdentity> identity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NSNumber* profile_index_number = profile_index_per_gaia_id_[identity.gaiaID];
  if (!profile_index_number) {
    // Test if the identity can be assigned to a profile.
    if (!CheckIdentityProfile(identity, profile_indexes_to_notify)) {
      // The hosted domain is not yet available, `identity` cannot be
      // presented to `callback`.
      return SystemIdentityManager::IteratorResult::kContinueIteration;
    }
    profile_index_number = profile_index_per_gaia_id_[identity.gaiaID];
    // CheckIdentityProfile did update the profile.
    CHECK(profile_index_number);
  }
  if (profile_index != profile_index_number.unsignedLongValue) {
    // Wrong profile.
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
    std::set<size_t>* profile_indexes_to_notify) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/331783685): Need to filter out identities that are filtered
  // by enterprise policy, and remove that filter done by
  // ChromeAccountManagerService.
  if (!experimental_flags::DisplaySwitchProfile().has_value()) {
    // If the multiple profile is not enabled, there is no need to async fetch
    // the hosted domain. All identities are set to the personal profile.
    CheckIdentityProfileWithHostedDomain(identity, @"",
                                         profile_indexes_to_notify);
    return true;
  }
  NSString* hosted_domain =
      system_identity_manager_->GetCachedHostedDomainForIdentity(identity);
  if (hosted_domain) {
    // If the hosted domain is in cache, the identity can checked synchronously.
    CheckIdentityProfileWithHostedDomain(identity, hosted_domain,
                                         profile_indexes_to_notify);
    return true;
  }
  auto hosted_domain_callback =
      base::BindOnce(&AccountProfileMapper::HostedDomainedFetched,
                     base::Unretained(this), identity);
  system_identity_manager_->GetHostedDomain(identity,
                                            std::move(hosted_domain_callback));
  // `identity` may have been set to a profile previously.
  return profile_index_per_gaia_id_[identity.gaiaID] != nil;
}

void AccountProfileMapper::HostedDomainedFetched(id<SystemIdentity> identity,
                                                 NSString* hosted_domain,
                                                 NSError* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::set<size_t> profile_indexes_to_notify;
  if (error) {
    // TODO(crbug.com/331783685): Need to retry.
    // For now, the identity is assigned to the personal profile.
    CheckIdentityProfileWithHostedDomain(identity, @"",
                                         &profile_indexes_to_notify);
  } else {
    // Update the identity profile if needed.
    CheckIdentityProfileWithHostedDomain(identity, hosted_domain,
                                         &profile_indexes_to_notify);
  }
  // Notify observers for all the profile that were updated.
  for (size_t profile_index : profile_indexes_to_notify) {
    NotifyIdentityListChanged(profile_index);
  }
}

void AccountProfileMapper::CheckIdentityProfileWithHostedDomain(
    id<SystemIdentity> identity,
    NSString* hosted_domain,
    std::set<size_t>* profile_indexes_to_notify) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NSNumber* profile_index_number = profile_index_per_gaia_id_[identity.gaiaID];
  if (!profile_index_number) {
    // Add `identity` to its new profile.
    AddIdentityToProfile(identity, hosted_domain, profile_indexes_to_notify);
    return;
  }
  bool is_in_personal_profile = profile_index_number.unsignedLongValue == 0;
  // TODO(crbug.com/331783685): Need to make sure no cached hosted domain is
  // nil, and no hosted domain is @"".
  bool is_managed_account = hosted_domain.length > 0;
  if ((is_in_personal_profile && is_managed_account) ||
      (!is_in_personal_profile && !is_managed_account)) {
    // Remove from previous profile, and add to its new profile.
    RemoveIdentityFromProfile(identity, profile_indexes_to_notify);
    AddIdentityToProfile(identity, hosted_domain, profile_indexes_to_notify);
  }
}

void AccountProfileMapper::AddIdentityToProfile(
    id<SystemIdentity> identity,
    NSString* hosted_domain,
    std::set<size_t>* profile_indexes_to_notify) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!profile_index_per_gaia_id_[identity.gaiaID]);
  CHECK(hosted_domain);
  size_t new_profile_index = 0;
  if (experimental_flags::DisplaySwitchProfile().has_value()) {
    // TODO(crbug.com/331783685): Need to make sure no cached hosted domain is
    // nil, and no hosted domain is @"".
    if (hosted_domain.length != 0) {
      // This is a managed identity, so search for the next available profile
      // index.
      NSArray* all_profile_indexes = profile_index_per_gaia_id_.allValues;
      do {
        new_profile_index += 1;
      } while ([all_profile_indexes containsObject:@(new_profile_index)]);
      if (new_profile_index >= profile_count_) {
        // No more profile available, so for now, the managed identity is added
        // the personal profile.
        new_profile_index = 0;
      }
    }
  }
  profile_index_per_gaia_id_[identity.gaiaID] = @(new_profile_index);
  // Make sure observers for this profile will be notified for this new
  // identity.
  profile_indexes_to_notify->insert(new_profile_index);
}

void AccountProfileMapper::RemoveIdentityFromProfile(
    id<SystemIdentity> identity,
    std::set<size_t>* profile_indexes_to_notify) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NSNumber* profile_index_number = profile_index_per_gaia_id_[identity.gaiaID];
  CHECK(profile_index_number);
  [profile_index_per_gaia_id_ removeObjectForKey:identity.gaiaID];
  // Make sure observers for this profile will be notified for this removed
  // identity.
  profile_indexes_to_notify->insert(profile_index_number.unsignedLongValue);
}

void AccountProfileMapper::NotifyIdentityListChanged(size_t profile_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = observer_lists_per_profile_index_.find(profile_index);
  if (it == observer_lists_per_profile_index_.end()) {
    return;
  }
  for (Observer& observer : it->second) {
    observer.OnIdentityListChanged();
  }
}

void AccountProfileMapper::NotifyIdentityUpdated(id<SystemIdentity> identity,
                                                 size_t profile_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = observer_lists_per_profile_index_.find(profile_index);
  if (it == observer_lists_per_profile_index_.end()) {
    return;
  }
  for (Observer& observer : it->second) {
    observer.OnIdentityUpdated(identity);
  }
}

void AccountProfileMapper::NotifyAccessTokenRefreshFailed(
    id<SystemIdentity> identity,
    id<RefreshAccessTokenError> error,
    size_t profile_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = observer_lists_per_profile_index_.find(profile_index);
  if (it == observer_lists_per_profile_index_.end()) {
    return;
  }
  for (Observer& observer : it->second) {
    observer.OnIdentityAccessTokenRefreshFailed(identity, error);
  }
}

base::WeakPtr<AccountProfileMapper> AccountProfileMapper::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
