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

// Class to map the identities from SystemIdentityManager to each available
// profiles.
// TODO(crbug.com/331783685): Need to save and load the mapping to the disk.
// Since the identities are always in the same order, after restart, if the
// identities are the same, the mapping should stay the same.
// TODO(crbug.com/331783685): Need to be create and remove profiles when needed.
// TODO(crbug.com/331783685): Need to replace profile index with a more robust
// way to identify a profile.
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

    // Handles access token refresh failed events.
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
                       size_t profile_count);

  AccountProfileMapper(const AccountProfileMapper&) = delete;
  AccountProfileMapper& operator=(const AccountProfileMapper&) = delete;

  ~AccountProfileMapper() override;

  // Adds/removes observers for a profile based on `profile_index`.
  void AddObserver(Observer* observer, size_t profile_index);
  void RemoveObserver(Observer* observer, size_t profile_index);

  // Returns whether signin is supported by the provider.
  bool IsSigninSupported();

  // Iterates over all known identities for `profile_index`, sortted by
  // the ordering used in system identity manager, which is typically based
  // on the keychain ordering of the accounts.
  // In rare cases, it is possible to receive new identities during this call
  // that was not notified by `OnIdentityListChanged()`. If that happen,
  // the `OnIdentityListChanged()` notification will happen right after this
  // call.
  void IterateOverIdentities(IdentityIteratorCallback callback,
                             size_t profile_index);

 private:
  // SystemIdentityManagerObserver implementation.
  void OnIdentityListChanged() final;
  void OnIdentityUpdated(id<SystemIdentity> identity) final;
  void OnIdentityAccessTokenRefreshFailed(
      id<SystemIdentity> identity,
      id<RefreshAccessTokenError> error) final;

  // Iterator callback for SystemIdentityManager, to update
  // `known_gaia_ids_before_iteration` and `profile_indexes_to_notify`.
  // Used by `OnIdentityListChanged()`.
  // `known_gaia_ids_before_iteration`: contains all current known gaia ids
  //   before the iteration. While iterating, gaia id from `identity` are
  //   removed from this set.
  // `profile_indexes_to_notify`: set of profile indexes that are updated during
  //   the iteration. `OnIdentityListChanged()` notification needs to be called
  //   for each profile after the iteration, by the caller.
  SystemIdentityManager::IteratorResult ProcessIdentityToUpdateMapping(
      NSMutableSet* known_gaia_ids_before_iteration,
      std::set<size_t>* profile_indexes_to_notify,
      id<SystemIdentity> identity);

  // Iterator callback for SystemIdentityManager. Calls `callback` when
  // receiving an identity assigned to `profile_index` profile.
  // `profile_indexes_to_notify`: set of profile indexes that are updated during
  //   the iteration. `OnIdentityListChanged()` notification needs to be called
  //   for each profile after the iteration, by the caller.
  SystemIdentityManager::IteratorResult ProcessIdentitiesForProfile(
      size_t profile_index,
      std::set<size_t>* profile_indexes_to_notify,
      IdentityIteratorCallback callback,
      id<SystemIdentity> identity);

  // Checks `identity` has been assigned to its right profile, synchronously
  // if the cached hosted domain is available, or asynchronously otherwise.
  // If the hosted domain is fetch asynchronously `profile_indexes_to_notify`
  // is unmodified.
  // Returns true if the identity is assigned to a profile.
  // `profile_indexes_to_notify`: set of profile indexes that are updated during
  //   the iteration. Notification `OnIdentityListChanged()` needs to be called
  //   for each profile after the iteration.
  // Returns `true` if the identity is attached to a profile.
  bool CheckIdentityProfile(id<SystemIdentity> identity,
                            std::set<size_t>* profile_indexes_to_notify);

  // Updates the `identity` to the right profile according to `hosted_domain`.
  // And sends `OnIdentityListChanged()` notifications to the right profiles.
  void HostedDomainedFetched(id<SystemIdentity> identity,
                             NSString* hosted_domain,
                             NSError* error);

  // Sets or moves `identity` to the right profile according to `hosted_domain`.
  // `profile_indexes_to_notify`: set of profile indexes that are updated during
  // the iteration. Notification `OnIdentityListChanged()` needs to be called
  // for each profile after the iteration.
  void CheckIdentityProfileWithHostedDomain(
      id<SystemIdentity> identity,
      NSString* hosted_domain,
      std::set<size_t>* profile_indexes_to_notify);

  // Adds `identity` the right profile according to its `hosted_domain`, and
  // adds the profile index into `profile_indexes_to_notify`.
  // `identity` should not be already attached to a profile.
  void AddIdentityToProfile(id<SystemIdentity> identity,
                            NSString* hosted_domain,
                            std::set<size_t>* profile_indexes_to_notify);

  // Removes `identity` from its profile, and adds the profile index into
  // `profile_indexes_to_notify`. `identity` should be already attached to a
  // profile.
  void RemoveIdentityFromProfile(id<SystemIdentity> identity,
                                 std::set<size_t>* profile_indexes_to_notify);

  // Invokes `OnIdentityListChanged(...)` for all observers for `profile_index`
  // profile.
  void NotifyIdentityListChanged(size_t profile_index);
  // Invokes `OnIdentityUpdated(...)` for all observers for `profile_index`
  // profile.
  void NotifyIdentityUpdated(id<SystemIdentity> identity, size_t profile_index);
  // Invokes `OnIdentityAccessTokenRefreshFailed(...)` for all observers for
  // `profile_index` profile.
  void NotifyAccessTokenRefreshFailed(id<SystemIdentity> identity,
                                      id<RefreshAccessTokenError> error,
                                      size_t profile_index);

  base::WeakPtr<AccountProfileMapper> GetWeakPtr();

  // The AccountProfileMapper is sequence-affine.
  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<SystemIdentityManager> system_identity_manager_;
  base::ScopedObservation<SystemIdentityManager, SystemIdentityManagerObserver>
      system_identity_manager_observation_{this};

  // Dictionary to assign an identity to a profile.
  __strong NSMutableDictionary<NSString*, NSNumber*>*
      profile_index_per_gaia_id_;

  // Number of profiles available.
  // TODO(crbug.com/331783685): This can be removed when APIs to create/remove
  // profiles will be available.
  const size_t profile_count_;

  // Registered observers.
  std::map<size_t, base::ObserverList<Observer>>
      observer_lists_per_profile_index_;

  base::WeakPtrFactory<AccountProfileMapper> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_PROFILE_MAPPER_H_
