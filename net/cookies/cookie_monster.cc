// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Portions of this code based on Mozilla:
//   (netwerk/cookie/src/nsCookieService.cpp)
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Witte (dwitte@stanford.edu)
 *   Michiel van Leeuwen (mvl@exedo.nl)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "net/cookies/cookie_monster.h"

#include <functional>
#include <numeric>
#include <set>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/process_memory_dump.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_monster_change_dispatcher.h"
#include "net/cookies/cookie_monster_netlog_params.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "net/log/net_log.h"
#include "net/log/net_log_values.h"
#include "url/origin.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;
using TimeRange = net::CookieDeletionInfo::TimeRange;

// In steady state, most cookie requests can be satisfied by the in memory
// cookie monster store. If the cookie request cannot be satisfied by the in
// memory store, the relevant cookies must be fetched from the persistent
// store. The task is queued in CookieMonster::tasks_pending_ if it requires
// all cookies to be loaded from the backend, or tasks_pending_for_key_ if it
// only requires all cookies associated with an eTLD+1.
//
// On the browser critical paths (e.g. for loading initial web pages in a
// session restore) it may take too long to wait for the full load. If a cookie
// request is for a specific URL, DoCookieCallbackForURL is called, which
// triggers a priority load if the key is not loaded yet by calling
// PersistentCookieStore::LoadCookiesForKey. The request is queued in
// CookieMonster::tasks_pending_for_key_ and executed upon receiving
// notification of key load completion via CookieMonster::OnKeyLoaded(). If
// multiple requests for the same eTLD+1 are received before key load
// completion, only the first request calls
// PersistentCookieStore::LoadCookiesForKey, all subsequent requests are queued
// in CookieMonster::tasks_pending_for_key_ and executed upon receiving
// notification of key load completion triggered by the first request for the
// same eTLD+1.

static const int kMinutesInTenYears = 10 * 365 * 24 * 60;

namespace {

void MaybeRunDeleteCallback(base::WeakPtr<net::CookieMonster> cookie_monster,
                            base::OnceClosure callback) {
  if (cookie_monster && callback)
    std::move(callback).Run();
}

template <typename T>
void MaybeRunCookieCallback(base::OnceCallback<void(const T&)> callback,
                            const T& result) {
  if (callback)
    std::move(callback).Run(result);
}

template <typename T, typename U>
void MaybeRunCookieCallback(
    base::OnceCallback<void(const T&, const U&)> callback,
    const T& first,
    const U& second) {
  if (callback)
    std::move(callback).Run(first, second);
}

template <typename T>
void MaybeRunCookieCallback(base::OnceCallback<void(T)> callback,
                            const T& result) {
  if (callback)
    std::move(callback).Run(result);
}

}  // namespace

namespace net {

// See comments at declaration of these variables in cookie_monster.h
// for details.
const size_t CookieMonster::kDomainMaxCookies = 180;
const size_t CookieMonster::kDomainPurgeCookies = 30;
const size_t CookieMonster::kMaxCookies = 3300;
const size_t CookieMonster::kPurgeCookies = 300;

const size_t CookieMonster::kMaxDomainPurgedKeys = 100;

const size_t CookieMonster::kDomainCookiesQuotaLow = 30;
const size_t CookieMonster::kDomainCookiesQuotaMedium = 50;
const size_t CookieMonster::kDomainCookiesQuotaHigh =
    kDomainMaxCookies - kDomainPurgeCookies - kDomainCookiesQuotaLow -
    kDomainCookiesQuotaMedium;

const int CookieMonster::kSafeFromGlobalPurgeDays = 30;

namespace {

bool ContainsControlCharacter(const std::string& s) {
  return base::ranges::any_of(s, [](char c) { return c >= 0 && c <= 31; });
}

typedef std::vector<CanonicalCookie*> CanonicalCookieVector;

// Default minimum delay after updating a cookie's LastAccessDate before we
// will update it again.
const int kDefaultAccessUpdateThresholdSeconds = 60;

// Comparator to sort cookies from highest creation date to lowest
// creation date.
struct OrderByCreationTimeDesc {
  bool operator()(const CookieMonster::CookieMap::iterator& a,
                  const CookieMonster::CookieMap::iterator& b) const {
    return a->second->CreationDate() > b->second->CreationDate();
  }
};

// Mozilla sorts on the path length (longest first), and then it
// sorts by creation time (oldest first).
// The RFC says the sort order for the domain attribute is undefined.
bool CookieSorter(CanonicalCookie* cc1, CanonicalCookie* cc2) {
  if (cc1->Path().length() == cc2->Path().length())
    return cc1->CreationDate() < cc2->CreationDate();
  return cc1->Path().length() > cc2->Path().length();
}

bool LRACookieSorter(const CookieMonster::CookieMap::iterator& it1,
                     const CookieMonster::CookieMap::iterator& it2) {
  if (it1->second->LastAccessDate() != it2->second->LastAccessDate())
    return it1->second->LastAccessDate() < it2->second->LastAccessDate();

  // Ensure stability for == last access times by falling back to creation.
  return it1->second->CreationDate() < it2->second->CreationDate();
}

// For a CookieItVector iterator range [|it_begin|, |it_end|),
// sorts the first |num_sort| elements by LastAccessDate().
void SortLeastRecentlyAccessed(CookieMonster::CookieItVector::iterator it_begin,
                               CookieMonster::CookieItVector::iterator it_end,
                               size_t num_sort) {
  DCHECK_LE(static_cast<int>(num_sort), it_end - it_begin);
  std::partial_sort(it_begin, it_begin + num_sort, it_end, LRACookieSorter);
}

// Given a single cookie vector |cookie_its|, pushs all of the secure cookies in
// |cookie_its| into |secure_cookie_its| and all of the non-secure cookies into
// |non_secure_cookie_its|. Both |secure_cookie_its| and |non_secure_cookie_its|
// must be non-NULL.
void SplitCookieVectorIntoSecureAndNonSecure(
    const CookieMonster::CookieItVector& cookie_its,
    CookieMonster::CookieItVector* secure_cookie_its,
    CookieMonster::CookieItVector* non_secure_cookie_its) {
  DCHECK(secure_cookie_its && non_secure_cookie_its);
  for (const auto& curit : cookie_its) {
    if (curit->second->IsSecure())
      secure_cookie_its->push_back(curit);
    else
      non_secure_cookie_its->push_back(curit);
  }
}

bool LowerBoundAccessDateComparator(const CookieMonster::CookieMap::iterator it,
                                    const Time& access_date) {
  return it->second->LastAccessDate() < access_date;
}

// For a CookieItVector iterator range [|it_begin|, |it_end|)
// from a CookieItVector sorted by LastAccessDate(), returns the
// first iterator with access date >= |access_date|, or cookie_its_end if this
// holds for all.
CookieMonster::CookieItVector::iterator LowerBoundAccessDate(
    const CookieMonster::CookieItVector::iterator its_begin,
    const CookieMonster::CookieItVector::iterator its_end,
    const Time& access_date) {
  return std::lower_bound(its_begin, its_end, access_date,
                          LowerBoundAccessDateComparator);
}

// Mapping between DeletionCause and CookieChangeCause; the
// mapping also provides a boolean that specifies whether or not an
// OnCookieChange notification ought to be generated.
typedef struct ChangeCausePair_struct {
  CookieChangeCause cause;
  bool notify;
} ChangeCausePair;
const ChangeCausePair kChangeCauseMapping[] = {
    // DELETE_COOKIE_EXPLICIT
    {CookieChangeCause::EXPLICIT, true},
    // DELETE_COOKIE_OVERWRITE
    {CookieChangeCause::OVERWRITE, true},
    // DELETE_COOKIE_EXPIRED
    {CookieChangeCause::EXPIRED, true},
    // DELETE_COOKIE_EVICTED
    {CookieChangeCause::EVICTED, true},
    // DELETE_COOKIE_DUPLICATE_IN_BACKING_STORE
    {CookieChangeCause::EXPLICIT, false},
    // DELETE_COOKIE_DONT_RECORD
    {CookieChangeCause::EXPLICIT, false},
    // DELETE_COOKIE_EVICTED_DOMAIN
    {CookieChangeCause::EVICTED, true},
    // DELETE_COOKIE_EVICTED_GLOBAL
    {CookieChangeCause::EVICTED, true},
    // DELETE_COOKIE_EVICTED_DOMAIN_PRE_SAFE
    {CookieChangeCause::EVICTED, true},
    // DELETE_COOKIE_EVICTED_DOMAIN_POST_SAFE
    {CookieChangeCause::EVICTED, true},
    // DELETE_COOKIE_EXPIRED_OVERWRITE
    {CookieChangeCause::EXPIRED_OVERWRITE, true},
    // DELETE_COOKIE_CONTROL_CHAR
    {CookieChangeCause::EVICTED, true},
    // DELETE_COOKIE_NON_SECURE
    {CookieChangeCause::EVICTED, true},
    // DELETE_COOKIE_LAST_ENTRY
    {CookieChangeCause::EXPLICIT, false}};

bool IsCookieEligibleForEviction(CookiePriority current_priority_level,
                                 bool protect_secure_cookies,
                                 const CanonicalCookie* cookie) {
  if (cookie->Priority() == current_priority_level && protect_secure_cookies)
    return !cookie->IsSecure();

  return cookie->Priority() == current_priority_level;
}

size_t CountCookiesForPossibleDeletion(
    CookiePriority priority,
    const CookieMonster::CookieItVector* cookies,
    bool protect_secure_cookies) {
  size_t cookies_count = 0U;
  for (const auto& cookie : *cookies) {
    if (cookie->second->Priority() == priority) {
      if (!protect_secure_cookies || cookie->second->IsSecure())
        cookies_count++;
    }
  }
  return cookies_count;
}

}  // namespace

CookieMonster::CookieMonster(scoped_refptr<PersistentCookieStore> store,
                             NetLog* net_log)
    : CookieMonster(
          std::move(store),
          base::TimeDelta::FromSeconds(kDefaultAccessUpdateThresholdSeconds),
          net_log) {}

CookieMonster::CookieMonster(scoped_refptr<PersistentCookieStore> store,
                             base::TimeDelta last_access_threshold,
                             NetLog* net_log)
    : num_keys_(0u),
      change_dispatcher_(this),
      initialized_(false),
      started_fetching_all_cookies_(false),
      finished_fetching_all_cookies_(false),
      seen_global_task_(false),
      net_log_(NetLogWithSource::Make(net_log, NetLogSourceType::COOKIE_STORE)),
      store_(std::move(store)),
      last_access_threshold_(last_access_threshold),
      last_statistic_record_time_(base::Time::Now()),
      persist_session_cookies_(false) {
  InitializeHistograms();
  cookieable_schemes_.insert(
      cookieable_schemes_.begin(), kDefaultCookieableSchemes,
      kDefaultCookieableSchemes + kDefaultCookieableSchemesCount);
  net_log_.BeginEvent(NetLogEventType::COOKIE_STORE_ALIVE, [&] {
    return NetLogCookieMonsterConstructorParams(store != nullptr);
  });
}

// Asynchronous CookieMonster API

void CookieMonster::FlushStore(base::OnceClosure callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (initialized_ && store_.get()) {
    store_->Flush(std::move(callback));
  } else if (callback) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
  }
}

