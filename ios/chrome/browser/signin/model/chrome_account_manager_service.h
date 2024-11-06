// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_CHROME_ACCOUNT_MANAGER_SERVICE_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_CHROME_ACCOUNT_MANAGER_SERVICE_H_

#import <UIKit/UIKit.h>

#import <string_view>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/observer_list.h"
#import "base/scoped_observation.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/prefs/pref_change_registrar.h"
#import "ios/chrome/browser/signin/model/account_profile_mapper.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/pattern_account_restriction.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

struct AccountInfo;
class PrefService;
@protocol RefreshAccessTokenError;
@class ResizedAvatarCache;

// Service that provides SystemIdentities for use within a Chrome profile. In
// particular, it only passes on accounts that AccountProfileMapper has assigned
// to this profile, and it additionally filters out identities according to the
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

    // Handles identity list changed events.
    // Notifications with no account list update are possible, this has to be
    // handled by the observer.
    virtual void OnIdentityListChanged() {}

    // Called when the identity is updated.
    virtual void OnIdentityUpdated(id<SystemIdentity> identity) {}

    // Handles refresh token updated events.
    // `identity` is the identity for which the refresh token was updated.
    virtual void OnRefreshTokenUpdated(id<SystemIdentity> identity) {}

    // Handles access token refresh failed events.
    // `identity` is the the identity for which the access token refresh failed.
    // `error` is an opaque type containing information about the error.
    virtual void OnAccessTokenRefreshFailed(id<SystemIdentity> identity,
                                            id<RefreshAccessTokenError> error) {
    }

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

  // Returns true if there is at least one identity known by the service.
  bool HasIdentities() const;

  // Returns whether `identity` is valid and known by the service.
  bool IsValidIdentity(id<SystemIdentity> identity) const;

  // Returns whether `email` is restricted according to the
  // RestrictAccountsToPatterns policy.
  bool IsEmailRestricted(std::string_view email) const;

  // Returns the SystemIdentity with gaia ID equals to `gaia_id` or nil if
  // no matching identity is found. There are two overloads to reduce the
  // need to convert between NSString* and std::string.
  id<SystemIdentity> GetIdentityWithGaiaID(NSString* gaia_id) const;
  id<SystemIdentity> GetIdentityWithGaiaID(std::string_view gaia_id) const;

  // Returns all SystemIdentity objects, sorted by the ordering used in the
  // SystemIdentityManager, which is typically based on the keychain ordering of
  // accounts.
  NSArray<id<SystemIdentity>>* GetAllIdentities() const;

  // Returns the first SystemIdentity object.
  id<SystemIdentity> GetDefaultIdentity() const;

  // Returns the identity avatar. If the avatar is not available, it is fetched
  // in background (a notification will be received when it will be available),
  // and the default avatar is returned (see `Observer::OnIdentityUpdated()`).
  UIImage* GetIdentityAvatarWithIdentity(id<SystemIdentity> identity,
                                         IdentityAvatarSize size);

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
  id<SystemIdentity> GetIdentityOnDeviceWithGaiaID(
      std::string_view gaia_id) const;
  id<SystemIdentity> GetIdentityOnDeviceWithGaiaID(NSString* gaia_id) const;
  // Converts a vector of AccountInfos, as returned by
  // IdentityManager::GetAccountsOnDevice(), to `SystemIdentities (by looking
  // them up via their Gaia IDs). Note that, as opposed to most other methods in
  // this service, this also handles accounts that are assigned to other
  // profiles.
  NSArray<id<SystemIdentity>>* GetIdentitiesOnDeviceWithGaiaIDs(
      const std::vector<AccountInfo>& account_infos) const;

  // SystemIdentityManagerObserver implementation.
  void OnIdentityListChanged() override;
  void OnIdentityUpdated(id<SystemIdentity> identity) override;
  void OnIdentityRefreshTokenUpdated(id<SystemIdentity> identity) override;
  void OnIdentityAccessTokenRefreshFailed(
      id<SystemIdentity> identity,
      id<RefreshAccessTokenError> error) override;

 private:
  // Updates PatternAccountRestriction with the current `local_state_`. If
  // `local_state_` is null, no identity will be filtered.
  void UpdateRestriction();

  // Returns a ResizedAvatarCache based on `avatar_size`.
  ResizedAvatarCache* GetAvatarCacheForIdentityAvatarSize(
      IdentityAvatarSize avatar_size);

  // The local-state pref service, used to retrieve restricted patterns.
  raw_ptr<PrefService> local_state_ = nullptr;
  // Used to filter ChromeIdentities.
  PatternAccountRestriction restriction_;
  // Used to listen pref change.
  PrefChangeRegistrar registrar_;

  base::ObserverList<Observer, true> observer_list_;

  // ResizedAvatarCache for IdentityAvatarSize::TableViewIcon.
  ResizedAvatarCache* default_table_view_avatar_cache_;
  // ResizedAvatarCache for IdentityAvatarSize::SmallSize.
  ResizedAvatarCache* small_size_avatar_cache_;
  // ResizedAvatarCache for IdentityAvatarSize::Regular.
  ResizedAvatarCache* regular_avatar_cache_;
  // ResizedAvatarCache for IdentityAvatarSize::Large.
  ResizedAvatarCache* large_avatar_cache_;

  const std::string profile_name_;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_CHROME_ACCOUNT_MANAGER_SERVICE_H_
