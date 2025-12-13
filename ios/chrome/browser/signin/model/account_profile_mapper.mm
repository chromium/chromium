// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_profile_mapper.h"

#import <Foundation/Foundation.h>

#import "base/auto_reset.h"
#import "base/check_is_test.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/policy/core/common/management/platform_management_service.h"
#import "components/prefs/pref_service.h"
#import "components/signin/core/browser/account_management_type_metrics_recorder.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/app/change_profile_commands.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_observer_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager_observer.h"
#import "net/base/backoff_entry.h"

namespace {

// Minimal number of profile fetches to try before deciding to call all fetches
// a failure.
const int kMinimalNumberOfRetry = 5;

const net::BackoffEntry::Policy kBackoffPolicy = {
    // Number of initial errors to ignore before applying
    // exponential back-off rules.
    /*num_errors_to_ignore=*/0,

    // Initial delay is 1 second after the first error.
    /*initial_delay_ms=*/1 * 1000,

    // Factor by which the waiting time will be multiplied.
    /*multiply_factor=*/2,

    // Fuzzing percentage.
    /*jitter_factor=*/0.1,

    // No maximum delay.
    /*maximum_backoff_ms=*/-1,

    // After 10 minute without fetch, let’s reset.
    /*entry_lifetime_ms=*/10 * 60 * 1000,

    /*always_use_initial_delay=*/false,
};

// Name of the personal profile for tests.
constexpr char kPersonalProfileNameForTesting[] =
    "bf09f5cf-94cc-4336-9cc2-26a5e1b8c358";

using ProfileNameToGaiaIds =
    std::map<std::string, std::set<GaiaId, std::less<>>, std::less<>>;

// Stores attached gaia ids from `attr` into `mapping`.
void ExtractAttachedGaiaIds(ProfileNameToGaiaIds& mapping,
                            const ProfileAttributesIOS& attr) {
  mapping[attr.GetProfileName()] = attr.GetAttachedGaiaIds();
}

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
          result[std::string()].insert(identity.gaiaId);
          return SystemIdentityManager::IteratorResult::kContinueIteration;
        },
        std::ref(result)));
    return result;
  }

  if (!profile_attributes_storage) {
    CHECK_IS_TEST();
    return result;
  }

  profile_attributes_storage->IterateOverProfileAttributes(
      base::BindRepeating(&ExtractAttachedGaiaIds, std::ref(result)));

  return result;
}

// Enum for `Signin.IOSHostedDomainFetchEvent` histogram.
// Entries should not be renumbered and numeric values should never be reused.
// LINT.IfChange(IOSHostedDomainFetchEvent)
enum class HostedDomainFetchEvent {
  kStarted = 0,
  kFinishedWithSuccess = 1,
  kFinishedWithErrorWillRetry = 2,
  kFinishedWithErrorFinal = 3,
  kMaxValue = kFinishedWithErrorFinal
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:IOSHostedDomainFetchEvent)

void RecordHostedDomainFetchEvent(HostedDomainFetchEvent event) {
  base::UmaHistogramEnumeration("Signin.IOSHostedDomainFetchEvent", event);
}

// Enum for `Signin.AccountProfileStartupState2` histogram.
// Entries should not be renumbered and numeric values should never be reused.
// LINT.IfChange(AccountProfileStartupState)
enum class AccountProfileStartupState {
  kManagedAccountInPersonalProfile = 0,
  kManagedAccountInManagedProfile = 1,
  kPersonalAccountInManagedProfile = 2,
  kPersonalAccountInPersonalProfile = 3,
  kMaxValue = kPersonalAccountInPersonalProfile
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:AccountProfileStartupState)

void RecordAccountProfileStartupState(bool is_primary_account,
                                      bool is_personal_profile,
                                      bool is_managed_account) {
  if (!is_primary_account) {
    return;
  }

  AccountProfileStartupState state;

  if (is_personal_profile && !is_managed_account) {
    state = AccountProfileStartupState::kPersonalAccountInPersonalProfile;
  } else if (!is_personal_profile && is_managed_account) {
    state = AccountProfileStartupState::kManagedAccountInManagedProfile;
  } else if (is_personal_profile && is_managed_account) {
    state = AccountProfileStartupState::kManagedAccountInPersonalProfile;
  } else {
    state = AccountProfileStartupState::kPersonalAccountInManagedProfile;
  }

  base::UmaHistogramEnumeration("Signin.AccountProfileStartupState2", state);
}

}  // namespace

