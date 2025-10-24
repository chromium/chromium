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
#import "base/scoped_observation.h"
#include "base/sequence_checker.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/signin/model/system_account_updater.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"

@protocol ChangeProfileCommands;
class GaiaId;
class PrefService;
class ProfileManagerIOS;
@protocol SystemIdentity;

// Class to map the identities from SystemIdentityManager to profiles.
class AccountProfileMapper {
 public:
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
    // `identity` is the the identity for which the refresh token was updated.
    virtual void OnIdentityRefreshTokenUpdated(id<SystemIdentity> identity) {}

    // Called on access token refresh failed events.
    // `identity` is the the identity for which the access token refresh failed.
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
  // if no such profile exists.
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
  // situation where a managed account was already signed in before
  // kSeparateProfilesForManagedAccounts was enabled (but makes test setup much
  // easier).
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
  // `profile_names_to_notify`. If `kSeparateProfilesForManagedAccounts` is
  // disabled, all observers are notified, and `profile_names_to_notify` is
  // ignored.
  void NotifyIdentityListChanged(
      const std::set<std::string>& profile_names_to_notify);
  // Invokes `OnIdentityUpdated(...)` for all observers for `profile_name`. If
  // `kSeparateProfilesForManagedAccounts` is disabled, all observers are
  // notified, and `profile_name` is ignored.
  void NotifyIdentityUpdated(id<SystemIdentity> identity,
                             const std::optional<std::string>& profile_name);
  // Invokes `OnIdentityRefreshTokenUpdated(...)` for all observers for
  // the profile with `profile_name`. If `kSeparateProfilesForManagedAccounts`
  // is disabled, all observers are notified, and `profile_name` is ignored.
  void NotifyRefreshTokenUpdated(
      id<SystemIdentity> identity,
      const std::optional<std::string>& profile_name);
  // Invokes `OnIdentityAccessTokenRefreshFailed(...)` for all observers for
  // the profile with `profile_name`. If `kSeparateProfilesForManagedAccounts`
  // is disabled, all observers are notified, and `profile_name` is ignored.
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

  // Registered observers.
  std::map<std::string, base::ObserverList<Observer>, std::less<>>
      observer_lists_per_profile_name_;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_PROFILE_MAPPER_H_