void CookieMonster::SetForceKeepSessionState() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (store_)
    store_->SetForceKeepSessionState();
}

void CookieMonster::SetAllCookiesAsync(const CookieList& list,
                                       SetCookiesCallback callback) {
  DoCookieCallback(base::BindOnce(
      // base::Unretained is safe as DoCookieCallback stores
      // the callback on |*this|, so the callback will not outlive
      // the object.
      &CookieMonster::SetAllCookies, base::Unretained(this), list,
      std::move(callback)));
}

void CookieMonster::SetCanonicalCookieAsync(
    std::unique_ptr<CanonicalCookie> cookie,
    const GURL& source_url,
    const CookieOptions& options,
    SetCookiesCallback callback) {
  DCHECK(cookie->IsCanonical());

  std::string domain = cookie->Domain();
  DoCookieCallbackForHostOrDomain(
      base::BindOnce(
          // base::Unretained is safe as DoCookieCallbackForHostOrDomain stores
          // the callback on |*this|, so the callback will not outlive
          // the object.
          &CookieMonster::SetCanonicalCookie, base::Unretained(this),
          std::move(cookie), source_url, options, std::move(callback)),
      domain);
}

void CookieMonster::GetCookieListWithOptionsAsync(
    const GURL& url,
    const CookieOptions& options,
    GetCookieListCallback callback) {
  DoCookieCallbackForURL(
      base::BindOnce(
          // base::Unretained is safe as DoCookieCallbackForURL stores
          // the callback on |*this|, so the callback will not outlive
          // the object.
          &CookieMonster::GetCookieListWithOptions, base::Unretained(this), url,
          options, std::move(callback)),
      url);
}

void CookieMonster::GetAllCookiesAsync(GetAllCookiesCallback callback) {
  DoCookieCallback(base::BindOnce(
      // base::Unretained is safe as DoCookieCallback stores
      // the callback on |*this|, so the callback will not outlive
      // the object.
      &CookieMonster::GetAllCookies, base::Unretained(this),
      std::move(callback)));
}

void CookieMonster::GetAllCookiesWithAccessSemanticsAsync(
    GetAllCookiesWithAccessSemanticsCallback callback) {
  DoCookieCallback(base::BindOnce(
      // base::Unretained is safe as DoCookieCallback stores
      // the callback on |*this|, so the callback will not outlive
      // the object.
      &CookieMonster::GetAllCookies, base::Unretained(this),
      base::BindOnce(&CookieMonster::AttachAccessSemanticsListForCookieList,
                     base::Unretained(this), std::move(callback))));
}

void CookieMonster::DeleteCanonicalCookieAsync(const CanonicalCookie& cookie,
                                               DeleteCallback callback) {
  DoCookieCallback(base::BindOnce(
      // base::Unretained is safe as DoCookieCallback stores
      // the callback on |*this|, so the callback will not outlive
      // the object.
      &CookieMonster::DeleteCanonicalCookie, base::Unretained(this), cookie,
      std::move(callback)));
}

void CookieMonster::DeleteAllCreatedInTimeRangeAsync(
    const TimeRange& creation_range,
    DeleteCallback callback) {
  DoCookieCallback(base::BindOnce(
      // base::Unretained is safe as DoCookieCallback stores
      // the callback on |*this|, so the callback will not outlive
      // the object.
      &CookieMonster::DeleteAllCreatedInTimeRange, base::Unretained(this),
      creation_range, std::move(callback)));
}

void CookieMonster::DeleteAllMatchingInfoAsync(CookieDeletionInfo delete_info,
                                               DeleteCallback callback) {
  DoCookieCallback(base::BindOnce(
      // base::Unretained is safe as DoCookieCallback stores
      // the callback on |*this|, so the callback will not outlive
      // the object.
      &CookieMonster::DeleteAllMatchingInfo, base::Unretained(this),
      std::move(delete_info), std::move(callback)));
}

void CookieMonster::DeleteSessionCookiesAsync(
    CookieStore::DeleteCallback callback) {
  DoCookieCallback(base::BindOnce(
      // base::Unretained is safe as DoCookieCallback stores
      // the callback on |*this|, so the callback will not outlive
      // the object.
      &CookieMonster::DeleteSessionCookies, base::Unretained(this),
      std::move(callback)));
}

void CookieMonster::SetCookieableSchemes(
    const std::vector<std::string>& schemes,
    SetCookieableSchemesCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Calls to this method will have no effect if made after a WebView or
  // CookieManager instance has been created.
  if (initialized_) {
    MaybeRunCookieCallback(std::move(callback), false);
    return;
  }

  cookieable_schemes_ = schemes;
  MaybeRunCookieCallback(std::move(callback), true);
}

// This function must be called before the CookieMonster is used.
void CookieMonster::SetPersistSessionCookies(bool persist_session_cookies) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!initialized_);
  net_log_.AddEntryWithBoolParams(
      NetLogEventType::COOKIE_STORE_SESSION_PERSISTENCE, NetLogEventPhase::NONE,
      "persistence", persist_session_cookies);
  persist_session_cookies_ = persist_session_cookies;
}

const char* const CookieMonster::kDefaultCookieableSchemes[] = {"http", "https",
                                                                "ws", "wss"};
const int CookieMonster::kDefaultCookieableSchemesCount =
    base::size(kDefaultCookieableSchemes);

CookieChangeDispatcher& CookieMonster::GetChangeDispatcher() {
  return change_dispatcher_;
}

void CookieMonster::DumpMemoryStats(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& parent_absolute_name) const {
  const char kRelPath[] = "/cookie_monster";

  pmd->CreateAllocatorDump(parent_absolute_name + kRelPath + "/cookies")
      ->AddScalar(base::trace_event::MemoryAllocatorDump::kNameObjectCount,
                  base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                  cookies_.size());

  pmd->CreateAllocatorDump(parent_absolute_name + kRelPath +
                           "/tasks_pending_global")
      ->AddScalar(base::trace_event::MemoryAllocatorDump::kNameObjectCount,
                  base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                  tasks_pending_.size());

  size_t total_pending_for_key = 0;
  for (const auto& kv : tasks_pending_for_key_)
    total_pending_for_key += kv.second.size();
  pmd->CreateAllocatorDump(parent_absolute_name + kRelPath +
                           "/tasks_pending_for_key")
      ->AddScalar(base::trace_event::MemoryAllocatorDump::kNameObjectCount,
                  base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                  total_pending_for_key);
}

CookieMonster::~CookieMonster() {
  DCHECK(thread_checker_.CalledOnValidThread());
  net_log_.EndEvent(NetLogEventType::COOKIE_STORE_ALIVE);
}

void CookieMonster::GetAllCookies(GetAllCookiesCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // This function is being called to scrape the cookie list for management UI
  // or similar.  We shouldn't show expired cookies in this list since it will
  // just be confusing to users, and this function is called rarely enough (and
  // is already slow enough) that it's OK to take the time to garbage collect
  // the expired cookies now.
  //
  // Note that this does not prune cookies to be below our limits (if we've
  // exceeded them) the way that calling GarbageCollect() would.
  GarbageCollectExpired(
      Time::Now(), CookieMapItPair(cookies_.begin(), cookies_.end()), nullptr);

  // Copy the CanonicalCookie pointers from the map so that we can use the same
  // sorter as elsewhere, then copy the result out.
  std::vector<CanonicalCookie*> cookie_ptrs;
  cookie_ptrs.reserve(cookies_.size());
  for (const auto& cookie : cookies_)
    cookie_ptrs.push_back(cookie.second.get());
  std::sort(cookie_ptrs.begin(), cookie_ptrs.end(), CookieSorter);

  CookieList cookie_list;
  cookie_list.reserve(cookie_ptrs.size());
  for (auto* cookie_ptr : cookie_ptrs)
    cookie_list.push_back(*cookie_ptr);

  MaybeRunCookieCallback(std::move(callback), cookie_list);
}

