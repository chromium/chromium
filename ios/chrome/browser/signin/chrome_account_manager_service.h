// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_CHROME_ACCOUNT_MANAGER_SERVICE_H_
#define IOS_CHROME_BROWSER_SIGNIN_CHROME_ACCOUNT_MANAGER_SERVICE_H_

#import <Foundation/Foundation.h>

#include "base/strings/string_piece.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#import "ios/chrome/browser/signin/pattern_account_restriction.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"

class PrefService;

// Service that provides Chrome identities.
class ChromeAccountManagerService : public KeyedService {
 public:
  // Initializes the service.
  explicit ChromeAccountManagerService(PrefService* pref_service);
  ChromeAccountManagerService(const ChromeAccountManagerService&) = delete;
  ChromeAccountManagerService& operator=(const ChromeAccountManagerService&) =
      delete;

  // Returns true if there is at least one identity known by the service.
  bool HasIdentities();

  // Returns whether |identity| is valid and known by the service.
  bool IsValidIdentity(ChromeIdentity* identity);

  // Returns the ChromeIdentity with gaia ID equals to |gaia_id| or nil if
  // no matching identity is found. There are two overloads to reduce the
  // need to convert between NSString* and std::string.
  ChromeIdentity* GetIdentityWithGaiaID(NSString* gaia_id);
  ChromeIdentity* GetIdentityWithGaiaID(base::StringPiece gaia_id);

  // Returns all ChromeIdentity objects, sorted by the ordering used in the
  // account manager, which is typically based on the keychain ordering of
  // accounts.
  NSArray<ChromeIdentity*>* GetAllIdentities();

  // Returns the first ChromeIdentity object.
  ChromeIdentity* GetDefaultIdentity();

  // KeyedService implementation.
  void Shutdown() override;

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
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_CHROME_ACCOUNT_MANAGER_SERVICE_H_
