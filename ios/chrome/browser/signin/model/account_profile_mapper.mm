// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_profile_mapper.h"

#import "base/check_is_test.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/profile/model/constants.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager_observer.h"

namespace {

using ProfileNameToGaiaIds =
    std::map<std::string, std::set<std::string, std::less<>>, std::less<>>;

// Returns a map from each profile name to the set of attached Gaia IDs.
ProfileNameToGaiaIds GetMappingFromProfileAttributes(
    SystemIdentityManager* system_identity_manager,
    const ProfileAttributesStorageIOS* profile_attributes_storage) {
  ProfileNameToGaiaIds result;

  if (!base::FeatureList::IsEnabled(kSeparateProfilesForManagedAccounts)) {
    system_identity_manager->IterateOverIdentities(base::BindRepeating(
        [](std::map<std::string, std::set<std::string, std::less<>>,
                    std::less<>>& result,
           id<SystemIdentity> identity) {
          // Note: In this case (with the feature flag disabled), the profile
          // name in the mapping isn't used - every identity is considered
          // assigned to every profile.
          result[std::string()].insert(
              base::SysNSStringToUTF8(identity.gaiaID));
          return SystemIdentityManager::IteratorResult::kContinueIteration;
        },
        std::ref(result)));
    return result;
  }

  if (!profile_attributes_storage) {
    CHECK_IS_TEST();
    return result;
  }
  for (size_t index = 0;
       index < profile_attributes_storage->GetNumberOfProfiles(); index++) {
    ProfileAttributesIOS attr =
        profile_attributes_storage->GetAttributesForProfileAtIndex(index);
    result[attr.GetProfileName()] = attr.GetAttachedGaiaIds();
  }
  return result;
}

// Returns the name of the profile that has `gaia_id` attached, or the empty
// string if no such profile exists.
std::string FindProfileNameForGaiaId(
    const ProfileAttributesStorageIOS* profile_attributes_storage,
    std::string_view gaia_id) {
  if (!profile_attributes_storage) {
    CHECK_IS_TEST();
    return std::string();
  }
  for (size_t index = 0;
       index < profile_attributes_storage->GetNumberOfProfiles(); index++) {
    ProfileAttributesIOS attr =
        profile_attributes_storage->GetAttributesForProfileAtIndex(index);
    if (attr.GetAttachedGaiaIds().contains(gaia_id)) {
      return attr.GetProfileName();
    }
  }
  return std::string();
}

void AttachGaiaIdToProfile(
    ProfileAttributesStorageIOS* profile_attributes_storage,
    std::string_view profile_name,
    std::string_view gaia_id) {
  if (!profile_attributes_storage ||
      !profile_attributes_storage->HasProfileWithName(profile_name)) {
    CHECK_IS_TEST();
    return;
  }
  profile_attributes_storage->UpdateAttributesForProfileWithName(
      profile_name,
      base::BindOnce(
          [](std::string_view gaia_id, ProfileAttributesIOS attr) {
            auto gaia_ids = attr.GetAttachedGaiaIds();
            gaia_ids.insert(std::string(gaia_id));
            attr.SetAttachedGaiaIds(gaia_ids);
            return attr;
          },
          gaia_id));
}

void DetachGaiaIdFromProfile(
    ProfileAttributesStorageIOS* profile_attributes_storage,
    std::string_view profile_name,
    std::string_view gaia_id) {
  if (!profile_attributes_storage ||
      !profile_attributes_storage->HasProfileWithName(profile_name)) {
    CHECK_IS_TEST();
    return;
  }
  profile_attributes_storage->UpdateAttributesForProfileWithName(
      profile_name,
      base::BindOnce(
          [](std::string_view gaia_id, ProfileAttributesIOS attr) {
            auto gaia_ids = attr.GetAttachedGaiaIds();
            gaia_ids.erase(std::string(gaia_id));
            attr.SetAttachedGaiaIds(gaia_ids);
            return attr;
          },
          gaia_id));
}

}  // namespace

// Helper class that handles assignment of accounts to profiles. Specifically,
// it updates the "attached Gaia IDs" property in ProfileAttributesIOS, and
// calls back out into AccountProfileMapper whenever the mapping changes. Also
// propagates other SystemIdentityManagerObserver events out.
class AccountProfileMapper::Assigner : public SystemIdentityManagerObserver {
 public:
  using MappingUpdatedCallback =
      base::RepeatingCallback<void(const ProfileNameToGaiaIds& old_mapping,
                                   const ProfileNameToGaiaIds& new_mapping)>;
  using IdentityUpdatedCallback =
      base::RepeatingCallback<void(id<SystemIdentity> identity)>;
  using IdentityAccessTokenRefreshFailedCallback =
      base::RepeatingCallback<void(id<SystemIdentity> identity,
                                   id<RefreshAccessTokenError> error)>;