// Helper class that handles assignment of accounts to profiles. Specifically,
// it updates the "attached Gaia IDs" property in ProfileAttributesIOS, and
// calls back out into AccountProfileMapper whenever the mapping changes. Also
// propagates other SystemIdentityManagerObserver events out.
class AccountProfileMapper::Assigner
    : public SystemIdentityManagerObserver,
      public ProfileAttributesStorageObserverIOS {
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
                                   id<RefreshAccessTokenError> error,
                                   const std::set<std::string>& scopes)>;

  // `mapping_updated_cb` will be run every time any identities are added or
  // removed from any profiles.
  // `identity_updated_cb` and `identity_access_token_refresh_failed_cb`
  // correspond to the similarly-named methods on Observer.
  Assigner(
      SystemIdentityManager* system_identity_manager,
      ProfileManagerIOS* profile_manager,
      PrefService* local_pref_service,
      IdentitiesOnDeviceChangedCallback identitites_on_device_changed_cb,
      MappingUpdatedCallback mapping_updated_cb,
      IdentityUpdatedCallback identity_updated_cb,
      IdentityRefreshTokenUpdatedCallback identity_refresh_token_updated_cb,
      IdentityAccessTokenRefreshFailedCallback
          identity_access_token_refresh_failed_cb);
  ~Assigner() override;

  void SetChangeProfileCommandsHandler(id<ChangeProfileCommands> handler) {
    handler_ = handler;
  }

  std::optional<std::string> FindProfileNameForGaiaID(
      const GaiaId& gaia_id) const;

  std::string GetPersonalProfileName();

  bool IsProfileForGaiaIDFullyInitialized(const GaiaId& gaia_id);
  void MakePersonalProfileManagedWithGaiaID(
      const GaiaId& managed_gaia_id,
      bool migrating_primary_managed_account = false);
  void MoveManagedAccountToPersonalProfileForTesting(  // IN-TEST
      const GaiaId& managed_gaia_id);

  // SystemIdentityManagerObserver implementation.
  void OnIdentityListChanged() final;
  void OnIdentityUpdated(id<SystemIdentity> identity) final;
  void OnIdentityRefreshTokenUpdated(id<SystemIdentity> identity) final;
  void OnIdentityAccessTokenRefreshFailed(
      id<SystemIdentity> identity,
      id<RefreshAccessTokenError> error,
      const std::set<std::string>& scopes) final;

  // ProfileAttributesStorageObserverIOS implementation.
  void OnProfileAttributesUpdated(std::string_view profile_name) final;

 private:
  // Returns the ProfileAttributesStorageIOS if available - it can be null in
  // tests where no ProfileManager exists.
  ProfileAttributesStorageIOS* GetProfileAttributesStorage();

  // Helpers for managing the mapping of accounts to profiles. Before calling
  // these, the caller must have set `is_updating_profile_attributes_storage_`
  // to true.
  void AttachGaiaIdToProfile(std::string_view profile_name,
                             const GaiaId& gaia_id);
  void DetachGaiaIdFromProfile(std::string_view profile_name,
                               const GaiaId& gaia_id);

  // Helper to delete a profile given its name. Before calling this, the caller
  // must have set `is_updating_profile_attributes_storage_` to true (since
  // deleting the profile also triggers an attributes-changed notification).
  void DeleteProfileNamed(std::string_view name);

  // Iterates over all identities and, if necessary, assigns them to profiles.
  // Also cleans up mappings (and related profiles) if identities have been
  // removed from the device.
  void UpdateIdentityProfileMappings();
  // Callback for SystemIdentityManager::IterateOverIdentities(). Checks the
  // mapping of `identity` to a profile, and attaches (or re-attaches) it as
  // necessary. Note that the attaching may happen asynchronously, if the hosted
  // domain needs to be fetched first.
  SystemIdentityManager::IteratorResult ProcessIdentityForAssignmentToProfile(
      std::set<GaiaId>& processed_gaia_ids,
      id<SystemIdentity> identity);
  // Fetches the hosted domain for the last entry of
  // `system_identities_to_fetch_`.
  void FetchHostedDomainNow();
  // Fetches the hosted domain for the last entry of
  // `system_identities_to_fetch_` asynchronously according to the backoff
  // policy.
  void FetchHostedDomain();
  // Called when the hosted domain for `identity` has been fetched
  // asynchronously. Triggers the assignment to an appropriate profile.
  void HostedDomainFetched(NSString* hosted_domain, NSError* error);
  HostedDomainFetchEvent HostedDomainFetchedImpl(NSString* hosted_domain,
                                                 NSError* error);
  // Ensure that each identity is fetched at least twice, and
  // kMinimalNumberOfRetry fetches are tried.
  void ResetNumberOfFetchTries();
  // Assigns `identity` to a profile (or re-assigns it to a different profile)
  // if necessary, based on whether it's a managed account or not. Note that the
  // assignment may happen asynchronously in some cases.
  void AssignIdentityToProfile(id<SystemIdentity> identity,
                               bool is_managed_account);
  // Handles migrating pre-multi-profile managed accounts when it is ready for
  // migration, otherwise updates the migration prefs.
  void MaybeMigratePrimaryManagedAccount(const GaiaId gaia_id);

  // Re-fetches the account<->profile mappings from ProfileAttributesStorageIOS,
  // and if anything changed, notifies AccountProfileMapper via the callback.
  void MaybeUpdateCachedMappingAndNotify();

  raw_ptr<SystemIdentityManager> system_identity_manager_;
  base::ScopedObservation<SystemIdentityManager, SystemIdentityManagerObserver>
      system_identity_manager_observation_{this};

  raw_ptr<ProfileManagerIOS> profile_manager_;

  raw_ptr<PrefService> local_pref_service_;

  // The ChangeProfileCommands handler. If nil, the code assumes that there
  // is not UI loaded yet and that it is safe to delete profiles directly
  // using the ProfileManagerIOS.
  __weak id<ChangeProfileCommands> handler_;

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

  // The system identities for which the hosted domain must be fetched. Last
  // identity of the array is fetched first. If an identity is currently being
  // fetched, it’s the first one.
  NSMutableArray<id<SystemIdentity>>* system_identities_to_fetch_ =
      [NSMutableArray array];
  // The identities for which fetching the hosted domain has repeatedly failed,
  // and should not be attempted again until the next browser restart. (As
  // opposed to `system_identities_to_fetch_`, this stores Gaia IDs instead of
  // the actual SystemIdentity objects, to avoid retaining them.)
  base::flat_set<GaiaId> gaia_ids_failed_fetching_;

  // Number of time we try to fetch an identity’s hosted domain before stopping
  // all tries.
  int number_of_remaining_tries_ = 0;

  // The back off entry deciding when to retry fetching an identity.
  net::BackoffEntry backoff_entry_{&kBackoffPolicy};

  // Set to true while this class is updating ProfileAttributesStorageIOS. Used
  // to avoid self-notifying which would lead to infinite loops.
  bool is_updating_profile_attributes_storage_ = false;

  base::WeakPtrFactory<Assigner> weak_ptr_factory_{this};
};

