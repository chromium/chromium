// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_profile_mapper.h"

#import "base/check_is_test.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager_observer.h"

namespace {

// Name of the personal profile for tests.
constexpr char kPersonalProfileNameForTesting[] =
    "bf09f5cf-94cc-4336-9cc2-26a5e1b8c358";

using ProfileNameToGaiaIds =
    std::map<std::string, std::set<std::string, std::less<>>, std::less<>>;

// Returns a map from each profile name to the set of attached Gaia IDs.
ProfileNameToGaiaIds GetMappingFromProfileAttributes(
    SystemIdentityManager* system_identity_manager,
    const ProfileAttributesStorageIOS* profile_attributes_storage) {
  ProfileNameToGaiaIds result;

  if (!AreSeparateProfilesForManagedAccountsEnabled()) {
    system_identity_manager->IterateOverIdentities(base::BindRepeating(
        [](ProfileNameToGaiaIds& result, id<SystemIdentity> identity) {
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

void AttachGaiaIdToProfile(
    ProfileAttributesStorageIOS* profile_attributes_storage,
    std::string_view profile_name,
    std::string_view gaia_id) {
  if (!profile_attributes_storage) {
    CHECK_IS_TEST();
    return;
  }
  if (profile_name.empty()) {
    CHECK_IS_TEST();
    return;
  }
  if (!profile_attributes_storage->HasProfileWithName(profile_name)) {
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
  using IdentitiesOnDeviceChangedCallback = base::RepeatingCallback<void()>;
  using MappingUpdatedCallback =
      base::RepeatingCallback<void(const ProfileNameToGaiaIds& old_mapping,
                                   const ProfileNameToGaiaIds& new_mapping)>;
  using IdentityUpdatedCallback =
      base::RepeatingCallback<void(id<SystemIdentity> identity)>;
  using IdentityRefreshTokenUpdatedCallback =
      base::RepeatingCallback<void(id<SystemIdentity> identity)>;
  using IdentityAccessTokenRefreshFailedCallback =
      base::RepeatingCallback<void(id<SystemIdentity> identity,
                                   id<RefreshAccessTokenError> error)>;

  // `mapping_updated_cb` will be run every time any identities are added or
  // removed from any profiles.
  // `identity_updated_cb` and `identity_access_token_refresh_failed_cb`
  // correspond to the similarly-named methods on Observer.
  Assigner(
      SystemIdentityManager* system_identity_manager,
      ProfileManagerIOS* profile_manager,
      IdentitiesOnDeviceChangedCallback identitites_on_device_changed_cb,
      MappingUpdatedCallback mapping_updated_cb,
      IdentityUpdatedCallback identity_updated_cb,
      IdentityRefreshTokenUpdatedCallback identity_refresh_token_updated_cb,
      IdentityAccessTokenRefreshFailedCallback
          identity_access_token_refresh_failed_cb);
  ~Assigner() override;

  std::optional<std::string> FindProfileNameForGaiaID(
      std::string_view gaia_id) const;

  void MakePersonalProfileManagedWithGaiaID(std::string_view gaia_id);

  // SystemIdentityManagerObserver implementation.
  void OnIdentityListChanged() final;
  void OnIdentityUpdated(id<SystemIdentity> identity) final;
  void OnIdentityRefreshTokenUpdated(id<SystemIdentity> identity) final;
  void OnIdentityAccessTokenRefreshFailed(
      id<SystemIdentity> identity,
      id<RefreshAccessTokenError> error) final;

 private:
  // Returns the ProfileAttributesStorageIOS if available - it can be null in
  // tests where no ProfileManager exists.
  ProfileAttributesStorageIOS* GetProfileAttributesStorage();

  // Returns the name of the personal profile, queried from the
  // ProfileAttributesStorageIOS.
  std::string GetPersonalProfileName();

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
  // if necessary, based on whether it's a managed account or not. Note that the
  // assignment may happen asynchronously in some cases.
  void AssignIdentityToProfile(id<SystemIdentity> identity,
                               bool is_managed_account);

  // Re-fetches the account<->profile mappings from ProfileAttributesStorageIOS,
  // and if anything changed, notifies AccountProfileMapper via the callback.
  void MaybeUpdateCachedMappingAndNotify();

  raw_ptr<SystemIdentityManager> system_identity_manager_;
  base::ScopedObservation<SystemIdentityManager, SystemIdentityManagerObserver>
      system_identity_manager_observation_{this};

  raw_ptr<ProfileManagerIOS> profile_manager_;

  IdentitiesOnDeviceChangedCallback identitites_on_device_changed_cb_;
  MappingUpdatedCallback mapping_updated_cb_;
  IdentityUpdatedCallback identity_updated_cb_;
  IdentityRefreshTokenUpdatedCallback identity_refresh_token_updated_cb_;
  IdentityAccessTokenRefreshFailedCallback
      identity_access_token_refresh_failed_cb_;

  // The mapping from profile name to the list of attached Gaia IDs.
  // If `kSeparateProfilesForManagedAccounts` is enabled, this is a cache of
  // the data in ProfileAttributesStorageIOS. It is used to detect when any
  // mappings have changed.
  // If `kSeparateProfilesForManagedAccounts` is disabled, the data from
  // ProfileAttributesStorageIOS isn't used here, and all Gaia IDs are
  // nominally assigned to an empty profile name (just to detect changes to
  // the list - AccountProfileMapper won't do any filtering).
  ProfileNameToGaiaIds profile_to_gaia_ids_;

  base::WeakPtrFactory<Assigner> weak_ptr_factory_{this};
};

AccountProfileMapper::Assigner::Assigner(
    SystemIdentityManager* system_identity_manager,
    ProfileManagerIOS* profile_manager,
    IdentitiesOnDeviceChangedCallback identitites_on_device_changed_cb,
    MappingUpdatedCallback mapping_updated_cb,
    IdentityUpdatedCallback identity_updated_cb,
    IdentityRefreshTokenUpdatedCallback identity_refresh_token_updated_cb,
    IdentityAccessTokenRefreshFailedCallback
        identity_access_token_refresh_failed_cb)
    : system_identity_manager_(system_identity_manager),
      profile_manager_(profile_manager),
      identitites_on_device_changed_cb_(identitites_on_device_changed_cb),
      mapping_updated_cb_(mapping_updated_cb),
      identity_updated_cb_(identity_updated_cb),
      identity_refresh_token_updated_cb_(identity_refresh_token_updated_cb),
      identity_access_token_refresh_failed_cb_(
          identity_access_token_refresh_failed_cb) {
  CHECK(system_identity_manager_);
  if (!profile_manager_) {
    CHECK_IS_TEST();
  }

  system_identity_manager_observation_.Observe(system_identity_manager_);

  // TODO(crbug.com/355167413): Figure out how to keep pre-existing managed
  // accounts in the personal profile.
  profile_to_gaia_ids_ = GetMappingFromProfileAttributes(
      system_identity_manager_, GetProfileAttributesStorage());
  // Ensure the mapping is populated and up-to-date.
  OnIdentityListChanged();
}

AccountProfileMapper::Assigner::~Assigner() = default;

std::optional<std::string>
AccountProfileMapper::Assigner::FindProfileNameForGaiaID(
    std::string_view gaia_id) const {
  for (const auto& [profile_name, gaia_ids] : profile_to_gaia_ids_) {
    if (gaia_ids.contains(gaia_id)) {
      return profile_name;
    }
  }
  // The identity isn't assigned to any profile. This can happen (temporarily)
  // just after an identity is added to the device.
  return std::nullopt;
}

void AccountProfileMapper::Assigner::MakePersonalProfileManagedWithGaiaID(
    std::string_view managed_gaia_id) {
  CHECK(profile_manager_);

  ProfileAttributesStorageIOS* storage = GetProfileAttributesStorage();
  CHECK(storage);

  const std::string previous_personal_profile_name = GetPersonalProfileName();
  const std::optional<std::string> abandoned_managed_profile_name =
      FindProfileNameForGaiaID(managed_gaia_id);

  const std::set<std::string, std::less<>> personal_gaia_ids =
      profile_to_gaia_ids_[previous_personal_profile_name];

  // Detach all Gaia IDs from the old personal profile.
  for (const std::string& gaia_id : personal_gaia_ids) {
    DetachGaiaIdFromProfile(storage, previous_personal_profile_name, gaia_id);
  }
  // Delete the old managed profile (if it exists).
  if (abandoned_managed_profile_name) {
    // The old managed profile should still be "new", i.e. not actually created
    // on disk, so that no actual user data gets deleted here.
    CHECK(storage
              ->GetAttributesForProfileWithName(*abandoned_managed_profile_name)
              .IsNewProfile());
    // Also, the old managed profile mustn't be loaded, since it's going to be
    // deleted and there's no good way to unload it.
    CHECK(
        !profile_manager_->GetProfileWithName(*abandoned_managed_profile_name));

    // Since the profile doesn't exist on disk, just removing the entry in the
    // attributes storage is sufficient.
    storage->RemoveProfile(*abandoned_managed_profile_name);
  }

  // Register a new personal profile.
  const std::string new_personal_profile_name =
      profile_manager_->ReserveNewProfileName();
  storage->SetPersonalProfileName(new_personal_profile_name);

  // ..and re-interpret the previous personal profile as a managed profile.
  const std::string& new_managed_profile_name = previous_personal_profile_name;

  // Re-attach all relevant Gaia IDs to their new profiles.
  for (const std::string& gaia_id : personal_gaia_ids) {
    AttachGaiaIdToProfile(storage, new_personal_profile_name, gaia_id);
  }
  AttachGaiaIdToProfile(storage, new_managed_profile_name, managed_gaia_id);

  // Let observers know about the changes.
  MaybeUpdateCachedMappingAndNotify();
}

void AccountProfileMapper::Assigner::OnIdentityListChanged() {
  // Send notification about on-device identities first, before sending
  // per-profile ones.
  identitites_on_device_changed_cb_.Run();

  // Assign identities to profiles, if they're not assigned yet.
  std::set<std::string> processed_gaia_ids;
  system_identity_manager_->IterateOverIdentities(base::BindRepeating(
      &Assigner::ProcessIdentityForAssignmentToProfile, base::Unretained(this),
      std::ref(processed_gaia_ids)));

  // Check if any of the previously-assigned Gaia IDs have been removed.
  ProfileAttributesStorageIOS* attributes_storage =
      GetProfileAttributesStorage();
  if (AreSeparateProfilesForManagedAccountsEnabled() && attributes_storage) {
    for (const auto& [profile_name, gaia_ids] : profile_to_gaia_ids_) {
      for (const std::string& gaia_id : gaia_ids) {
        if (processed_gaia_ids.contains(gaia_id)) {
          // `gaia_id` still exists, nothing to be done.
          continue;
        }
        // `gaia_id` was removed from the device. Handle the removal, depending
        // on whether it was in the personal or in a managed profile.
        if (profile_name == attributes_storage->GetPersonalProfileName()) {
          // A personal identity was removed; clean it up from the mapping.
          DetachGaiaIdFromProfile(attributes_storage, profile_name, gaia_id);
        } else {
          // A managed identity was removed, so its corresponding profile
          // should be deleted. This is only possible if the profile isn't
          // currently loaded
          CHECK(profile_manager_);
          if (!profile_manager_->GetProfileWithName(profile_name)) {
            attributes_storage->RemoveProfile(profile_name);
            // TODO(crbug.com/331783685): Also delete the actual profile
            // folder from disk, once an API for that exists.
          }
          // Else: If the profile is currently loaded, do nothing. Deletion
          // will be attempted again the next time account-profile mappings
          // are processed (e.g. on the next browser restart).
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

void AccountProfileMapper::Assigner::OnIdentityRefreshTokenUpdated(
    id<SystemIdentity> identity) {
  identity_refresh_token_updated_cb_.Run(identity);
}

void AccountProfileMapper::Assigner::OnIdentityAccessTokenRefreshFailed(
    id<SystemIdentity> identity,
    id<RefreshAccessTokenError> error) {
  identity_access_token_refresh_failed_cb_.Run(identity, error);
}

ProfileAttributesStorageIOS*
AccountProfileMapper::Assigner::GetProfileAttributesStorage() {
  return profile_manager_ ? profile_manager_->GetProfileAttributesStorage()
                          : nullptr;
}

std::string AccountProfileMapper::Assigner::GetPersonalProfileName() {
  ProfileAttributesStorageIOS* attributes = GetProfileAttributesStorage();
  if (!attributes) {
    CHECK_IS_TEST();
    return std::string(kPersonalProfileNameForTesting);
  }
  return attributes->GetPersonalProfileName();
}

SystemIdentityManager::IteratorResult
AccountProfileMapper::Assigner::ProcessIdentityForAssignmentToProfile(
    std::set<std::string>& processed_gaia_ids,
    id<SystemIdentity> identity) {
  processed_gaia_ids.insert(base::SysNSStringToUTF8(identity.gaiaID));

  if (!AreSeparateProfilesForManagedAccountsEnabled()) {
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
  CHECK(AreSeparateProfilesForManagedAccountsEnabled());

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
  CHECK(AreSeparateProfilesForManagedAccountsEnabled());

  const std::string gaia_id = base::SysNSStringToUTF8(identity.gaiaID);

  // If the identity is already assigned to a profile, leave it there, even if
  // it's the wrong type of profile (i.e. managed identity assigned to the
  // personal profile). This can happen for pre-existing managed identities, and
  // they should not be automatically migrated.
  for (const auto& [profile_name, gaia_ids] : profile_to_gaia_ids_) {
    if (gaia_ids.contains(gaia_id)) {
      return;
    }
  }

  std::string new_profile_name;
  if (is_managed_account && profile_manager_) {
    // Managed account, assign to a new dedicated profile.
    new_profile_name = profile_manager_->ReserveNewProfileName();
    DCHECK(!new_profile_name.empty());
  } else {
    // Consumer account, assign to the personal profile.
    new_profile_name = GetPersonalProfileName();
  }
  AttachGaiaIdToProfile(GetProfileAttributesStorage(), new_profile_name,
                        gaia_id);
}

void AccountProfileMapper::Assigner::MaybeUpdateCachedMappingAndNotify() {
  // Build the effective new mapping from the persisted mapping in profile
  // attributes, plus the pending mappings.
  ProfileNameToGaiaIds new_mapping = GetMappingFromProfileAttributes(
      system_identity_manager_, GetProfileAttributesStorage());

  // If the effective mapping has changed, update the cache and notify.
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
  if (!profile_manager_) {
    CHECK_IS_TEST();
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  assigner_ = std::make_unique<Assigner>(
      system_identity_manager_, profile_manager_,
      base::BindRepeating(&AccountProfileMapper::IdentitiesOnDeviceChanged,
                          base::Unretained(this)),
      base::BindRepeating(&AccountProfileMapper::MappingUpdated,
                          base::Unretained(this)),
      base::BindRepeating(&AccountProfileMapper::IdentityUpdated,
                          base::Unretained(this)),
      base::BindRepeating(&AccountProfileMapper::IdentityRefreshTokenUpdated,
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

std::optional<std::string> AccountProfileMapper::FindProfileNameForGaiaID(
    std::string_view gaia_id) const {
  return assigner_->FindProfileNameForGaiaID(gaia_id);
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

void AccountProfileMapper::IterateOverAllIdentitiesOnDevice(
    IdentityIteratorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  system_identity_manager_->IterateOverIdentities(base::BindRepeating(
      [](IdentityIteratorCallback callback,
         id<SystemIdentity> identity) -> SystemIdentityManager::IteratorResult {
        switch (callback.Run(identity)) {
          case IteratorResult::kContinueIteration:
            return SystemIdentityManager::IteratorResult::kContinueIteration;
          case IteratorResult::kInterruptIteration:
            return SystemIdentityManager::IteratorResult::kInterruptIteration;
        }
      },
      callback));
}

void AccountProfileMapper::MakePersonalProfileManagedWithGaiaID(
    std::string_view gaia_id) {
  assigner_->MakePersonalProfileManagedWithGaiaID(gaia_id);
}

void AccountProfileMapper::IdentitiesOnDeviceChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& [name, observer_list] : observer_lists_per_profile_name_) {
    for (Observer& observer : observer_list) {
      observer.OnIdentitiesOnDeviceChanged();
    }
  }
}

void AccountProfileMapper::IdentityUpdated(id<SystemIdentity> identity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NotifyIdentityUpdated(identity,
                        assigner_->FindProfileNameForGaiaID(
                            base::SysNSStringToUTF8(identity.gaiaID)));
}

void AccountProfileMapper::IdentityRefreshTokenUpdated(
    id<SystemIdentity> identity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NotifyRefreshTokenUpdated(identity,
                            assigner_->FindProfileNameForGaiaID(
                                base::SysNSStringToUTF8(identity.gaiaID)));
}

void AccountProfileMapper::IdentityAccessTokenRefreshFailed(
    id<SystemIdentity> identity,
    id<RefreshAccessTokenError> error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NotifyAccessTokenRefreshFailed(identity, error,
                                 assigner_->FindProfileNameForGaiaID(
                                     base::SysNSStringToUTF8(identity.gaiaID)));
}

SystemIdentityManager::IteratorResult
AccountProfileMapper::FilterIdentitiesForProfile(
    std::string_view profile_name,
    IdentityIteratorCallback callback,
    id<SystemIdentity> identity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (AreSeparateProfilesForManagedAccountsEnabled() && profile_manager_) {
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
  if (AreSeparateProfilesForManagedAccountsEnabled()) {
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
  if (AreSeparateProfilesForManagedAccountsEnabled()) {
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
    const std::optional<std::string>& profile_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (AreSeparateProfilesForManagedAccountsEnabled()) {
    if (!profile_name.has_value()) {
      return;
    }
    auto it = observer_lists_per_profile_name_.find(*profile_name);
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

void AccountProfileMapper::NotifyRefreshTokenUpdated(
    id<SystemIdentity> identity,
    const std::optional<std::string>& profile_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (base::FeatureList::IsEnabled(kSeparateProfilesForManagedAccounts)) {
    if (!profile_name.has_value()) {
      return;
    }
    auto it = observer_lists_per_profile_name_.find(*profile_name);
    if (it == observer_lists_per_profile_name_.end()) {
      return;
    }
    for (Observer& observer : it->second) {
      observer.OnIdentityRefreshTokenUpdated(identity);
    }
  } else {
    // If the feature flag is not enabled, notify all profiles.
    for (const auto& [name, observer_list] : observer_lists_per_profile_name_) {
      for (Observer& observer : observer_list) {
        observer.OnIdentityRefreshTokenUpdated(identity);
      }
    }
  }
}

void AccountProfileMapper::NotifyAccessTokenRefreshFailed(
    id<SystemIdentity> identity,
    id<RefreshAccessTokenError> error,
    const std::optional<std::string>& profile_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (AreSeparateProfilesForManagedAccountsEnabled()) {
    if (!profile_name.has_value()) {
      return;
    }
    auto it = observer_lists_per_profile_name_.find(*profile_name);
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
