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
#include <set>

#include "base/bind.h"
#include "base/callback.h"
#include "base/debug/dump_without_crashing.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/process_memory_dump.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_monster_change_dispatcher.h"
#include "net/cookies/cookie_monster_netlog_params.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "net/log/net_log.h"
#include "net/ssl/channel_id_service.h"
#include "url/origin.h"

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

void MaybeRunCookieCallback(base::OnceClosure callback) {
  if (callback)
    std::move(callback).Run();
}

template <typename T>
void MaybeRunCookieCallback(base::OnceCallback<void(const T&)> callback,
                            const T& result) {
  if (callback)
    std::move(callback).Run(result);
}

template <typename T>
void MaybeRunCookieCallback(base::OnceCallback<void(T)> callback,
                            const T& result) {
  if (callback)
    std::move(callback).Run(result);
}

// Wraps a OnceClosure -- specifically one used by
// |GetCookieListWithOptionsAsync()| -- with additional bound state to track the
// duration between when its creation and destruction time.
// See https://crbug.com/824024 for context.
base::OnceClosure InstrumentGetCookieListClosure(base::OnceClosure closure) {
  return base::BindOnce(
      [](std::unique_ptr<base::ElapsedTimer> timer, base::OnceClosure closure) {
        UMA_HISTOGRAM_CUSTOM_TIMES("Cookie.GetCookieListCompletionTime",
                                   timer->Elapsed(),
                                   base::TimeDelta::FromMilliseconds(10),
                                   base::TimeDelta::FromSeconds(60), 50);
        std::move(closure).Run();
      },
      std::make_unique<base::ElapsedTimer>(), std::move(closure));
}

}  // namespace

namespace net {

// See comments at declaration of these variables in cookie_monster.h
// for details.
const size_t CookieMonster::kDomainMaxCookies = 180;
const size_t CookieMonster::kDomainPurgeCookies = 30;
const size_t CookieMonster::kMaxCookies = 3300;
const size_t CookieMonster::kPurgeCookies = 300;

const size_t CookieMonster::kDomainCookiesQuotaLow = 30;
const size_t CookieMonster::kDomainCookiesQuotaMedium = 50;
const size_t CookieMonster::kDomainCookiesQuotaHigh =
    kDomainMaxCookies - kDomainPurgeCookies - kDomainCookiesQuotaLow -
    kDomainCookiesQuotaMedium;

const int CookieMonster::kSafeFromGlobalPurgeDays = 30;

namespace {

bool ContainsControlCharacter(const std::string& s) {
  for (std::string::const_iterator i = s.begin(); i != s.end(); ++i) {
    if ((*i >= 0) && (*i <= 31))
      return true;
  }

  return false;
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

// Our strategy to find duplicates is:
// (1) Build a map from (cookiename, cookiepath) to
//     {list of cookies with this signature, sorted by creation time}.
// (2) For each list with more than 1 entry, keep the cookie having the
//     most recent creation time, and delete the others.
//
// Two cookies are considered equivalent if they have the same domain,
// name, and path.
struct CookieSignature {
 public:
  CookieSignature(const std::string& name,
                  const std::string& domain,
                  const std::string& path)
      : name(name), domain(domain), path(path) {}

  // To be a key for a map this class needs to be assignable, copyable,
  // and have an operator<.  The default assignment operator
  // and copy constructor are exactly what we want.

  bool operator<(const CookieSignature& cs) const {
    // Name compare dominates, then domain, then path.
    int diff = name.compare(cs.name);
    if (diff != 0)
      return diff < 0;

    diff = domain.compare(cs.domain);
    if (diff != 0)
      return diff < 0;

    return path.compare(cs.path) < 0;
  }

  std::string name;
  std::string domain;
  std::string path;
};

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
                             ChannelIDService* channel_id_service,
                             NetLog* net_log)
    : CookieMonster(
          std::move(store),
          channel_id_service,
          base::TimeDelta::FromSeconds(kDefaultAccessUpdateThresholdSeconds),
          net_log) {}

CookieMonster::CookieMonster(scoped_refptr<PersistentCookieStore> store,
                             base::TimeDelta last_access_threshold,
                             NetLog* net_log)
    : CookieMonster(std::move(store), nullptr, last_access_threshold, net_log) {
}

CookieMonster::CookieMonster(scoped_refptr<PersistentCookieStore> store,
                             ChannelIDService* channel_id_service,
                             base::TimeDelta last_access_threshold,
                             NetLog* net_log)
    : initialized_(false),
      started_fetching_all_cookies_(false),
      finished_fetching_all_cookies_(false),
      seen_global_task_(false),
      net_log_(NetLogWithSource::Make(net_log, NetLogSourceType::COOKIE_STORE)),
      store_(std::move(store)),
      last_access_threshold_(last_access_threshold),
      channel_id_service_(channel_id_service),
      last_statistic_record_time_(base::Time::Now()),
      persist_session_cookies_(false),
      weak_ptr_factory_(this) {
  InitializeHistograms();
  cookieable_schemes_.insert(
      cookieable_schemes_.begin(), kDefaultCookieableSchemes,
      kDefaultCookieableSchemes + kDefaultCookieableSchemesCount);
  if (channel_id_service_ && store_) {
    // |store_| can outlive this CookieMonster, but there are no guarantees
    // about the lifetime of |channel_id_service_| relative to |store_|. The
    // only guarantee is that |channel_id_service_| will outlive this
    // CookieMonster. To avoid the PersistentCookieStore retaining a pointer to
    // the ChannelIDStore via this callback after this CookieMonster is
    // destroyed, CookieMonster's d'tor sets the callback to a null callback.
    store_->SetBeforeFlushCallback(
        base::Bind(&ChannelIDStore::Flush,
                   base::Unretained(channel_id_service_->GetChannelIDStore())));
  }
  net_log_.BeginEvent(
      NetLogEventType::COOKIE_STORE_ALIVE,
      base::BindRepeating(&NetLogCookieMonsterConstructorCallback,
                          store != nullptr, channel_id_service != nullptr));
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
      // base::Unretained is safe as DoCookieCallbackForURL stores
      // the callback on |*this|, so the callback will not outlive
      // the object.
      &CookieMonster::SetAllCookies, base::Unretained(this), list,
      std::move(callback)));
}