AccountProfileMapper::Assigner::Assigner(
    SystemIdentityManager* system_identity_manager,
    ProfileManagerIOS* profile_manager,
    PrefService* local_pref_service,
    IdentitiesOnDeviceChangedCallback identitites_on_device_changed_cb,
    MappingUpdatedCallback mapping_updated_cb,
    IdentityUpdatedCallback identity_updated_cb,
    IdentityRefreshTokenUpdatedCallback identity_refresh_token_updated_cb,
    IdentityAccessTokenRefreshFailedCallback
        identity_access_token_refresh_failed_cb)
    : system_identity_manager_(system_identity_manager),
      profile_manager_(profile_manager),
      local_pref_service_(local_pref_service),
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

  ProfileAttributesStorageIOS* storage = GetProfileAttributesStorage();
  profile_to_gaia_ids_ =
      GetMappingFromProfileAttributes(system_identity_manager_, storage);
  // Ensure the mapping is populated and up-to-date.
  UpdateIdentityProfileMappings();

  if (storage) {
    storage->AddObserver(this);
  }
}

AccountProfileMapper::Assigner::~Assigner() {
  ProfileAttributesStorageIOS* storage = GetProfileAttributesStorage();
  if (storage) {
    storage->RemoveObserver(this);
  }
}

std::optional<std::string>
AccountProfileMapper::Assigner::FindProfileNameForGaiaID(
    const GaiaId& gaia_id) const {
  for (const auto& [profile_name, gaia_ids] : profile_to_gaia_ids_) {
    if (gaia_ids.contains(gaia_id)) {
      return profile_name;
    }
  }
  // The identity isn't assigned to any profile. This can happen (temporarily)
  // just after an identity is added to the device.
  return std::nullopt;
}

bool AccountProfileMapper::Assigner::IsProfileForGaiaIDFullyInitialized(
    const GaiaId& gaia_id) {
  CHECK(profile_manager_);
  const std::optional<std::string> profile_name =
      FindProfileNameForGaiaID(gaia_id);
  if (!profile_name) {
    return false;
  }

  ProfileAttributesStorageIOS* storage = GetProfileAttributesStorage();
  CHECK(storage);
  return storage->GetAttributesForProfileWithName(*profile_name)
      .IsFullyInitialized();
}