void CookieMonster::AttachAccessSemanticsListForCookieList(
    GetAllCookiesWithAccessSemanticsCallback callback,
    const CookieList& cookie_list) {
  std::vector<CookieAccessSemantics> access_semantics_list;
  for (const CanonicalCookie& cookie : cookie_list) {
    access_semantics_list.push_back(GetAccessSemanticsForCookie(cookie));
  }
  MaybeRunCookieCallback(std::move(callback), cookie_list,
                         access_semantics_list);
}

void CookieMonster::GetCookieListWithOptions(const GURL& url,
                                             const CookieOptions& options,
                                             GetCookieListCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  CookieAccessResultList included_cookies;
  CookieAccessResultList excluded_cookies;
  if (HasCookieableScheme(url)) {
    std::vector<CanonicalCookie*> cookie_ptrs =
        FindCookiesForRegistryControlledHost(url);
    std::sort(cookie_ptrs.begin(), cookie_ptrs.end(), CookieSorter);

    included_cookies.reserve(cookie_ptrs.size());
    std::vector<CanonicalCookie*> included_cookie_ptrs;
    FilterCookiesWithOptions(url, options, &cookie_ptrs, &included_cookies,
                             &excluded_cookies);
  }

  MaybeRunCookieCallback(std::move(callback), included_cookies,
                         excluded_cookies);
}

void CookieMonster::DeleteAllCreatedInTimeRange(const TimeRange& creation_range,
                                                DeleteCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  uint32_t num_deleted = 0;
  for (auto it = cookies_.begin(); it != cookies_.end();) {
    auto curit = it;
    CanonicalCookie* cc = curit->second.get();
    ++it;

    if (creation_range.Contains(cc->CreationDate())) {
      InternalDeleteCookie(curit, true, /*sync_to_store*/
                           DELETE_COOKIE_EXPLICIT);
      ++num_deleted;
    }
  }

  FlushStore(
      base::BindOnce(&MaybeRunDeleteCallback, weak_ptr_factory_.GetWeakPtr(),
                     callback ? base::BindOnce(std::move(callback), num_deleted)
                              : base::OnceClosure()));
}

void CookieMonster::DeleteAllMatchingInfo(CookieDeletionInfo delete_info,
                                          DeleteCallback callback) {
  uint32_t num_deleted = 0;
  for (auto it = cookies_.begin(); it != cookies_.end();) {
    auto curit = it;
    CanonicalCookie* cc = curit->second.get();
    ++it;

    bool delegate_treats_url_as_trustworthy = false;  // irrelevant if no URL.
    if (delete_info.url.has_value()) {
      delegate_treats_url_as_trustworthy =
          cookie_access_delegate() &&
          cookie_access_delegate()->ShouldTreatUrlAsTrustworthy(
              delete_info.url.value());
    }

    // Deletion uses all inclusive options, so it's ok to get the
    // `CookieSamePartyStatus` wrong here.
    if (delete_info.Matches(
            *cc, CookieAccessParams{
                     GetAccessSemanticsForCookie(*cc),
                     delegate_treats_url_as_trustworthy,
                     CookieSamePartyStatus::kNoSamePartyEnforcement})) {
      InternalDeleteCookie(curit, true, /*sync_to_store*/
                           DELETE_COOKIE_EXPLICIT);
      ++num_deleted;
    }
  }

  FlushStore(
      base::BindOnce(&MaybeRunDeleteCallback, weak_ptr_factory_.GetWeakPtr(),
                     callback ? base::BindOnce(std::move(callback), num_deleted)
                              : base::OnceClosure()));
}

void CookieMonster::DeleteCanonicalCookie(const CanonicalCookie& cookie,
                                          DeleteCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  uint32_t result = 0u;
  for (CookieMapItPair its = cookies_.equal_range(GetKey(cookie.Domain()));
       its.first != its.second; ++its.first) {
    const std::unique_ptr<CanonicalCookie>& candidate = its.first->second;
    // Historically, this has refused modification if the cookie has changed
    // value in between the CanonicalCookie object was returned by a getter
    // and when this ran.  The later parts of the conditional (everything but
    // the equivalence check) attempt to preserve this behavior.
    if (candidate->IsEquivalent(cookie) &&
        candidate->Value() == cookie.Value()) {
      InternalDeleteCookie(its.first, true, DELETE_COOKIE_EXPLICIT);
      result = 1u;
      break;
    }
  }
  FlushStore(
      base::BindOnce(&MaybeRunDeleteCallback, weak_ptr_factory_.GetWeakPtr(),
                     callback ? base::BindOnce(std::move(callback), result)
                              : base::OnceClosure()));
}

void CookieMonster::DeleteSessionCookies(DeleteCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  uint32_t num_deleted = 0;
  for (auto it = cookies_.begin(); it != cookies_.end();) {
    auto curit = it;
    CanonicalCookie* cc = curit->second.get();
    ++it;

    if (!cc->IsPersistent()) {
      InternalDeleteCookie(curit, true, /*sync_to_store*/
                           DELETE_COOKIE_EXPIRED);
      ++num_deleted;
    }
  }

  FlushStore(
      base::BindOnce(&MaybeRunDeleteCallback, weak_ptr_factory_.GetWeakPtr(),
                     callback ? base::BindOnce(std::move(callback), num_deleted)
                              : base::OnceClosure()));
}

void CookieMonster::MarkCookieStoreAsInitialized() {
  DCHECK(thread_checker_.CalledOnValidThread());
  initialized_ = true;
}

void CookieMonster::FetchAllCookiesIfNecessary() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (store_.get() && !started_fetching_all_cookies_) {
    started_fetching_all_cookies_ = true;
    FetchAllCookies();
  }
}

void CookieMonster::FetchAllCookies() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(store_.get()) << "Store must exist to initialize";
  DCHECK(!finished_fetching_all_cookies_)
      << "All cookies have already been fetched.";

  // We bind in the current time so that we can report the wall-clock time for
  // loading cookies.
  store_->Load(base::BindOnce(&CookieMonster::OnLoaded,
                              weak_ptr_factory_.GetWeakPtr(), TimeTicks::Now()),
               net_log_);
}

void CookieMonster::OnLoaded(
    TimeTicks beginning_time,
    std::vector<std::unique_ptr<CanonicalCookie>> cookies) {
  DCHECK(thread_checker_.CalledOnValidThread());
  StoreLoadedCookies(std::move(cookies));
  histogram_time_blocked_on_load_->AddTime(TimeTicks::Now() - beginning_time);

  // Invoke the task queue of cookie request.
  InvokeQueue();
}

void CookieMonster::OnKeyLoaded(
    const std::string& key,
    std::vector<std::unique_ptr<CanonicalCookie>> cookies) {
  DCHECK(thread_checker_.CalledOnValidThread());

  StoreLoadedCookies(std::move(cookies));

  auto tasks_pending_for_key = tasks_pending_for_key_.find(key);

  // TODO(mmenke): Can this be turned into a DCHECK?
  if (tasks_pending_for_key == tasks_pending_for_key_.end())
    return;

  // Run all tasks for the key. Note that running a task can result in multiple
  // tasks being added to the back of the deque.
  while (!tasks_pending_for_key->second.empty()) {
    base::OnceClosure task = std::move(tasks_pending_for_key->second.front());
    tasks_pending_for_key->second.pop_front();
    std::move(task).Run();
  }

  tasks_pending_for_key_.erase(tasks_pending_for_key);

  // This has to be done last, in case running a task queues a new task for the
  // key, to ensure tasks are run in the correct order.
  keys_loaded_.insert(key);
}

void CookieMonster::StoreLoadedCookies(
    std::vector<std::unique_ptr<CanonicalCookie>> cookies) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Even if a key is expired, insert it so it can be garbage collected,
  // removed, and sync'd.
  CookieItVector cookies_with_control_chars;

  bool dispatch_change = !base::FeatureList::IsEnabled(
      features::kNoCookieChangeNotificationOnLoad);

  for (auto& cookie : cookies) {
    CanonicalCookie* cookie_ptr = cookie.get();
    CookieAccessResult access_result;
    access_result.access_semantics = CookieAccessSemantics::UNKNOWN;
    auto inserted = InternalInsertCookie(
        GetKey(cookie_ptr->Domain()), std::move(cookie),
        false /* sync_to_store */, access_result, dispatch_change);
    const Time cookie_access_time(cookie_ptr->LastAccessDate());
    if (earliest_access_time_.is_null() ||
        cookie_access_time < earliest_access_time_) {
      earliest_access_time_ = cookie_access_time;
    }

    if (ContainsControlCharacter(cookie_ptr->Name()) ||
        ContainsControlCharacter(cookie_ptr->Value())) {
      cookies_with_control_chars.push_back(inserted);
    }
  }

  // Any cookies that contain control characters that we have loaded from the
  // persistent store should be deleted. See http://crbug.com/238041.
  for (auto it = cookies_with_control_chars.begin();
       it != cookies_with_control_chars.end();) {
    auto curit = it;
    ++it;

    InternalDeleteCookie(*curit, true, DELETE_COOKIE_CONTROL_CHAR);
  }

  // After importing cookies from the PersistentCookieStore, verify that
  // none of our other constraints are violated.
  // In particular, the backing store might have given us duplicate cookies.

  // This method could be called multiple times due to priority loading, thus
  // cookies loaded in previous runs will be validated again, but this is OK
  // since they are expected to be much fewer than total DB.
  EnsureCookiesMapIsValid();
}