  // `mapping_updated_cb` will be run every time any identities are added or
  // removed from any profiles.
  // `identity_updated_cb` and `identity_access_token_refresh_failed_cb`
  // correspond to the similarly-named methods on Observer.
  Assigner(SystemIdentityManager* system_identity_manager,
           ProfileAttributesStorageIOS* profile_attributes_storage,
           MappingUpdatedCallback mapping_updated_cb,
           IdentityUpdatedCallback identity_updated_cb,
           IdentityAccessTokenRefreshFailedCallback
               identity_access_token_refresh_failed_cb);
  ~Assigner() override;

  // SystemIdentityManagerObserver implementation.
  void OnIdentityListChanged() final;
  void OnIdentityUpdated(id<SystemIdentity> identity) final;
  void OnIdentityAccessTokenRefreshFailed(
      id<SystemIdentity> identity,
      id<RefreshAccessTokenError> error) final;

 private:
  std::string GetAvailableProfileName() const;

  // Callback for SystemIdentityManager::IterateOverIdentities(). Checks the
  // mapping of `identity` to a profile, and attaches (or re-attaches) it as
  // necessary. Note that the attaching may happen asynchronously, if the hosted
  // domain needs to be fetched first.
  SystemIdentityManager::IteratorResult ProcessIdentityForAssignmentToProfile(
      std::set<std::string>& processed_gaia_ids,
      id<SystemIdentity> identity);
  // Called when the hosted domain for `identity` has been fetched
  // asynchronously. Triggers the assignment to an appropriate profile.
  void HostedDomainedFetched(id<SystemIdentity> identity,
                             NSString* hosted_domain,
                             NSError* error);
  // Assigns `identity` to a profile (or re-assigns it to a different profile)
  // if necessary, based on whether it's a managed account or not.
  void AssignIdentityToProfile(id<SystemIdentity> identity,
                               bool is_managed_account);

  // Re-fetches the account<->profile mappings from ProfileAttributesStorageIOS,
  // and if anything changed, notifies AccountProfileMapper via the callback.
  void MaybeUpdateCachedMappingAndNotify();

  raw_ptr<SystemIdentityManager> system_identity_manager_;
  base::ScopedObservation<SystemIdentityManager, SystemIdentityManagerObserver>
      system_identity_manager_observation_{this};

  raw_ptr<ProfileAttributesStorageIOS> profile_attributes_storage_;

  MappingUpdatedCallback mapping_updated_cb_;
  IdentityUpdatedCallback identity_updated_cb_;
  IdentityAccessTokenRefreshFailedCallback
      identity_access_token_refresh_failed_cb_;

  // The mapping from profile name to the list of attached Gaia IDs.
  // If `kSeparateProfilesForManagedAccounts` is enabled, this is a cache of
  // the data in ProfileAttributesStorageIOS, and used to detect when the
  // values there have changed.
  // If `kSeparateProfilesForManagedAccounts` is disabled, the data from
  // ProfileAttributesStorageIOS isn't used here, and all Gaia IDs are
  // nominally assigned to an empty profile name (just to detect changes to
  // the list - AccountProfileMapper won't do any filtering).
  ProfileNameToGaiaIds profile_to_gaia_ids_;

  base::WeakPtrFactory<Assigner> weak_ptr_factory_{this};
};

AccountProfileMapper::Assigner::Assigner(
    SystemIdentityManager* system_identity_manager,
    ProfileAttributesStorageIOS* profile_attributes_storage,
    MappingUpdatedCallback mapping_updated_cb,
    IdentityUpdatedCallback identity_updated_cb,
    IdentityAccessTokenRefreshFailedCallback
        identity_access_token_refresh_failed_cb)
    : system_identity_manager_(system_identity_manager),
      profile_attributes_storage_(profile_attributes_storage),
      mapping_updated_cb_(mapping_updated_cb),
      identity_updated_cb_(identity_updated_cb),
      identity_access_token_refresh_failed_cb_(
          identity_access_token_refresh_failed_cb) {
  CHECK(system_identity_manager_);
  // `profile_attributes_storage_` can be null in unit tests.

  system_identity_manager_observation_.Observe(system_identity_manager_);

  profile_to_gaia_ids_ = GetMappingFromProfileAttributes(
      system_identity_manager_, profile_attributes_storage_);
}

