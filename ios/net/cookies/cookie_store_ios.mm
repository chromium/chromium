// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/cookies/cookie_store_ios.h"

#import <Foundation/Foundation.h>
#include <stddef.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "ios/net/cookies/cookie_store_ios_client.h"
#import "ios/net/cookies/ns_http_system_cookie_store.h"
#import "ios/net/cookies/system_cookie_util.h"
#include "ios/net/ios_net_buildflags.h"
#import "net/base/mac/url_conversions.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "net/log/net_log.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace net {

using CookieDeletionInfo = CookieDeletionInfo;

namespace {

#pragma mark NotificationTrampoline

// NotificationTrampoline dispatches cookie notifications to all the existing
// CookieStoreIOS.
class NotificationTrampoline {
 public:
  static NotificationTrampoline* GetInstance();

  void AddObserver(CookieNotificationObserver* obs);
  void RemoveObserver(CookieNotificationObserver* obs);

  // Notify the observers.
  void NotifyCookiesChanged();

 private:
  NotificationTrampoline();
  ~NotificationTrampoline();

  base::ObserverList<CookieNotificationObserver>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(NotificationTrampoline);

  static NotificationTrampoline* g_notification_trampoline;
};

#pragma mark NotificationTrampoline implementation

NotificationTrampoline* NotificationTrampoline::GetInstance() {
  if (!g_notification_trampoline)
    g_notification_trampoline = new NotificationTrampoline;
  return g_notification_trampoline;
}

void NotificationTrampoline::AddObserver(CookieNotificationObserver* obs) {
  observer_list_.AddObserver(obs);
}

void NotificationTrampoline::RemoveObserver(CookieNotificationObserver* obs) {
  observer_list_.RemoveObserver(obs);
}

void NotificationTrampoline::NotifyCookiesChanged() {
  for (auto& observer : observer_list_)
    observer.OnSystemCookiesChanged();
}

NotificationTrampoline::NotificationTrampoline() {
}

NotificationTrampoline::~NotificationTrampoline() {
}

// Global instance of NotificationTrampoline.
NotificationTrampoline* NotificationTrampoline::g_notification_trampoline =
    nullptr;

#pragma mark Utility functions

// Returns an empty closure if |callback| is null callback or binds the
// callback to |status|.
base::OnceClosure BindSetCookiesCallback(
    CookieStoreIOS::SetCookiesCallback* callback,
    net::CanonicalCookie::CookieInclusionStatus status) {
  base::OnceClosure set_callback;
  if (!callback->is_null()) {
    set_callback = base::BindOnce(std::move(*callback), status);
  }
  return set_callback;
}

// Adds cookies in |cookies| with name |name| to |filtered|.
void OnlyCookiesWithName(const net::CookieStatusList& cookies,
                         const std::string& name,
                         net::CookieList* filtered) {
  for (const auto& cookie_with_status : cookies) {
    if (cookie_with_status.cookie.Name() == name)
      filtered->push_back(cookie_with_status.cookie);
  }
}

}  // namespace

#pragma mark -

#pragma mark CookieStoreIOS::Subscription

CookieStoreIOS::Subscription::Subscription(
    std::unique_ptr<CookieChangeCallbackList::Subscription> subscription)
    : subscription_(std::move(subscription)) {
  DCHECK(subscription_);
}

CookieStoreIOS::Subscription::~Subscription() {
  if (!subscription_) {
    // |CookieStoreIOS| already destroyed - bail out.
    return;
  }

  // |CookieStoreIOS| is alive - unsubscribe.
  RemoveFromList();
}

void CookieStoreIOS::Subscription::ResetSubscription() {
  subscription_.reset();
}

#pragma mark -

#pragma mark CookieStoreIOS::CookieChangeDispatcherIOS

CookieStoreIOS::CookieChangeDispatcherIOS::CookieChangeDispatcherIOS(
    CookieStoreIOS* cookie_store)
    : cookie_store_(cookie_store) {
  DCHECK(cookie_store);
}

CookieStoreIOS::CookieChangeDispatcherIOS::~CookieChangeDispatcherIOS() =
    default;