void CookieMonster::InvokeQueue() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Move all per-key tasks into the global queue, if there are any.  This is
  // protection about a race where the store learns about all cookies loading
  // before it learned about the cookies for a key loading.

  // Needed to prevent any recursively queued tasks from going back into the
  // per-key queues.
  seen_global_task_ = true;
  for (auto& tasks_for_key : tasks_pending_for_key_) {
    tasks_pending_.insert(tasks_pending_.begin(),
                          std::make_move_iterator(tasks_for_key.second.begin()),
                          std::make_move_iterator(tasks_for_key.second.end()));
  }
  tasks_pending_for_key_.clear();

  while (!tasks_pending_.empty()) {
    base::OnceClosure request_task = std::move(tasks_pending_.front());
    tasks_pending_.pop_front();
    std::move(request_task).Run();
  }

  DCHECK(tasks_pending_for_key_.empty());

  finished_fetching_all_cookies_ = true;
  keys_loaded_.clear();
}

void CookieMonster::EnsureCookiesMapIsValid() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Iterate through all the of the cookies, grouped by host.
  auto prev_range_end = cookies_.begin();
  while (prev_range_end != cookies_.end()) {
    auto cur_range_begin = prev_range_end;
    const std::string key = cur_range_begin->first;  // Keep a copy.
    auto cur_range_end = cookies_.upper_bound(key);
    prev_range_end = cur_range_end;

    // Ensure no equivalent cookies for this host.
    TrimDuplicateCookiesForKey(key, cur_range_begin, cur_range_end);
  }
}

// Our strategy to find duplicates is:
// (1) Build a map from cookie unique key to
//     {list of cookies with this signature, sorted by creation time}.
// (2) For each list with more than 1 entry, keep the cookie having the
//     most recent creation time, and delete the others.
//
void CookieMonster::TrimDuplicateCookiesForKey(const std::string& key,
                                               CookieMap::iterator begin,
                                               CookieMap::iterator end) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Set of cookies ordered by creation time.
  typedef std::multiset<CookieMap::iterator, OrderByCreationTimeDesc> CookieSet;

  // Helper map we populate to find the duplicates.
  typedef std::map<CanonicalCookie::UniqueCookieKey, CookieSet> EquivalenceMap;
  EquivalenceMap equivalent_cookies;

  // The number of duplicate cookies that have been found.
  int num_duplicates = 0;

  // Iterate through all of the cookies in our range, and insert them into
  // the equivalence map.
  for (auto it = begin; it != end; ++it) {
    DCHECK_EQ(key, it->first);
    CanonicalCookie* cookie = it->second.get();

    CanonicalCookie::UniqueCookieKey signature(cookie->UniqueKey());
    CookieSet& set = equivalent_cookies[signature];

    // We found a duplicate!
    if (!set.empty())
      num_duplicates++;

    // We save the iterator into |cookies_| rather than the actual cookie
    // pointer, since we may need to delete it later.
    set.insert(it);
  }

  // If there were no duplicates, we are done!
  if (num_duplicates == 0)
    return;

  // Make sure we find everything below that we did above.
  int num_duplicates_found = 0;

  // Otherwise, delete all the duplicate cookies, both from our in-memory store
  // and from the backing store.
  for (std::pair<const CanonicalCookie::UniqueCookieKey, CookieSet>&
           equivalent_cookie : equivalent_cookies) {
    const CanonicalCookie::UniqueCookieKey& signature = equivalent_cookie.first;
    CookieSet& dupes = equivalent_cookie.second;

    if (dupes.size() <= 1)
      continue;  // This cookiename/path has no duplicates.
    num_duplicates_found += dupes.size() - 1;

    // Since |dupes| is sorted by creation time (descending), the first cookie
    // is the most recent one (or tied for it), so we will keep it. The rest are
    // duplicates.
    dupes.erase(dupes.begin());

    LOG(ERROR) << base::StringPrintf(
        "Found %d duplicate cookies for key='%s', "
        "with {name='%s', domain='%s', path='%s'}",
        static_cast<int>(dupes.size()), key.c_str(),
        std::get<0>(signature).c_str(), std::get<1>(signature).c_str(),
        std::get<2>(signature).c_str());

    // Remove all the cookies identified by |dupes|. It is valid to delete our
    // list of iterators one at a time, since |cookies_| is a multimap (they
    // don't invalidate existing iterators following deletion).
    for (const CookieMap::iterator& dupe : dupes) {
      InternalDeleteCookie(dupe, true,
                           DELETE_COOKIE_DUPLICATE_IN_BACKING_STORE);
    }
  }
  DCHECK_EQ(num_duplicates, num_duplicates_found);
}

std::vector<CanonicalCookie*>
CookieMonster::FindCookiesForRegistryControlledHost(const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());

  Time current_time = Time::Now();

  // Retrieve all cookies for a given key
  const std::string key(GetKey(url.host_piece()));

  std::vector<CanonicalCookie*> cookies;
  for (CookieMapItPair its = cookies_.equal_range(key);
       its.first != its.second;) {
    auto curit = its.first;
    CanonicalCookie* cc = curit->second.get();
    ++its.first;

    // If the cookie is expired, delete it.
    if (cc->IsExpired(current_time)) {
      InternalDeleteCookie(curit, true, DELETE_COOKIE_EXPIRED);
      continue;
    }
    cookies.push_back(cc);
  }
  return cookies;
}

void CookieMonster::FilterCookiesWithOptions(
    const GURL url,
    const CookieOptions options,
    std::vector<CanonicalCookie*>* cookie_ptrs,
    CookieAccessResultList* included_cookies,
    CookieAccessResultList* excluded_cookies) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Probe to save statistics relatively frequently.  We do it here rather
  // than in the set path as many websites won't set cookies, and we
  // want to collect statistics whenever the browser's being used.
  Time current_time = Time::Now();
  RecordPeriodicStats(current_time);

  bool delegate_treats_url_as_trustworthy =
      cookie_access_delegate() &&
      cookie_access_delegate()->ShouldTreatUrlAsTrustworthy(url);

  for (CanonicalCookie* cookie_ptr : *cookie_ptrs) {
    // Filter out cookies that should not be included for a request to the
    // given |url|. HTTP only cookies are filtered depending on the passed
    // cookie |options|.
    CookieAccessResult access_result = cookie_ptr->IncludeForRequestURL(
        url, options,
        CookieAccessParams{
            GetAccessSemanticsForCookie(*cookie_ptr),
            delegate_treats_url_as_trustworthy,
            cookie_util::GetSamePartyStatus(*cookie_ptr, options)});

    if (!access_result.status.IsInclude()) {
      UMA_HISTOGRAM_BOOLEAN(
          "Cookie.SameParty.ReadExclusionDecidedBySameParty",
          access_result.status.HasOnlyExclusionReason(
              CookieInclusionStatus::EXCLUDE_SAMEPARTY_CROSS_PARTY_CONTEXT));

      if (options.return_excluded_cookies())
        excluded_cookies->push_back({*cookie_ptr, access_result});
      continue;
    }

    if (options.update_access_time())
      InternalUpdateCookieAccessTime(cookie_ptr, current_time);

    int destination_port = url.EffectiveIntPort();

    if (IsLocalhost(url)) {
      UMA_HISTOGRAM_ENUMERATION(
          "Cookie.Port.Read.Localhost",
          ReducePortRangeForCookieHistogram(destination_port));
      UMA_HISTOGRAM_ENUMERATION(
          "Cookie.Port.ReadDiffersFromSet.Localhost",
          IsCookieSentToSamePortThatSetIt(url, cookie_ptr->SourcePort(),
                                          cookie_ptr->SourceScheme()));
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "Cookie.Port.Read.RemoteHost",
          ReducePortRangeForCookieHistogram(destination_port));
      UMA_HISTOGRAM_ENUMERATION(
          "Cookie.Port.ReadDiffersFromSet.RemoteHost",
          IsCookieSentToSamePortThatSetIt(url, cookie_ptr->SourcePort(),
                                          cookie_ptr->SourceScheme()));
    }

    if (cookie_ptr->IsDomainCookie()) {
      UMA_HISTOGRAM_ENUMERATION(
          "Cookie.Port.ReadDiffersFromSet.DomainSet",
          IsCookieSentToSamePortThatSetIt(url, cookie_ptr->SourcePort(),
                                          cookie_ptr->SourceScheme()));
    }

    if (cookie_ptr->IsSameParty()) {
      UMA_HISTOGRAM_BOOLEAN("Cookie.SamePartyReadIncluded.IsHTTP",
                            !options.exclude_httponly());
      UMA_HISTOGRAM_EXACT_LINEAR(
          "Cookie.SamePartyReadIncluded.PartyContextSize",
          options.full_party_context_size(),
          1 + IsolationInfo::kPartyContextMaxSize);
    }

    included_cookies->push_back({*cookie_ptr, access_result});
  }
}

