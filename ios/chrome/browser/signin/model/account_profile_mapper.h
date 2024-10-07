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
#import "ios/chrome/browser/signin/model/system_identity_manager_observer.h"

@protocol SystemIdentity;
class SystemIdentityManager;

// Class to map the identities from SystemIdentityManager to profiles.
// TODO(crbug.com/331783685): If `kSeparateProfilesForManagedAccounts` is
// enabled, use the data from `ProfileAttributesIOS::GetAttachedGaiaIds()` to
// map identities into profiles.
// TODO(crbug.com/331783685): Hook up `AccountProfileMapper` between
// `SystemIdentityManager` and `ChromeAccountManagerService` (i.e. revert parts
// of crrev.com/c/5849614).
class AccountProfileMapper : public SystemIdentityManagerObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    ~Observer() override = default;

    // Called when the list of identity has changed.
    virtual void OnIdentityListChanged() {}

    // Called when information about `identity` (such as the name or the image)
    // have been updated.
    virtual void OnIdentityUpdated(id<SystemIdentity> identity) {}

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

  explicit AccountProfileMapper(SystemIdentityManager* system_identity_manager);

  AccountProfileMapper(const AccountProfileMapper&) = delete;
  AccountProfileMapper& operator=(const AccountProfileMapper&) = delete;

  ~AccountProfileMapper() override;

  // Adds/removes observers for a profile based on `profile_name`.
  void AddObserver(Observer* observer, std::string_view profile_name);
  void RemoveObserver(Observer* observer, std::string_view profile_name);

  // Returns whether signin is supported by the provider.
  bool IsSigninSupported();

  // Iterates over all known identities for `profile_name`, sorted by
  // the ordering used in system identity manager, which is typically based
  // on the keychain ordering of the accounts.
  // In rare cases, it is possible to receive new identities during this call
  // that was not notified by `OnIdentityListChanged()`. If that happen,
  // the `OnIdentityListChanged()` notification will happen right after this
  // call.
  void IterateOverIdentities(IdentityIteratorCallback callback,
                             std::string_view profile_name);

 private:
  // SystemIdentityManagerObserver implementation.
  void OnIdentityListChanged() final;
  void OnIdentityUpdated(id<SystemIdentity> identity) final;
  void OnIdentityAccessTokenRefreshFailed(
      id<SystemIdentity> identity,
      id<RefreshAccessTokenError> error) final;

  // Iterator callback for SystemIdentityManager, to update
  // `known_gaia_ids_before_iteration` and `profile_names_to_notify`.
  // Used by `OnIdentityListChanged()`.
  // `known_gaia_ids_before_iteration`: contains all current known gaia ids
  //   before the iteration. While iterating, gaia id from `identity` are
  //   removed from this set.
  // `profile_names_to_notify`: set of profile indexes that are updated during
  //   the iteration. `OnIdentityListChanged()` notification needs to be called
  //   for each profile after the iteration, by the caller.
  SystemIdentityManager::IteratorResult ProcessIdentityToUpdateMapping(
      std::set<std::string>& known_gaia_ids_before_iteration,
      std::set<std::string>& profile_names_to_notify,
      id<SystemIdentity> identity);

  // Iterator callback for SystemIdentityManager. Calls `callback` when
  // receiving an identity assigned to `profile_index` profile.
  // `profile_names_to_notify`: set of profile indexes that are updated during
  //   the iteration. `OnIdentityListChanged()` notification needs to be called
  //   for each profile after the iteration, by the caller.
  SystemIdentityManager::IteratorResult ProcessIdentitiesForProfile(
      std::string_view profile_name,
      std::set<std::string>& profile_names_to_notify,
      IdentityIteratorCallback callback,
      id<SystemIdentity> identity);

  // Checks `identity` has been assigned to its right profile, synchronously
  // if the cached hosted domain is available, or asynchronously otherwise.
  // If the hosted domain is fetch asynchronously `profile_names_to_notify`
  // is unmodified.
  // Returns true if the identity is assigned to a profile.
  // `profile_names_to_notify`: set of profile indexes that are updated during
  //   the iteration. Notification `OnIdentityListChanged()` needs to be called
  //   for each profile after the iteration.
  // Returns `true` if the identity is attached to a profile.
  bool CheckIdentityProfile(id<SystemIdentity> identity,
                            std::set<std::string>& profile_names_to_notify);

  // Updates the `identity` to the right profile according to `hosted_domain`.
  // And sends `OnIdentityListChanged()` notifications to the right profiles.
  void HostedDomainedFetched(id<SystemIdentity> identity,
                             NSString* hosted_domain,
                             NSError* error);

  // Sets or moves `identity` to the right profile according to `hosted_domain`.
  // `profile_names_to_notify`: set of profile indexes that are updated during
  // the iteration. Notification `OnIdentityListChanged()` needs to be called
  // for each profile after the iteration.
  void CheckIdentityProfileWithHostedDomain(
      id<SystemIdentity> identity,
      NSString* hosted_domain,
      std::set<std::string>& profile_names_to_notify);

  // Adds `identity` the right profile according to its `hosted_domain`, and
  // adds the profile index into `profile_names_to_notify`.
  // `identity` should not be already attached to a profile.
  void AddIdentityToProfile(id<SystemIdentity> identity,
                            NSString* hosted_domain,
                            std::set<std::string>& profile_names_to_notify);

  // Removes `identity` from its profile, and adds the profile index into
  // `profile_names_to_notify`. `identity` should be already attached to a
  // profile.
  void RemoveIdentityFromProfile(
      id<SystemIdentity> identity,
      std::set<std::string>& profile_names_to_notify);

  // Invokes `OnIdentityListChanged(...)` for all observers in
  // `profile_names_to_notify`.
  void NotifyIdentityListChanged(
      const std::set<std::string>& profile_names_to_notify);
  // Invokes `OnIdentityUpdated(...)` for all observers for `profile_name`.
  void NotifyIdentityUpdated(id<SystemIdentity> identity,
                             std::string_view profile_name);
  // Invokes `OnIdentityAccessTokenRefreshFailed(...)` for all observers for
  // the profile with `profile_name`.
  void NotifyAccessTokenRefreshFailed(id<SystemIdentity> identity,
                                      id<RefreshAccessTokenError> error,
                                      std::string_view profile_name);

  base::WeakPtr<AccountProfileMapper> GetWeakPtr();

  // The AccountProfileMapper is sequence-affine.
  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<SystemIdentityManager> system_identity_manager_;
  base::ScopedObservation<SystemIdentityManager, SystemIdentityManagerObserver>
      system_identity_manager_observation_{this};

  // Dictionary to assign each identity to a profile.
  std::map<std::string, std::string, std::less<>> profile_name_per_gaia_id_;

  // Registered observers.
  std::map<std::string, base::ObserverList<Observer>, std::less<>>
      observer_lists_per_profile_name_;

  base::WeakPtrFactory<AccountProfileMapper> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_PROFILE_MAPPER_H_
