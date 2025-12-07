// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_CHROME_ACCOUNT_MANAGER_SERVICE_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_CHROME_ACCOUNT_MANAGER_SERVICE_H_

#import <UIKit/UIKit.h>

#import <string_view>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/observer_list.h"
#import "base/scoped_observation.h"
#import "base/types/pass_key.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/prefs/pref_change_registrar.h"
#import "ios/chrome/browser/signin/model/account_profile_mapper.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/pattern_account_restriction.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

struct AccountInfo;
class DeviceAccountsProviderImpl;
class PrefService;
@protocol RefreshAccessTokenError;

// Service that provides SystemIdentities and avatar caches. Most methods only
// return the identities belonging to `profile_name_`  according to
// `AccountProfileMapper`, but this can also be used to access identities on
// device. This service filters out identities according to the
// RestrictAccountsToPatterns policy.
class ChromeAccountManagerService : public KeyedService,
                                    public AccountProfileMapper::Observer {
 public:
  // Observer handling events related to the ChromeAccountManagerService.
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override {}

    // Handles change events for per-profile identity list.
    // Notifications with no account list update are possible, this has to be
    // handled by the observer.
    virtual void OnIdentitiesInProfileChanged() {}

    // Handles change events for on-device identity list.
    // Notifications with no account list update are possible, this has to be
    // handled by the observer.
    virtual void OnIdentitiesOnDeviceChanged() {}

    // Called when an identity in this profile is updated.
    virtual void OnIdentityInProfileUpdated(id<SystemIdentity> identity) {}

    // Called when an identity on this device is updated.
    virtual void OnIdentityOnDeviceUpdated(id<SystemIdentity> identity) {}

    // Handles refresh token updated events.
    // `identity` is the identity for which the refresh token was updated.
    virtual void OnRefreshTokenUpdated(id<SystemIdentity> identity) {}

    // Handles access token refresh failed events.
    // `identity` is the the identity for which the access token refresh failed.
    // `error` is an opaque type containing information about the error.
    virtual void OnAccessTokenRefreshFailed(
        id<SystemIdentity> identity,
        id<RefreshAccessTokenError> error,
        const std::set<std::string>& scopes) {}

    // Called on Shutdown(), for observers that aren't KeyedServices to remove
    // their observers.
    virtual void OnChromeAccountManagerServiceShutdown(
        ChromeAccountManagerService* chrome_account_manager_service) {}
  };

  // Initializes the service, getting identities corresponding to `profile_name`
  // from the AccountProfileMapper.
  ChromeAccountManagerService(PrefService* local_state,
                              std::string_view profile_name);
  ChromeAccountManagerService(const ChromeAccountManagerService&) = delete;
  ChromeAccountManagerService& operator=(const ChromeAccountManagerService&) =
      delete;
  ~ChromeAccountManagerService() override;

  // Returns the name of the profile that this service belongs to.
  const std::string& GetProfileName() const;

  // Returns whether there is at least one identity in the profile of this
  // service.
  bool HasIdentities() const;

  // Returns whether `identity` is valid and belongs to the profile of this
  // service.
  bool IsValidIdentity(id<SystemIdentity> identity) const;

  // Returns whether `email` is restricted according to the
  // RestrictAccountsToPatterns policy.
  bool IsEmailRestricted(std::string_view email) const;

  // Returns the SystemIdentity with gaia ID equals to `gaia_id` or nil if
  // no matching identity is found in the profile of this service.
  id<SystemIdentity> GetIdentityWithGaiaID(const GaiaId& gaia_id) const;

  // Returns all SystemIdentity objects of the profile of this service, sorted
  // by the ordering used in the SystemIdentityManager, which is typically based
  // on the keychain ordering of accounts.
  NSArray<id<SystemIdentity>>* GetAllIdentities() const;

  // Returns the first SystemIdentity object of the profile of this service.
  id<SystemIdentity> GetDefaultIdentity() const;

  // Returns whether signin is supported.
  bool IsServiceSupported() const;

  // KeyedService implementation.
  void Shutdown() override;

  // Adds and removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the SystemIdentity with the given Gaia ID, or nil if no matching
  // identity exists on the device. Similar to GetIdentityWithGaiaID(), but as
  // opposed to that (and most other methods in this service), this also handles
  // accounts that are assigned to other profiles.
  id<SystemIdentity> GetIdentityOnDeviceWithGaiaID(const GaiaId& gaia_id) const;
  // Converts a vector of AccountInfos, as returned by
  // IdentityManager::GetAccountsOnDevice(), to `SystemIdentity`’s (by looking
  // them up via their Gaia IDs). Note that, as opposed to most other methods in
  // this service, this also handles accounts that are assigned to other
  // profiles.
  NSArray<id<SystemIdentity>>* GetIdentitiesOnDeviceWithGaiaIDs(
      const std::vector<AccountInfo>& account_infos) const;

  // For use by DeviceAccountsProviderImpl only, may not be called otherwise!
  // Returns all SystemIdentity objects that are available on the device,
  // including (as opposed to GetAllIdentities()) those that are assigned to
  // different profiles. (Identities that are restricted due to enterprise
  // policy are still filtered out.)
  NSArray<id<SystemIdentity>>* GetAllIdentitiesOnDevice(
      base::PassKey<DeviceAccountsProviderImpl>) const;

  // AccountProfileMapper::Observer implementation.
  void OnIdentitiesInProfileChanged() override;
  void OnIdentitiesOnDeviceChanged() override;
  void OnIdentityInProfileUpdated(id<SystemIdentity> identity) override;
  void OnIdentityOnDeviceUpdated(id<SystemIdentity> identity) override;
  void OnIdentityRefreshTokenUpdated(id<SystemIdentity> identity) override;
  void OnIdentityAccessTokenRefreshFailed(
      id<SystemIdentity> identity,
      id<RefreshAccessTokenError> error,
      const std::set<std::string>& scopes) override;

  // Gets base::WeakPtr to the object.
  base::WeakPtr<ChromeAccountManagerService> GetWeakPtr();

 private:
  // Updates PatternAccountRestriction with the current `local_state_`. If
  // `local_state_` is null, no identity will be filtered.
  void UpdateRestriction();

  // The local-state pref service, used to retrieve restricted patterns.
  raw_ptr<PrefService> local_state_ = nullptr;
  // Used to filter ChromeIdentities.
  PatternAccountRestriction restriction_;
  // Used to listen pref change.
  PrefChangeRegistrar registrar_;

  base::ObserverList<Observer, true> observer_list_;

  const std::string profile_name_;

  base::WeakPtrFactory<ChromeAccountManagerService> weak_ptr_factory_;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_CHROME_ACCOUNT_MANAGER_SERVICE_H_