void CookieMonster::MaybeDeleteEquivalentCookieAndUpdateStatus(
    const std::string& key,
    const CanonicalCookie& cookie_being_set,
    bool allowed_to_set_secure_cookie,
    bool skip_httponly,
    bool already_expired,
    base::Time* creation_date_to_inherit,
    CookieInclusionStatus* status) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!status->HasExclusionReason(
      CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE));
  DCHECK(!status->HasExclusionReason(
      CookieInclusionStatus::EXCLUDE_OVERWRITE_HTTP_ONLY));

  bool found_equivalent_cookie = false;
  CookieMap::iterator deletion_candidate_it = cookies_.end();
  CanonicalCookie* skipped_secure_cookie = nullptr;

  // Check every cookie matching this domain key for equivalence.
  CookieMapItPair range_its = cookies_.equal_range(key);
  for (auto cur_it = range_its.first; cur_it != range_its.second; ++cur_it) {
    CanonicalCookie* cur_existing_cookie = cur_it->second.get();

    // Evaluate "Leave Secure Cookies Alone":
    // If the cookie is being set from an insecure source, then if an
    // "equivalent" Secure cookie already exists, then the cookie should *not*
    // be updated.
    //
    // "Equivalent" means they are the same by
    // IsEquivalentForSecureCookieMatching(). See the comment there for
    // details. (Note this is not a symmetric comparison.) This notion of
    // equivalence is slightly more inclusive than the usual IsEquivalent() one.
    //
    // See: https://tools.ietf.org/html/draft-ietf-httpbis-cookie-alone
    if (cur_existing_cookie->IsSecure() && !allowed_to_set_secure_cookie &&
        cookie_being_set.IsEquivalentForSecureCookieMatching(
            *cur_existing_cookie)) {
      // Hold onto this for additional Netlogging later if we end up preserving
      // a would-have-been-deleted cookie because of this.
      skipped_secure_cookie = cur_existing_cookie;
      net_log_.AddEvent(NetLogEventType::COOKIE_STORE_COOKIE_REJECTED_SECURE,
                        [&](NetLogCaptureMode capture_mode) {
                          return NetLogCookieMonsterCookieRejectedSecure(
                              skipped_secure_cookie, &cookie_being_set,
                              capture_mode);
                        });
      status->AddExclusionReason(
          CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE);
    }

    if (cookie_being_set.IsEquivalent(*cur_existing_cookie)) {
      // We should never have more than one equivalent cookie, since they should
      // overwrite each other.
      CHECK(!found_equivalent_cookie)
          << "Duplicate equivalent cookies found, cookie store is corrupted.";
      DCHECK(deletion_candidate_it == cookies_.end());
      found_equivalent_cookie = true;

      // The |cookie_being_set| is rejected for trying to overwrite an httponly
      // cookie when it should not be able to.
      if (skip_httponly && cur_existing_cookie->IsHttpOnly()) {
        net_log_.AddEvent(
            NetLogEventType::COOKIE_STORE_COOKIE_REJECTED_HTTPONLY,
            [&](NetLogCaptureMode capture_mode) {
              return NetLogCookieMonsterCookieRejectedHttponly(
                  cur_existing_cookie, &cookie_being_set, capture_mode);
            });
        status->AddExclusionReason(
            CookieInclusionStatus::EXCLUDE_OVERWRITE_HTTP_ONLY);
      } else {
        deletion_candidate_it = cur_it;
      }
    }
  }

  if (deletion_candidate_it != cookies_.end()) {
    CanonicalCookie* deletion_candidate = deletion_candidate_it->second.get();
    if (deletion_candidate->Value() == cookie_being_set.Value())
      *creation_date_to_inherit = deletion_candidate->CreationDate();
    if (status->IsInclude()) {
      InternalDeleteCookie(deletion_candidate_it, true /* sync_to_store */,
                           already_expired ? DELETE_COOKIE_EXPIRED_OVERWRITE
                                           : DELETE_COOKIE_OVERWRITE);
    } else if (status->HasExclusionReason(
                   CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE)) {
      // Log that we preserved a cookie that would have been deleted due to
      // Leave Secure Cookies Alone. This arbitrarily only logs the last
      // |skipped_secure_cookie| that we were left with after the for loop, even
      // if there were multiple matching Secure cookies that were left alone.
      DCHECK(skipped_secure_cookie);
      net_log_.AddEvent(
          NetLogEventType::COOKIE_STORE_COOKIE_PRESERVED_SKIPPED_SECURE,
          [&](NetLogCaptureMode capture_mode) {
            return NetLogCookieMonsterCookiePreservedSkippedSecure(
                skipped_secure_cookie, deletion_candidate, &cookie_being_set,
                capture_mode);
          });
    }
  }
}

CookieMonster::CookieMap::iterator CookieMonster::InternalInsertCookie(
    const std::string& key,
    std::unique_ptr<CanonicalCookie> cc,
    bool sync_to_store,
    const CookieAccessResult& access_result,
    bool dispatch_change) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CanonicalCookie* cc_ptr = cc.get();

  net_log_.AddEvent(NetLogEventType::COOKIE_STORE_COOKIE_ADDED,
                    [&](NetLogCaptureMode capture_mode) {
                      return NetLogCookieMonsterCookieAdded(
                          cc.get(), sync_to_store, capture_mode);
                    });
  if ((cc_ptr->IsPersistent() || persist_session_cookies_) && store_.get() &&
      sync_to_store) {
    store_->AddCookie(*cc_ptr);
  }
  auto inserted = cookies_.insert(CookieMap::value_type(key, std::move(cc)));

  // See InitializeHistograms() for details.
  int32_t type_sample =
      !cc_ptr->IsEffectivelySameSiteNone(access_result.access_semantics)
          ? 1 << COOKIE_TYPE_SAME_SITE
          : 0;
  type_sample |= cc_ptr->IsHttpOnly() ? 1 << COOKIE_TYPE_HTTPONLY : 0;
  type_sample |= cc_ptr->IsSecure() ? 1 << COOKIE_TYPE_SECURE : 0;
  histogram_cookie_type_->Add(type_sample);

  DCHECK(access_result.status.IsInclude());
  if (dispatch_change) {
    change_dispatcher_.DispatchChange(
        CookieChangeInfo(*cc_ptr, access_result, CookieChangeCause::INSERTED),
        true);
  }

  // If this is the first cookie in |cookies_| with this key, increment the
  // |num_keys_| counter.
  bool different_prev =
      inserted == cookies_.begin() || std::prev(inserted)->first != key;
  // According to std::multiqueue documentation:
  // "If the container has elements with equivalent key, inserts at the upper
  // bound of that range. (since C++11)"
  // This means that "inserted" iterator either points to the last element in
  // the map, or the element succeeding it has to have different key.
  DCHECK(std::next(inserted) == cookies_.end() ||
         std::next(inserted)->first != key);
  if (different_prev)
    ++num_keys_;

  return inserted;
}

void CookieMonster::SetCanonicalCookie(std::unique_ptr<CanonicalCookie> cc,
                                       const GURL& source_url,
                                       const CookieOptions& options,
                                       SetCookiesCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool delegate_treats_url_as_trustworthy =
      cookie_access_delegate() &&
      cookie_access_delegate()->ShouldTreatUrlAsTrustworthy(source_url);

  CookieAccessResult access_result = cc->IsSetPermittedInContext(
      source_url, options,
      CookieAccessParams(GetAccessSemanticsForCookie(*cc),
                         delegate_treats_url_as_trustworthy,
                         cookie_util::GetSamePartyStatus(*cc, options)),
      cookieable_schemes_);

  const std::string key(GetKey(cc->Domain()));

  base::Time creation_date = cc->CreationDate();
  if (creation_date.is_null()) {
    creation_date = Time::Now();
    cc->SetCreationDate(creation_date);
  }
  bool already_expired = cc->IsExpired(creation_date);

  base::Time creation_date_to_inherit;

  // Iterates through existing cookies for the same eTLD+1, and potentially
  // deletes an existing cookie, so any ExclusionReasons in |status| that would
  // prevent such deletion should be finalized beforehand.
  MaybeDeleteEquivalentCookieAndUpdateStatus(
      key, *cc, access_result.is_allowed_to_access_secure_cookies,
      options.exclude_httponly(), already_expired, &creation_date_to_inherit,
      &access_result.status);

  if (access_result.status.HasExclusionReason(
          CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE) ||
      access_result.status.HasExclusionReason(
          CookieInclusionStatus::EXCLUDE_OVERWRITE_HTTP_ONLY)) {
    DVLOG(net::cookie_util::kVlogSetCookies)
        << "SetCookie() not clobbering httponly cookie or secure cookie for "
           "insecure scheme";
  }

  // Now that IsSetPermittedInContext() and
  // MaybeDeleteEquivalentCookieAndUpdateStatus() have had a chance to set
  // cookie warnings/exclusions, record the downgrade metric.
  if (access_result.status.ShouldRecordDowngradeMetrics()) {
    UMA_HISTOGRAM_ENUMERATION(
        "Cookie.SameSiteContextDowngradeResponse",
        access_result.status.GetBreakingDowngradeMetricsEnumValue(source_url));
  }

  if (access_result.status.IsInclude()) {
    DVLOG(net::cookie_util::kVlogSetCookies)
        << "SetCookie() key: " << key << " cc: " << cc->DebugString();

    if (cc->IsSameParty()) {
      UMA_HISTOGRAM_BOOLEAN("Cookie.SamePartySetIncluded.IsHTTP",
                            !options.exclude_httponly());
      UMA_HISTOGRAM_EXACT_LINEAR("Cookie.SamePartySetIncluded.PartyContextSize",
                                 options.full_party_context_size(),
                                 1 + IsolationInfo::kPartyContextMaxSize);
    }

    // Realize that we might be setting an expired cookie, and the only point
    // was to delete the cookie which we've already done.
    if (!already_expired) {
      // See InitializeHistograms() for details.
      if (cc->IsPersistent()) {
        if (cc->IsSecure()) {
          histogram_expiration_duration_minutes_secure_->Add(
              (cc->ExpiryDate() - creation_date).InMinutes());
        } else {
          histogram_expiration_duration_minutes_non_secure_->Add(
              (cc->ExpiryDate() - creation_date).InMinutes());
        }
      }

      // Histogram the type of scheme used on URLs that set cookies. This
      // intentionally includes cookies that are set or overwritten by
      // http:// URLs, but not cookies that are cleared by http:// URLs, to
      // understand if the former behavior can be deprecated for Secure
      // cookies.
      // TODO(crbug.com/993120): Consider removing this histogram. The decision
      // it was added to evaluate has been implemented and standardized.
      CookieSource cookie_source_sample =
          (source_url.SchemeIsCryptographic()
               ? (cc->IsSecure()
                      ? COOKIE_SOURCE_SECURE_COOKIE_CRYPTOGRAPHIC_SCHEME
                      : COOKIE_SOURCE_NONSECURE_COOKIE_CRYPTOGRAPHIC_SCHEME)
               : (cc->IsSecure()
                      ? COOKIE_SOURCE_SECURE_COOKIE_NONCRYPTOGRAPHIC_SCHEME
                      : COOKIE_SOURCE_NONSECURE_COOKIE_NONCRYPTOGRAPHIC_SCHEME));
      histogram_cookie_source_scheme_->Add(cookie_source_sample);

      UMA_HISTOGRAM_BOOLEAN("Cookie.DomainSet", cc->IsDomainCookie());

      if (!creation_date_to_inherit.is_null()) {
        cc->SetCreationDate(creation_date_to_inherit);
      }

      InternalInsertCookie(key, std::move(cc), true, access_result);
    } else {
      DVLOG(net::cookie_util::kVlogSetCookies)
          << "SetCookie() not storing already expired cookie.";
    }

    // We assume that hopefully setting a cookie will be less common than
    // querying a cookie.  Since setting a cookie can put us over our limits,
    // make sure that we garbage collect...  We can also make the assumption
    // that if a cookie was set, in the common case it will be used soon after,
    // and we will purge the expired cookies in GetCookies().
    GarbageCollect(creation_date, key);

    if (IsLocalhost(source_url)) {
      UMA_HISTOGRAM_ENUMERATION(
          "Cookie.Port.Set.Localhost",
          ReducePortRangeForCookieHistogram(source_url.EffectiveIntPort()));
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "Cookie.Port.Set.RemoteHost",
          ReducePortRangeForCookieHistogram(source_url.EffectiveIntPort()));
    }
  } else {
    UMA_HISTOGRAM_BOOLEAN(
        "Cookie.SameParty.SetExclusionDecidedBySameParty",
        access_result.status.HasOnlyExclusionReason(
            CookieInclusionStatus::EXCLUDE_SAMEPARTY_CROSS_PARTY_CONTEXT));
  }

  // TODO(chlily): Log metrics.
  MaybeRunCookieCallback(std::move(callback), access_result);
}

