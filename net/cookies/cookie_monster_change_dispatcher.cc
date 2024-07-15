// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_monster_change_dispatcher.h"

#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "net/base/features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_delegate.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_util.h"

namespace net {

namespace {

// Special key in GlobalDomainMap for global listeners.
constexpr std::string_view kGlobalDomainKey = std::string_view("\0", 1);

//
constexpr std::string_view kGlobalNameKey = std::string_view("\0", 1);

}  // anonymous namespace

CookieMonsterChangeDispatcher::Subscription::Subscription(
    base::WeakPtr<CookieMonsterChangeDispatcher> change_dispatcher,
    std::string domain_key,
    std::string name_key,
    GURL url,
    CookiePartitionKeyCollection cookie_partition_key_collection,
    net::CookieChangeCallback callback)
    : change_dispatcher_(std::move(change_dispatcher)),
      domain_key_(std::move(domain_key)),
      name_key_(std::move(name_key)),
      url_(std::move(url)),
      cookie_partition_key_collection_(
          std::move(cookie_partition_key_collection)),
      callback_(std::move(callback)),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  DCHECK(url_.is_valid() || url_.is_empty());
  DCHECK_EQ(url_.is_empty(), domain_key_ == kGlobalDomainKey);
}

CookieMonsterChangeDispatcher::Subscription::~Subscription() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (change_dispatcher_) {
    change_dispatcher_->UnlinkSubscription(this);
  }
}

void CookieMonsterChangeDispatcher::Subscription::DispatchChange(
    const CookieChangeInfo& change,
    const CookieAccessDelegate* cookie_access_delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const CanonicalCookie& cookie = change.cookie;

  // The net::CookieOptions are hard-coded for now, but future APIs may set
  // different options. For example, JavaScript observers will not be allowed to
  // see HTTP-only changes.
  if (!url_.is_empty()) {
    bool delegate_treats_url_as_trustworthy =
        cookie_access_delegate &&
        cookie_access_delegate->ShouldTreatUrlAsTrustworthy(url_);
    CookieOptions options = CookieOptions::MakeAllInclusive();
    if (!cookie
             .IncludeForRequestURL(
                 url_, options,
                 CookieAccessParams{change.access_result.access_semantics,
                                    delegate_treats_url_as_trustworthy})
             .status.IsInclude()) {
      return;
    }
  }

  if (!cookie_partition_key_collection_.ContainsAllKeys()) {
    if (cookie_partition_key_collection_.PartitionKeys().empty()) {
      if (cookie.IsPartitioned()) {
        return;
      }
    } else {
      DCHECK_EQ(1u, cookie_partition_key_collection_.PartitionKeys().size());
      const CookiePartitionKey& key =
          *cookie_partition_key_collection_.PartitionKeys().begin();
      if (CookiePartitionKey::HasNonce(key) && !cookie.IsPartitioned()) {
        return;
      }
      if (cookie.IsPartitioned() && key != *cookie.PartitionKey()) {
        return;
      }
    }
  }
  Subscription::DoDispatchChange(change);
}

void CookieMonsterChangeDispatcher::Subscription::DoDispatchChange(
    const CookieChangeInfo& change) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  callback_.Run(change);
}

CookieMonsterChangeDispatcher::CookieMonsterChangeDispatcher(
    const CookieMonster* cookie_monster)
    : cookie_monster_(cookie_monster) {}

CookieMonsterChangeDispatcher::~CookieMonsterChangeDispatcher() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