AccountProfileMapper::Assigner::~Assigner() = default;

void AccountProfileMapper::Assigner::OnIdentityListChanged() {
  // Assign identities to profiles, if they're not assigned yet.
  std::set<std::string> processed_gaia_ids;
  system_identity_manager_->IterateOverIdentities(base::BindRepeating(
      &Assigner::ProcessIdentityForAssignmentToProfile, base::Unretained(this),
      std::ref(processed_gaia_ids)));

  // Check if any of the previously-assigned Gaia IDs have been removed.
  if (base::FeatureList::IsEnabled(kSeparateProfilesForManagedAccounts)) {
    for (const auto& [profile_name, gaia_ids] : profile_to_gaia_ids_) {
      for (const std::string& gaia_id : gaia_ids) {
        if (!processed_gaia_ids.contains(gaia_id)) {
          DetachGaiaIdFromProfile(profile_attributes_storage_, profile_name,
                                  gaia_id);
        }
      }
    }
  }

  // If any mappings were added/changed, let the observers know.
  MaybeUpdateCachedMappingAndNotify();
}

void AccountProfileMapper::Assigner::OnIdentityUpdated(
    id<SystemIdentity> identity) {
  // Usually `OnIdentityUpdated` means that something like the account name or
  // image was updated, i.e. nothing that would affect the profile mapping. But
  // it's also possible (though should be rare) that the hosted domain was
  // changed, so, re-evaluate the mapping.
  // Note: It's not possible for the identity to be removed from the
  // `SystemIdentityManager` here, so (as opposed to `OnIdentityListChanged`) no
  // need to track the processed Gaia IDs to detect removals.
  std::set<std::string> processed_gaia_ids_unused;
  ProcessIdentityForAssignmentToProfile(processed_gaia_ids_unused, identity);

  // If any mappings were added/changed (unlikely), let the AccountProfileMapper
  // know.
  MaybeUpdateCachedMappingAndNotify();

  // After updating the mappings, let the AccountProfileMapper know about the
  // updated identity.
  identity_updated_cb_.Run(identity);
}

void AccountProfileMapper::Assigner::OnIdentityAccessTokenRefreshFailed(
    id<SystemIdentity> identity,
    id<RefreshAccessTokenError> error) {
  identity_access_token_refresh_failed_cb_.Run(identity, error);
}

std::string AccountProfileMapper::Assigner::GetAvailableProfileName() const {
  // TODO(crbug.com/331783685): Remove assumption that "Default" is the
  // personal profile.
  std::string new_profile_name = std::string(kIOSChromeInitialProfile);

  // Search for the next available profile from the existing profiles.
  // TODO(crbug.com/331783685): Instead of relying on pre-created profiles,
  // create new profiles as necessary.
  std::set<std::string_view> used_profile_names;
  for (const auto& [profile_name, gaia_ids] : profile_to_gaia_ids_) {
    if (!gaia_ids.empty()) {
      used_profile_names.insert(profile_name);
    }
  }
  if (profile_attributes_storage_) {
    for (size_t index = 0;
         index < profile_attributes_storage_->GetNumberOfProfiles(); index++) {
      ProfileAttributesIOS attr =
          profile_attributes_storage_->GetAttributesForProfileAtIndex(index);
      if (!used_profile_names.contains(attr.GetProfileName())) {
        new_profile_name = attr.GetProfileName();
        break;
      }
    }
  }
  // Note: It's possible that there was no available profile. In that case,
  // fall back to the default profile.
  return new_profile_name;
}

SystemIdentityManager::IteratorResult
AccountProfileMapper::Assigner::ProcessIdentityForAssignmentToProfile(
    std::set<std::string>& processed_gaia_ids,
    id<SystemIdentity> identity) {
  processed_gaia_ids.insert(base::SysNSStringToUTF8(identity.gaiaID));

  if (!base::FeatureList::IsEnabled(kSeparateProfilesForManagedAccounts)) {
    // With the feature flag disabled, no actual assignment is necessary.
    return SystemIdentityManager::IteratorResult::kContinueIteration;
  }

  NSString* hosted_domain =
      system_identity_manager_->GetCachedHostedDomainForIdentity(identity);
  if (!hosted_domain) {
    // If the hosted domain is not in the cache yet, this identity can't be
    // assigned to a profile yet. Query it, and assign once available.
    system_identity_manager_->GetHostedDomain(
        identity,
        base::BindOnce(&AccountProfileMapper::Assigner::HostedDomainedFetched,
                       weak_ptr_factory_.GetWeakPtr(), identity));
    return SystemIdentityManager::IteratorResult::kContinueIteration;
  }

  bool is_managed_account = hosted_domain.length > 0;
  AssignIdentityToProfile(identity, is_managed_account);

  return SystemIdentityManager::IteratorResult::kContinueIteration;
}

