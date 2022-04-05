// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/chrome_account_manager_service.h"

#include "base/check.h"
#include "base/strings/sys_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/signin/resized_avatar_cache.h"
#import "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Helper base class for functors.
template <typename T>
class Functor {
 public:
  explicit Functor(const PatternAccountRestriction& restriction,
                   const bool applies_to_restriction = false)
      : restriction_(restriction),
        applies_to_restriction_(applies_to_restriction) {}

  Functor(const Functor&) = delete;
  Functor& operator=(const Functor&) = delete;

  ios::ChromeIdentityService::IdentityIteratorCallback Callback() {
    // The callback is invoked synchronously and does not escape the scope
    // in which the Functor is defined. Thus it is safe to use Unretained
    // here.
    return base::BindRepeating(&Functor::Run, base::Unretained(this));
  }

 private:
  ios::IdentityIteratorCallbackResult Run(ChromeIdentity* identity) {
    // Filtering the ChromeIdentity.
    const std::string email = base::SysNSStringToUTF8(identity.userEmail);
    if (restriction_.IsAccountRestricted(email) != applies_to_restriction_)
      return ios::kIdentityIteratorContinueIteration;

    return static_cast<T*>(this)->Run(identity);
  }

  const PatternAccountRestriction& restriction_;
  const bool applies_to_restriction_;
};

// Helper class used to implement HasIdentities().
class FunctorHasIdentities : public Functor<FunctorHasIdentities> {
 public:
  explicit FunctorHasIdentities(const PatternAccountRestriction& restriction)
      : Functor(restriction) {}

  ios::IdentityIteratorCallbackResult Run(ChromeIdentity* identity) {
    has_identities_ = true;
    return ios::kIdentityIteratorInterruptIteration;
  }

  bool has_identities() const { return has_identities_; }

 private:
  bool has_identities_ = false;
};

// Helper class used to implement GetIdentityWithGaiaID().
class FunctorLookupIdentityByGaiaID
    : public Functor<FunctorLookupIdentityByGaiaID> {
 public:
  FunctorLookupIdentityByGaiaID(const PatternAccountRestriction& restriction,
                                NSString* gaia_id)
      : Functor(restriction), lookup_gaia_id_(gaia_id) {
    DCHECK(lookup_gaia_id_.length);
  }

  ios::IdentityIteratorCallbackResult Run(ChromeIdentity* identity) {
    if ([lookup_gaia_id_ isEqualToString:identity.gaiaID]) {
      identity_ = identity;
      return ios::kIdentityIteratorInterruptIteration;
    }
    return ios::kIdentityIteratorContinueIteration;
  }

  ChromeIdentity* identity() const { return identity_; }

 private:
  NSString* lookup_gaia_id_ = nil;
  ChromeIdentity* identity_ = nil;
};

// Helper class used to implement GetAllIdentities().
class FunctorCollectIdentities : public Functor<FunctorCollectIdentities> {
 public:
  FunctorCollectIdentities(const PatternAccountRestriction& restriction)
      : Functor(restriction), identities_([NSMutableArray array]) {}

  ios::IdentityIteratorCallbackResult Run(ChromeIdentity* identity) {
    [identities_ addObject:identity];
    return ios::kIdentityIteratorContinueIteration;
  }

  NSArray<ChromeIdentity*>* identities() const { return [identities_ copy]; }

 private:
  NSMutableArray<ChromeIdentity*>* identities_ = nil;
};

// Helper class used to implement GetDefaultIdentity().
class FunctorGetFirstIdentity : public Functor<FunctorGetFirstIdentity> {
 public:
  FunctorGetFirstIdentity(const PatternAccountRestriction& restriction)
      : Functor(restriction) {}

  ios::IdentityIteratorCallbackResult Run(ChromeIdentity* identity) {
    default_identity_ = identity;
    return ios::kIdentityIteratorInterruptIteration;
  }

  ChromeIdentity* default_identity() const { return default_identity_; }

 private:
  ChromeIdentity* default_identity_ = nil;
};

