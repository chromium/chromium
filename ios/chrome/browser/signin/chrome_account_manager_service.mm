// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/chrome_account_manager_service.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/resized_avatar_cache.h"
#import "ios/chrome/browser/signin/system_identity_manager.h"
#import "ios/public/provider/chrome/browser/signin/signin_identity_api.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using IteratorResult = SystemIdentityManager::IteratorResult;

// Filter class skipping restricted account.
class SkipRestricted {
 public:
  SkipRestricted(const PatternAccountRestriction& restriction)
      : restriction_(restriction) {}

  bool ShouldFilter(id<SystemIdentity> identity) const {
    return restriction_.IsAccountRestricted(
        base::SysNSStringToUTF8(identity.userEmail));
  }

 private:
  const PatternAccountRestriction& restriction_;
};

// Filter class skipping unrestricted account.
class KeepRestricted {
 public:
  KeepRestricted(const PatternAccountRestriction& restriction)
      : restriction_(restriction) {}

  bool ShouldFilter(id<SystemIdentity> identity) const {
    return !restriction_.IsAccountRestricted(
        base::SysNSStringToUTF8(identity.userEmail));
  }

 private:
  const PatternAccountRestriction& restriction_;
};

// Filter class skipping identities that do not have the given Gaia ID.
class KeepGaiaID {
 public:
  KeepGaiaID(NSString* gaia_id) : gaia_id_(gaia_id) { DCHECK(gaia_id_.length); }

  bool ShouldFilter(id<SystemIdentity> identity) const {
    return ![gaia_id_ isEqualToString:identity.gaiaID];
  }

 private:
  NSString* gaia_id_ = nil;
};

// Filter skipping identities if either sub-filter match.
template <typename F1, typename F2>
class CombineOr {
 public:
  CombineOr(F1&& f1, F2&& f2)
      : f1_(std::forward<F1>(f1)), f2_(std::forward<F2>(f2)) {}

  bool ShouldFilter(id<SystemIdentity> identity) const {
    return f1_.ShouldFilter(identity) || f2_.ShouldFilter(identity);
  }

 private:
  F1 f1_;
  F2 f2_;
};

// Helper class returning the first identity found when iterating
// over identities matching the filter.
class FindFirstIdentity {
 public:
  using ResultType = id<SystemIdentity>;

  IteratorResult ForEach(id<SystemIdentity> identity) {
    identity_ = identity;
    return IteratorResult::kInterruptIteration;
  }

  ResultType Result() const { return identity_; }

 private:
  id<SystemIdentity> identity_ = nil;
};

// Helper class returning the list of all identities matching the filter
// when iterating over identities.
class CollectIdentities {
 public:
  using ResultType = NSArray<id<SystemIdentity>>*;

  IteratorResult ForEach(id<SystemIdentity> identity) {
    [identities_ addObject:identity];
    return IteratorResult::kContinueIteration;
  }

  ResultType Result() const { return [identities_ copy]; }

 private:
  NSMutableArray<id<SystemIdentity>>* identities_ = [NSMutableArray array];
};

// Helper class implementing iteration in IterateOverIdentities.
template <typename T, typename F>
class Iterator {
 public:
  using ResultType = typename T::ResultType;

  Iterator(T t, F f) : t_(t), f_(f) {}

  IteratorResult Run(id<SystemIdentity> identity) {
    if (f_.ShouldFilter(identity))
      return IteratorResult::kContinueIteration;

    return t_.ForEach(identity);
  }

  ResultType Result() const { return t_.Result(); }

 private:
  T t_;
  F f_;
};

// Helper function to iterator over ChromeIdentityService identities.
template <typename T, typename F>
typename T::ResultType IterateOverIdentities(T t, F f) {
  using Iter = Iterator<T, F>;
  Iter iterator(std::move(t), std::move(f));
  GetApplicationContext()->GetSystemIdentityManager()->IterateOverIdentities(
      base::BindRepeating(&Iter::Run, base::Unretained(&iterator)));
  return iterator.Result();
}