void AccountProfileMapper::Assigner::MakePersonalProfileManagedWithGaiaID(
    const GaiaId& managed_gaia_id,
    bool migrating_primary_managed_account) {
  CHECK(profile_manager_);

  {
    base::AutoReset<bool> updating_attributes(
        &is_updating_profile_attributes_storage_, true);

    const std::string previous_personal_profile_name = GetPersonalProfileName();

    const std::set<GaiaId, std::less<>> personal_gaia_ids =
        profile_to_gaia_ids_[previous_personal_profile_name];

    // Detach all Gaia IDs from the old personal profile.
    for (const GaiaId& gaia_id : personal_gaia_ids) {
      DetachGaiaIdFromProfile(previous_personal_profile_name, gaia_id);
    }

    ProfileAttributesStorageIOS* storage = GetProfileAttributesStorage();
    CHECK(storage);

    if (!migrating_primary_managed_account) {
      CHECK(!IsProfileForGaiaIDFullyInitialized(managed_gaia_id));
      const std::optional<std::string> abandoned_managed_profile_name =
          FindProfileNameForGaiaID(managed_gaia_id);
      // Delete the old managed profile (if it exists).
      if (abandoned_managed_profile_name) {
        // The old managed profile must not have been initialized, so that no
        // actual user data gets deleted here.
        CHECK(!storage
                   ->GetAttributesForProfileWithName(
                       *abandoned_managed_profile_name)
                   .IsFullyInitialized());

        DeleteProfileNamed(*abandoned_managed_profile_name);
      }
    }

    // Register a new personal profile.
    const std::string new_personal_profile_name =
        profile_manager_->ReserveNewProfileName();
    storage->SetPersonalProfileName(new_personal_profile_name);

    // ..and re-interpret the previous personal profile as a managed profile.
    const std::string& new_managed_profile_name =
        previous_personal_profile_name;

    // Re-attach all relevant Gaia IDs to their new profiles.
    for (const GaiaId& gaia_id : personal_gaia_ids) {
      AttachGaiaIdToProfile(new_personal_profile_name, gaia_id);
    }
    AttachGaiaIdToProfile(new_managed_profile_name, managed_gaia_id);
  }

  if (migrating_primary_managed_account) {
    // At this point, the migration is done.
    local_pref_service_->ClearPref(
        prefs::kWaitingForMultiProfileForcedMigrationTimestamp);
    local_pref_service_->SetBoolean(prefs::kMultiProfileForcedMigrationDone,
                                    true);
    base::RecordAction(base::UserMetricsAction(
        "Signin_MultiProfileForcedMigration_MigrationDone"));
  }

  // Let observers know about the changes.
  MaybeUpdateCachedMappingAndNotify();
}

void AccountProfileMapper::Assigner::
    MoveManagedAccountToPersonalProfileForTesting(
        const GaiaId& managed_gaia_id) {
  CHECK(!IsProfileForGaiaIDFullyInitialized(managed_gaia_id));
  CHECK(profile_manager_);

  {
    base::AutoReset<bool> updating_attributes(
        &is_updating_profile_attributes_storage_, true);

    const std::string personal_profile_name = GetPersonalProfileName();

    ProfileAttributesStorageIOS* storage = GetProfileAttributesStorage();
    CHECK(storage);

    const std::optional<std::string> old_managed_profile_name =
        FindProfileNameForGaiaID(managed_gaia_id);
    CHECK(old_managed_profile_name.has_value());

    DetachGaiaIdFromProfile(*old_managed_profile_name, managed_gaia_id);
    AttachGaiaIdToProfile(personal_profile_name, managed_gaia_id);

    DeleteProfileNamed(*old_managed_profile_name);
  }

  // Let observers know about the changes.
  MaybeUpdateCachedMappingAndNotify();
}

void AccountProfileMapper::Assigner::OnIdentityListChanged() {
  // Send notification about on-device identities first, before sending
  // per-profile ones.
  identitites_on_device_changed_cb_.Run();

  // Assign identities to profiles, if they're not assigned yet.
  UpdateIdentityProfileMappings();
}