void CookieMonster::SetCanonicalCookieAsync(
    std::unique_ptr<CanonicalCookie> cookie,
    bool secure_source,
    bool modify_http_only,
    SetCookiesCallback callback) {
  DCHECK(cookie->IsCanonical());

  std::string domain = cookie->Domain();
  DoCookieCallbackForHostOrDomain(
      base::BindOnce(
          // base::Unretained is safe as DoCookieCallbackForURL stores
          // the callback on |*this|, so the callback will not outlive
          // the object.
          &CookieMonster::SetCanonicalCookie, base::Unretained(this),
          std::move(cookie), secure_source, modify_http_only,
          std::move(callback)),
      domain);
}

void CookieMonster::SetCookieWithOptionsAsync(const GURL& url,
                                              const std::string& cookie_line,
                                              const CookieOptions& options,
                                              SetCookiesCallback callback) {
  DoCookieCallbackForURL(
      base::BindOnce(
          // base::Unretained is safe as DoCookieCallbackForURL stores
          // the callback on |*this|, so the callback will not outlive
          // the object.
          &CookieMonster::SetCookieWithOptions, base::Unretained(this), url,
          cookie_line, options, std::move(callback)),
      url);
}

void CookieMonster::GetCookieListWithOptionsAsync(
    const GURL& url,
    const CookieOptions& options,
    GetCookieListCallback callback) {
  DoCookieCallbackForURL(
      InstrumentGetCookieListClosure(base::BindOnce(
          // base::Unretained is safe as DoCookieCallbackForURL stores
          // the callback on |*this|, so the callback will not outlive
          // the object.
          &CookieMonster::GetCookieListWithOptions, base::Unretained(this), url,
          options, std::move(callback))),
      url);
}

void CookieMonster::GetAllCookiesAsync(GetCookieListCallback callback) {
  DoCookieCallback(base::BindOnce(
      // base::Unretained is safe as DoCookieCallbackForURL stores
      // the callback on |*this|, so the callback will not outlive
      // the object.
      &CookieMonster::GetAllCookies, base::Unretained(this),
      std::move(callback)));
}

void CookieMonster::DeleteCookieAsync(const GURL& url,
                                      const std::string& cookie_name,
                                      base::OnceClosure callback) {
  DoCookieCallbackForURL(
      base::BindOnce(
          // base::Unretained is safe as DoCookieCallbackForURL stores
          // the callback on |*this|, so the callback will not outlive
          // the object.
          &CookieMonster::DeleteCookie, base::Unretained(this), url,
          cookie_name, std::move(callback)),
      url);
}

void CookieMonster::DeleteCanonicalCookieAsync(const CanonicalCookie& cookie,
                                               DeleteCallback callback) {
  DoCookieCallback(base::BindOnce(
      // base::Unretained is safe as DoCookieCallbackForURL stores
      // the callback on |*this|, so the callback will not outlive
      // the object.
      &CookieMonster::DeleteCanonicalCookie, base::Unretained(this), cookie,
      std::move(callback)));
}

