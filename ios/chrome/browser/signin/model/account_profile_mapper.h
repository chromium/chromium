// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_PROFILE_MAPPER_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_PROFILE_MAPPER_H_

#import <UIKit/UIKit.h>

#include <map>
#include <set>
#include <string>

#import "base/functional/callback.h"
#import "base/observer_list.h"
#import "base/observer_list_types.h"
#include "base/sequence_checker.h"
#import "ios/chrome/browser/signin/model/system_account_updater.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"

@protocol ChangeProfileCommands;
class GaiaId;
class PrefService;
class ProfileManagerIOS;
@protocol SystemIdentity;

// Enum for `Signin.IOSAccountProfileMappingStartupState` histogram.
// Entries should not be renumbered and numeric values should never be reused.
// LINT.IfChange(IOSAccountProfileMappingStartupState)
enum class IOSAccountProfileMappingStartupState {
  // Healthy state: each managed Gaia ID on the device is attached to its own
  // single Managed profile.
  kEachManagedAccountInSingleProfile = 0,

  // Exactly 1 managed Gaia ID is attached to 2 Managed profiles; `profiles[0]`
  // has Init=0 and `profiles[1]` has Init=1 (Cohort A: `UUID_M < orig_profile`,
  // so uninitialized `UUID_M` shadows `orig_profile`):
  // - A1: Personal profile (`UUID_P`) has Init=0 (`UUID_P` was never loaded).
  kCohortA1_ShadowedByUninitManaged_PersonalUninit = 1,
  // - A2: Personal profile (`UUID_P`) has Init=1 (`UUID_P` was loaded at least
  //   once).
  kCohortA2_ShadowedByUninitManaged_PersonalInit = 2,

  // Exactly 1 managed Gaia ID is attached to 2 Managed profiles; both
  // `profiles[0]` and `profiles[1]` have Init=1 (Cohort A3: user initialized
  // both `orig_profile` and `UUID_M`):
  // - A3a: `profiles[0]` (`UUID_M`) has a more recent `LastActiveTime` than
  //   both `profiles[1]` (`orig_profile`) and `UUID_P`.
  kCohortA3a_TwoInitManaged_FirstMoreRecent = 3,
  // - A3b: `profiles[0]` (`UUID_M`) has a more recent `LastActiveTime` than
  //   `profiles[1]` (`orig_profile`), and `UUID_P` has a more recent
  //   `LastActiveTime` than `UUID_M`.
  kCohortA3b_TwoInitManaged_PersonalMoreRecent = 4,
  // - A3c: `profiles[1]` (`orig_profile`) has a more recent `LastActiveTime`
  //   than `profiles[0]` (`UUID_M`) (unexpected, as `UUID_M` shadows
  //   `orig_profile`).
  kCohortA3c_TwoInitManaged_SecondMoreRecent = 5,

  // Exactly 1 managed Gaia ID is attached to 2 Managed profiles; `profiles[0]`
  // has Init=1 and `profiles[1]` has Init=0 (Cohort B: `orig_profile < UUID_M`,
  // so `orig_profile` is found first and uninitialized `UUID_M` is shadowed):
  // - B1: Personal profile (`UUID_P`) has Init=0, or `orig_profile` has a more
  //   recent `LastActiveTime` than `UUID_P`.
  kCohortB1_ExtraUninitManaged_OrigMoreRecent = 6,
  // - B2: Personal profile (`UUID_P`) has Init=1 and has a more recent
  //   `LastActiveTime` than `orig_profile`.
  kCohortB2_ExtraUninitManaged_PersonalMoreRecent = 7,
  // - Unexpected Cohort B case: both `profiles[0]` and `profiles[1]` have
  //   Init=1, and `profiles[0]` is named `"Default"` (so `profiles[0]` is
  //   `orig_profile` and `profiles[1]` is `UUID_M`, which should have been
  //   shadowed; when neither profile is `"Default"`, this case is
  //   indistinguishable from Cohort A3 above).
  kCohortB_TwoInitManaged_FirstIsDefault = 8,
  // - Unexpected Cohort B case: `profiles[0]` is named `"Default"` with Init=0
  //   and `profiles[1]` (`UUID_M`) has Init=1.
  kCohortB_FirstIsDefaultUninit_SecondInit = 9,

  // Exactly 1 managed Gaia ID is attached to 2 Managed profiles; both have
  // Init=0 (unexpected).
  kTwoUninitManagedProfiles = 10,