std::unique_ptr<CookieChangeSubscription>
CookieStoreIOS::CookieChangeDispatcherIOS::AddCallbackForCookie(
    const GURL& gurl,
    const std::string& name,
    CookieChangeCallback callback) {
  return cookie_store_->AddCallbackForCookie(gurl, name, std::move(callback));
}

std::unique_ptr<CookieChangeSubscription>
CookieStoreIOS::CookieChangeDispatcherIOS::AddCallbackForUrl(
    const GURL& gurl,
    CookieChangeCallback callback) {
  // Implement when needed by iOS consumers.
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<CookieChangeSubscription>
CookieStoreIOS::CookieChangeDispatcherIOS::AddCallbackForAllChanges(
    CookieChangeCallback callback) {
  // Implement when needed by iOS consumers.
  NOTIMPLEMENTED();
  return nullptr;
}

#pragma mark CookieStoreIOS

CookieStoreIOS::CookieStoreIOS(
    std::unique_ptr<SystemCookieStore> system_cookie_store,
    NetLog* net_log)
    : CookieStoreIOS(/*persistent_store=*/nullptr,
                     std::move(system_cookie_store),
                     net_log) {}

CookieStoreIOS::CookieStoreIOS(NSHTTPCookieStorage* ns_cookie_store,
                               NetLog* net_log)
    : CookieStoreIOS(std::make_unique<NSHTTPSystemCookieStore>(ns_cookie_store),
                     net_log) {}

CookieStoreIOS::~CookieStoreIOS() {
  NotificationTrampoline::GetInstance()->RemoveObserver(this);

  // Reset subscriptions.
  for (auto* node = all_subscriptions_.head(); node != all_subscriptions_.end();
       node = node->next()) {
    node->value()->ResetSubscription();
  }
}

// static
void CookieStoreIOS::NotifySystemCookiesChanged() {
  NotificationTrampoline::GetInstance()->NotifyCookiesChanged();
}

void CookieStoreIOS::SetMetricsEnabled() {
  static CookieStoreIOS* g_cookie_store_with_metrics = nullptr;
  DCHECK(!g_cookie_store_with_metrics || g_cookie_store_with_metrics == this)
      << "Only one cookie store may use metrics.";
  g_cookie_store_with_metrics = this;
  metrics_enabled_ = true;
}

#pragma mark -
#pragma mark CookieStore methods

void CookieStoreIOS::SetCanonicalCookieAsync(
    std::unique_ptr<net::CanonicalCookie> cookie,
    std::string source_scheme,
    const net::CookieOptions& options,
    SetCookiesCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If cookies are not allowed, a CookieStoreIOS subclass should be used
  // instead.
  DCHECK(SystemCookiesAllowed());

  DCHECK(cookie->IsCanonical());
  // The exclude_httponly() option would only be used by a javascript
  // engine.
  DCHECK(!options.exclude_httponly());

  bool secure_source =
      GURL::SchemeIsCryptographic(base::ToLowerASCII(source_scheme));

  if (cookie->IsSecure() && !secure_source) {
    if (!callback.is_null())
      std::move(callback).Run(net::CanonicalCookie::CookieInclusionStatus(
          net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_SECURE_ONLY));
    return;
  }

  NSHTTPCookie* ns_cookie = SystemCookieFromCanonicalCookie(*cookie.get());

  if (ns_cookie != nil) {
    system_store_->SetCookieAsync(
        ns_cookie, &cookie->CreationDate(),
        BindSetCookiesCallback(&callback,
                               net::CanonicalCookie::CookieInclusionStatus()));
    return;
  }

  if (!callback.is_null())
    std::move(callback).Run(net::CanonicalCookie::CookieInclusionStatus(
        net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_FAILURE_TO_STORE));
}

void CookieStoreIOS::GetCookieListWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    GetCookieListCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If cookies are not allowed, a CookieStoreIOS subclass should be used
  // instead.
  DCHECK(SystemCookiesAllowed());

  // TODO(mkwst): If/when iOS supports Same-Site cookies, we'll need to pass
  // options in here as well. https://crbug.com/459154
  system_store_->GetCookiesForURLAsync(
      url,
      base::BindOnce(&CookieStoreIOS::RunGetCookieListCallbackOnSystemCookies,
                     weak_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void CookieStoreIOS::GetAllCookiesAsync(GetAllCookiesCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If cookies are not allowed, a CookieStoreIOS subclass should be used
  // instead.
  DCHECK(SystemCookiesAllowed());

  // TODO(crbug.com/459154): If/when iOS supports Same-Site cookies, we'll need
  // to pass options in here as well.
  system_store_->GetAllCookiesAsync(
      base::BindOnce(&CookieStoreIOS::RunGetAllCookiesCallbackOnSystemCookies,
                     weak_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void CookieStoreIOS::DeleteCanonicalCookieAsync(const CanonicalCookie& cookie,
                                                DeleteCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If cookies are not allowed, a CookieStoreIOS subclass should be used
  // instead.
  DCHECK(SystemCookiesAllowed());

  DeleteCookiesMatchingPredicateAsync(base::BindRepeating(
                                          [](const net::CanonicalCookie& target,
                                             const net::CanonicalCookie& cc) {
                                            return cc.IsEquivalent(target) &&
                                                   cc.Value() == target.Value();
                                          },
                                          cookie),
                                      std::move(callback));
}

void CookieStoreIOS::DeleteAllCreatedInTimeRangeAsync(
    const CookieDeletionInfo::TimeRange& creation_range,
    DeleteCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If cookies are not allowed, a CookieStoreIOS subclass should be used
  // instead.
  DCHECK(SystemCookiesAllowed());

  if (metrics_enabled())
    ResetCookieCountMetrics();

  CookieDeletionInfo delete_info(creation_range.start(), creation_range.end());
  DeleteCookiesMatchingInfoAsync(std::move(delete_info), std::move(callback));
}

void CookieStoreIOS::DeleteAllMatchingInfoAsync(CookieDeletionInfo delete_info,
                                                DeleteCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If cookies are not allowed, a CookieStoreIOS subclass should be used
  // instead.
  DCHECK(SystemCookiesAllowed());

  if (metrics_enabled())
    ResetCookieCountMetrics();

  DeleteCookiesMatchingInfoAsync(std::move(delete_info), std::move(callback));
}

void CookieStoreIOS::DeleteSessionCookiesAsync(DeleteCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If cookies are not allowed, a CookieStoreIOS subclass should be used
  // instead.
  DCHECK(SystemCookiesAllowed());

  if (metrics_enabled())
    ResetCookieCountMetrics();

  CookieDeletionInfo delete_info;
  delete_info.session_control =
      CookieDeletionInfo::SessionControl::SESSION_COOKIES;
  DeleteCookiesMatchingInfoAsync(std::move(delete_info), std::move(callback));
}

void CookieStoreIOS::FlushStore(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (SystemCookiesAllowed()) {
    // If cookies are disabled, the system store is empty, and the cookies are
    // stashed on disk. Do not delete the cookies on the disk in this case.
    system_store_->GetAllCookiesAsync(
        base ::BindOnce(&CookieStoreIOS::FlushStoreFromCookies,
                        weak_factory_.GetWeakPtr(), std::move(closure)));
    return;
  }

  // This code path is used by a CookieStoreIOS subclass, which shares this
  // implementation.
  cookie_monster_->FlushStore(std::move(closure));
  flush_closure_.Cancel();
}

#pragma mark -
#pragma mark Protected methods

CookieStoreIOS::CookieStoreIOS(
    net::CookieMonster::PersistentCookieStore* persistent_store,
    std::unique_ptr<SystemCookieStore> system_store,
    NetLog* net_log)
    : cookie_monster_(new net::CookieMonster(persistent_store,
                                             net_log)),
      system_store_(std::move(system_store)),
      metrics_enabled_(false),
      cookie_cache_(new CookieCache()),
      change_dispatcher_(this),
      weak_factory_(this) {
  DCHECK(system_store_);

  NotificationTrampoline::GetInstance()->AddObserver(this);

  cookie_monster_->SetPersistSessionCookies(true);
  cookie_monster_->SetForceKeepSessionState();
}

void CookieStoreIOS::FlushStoreFromCookies(base::OnceClosure closure,
                                           NSArray<NSHTTPCookie*>* cookies) {
  WriteToCookieMonster(cookies);
  cookie_monster_->FlushStore(std::move(closure));
  flush_closure_.Cancel();
}

CookieStoreIOS::SetCookiesCallback CookieStoreIOS::WrapSetCallback(
    SetCookiesCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return base::BindOnce(&CookieStoreIOS::UpdateCachesAfterSet,
                        weak_factory_.GetWeakPtr(), std::move(callback));
}

CookieStoreIOS::DeleteCallback CookieStoreIOS::WrapDeleteCallback(
    DeleteCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return base::BindOnce(&CookieStoreIOS::UpdateCachesAfterDelete,
                        weak_factory_.GetWeakPtr(), std::move(callback));
}

base::OnceClosure CookieStoreIOS::WrapClosure(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return base::BindOnce(&CookieStoreIOS::UpdateCachesAfterClosure,
                        weak_factory_.GetWeakPtr(), std::move(callback));
}

#pragma mark -
#pragma mark Private methods

bool CookieStoreIOS::SystemCookiesAllowed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return system_store_->GetCookieAcceptPolicy() !=
         NSHTTPCookieAcceptPolicyNever;
}

void CookieStoreIOS::WriteToCookieMonster(NSArray* system_cookies) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Copy the cookies from the global cookie store to |cookie_monster_|.
  // Unlike the system store, CookieMonster requires unique creation times.
  net::CookieList cookie_list;
  NSUInteger cookie_count = [system_cookies count];
  cookie_list.reserve(cookie_count);
  for (NSHTTPCookie* cookie in system_cookies) {
    cookie_list.push_back(CanonicalCookieFromSystemCookie(
        cookie, system_store_->GetCookieCreationTime(cookie)));
  }
  cookie_monster_->SetAllCookiesAsync(cookie_list, SetCookiesCallback());

  // Update metrics.
  if (metrics_enabled_)
    UMA_HISTOGRAM_COUNTS_10000("CookieIOS.CookieWrittenCount", cookie_count);
}

void CookieStoreIOS::DeleteCookiesMatchingInfoAsync(
    net::CookieDeletionInfo delete_info,
    DeleteCallback callback) {
  DeleteCookiesMatchingPredicateAsync(
      base::BindRepeating(
          [](const CookieDeletionInfo& delete_info,
             const net::CanonicalCookie& cc) {
            return delete_info.Matches(cc);
          },
          std::move(delete_info)),
      std::move(callback));
}

void CookieStoreIOS::DeleteCookiesMatchingPredicateAsync(
    const base::RepeatingCallback<bool(const net::CanonicalCookie&)>& predicate,
    DeleteCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  __block DeleteCallback shared_callback = std::move(callback);
  __block base::RepeatingCallback<bool(const net::CanonicalCookie&)>
      shared_predicate = predicate;
  base::WeakPtr<SystemCookieStore> weak_system_store =
      system_store_->GetWeakPtr();
  system_store_->GetAllCookiesAsync(
      base::BindOnce(^(NSArray<NSHTTPCookie*>* cookies) {
        if (!weak_system_store) {
          if (!shared_callback.is_null())
            std::move(shared_callback).Run(0);
          return;
        }
        int to_delete_count = 0;
        for (NSHTTPCookie* cookie in cookies) {
          base::Time creation_time =
              weak_system_store->GetCookieCreationTime(cookie);
          CanonicalCookie cc =
              CanonicalCookieFromSystemCookie(cookie, creation_time);
          if (shared_predicate.Run(cc)) {
            weak_system_store->DeleteCookieAsync(
                cookie, SystemCookieStore::SystemCookieCallback());
            to_delete_count++;
          }
        }

        if (!shared_callback.is_null())
          std::move(shared_callback).Run(to_delete_count);
      }));
}

void CookieStoreIOS::OnSystemCookiesChanged() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  for (const auto& hook_map_entry : hook_map_) {
    std::pair<GURL, std::string> key = hook_map_entry.first;
    UpdateCacheForCookieFromSystem(key.first, key.second,
                                   /*run_callbacks=*/true);
  }

  // Do not schedule a flush if one is already scheduled.
  if (!flush_closure_.IsCancelled())
    return;

  flush_closure_.Reset(base::Bind(&CookieStoreIOS::FlushStore,
                                  weak_factory_.GetWeakPtr(), base::Closure()));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, flush_closure_.callback(), base::TimeDelta::FromSeconds(10));
}

CookieChangeDispatcher& CookieStoreIOS::GetChangeDispatcher() {
  return change_dispatcher_;
}

void CookieStoreIOS::SetCookieableSchemes(
    const std::vector<std::string>& schemes,
    SetCookieableSchemesCallback callback) {
  // Not supported on iOS.
  std::move(callback).Run(false);
}

std::unique_ptr<CookieChangeSubscription> CookieStoreIOS::AddCallbackForCookie(
    const GURL& gurl,
    const std::string& name,
    CookieChangeCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Prefill cookie cache with all pertinent cookies for |url| if needed.
  std::pair<GURL, std::string> key(gurl, name);
  if (hook_map_.count(key) == 0) {
    UpdateCacheForCookieFromSystem(gurl, name, /*run_callbacks=*/false);
    hook_map_[key] = std::make_unique<CookieChangeCallbackList>();
  }

  DCHECK(hook_map_.find(key) != hook_map_.end());
  auto subscription =
      std::make_unique<Subscription>(hook_map_[key]->Add(std::move(callback)));
  all_subscriptions_.Append(subscription.get());

  return subscription;
}

void CookieStoreIOS::UpdateCacheForCookieFromSystem(
    const GURL& gurl,
    const std::string& cookie_name,
    bool run_callbacks) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  system_store_->GetCookiesForURLAsync(
      gurl, base::BindOnce(&CookieStoreIOS::UpdateCacheForCookies,
                           weak_factory_.GetWeakPtr(), gurl, cookie_name,
                           run_callbacks));
}

void CookieStoreIOS::UpdateCacheForCookies(const GURL& gurl,
                                           const std::string& cookie_name,
                                           bool run_callbacks,
                                           NSArray<NSHTTPCookie*>* nscookies) {
  std::vector<net::CanonicalCookie> cookies;
  std::vector<net::CanonicalCookie> out_removed_cookies;
  std::vector<net::CanonicalCookie> out_added_cookies;
  for (NSHTTPCookie* nscookie in nscookies) {
    if (base::SysNSStringToUTF8(nscookie.name) == cookie_name) {
      net::CanonicalCookie canonical_cookie = CanonicalCookieFromSystemCookie(
          nscookie, system_store_->GetCookieCreationTime(nscookie));
      cookies.push_back(canonical_cookie);
    }
  }

  bool changes = cookie_cache_->Update(
      gurl, cookie_name, cookies, &out_removed_cookies, &out_added_cookies);
  if (run_callbacks && changes) {
    RunCallbacksForCookies(gurl, cookie_name, out_removed_cookies,
                           net::CookieChangeCause::UNKNOWN_DELETION);
    RunCallbacksForCookies(gurl, cookie_name, out_added_cookies,
                           net::CookieChangeCause::INSERTED);
  }
}

void CookieStoreIOS::RunCallbacksForCookies(
    const GURL& url,
    const std::string& name,
    const std::vector<net::CanonicalCookie>& cookies,
    net::CookieChangeCause cause) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (cookies.empty())
    return;

  std::pair<GURL, std::string> key(url, name);
  CookieChangeCallbackList* callbacks = hook_map_[key].get();
  for (const auto& cookie : cookies) {
    DCHECK_EQ(name, cookie.Name());
    // TODO(crbug.com/978172): Support CookieAccessSemantics values on iOS and
    // use it to check IncludeForRequestURL before notifying?
    callbacks->Notify(net::CookieChangeInfo(
        cookie, net::CookieAccessSemantics::UNKNOWN, cause));
  }
}

