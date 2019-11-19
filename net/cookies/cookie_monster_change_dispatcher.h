// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_MONSTER_CHANGE_DISPATCHER_H_
#define NET_COOKIES_COOKIE_MONSTER_CHANGE_DISPATCHER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/containers/linked_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "url/gurl.h"

namespace net {

// CookieChangeDispatcher implementation used by CookieMonster.
class CookieMonsterChangeDispatcher : public CookieChangeDispatcher {
 public:
  using CookieChangeCallbackList =
      base::CallbackList<void(const CookieChangeInfo&)>;

  CookieMonsterChangeDispatcher();
  ~CookieMonsterChangeDispatcher() override;

  // The key in CookieNameMap for a cookie name.
  static std::string NameKey(std::string name);

  // The key in CookieDomainName for a cookie domain.
  static std::string DomainKey(const std::string& domain);

  // The key in CookieDomainName for a listener URL.
  static std::string DomainKey(const GURL& url);

  // net::CookieChangeDispatcher
  std::unique_ptr<CookieChangeSubscription> AddCallbackForCookie(
      const GURL& url,
      const std::string& name,
      CookieChangeCallback callback) override WARN_UNUSED_RESULT;
  std::unique_ptr<CookieChangeSubscription> AddCallbackForUrl(
      const GURL& url,
      CookieChangeCallback callback) override WARN_UNUSED_RESULT;
  std::unique_ptr<CookieChangeSubscription> AddCallbackForAllChanges(
      CookieChangeCallback callback) override WARN_UNUSED_RESULT;

  // |notify_global_hooks| is true if the function should run the
  // global hooks in addition to the per-cookie hooks.
  //
  // TODO(pwnall): Remove |notify_global_hooks| and fix consumers.
  void DispatchChange(const CookieChangeInfo& change, bool notify_global_hooks);

 private:
  class Subscription : public base::LinkNode<Subscription>,
                       public CookieChangeSubscription {
   public:
    Subscription(base::WeakPtr<CookieMonsterChangeDispatcher> change_dispatcher,
                 std::string domain_key,
                 std::string name_key,
                 GURL url,
                 net::CookieChangeCallback callback);

    ~Subscription() override;

    // The lookup key used in the domain subscription map.
    //
    // The empty string means no domain filtering.
    const std::string& domain_key() const { return domain_key_; }
    // The lookup key used in the name subscription map.
    //
    // The empty string means no name filtering.
    const std::string& name_key() const { return name_key_; }

    // Dispatches a cookie change notification if the listener is interested.
    void DispatchChange(const CookieChangeInfo& change);

   private:
    base::WeakPtr<CookieMonsterChangeDispatcher> change_dispatcher_;
    const std::string domain_key_;  // kGlobalDomainKey means no filtering.
    const std::string name_key_;    // kGlobalNameKey means no filtering.
    const GURL url_;                // empty() means no URL-based filtering.
    const net::CookieChangeCallback callback_;

    void DoDispatchChange(const CookieChangeInfo& change) const;

    // Used to post DoDispatchChange() calls to this subscription's thread.
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

    THREAD_CHECKER(thread_checker_);

    // Used to cancel delayed calls to DoDispatchChange() when the subscription
    // gets destroyed.
    base::WeakPtrFactory<Subscription> weak_ptr_factory_{this};

    DISALLOW_COPY_AND_ASSIGN(Subscription);
  };

  // The last level of the subscription data structures.
  using SubscriptionList = base::LinkedList<Subscription>;

  // Buckets subscriptions according to cookie names.
  //
  // Map keys are cookie names, as we only support exact name matching.
  using CookieNameMap = std::map<std::string, SubscriptionList>;

  // Buckets subscriptions according to cookie domains.
  //
  // Map keys are the eTLD+1 of cookie domains. Cookies are either host-locked,
  // or visible to all the subdomain of a given domain. A cookie's scope cannot
  // exceed eTLD+1, so we stop there.
  using CookieDomainMap = std::map<std::string, CookieNameMap>;

  void DispatchChangeToDomainKey(const CookieChangeInfo& change,
                                 const std::string& domain_key);

  void DispatchChangeToNameKey(const CookieChangeInfo& change,
                               CookieNameMap& name_map,
                               const std::string& name_key);

  // Inserts a subscription into the map.
  //
  // Called by the AddCallback* methods, after creating the Subscription.
  void LinkSubscription(Subscription* subscription);

  // Removes a subscription from the map.
  //
  // Called by the Subscription destructor.
  void UnlinkSubscription(Subscription* subscription);

  CookieDomainMap cookie_domain_map_;

  THREAD_CHECKER(thread_checker_);

  // Vends weak pointers to subscriptions.
  base::WeakPtrFactory<CookieMonsterChangeDispatcher> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CookieMonsterChangeDispatcher);
};

}  // namespace net

#endif  // NET_COOKIES_COOKIE_MONSTER_CHANGE_DISPATCHER_H_
