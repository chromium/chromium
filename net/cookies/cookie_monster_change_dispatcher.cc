// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_monster_change_dispatcher.h"

#include "base/bind.h"
#include "base/strings/string_piece.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_change_dispatcher.h"

namespace net {

namespace {

// Special key in GlobalDomainMap for global listeners.
constexpr base::StringPiece kGlobalDomainKey = base::StringPiece("\0", 1);

//
constexpr base::StringPiece kGlobalNameKey = base::StringPiece("\0", 1);

}  // anonymous namespace

CookieMonsterChangeDispatcher::Subscription::Subscription(
    base::WeakPtr<CookieMonsterChangeDispatcher> change_dispatcher,
    std::string domain_key,
    std::string name_key,
    GURL url,
    net::CookieChangeCallback callback)
    : change_dispatcher_(std::move(change_dispatcher)),
      domain_key_(std::move(domain_key)),
      name_key_(std::move(name_key)),
      url_(std::move(url)),
      callback_(std::move(callback)),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {
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
    const CookieChangeInfo& change) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const CanonicalCookie& cookie = change.cookie;

  // The net::CookieOptions are hard-coded for now, but future APIs may set
  // different options. For example, JavaScript observers will not be allowed to
  // see HTTP-only changes.
  if (!url_.is_empty() &&
      !cookie
           .IncludeForRequestURL(url_, CookieOptions::MakeAllInclusive(),
                                 change.access_semantics)
           .IsInclude()) {
    return;
  }

  // TODO(mmenke, pwnall): Run callbacks synchronously?
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Subscription::DoDispatchChange,
                                weak_ptr_factory_.GetWeakPtr(), change));
}

void CookieMonsterChangeDispatcher::Subscription::DoDispatchChange(
    const CookieChangeInfo& change) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  callback_.Run(change);
}

CookieMonsterChangeDispatcher::CookieMonsterChangeDispatcher() {}

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
    CookieChangeCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::unique_ptr<Subscription> subscription = std::make_unique<Subscription>(
      weak_ptr_factory_.GetWeakPtr(), DomainKey(url), NameKey(name), url,
      std::move(callback));

  LinkSubscription(subscription.get());
  return subscription;
}

std::unique_ptr<CookieChangeSubscription>
CookieMonsterChangeDispatcher::AddCallbackForUrl(
    const GURL& url,
    CookieChangeCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::unique_ptr<Subscription> subscription = std::make_unique<Subscription>(
      weak_ptr_factory_.GetWeakPtr(), DomainKey(url),
      std::string(kGlobalNameKey), url, std::move(callback));

  LinkSubscription(subscription.get());
  return subscription;
}

std::unique_ptr<CookieChangeSubscription>
CookieMonsterChangeDispatcher::AddCallbackForAllChanges(
    CookieChangeCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::unique_ptr<Subscription> subscription = std::make_unique<Subscription>(
      weak_ptr_factory_.GetWeakPtr(), std::string(kGlobalDomainKey),
      std::string(kGlobalNameKey), GURL(""), std::move(callback));

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
    node->value()->DispatchChange(change);
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
  DCHECK(cookie_domain_map_iterator != cookie_domain_map_.end());

  CookieNameMap& cookie_name_map = cookie_domain_map_iterator->second;
  auto cookie_name_map_iterator =
      cookie_name_map.find(subscription->name_key());
  DCHECK(cookie_name_map_iterator != cookie_name_map.end());

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
