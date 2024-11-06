// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_PROFILE_MAPPER_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_PROFILE_MAPPER_H_

#import <UIKit/UIKit.h>

#import <map>

#import "base/functional/callback.h"
#import "base/observer_list.h"
#import "base/observer_list_types.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"

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
    virtual void OnIdentityListChanged() {}

    // Called when information about `identity` (such as the name or the image)
    // have been updated.
    virtual void OnIdentityUpdated(id<SystemIdentity> identity) {}

    // Called on identity refresh token updated events.
    // `identity` is the the identity for which the refresh token was updated.
    virtual void OnIdentityRefreshTokenUpdated(id<SystemIdentity> identity) {}

    // Called on access token refresh failed events.
    // `identity` is the the identity for which the access token refresh failed.
    // `error` is an opaque type containing information about the error.
    virtual void OnIdentityAccessTokenRefreshFailed(
        id<SystemIdentity> identity,
        id<RefreshAccessTokenError> error) {}
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
                       ProfileManagerIOS* profile_manager);

  AccountProfileMapper(const AccountProfileMapper&) = delete;
  AccountProfileMapper& operator=(const AccountProfileMapper&) = delete;

  ~AccountProfileMapper();

  // Adds/removes observers for a profile based on `profile_name`.
  void AddObserver(Observer* observer, std::string_view profile_name);
  void RemoveObserver(Observer* observer, std::string_view profile_name);

  // Returns whether signin is supported by the provider.
  bool IsSigninSupported();

  // Iterates over all known identities for `profile_name`, sorted by
  // the ordering used in system identity manager, which is typically based
  // on the keychain ordering of the accounts.
  void IterateOverIdentities(IdentityIteratorCallback callback,
                             std::string_view profile_name);

  // Iterates over all known identities on the device, i.e. including the ones
  // assigned to other profiles. Using this should be rare!
  void IterateOverAllIdentitiesOnDevice(IdentityIteratorCallback callback);

 private:
  class Assigner;

  using ProfileNameToGaiaIds =
      std::map<std::string, std::set<std::string, std::less<>>, std::less<>>;

  // Iterator callback for SystemIdentityManager. Calls `callback` when
  // receiving an identity assigned to the profile with `profile_name`.
  SystemIdentityManager::IteratorResult FilterIdentitiesForProfile(
      std::string_view profile_name,
      IdentityIteratorCallback callback,
      id<SystemIdentity> identity);

  // Called by the Assigner when any profile<->account mappings have been
  // updated.
  void MappingUpdated(const ProfileNameToGaiaIds& old_mapping,
                      const ProfileNameToGaiaIds& new_mapping);
  // Called by the Assigner on the corresponding SystemIdentityManager events.
  void IdentityUpdated(id<SystemIdentity> identity);
  void IdentityRefreshTokenUpdated(id<SystemIdentity> identity);
  void IdentityAccessTokenRefreshFailed(id<SystemIdentity> identity,
                                        id<RefreshAccessTokenError> error);

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
                             std::string_view profile_name);
  // Invokes `OnIdentityRefreshTokenUpdated(...)` for all observers for
  // the profile with `profile_name`. If `kSeparateProfilesForManagedAccounts`
  // is disabled, all observers are notified, and `profile_name` is ignored.
  void NotifyRefreshTokenUpdated(id<SystemIdentity> identity,
                                 std::string_view profile_name);
  // Invokes `OnIdentityAccessTokenRefreshFailed(...)` for all observers for
  // the profile with `profile_name`. If `kSeparateProfilesForManagedAccounts`
  // is disabled, all observers are notified, and `profile_name` is ignored.
  void NotifyAccessTokenRefreshFailed(id<SystemIdentity> identity,
                                      id<RefreshAccessTokenError> error,
                                      std::string_view profile_name);

  // The AccountProfileMapper is sequence-affine.
  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<SystemIdentityManager> system_identity_manager_;

  raw_ptr<ProfileManagerIOS> profile_manager_;

  std::unique_ptr<Assigner> assigner_;

  // Registered observers.
  std::map<std::string, base::ObserverList<Observer>, std::less<>>
      observer_lists_per_profile_name_;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_PROFILE_MAPPER_H_