void CookieMonster::SetAllCookies(CookieList list,
                                  SetCookiesCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Nuke the existing store.
  while (!cookies_.empty()) {
    // TODO(rdsmith): The CANONICAL is a lie.
    InternalDeleteCookie(cookies_.begin(), true, DELETE_COOKIE_EXPLICIT);
  }

  // Set all passed in cookies.
  for (const auto& cookie : list) {
    const std::string key(GetKey(cookie.Domain()));
    Time creation_time = cookie.CreationDate();
    if (cookie.IsExpired(creation_time))
      continue;

    if (cookie.IsPersistent()) {
      if (cookie.IsSecure()) {
        histogram_expiration_duration_minutes_secure_->Add(
            (cookie.ExpiryDate() - creation_time).InMinutes());
      } else {
        histogram_expiration_duration_minutes_non_secure_->Add(
            (cookie.ExpiryDate() - creation_time).InMinutes());
      }
    }

    CookieAccessResult access_result;
    access_result.access_semantics = GetAccessSemanticsForCookie(cookie);
    InternalInsertCookie(key, std::make_unique<CanonicalCookie>(cookie), true,
                         access_result);
    GarbageCollect(creation_time, key);
  }

  // TODO(rdsmith): If this function always returns the same value, it
  // shouldn't have a return value.  But it should also be deleted (see
  // https://codereview.chromium.org/2882063002/#msg64), which would
  // solve the return value problem.
  MaybeRunCookieCallback(std::move(callback), CookieAccessResult());
}

void CookieMonster::InternalUpdateCookieAccessTime(CanonicalCookie* cc,
                                                   const Time& current) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Based off the Mozilla code.  When a cookie has been accessed recently,
  // don't bother updating its access time again.  This reduces the number of
  // updates we do during pageload, which in turn reduces the chance our storage
  // backend will hit its batch thresholds and be forced to update.
  if ((current - cc->LastAccessDate()) < last_access_threshold_)
    return;

  cc->SetLastAccessDate(current);
  if ((cc->IsPersistent() || persist_session_cookies_) && store_.get())
    store_->UpdateCookieAccessTime(*cc);
}

// InternalDeleteCookies must not invalidate iterators other than the one being
// deleted.
void CookieMonster::InternalDeleteCookie(CookieMap::iterator it,
                                         bool sync_to_store,
                                         DeletionCause deletion_cause) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Ideally, this would be asserted up where we define kChangeCauseMapping,
  // but DeletionCause's visibility (or lack thereof) forces us to make
  // this check here.
  static_assert(base::size(kChangeCauseMapping) == DELETE_COOKIE_LAST_ENTRY + 1,
                "kChangeCauseMapping size should match DeletionCause size");

  CanonicalCookie* cc = it->second.get();
  DVLOG(net::cookie_util::kVlogSetCookies)
      << "InternalDeleteCookie()"
      << ", cause:" << deletion_cause << ", cc: " << cc->DebugString();

  ChangeCausePair mapping = kChangeCauseMapping[deletion_cause];
  if (deletion_cause != DELETE_COOKIE_DONT_RECORD) {
    net_log_.AddEvent(NetLogEventType::COOKIE_STORE_COOKIE_DELETED,
                      [&](NetLogCaptureMode capture_mode) {
                        return NetLogCookieMonsterCookieDeleted(
                            cc, mapping.cause, sync_to_store, capture_mode);
                      });
  }

  if ((cc->IsPersistent() || persist_session_cookies_) && store_.get() &&
      sync_to_store) {
    store_->DeleteCookie(*cc);
  }
  change_dispatcher_.DispatchChange(
      CookieChangeInfo(
          *cc,
          CookieAccessResult(CookieEffectiveSameSite::UNDEFINED,
                             CookieInclusionStatus(),
                             GetAccessSemanticsForCookie(*cc),
                             true /* is_allowed_to_access_secure_cookies */),
          mapping.cause),
      mapping.notify);

  // If this is the last cookie in |cookies_| with this key, decrement the
  // |num_keys_| counter.
  bool different_prev =
      it == cookies_.begin() || std::prev(it)->first != it->first;
  bool different_next =
      std::next(it) == cookies_.end() || std::next(it)->first != it->first;
  if (different_prev && different_next)
    --num_keys_;

  cookies_.erase(it);
}