void CookieStoreIOS::GotCookieListFor(
    const std::pair<GURL, std::string> key,
    const net::CookieStatusList& cookies,
    const net::CookieStatusList& excluded_cookies) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  net::CookieList filtered;
  OnlyCookiesWithName(cookies, key.second, &filtered);
  std::vector<net::CanonicalCookie> removed_cookies;
  std::vector<net::CanonicalCookie> added_cookies;
  if (cookie_cache_->Update(key.first, key.second, filtered, &removed_cookies,
                            &added_cookies)) {
    RunCallbacksForCookies(key.first, key.second, removed_cookies,
                           net::CookieChangeCause::UNKNOWN_DELETION);
    RunCallbacksForCookies(key.first, key.second, added_cookies,
                           net::CookieChangeCause::INSERTED);
  }
}

void CookieStoreIOS::UpdateCachesFromCookieMonster() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (const auto& hook_map_entry : hook_map_) {
    std::pair<GURL, std::string> key = hook_map_entry.first;
    GetCookieListCallback callback = base::BindOnce(
        &CookieStoreIOS::GotCookieListFor, weak_factory_.GetWeakPtr(), key);
    cookie_monster_->GetCookieListWithOptionsAsync(
        key.first, net::CookieOptions::MakeAllInclusive(), std::move(callback));
  }
}