// static
std::string CookieMonsterChangeDispatcher::DomainKey(
    const std::string& domain) {
  std::string domain_key =
      net::registry_controlled_domains::GetDomainAndRegistry(
          domain, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  DCHECK_NE(domain_key, kGlobalDomainKey);
  return domain_key;
}

// static
std::string CookieMonsterChangeDispatcher::DomainKey(const GURL& url) {
  std::string domain_key =
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  DCHECK_NE(domain_key, kGlobalDomainKey);
  return domain_key;
}

// static
std::string CookieMonsterChangeDispatcher::NameKey(std::string name) {
  DCHECK_NE(name, kGlobalNameKey);
  return name;
}

std::unique_ptr<CookieChangeSubscription>
CookieMonsterChangeDispatcher::AddCallbackForCookie(
    const GURL& url,
    const std::string& name,
    const std::optional<CookiePartitionKey>& cookie_partition_key,
    CookieChangeCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::unique_ptr<Subscription> subscription = std::make_unique<Subscription>(
      weak_ptr_factory_.GetWeakPtr(), DomainKey(url), NameKey(name), url,
      CookiePartitionKeyCollection::FromOptional(cookie_partition_key),
      std::move(callback));

  LinkSubscription(subscription.get());
  return subscription;
}

std::unique_ptr<CookieChangeSubscription>
CookieMonsterChangeDispatcher::AddCallbackForUrl(
    const GURL& url,
    const std::optional<CookiePartitionKey>& cookie_partition_key,
    CookieChangeCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::unique_ptr<Subscription> subscription = std::make_unique<Subscription>(
      weak_ptr_factory_.GetWeakPtr(), DomainKey(url),
      std::string(kGlobalNameKey), url,
      CookiePartitionKeyCollection::FromOptional(cookie_partition_key),
      std::move(callback));

  LinkSubscription(subscription.get());
  return subscription;
}

std::unique_ptr<CookieChangeSubscription>
CookieMonsterChangeDispatcher::AddCallbackForAllChanges(
    CookieChangeCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::unique_ptr<Subscription> subscription = std::make_unique<Subscription>(
      weak_ptr_factory_.GetWeakPtr(), std::string(kGlobalDomainKey),
      std::string(kGlobalNameKey), GURL(""),
      CookiePartitionKeyCollection::ContainsAll(), std::move(callback));

  LinkSubscription(subscription.get());
  return subscription;
}

void CookieMonsterChangeDispatcher::DispatchChange(
    const CookieChangeInfo& change,
    bool notify_global_hooks) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DispatchChangeToDomainKey(change, DomainKey(change.cookie.Domain()));
  if (notify_global_hooks)
    DispatchChangeToDomainKey(change, std::string(kGlobalDomainKey));
}

void CookieMonsterChangeDispatcher::DispatchChangeToDomainKey(
    const CookieChangeInfo& change,
    const std::string& domain_key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = cookie_domain_map_.find(domain_key);
  if (it == cookie_domain_map_.end())
    return;

  DispatchChangeToNameKey(change, it->second, NameKey(change.cookie.Name()));
  DispatchChangeToNameKey(change, it->second, std::string(kGlobalNameKey));
}

void CookieMonsterChangeDispatcher::DispatchChangeToNameKey(
    const CookieChangeInfo& change,
    CookieNameMap& cookie_name_map,
    const std::string& name_key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = cookie_name_map.find(name_key);
  if (it == cookie_name_map.end())
    return;

  SubscriptionList& subscription_list = it->second;
  for (base::LinkNode<Subscription>* node = subscription_list.head();
       node != subscription_list.end(); node = node->next()) {
    node->value()->DispatchChange(change,
                                  cookie_monster_->cookie_access_delegate());
  }
}

void CookieMonsterChangeDispatcher::LinkSubscription(
    Subscription* subscription) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // The subscript operator creates empty maps if the lookups fail. This is
  // exactly what this method needs.
  CookieNameMap& cookie_name_map =
      cookie_domain_map_[subscription->domain_key()];
  SubscriptionList& subscription_list =
      cookie_name_map[subscription->name_key()];
  subscription_list.Append(subscription);
}

void CookieMonsterChangeDispatcher::UnlinkSubscription(
    Subscription* subscription) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto cookie_domain_map_iterator =
      cookie_domain_map_.find(subscription->domain_key());
  CHECK(cookie_domain_map_iterator != cookie_domain_map_.end(),
        base::NotFatalUntil::M130);

  CookieNameMap& cookie_name_map = cookie_domain_map_iterator->second;
  auto cookie_name_map_iterator =
      cookie_name_map.find(subscription->name_key());
  CHECK(cookie_name_map_iterator != cookie_name_map.end(),
        base::NotFatalUntil::M130);

  SubscriptionList& subscription_list = cookie_name_map_iterator->second;
  subscription->RemoveFromList();
  if (!subscription_list.empty())
    return;

  cookie_name_map.erase(cookie_name_map_iterator);
  if (!cookie_name_map.empty())
    return;

  cookie_domain_map_.erase(cookie_domain_map_iterator);
}

}  // namespace net
