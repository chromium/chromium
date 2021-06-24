// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#include "base/strings/sys_string_conversions.h"
#include "google_apis/gaia/gaia_auth_util.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_interaction_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
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

}  // anonymous namespace

ChromeIdentityService::ChromeIdentityService() {}

ChromeIdentityService::~ChromeIdentityService() {
  for (auto& observer : observer_list_)
    observer.OnChromeIdentityServiceWillBeDestroyed();
}

void ChromeIdentityService::DismissDialogs() {}

bool ChromeIdentityService::HandleApplicationOpenURL(UIApplication* application,
                                                     NSURL* url,
                                                     NSDictionary* options) {
  return false;
}

bool ChromeIdentityService::HandleSessionOpenURLContexts(UIScene* scene,
                                                         NSSet* URLContexts) {
  return false;
}

void ChromeIdentityService::ApplicationDidDiscardSceneSessions(
    NSSet* scene_sessions) {}

DismissASMViewControllerBlock
ChromeIdentityService::PresentAccountDetailsController(
    ChromeIdentity* identity,
    UIViewController* view_controller,
    BOOL animated) {
  return nil;
}

DismissASMViewControllerBlock
ChromeIdentityService::PresentWebAndAppSettingDetailsController(
    ChromeIdentity* identity,
    UIViewController* view_controller,
    BOOL animated) {
  return nil;
}

ChromeIdentityInteractionManager*
ChromeIdentityService::CreateChromeIdentityInteractionManager(
    id<ChromeIdentityInteractionManagerDelegate> delegate) const {
  return nil;
}

void ChromeIdentityService::IterateOverIdentities(IdentityIteratorCallback) {}

bool ChromeIdentityService::IsValidIdentity(ChromeIdentity* identity) {
  return GetIdentityWithGaiaID(base::SysNSStringToUTF8(identity.gaiaID)) != nil;
}

ChromeIdentity* ChromeIdentityService::GetIdentityWithGaiaID(
    const std::string& gaia_id) {
  // Do not iterate if the gaia ID is invalid.
  if (gaia_id.empty())
    return nil;

  FunctorLookupIdentityByGaiaID helper(base::SysUTF8ToNSString(gaia_id));
  IterateOverIdentities(helper.Callback());
  return helper.identity;
}

bool ChromeIdentityService::HasIdentities() {
  FunctorHasIdentities helper;
  IterateOverIdentities(helper.Callback());
  return helper.has_identities;
}

NSArray* ChromeIdentityService::GetAllIdentities(PrefService* pref_service) {
  FunctorCollectIdentities helper;
  IterateOverIdentities(helper.Callback());
  return [helper.identities copy];
}

void ChromeIdentityService::ForgetIdentity(ChromeIdentity* identity,
                                           ForgetIdentityCallback callback) {}

void ChromeIdentityService::GetAccessToken(ChromeIdentity* identity,
                                           const std::set<std::string>& scopes,
                                           AccessTokenCallback callback) {}

void ChromeIdentityService::GetAccessToken(ChromeIdentity* identity,
                                           const std::string& client_id,
                                           const std::set<std::string>& scopes,
                                           AccessTokenCallback callback) {}

void ChromeIdentityService::GetAvatarForIdentity(ChromeIdentity* identity,
                                                 GetAvatarCallback callback) {}

UIImage* ChromeIdentityService::GetCachedAvatarForIdentity(
    ChromeIdentity* identity) {
  return nil;
}

void ChromeIdentityService::GetHostedDomainForIdentity(
    ChromeIdentity* identity,
    GetHostedDomainCallback callback) {}

NSString* ChromeIdentityService::GetCachedHostedDomainForIdentity(
    ChromeIdentity* identity) {
  // @gmail.com accounts are end consumer accounts so it is safe to return @""
  // even when SSOProfileSource has a nil profile for |sso_identity|.
  //
  // Note: This is also needed during the sign-in flow as it avoids waiting for
  // the profile of |sso_identity| to be fetched from the server.
  if (gaia::ExtractDomainName(base::SysNSStringToUTF8(identity.userEmail)) ==
      "gmail.com") {
    return @"";
  }
  return nil;
}

bool ChromeIdentityService::CanOfferExtendedSyncPromos(
    ChromeIdentity* identity) {
  return false;
}

MDMDeviceStatus ChromeIdentityService::GetMDMDeviceStatus(
    NSDictionary* user_info) {
  return 0;
}

bool ChromeIdentityService::HandleMDMNotification(ChromeIdentity* identity,
                                                  NSDictionary* user_info,
                                                  MDMStatusCallback callback) {
  return false;
}

bool ChromeIdentityService::IsMDMError(ChromeIdentity* identity,
                                       NSError* error) {
  return false;
}

void ChromeIdentityService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ChromeIdentityService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool ChromeIdentityService::IsInvalidGrantError(NSDictionary* user_info) {
  return false;
}

void ChromeIdentityService::FireIdentityListChanged(bool keychainReload) {
  for (auto& observer : observer_list_)
    observer.OnIdentityListChanged(keychainReload);
}

void ChromeIdentityService::FireAccessTokenRefreshFailed(
    ChromeIdentity* identity,
    NSDictionary* user_info) {
  for (auto& observer : observer_list_)
    observer.OnAccessTokenRefreshFailed(identity, user_info);
}

void ChromeIdentityService::FireProfileDidUpdate(ChromeIdentity* identity) {
  for (auto& observer : observer_list_)
    observer.OnProfileUpdate(identity);
}

}  // namespace ios