void CookieStoreIOS::UpdateCachesAfterSet(
    SetCookiesCallback callback,
    net::CanonicalCookie::CookieInclusionStatus status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (status.IsInclude())
    UpdateCachesFromCookieMonster();
  if (!callback.is_null())
    std::move(callback).Run(status);
}

void CookieStoreIOS::UpdateCachesAfterDelete(DeleteCallback callback,
                                             uint32_t num_deleted) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  UpdateCachesFromCookieMonster();
  if (!callback.is_null())
    std::move(callback).Run(num_deleted);
}

void CookieStoreIOS::UpdateCachesAfterClosure(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  UpdateCachesFromCookieMonster();
  if (!callback.is_null())
    std::move(callback).Run();
}

net::CookieList
CookieStoreIOS::CanonicalCookieListFromSystemCookies(NSArray* cookies) {
  net::CookieList cookie_list;
  cookie_list.reserve([cookies count]);
  for (NSHTTPCookie* cookie in cookies) {
    base::Time created = system_store_->GetCookieCreationTime(cookie);
    cookie_list.push_back(CanonicalCookieFromSystemCookie(cookie, created));
  }
  return cookie_list;
}

net::CookieStatusList
CookieStoreIOS::CanonicalCookieWithStatusListFromSystemCookies(
    NSArray* cookies) {
  net::CookieStatusList cookie_list;
  cookie_list.reserve([cookies count]);
  for (NSHTTPCookie* cookie in cookies) {
    base::Time created = system_store_->GetCookieCreationTime(cookie);
    cookie_list.push_back({CanonicalCookieFromSystemCookie(cookie, created),
                           net::CanonicalCookie::CookieInclusionStatus()});
  }
  return cookie_list;
}

void CookieStoreIOS::RunGetCookieListCallbackOnSystemCookies(
    CookieStoreIOS::GetCookieListCallback callback,
    NSArray<NSHTTPCookie*>* cookies) {
  if (!callback.is_null()) {
    net::CookieStatusList excluded_cookies;
    std::move(callback).Run(
        CanonicalCookieWithStatusListFromSystemCookies(cookies),
        excluded_cookies);
  }
}

void CookieStoreIOS::RunGetAllCookiesCallbackOnSystemCookies(
    CookieStoreIOS::GetAllCookiesCallback callback,
    NSArray<NSHTTPCookie*>* cookies) {
  if (!callback.is_null()) {
    std::move(callback).Run(CanonicalCookieListFromSystemCookies(cookies));
  }
}

}  // namespace net