void CookieMonster::DeleteAllCreatedInTimeRangeAsync(
    const TimeRange& creation_range,
    DeleteCallback callback) {
  DoCookieCallback(base::BindOnce(
      // base::Unretained is safe as DoCookieCallbackForURL stores
      // the callback on |*this|, so the callback will not outlive
      // the object.
      &CookieMonster::DeleteAllCreatedInTimeRange, base::Unretained(this),
      creation_range, std::move(callback)));
}

void CookieMonster::DeleteAllMatchingInfoAsync(CookieDeletionInfo delete_info,
                                               DeleteCallback callback) {
  DoCookieCallback(base::BindOnce(
      // base::Unretained is safe as DoCookieCallbackForURL stores
      // the callback on |*this|, so the callback will not outlive
      // the object.
      &CookieMonster::DeleteAllMatchingInfo, base::Unretained(this),
      std::move(delete_info), std::move(callback)));
}

void CookieMonster::DeleteSessionCookiesAsync(
    CookieStore::DeleteCallback callback) {
  DoCookieCallback(base::BindOnce(
      // base::Unretained is safe as DoCookieCallbackForURL stores
      // the callback on |*this|, so the callback will not outlive
      // the object.
      &CookieMonster::DeleteSessionCookies, base::Unretained(this),
      std::move(callback)));
}

void CookieMonster::SetCookieableSchemes(
    const std::vector<std::string>& schemes) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Calls to this method will have no effect if made after a WebView or
  // CookieManager instance has been created.
  if (initialized_)
    return;

  cookieable_schemes_ = schemes;
}

// This function must be called before the CookieMonster is used.
void CookieMonster::SetPersistSessionCookies(bool persist_session_cookies) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!initialized_);
  net_log_.AddEvent(
      NetLogEventType::COOKIE_STORE_SESSION_PERSISTENCE,
      NetLog::BoolCallback("persistence", persist_session_cookies));
  persist_session_cookies_ = persist_session_cookies;
}

bool CookieMonster::IsCookieableScheme(const std::string& scheme) {
  DCHECK(thread_checker_.CalledOnValidThread());

  return base::ContainsValue(cookieable_schemes_, scheme);
}

const char* const CookieMonster::kDefaultCookieableSchemes[] = {"http", "https",
                                                                "ws", "wss"};
const int CookieMonster::kDefaultCookieableSchemesCount =
    base::size(kDefaultCookieableSchemes);

CookieChangeDispatcher& CookieMonster::GetChangeDispatcher() {
  return change_dispatcher_;
}

bool CookieMonster::IsEphemeral() {
  return store_.get() == nullptr;
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

  if (channel_id_service_ && store_) {
    store_->SetBeforeFlushCallback(base::Closure());
  }

  // TODO(mmenke): Does it really make sense to run
  // CookieChanged callbacks when the CookieStore is destroyed?
  for (auto cookie_it = cookies_.begin(); cookie_it != cookies_.end();) {
    auto current_cookie_it = cookie_it;
    ++cookie_it;
    InternalDeleteCookie(current_cookie_it, false /* sync_to_store */,
                         DELETE_COOKIE_DONT_RECORD);
  }
  net_log_.EndEvent(NetLogEventType::COOKIE_STORE_ALIVE);
}

void CookieMonster::GetAllCookies(GetCookieListCallback callback) {
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
      Time::Now(), CookieMapItPair(cookies_.begin(), cookies_.end()), NULL);

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