  // Exactly 1 managed Gaia ID is attached to 1 Personal profile (`UUID_P`) and
  // 1 Managed profile (`orig_profile`) (Cohort C):
  // - C1: Personal profile (`UUID_P`) has Init=0.
  kCohortC1_PersonalAndManaged_PersonalUninit = 11,
  // - C2: Personal profile (`UUID_P`) has Init=1, and this Gaia ID is NOT the
  //   primary account of `UUID_P`.
  kCohortC2_PersonalAndManaged_NotPrimaryInPersonal = 12,
  // - C3: Personal profile (`UUID_P`) has Init=1, and this Gaia ID IS the
  //   primary account of `UUID_P` (`UUID_P` has a more recent `LastActiveTime`
  //   than `orig_profile`).
  kCohortC3_PersonalAndManaged_PrimaryInPersonal = 13,
  // - C3 edge case (`C3 -> UUID_M2 -> orig_profile`): this Gaia ID IS the
  //   primary account of `UUID_P`, but the Managed profile (`orig_profile`) has
  //   a more recent `LastActiveTime` than `UUID_P`.
  kCohortC3_PersonalAndManaged_PrimaryInPersonal_ManagedMoreRecent = 14,

  // Exactly 1 managed Gaia ID is attached to 3 or more profiles (unexpected).
  kThreeOrMoreProfiles = 15,

  // 2 or more managed Gaia IDs on the same device are each attached to >= 2
  // profiles (unexpected).
  kMultipleCorruptedManagedAccounts = 16,

  kMaxValue = kMultipleCorruptedManagedAccounts,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:IOSAccountProfileMappingStartupState)

// Class to map the identities from SystemIdentityManager to profiles.
// Unless stated otherwise, this class and its `Observer` ignore the
// `kRestrictAccountsToPatterns` policy and deal with accounts on the device
// even if they are filtered by `kRestrictAccountsToPatterns`.
class AccountProfileMapper {
 public:
  // Observer handling events related to `AccountProfileMapper`.
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    ~Observer() override = default;

    // Called when the list of identities in a profile has changed.
    virtual void OnIdentitiesInProfileChanged() {}

    // Called when the list of identities on device has changed.
    virtual void OnIdentitiesOnDeviceChanged() {}

    // Called when information about an `identity` (such as the name or the
    // image) in a profile have been updated.
    virtual void OnIdentityInProfileUpdated(id<SystemIdentity> identity) {}

    // Called when information about an `identity` (such as the name or the
    // image) on the device have been updated.
    virtual void OnIdentityOnDeviceUpdated(id<SystemIdentity> identity) {}

    // Called on identity refresh token updated events.
    // `identity` is the identity for which the refresh token was updated.
    virtual void OnIdentityRefreshTokenUpdated(id<SystemIdentity> identity) {}

