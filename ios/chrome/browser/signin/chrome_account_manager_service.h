// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_CHROME_ACCOUNT_MANAGER_SERVICE_H_
#define IOS_CHROME_BROWSER_SIGNIN_CHROME_ACCOUNT_MANAGER_SERVICE_H_

#import <Foundation/Foundation.h>

#include "base/scoped_observation.h"
#include "base/strings/string_piece.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#import "ios/chrome/browser/signin/pattern_account_restriction.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

class PrefService;

// Service that provides Chrome identities.
class ChromeAccountManagerService : public KeyedService,
                                    ios::ChromeIdentityService::Observer,
                                    ios::ChromeBrowserProvider::Observer

{
 public:
  // Observer handling events related to the ChromeAccountManagerService.
  class Observer {
   public:
    Observer() {}
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    virtual ~Observer() {}

    // Handles identity list changed events.
    // If |need_user_approval| is true, the user need to approve the new account
    // list (related to SignedInAccountsViewController). Notifications with no
    // account list update are possible, this has to be handled by the observer.
    virtual void OnIdentityListChanged(bool need_user_approval) {}

    // Called when the identity is updated.
    virtual void OnIdentityChanged(ChromeIdentity* identity) {}
  };

  // Initializes the service.
  explicit ChromeAccountManagerService(PrefService* pref_service);
  ChromeAccountManagerService(const ChromeAccountManagerService&) = delete;
  ChromeAccountManagerService& operator=(const ChromeAccountManagerService&) =
      delete;
  ~ChromeAccountManagerService() override;

  // Returns true if there is at least one identity known by the service.
  bool HasIdentities() const;

  // Returns whether |identity| is valid and known by the service.
  bool IsValidIdentity(ChromeIdentity* identity) const;

  // Returns whether |email| is restricted.
  bool IsEmailRestricted(base::StringPiece email) const;

  // Returns the ChromeIdentity with gaia ID equals to |gaia_id| or nil if
  // no matching identity is found. There are two overloads to reduce the
  // need to convert between NSString* and std::string.
  ChromeIdentity* GetIdentityWithGaiaID(NSString* gaia_id) const;
  ChromeIdentity* GetIdentityWithGaiaID(base::StringPiece gaia_id) const;

  // Returns all ChromeIdentity objects, sorted by the ordering used in the
  // account manager, which is typically based on the keychain ordering of
  // accounts.
  NSArray<ChromeIdentity*>* GetAllIdentities() const;

  // Returns the first ChromeIdentity object.
  ChromeIdentity* GetDefaultIdentity() const;

  // KeyedService implementation.
  void Shutdown() override;

  // Adds and removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // ChromeIdentityServiceObserver implementation.
  void OnIdentityListChanged(bool need_user_approval) override;
  void OnProfileUpdate(ChromeIdentity* identity) override;
  void OnChromeIdentityServiceWillBeDestroyed() override;

  // ChromeBrowserProvider implementation.
  void OnChromeIdentityServiceDidChange(
      ios::ChromeIdentityService* new_service) override;
  void OnChromeBrowserProviderWillBeDestroyed() override;

 private:
  // Updates PatternAccountRestriction with the current pref_service_. If
  // pref_service_ is null, no identity will be filtered.
  void UpdateRestriction();

  // Used to retrieve restricted patterns.
  PrefService* pref_service_ = nullptr;
  // Used to filter ChromeIdentities.
  PatternAccountRestriction restriction_;
  // Used to listen pref change.
  PrefChangeRegistrar registrar_;

  base::ObserverList<Observer, true>::Unchecked observer_list_;
  base::ScopedObservation<ios::ChromeIdentityService,
                          ios::ChromeIdentityService::Observer>
      identity_service_observation_{this};
  base::ScopedObservation<ios::ChromeBrowserProvider,
                          ios::ChromeBrowserProvider::Observer>
      browser_provider_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_CHROME_ACCOUNT_MANAGER_SERVICE_H_
