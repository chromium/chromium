// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"

#import <string_view>

#import "base/check.h"
#import "base/memory/raw_ref.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/account_profile_mapper.h"
#import "ios/chrome/browser/signin/model/resized_avatar_cache.h"
#import "ios/public/provider/chrome/browser/signin/signin_identity_api.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"

namespace {

// In this file, we uses classes that implements the tow following traits.
// Predicate to decide which identity to filter.
// class Filter {
// public:
//  // Returns whether `identity` should be filtered out.
//  virtual bool ShouldFilter(id<SystemIdentity> identity) const;
// }
//
// // Helper to iterate over identities and gather some result of type
// `ResultType`.
// template <typename ResultType>
// class Collector {
//   // Returns whether iteration should continue or stop.
//   virtual IteratorResult ForEach(id<SystemIdentity> identity);
//   // Returns the result gathered thorugh the iteration.
//   virtual ResultType Result() const;
// }

using IteratorResult = SystemIdentityManager::IteratorResult;

// Filter class skipping restricted account.
class SkipRestricted {
 public:
  SkipRestricted(const PatternAccountRestriction& restriction)
      : restriction_(restriction) {}

  bool ShouldFilter(id<SystemIdentity> identity) const {
    return restriction_->IsAccountRestricted(
        base::SysNSStringToUTF8(identity.userEmail));
  }

 private:
  const raw_ref<const PatternAccountRestriction> restriction_;
};

// Filter class skipping unrestricted account.
class KeepRestricted {
 public:
  KeepRestricted(const PatternAccountRestriction& restriction)
      : restriction_(restriction) {}

  bool ShouldFilter(id<SystemIdentity> identity) const {
    return !restriction_->IsAccountRestricted(
        base::SysNSStringToUTF8(identity.userEmail));
  }

 private:
  const raw_ref<const PatternAccountRestriction> restriction_;
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
template <typename Filter1, typename Filter2>
class CombineOr {
 public:
  CombineOr(Filter1&& filter1, Filter2&& filter2)
      : filter1_(std::forward<Filter1>(filter1)),
        filter2_(std::forward<Filter2>(filter2)) {}

  bool ShouldFilter(id<SystemIdentity> identity) const {
    return filter1_.ShouldFilter(identity) || filter2_.ShouldFilter(identity);
  }

 private:
  Filter1 filter1_;
  Filter2 filter2_;
};

// Collector class returning the first identity found when iterating
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

// Collector class returning the list of all identities matching the filter
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
template <typename Collector, typename Filter>
class Iterator {
 public:
  using ResultType = typename Collector::ResultType;

  Iterator(Collector collector, Filter filter)
      : collector_(collector), filter_(filter) {}

  IteratorResult Run(id<SystemIdentity> identity) {
    if (filter_.ShouldFilter(identity)) {
      // `identity` is filtered out. So we don’t send it to `ForEach` and we
      // continue the iteration.
      return IteratorResult::kContinueIteration;
    }
    return collector_.ForEach(identity);
  }

  // The result of the collector.
  ResultType Result() const { return collector_.Result(); }

 private:
  Collector collector_;
  Filter filter_;
};

// Helper function to iterator over ChromeIdentityService identities.
// Return the collector’s result, after `collector` ’s `ForEach` received
// identities that `filter` did not filtered out. It receives all identities
// until the first kInterruptIteration.
template <typename Collector, typename Filter>
typename Collector::ResultType IterateOverIdentities(Collector collector,
                                                     Filter filter) {
  using Iter = Iterator<Collector, Filter>;
  Iter iterator(std::move(collector), std::move(filter));
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
  GetApplicationContext()->GetSystemIdentityManager()->AddObserver(this);
}

ChromeAccountManagerService::~ChromeAccountManagerService() {
  GetApplicationContext()->GetSystemIdentityManager()->RemoveObserver(this);
}

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
    std::string_view email) const {
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
    std::string_view gaia_id) const {
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
  for (auto& observer : observer_list_) {
    observer.OnChromeAccountManagerServiceShutdown(this);
  }
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

void ChromeAccountManagerService::OnIdentityListChanged() {
  for (auto& observer : observer_list_) {
    observer.OnIdentityListChanged();
  }
}

void ChromeAccountManagerService::OnIdentityUpdated(
    id<SystemIdentity> identity) {
  if (!this->IsValidIdentity(identity)) {
    return;
  }
  for (auto& observer : observer_list_) {
    observer.OnIdentityUpdated(identity);
  }
}

void ChromeAccountManagerService::OnIdentityAccessTokenRefreshFailed(
    id<SystemIdentity> identity,
    id<RefreshAccessTokenError> error) {
  if (!this->IsValidIdentity(identity)) {
    return;
  }
  for (auto& observer : observer_list_) {
    observer.OnAccessTokenRefreshFailed(identity, error);
  }
}

void ChromeAccountManagerService::UpdateRestriction() {
  restriction_ = PatternAccountRestrictionFromPreference(pref_service_);
  OnIdentityListChanged();
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
  }
  DCHECK(avatar_cache);
  if (!*avatar_cache) {
    *avatar_cache =
        [[ResizedAvatarCache alloc] initWithIdentityAvatarSize:avatar_size];
  }
  return *avatar_cache;
}