// Helper class used to implement HasRestrictedIdentities().
class FunctorHasRestrictedIdentities
    : public Functor<FunctorHasRestrictedIdentities> {
 public:
  explicit FunctorHasRestrictedIdentities(
      const PatternAccountRestriction& restriction)
      : Functor(restriction, /*applies_to_restriction*/ true) {}

  ios::IdentityIteratorCallbackResult Run(ChromeIdentity* identity) {
    has_restricted_identities_ = true;
    return ios::kIdentityIteratorInterruptIteration;
  }

  bool has_restricted_identities() const { return has_restricted_identities_; }

 private:
  bool has_restricted_identities_ = false;
};

// Returns the PatternAccountRestriction according to the given PrefService.
PatternAccountRestriction PatternAccountRestrictionFromPreference(
    PrefService* pref_service) {
  auto maybe_restriction = PatternAccountRestrictionFromValue(
      pref_service->GetList(prefs::kRestrictAccountsToPatterns)
          ->GetListDeprecated());
  return *std::move(maybe_restriction);
}

}  // anonymous namespace.

ChromeAccountManagerService::ChromeAccountManagerService(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  // pref_service is null in test environment. In prod environment pref_service
  // comes from GetApplicationContext()->GetLocalState() and couldn't be null.
  if (pref_service_) {
    registrar_.Init(pref_service_);
    registrar_.Add(
        prefs::kRestrictAccountsToPatterns,
        base::BindRepeating(&ChromeAccountManagerService::UpdateRestriction,
                            base::Unretained(this)));

    // Force initialisation of `restriction_`.
    UpdateRestriction();
  }

  browser_provider_observation_.Observe(&ios::GetChromeBrowserProvider());
  identity_service_observation_.Observe(
      ios::GetChromeBrowserProvider().GetChromeIdentityService());
}

ChromeAccountManagerService::~ChromeAccountManagerService() {}

bool ChromeAccountManagerService::HasIdentities() const {
  FunctorHasIdentities helper(restriction_);
  ios::GetChromeBrowserProvider()
      .GetChromeIdentityService()
      ->IterateOverIdentities(helper.Callback());
  return helper.has_identities();
}

bool ChromeAccountManagerService::HasRestrictedIdentities() const {
  FunctorHasRestrictedIdentities helper(restriction_);
  ios::GetChromeBrowserProvider()
      .GetChromeIdentityService()
      ->IterateOverIdentities(helper.Callback());
  return helper.has_restricted_identities();
}

bool ChromeAccountManagerService::IsValidIdentity(
    ChromeIdentity* identity) const {
  return GetIdentityWithGaiaID(identity.gaiaID) != nil;
}

bool ChromeAccountManagerService::IsEmailRestricted(
    base::StringPiece email) const {
  return restriction_.IsAccountRestricted(email);
}

ChromeIdentity* ChromeAccountManagerService::GetIdentityWithGaiaID(
    NSString* gaia_id) const {
  // Do not iterate if the gaia ID is invalid.
  if (!gaia_id.length)
    return nil;

  FunctorLookupIdentityByGaiaID helper(restriction_, gaia_id);
  ios::GetChromeBrowserProvider()
      .GetChromeIdentityService()
      ->IterateOverIdentities(helper.Callback());
  return helper.identity();
}

ChromeIdentity* ChromeAccountManagerService::GetIdentityWithGaiaID(
    base::StringPiece gaia_id) const {
  // Do not iterate if the gaia ID is invalid. This is duplicated here
  // to avoid allocating a NSString unnecessarily.
  if (gaia_id.empty())
    return nil;

  // Use the NSString* overload to avoid duplicating implementation.
  return GetIdentityWithGaiaID(base::SysUTF8ToNSString(gaia_id));
}

NSArray<ChromeIdentity*>* ChromeAccountManagerService::GetAllIdentities()
    const {
  FunctorCollectIdentities helper(restriction_);
  ios::GetChromeBrowserProvider()
      .GetChromeIdentityService()
      ->IterateOverIdentities(helper.Callback());
  return [helper.identities() copy];
}

