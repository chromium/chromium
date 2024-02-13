// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_store_test_helpers.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_util.h"
#include "net/http/http_util.h"
#include "url/gurl.h"

using net::registry_controlled_domains::GetDomainAndRegistry;
using net::registry_controlled_domains::GetRegistryLength;
using net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES;
using net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES;
using TimeRange = net::CookieDeletionInfo::TimeRange;

namespace {

std::string GetRegistry(const GURL& url) {
  size_t registry_length = GetRegistryLength(url, INCLUDE_UNKNOWN_REGISTRIES,
                                             INCLUDE_PRIVATE_REGISTRIES);
  if (registry_length == 0)
    return std::string();
  return std::string(url.host(), url.host().length() - registry_length,
                     registry_length);
}

}  // namespace

namespace net {

const int kDelayedTime = 0;

DelayedCookieMonsterChangeDispatcher::DelayedCookieMonsterChangeDispatcher() =
    default;
DelayedCookieMonsterChangeDispatcher::~DelayedCookieMonsterChangeDispatcher() =
    default;

std::unique_ptr<CookieChangeSubscription>
DelayedCookieMonsterChangeDispatcher::AddCallbackForCookie(
    const GURL& url,
    const std::string& name,
    const std::optional<CookiePartitionKey>& cookie_partition_key,
    CookieChangeCallback callback) {
  ADD_FAILURE();
  return nullptr;
}
std::unique_ptr<CookieChangeSubscription>
DelayedCookieMonsterChangeDispatcher::AddCallbackForUrl(
    const GURL& url,
    const std::optional<CookiePartitionKey>& cookie_partition_key,
    CookieChangeCallback callback) {
  ADD_FAILURE();
  return nullptr;
}
std::unique_ptr<CookieChangeSubscription>
DelayedCookieMonsterChangeDispatcher::AddCallbackForAllChanges(
    CookieChangeCallback callback) {
  ADD_FAILURE();
  return nullptr;
}

DelayedCookieMonster::DelayedCookieMonster()
    : cookie_monster_(std::make_unique<CookieMonster>(nullptr /* store */,
                                                      nullptr /* netlog */)),
      result_(CookieAccessResult(CookieInclusionStatus(
          CookieInclusionStatus::EXCLUDE_FAILURE_TO_STORE))) {}

DelayedCookieMonster::~DelayedCookieMonster() = default;

void DelayedCookieMonster::SetCookiesInternalCallback(
    CookieAccessResult result) {
  result_ = result;
  did_run_ = true;
}

void DelayedCookieMonster::GetCookieListWithOptionsInternalCallback(
    const CookieAccessResultList& cookie_list,
    const CookieAccessResultList& excluded_cookies) {
  cookie_access_result_list_ = cookie_list;
  cookie_list_ = cookie_util::StripAccessResults(cookie_access_result_list_);
  did_run_ = true;
}

void DelayedCookieMonster::SetCanonicalCookieAsync(
    std::unique_ptr<CanonicalCookie> cookie,
    const GURL& source_url,
    const CookieOptions& options,
    SetCookiesCallback callback,
    std::optional<CookieAccessResult> cookie_access_result) {
  did_run_ = false;
  cookie_monster_->SetCanonicalCookieAsync(
      std::move(cookie), source_url, options,
      base::BindOnce(&DelayedCookieMonster::SetCookiesInternalCallback,
                     base::Unretained(this)),
      std::move(cookie_access_result));
  DCHECK_EQ(did_run_, true);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DelayedCookieMonster::InvokeSetCookiesCallback,
                     base::Unretained(this), std::move(callback)),
      base::Milliseconds(kDelayedTime));
}

void DelayedCookieMonster::GetCookieListWithOptionsAsync(
    const GURL& url,
    const CookieOptions& options,
    const CookiePartitionKeyCollection& cookie_partition_key_collection,
    CookieMonster::GetCookieListCallback callback) {
  did_run_ = false;
  cookie_monster_->GetCookieListWithOptionsAsync(
      url, options, cookie_partition_key_collection,
      base::BindOnce(
          &DelayedCookieMonster::GetCookieListWithOptionsInternalCallback,
          base::Unretained(this)));
  DCHECK_EQ(did_run_, true);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DelayedCookieMonster::InvokeGetCookieListCallback,
                     base::Unretained(this), std::move(callback)),
      base::Milliseconds(kDelayedTime));
}

void DelayedCookieMonster::GetAllCookiesAsync(GetAllCookiesCallback callback) {
  cookie_monster_->GetAllCookiesAsync(std::move(callback));
}