void CookieMonster::GetCookieListWithOptions(const GURL& url,
                                             const CookieOptions& options,
                                             GetCookieListCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  CookieList cookies;
  if (HasCookieableScheme(url)) {
    std::vector<CanonicalCookie*> cookie_ptrs;
    FindCookiesForHostAndDomain(url, options, &cookie_ptrs);
    std::sort(cookie_ptrs.begin(), cookie_ptrs.end(), CookieSorter);

    cookies.reserve(cookie_ptrs.size());
    for (std::vector<CanonicalCookie*>::const_iterator it = cookie_ptrs.begin();
         it != cookie_ptrs.end(); it++)
      cookies.push_back(**it);
  }
  MaybeRunCookieCallback(std::move(callback), cookies);
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

    if (delete_info.Matches(*cc)) {
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

void CookieMonster::SetCookieWithOptions(const GURL& url,
                                         const std::string& cookie_line,
                                         const CookieOptions& options,
                                         SetCookiesCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!HasCookieableScheme(url)) {
    MaybeRunCookieCallback(std::move(callback), false);
    return;
  }

  VLOG(net::cookie_util::kVlogSetCookies)
      << "SetCookie() line: " << cookie_line;

  Time creation_time = CurrentTime();
  last_time_seen_ = creation_time;

  std::unique_ptr<CanonicalCookie> cc(
      CanonicalCookie::Create(url, cookie_line, creation_time, options));

  if (!cc.get()) {
    VLOG(net::cookie_util::kVlogSetCookies)
        << "WARNING: Failed to allocate CanonicalCookie";
    MaybeRunCookieCallback(std::move(callback), false);
    return;
  }
  SetCanonicalCookie(std::move(cc), url.SchemeIsCryptographic(),
                     !options.exclude_httponly(), std::move(callback));
}

void CookieMonster::DeleteCookie(const GURL& url,
                                 const std::string& cookie_name,
                                 base::OnceClosure callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!HasCookieableScheme(url)) {
    // TODO(rdsmith): Would be good to provide a failure indication here.
    MaybeRunCookieCallback(std::move(callback));
    return;
  }

  CookieOptions options;
  options.set_include_httponly();
  options.set_same_site_cookie_mode(
      CookieOptions::SameSiteCookieMode::INCLUDE_STRICT_AND_LAX);
  // Get the cookies for this host and its domain(s).
  std::vector<CanonicalCookie*> cookies;
  FindCookiesForHostAndDomain(url, options, &cookies);
  std::set<CanonicalCookie*> matching_cookies;

  for (auto* cookie : cookies) {
    if (cookie->Name() != cookie_name)
      continue;
    if (!cookie->IsOnPath(url.path()))
      continue;
    matching_cookies.insert(cookie);
  }

  for (auto it = cookies_.begin(); it != cookies_.end();) {
    auto curit = it;
    ++it;
    if (matching_cookies.find(curit->second.get()) != matching_cookies.end()) {
      InternalDeleteCookie(curit, true, DELETE_COOKIE_EXPLICIT);
    }
  }

  FlushStore(base::BindOnce(&MaybeRunDeleteCallback,
                            weak_ptr_factory_.GetWeakPtr(),
                            // No callback null check needed as BindOnce
                            // is not being called and MaybeRunDeleteCallback
                            // has its own check.
                            std::move(callback)));
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
        candidate->CreationDate() == cookie.CreationDate() &&
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
  store_->Load(base::Bind(&CookieMonster::OnLoaded,
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

  for (auto& cookie : cookies) {
    CanonicalCookie* cookie_ptr = cookie.get();
    auto inserted = InternalInsertCookie(GetKey(cookie_ptr->Domain()),
                                         std::move(cookie), false);
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

void CookieMonster::TrimDuplicateCookiesForKey(const std::string& key,
                                               CookieMap::iterator begin,
                                               CookieMap::iterator end) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Set of cookies ordered by creation time.
  typedef std::multiset<CookieMap::iterator, OrderByCreationTimeDesc> CookieSet;

  // Helper map we populate to find the duplicates.
  typedef std::map<CookieSignature, CookieSet> EquivalenceMap;
  EquivalenceMap equivalent_cookies;

  // The number of duplicate cookies that have been found.
  int num_duplicates = 0;

  // Iterate through all of the cookies in our range, and insert them into
  // the equivalence map.
  for (auto it = begin; it != end; ++it) {
    DCHECK_EQ(key, it->first);
    CanonicalCookie* cookie = it->second.get();

    CookieSignature signature(cookie->Name(), cookie->Domain(), cookie->Path());
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
  for (auto it = equivalent_cookies.begin(); it != equivalent_cookies.end();
       ++it) {
    const CookieSignature& signature = it->first;
    CookieSet& dupes = it->second;

    if (dupes.size() <= 1)
      continue;  // This cookiename/path has no duplicates.
    num_duplicates_found += dupes.size() - 1;

    // Since |dupes| is sorted by creation time (descending), the first cookie
    // is the most recent one (or tied for it), so we will keep it. The rest are
    // duplicates.
    dupes.erase(dupes.begin());

    LOG(ERROR) << base::StringPrintf(
        "Found %d duplicate cookies for host='%s', "
        "with {name='%s', domain='%s', path='%s'}",
        static_cast<int>(dupes.size()), key.c_str(), signature.name.c_str(),
        signature.domain.c_str(), signature.path.c_str());

    // Remove all the cookies identified by |dupes|. It is valid to delete our
    // list of iterators one at a time, since |cookies_| is a multimap (they
    // don't invalidate existing iterators following deletion).
    for (auto dupes_it = dupes.begin(); dupes_it != dupes.end(); ++dupes_it) {
      InternalDeleteCookie(*dupes_it, true,
                           DELETE_COOKIE_DUPLICATE_IN_BACKING_STORE);
    }
  }
  DCHECK_EQ(num_duplicates, num_duplicates_found);
}

void CookieMonster::FindCookiesForHostAndDomain(
    const GURL& url,
    const CookieOptions& options,
    std::vector<CanonicalCookie*>* cookies) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const Time current_time(CurrentTime());

  // Probe to save statistics relatively frequently.  We do it here rather
  // than in the set path as many websites won't set cookies, and we
  // want to collect statistics whenever the browser's being used.
  RecordPeriodicStats(current_time);

  // Can just dispatch to FindCookiesForKey
  const std::string key(GetKey(url.host_piece()));
  FindCookiesForKey(key, url, options, current_time, cookies);
}

void CookieMonster::FindCookiesForKey(const std::string& key,
                                      const GURL& url,
                                      const CookieOptions& options,
                                      const Time& current,
                                      std::vector<CanonicalCookie*>* cookies) {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (CookieMapItPair its = cookies_.equal_range(key);
       its.first != its.second;) {
    auto curit = its.first;
    CanonicalCookie* cc = curit->second.get();
    ++its.first;

    // If the cookie is expired, delete it.
    if (cc->IsExpired(current)) {
      InternalDeleteCookie(curit, true, DELETE_COOKIE_EXPIRED);
      continue;
    }

    // Filter out cookies that should not be included for a request to the
    // given |url|. HTTP only cookies are filtered depending on the passed
    // cookie |options|.
    if (!cc->IncludeForRequestURL(url, options))
      continue;

    // Add this cookie to the set of matching cookies. Update the access
    // time if we've been requested to do so.
    if (options.update_access_time()) {
      InternalUpdateCookieAccessTime(cc, current);
    }
    cookies->push_back(cc);
  }
}

bool CookieMonster::DeleteAnyEquivalentCookie(
    const std::string& key,
    const CanonicalCookie& ecc,
    bool source_secure,
    bool skip_httponly,
    bool already_expired,
    base::Time* creation_date_to_inherit) {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool found_equivalent_cookie = false;
  bool skipped_httponly = false;
  bool skipped_secure_cookie = false;

  histogram_cookie_delete_equivalent_->Add(COOKIE_DELETE_EQUIVALENT_ATTEMPT);

  CookieMap::iterator cookie_it_to_possibly_delete = cookies_.end();
  CanonicalCookie* cc_skipped_secure = nullptr;
  for (CookieMapItPair its = cookies_.equal_range(key);
       its.first != its.second;) {
    auto curit = its.first;
    CanonicalCookie* cc = curit->second.get();
    ++its.first;

    // If the cookie is being set from an insecure scheme, then if a cookie
    // already exists with the same name and it is Secure, then the cookie
    // should *not* be updated if they domain-match and ignoring the path
    // attribute.
    //
    // See: https://tools.ietf.org/html/draft-ietf-httpbis-cookie-alone
    if (cc->IsSecure() && !source_secure &&
        ecc.IsEquivalentForSecureCookieMatching(*cc)) {
      skipped_secure_cookie = true;
      cc_skipped_secure = cc;
      histogram_cookie_delete_equivalent_->Add(
          COOKIE_DELETE_EQUIVALENT_SKIPPING_SECURE);
      net_log_.AddEvent(
          NetLogEventType::COOKIE_STORE_COOKIE_REJECTED_SECURE,
          base::BindRepeating(&NetLogCookieMonsterCookieRejectedSecure, cc,
                              &ecc));
      // If the cookie is equivalent to the new cookie and wouldn't have been
      // skipped for being HTTP-only, record that it is a skipped secure cookie
      // that would have been deleted otherwise.
      if (ecc.IsEquivalent(*cc)) {
        found_equivalent_cookie = true;

        if (!skip_httponly || !cc->IsHttpOnly()) {
          histogram_cookie_delete_equivalent_->Add(
              COOKIE_DELETE_EQUIVALENT_WOULD_HAVE_DELETED);
        }
      }
    } else if (ecc.IsEquivalent(*cc)) {
      // We should never have more than one equivalent cookie, since they should
      // overwrite each other, unless secure cookies require secure scheme is
      // being enforced. In that case, cookies with different paths might exist
      // and be considered equivalent.
      CHECK(!found_equivalent_cookie)
          << "Duplicate equivalent cookies found, cookie store is corrupted.";
      DCHECK(cookie_it_to_possibly_delete == cookies_.end());
      if (skip_httponly && cc->IsHttpOnly()) {
        skipped_httponly = true;
        net_log_.AddEvent(
            NetLogEventType::COOKIE_STORE_COOKIE_REJECTED_HTTPONLY,
            base::BindRepeating(&NetLogCookieMonsterCookieRejectedHttponly, cc,
                                &ecc));
      } else {
        cookie_it_to_possibly_delete = curit;
      }
      found_equivalent_cookie = true;
    }
  }

  if (cookie_it_to_possibly_delete != cookies_.end()) {
    CanonicalCookie* cc_to_possibly_delete =
        cookie_it_to_possibly_delete->second.get();
    // If a secure cookie was encountered (and left alone), don't actually
    // modify any of the pre-existing cookies. Only delete if no secure cookies
    // were skipped.
    if (!skipped_secure_cookie) {
      histogram_cookie_delete_equivalent_->Add(COOKIE_DELETE_EQUIVALENT_FOUND);
      if (cc_to_possibly_delete->Value() == ecc.Value()) {
        *creation_date_to_inherit = cc_to_possibly_delete->CreationDate();
        histogram_cookie_delete_equivalent_->Add(
            COOKIE_DELETE_EQUIVALENT_FOUND_WITH_SAME_VALUE);
      }
      InternalDeleteCookie(cookie_it_to_possibly_delete, true,
                           already_expired ? DELETE_COOKIE_EXPIRED_OVERWRITE
                                           : DELETE_COOKIE_OVERWRITE);
    } else {
      // If any secure cookie was skipped, preserve the pre-existing cookie.
      DCHECK(cc_skipped_secure);
      net_log_.AddEvent(
          NetLogEventType::COOKIE_STORE_COOKIE_PRESERVED_SKIPPED_SECURE,
          base::BindRepeating(&NetLogCookieMonsterCookiePreservedSkippedSecure,
                              cc_skipped_secure, cc_to_possibly_delete, &ecc));
    }
  }

  return skipped_httponly || skipped_secure_cookie;
}

CookieMonster::CookieMap::iterator CookieMonster::InternalInsertCookie(
    const std::string& key,
    std::unique_ptr<CanonicalCookie> cc,
    bool sync_to_store) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CanonicalCookie* cc_ptr = cc.get();

  net_log_.AddEvent(NetLogEventType::COOKIE_STORE_COOKIE_ADDED,
                    base::BindRepeating(&NetLogCookieMonsterCookieAdded,
                                        cc.get(), sync_to_store));
  if ((cc_ptr->IsPersistent() || persist_session_cookies_) && store_.get() &&
      sync_to_store) {
    store_->AddCookie(*cc_ptr);
  }
  auto inserted = cookies_.insert(CookieMap::value_type(key, std::move(cc)));

  // See InitializeHistograms() for details.
  int32_t type_sample = cc_ptr->SameSite() != CookieSameSite::NO_RESTRICTION
                            ? 1 << COOKIE_TYPE_SAME_SITE
                            : 0;
  type_sample |= cc_ptr->IsHttpOnly() ? 1 << COOKIE_TYPE_HTTPONLY : 0;
  type_sample |= cc_ptr->IsSecure() ? 1 << COOKIE_TYPE_SECURE : 0;
  histogram_cookie_type_->Add(type_sample);

  change_dispatcher_.DispatchChange(*cc_ptr, CookieChangeCause::INSERTED, true);

  return inserted;
}

void CookieMonster::SetCanonicalCookie(std::unique_ptr<CanonicalCookie> cc,
                                       bool secure_source,
                                       bool modify_http_only,
                                       SetCookiesCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if ((cc->IsSecure() && !secure_source) ||
      (cc->IsHttpOnly() && !modify_http_only)) {
    MaybeRunCookieCallback(std::move(callback), false);
    return;
  }

  const std::string key(GetKey(cc->Domain()));

  // TODO(mmenke): This class assumes each cookie to have a unique creation
  // time. Allowing the caller to set the creation time violates that
  // assumption. Worth fixing? Worth noting that time changes between browser
  // restarts can cause the same issue.
  base::Time creation_date = cc->CreationDate();
  if (creation_date.is_null()) {
    creation_date = CurrentTime();
    cc->SetCreationDate(creation_date);
    last_time_seen_ = creation_date;
  }
  bool already_expired = cc->IsExpired(creation_date);

  base::Time creation_date_to_inherit;
  if (DeleteAnyEquivalentCookie(key, *cc, secure_source, !modify_http_only,
                                already_expired, &creation_date_to_inherit)) {
    std::string error;
    error =
        "SetCookie() not clobbering httponly cookie or secure cookie for "
        "insecure scheme";

    VLOG(net::cookie_util::kVlogSetCookies) << error;
    MaybeRunCookieCallback(std::move(callback), false);
    return;
  }

  VLOG(net::cookie_util::kVlogSetCookies)
      << "SetCookie() key: " << key << " cc: " << cc->DebugString();

  // Realize that we might be setting an expired cookie, and the only point
  // was to delete the cookie which we've already done.
  if (!already_expired) {
    // See InitializeHistograms() for details.
    if (cc->IsPersistent()) {
      histogram_expiration_duration_minutes_->Add(
          (cc->ExpiryDate() - creation_date).InMinutes());
    }

    // Histogram the type of scheme used on URLs that set cookies. This
    // intentionally includes cookies that are set or overwritten by
    // http:// URLs, but not cookies that are cleared by http:// URLs, to
    // understand if the former behavior can be deprecated for Secure
    // cookies.
    CookieSource cookie_source_sample =
        (secure_source
             ? (cc->IsSecure()
                    ? COOKIE_SOURCE_SECURE_COOKIE_CRYPTOGRAPHIC_SCHEME
                    : COOKIE_SOURCE_NONSECURE_COOKIE_CRYPTOGRAPHIC_SCHEME)
             : (cc->IsSecure()
                    ? COOKIE_SOURCE_SECURE_COOKIE_NONCRYPTOGRAPHIC_SCHEME
                    : COOKIE_SOURCE_NONSECURE_COOKIE_NONCRYPTOGRAPHIC_SCHEME));
    histogram_cookie_source_scheme_->Add(cookie_source_sample);

    if (!creation_date_to_inherit.is_null()) {
      cc->SetCreationDate(creation_date_to_inherit);
      // |last_time_seen_| is intentionally not updated, as moving it into the
      // past might cause duplicate cookie creation dates. See
      // `CookieMonster::CurrentTime()` for details.
    }

    InternalInsertCookie(key, std::move(cc), true);
  } else {
    VLOG(net::cookie_util::kVlogSetCookies)
        << "SetCookie() not storing already expired cookie.";
  }

  // We assume that hopefully setting a cookie will be less common than
  // querying a cookie.  Since setting a cookie can put us over our limits,
  // make sure that we garbage collect...  We can also make the assumption that
  // if a cookie was set, in the common case it will be used soon after,
  // and we will purge the expired cookies in GetCookies().
  GarbageCollect(creation_date, key);

  MaybeRunCookieCallback(std::move(callback), true);
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
      histogram_expiration_duration_minutes_->Add(
          (cookie.ExpiryDate() - creation_time).InMinutes());
    }

    InternalInsertCookie(key, std::make_unique<CanonicalCookie>(cookie), true);
    GarbageCollect(creation_time, key);
  }

  // TODO(rdsmith): If this function always returns the same value, it
  // shouldn't have a return value.  But it should also be deleted (see
  // https://codereview.chromium.org/2882063002/#msg64), which would
  // solve the return value problem.
  MaybeRunCookieCallback(std::move(callback), true);
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
  VLOG(net::cookie_util::kVlogSetCookies)
      << "InternalDeleteCookie()"
      << ", cause:" << deletion_cause << ", cc: " << cc->DebugString();

  ChangeCausePair mapping = kChangeCauseMapping[deletion_cause];
  if (deletion_cause != DELETE_COOKIE_DONT_RECORD) {
    net_log_.AddEvent(NetLogEventType::COOKIE_STORE_COOKIE_DELETED,
                      base::BindRepeating(&NetLogCookieMonsterCookieDeleted, cc,
                                          mapping.cause, sync_to_store));
  }

  if ((cc->IsPersistent() || persist_session_cookies_) && store_.get() &&
      sync_to_store) {
    store_->DeleteCookie(*cc);
  }
  change_dispatcher_.DispatchChange(*cc, mapping.cause, mapping.notify);
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
    VLOG(net::cookie_util::kVlogGarbageCollection)
        << "GarbageCollect() key: " << key;

    CookieItVector* cookie_its;

    CookieItVector non_expired_cookie_its;
    cookie_its = &non_expired_cookie_its;
    num_deleted +=
        GarbageCollectExpired(current, cookies_.equal_range(key), cookie_its);

    if (cookie_its->size() > kDomainMaxCookies) {
      VLOG(net::cookie_util::kVlogGarbageCollection)
          << "Deep Garbage Collect domain.";
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
    VLOG(net::cookie_util::kVlogGarbageCollection)
        << "GarbageCollect() everything";
    CookieItVector cookie_its;

    num_deleted += GarbageCollectExpired(
        current, CookieMapItPair(cookies_.begin(), cookies_.end()),
        &cookie_its);

    if (cookie_its.size() > kMaxCookies) {
      VLOG(net::cookie_util::kVlogGarbageCollection)
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
// store's use. Here and in FindCookiesForHostAndDomain() are the only two
// places where we need to conditionalize based on key type.
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
    domain.CopyToString(&effective_domain);

  if (!effective_domain.empty() && effective_domain[0] == '.')
    return effective_domain.substr(1);
  return effective_domain;
}

bool CookieMonster::HasCookieableScheme(const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Make sure the request is on a cookie-able url scheme.
  for (size_t i = 0; i < cookieable_schemes_.size(); ++i) {
    // We matched a scheme.
    if (url.SchemeIs(cookieable_schemes_[i].c_str())) {
      // We've matched a supported scheme.
      return true;
    }
  }

  // The scheme didn't match any in our whitelist.
  VLOG(net::cookie_util::kVlogPerCookieMonster)
      << "WARNING: Unsupported cookie scheme: " << url.scheme();
  return false;
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

  // See InitializeHistograms() for details.
  histogram_count_->Add(cookies_.size());

  // More detailed statistics on cookie counts at different granularities.
  last_statistic_record_time_ = current_time;
}

// Initialize all histogram counter variables used in this class.
//
// Normal histogram usage involves using the macros defined in
// histogram.h, which automatically takes care of declaring these
// variables (as statics), initializing them, and accumulating into
// them, all from a single entry point.  Unfortunately, that solution
// doesn't work for the CookieMonster, as it's vulnerable to races between
// separate threads executing the same functions and hence initializing the
// same static variables.  There isn't a race danger in the histogram
// accumulation calls; they are written to be resilient to simultaneous
// calls from multiple threads.
//
// The solution taken here is to have per-CookieMonster instance
// variables that are constructed during CookieMonster construction.
// Note that these variables refer to the same underlying histogram,
// so we still race (but safely) with other CookieMonster instances
// for accumulation.
//
// To do this we've expanded out the individual histogram macros calls,
// with declarations of the variables in the class decl, initialization here
// (done from the class constructor) and direct calls to the accumulation
// methods where needed.  The specific histogram macro calls on which the
// initialization is based are included in comments below.
void CookieMonster::InitializeHistograms() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // From UMA_HISTOGRAM_CUSTOM_COUNTS
  histogram_expiration_duration_minutes_ = base::Histogram::FactoryGet(
      "Cookie.ExpirationDurationMinutes", 1, kMinutesInTenYears, 50,
      base::Histogram::kUmaTargetedHistogramFlag);
  histogram_count_ = base::Histogram::FactoryGet(
      "Cookie.Count", 1, 4000, 50, base::Histogram::kUmaTargetedHistogramFlag);

  // From UMA_HISTOGRAM_ENUMERATION
  histogram_cookie_type_ = base::LinearHistogram::FactoryGet(
      "Cookie.Type", 1, (1 << COOKIE_TYPE_LAST_ENTRY) - 1,
      1 << COOKIE_TYPE_LAST_ENTRY, base::Histogram::kUmaTargetedHistogramFlag);
  histogram_cookie_source_scheme_ = base::LinearHistogram::FactoryGet(
      "Cookie.CookieSourceScheme", 1, COOKIE_SOURCE_LAST_ENTRY - 1,
      COOKIE_SOURCE_LAST_ENTRY, base::Histogram::kUmaTargetedHistogramFlag);
  histogram_cookie_delete_equivalent_ = base::LinearHistogram::FactoryGet(
      "Cookie.CookieDeleteEquivalent", 1,
      COOKIE_DELETE_EQUIVALENT_LAST_ENTRY - 1,
      COOKIE_DELETE_EQUIVALENT_LAST_ENTRY,
      base::Histogram::kUmaTargetedHistogramFlag);

  // From UMA_HISTOGRAM_{CUSTOM_,}TIMES
  histogram_time_blocked_on_load_ = base::Histogram::FactoryTimeGet(
      "Cookie.TimeBlockedOnLoad", base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromMinutes(1), 50,
      base::Histogram::kUmaTargetedHistogramFlag);
}

// The system resolution is not high enough, so we can have multiple
// set cookies that result in the same system time.  When this happens, we
// increment by one Time unit.  Let's hope computers don't get too fast.
Time CookieMonster::CurrentTime() {
  return std::max(Time::Now(), Time::FromInternalValue(
                                   last_time_seen_.ToInternalValue() + 1));
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
            key, base::Bind(&CookieMonster::OnKeyLoaded,
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

}  // namespace net