ChromeIdentity* ChromeAccountManagerService::GetDefaultIdentity() const {
  FunctorGetFirstIdentity helper(restriction_);
  ios::GetChromeBrowserProvider()
      .GetChromeIdentityService()
      ->IterateOverIdentities(helper.Callback());
  return helper.default_identity();
}

UIImage* ChromeAccountManagerService::GetIdentityAvatarWithIdentity(
    ChromeIdentity* identity,
    IdentityAvatarSize avatar_size) {
  ResizedAvatarCache* avatar_cache =
      GetAvatarCacheForIdentityAvatarSize(avatar_size);
  DCHECK(avatar_cache);
  return [avatar_cache resizedAvatarForIdentity:identity];
}

bool ChromeAccountManagerService::IsServiceSupported() const {
  ios::ChromeIdentityService* identity_service =
      ios::GetChromeBrowserProvider().GetChromeIdentityService();
  return identity_service->IsServiceSupported();
}

void ChromeAccountManagerService::Shutdown() {
  if (pref_service_) {
    registrar_.RemoveAll();
    pref_service_ = nullptr;
  }
}

void ChromeAccountManagerService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ChromeAccountManagerService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ChromeAccountManagerService::OnAccessTokenRefreshFailed(
    ChromeIdentity* identity,
    NSDictionary* user_info) {
  for (auto& observer : observer_list_)
    observer.OnAccessTokenRefreshFailed(identity, user_info);
}

void ChromeAccountManagerService::OnIdentityListChanged(
    bool need_user_approval) {
  for (auto& observer : observer_list_)
    observer.OnIdentityListChanged(need_user_approval);
}

void ChromeAccountManagerService::OnProfileUpdate(ChromeIdentity* identity) {
  for (auto& observer : observer_list_)
    observer.OnIdentityChanged(identity);
}

void ChromeAccountManagerService::OnChromeIdentityServiceWillBeDestroyed() {
  identity_service_observation_.Reset();
}

void ChromeAccountManagerService::OnChromeIdentityServiceDidChange(
    ios::ChromeIdentityService* new_service) {
  identity_service_observation_.Observe(
      ios::GetChromeBrowserProvider().GetChromeIdentityService());
  // All avatar caches needs to be removed to avoid mixing fake identities and
  // sso identities.
  default_table_view_avatar_cache_ = nil;
  small_size_avatar_cache_ = nil;
  default_large_avatar_cache_ = nil;
  OnIdentityListChanged(false);
}

void ChromeAccountManagerService::OnChromeBrowserProviderWillBeDestroyed() {
  DCHECK(!identity_service_observation_.IsObserving());
  browser_provider_observation_.Reset();
}

void ChromeAccountManagerService::UpdateRestriction() {
  restriction_ = PatternAccountRestrictionFromPreference(pref_service_);
  // We want to notify the user that the account list has been updated. This
  // might provide notifications with no changes (if the new restriction doesn't
  // change the account list).
  OnIdentityListChanged(/* need_user_approval */ true);
}

ResizedAvatarCache*
ChromeAccountManagerService::GetAvatarCacheForIdentityAvatarSize(
    IdentityAvatarSize avatar_size) {
  ResizedAvatarCache* __strong* avatar_cache = nil;
  switch (avatar_size) {
    case IdentityAvatarSize::TableViewIcon:
      avatar_cache = &default_table_view_avatar_cache_;
      break;
    case IdentityAvatarSize::SmallSize:
      avatar_cache = &small_size_avatar_cache_;
      break;
    case IdentityAvatarSize::DefaultLarge:
      avatar_cache = &default_large_avatar_cache_;
      break;
  }
  DCHECK(avatar_cache);
  if (!*avatar_cache) {
    *avatar_cache =
        [[ResizedAvatarCache alloc] initWithIdentityAvatarSize:avatar_size];
  }
  return *avatar_cache;
}