void AccountProfileMapper::Assigner::UpdateIdentityProfileMappings() {
  std::set<GaiaId> processed_gaia_ids;
  system_identity_manager_->IterateOverIdentities(base::BindRepeating(
      &Assigner::ProcessIdentityForAssignmentToProfile, base::Unretained(this),
      std::ref(processed_gaia_ids)));

  {
    base::AutoReset<bool> updating_attributes(
        &is_updating_profile_attributes_storage_, true);

    // Check if any of the previously-assigned Gaia IDs have been removed.
    ProfileAttributesStorageIOS* attributes_storage =
        GetProfileAttributesStorage();
    if (AreSeparateProfilesForManagedAccountsEnabled() && attributes_storage) {
      for (const auto& [profile_name, gaia_ids] : profile_to_gaia_ids_) {
        for (const GaiaId& gaia_id : gaia_ids) {
          if (processed_gaia_ids.contains(gaia_id)) {
            // `gaia_id` still exists, nothing to be done.
            continue;
          }
          // `gaia_id` was removed from the device. Handle the removal,
          // depending on whether it was in the personal or in a managed
          // profile.
          if (profile_name == attributes_storage->GetPersonalProfileName()) {
            // A personal identity was removed; clean it up from the mapping.
            DetachGaiaIdFromProfile(profile_name, gaia_id);
          } else {
            // A managed identity was removed, so its corresponding profile
            // should be deleted.
            DeleteProfileNamed(profile_name);
          }

          gaia_ids_failed_fetching_.erase(gaia_id);
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
  std::set<GaiaId> processed_gaia_ids_unused;
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
    id<RefreshAccessTokenError> error,
    const std::set<std::string>& scopes) {
  identity_access_token_refresh_failed_cb_.Run(identity, error, scopes);
}

void AccountProfileMapper::Assigner::OnProfileAttributesUpdated(
    std::string_view profile_name) {
  if (is_updating_profile_attributes_storage_) {
    return;
  }
  UpdateIdentityProfileMappings();
}

ProfileAttributesStorageIOS*
AccountProfileMapper::Assigner::GetProfileAttributesStorage() {
  return profile_manager_ ? profile_manager_->GetProfileAttributesStorage()
                          : nullptr;
}

void AccountProfileMapper::Assigner::AttachGaiaIdToProfile(
    std::string_view profile_name,
    const GaiaId& gaia_id) {
  CHECK(is_updating_profile_attributes_storage_);

  ProfileAttributesStorageIOS* profile_attributes_storage =
      GetProfileAttributesStorage();
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
      profile_name, base::BindOnce(
                        [](const GaiaId& gaia_id, ProfileAttributesIOS& attr) {
                          auto gaia_ids = attr.GetAttachedGaiaIds();
                          gaia_ids.insert(gaia_id);
                          attr.SetAttachedGaiaIds(gaia_ids);
                        },
                        gaia_id));
}

void AccountProfileMapper::Assigner::DetachGaiaIdFromProfile(
    std::string_view profile_name,
    const GaiaId& gaia_id) {
  CHECK(is_updating_profile_attributes_storage_);

  ProfileAttributesStorageIOS* profile_attributes_storage =
      GetProfileAttributesStorage();
  if (!profile_attributes_storage ||
      !profile_attributes_storage->HasProfileWithName(profile_name)) {
    CHECK_IS_TEST();
    return;
  }
  profile_attributes_storage->UpdateAttributesForProfileWithName(
      profile_name, base::BindOnce(
                        [](const GaiaId& gaia_id, ProfileAttributesIOS& attr) {
                          auto gaia_ids = attr.GetAttachedGaiaIds();
                          gaia_ids.erase(gaia_id);
                          attr.SetAttachedGaiaIds(gaia_ids);
                        },
                        gaia_id));
}

void AccountProfileMapper::Assigner::DeleteProfileNamed(std::string_view name) {
  CHECK(is_updating_profile_attributes_storage_);

  if (handler_) {
    [handler_ deleteProfile:name];
    return;
  }

  // This may be reached if DeleteProfileNamed(...) is called during the
  // AccountProfileMapper constructor (as the handler is set at a later
  // stage), during application shutdown (as the AccountProfileMapper may
  // outlive the handler) or during unit tests. In all cases there should
  // be no UI loaded and thus it is safe to simply mark the profile for
  // deletion directly via the ProfileManagerIOS.
  profile_manager_->MarkProfileForDeletion(name);
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
    std::set<GaiaId>& processed_gaia_ids,
    id<SystemIdentity> identity) {
  CHECK(identity, base::NotFatalUntil::M147);
  processed_gaia_ids.insert(identity.gaiaId);

  if (!AreSeparateProfilesForManagedAccountsEnabled()) {
    if (!local_pref_service_) {
      CHECK_IS_TEST();
    } else if (local_pref_service_->GetTime(
                   prefs::kWaitingForMultiProfileForcedMigrationTimestamp) !=
               base::Time()) {
      // Clear `kWaitingForMultiProfileForcedMigrationTimestamp` if the feature
      // gets disabled.
      local_pref_service_->ClearPref(
          prefs::kWaitingForMultiProfileForcedMigrationTimestamp);
    }
    // With the feature flag disabled, no actual assignment is necessary.
    return SystemIdentityManager::IteratorResult::kContinueIteration;
  }

  NSString* hosted_domain =
      system_identity_manager_->GetCachedHostedDomainForIdentity(identity);
  if (!hosted_domain) {
    // If the hosted domain is not in the cache yet, this identity can't be
    // assigned to a profile yet. Query it, and assign once available.

    if (![system_identities_to_fetch_ containsObject:identity] &&
        !gaia_ids_failed_fetching_.contains(identity.gaiaId)) {
      // If we have not yet planned to fetch this identity, let’s add it to the
      // list of identities to fetch and reset the total number of tries.
      [system_identities_to_fetch_ addObject:identity];
      ResetNumberOfFetchTries();
      if ([system_identities_to_fetch_ count] == 1) {
        // There was no other fetch planned, we need to plan it.
        FetchHostedDomain();
      }
      // Otherwise, we use the exponential backoff and async call from the
      // previous calls.
    }
    // Else: Fetching the hosted domain for this identity is already
    // scheduled; nothing to be done here.

    return SystemIdentityManager::IteratorResult::kContinueIteration;
  }

  bool is_managed_account = hosted_domain.length > 0;
  AssignIdentityToProfile(identity, is_managed_account);

  return SystemIdentityManager::IteratorResult::kContinueIteration;
}

void AccountProfileMapper::Assigner::ResetNumberOfFetchTries() {
  // Let’s retry at least twice for each identity, and at least
  // kMinimalNumberOfRetry times.
  int number_of_identities_to_fetch = [system_identities_to_fetch_ count];
  number_of_remaining_tries_ =
      (2 * number_of_identities_to_fetch > kMinimalNumberOfRetry)
          ? number_of_identities_to_fetch * 2
          : kMinimalNumberOfRetry;
}

void AccountProfileMapper::Assigner::FetchHostedDomainNow() {
  id<SystemIdentity> identity = [system_identities_to_fetch_ lastObject];
  // We must try to fetch the `identity`, the last identity of
  // `system_identities_to_fetch_`. `identity` was either added recently and not
  // yet fetched, or all other identities of the array have already failed to be
  // fetched once since the last time we tried to fetch `identity`. Moving
  // `identity` to the front of the array to note it’s the identity currently
  // being fetched and, in case of failure, ensure it’s only fetched once all
  // other identities are fetched. While inserting at index 0 in an array is
  // inneficient, the array should be small enough that the lost computation
  // time is negligeable compared to the time taken by the fetch request.
  [system_identities_to_fetch_ removeLastObject];
  [system_identities_to_fetch_ insertObject:identity atIndex:0];
  system_identity_manager_->GetHostedDomain(
      identity,
      base::BindOnce(&AccountProfileMapper::Assigner::HostedDomainFetched,
                     weak_ptr_factory_.GetWeakPtr()));
  RecordHostedDomainFetchEvent(HostedDomainFetchEvent::kStarted);
}

void AccountProfileMapper::Assigner::FetchHostedDomain() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AccountProfileMapper::Assigner::FetchHostedDomainNow,
                     weak_ptr_factory_.GetWeakPtr()),
      backoff_entry_.GetTimeUntilRelease());
}