// Returns the PatternAccountRestriction according to the given PrefService.
PatternAccountRestriction PatternAccountRestrictionFromPreference(
    PrefService* pref_service) {
  auto maybe_restriction = PatternAccountRestrictionFromValue(
      pref_service->GetList(prefs::kRestrictAccountsToPatterns));
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

  system_identity_manager_observation_.Observe(
      GetApplicationContext()->GetSystemIdentityManager());
}

ChromeAccountManagerService::~ChromeAccountManagerService() {}

bool ChromeAccountManagerService::HasIdentities() const {
  return IterateOverIdentities(FindFirstIdentity{},
                               SkipRestricted{restriction_}) != nil;
}

bool ChromeAccountManagerService::HasRestrictedIdentities() const {
  return IterateOverIdentities(FindFirstIdentity{},
                               KeepRestricted{restriction_}) != nil;
}

bool ChromeAccountManagerService::IsValidIdentity(
    id<SystemIdentity> identity) const {
  return GetIdentityWithGaiaID(identity.gaiaID) != nil;
}

bool ChromeAccountManagerService::IsEmailRestricted(
    base::StringPiece email) const {
  return restriction_.IsAccountRestricted(email);
}

id<SystemIdentity> ChromeAccountManagerService::GetIdentityWithGaiaID(
    NSString* gaia_id) const {
  // Do not iterate if the gaia ID is invalid.
  if (!gaia_id.length)
    return nil;

  return IterateOverIdentities(
      FindFirstIdentity{},
      CombineOr{SkipRestricted{restriction_}, KeepGaiaID{gaia_id}});
}

id<SystemIdentity> ChromeAccountManagerService::GetIdentityWithGaiaID(
    base::StringPiece gaia_id) const {
  // Do not iterate if the gaia ID is invalid. This is duplicated here
  // to avoid allocating a NSString unnecessarily.
  if (gaia_id.empty())
    return nil;

  // Use the NSString* overload to avoid duplicating implementation.
  return GetIdentityWithGaiaID(base::SysUTF8ToNSString(gaia_id));
}

NSArray<id<SystemIdentity>>* ChromeAccountManagerService::GetAllIdentities()
    const {
  return IterateOverIdentities(CollectIdentities{},
                               SkipRestricted{restriction_});
}

id<SystemIdentity> ChromeAccountManagerService::GetDefaultIdentity() const {
  return IterateOverIdentities(FindFirstIdentity{},
                               SkipRestricted{restriction_});
}

UIImage* ChromeAccountManagerService::GetIdentityAvatarWithIdentity(
    id<SystemIdentity> identity,
    IdentityAvatarSize avatar_size) {
  ResizedAvatarCache* avatar_cache =
      GetAvatarCacheForIdentityAvatarSize(avatar_size);
  DCHECK(avatar_cache);
  return [avatar_cache resizedAvatarForIdentity:identity];
}

bool ChromeAccountManagerService::IsServiceSupported() const {
  return GetApplicationContext()
      ->GetSystemIdentityManager()
      ->IsSigninSupported();
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

void ChromeAccountManagerService::OnIdentityListChanged(
    bool need_user_approval) {
  for (auto& observer : observer_list_)
    observer.OnIdentityListChanged(need_user_approval);
}

void ChromeAccountManagerService::OnIdentityUpdated(
    id<SystemIdentity> identity) {
  for (auto& observer : observer_list_)
    observer.OnIdentityUpdated(identity);
}

void ChromeAccountManagerService::OnIdentityAccessTokenRefreshFailed(
    id<SystemIdentity> identity,
    id<RefreshAccessTokenError> error) {
  for (auto& observer : observer_list_)
    observer.OnAccessTokenRefreshFailed(identity, error);
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
    case IdentityAvatarSize::Regular:
      avatar_cache = &regular_avatar_cache_;
      break;
    case IdentityAvatarSize::Large:
      avatar_cache = &large_avatar_cache_;
      break;
    case IdentityAvatarSize::ExtraLarge:
      avatar_cache = &extra_large_avatar_cache_;
      break;
  }
  DCHECK(avatar_cache);
  if (!*avatar_cache) {
    *avatar_cache =
        [[ResizedAvatarCache alloc] initWithIdentityAvatarSize:avatar_size];
  }
  return *avatar_cache;
}