void AccountProfileMapper::Assigner::HostedDomainedFetched(
    id<SystemIdentity> identity,
    NSString* hosted_domain,
    NSError* error) {
  CHECK(base::FeatureList::IsEnabled(kSeparateProfilesForManagedAccounts));

  if (error) {
    // TODO(crbug.com/331783685): Need to retry.
    // For now, assume an empty hosted domain, which means the identity will get
    // assigned to the personal profile.
    hosted_domain = @"";
  } else {
    CHECK(hosted_domain);
  }
  bool is_managed_account = hosted_domain.length > 0;
  AssignIdentityToProfile(identity, is_managed_account);

  MaybeUpdateCachedMappingAndNotify();
}

void AccountProfileMapper::Assigner::AssignIdentityToProfile(
    id<SystemIdentity> identity,
    bool is_managed_account) {
  CHECK(base::FeatureList::IsEnabled(kSeparateProfilesForManagedAccounts));

  std::string gaia_id = base::SysNSStringToUTF8(identity.gaiaID);

  std::optional<std::string> current_assigned_profile;
  for (const auto& [profile_name, gaia_ids] : profile_to_gaia_ids_) {
    if (gaia_ids.contains(gaia_id)) {
      current_assigned_profile = profile_name;
      break;
    }
  }

  if (!current_assigned_profile) {
    if (is_managed_account) {
      // TODO(crbug.com/331783685): Create a new profile (asynchronously).
      AttachGaiaIdToProfile(profile_attributes_storage_,
                            GetAvailableProfileName(), gaia_id);
    } else {
      // Consumer account, assign to the personal profile.
      // TODO(crbug.com/331783685): Remove assumption that "Default" is the
      // personal profile.
      AttachGaiaIdToProfile(profile_attributes_storage_,
                            kIOSChromeInitialProfile, gaia_id);
    }
  } else {
    // Already assigned, check if it needs to be re-assigned.
    bool is_in_personal_profile =
        (*current_assigned_profile == kIOSChromeInitialProfile);
    if (is_in_personal_profile == is_managed_account) {
      DetachGaiaIdFromProfile(profile_attributes_storage_,
                              *current_assigned_profile, gaia_id);
      // TODO(crbug.com/331783685): Create a new profile (asynchronously) if
      // necessary.
      AttachGaiaIdToProfile(profile_attributes_storage_,
                            is_managed_account ? GetAvailableProfileName()
                                               : kIOSChromeInitialProfile,
                            gaia_id);
    }
    // Else: The account is already in the correct kind of profile, nothing to
    // be done.
  }
}

void AccountProfileMapper::Assigner::MaybeUpdateCachedMappingAndNotify() {
  auto new_mapping = GetMappingFromProfileAttributes(
      system_identity_manager_, profile_attributes_storage_);
  if (new_mapping != profile_to_gaia_ids_) {
    auto old_mapping = std::move(profile_to_gaia_ids_);
    profile_to_gaia_ids_ = std::move(new_mapping);
    mapping_updated_cb_.Run(old_mapping, profile_to_gaia_ids_);
  }
}

AccountProfileMapper::AccountProfileMapper(
    SystemIdentityManager* system_identity_manager,
    ProfileManagerIOS* profile_manager)
    : system_identity_manager_(system_identity_manager),
      profile_manager_(profile_manager) {
  CHECK(system_identity_manager);
  // `profile_manager_` can be null in unit tests.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  assigner_ = std::make_unique<Assigner>(
      system_identity_manager_,
      profile_manager_ ? profile_manager_->GetProfileAttributesStorage()
                       : nullptr,
      base::BindRepeating(&AccountProfileMapper::MappingUpdated,
                          base::Unretained(this)),
      base::BindRepeating(&AccountProfileMapper::IdentityUpdated,
                          base::Unretained(this)),
      base::BindRepeating(
          &AccountProfileMapper::IdentityAccessTokenRefreshFailed,
          base::Unretained(this)));
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
  auto manager_callback =
      base::BindRepeating(&AccountProfileMapper::FilterIdentitiesForProfile,
                          base::Unretained(this), profile_name, callback);
  system_identity_manager_->IterateOverIdentities(manager_callback);
}