// Domain expiry behavior is unchanged by key/expiry scheme (the
// meaning of the key is different, but that's not visible to this routine).
size_t CookieMonster::GarbageCollect(const Time& current,
                                     const std::string& key) {
  DCHECK(thread_checker_.CalledOnValidThread());

  size_t num_deleted = 0;
  Time safe_date(Time::Now() - TimeDelta::FromDays(kSafeFromGlobalPurgeDays));

  // Collect garbage for this key, minding cookie priorities.
  if (cookies_.count(key) > kDomainMaxCookies) {
    DVLOG(net::cookie_util::kVlogGarbageCollection)
        << "GarbageCollect() key: " << key;

    CookieItVector* cookie_its;

    CookieItVector non_expired_cookie_its;
    cookie_its = &non_expired_cookie_its;
    num_deleted +=
        GarbageCollectExpired(current, cookies_.equal_range(key), cookie_its);

    if (cookie_its->size() > kDomainMaxCookies) {
      DVLOG(net::cookie_util::kVlogGarbageCollection)
          << "Deep Garbage Collect domain.";

      if (domain_purged_keys_.size() < kMaxDomainPurgedKeys)
        domain_purged_keys_.insert(key);

      size_t purge_goal =
          cookie_its->size() - (kDomainMaxCookies - kDomainPurgeCookies);
      DCHECK(purge_goal > kDomainPurgeCookies);

      // Sort the cookies by access date, from least-recent to most-recent.
      std::sort(cookie_its->begin(), cookie_its->end(), LRACookieSorter);

      // Remove all but the kDomainCookiesQuotaLow most-recently accessed
      // cookies with low-priority. Then, if cookies still need to be removed,
      // bump the quota and remove low- and medium-priority. Then, if cookies
      // _still_ need to be removed, bump the quota and remove cookies with
      // any priority.
      //
      // 1.  Low-priority non-secure cookies.
      // 2.  Low-priority secure cookies.
      // 3.  Medium-priority non-secure cookies.
      // 4.  High-priority non-secure cookies.
      // 5.  Medium-priority secure cookies.
      // 6.  High-priority secure cookies.
      const static struct {
        CookiePriority priority;
        bool protect_secure_cookies;
      } purge_rounds[] = {
          // 1.  Low-priority non-secure cookies.
          {COOKIE_PRIORITY_LOW, true},
          // 2.  Low-priority secure cookies.
          {COOKIE_PRIORITY_LOW, false},
          // 3.  Medium-priority non-secure cookies.
          {COOKIE_PRIORITY_MEDIUM, true},
          // 4.  High-priority non-secure cookies.
          {COOKIE_PRIORITY_HIGH, true},
          // 5.  Medium-priority secure cookies.
          {COOKIE_PRIORITY_MEDIUM, false},
          // 6.  High-priority secure cookies.
          {COOKIE_PRIORITY_HIGH, false},
      };

      size_t quota = 0;
      for (const auto& purge_round : purge_rounds) {
        // Adjust quota according to the priority of cookies. Each round should
        // protect certain number of cookies in order to avoid starvation.
        // For example, when each round starts to remove cookies, the number of
        // cookies of that priority are counted and a decision whether they
        // should be deleted or not is made. If yes, some number of cookies of
        // that priority are deleted considering the quota.
        switch (purge_round.priority) {
          case COOKIE_PRIORITY_LOW:
            quota = kDomainCookiesQuotaLow;
            break;
          case COOKIE_PRIORITY_MEDIUM:
            quota = kDomainCookiesQuotaMedium;
            break;
          case COOKIE_PRIORITY_HIGH:
            quota = kDomainCookiesQuotaHigh;
            break;
        }
        size_t just_deleted = 0u;
        // Purge up to |purge_goal| for all cookies at the given priority.  This
        // path will be taken only if the initial non-secure purge did not evict
        // enough cookies.
        if (purge_goal > 0) {
          just_deleted = PurgeLeastRecentMatches(
              cookie_its, purge_round.priority, quota, purge_goal,
              purge_round.protect_secure_cookies);
          DCHECK_LE(just_deleted, purge_goal);
          purge_goal -= just_deleted;
          num_deleted += just_deleted;
        }
      }

      DCHECK_EQ(0u, purge_goal);
    }
  }

  // Collect garbage for everything. With firefox style we want to preserve
  // cookies accessed in kSafeFromGlobalPurgeDays, otherwise evict.
  if (cookies_.size() > kMaxCookies && earliest_access_time_ < safe_date) {
    DVLOG(net::cookie_util::kVlogGarbageCollection)
        << "GarbageCollect() everything";
    CookieItVector cookie_its;

    num_deleted += GarbageCollectExpired(
        current, CookieMapItPair(cookies_.begin(), cookies_.end()),
        &cookie_its);

    if (cookie_its.size() > kMaxCookies) {
      DVLOG(net::cookie_util::kVlogGarbageCollection)
          << "Deep Garbage Collect everything.";
      size_t purge_goal = cookie_its.size() - (kMaxCookies - kPurgeCookies);
      DCHECK(purge_goal > kPurgeCookies);

      CookieItVector secure_cookie_its;
      CookieItVector non_secure_cookie_its;
      SplitCookieVectorIntoSecureAndNonSecure(cookie_its, &secure_cookie_its,
                                              &non_secure_cookie_its);
      size_t non_secure_purge_goal =
          std::min<size_t>(purge_goal, non_secure_cookie_its.size());

      base::Time earliest_non_secure_access_time;
      size_t just_deleted = GarbageCollectLeastRecentlyAccessed(
          current, safe_date, non_secure_purge_goal, non_secure_cookie_its,
          &earliest_non_secure_access_time);
      num_deleted += just_deleted;

      if (secure_cookie_its.size() == 0) {
        // This case is unlikely, but should still update
        // |earliest_access_time_| if only have non-secure cookies.
        earliest_access_time_ = earliest_non_secure_access_time;
        // Garbage collection can't delete all cookies.
        DCHECK(!earliest_access_time_.is_null());
      } else if (just_deleted < purge_goal) {
        size_t secure_purge_goal = std::min<size_t>(purge_goal - just_deleted,
                                                    secure_cookie_its.size());
        base::Time earliest_secure_access_time;
        num_deleted += GarbageCollectLeastRecentlyAccessed(
            current, safe_date, secure_purge_goal, secure_cookie_its,
            &earliest_secure_access_time);

        if (!earliest_non_secure_access_time.is_null() &&
            earliest_non_secure_access_time < earliest_secure_access_time) {
          earliest_access_time_ = earliest_non_secure_access_time;
        } else {
          earliest_access_time_ = earliest_secure_access_time;
        }

        // Garbage collection can't delete all cookies.
        DCHECK(!earliest_access_time_.is_null());
      }

      // If there are secure cookies, but deleting non-secure cookies was enough
      // to meet the purge goal, secure cookies are never examined, so
      // |earliest_access_time_| can't be determined. Leaving it alone will mean
      // it's no later than the real earliest last access time, so this won't
      // lead to any problems.
    }
  }

  return num_deleted;
}

size_t CookieMonster::PurgeLeastRecentMatches(CookieItVector* cookies,
                                              CookiePriority priority,
                                              size_t to_protect,
                                              size_t purge_goal,
                                              bool protect_secure_cookies) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // 1. Count number of the cookies at |priority|
  size_t cookies_count_possibly_to_be_deleted = CountCookiesForPossibleDeletion(
      priority, cookies, false /* count all cookies */);

  // 2. If |cookies_count_possibly_to_be_deleted| at |priority| is less than or
  // equal |to_protect|, skip round in order to preserve the quota. This
  // involves secure and non-secure cookies at |priority|.
  if (cookies_count_possibly_to_be_deleted <= to_protect)
    return 0u;

  // 3. Calculate number of secure cookies at |priority|
  // and number of cookies at |priority| that can possibly be deleted.
  // It is guaranteed we do not delete more than |purge_goal| even if
  // |cookies_count_possibly_to_be_deleted| is higher.
  size_t secure_cookies = 0u;
  if (protect_secure_cookies) {
    secure_cookies = CountCookiesForPossibleDeletion(
        priority, cookies, protect_secure_cookies /* count secure cookies */);
    cookies_count_possibly_to_be_deleted -=
        std::max(secure_cookies, to_protect - secure_cookies);
  } else {
    cookies_count_possibly_to_be_deleted -= to_protect;
  }

  size_t removed = 0u;
  size_t current = 0u;
  while ((removed < purge_goal && current < cookies->size()) &&
         cookies_count_possibly_to_be_deleted > 0) {
    const CanonicalCookie* current_cookie = cookies->at(current)->second.get();
    // Only delete the current cookie if the priority is equal to
    // the current level.
    if (IsCookieEligibleForEviction(priority, protect_secure_cookies,
                                    current_cookie)) {
      InternalDeleteCookie(cookies->at(current), true,
                           DELETE_COOKIE_EVICTED_DOMAIN);
      cookies->erase(cookies->begin() + current);
      removed++;
      cookies_count_possibly_to_be_deleted--;
    } else {
      current++;
    }
  }
  return removed;
}

size_t CookieMonster::GarbageCollectExpired(const Time& current,
                                            const CookieMapItPair& itpair,
                                            CookieItVector* cookie_its) {
  DCHECK(thread_checker_.CalledOnValidThread());

  int num_deleted = 0;
  for (CookieMap::iterator it = itpair.first, end = itpair.second; it != end;) {
    auto curit = it;
    ++it;

    if (curit->second->IsExpired(current)) {
      InternalDeleteCookie(curit, true, DELETE_COOKIE_EXPIRED);
      ++num_deleted;
    } else if (cookie_its) {
      cookie_its->push_back(curit);
    }
  }

  return num_deleted;
}

size_t CookieMonster::GarbageCollectDeleteRange(
    const Time& current,
    DeletionCause cause,
    CookieItVector::iterator it_begin,
    CookieItVector::iterator it_end) {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (auto it = it_begin; it != it_end; it++) {
    InternalDeleteCookie((*it), true, cause);
  }
  return it_end - it_begin;
}

size_t CookieMonster::GarbageCollectLeastRecentlyAccessed(
    const base::Time& current,
    const base::Time& safe_date,
    size_t purge_goal,
    CookieItVector cookie_its,
    base::Time* earliest_time) {
  DCHECK_LE(purge_goal, cookie_its.size());
  DCHECK(thread_checker_.CalledOnValidThread());

  // Sorts up to *and including* |cookie_its[purge_goal]| (if it exists), so
  // |earliest_time| will be properly assigned even if
  // |global_purge_it| == |cookie_its.begin() + purge_goal|.
  SortLeastRecentlyAccessed(
      cookie_its.begin(), cookie_its.end(),
      cookie_its.size() < purge_goal ? purge_goal + 1 : purge_goal);
  // Find boundary to cookies older than safe_date.
  auto global_purge_it = LowerBoundAccessDate(
      cookie_its.begin(), cookie_its.begin() + purge_goal, safe_date);
  // Only delete the old cookies and delete non-secure ones first.
  size_t num_deleted =
      GarbageCollectDeleteRange(current, DELETE_COOKIE_EVICTED_GLOBAL,
                                cookie_its.begin(), global_purge_it);
  if (global_purge_it != cookie_its.end())
    *earliest_time = (*global_purge_it)->second->LastAccessDate();
  return num_deleted;
}