    // Called on access token refresh failed events.
    // `identity` is the identity for which the access token refresh failed.
    // `error` is an opaque type containing information about the error.
    virtual void OnIdentityAccessTokenRefreshFailed(
        id<SystemIdentity> identity,
        id<RefreshAccessTokenError> error,
        const std::set<std::string>& scopes) {}
  };

  // Value returned by IdentityIteratorCallback.
  enum class IteratorResult {
    kContinueIteration,
    kInterruptIteration,
  };

  // Callback invoked for each id<SystemIdentity> when iterating over them
  // with `IterateOverIdentities()`. The returned value can be used to stop
  // the iteration prematurely.
  using IdentityIteratorCallback =
      base::RepeatingCallback<IteratorResult(id<SystemIdentity>)>;

  AccountProfileMapper(SystemIdentityManager* system_identity_manager,
                       ProfileManagerIOS* profile_manager,
                       PrefService* local_pref_service);

  AccountProfileMapper(const AccountProfileMapper&) = delete;
  AccountProfileMapper& operator=(const AccountProfileMapper&) = delete;

  ~AccountProfileMapper();

  // Sets the ChangeProfileCommands handler.
  void SetChangeProfileCommandsHandler(id<ChangeProfileCommands> handler);

  // Adds/removes observers for a profile based on `profile_name`.
  void AddObserver(Observer* observer, std::string_view profile_name);
  void RemoveObserver(Observer* observer, std::string_view profile_name);

  // Returns whether signin is supported by the provider.
  bool IsSigninSupported();

  // Returns the name of the profile to which `gaia_id` is assigned, or nullopt
  // if no such profile exists. `gaia_id` does not need to correspond to an
  // account on the device.
  std::optional<std::string> FindProfileNameForGaiaID(
      const GaiaId& gaia_id) const;

  // Iterates over all known identities for `profile_name`, sorted by
  // the ordering used in system identity manager, which is typically based
  // on the keychain ordering of the accounts.
  void IterateOverIdentities(IdentityIteratorCallback callback,
                             std::string_view profile_name);

  // Iterates over all known identities on the device, i.e. including the ones
  // assigned to other profiles. Using this should be rare!
  void IterateOverAllIdentitiesOnDevice(IdentityIteratorCallback callback);

  // Returns the name of the personal profile, queried from the
  // ProfileAttributesStorageIOS.
  std::string GetPersonalProfileName();

  // Returns whether the profile assigned to `gaia_id` has been fully
  // initialized.
  bool IsProfileForGaiaIDFullyInitialized(const GaiaId& gaia_id);

  // Marks the personal profile as managed, attaches the given `gaia_id`, and
  // moves all personal accounts to a new empty personal profile. Deletes the
  // managed profile to which `gaia_id` was attached. That profile must not be
  // fully initialized yet (per ProfileAttributesIOS::IsFullyInitialized()).
  // This is meant for two situations:
  // 1. Signing in with a managed account during the FRE. In this case, there
  //    can't be any pre-existing local data, so no need to move to a new empty
  //    profile (and it's easier to continue the flow in the existing profile).
  // 2. Signing in with a managed account, while not signed in yet in the
  //    personal profile. In this case, the user *may* be offered to take
  //    existing local data along into the managed profile, which is implemented
  //    as converting the personal profile into a managed one.
  void MakePersonalProfileManagedWithGaiaID(const GaiaId& gaia_id);

  // For testing purposes, this moves the account with `gaia_id` from its
  // current (managed) profile into the personal profile. This simulates the
  // situation where a managed account was already signed in before profile
  // separation (this makes test setup much easier).
  void MoveManagedAccountToPersonalProfileForTesting(const GaiaId& gaia_id);

 private:
  class Assigner;

  using ProfileNameToGaiaIds =
      std::map<std::string, std::set<GaiaId, std::less<>>, std::less<>>;

  // Iterator callback for SystemIdentityManager. Calls `callback` when
  // receiving an identity assigned to the profile with `profile_name`.
  SystemIdentityManager::IteratorResult FilterIdentitiesForProfile(
      std::string_view profile_name,
      IdentityIteratorCallback callback,
      id<SystemIdentity> identity);

  // Called by the Assigner whenever the list of identities on the device
  // changes.
  void IdentitiesOnDeviceChanged();
  // Called by the Assigner when any profile<->account mappings have been
  // updated.
  void MappingUpdated(const ProfileNameToGaiaIds& old_mapping,
                      const ProfileNameToGaiaIds& new_mapping);
  // Called by the Assigner on the corresponding SystemIdentityManager events.
  void IdentityUpdated(id<SystemIdentity> identity);
  void IdentityRefreshTokenUpdated(id<SystemIdentity> identity);
  void IdentityAccessTokenRefreshFailed(id<SystemIdentity> identity,
                                        id<RefreshAccessTokenError> error,
                                        const std::set<std::string>& scopes);

  // Invokes `OnIdentityListChanged(...)` for all observers in
  // `profile_names_to_notify`.
  void NotifyIdentityListChanged(
      const std::set<std::string>& profile_names_to_notify);
  // Invokes `OnIdentityUpdated(...)` for all observers for `profile_name`.
  void NotifyIdentityUpdated(id<SystemIdentity> identity,
                             const std::optional<std::string>& profile_name);
  // Invokes `OnIdentityRefreshTokenUpdated(...)` for all observers for
  // the profile with `profile_name`.
  void NotifyRefreshTokenUpdated(
      id<SystemIdentity> identity,
      const std::optional<std::string>& profile_name);
  // Invokes `OnIdentityAccessTokenRefreshFailed(...)` for all observers for
  // the profile with `profile_name`.
  void NotifyAccessTokenRefreshFailed(
      id<SystemIdentity> identity,
      id<RefreshAccessTokenError> error,
      const std::optional<std::string>& profile_name,
      const std::set<std::string>& scopes);

  // The AccountProfileMapper is sequence-affine.
  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<SystemIdentityManager> system_identity_manager_;

  raw_ptr<ProfileManagerIOS> profile_manager_;

  std::unique_ptr<SystemAccountUpdater> system_account_updater_;

  std::unique_ptr<Assigner> assigner_;

  // TODO(crbug.com/484371187): Investigate if reentrancy can be removed.
  std::map<std::string,
           base::ObserverList<
               Observer,
               false,
               base::ObserverListReentrancyPolicy::kAllowReentrancyUntriaged>,
           std::less<>>
      observer_lists_per_profile_name_;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_PROFILE_MAPPER_H_