void AccountProfileMapper::IdentityUpdated(id<SystemIdentity> identity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NotifyIdentityUpdated(
      identity,
      FindProfileNameForGaiaId(
          profile_manager_ ? profile_manager_->GetProfileAttributesStorage()
                           : nullptr,
          base::SysNSStringToUTF8(identity.gaiaID)));
}

void AccountProfileMapper::IdentityAccessTokenRefreshFailed(
    id<SystemIdentity> identity,
    id<RefreshAccessTokenError> error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NotifyAccessTokenRefreshFailed(
      identity, error,
      FindProfileNameForGaiaId(
          profile_manager_ ? profile_manager_->GetProfileAttributesStorage()
                           : nullptr,
          base::SysNSStringToUTF8(identity.gaiaID)));
}

SystemIdentityManager::IteratorResult
AccountProfileMapper::FilterIdentitiesForProfile(
    std::string_view profile_name,
    IdentityIteratorCallback callback,
    id<SystemIdentity> identity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/331783685): Need to filter out identities that are filtered
  // by enterprise policy, and remove that filter done by
  // ChromeAccountManagerService.

  if (base::FeatureList::IsEnabled(kSeparateProfilesForManagedAccounts)) {
    ProfileAttributesIOS attr =
        profile_manager_->GetProfileAttributesStorage()
            ->GetAttributesForProfileWithName(profile_name);
    if (!attr.GetAttachedGaiaIds().contains(
            base::SysNSStringToUTF8(identity.gaiaID))) {
      // The identity doesn't belong to this profile; skip over it.
      return SystemIdentityManager::IteratorResult::kContinueIteration;
    }
  }

  switch (callback.Run(identity)) {
    case AccountProfileMapper::IteratorResult::kContinueIteration:
      return SystemIdentityManager::IteratorResult::kContinueIteration;
    case AccountProfileMapper::IteratorResult::kInterruptIteration:
      return SystemIdentityManager::IteratorResult::kInterruptIteration;
  }
}

void AccountProfileMapper::MappingUpdated(
    const ProfileNameToGaiaIds& old_mapping,
    const ProfileNameToGaiaIds& new_mapping) {
  std::set<std::string> profiles_to_notify;
  // Note: If the feature flag is disabled, all profiles are notified, so no
  // need to find the affected profiles.
  if (base::FeatureList::IsEnabled(kSeparateProfilesForManagedAccounts)) {
    std::set<std::string> all_profiles;
    for (const auto& [name, gaia_ids] : old_mapping) {
      all_profiles.insert(name);
    }
    for (const auto& [name, gaia_ids] : new_mapping) {
      all_profiles.insert(name);
    }
    // Notify all profiles for which the mapping was added, removed, or changed.
    for (const std::string& name : all_profiles) {
      auto old_it = old_mapping.find(name);
      auto new_it = new_mapping.find(name);
      if (old_it == old_mapping.end() || new_it == new_mapping.end() ||
          old_it->second != new_it->second) {
        profiles_to_notify.insert(name);
      }
    }
  }
  NotifyIdentityListChanged(profiles_to_notify);
}

void AccountProfileMapper::NotifyIdentityListChanged(
    const std::set<std::string>& profile_names_to_notify) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
    // If the feature flag is not enabled, notify all profiles.
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
    if (profile_name.empty()) {
      return;
    }
    auto it = observer_lists_per_profile_name_.find(profile_name);
    if (it == observer_lists_per_profile_name_.end()) {
      return;
    }
    for (Observer& observer : it->second) {
      observer.OnIdentityUpdated(identity);
    }
  } else {
    // If the feature flag is not enabled, notify all profiles.
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
    if (profile_name.empty()) {
      return;
    }
    auto it = observer_lists_per_profile_name_.find(profile_name);
    if (it == observer_lists_per_profile_name_.end()) {
      return;
    }
    for (Observer& observer : it->second) {
      observer.OnIdentityAccessTokenRefreshFailed(identity, error);
    }
  } else {
    // If the feature flag is not enabled, notify all profiles.
    for (const auto& [name, observer_list] : observer_lists_per_profile_name_) {
      for (Observer& observer : observer_list) {
        observer.OnIdentityAccessTokenRefreshFailed(identity, error);
      }
    }
  }
}