void AccountProfileMapper::Assigner::HostedDomainFetched(
    NSString* hosted_domain,
    NSError* error) {
  HostedDomainFetchEvent outcome =
      HostedDomainFetchedImpl(hosted_domain, error);
  RecordHostedDomainFetchEvent(outcome);
}

HostedDomainFetchEvent AccountProfileMapper::Assigner::HostedDomainFetchedImpl(
    NSString* hosted_domain,
    NSError* error) {
  CHECK(AreSeparateProfilesForManagedAccountsEnabled());
  backoff_entry_.InformOfRequest(!error);
  if (error) {
    if (--number_of_remaining_tries_ > 0) {
      // Let’s try again.
      FetchHostedDomain();
      return HostedDomainFetchEvent::kFinishedWithErrorWillRetry;
    }
    // Each identity has failed to be fetched at least twice.
    // We had kMinimalNumberOfRetry consecutive fetch failures.
    // Let’s stop trying (until the next browser restart).
    for (id<SystemIdentity> identity : system_identities_to_fetch_) {
      gaia_ids_failed_fetching_.insert(identity.gaiaId);
    }
    [system_identities_to_fetch_ removeAllObjects];

    return HostedDomainFetchEvent::kFinishedWithErrorFinal;
  }

  id<SystemIdentity> identity = [system_identities_to_fetch_ firstObject];
  [system_identities_to_fetch_ removeObjectAtIndex:0];
  ResetNumberOfFetchTries();
  CHECK(hosted_domain);
  bool is_managed_account = hosted_domain.length > 0;
  AssignIdentityToProfile(identity, is_managed_account);
  if ([system_identities_to_fetch_ count] > 0) {
    // More domains to fetch.
    FetchHostedDomain();
  }

  MaybeUpdateCachedMappingAndNotify();

  return HostedDomainFetchEvent::kFinishedWithSuccess;
}

void AccountProfileMapper::Assigner::AssignIdentityToProfile(
    id<SystemIdentity> identity,
    bool is_managed_account) {
  CHECK(AreSeparateProfilesForManagedAccountsEnabled());

  const GaiaId gaia_id(identity.gaiaId);
  const std::optional<std::string> profile_name =
      FindProfileNameForGaiaID(gaia_id);

  if (profile_name) {
    // Found the profile! Check if it's the right kind of profile.
    bool is_personal_profile = (*profile_name == GetPersonalProfileName());
    bool is_primary_account = false;
    if (profile_manager_) {
      is_primary_account =
          (gaia_id == GetProfileAttributesStorage()
                          ->GetAttributesForProfileWithName(*profile_name)
                          .GetGaiaId());
    } else {
      CHECK_IS_TEST();
    }

    RecordAccountProfileStartupState(is_primary_account, is_personal_profile,
                                     is_managed_account);

    if (is_personal_profile == !is_managed_account) {
      // The account is already assigned to the right profile.
      return;
    }
    // The account is assigned to the "wrong" profile (managed account in the
    // personal profile, or vice versa). This can happen in two cases:
    // 1. A managed account was already the primary account before multi-profile
    // was supported. In this case, leave the account where it is; after a grace
    // period, the profile will be force-migrated to a managed one.
    // 2. (Very rarely) The account's managed-ness status changed. In this
    // cases, leave the account where it is iff it's currently the primary
    // account in its profile.
    if (is_primary_account) {
      if (is_personal_profile && is_managed_account) {
        // Handles case 1.
        MaybeMigratePrimaryManagedAccount(gaia_id);
      }
      return;
    }
  }

  base::AutoReset<bool> updating_attributes(
      &is_updating_profile_attributes_storage_, true);

  // The account needs to be assigned (or re-assigned) to a profile.

  if (profile_name) {
    // Allow re-assignment for secondary accounts, as the primary case was
    // handled above.
    DetachGaiaIdFromProfile(*profile_name, gaia_id);
  }

  std::string assigned_profile_name = GetPersonalProfileName();
  if (is_managed_account && profile_manager_) {
    // Managed account: Assign to a new dedicated profile, unless it's
    // currently the primary account in the personal profile.
    // TODO(crbug.com/408131474): Simplify this code, since the
    // already-assigned-primary-account case was handled above.
    ProfileAttributesIOS attr =
        GetProfileAttributesStorage()->GetAttributesForProfileWithName(
            GetPersonalProfileName());
    if (attr.GetGaiaId() != gaia_id) {
      assigned_profile_name = profile_manager_->ReserveNewProfileName();
      DCHECK(!assigned_profile_name.empty());
    }
    // Else: This managed account is the primary account in the personal
    // profile. That can happen if it was signed in before multi-profile was
    // supported. In that case, leave the account in the personal profile if
    // not ready for force migration yet.
  }
  AttachGaiaIdToProfile(assigned_profile_name, gaia_id);
}

