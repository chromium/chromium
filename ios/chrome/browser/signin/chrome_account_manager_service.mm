// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/chrome_account_manager_service.h"

#include "base/check.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Helper base class for functors.
template <typename T>
struct Functor {
  Functor() = default;

  Functor(const Functor&) = delete;
  Functor& operator=(const Functor&) = delete;

  ios::ChromeIdentityService::IdentityIteratorCallback Callback() {
    // The callback is invoked synchronously and does not escape the scope
    // in which the Functor is defined. Thus it is safe to use Unretained
    // here.
    return base::BindRepeating(&Functor::Run, base::Unretained(this));
  }

  ios::IdentityIteratorCallbackResult Run(ChromeIdentity* identity) {
    // Filtering of the ChromeIdentity can be done here before calling
    // the sub-class `Run()` method. This will ensure that all functor
    // perform the same filtering (and thus consider exactly the same
    // identities).
    return static_cast<T*>(this)->Run(identity);
  }
};

// Helper class used to implement HasIdentities().
struct FunctorHasIdentities : Functor<FunctorHasIdentities> {
  bool has_identities = false;

  ios::IdentityIteratorCallbackResult Run(ChromeIdentity* identity) {
    has_identities = true;
    return ios::kIdentityIteratorInterruptIteration;
  }
};

// Helper class used to implement GetIdentityWithGaiaID().
struct FunctorLookupIdentityByGaiaID : Functor<FunctorLookupIdentityByGaiaID> {
  NSString* lookup_gaia_id;
  ChromeIdentity* identity;

  FunctorLookupIdentityByGaiaID(NSString* gaia_id)
      : lookup_gaia_id(gaia_id), identity(nil) {}

  ios::IdentityIteratorCallbackResult Run(ChromeIdentity* identity) {
    if ([lookup_gaia_id isEqualToString:identity.gaiaID]) {
      this->identity = identity;
      return ios::kIdentityIteratorInterruptIteration;
    }
    return ios::kIdentityIteratorContinueIteration;
  }
};

// Helper class used to implement GetAllIdentities().
struct FunctorCollectIdentities : Functor<FunctorCollectIdentities> {
  NSMutableArray<ChromeIdentity*>* identities;

  FunctorCollectIdentities() : identities([NSMutableArray array]) {}

  ios::IdentityIteratorCallbackResult Run(ChromeIdentity* identity) {
    [identities addObject:identity];
    return ios::kIdentityIteratorContinueIteration;
  }
};

// Helper class used to implement GetDefaultIdentity().
struct FunctorGetFirstIdentity : Functor<FunctorGetFirstIdentity> {
  ChromeIdentity* default_identity = nil;

  ios::IdentityIteratorCallbackResult Run(ChromeIdentity* identity) {
    default_identity = identity;
    return ios::kIdentityIteratorInterruptIteration;
  }
};

}  // anonymous namespace.

ChromeAccountManagerService::ChromeAccountManagerService(
    PrefService* pref_service) {
  DCHECK(pref_service);
}

bool ChromeAccountManagerService::HasIdentities() {
  FunctorHasIdentities helper;
  ios::GetChromeBrowserProvider()
      ->GetChromeIdentityService()
      ->IterateOverIdentities(helper.Callback());
  return helper.has_identities;
}

bool ChromeAccountManagerService::IsValidIdentity(ChromeIdentity* identity) {
  return GetIdentityWithGaiaID(identity.gaiaID) != nil;
}

ChromeIdentity* ChromeAccountManagerService::GetIdentityWithGaiaID(
    NSString* gaia_id) {
  // Do not iterate if the gaia ID is invalid.
  if (!gaia_id.length)
    return nil;

  FunctorLookupIdentityByGaiaID helper(gaia_id);
  ios::GetChromeBrowserProvider()
      ->GetChromeIdentityService()
      ->IterateOverIdentities(helper.Callback());
  return helper.identity;
}

ChromeIdentity* ChromeAccountManagerService::GetIdentityWithGaiaID(
    base::StringPiece gaia_id) {
  // Do not iterate if the gaia ID is invalid. This is duplicated here
  // to avoid allocating a NSString unnecessarily.
  if (gaia_id.empty())
    return nil;

  // Use the NSString* overload to avoid duplicating implementation.
  return GetIdentityWithGaiaID(base::SysUTF8ToNSString(gaia_id));
}

NSArray<ChromeIdentity*>* ChromeAccountManagerService::GetAllIdentities() {
  FunctorCollectIdentities helper;
  ios::GetChromeBrowserProvider()
      ->GetChromeIdentityService()
      ->IterateOverIdentities(helper.Callback());
  return [helper.identities copy];
}

ChromeIdentity* ChromeAccountManagerService::GetDefaultIdentity() {
  FunctorGetFirstIdentity helper;
  ios::GetChromeBrowserProvider()
      ->GetChromeIdentityService()
      ->IterateOverIdentities(helper.Callback());
  return helper.default_identity;
}

void ChromeAccountManagerService::Shutdown() {}