void DelayedCookieMonster::InvokeSetCookiesCallback(
    CookieMonster::SetCookiesCallback callback) {
  if (!callback.is_null())
    std::move(callback).Run(result_);
}

void DelayedCookieMonster::InvokeGetCookieListCallback(
    CookieMonster::GetCookieListCallback callback) {
  if (!callback.is_null())
    std::move(callback).Run(cookie_access_result_list_,
                            CookieAccessResultList());
}

void DelayedCookieMonster::DeleteCanonicalCookieAsync(
    const CanonicalCookie& cookie,
    DeleteCallback callback) {
  ADD_FAILURE();
}

void DelayedCookieMonster::DeleteAllCreatedInTimeRangeAsync(
    const TimeRange& creation_range,
    DeleteCallback callback) {
  ADD_FAILURE();
}

void DelayedCookieMonster::DeleteAllMatchingInfoAsync(
    net::CookieDeletionInfo delete_info,
    DeleteCallback callback) {
  ADD_FAILURE();
}

void DelayedCookieMonster::DeleteMatchingCookiesAsync(DeletePredicate,
                                                      DeleteCallback) {
  ADD_FAILURE();
}

void DelayedCookieMonster::DeleteSessionCookiesAsync(DeleteCallback) {
  ADD_FAILURE();
}

void DelayedCookieMonster::FlushStore(base::OnceClosure callback) {
  ADD_FAILURE();
}

CookieChangeDispatcher& DelayedCookieMonster::GetChangeDispatcher() {
  return change_dispatcher_;
}

void DelayedCookieMonster::SetCookieableSchemes(
    const std::vector<std::string>& schemes,
    SetCookieableSchemesCallback callback) {
  ADD_FAILURE();
}

//
// CookieURLHelper
//
CookieURLHelper::CookieURLHelper(const std::string& url_string)
    : url_(url_string),
      registry_(GetRegistry(url_)),
      domain_and_registry_(
          GetDomainAndRegistry(url_, INCLUDE_PRIVATE_REGISTRIES)) {}

const GURL CookieURLHelper::AppendPath(const std::string& path) const {
  return GURL(url_.spec() + path);
}

std::string CookieURLHelper::Format(const std::string& format_string) const {
  std::string new_string = format_string;
  base::ReplaceSubstringsAfterOffset(&new_string, 0, "%D",
                                     domain_and_registry_);
  base::ReplaceSubstringsAfterOffset(&new_string, 0, "%R", registry_);
  return new_string;
}

//
// FlushablePersistentStore
//
FlushablePersistentStore::FlushablePersistentStore() = default;

void FlushablePersistentStore::Load(LoadedCallback loaded_callback,
                                    const NetLogWithSource& /* net_log */) {
  std::vector<std::unique_ptr<CanonicalCookie>> out_cookies;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(loaded_callback), std::move(out_cookies)));
}

void FlushablePersistentStore::LoadCookiesForKey(
    const std::string& key,
    LoadedCallback loaded_callback) {
  Load(std::move(loaded_callback), NetLogWithSource());
}

void FlushablePersistentStore::AddCookie(const CanonicalCookie&) {}

void FlushablePersistentStore::UpdateCookieAccessTime(const CanonicalCookie&) {}

void FlushablePersistentStore::DeleteCookie(const CanonicalCookie&) {}

void FlushablePersistentStore::SetForceKeepSessionState() {}

void FlushablePersistentStore::SetBeforeCommitCallback(
    base::RepeatingClosure callback) {}

void FlushablePersistentStore::Flush(base::OnceClosure callback) {
  base::AutoLock lock(flush_count_lock_);
  ++flush_count_;
  std::move(callback).Run();
}

int FlushablePersistentStore::flush_count() {
  base::AutoLock lock(flush_count_lock_);
  return flush_count_;
}

FlushablePersistentStore::~FlushablePersistentStore() = default;

//
// CallbackCounter
//
CallbackCounter::CallbackCounter() = default;

void CallbackCounter::Callback() {
  base::AutoLock lock(callback_count_lock_);
  ++callback_count_;
}

int CallbackCounter::callback_count() {
  base::AutoLock lock(callback_count_lock_);
  return callback_count_;
}

CallbackCounter::~CallbackCounter() = default;

std::string FutureCookieExpirationString() {
  return "; expires=" +
         HttpUtil::TimeFormatHTTP(base::Time::Now() + base::Days(365));
}

}  // namespace net