void AccountProfileMapper::Assigner::MaybeMigratePrimaryManagedAccount(
    const GaiaId gaia_id) {
  CHECK(AreSeparateProfilesForManagedAccountsEnabled());

  base::Time recorded_at = local_pref_service_->GetTime(
      prefs::kWaitingForMultiProfileForcedMigrationTimestamp);
  if (recorded_at == base::Time()) {
    // Record force migration pref for managed accounts in personal profile if
    // not recoreded yet.
    local_pref_service_->SetTime(
        prefs::kWaitingForMultiProfileForcedMigrationTimestamp,
        base::Time::Now());
    return;
  }

  if (!base::FeatureList::IsEnabled(
          kSeparateProfilesForManagedAccountsForceMigration) ||
      base::Time::Now() - recorded_at <
          kMultiProfileMigrationGracePeriod.Get()) {
    return;
  }

  MakePersonalProfileManagedWithGaiaID(gaia_id, true);
}

void AccountProfileMapper::Assigner::MaybeUpdateCachedMappingAndNotify() {
  CHECK(!is_updating_profile_attributes_storage_);

  // Get the new mapping as persisted in profile attributes.
  ProfileNameToGaiaIds new_mapping = GetMappingFromProfileAttributes(
      system_identity_manager_, GetProfileAttributesStorage());

  // If the mapping has changed, update the cache and notify.
  if (new_mapping != profile_to_gaia_ids_) {
    auto old_mapping = std::move(profile_to_gaia_ids_);
    profile_to_gaia_ids_ = std::move(new_mapping);
    mapping_updated_cb_.Run(old_mapping, profile_to_gaia_ids_);
  }
}