// A wrapper around registry_controlled_domains::GetDomainAndRegistry
// to make clear we're creating a key for our local map or for the persistent
// store's use. Here and in FindCookiesForRegistryControlledHost() are the only
// two places where we need to conditionalize based on key type.
//
// Note that this key algorithm explicitly ignores the scheme.  This is
// because when we're entering cookies into the map from the backing store,
// we in general won't have the scheme at that point.
// In practical terms, this means that file cookies will be stored
// in the map either by an empty string or by UNC name (and will be
// limited by kMaxCookiesPerHost), and extension cookies will be stored
// based on the single extension id, as the extension id won't have the
// form of a DNS host and hence GetKey() will return it unchanged.
//
// Arguably the right thing to do here is to make the key
// algorithm dependent on the scheme, and make sure that the scheme is
// available everywhere the key must be obtained (specfically at backing
// store load time).  This would require either changing the backing store
// database schema to include the scheme (far more trouble than it's worth), or
// separating out file cookies into their own CookieMonster instance and
// thus restricting each scheme to a single cookie monster (which might
// be worth it, but is still too much trouble to solve what is currently a
// non-problem).
//
// static
std::string CookieMonster::GetKey(base::StringPiece domain) {
  std::string effective_domain(
      registry_controlled_domains::GetDomainAndRegistry(
          domain, registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
  if (effective_domain.empty())
    effective_domain = std::string(domain);

  return cookie_util::CookieDomainAsHost(effective_domain);
}

bool CookieMonster::HasCookieableScheme(const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Make sure the request is on a cookie-able url scheme.
  bool is_cookieable = base::ranges::any_of(
      cookieable_schemes_, [&url](const std::string& cookieable_scheme) {
        return url.SchemeIs(cookieable_scheme.c_str());
      });

  if (!is_cookieable) {
    // The scheme didn't match any in our allowed list.
    DVLOG(net::cookie_util::kVlogPerCookieMonster)
        << "WARNING: Unsupported cookie scheme: " << url.scheme();
  }
  return is_cookieable;
}

CookieAccessSemantics CookieMonster::GetAccessSemanticsForCookie(
    const CanonicalCookie& cookie) const {
  if (cookie_access_delegate())
    return cookie_access_delegate()->GetAccessSemantics(cookie);
  return CookieAccessSemantics::UNKNOWN;
}

// Test to see if stats should be recorded, and record them if so.
// The goal here is to get sampling for the average browser-hour of
// activity.  We won't take samples when the web isn't being surfed,
// and when the web is being surfed, we'll take samples about every
// kRecordStatisticsIntervalSeconds.
// last_statistic_record_time_ is initialized to Now() rather than null
// in the constructor so that we won't take statistics right after
// startup, to avoid bias from browsers that are started but not used.
void CookieMonster::RecordPeriodicStats(const base::Time& current_time) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const base::TimeDelta kRecordStatisticsIntervalTime(
      base::TimeDelta::FromSeconds(kRecordStatisticsIntervalSeconds));

  // If we've taken statistics recently, return.
  if (current_time - last_statistic_record_time_ <=
      kRecordStatisticsIntervalTime) {
    return;
  }

  if (DoRecordPeriodicStats())
    last_statistic_record_time_ = current_time;
}

bool CookieMonster::DoRecordPeriodicStats() {
  // These values are all bogus if we have only partially loaded the cookies.
  if (started_fetching_all_cookies_ && !finished_fetching_all_cookies_)
    return false;

  // See InitializeHistograms() for details.
  histogram_count_->Add(cookies_.size());

  if (cookie_access_delegate()) {
    for (const auto& set : cookie_access_delegate()->RetrieveFirstPartySets()) {
      int sample = std::accumulate(
          set.second.begin(), set.second.end(), 0,
          [this](int acc, const net::SchemefulSite& site) -> int {
            if (!site.has_registrable_domain_or_host())
              return acc;
            return acc + cookies_.count(site.registrable_domain_or_host());
          });
      base::UmaHistogramCustomCounts("Cookie.PerFirstPartySetCount", sample, 0,
                                     4000, 50);
    }
  }

  // Can be up to kMaxDomainPurgedKeys.
  UMA_HISTOGRAM_COUNTS_100("Cookie.NumDomainPurgedKeys",
                           domain_purged_keys_.size());
  // Can be up to kMaxCookies.
  UMA_HISTOGRAM_COUNTS_10000("Cookie.NumKeys", num_keys_);

  return true;
}

// Initialize all histogram counter variables used in this class.
//
// TODO(https://crbug.com/1087445): remove this in favor of histogram_macros.h
// or histogram_functions.h usage, since both are now threadsafe.
void CookieMonster::InitializeHistograms() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // From UMA_HISTOGRAM_CUSTOM_COUNTS
  histogram_expiration_duration_minutes_secure_ = base::Histogram::FactoryGet(
      "Cookie.ExpirationDurationMinutesSecure", 1, kMinutesInTenYears, 50,
      base::Histogram::kUmaTargetedHistogramFlag);
  histogram_expiration_duration_minutes_non_secure_ =
      base::Histogram::FactoryGet("Cookie.ExpirationDurationMinutesNonSecure",
                                  1, kMinutesInTenYears, 50,
                                  base::Histogram::kUmaTargetedHistogramFlag);

  // From UMA_HISTOGRAM_COUNTS_100000
  // This replaces Cookie.Count which only counted up to 4000 cookies.
  histogram_count_ =
      base::Histogram::FactoryGet("Cookie.Count2", 1, 100000, 50,
                                  base::Histogram::kUmaTargetedHistogramFlag);

  // From UMA_HISTOGRAM_ENUMERATION
  histogram_cookie_type_ = base::LinearHistogram::FactoryGet(
      "Cookie.Type", 1, (1 << COOKIE_TYPE_LAST_ENTRY) - 1,
      1 << COOKIE_TYPE_LAST_ENTRY, base::Histogram::kUmaTargetedHistogramFlag);
  histogram_cookie_source_scheme_ = base::LinearHistogram::FactoryGet(
      "Cookie.CookieSourceScheme", 1, COOKIE_SOURCE_LAST_ENTRY - 1,
      COOKIE_SOURCE_LAST_ENTRY, base::Histogram::kUmaTargetedHistogramFlag);

  // From UMA_HISTOGRAM_{CUSTOM_,}TIMES
  histogram_time_blocked_on_load_ = base::Histogram::FactoryTimeGet(
      "Cookie.TimeBlockedOnLoad", base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromMinutes(1), 50,
      base::Histogram::kUmaTargetedHistogramFlag);
}

void CookieMonster::DoCookieCallback(base::OnceClosure callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  MarkCookieStoreAsInitialized();
  FetchAllCookiesIfNecessary();
  seen_global_task_ = true;

  if (!finished_fetching_all_cookies_ && store_.get()) {
    tasks_pending_.push_back(std::move(callback));
    return;
  }

  std::move(callback).Run();
}

void CookieMonster::DoCookieCallbackForURL(base::OnceClosure callback,
                                           const GURL& url) {
  DoCookieCallbackForHostOrDomain(std::move(callback), url.host_piece());
}

void CookieMonster::DoCookieCallbackForHostOrDomain(
    base::OnceClosure callback,
    base::StringPiece host_or_domain) {
  MarkCookieStoreAsInitialized();
  FetchAllCookiesIfNecessary();

  // If cookies for the requested domain key (eTLD+1) have been loaded from DB
  // then run the task, otherwise load from DB.
  if (!finished_fetching_all_cookies_ && store_.get()) {
    // If a global task has been previously seen, queue the task as a global
    // task. Note that the CookieMonster may be in the middle of executing
    // the global queue, |tasks_pending_| may be empty, which is why another
    // bool is needed.
    if (seen_global_task_) {
      tasks_pending_.push_back(std::move(callback));
      return;
    }

    // Checks if the domain key has been loaded.
    std::string key = GetKey(host_or_domain);
    if (keys_loaded_.find(key) == keys_loaded_.end()) {
      auto it = tasks_pending_for_key_.find(key);
      if (it == tasks_pending_for_key_.end()) {
        store_->LoadCookiesForKey(
            key, base::BindOnce(&CookieMonster::OnKeyLoaded,
                                weak_ptr_factory_.GetWeakPtr(), key));
        it = tasks_pending_for_key_
                 .insert(std::make_pair(
                     key, base::circular_deque<base::OnceClosure>()))
                 .first;
      }
      it->second.push_back(std::move(callback));
      return;
    }
  }

  std::move(callback).Run();
}

CookieMonster::CookieSentToSamePort
CookieMonster::IsCookieSentToSamePortThatSetIt(
    const GURL& destination,
    int source_port,
    CookieSourceScheme source_scheme) {
  if (source_port == url::PORT_UNSPECIFIED)
    return CookieSentToSamePort::kSourcePortUnspecified;

  if (source_port == url::PORT_INVALID)
    return CookieSentToSamePort::kInvalid;

  int destination_port = destination.EffectiveIntPort();
  if (source_port == destination_port)
    return CookieSentToSamePort::kYes;

  const std::string& destination_scheme = destination.scheme();
  bool destination_port_is_default =
      url::DefaultPortForScheme(destination_scheme.c_str(),
                                destination_scheme.length()) ==
      destination_port;

  // Since the source port has to be specified if we got to this point, that
  // means this is a newer cookie that therefore has its scheme set as well.
  DCHECK(source_scheme != CookieSourceScheme::kUnset);
  std::string source_scheme_string =
      source_scheme == CookieSourceScheme::kSecure
          ? url::kHttpsScheme
          : url::kHttpScheme;  // wss/ws have the same default port values as
                               // https/http, so it's ok that we use these.

  bool source_port_is_default =
      url::DefaultPortForScheme(source_scheme_string.c_str(),
                                source_scheme_string.length()) == source_port;

  if (destination_port_is_default && source_port_is_default)
    return CookieSentToSamePort::kNoButDefault;

  return CookieSentToSamePort::kNo;
}

}  // namespace net