AccountProfileMapper::AccountProfileMapper(
    SystemIdentityManager* system_identity_manager,
    ProfileManagerIOS* profile_manager,
    PrefService* local_pref_service)
    : system_identity_manager_(system_identity_manager),
      profile_manager_(profile_manager) {
  CHECK(system_identity_manager);
  if (!profile_manager_) {
    CHECK_IS_TEST();
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  system_account_updater_ =
      std::make_unique<SystemAccountUpdater>(system_identity_manager_);

  assigner_ = std::make_unique<Assigner>(
      system_identity_manager_, profile_manager_, local_pref_service,
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

  size_t num_consumer_accounts = 0;
  size_t num_managed_accounts = 0;
  size_t num_unknown_accounts = 0;
  IterateOverAllIdentitiesOnDevice(base::BindRepeating(
      [](SystemIdentityManager* system_identity_manager,
         size_t& num_consumer_accounts, size_t& num_managed_accounts,
         size_t& num_unknown_accounts, id<SystemIdentity> identity) {
        CHECK(identity, base::NotFatalUntil::M147);
        NSString* hosted_domain =
            system_identity_manager->GetCachedHostedDomainForIdentity(identity);
        if (hosted_domain) {
          bool is_managed_account = hosted_domain.length > 0;
          if (is_managed_account) {
            ++num_managed_accounts;
          } else {
            ++num_consumer_accounts;
          }
        } else {
          ++num_unknown_accounts;
        }
        return IteratorResult::kContinueIteration;
      },
      system_identity_manager_, std::ref(num_consumer_accounts),
      std::ref(num_managed_accounts), std::ref(num_unknown_accounts)));

  base::UmaHistogramCounts100(
      "Signin.IOSAccountsOnDeviceCount",
      num_consumer_accounts + num_managed_accounts + num_unknown_accounts);
  base::UmaHistogramCounts100("Signin.IOSAccountsOnDeviceCount.Consumer",
                              num_consumer_accounts);
  base::UmaHistogramCounts100("Signin.IOSAccountsOnDeviceCount.Managed",
                              num_managed_accounts);
  base::UmaHistogramCounts100("Signin.IOSAccountsOnDeviceCount.Unknown",
                              num_unknown_accounts);

  auto account_types_summary =
      signin::AccountManagementTypeMetricsRecorder::GetAccountTypesSummary(
          num_consumer_accounts, num_managed_accounts);
  base::UmaHistogramEnumeration(
      "Signin.IOSAccountsOnDeviceManagementTypesSummary",
      account_types_summary);
  if (policy::PlatformManagementService::GetInstance()->IsManaged()) {
    base::UmaHistogramEnumeration(
        "Signin.IOSAccountsOnDeviceManagementTypesSummary.ManagedDevice",
        account_types_summary);
  } else {
    base::UmaHistogramEnumeration(
        "Signin.IOSAccountsOnDeviceManagementTypesSummary.UnmanagedDevice",
        account_types_summary);
  }
  base::UmaHistogramBoolean(
      "Signin.IOSAccountsOnDeviceManagementTypesHadUnknownTypes",
      num_unknown_accounts > 0);
}

AccountProfileMapper::~AccountProfileMapper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void AccountProfileMapper::SetChangeProfileCommandsHandler(
    id<ChangeProfileCommands> handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  assigner_->SetChangeProfileCommandsHandler(handler);
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
    const GaiaId& gaia_id) const {
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

std::string AccountProfileMapper::GetPersonalProfileName() {
  return assigner_->GetPersonalProfileName();
}

bool AccountProfileMapper::IsProfileForGaiaIDFullyInitialized(
    const GaiaId& gaia_id) {
  return assigner_->IsProfileForGaiaIDFullyInitialized(gaia_id);
}

void AccountProfileMapper::MakePersonalProfileManagedWithGaiaID(
    const GaiaId& gaia_id) {
  assigner_->MakePersonalProfileManagedWithGaiaID(gaia_id);
}

void AccountProfileMapper::MoveManagedAccountToPersonalProfileForTesting(
    const GaiaId& gaia_id) {
  assigner_->MoveManagedAccountToPersonalProfileForTesting(gaia_id);  // IN-TEST
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
                        assigner_->FindProfileNameForGaiaID(identity.gaiaId));
}

void AccountProfileMapper::IdentityRefreshTokenUpdated(
    id<SystemIdentity> identity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NotifyRefreshTokenUpdated(
      identity, assigner_->FindProfileNameForGaiaID(identity.gaiaId));
}

void AccountProfileMapper::IdentityAccessTokenRefreshFailed(
    id<SystemIdentity> identity,
    id<RefreshAccessTokenError> error,
    const std::set<std::string>& scopes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NotifyAccessTokenRefreshFailed(
      identity, error, assigner_->FindProfileNameForGaiaID(identity.gaiaId),
      scopes);
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
    if (!attr.GetAttachedGaiaIds().contains(identity.gaiaId)) {
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
        continue;
      }
      for (Observer& observer : it->second) {
        observer.OnIdentitiesInProfileChanged();
      }
    }
  } else {
    // If the feature flag is not enabled, notify all profiles.
    for (const auto& [name, observer_list] : observer_lists_per_profile_name_) {
      for (Observer& observer : observer_list) {
        observer.OnIdentitiesInProfileChanged();
      }
    }
  }
}

void AccountProfileMapper::NotifyIdentityUpdated(
    id<SystemIdentity> identity,
    const std::optional<std::string>& profile_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Notify all observers (independent of profile) of updates to an identity on
  // the device.
  for (const auto& [name, observer_list] : observer_lists_per_profile_name_) {
    for (Observer& observer : observer_list) {
      observer.OnIdentityOnDeviceUpdated(identity);
    }
  }

  if (AreSeparateProfilesForManagedAccountsEnabled()) {
    // Notify observers for the affected profile.
    if (!profile_name.has_value()) {
      return;
    }
    auto it = observer_lists_per_profile_name_.find(*profile_name);
    if (it == observer_lists_per_profile_name_.end()) {
      return;
    }
    for (Observer& observer : it->second) {
      observer.OnIdentityInProfileUpdated(identity);
    }
  } else {
    // If the feature flag is not enabled, notify all profiles.
    for (const auto& [name, observer_list] : observer_lists_per_profile_name_) {
      for (Observer& observer : observer_list) {
        observer.OnIdentityInProfileUpdated(identity);
      }
    }
  }
}

void AccountProfileMapper::NotifyRefreshTokenUpdated(
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
    const std::optional<std::string>& profile_name,
    const std::set<std::string>& scopes) {
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
      observer.OnIdentityAccessTokenRefreshFailed(identity, error, scopes);
    }
  } else {
    // If the feature flag is not enabled, notify all profiles.
    for (const auto& [name, observer_list] : observer_lists_per_profile_name_) {
      for (Observer& observer : observer_list) {
        observer.OnIdentityAccessTokenRefreshFailed(identity, error, scopes);
      }
    }
  }
}
